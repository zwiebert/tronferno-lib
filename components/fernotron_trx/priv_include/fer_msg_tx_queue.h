/*
 * fer_msg_tx_queue.h
 *
 *  Created on: 15.04.2020
 *      Author: bertw
 */

#pragma once

#include "fernotron_trx/raw/fer_fsb.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * \brief   struct holding the RF messaga data plus some data about how and when to send
 *
 */
struct sf {
  uint32_t when_to_transmit_ts; ///< run-time (in s/10) when the messages should be sent
  fer_sbT fsb; ///< message data
  union {
    time_t rtc; ///< RTC time stamp if (mt == MSG_TYPE_RTC)
    //fer_rtc xx;
  };
  bool rf_repeater :1;   ///< msg sent by repeater
  fer_msg_type mt :3; ///< type/kind of message.
  int16_t repeats :4; ///< message should be transmitted  (1 + REPEATS) times
  int16_t sent_ct :5; ///< counter, how often the message has been transmitted already
};

/**
 * \brief    get message first in queue
 * \return   return pointer to message or NULL if queue is empty
 */
struct sf* fer_tx_nextMsg();

/**
 * \brief    remove first message from queue
 */
void fer_tx_popMsg();
/**
 * \brief     append message-pointer to end of queue
 *
 * \param msg pointer to message
 * \return    success (may fail if queue is full)
 */
bool fer_tx_pushMsg(const struct sf *msg);

/**
 * \brief   get current number of messages in queue
 *
 * \return  message count
 */
extern "C" int fer_tx_get_msgPendingCount();
