#include <fernotron_trx/raw/fer_msg_plain.h>
#include <fernotron_trx/fer_trx_c_api.h>
#include <fernotron_trx/raw/fer_radio_trx.h>
#include <string.h>
#include <fer_app_cfg.h>
#include <fernotron_trx/raw/fer_rawmsg_buffer.h>
#include <fernotron_trx/raw/fer_rawmsg_build.h>
#include <fernotron_trx/raw/fer_msg_tx.h>
#include <debug/dbg.h>

#include <fer_msg_tx_queue.h>
#include <fernotron_trx/fer_trx_api.hh>
#include <fer_trx_incoming_event.hh>
#include <fer_trx_impl.hh>
#include <utils_time/run_time.h>
#include <cc1101_ook/trx.hh>
#include <main_loop/main_queue.hh>


void (*fer_tx_READY_TO_TRANSMIT_cb)(uint32_t when_to_transmit_ts);

/**
 * \brief          call callback  \ref fer_tx_READY_TO_TRANSMIT_cb and notify user that we are ready to transmit by calling \ref  fer_trx_api_pushEvent_readyToTransmit
 *
 *                 this will be called, if there is a message in the send queue
 * \param time_ts  when the message wants to be send in s/10 since MCU start
 */
static inline void fer_tx_ready_to_transmit_cb(uint32_t time_ts) {
  if (fer_tx_READY_TO_TRANSMIT_cb)
    fer_tx_READY_TO_TRANSMIT_cb(time_ts);
  fer_trx_api_pushEvent_readyToTransmit();
}

/**
 * \brief if transmitter is not busy and we have a message to send in the queue, then
 *  1) call \ref fer_tx_ready_to_transmit_cb and 2) initiate a call to \ref fer_tx_loop from main task.
 *
 */
static void fer_send_checkQuedState() {
  struct sf *msg;

  if (fer_tx_isTransmitterBusy())
    return;

  if ((msg = fer_tx_nextMsg())) {
    fer_tx_ready_to_transmit_cb(msg->when_to_transmit_ts);
    mainLoop_callFun(fer_tx_loop);
  }
}

bool fer_send_msg_with_stop(const fer_sbT *fsb, uint16_t delay, uint16_t stopDelay, int8_t repeats) {
  precond(fsb);
  precond(stopDelay > 0);

  if (fer_send_msg(fsb, MSG_TYPE_PLAIN, repeats, delay)) {
    fer_sbT stop_fsb = *fsb;
    FER_SB_PUT_CMD(&stop_fsb, fer_cmd_STOP);
    fer_update_tglNibble(&stop_fsb);
    return fer_send_msg(&stop_fsb, MSG_TYPE_PLAIN, 2, delay + stopDelay);
  }
  return false;
}

bool fer_send_msg(const fer_sbT *fsb, fer_msg_type msgType, int8_t repeats, uint16_t delay) {
#ifdef CONFIG_APP_USE_FER_TRANSMITTER
  precond(fsb);
  precond (msgType == MSG_TYPE_PLAIN || msgType == MSG_TYPE_TIMER);

  struct sf msg = { .when_to_transmit_ts = (delay + (uint32_t)run_time_ts()), .fsb = *fsb, .mt = msgType, .repeats = repeats };
  if (!fer_tx_pushMsg(&msg))
    return false;

  fer_send_checkQuedState();
  return true;
#else
  return false;
#endif
}

bool fer_send_msg_rtc(const fer_sbT *fsb, time_t rtc, int8_t repeats) {
#ifdef CONFIG_APP_USE_FER_TRANSMITTER
  precond(fsb);

  struct sf msg = { .when_to_transmit_ts = (uint32_t)run_time_ts(), .fsb = *fsb, .rtc = rtc, .mt = MSG_TYPE_RTC, .repeats = repeats };

  if (!fer_tx_pushMsg(&msg))
    return false;

  fer_send_checkQuedState();
  return true;
#else
  return false;
#endif
}

static bool fer_send_queued_msg(struct sf *msg) {
  static uint8_t sf_toggle;
  precond(!fer_tx_isTransmitterBusy());
  precond(msg);

  Fer_Trx_IncomingEvent evt { .raw = fer_tx_msg, .fsb = msg->fsb, .kind = msg->mt, .tx = true };

  if (msg->sent_ct++ == 0) {
    if (!msg->rf_repeater) {
      sf_toggle = fer_tglNibble_ctUp(sf_toggle, 1);
    }
    FER_SB_PUT_TGL(&msg->fsb, sf_toggle);
    if (msg->mt == MSG_TYPE_PLAIN) {
      evt.first = true;
      Fer_Trx_API::push_event(&evt);
    }
  }
  if (msg->mt == MSG_TYPE_RTC) {
    fer_msg_raw_from_time(fer_tx_msg, msg->rtc, MSG_TYPE_RTC);
  }
  memcpy(fer_tx_msg->cmd.bd, msg->fsb.data, 5);

  fer_msg_raw_checksumsCreate(fer_tx_msg, msg->mt);

  evt.first = false;
  Fer_Trx_API::push_event(&evt);

  fer_tx_transmitFerMsg(0, msg->mt);

  if (msg->sent_ct > msg->repeats) {
    return true;
  }

  return false;
}

fer_sbT fer_construct_fsb(uint32_t a, uint8_t g, uint8_t m, fer_cmd cmd) {
  fer_sbT fsb;
  fer_init_sender(&fsb, a);

  if (FER_SB_ADDR_IS_CENTRAL(&fsb)) {
    FER_SB_PUT_NGRP(&fsb, g);
    FER_SB_PUT_NMEMB(&fsb, m);
  }
  FER_SB_PUT_CMD(&fsb, cmd);

  return fsb;
}

void fer_trx_direction(bool tx) {
  cc1101_ook_setDirection(tx);
}

extern uint32_t last_rx_ts;

/**
 * \brief Test if receiver is not receiving right now, to avoid RF collision with transmitter.
 * \return  true if transmitting is possible right now
 */
inline bool fer_rx_clear_to_send() {
#ifdef HOST_TESTING
  return true;
#endif
  return last_rx_ts + 5 <= run_time_ts(); // TODO: we could use RSSI with CC1101 here. For now just check if nothing was received in the last 500ms
}

/**
 * \brief  start or stop periodic timer to call \ref fer_tx_loop
 *
 *     if transmitting something new is not possible right now, start the periodic timer and return false
 *     it transmitting has become possible, then stop timer (if running) and return true
 *
 * \return  true if transmitter is ready to send a new message
 */
static bool wait_tx_available() {
  static void *tmr;

  // Wait for transmitter to become available
  if (fer_tx_isTransmitterBusy() || !fer_rx_clear_to_send()) {
    if (!tmr)
      tmr = mainLoop_callFunByTimer(fer_tx_loop, 100, true);
    return false;
  }
  if (tmr) {
    mainLoop_stopFun(tmr);
    tmr = NULL;
  }
  return true;
}
/**
 * \brief               start timer which calls \ref fer_tx_loop at the specified time
 * \param now_ts        the current time in s/10 since MCU start
 * \param when_ts       time we want to send in s/10 since MCU start
 * \return              success (starting the timer was successful)
 */
static bool wait_tx_when_to_transmit(uint32_t now_ts, uint32_t when_ts) {
  const uint32_t delay_ms = (when_ts - now_ts) * 100;
  static void *tmr;

  mainLoop_stopFun(tmr);
  tmr = mainLoop_callFunByTimer(fer_tx_loop, delay_ms);

  return tmr != NULL;
}

void fer_tx_loop() {
#ifdef CONFIG_APP_USE_FER_TRANSMITTER
  if (!wait_tx_available())
    return;

  struct sf *const msg = fer_tx_nextMsg();
  const uint32_t now_ts = run_time_ts();

  if (!msg) {
    fer_trx_direction(false);
    fer_send_checkQuedState();
  } else if (msg->when_to_transmit_ts <= now_ts) {
    fer_trx_direction(true);

    if (fer_send_queued_msg(msg)) {
      fer_tx_popMsg();
    }
    //fer_send_checkQuedState();
  } else {
    fer_trx_direction(false);
    wait_tx_when_to_transmit(now_ts, msg->when_to_transmit_ts);
  }
#endif
}

