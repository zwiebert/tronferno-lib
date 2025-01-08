#include <fernotron_trx/raw/fer_radio_trx.h>
#include "fer_trx_impl.hh"
#include "fer_radio_parity.h"
#include <fernotron_trx/raw/fer_msg_plain.h>
#include "fernotron_trx/isr_timer_config.h"
#include <stdlib.h>
#include "fer_app_cfg.h"
#include "fernotron_trx/raw/fer_rawmsg_buffer.h"
#include "fernotron_trx/raw/fer_msg_tx.h"
#include "debug/dbg.h"
#include "utils_misc/int_macros.h"
#include "fernotron_trx/raw/fer_radio_timings_us.h"
#include <fernotron_trx/raw/rf_capture.hh>
#ifndef HOST_TESTING
#include <esp_attr.h>
#endif



volatile fer_msg_type fer_rx_messageReceived;
void (*fer_rx_MSG_RECEIVED_ISR_cb)(void);


// private ///////////////////////////////////////////////////////s

#ifndef DISTRIBUTION
//#define FTRX_TEST_LOOP_BACK
static void IRAM_ATTR db_toggleTxPin() {
  static bool lvl;
  lvl = !lvl;
  void ftrx_testSetOutputLevel(bool level);
  ftrx_testSetOutputLevel(lvl);
}
#ifdef FTRX_TEST_LOOP_BACK
static bool ftrx_testLoopBack_getRxPin();
#define mcu_get_rxPin ftrx_testLoopBack_getRxPin
#endif
#endif


#define US2TCK(us) FER_RX_US_TO_TCK(us)   ///< calculate how many ticks occur during the given number of micro-seconds

/// \brief  Possible errors/warnings when receiving a message
/// \note   The only hard error is BAD_CHECKSUM
enum fer_error {
  fer_OK, ///< All is good
  fer_PAIR_NOT_EQUAL, ///< Warning: Each byte is sent twice, as a pair. This error occurs if they are not equal.
  fer_BAD_WORD_PARITY, ///< Warning: A Byte parity bit wrong
  fer_BAD_CHECKSUM ///< Error: One or more of the embedded checksums are wrong
};


/**
 * \brief some counter to represent current place in the received message.
 *
 *       These counters tells us exactly which bit and word we are receiving at a given moment
 */
struct fer_rx_counter {
  uint16_t Words; ///<  number of words received since /ref fer_rx_clear . From 0 to WORDS_MSG_TIMER
  uint16_t stopBits; ///< number of stop bits received since /ref fer_rx_clear
  uint8_t Bits;     ///< number of bits received numbered from 0 to FER_CMD_BIT_CT (10)
  uint8_t errors;    ///< number of fatal errors since /ref fer_rx_clear
  uint8_t recovered; ///< number of recoverable errors since /ref fer_rx_clear
};

static bool input_level;    ///< logical level of the RF input pin sampled at the current tick.
static bool input_edge_pos; ///< true if  input level moved from false to true at the current tick.
static bool input_edge_neg; ///< true if  input level moved from true to false at the current tick.

static uint16_t aTicks; ///< count ticks. resets at positive edge
static uint16_t pTicks; ///< count ticks while input_level is true. resets at positive edge (input_level going from false to true)
static uint16_t nTicks; ///< count ticks while input_level is false. resets at negative edge (input_level going from true to false)
static uint16_t wordsToReceive; ///< words we need to receive until the (partial) message is complete.

// flags
static uint16_t word_pair_buffer[2];            ///< buffer for storing the received word pair bit-by-bit
static struct fer_rx_counter frxCount;          ///< these counters indicate where we are in the current received message.


/////////////////////////// interrupt code //////////////////////
#define bitLen               US2TCK(FER_BIT_WIDTH_US)
// bit is 1, if data edge comes before sample position (/SHORT\..long../)
// bit is 0, if data edge comes after sample position  (/..LONG..\short/)
#define SAMPLE_BIT ((pTicks < US2TCK(FER_BIT_SAMP_POS_US)))
#define veryLongPauseLow_Len (2 * US2TCK(FER_STP_WIDTH_MAX_US))
#define rxbuf_current_byte() (&fer_rx_msg->cmd.bd[0] + (frxCount.Words / 2))
#define ct_incr(ct, limit) (!((++ct >= limit) ? (ct = 0) : 1))
#define ct_incrementP(ctp, limit) ((++*ctp, *ctp %= limit) == 0)

static fer_error IRAM_ATTR fer_rx_extract_Byte(const uint16_t *src, uint8_t *dst) {
  bool match = ((0xff & src[0]) == (0xff & src[1]));
  unsigned count = 0;
  uint8_t out_byte = 0x77;

  if (fer_word_parity_p(src[0], 0)) {
    out_byte = src[0];
    ++count;

  }

  if (fer_word_parity_p(src[1], 1)) {
    out_byte = src[1];
    ++count;
  }

  *dst = out_byte;

  if (count == 2 && match) {
    return fer_OK;
  } else if (count == 1) {
    return fer_PAIR_NOT_EQUAL;
  }
  return fer_BAD_WORD_PARITY;
}

static fer_error IRAM_ATTR fer_rx_verify_cmd(const uint8_t *dg) {
  int i;
  uint8_t checksum = 0;
  bool all_null = true;

  for (i = 0; i < FER_CMD_BYTE_CT - 1; ++i) {
    checksum += dg[i];
    if (dg[i] != 0)
      all_null = false;
  }

  if (all_null)
    return fer_BAD_CHECKSUM;

  return (checksum == dg[i] ? fer_OK : fer_BAD_CHECKSUM);
}

static void IRAM_ATTR fer_rx_recv_decodeByte(uint8_t *dst) {
  switch (fer_rx_extract_Byte(word_pair_buffer, dst)) {
  case fer_PAIR_NOT_EQUAL:
    ++frxCount.recovered;
    break;
  case fer_BAD_WORD_PARITY:
    //db_toggleTxPin();
    ++frxCount.errors;
    break;
  case fer_OK:
  default:
    break;
  }
}

/**
 * \brief Determine if current bit is a stop bit
 * \param len       total duration from starting positive edge to ending positive edge)
 * \param nedge     duration from starting positive edge to the negative edge in between
 */
static bool IRAM_ATTR fer_rx_is_stopBit(unsigned len, unsigned nedge) {
  return ((US2TCK(FER_STP_WIDTH_MIN_US) <= len && len <= US2TCK(FER_STP_WIDTH_MAX_US))
      && (US2TCK(FER_STP_NEDGE_MIN_US) <= nedge && nedge <= US2TCK(FER_STP_NEDGE_MAX_US)));
}

/**
 * \brief Determine if current bit is  a ??? bit (unused function)
 * \param len       total duration from starting positive edge to ending positive edge)
 * \param nedge     duration from starting positive edge to the negative edge in between
 */
static bool IRAM_ATTR fer_rx_is_endCmdBit(unsigned len, unsigned nedge) {
  return ((US2TCK(FER_STP_WIDTH_MIN_US) <= len) && (US2TCK(FER_STP_NEDGE_MIN_US) <= nedge && nedge <= US2TCK(FER_STP_NEDGE_MAX_US)));
}

/**
 * \brief Determine if current bit is a preambel bit (unused function)
 * \param len       total duration from starting positive edge to ending positive edge)
 * \param nedge     duration from starting positive edge to the negative edge in between
 */
static bool IRAM_ATTR fer_rx_is_pre_bit(unsigned len, unsigned nedge) {
  return ((US2TCK(FER_PRE_WIDTH_MIN_US) <= len && len <= US2TCK(FER_PRE_WIDTH_MAX_US))
      && (US2TCK(FER_PRE_NEDGE_MIN_US) <= nedge && nedge <= US2TCK(FER_PRE_NEDGE_MAX_US)));
}

/**
 * \brief Determine if current data bit has the value 1
 * \param len       total duration from starting positive edge to ending positive edge)
 * \param nedge     duration from starting positive edge to the negative edge in between
 */
static bool IRAM_ATTR fer_rx_is_dataBitOne(unsigned len, unsigned nedge) {
  return len > (2 * nedge);
}

/**
 * \brief Determine if current data bit has the value 0
 * \param len       total duration from starting positive edge to ending positive edge)
 * \param nedge     duration from starting positive edge to the negative edge in between
 */
static bool IRAM_ATTR fer_rx_is_dataBitZero(unsigned len, unsigned nedge) {
  return len < (2 * nedge);
}

/**
 * \brief Determine if current bit ia a data bit
 * \param len       total duration from starting positive edge to ending positive edge)
 * \param nedge     duration from starting positive edge to the negative edge in between
 */
static bool IRAM_ATTR fer_rx_is_dataBit(unsigned len, unsigned nedge) {
  return FER_BIT_WIDTH_MIN_US <= len && len <= FER_BIT_WIDTH_MAX_US;
}

/**
 * \brief Count bits and sample data bits or do nothing if there is no RF input
 * \return true if  a bit was sampled and stored into \ref word_pair_buffer
 */
static bool IRAM_ATTR fer_rx_wait_and_sample(void) {

  if (!input_edge_pos)
    return false;

  const bool isStopBit = fer_rx_is_stopBit(aTicks, pTicks);
  const bool isDataBit = fer_rx_is_dataBit(aTicks, pTicks);
  const bool isDataBitOne = fer_rx_is_dataBitOne(aTicks, pTicks);
  const bool isDataBitZero = fer_rx_is_dataBitZero(aTicks, pTicks);

  if (isStopBit) {
    if (frxCount.stopBits == 0) {
      fer_rx_clear();
    }
    ++frxCount.stopBits;
    return false;
  }

  if (frxCount.stopBits == 0)
    return false;

  if (!isDataBit && !(isDataBitOne || isDataBitZero)) {
    //db_toggleTxPin();
    fer_rx_clear();
    return false;
  }

  PUT_BIT(word_pair_buffer[frxCount.Words & 1], frxCount.Bits, isDataBitOne);
  return true;
}

/**
 * \brief Receive a (partial) message
 *
 *        The caller has to determine if a message is either partial or complete by looking into the message content.
 *
 * \return If the (partial) message is still incomplete or there was nothing received, it returns 0.  Othewise it returns the number of words the (partial) received message had.
 */
static int IRAM_ATTR fer_rx_receive_message(void) {

  if (fer_rx_wait_and_sample()) {
    if (ct_incr(frxCount.Bits, FER_CMD_BIT_CT)) {
      // word complete
      if ((frxCount.Words & 1) == 1) {
        // word pair complete
        fer_rx_recv_decodeByte(rxbuf_current_byte());
      }

      //if (frxCount.Words)  db_toggleTxPin();

      ++frxCount.Words;

      return frxCount.Words;
    }
  }
  return 0;  // continue
}



/**
 * \brief Try to decode a message from sampled RF input.
 *
 * This function is called from a timer interrupt.
 *
 * \return  true if a complete RF message was received
 */
static fer_msg_type IRAM_ATTR fer_rx_tick_receive_message() {

  auto word_count = fer_rx_receive_message();

  switch (word_count) {

  case WORDS_MSG_PLAIN:  // we received either complete a plain message or the 1st part of a bigger message

    //db_toggleTxPin();
    if (frxCount.errors || fer_OK != fer_rx_verify_cmd(fer_rx_msg->cmd.bd)) {
      fer_rx_clear();
      break;
    }

    if (fer_rx_msg->cmd.sd.cmd.cmd == fer_cmd_Program && FER_CMD_ADDR_IS_CENTRAL(&fer_rx_msg->cmd.sd.cmd)) {
      // continue. message still incomplete
      wordsToReceive = WORDS_MSG_RTC;
      break;
    }

    // done. message complete
    return MSG_TYPE_PLAIN;

  case WORDS_MSG_RTC: // we received either a complete RTC message or the 2nd part of a timer message

    //db_toggleTxPin();
    if (frxCount.errors) {
      fer_rx_clear();
      break;
    }

    if (!fer_rx_msg->rtc.sd.wd2.sd.rtc_only) {
      // continue. message still incomplete
      wordsToReceive = WORDS_MSG_TIMER;
      break;
    }

    // done. message complete
    return MSG_TYPE_RTC;

  case WORDS_MSG_TIMER: // we received a complete timer message

    // db_toggleTxPin();
    if (frxCount.errors) {
      fer_rx_clear();
      break;
    }

    // done. message complete
    return MSG_TYPE_TIMER;

  default:
    // continue. message still incomplete
    break;
  }

  return MSG_TYPE_NONE;
}

/**
 * \brief set global flags according to current RF input logic level
 *
 * \param pin_level   the current logic level of the RF input pin
 *
 * It sets the static variables
 *  \ref input_level
 *  \ref input_edge_pos
 *  \ref input_edge_neg
 */
static void IRAM_ATTR fer_rx_sampleInput(bool pin_level) {

  input_edge_pos = input_edge_neg = false;
  if (input_level != pin_level) {
    if (pin_level)
      input_edge_pos = true;
    else
      input_edge_neg = true;
  }

  input_level = pin_level;
}

// public //////////////////////////////////////////////////////////////////s

void IRAM_ATTR fer_rx_clear(void) {
  frxCount = (struct fer_rx_counter ) { };
  fer_rx_messageReceived = MSG_TYPE_NONE;
  wordsToReceive = WORDS_MSG_PLAIN;
}

void fer_rx_getQuality(struct fer_rx_quality *dst) {
  dst->bad_pair_count = frxCount.recovered;
}

void IRAM_ATTR fer_rx_tick(bool pin_level) {
#ifdef CONFIG_TRONFERNO_ENABLE_RF_CAPTURE
  rfCapture_sample_level(pin_level);
#endif
  bool msg_received = false;

  // prevent receiving our own transmitter
  // which would lead to ping pong in repeater function
  extern volatile bool fer_tx_messageToSend_isReady;
  if (fer_tx_messageToSend_isReady)
    return;

  fer_rx_sampleInput(pin_level);

  if ((input_level && nTicks > veryLongPauseLow_Len)) {
    fer_rx_clear();
  }

  // receive and decode input
  if (!fer_rx_isReceiverBlocked()) {
    if (fer_rx_messageReceived = fer_rx_tick_receive_message(); fer_rx_messageReceived != MSG_TYPE_NONE) {
      msg_received = true;
    }
  }

  if (input_edge_pos)
    aTicks = pTicks = 0;
  if (input_edge_neg)
    nTicks = 0;

  // measure the time between input edges
  ++aTicks;
  if (input_level) {
    ++pTicks;
  } else {
    ++nTicks;
  }

  if (msg_received && fer_rx_MSG_RECEIVED_ISR_cb)
    fer_rx_MSG_RECEIVED_ISR_cb();
}

