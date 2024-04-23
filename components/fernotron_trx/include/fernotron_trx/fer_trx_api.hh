/**
 * \file fernotron_trx/fer_trx_api.hh
 * \brief C++-API for sending and receiving Fernotron RF messages
 */
#pragma once

#include <utils_misc/int_types.h>
#include <fernotron_trx/fer_trx_c_api.h>
#include <fernotron_trx/astro.h>

/**
 * \class Fer_Trx_API
 *
 * \ingroup fernotron_trx
 *
 * \brief User interface for sending and receiving Fernotron protocol messages via RF
 *
 * \note The corresponding C API is defined in fer_trx_c_api.h
 *
 * \author Bert Winkelmann
 *
 */
class Fer_Trx_API {
public:

  /// type of a message
  enum MsgKind {
    MSG_TYPE_NONE, MSG_TYPE_PLAIN, MSG_TYPE_PLAIN_DOUBLE, MSG_TYPE_RTC, MSG_TYPE_TIMER
  };

public:
  virtual ~Fer_Trx_API() = default;

public:
  ////////////////////////////////////////////////////////////////
  ///////// to get events: 1. derive from this class. ////////////
  /////////                2. register your oubject here /////////
  ////////////////////////////////////////////////////////////////
  static void setup(Fer_Trx_API *derived_object);

  ///////////// setup and reinit astro data //////////////////////
  static void setup_astro(const struct cfg_astro *cfg_astro);

public:
  ////////////////////////////////////////////////////////////////
  ///////// get data of message (both rx and tx) /////////////////
  ////////////////////////////////////////////////////////////////

  ///
  /**
   * \brief    extract data from current received message
   * \return   plain command message (part)
   */
  Fer_MsgPlainCmd get_msg() const;
  /**
   * \brief    extract data from current received message
   * \return   Fernotron sender (or receiver RF code) address
   */
  uint32_t get_a() const;
  /**
   * \brief    extract data from current received message
   * \return   Fernotron group number
   */
  uint8_t get_g() const;
  /**
   * \brief    extract data from current received message
   * \return   Fernotron receiver number
   */
  uint8_t get_m() const;
  /**
   * \brief    extract data from current received message
   * \return   Fernotron command code
   */
  fer_if_cmd get_cmd() const;
  /**
   * \brief    extract data from current received message
   * \return   message-kind
   */
  MsgKind get_msgKind() const;
  /**
   * \brief    extract data from current received message
   * \return   pointer to raw message buffer
   */
  const struct fer_raw_msg * get_raw() const;
  /**
   * \brief    extract data from current received message
   * \return   true if message originates from programming central
   */
  bool is_centralUnit() const;

  /**
   * \brief    extract data from current received message
   * \return   RSSI value (if provided by RF receiver)
   */
  static int get_rssi();

public:
  ////////////////////////////////////////////////////////////////
  ///////// get notified if a message was received ///////////////
  ////////////////////////////////////////////////////////////////

  /// event: plain message was received (no doubles). Override it.
  virtual void event_plain_message_was_received() {  }
  /// event: double of a plain message was received. Override it.
  virtual void event_plain_double_message_was_received() {  }
  /// event: rtc only message was received. Override it.
  virtual void event_rtc_message_was_received() {  }
  /// event: a full timer message was received. Override it.
  virtual void event_timer_message_was_received() {  }
  /// event: a message of any kind was received. Override it.
  virtual void event_any_message_was_received() {  }

public:
  ////////////////////////////////////////////////////////////////
  ///////// get notified if a message is about to be send ////////
  ////////////////////////////////////////////////////////////////

  /// event: just before any queued message will be sent first (no repeats). Override it.
  virtual void event_first_message_will_be_sent() {   }
  /// event: just before any queued message will be sent (inclusive repeats). Override it.
  virtual void event_any_message_will_be_sent() {  }


public:
  ////////////////////////////////////////////////////////////////
  ///////// transmit messages ////////////////////////////////////
  ////////////////////////////////////////////////////////////////

   /**
    * \brief            push a message to the RF send queue.
    * \param msg        message command object
    * \return           success
    */
  static bool send_cmd(const Fer_MsgCmd &msg);
  /**
   * \brief             push a message to the RF send queue (using parameter list)
   * \param a           Fernotron address
   * \param g           group number
   * \param m           receiver number
   * \param cmd         command code
   * \param repeats     how often to repeat this message
   * \param delay       delay transmission by this value in 100 milliseconds (s/10)
   * \param stopDelay   follow up with sending a STOP command after stopDelay (s/10)
   * \return            success
   */
  static bool send_cmd(uint32_t a, uint8_t g, uint8_t m, fer_if_cmd cmd, uint8_t repeats = 0, uint16_t delay = 0, uint16_t stopDelay = 0);

   /**
    * \brief            push a RTC message to the RF send queue.
    * \param msg        message command object
    * \return           success
    */
  static bool send_rtc(const Fer_MsgRtc &msg);

  /**
   * \brief             build RTC message and push it to the RF send queue.
   * \param a           Fernotron address
   * \param g           group number
   * \param m           receiver number
   * \param rtc         POSIX time stamp
   * \return            success
   */
  static bool send_rtc(uint32_t a, uint8_t g, uint8_t m, time_t rtc);

  /**
   * \brief            push Timer message to the RF send queue.
   * \param msg        message command object
   * \return           success
   */
  static bool send_timer(const Fer_MsgTimer &msg);

  /**
   * \brief             build Timer message and push it to the RF send queue.
   * \param a           Fernotron address
   * \param g           group number
   * \param m           receiver number
   * \param rtc         POSIX time stamp
   * \param td          timer data object
   * \return            success
   */
  static bool send_timer(uint32_t a, uint8_t g, uint8_t m, time_t rtc, const Fer_TimerData &td);

  /**
   * \brief            push empty (all-disabled) Timer message to the RF send queue.
   * \param msg        RTC message command object (empty timers block is omitted)
   * \return           success
   */
  static bool send_empty_timer(const Fer_MsgRtc &msg);
  /**
   * \brief            push empty (all-disabled) Timer message to the RF send queue.
   * \param rtc        POSIX time stamp used to generate the RTC part of the message
   * \return           success
   */
  static bool send_empty_timer(uint32_t a, uint8_t g, uint8_t m, time_t rtc);

public:
  ////////////////////////////////////////////////////////////////
  ///////// timer interrupt for RF transceiver ///////////////////
  ////////////////////////////////////////////////////////////////
  static void isr_handle_rx(bool rx_pin_lvl); ///< call this from timer ISR every (200/INTR_TICK_FREQ_MULT)us

  static bool isr_get_tx_level(); ///< call this son top of timer ISR
  static void isr_handle_tx();  ///< call this from timer ISR every 200us

public:
  ////////////////////////////////////////////////////////////////
  /////////////// run the action /////////////////////////////////
  ////////////////////////////////////////////////////////////////

  // getting notified if there is something to do

  /// event: there is something to do for the related worker function. Override it.
  virtual void event_ready_to_transmit() { }  // something to do for loop_tx()
  // getting notified from ISR by callbacks (no virtual functions possible from ISR)
  // note: the callback functions must be located in IRAM (use ATTR_IRAM)

  /// event: there is something to do for the related worker function. Called from ISR. Provide an ATTR_IRAM function.
  /**
   * \brief     register callback which is called from RF-receiver ISR when there is work to do (a message was received)
   * \note      lambdas will not work with ATTR_IRAM tag, so don't use one as callback here
   * \param cb  callback, which must have the ATTR_IRAM tag
   */
  static void register_callback_msgReceived_ISR(CallBackFnType cb);

  /// event: there is something to do for the related worker function. Called from ISR. Provide an ATTR_IRAM function.
  /**
   * \brief     register callback which is called from RF-transmitter ISR when there is work to do (message transmission done)
   * \note      lambdas will not work with ATTR_IRAM tag, so don't use one as callback here
   * \param cb  callback, which must have the ATTR_IRAM tag
   */
  static void register_callback_msgTransmitted_ISR(CallBackFnType cb);

  /// worker function for Receiver. Call after related events.
  static void loop_rx();

  /// worker function for Transmitter. Call after related events.
  static void loop_tx();

public:
  /// private: called internally
  static void push_event(struct Fer_Trx_IncomingEvent *evt);
private:
  struct Fer_Trx_IncomingEvent *myEvt;
  Fer_MsgPlainCmd myMsg;
};

typedef Fer_Trx_API::MsgKind fer_msg_kindT;  ///<  message-kind type

#include "fer_trx_api__inline.hh"
