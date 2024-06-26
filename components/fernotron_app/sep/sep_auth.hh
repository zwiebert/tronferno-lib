/*
 * Sep_Auth.hh
 *
 *  Created on: Jun 10, 2022
 *      Author: bertw
 */

#pragma once

#include "auth_button.hh"
#include "sep_enable.hh"
#ifndef TEST_HOST
#include "gpio/pin.h"
#else
inline bool mcu_button_was_pressed() { return false; }
#endif

class Sep_Auth {

public:
  Sep_Auth() :
      m_auth_button(mcu_button_was_pressed, 'e') {

  }

public:
  void restart_timeout() {
    m_auth_timeout.set(m_auth_timeout_secs);
  }
  //sep_auth.restart_timeout();
public:
  bool authenticate(uint32_t auth_key, unsigned auth_timeout_secs = fer_sep_TIMEOUT, unsigned auth_button_timeout_secs = SEP_AUTH_BUTTON_TIMEOUT_SECS) {
    if (!m_auth_key)
      setKey(auth_key);
    else if (!isKeyMatched(auth_key))
      return false;

    m_auth_timeout_secs = auth_timeout_secs;
    m_auth_button.log_in(auth_button_timeout_secs);
    fer_sep_ENABLE_cb();
    return true;
  }

  bool deauthenticate(uint32_t auth_key) {
    if (!isKeyMatched(auth_key))
      return false;
    m_auth_button.log_out();
    m_auth_key = 0;
    fer_sep_DISABLE_cb();

    return true;
  }

  bool work_loop() {
    if (!m_auth_button.isLoggedIn())
      m_auth_button.work_loop();
    if (!m_auth_button.isLoggedIn())
      return false;

    return true;
  }

  bool hasTimedOut() {
    if (m_auth_timeout.isTimeoutReached()) {
      uoApp_publish_fer_sepState( { .auth_timeout = 1 });
      return true;
    }
    return false;
  }

  bool isKeyMatched(uint32_t auth_key) const {
    if (!m_auth_key)
      return false;
    if (auth_key != m_auth_key)
      return false;
    return true;
  }

  bool isAuthenticated(uint32_t auth_key) const {
    if (!isKeyMatched(auth_key))
      return false;

    if (!m_auth_button.isLoggedIn())
      return false;
    return true;
  }

  uint32_t getAuthKey() const {
    return m_auth_key;
  }
private:
  void setKey(uint32_t auth_key) {
    m_auth_key = auth_key;
  }
private:
  uint32_t m_auth_key = 0;
  Auth_Button m_auth_button;
  Time_Out_Secs m_auth_timeout;
  int16_t m_auth_timeout_secs = 0;
};
