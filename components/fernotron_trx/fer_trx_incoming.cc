#include <fernotron/trx/fer_trx_api.hh>
#include "fer_trx_incoming_event.hh"



static Fer_Trx_API *OurDerivedObject;

void Fer_Trx_API::setup(Fer_Trx_API *derived_object) {
  OurDerivedObject = derived_object;
}

void Fer_Trx_API::setup_astro(const struct cfg_astro *cfg_astro) {
  fer_astro_init_and_reinit(cfg_astro);
}


u32 Fer_Trx_API::get_a() const {
  return FER_SB_GET_DEVID(&myEvt->fsb);
}

u8 Fer_Trx_API::get_g() const {
  if (!is_centralUnit())
    return 0;

  fer_grp grp = myEvt->fsb.sd.grp;
  return grp;
}

u8 Fer_Trx_API::get_m() const {
  if (!is_centralUnit())
    return 0;

  fer_memb memb = myEvt->fsb.sd.memb;
  return memb == 0 ? 0 : memb - 7;
}

fer_if_cmd Fer_Trx_API::get_cmd() const {
  return static_cast<fer_if_cmd>(myEvt->fsb.sd.cmd);
}

const void *Fer_Trx_API::get_raw() const {
  return myEvt->raw;
}

const void *Fer_Trx_API::get_fsb() const {
  return &myEvt->fsb;
}

bool Fer_Trx_API::is_centralUnit() const {
  return FER_SB_ADDR_IS_CENTRAL(&myEvt->fsb);
}

Fer_Trx_API::MsgKind Fer_Trx_API::get_msgKind() const {
  return static_cast<MsgKind>(myEvt->kind);
}

void Fer_Trx_API::push_event(struct Fer_Trx_IncomingEvent *evt) {
  if (!OurDerivedObject)
    return;

  Fer_Trx_API &that = *OurDerivedObject;
  that.myEvt = evt;

  if (evt->tx) {
    if (evt->first)
      that.event_first_message_will_be_sent();
    else
      that.event_any_message_will_be_sent();

  } else { //rx
    switch (evt->kind) {
    case MSG_TYPE_PLAIN_DOUBLE:
      that.event_any_message_was_received();
      that.event_plain_double_message_was_received();
      break;

    case MSG_TYPE_PLAIN:
      that.event_any_message_was_received();
      that.event_plain_message_was_received();
      break;

    case MSG_TYPE_RTC:
      that.event_any_message_was_received();
      that.event_rtc_message_was_received();
      break;
    case MSG_TYPE_TIMER:
      that.event_any_message_was_received();
      that.event_timer_message_was_received();
      break;

    case MSG_TYPE_NONE:
    default:
      break;
    }
  }

  that.myEvt = 0;
}

///////////////// ISR //////////////////////////////
#include <fernotron/trx/raw/fer_radio_trx.h>
void IRAM_ATTR Fer_Trx_API::isr_sample_rx_pin(bool level) {
  fer_rx_sampleInput(level);
}
void IRAM_ATTR Fer_Trx_API::isr_handle_rx() {
  fer_rx_tick();
}

bool IRAM_ATTR Fer_Trx_API::isr_get_tx_level() {
  return fer_tx_setOutput();
}
void IRAM_ATTR Fer_Trx_API::isr_handle_tx() {
  fer_tx_dck();
}


///////////////// send API ///////////////////////////

#include <fernotron/trx/fer_trx_c_api.h>
#include "fernotron/trx/raw/fer_msg_rx.h"
#include <debug/dbg.h>

static void fill_fsb(fer_sbT &fsb, u32 a, u8 g, u8 m, fer_cmd cmd) {
  fer_init_sender(&fsb, a);

  if (FER_SB_ADDR_IS_CENTRAL(&fsb)) {
    FER_SB_PUT_NGRP(&fsb, g);
    FER_SB_PUT_NMEMB(&fsb, m);
  }
  FER_SB_PUT_CMD(&fsb, cmd);
}

bool Fer_Trx_API::send_cmd(u32 a, u8 g, u8 m, fer_if_cmd cmd, i8 repeats, u16 delay, u16 stopDelay) {
  fer_sbT fsb;
  fill_fsb(fsb,a, g, m, (fer_cmd)cmd);
  if (stopDelay) {
    return fer_send_msg_with_stop(&fsb, delay, stopDelay, repeats);
  } else {
    return fer_send_delayed_msg(&fsb, ::MSG_TYPE_PLAIN, delay, repeats);
  }

  return false;
}

bool Fer_Trx_API::send_cmd(const Fer_MsgCmd &msg) {
  fer_sbT fsb;
  fill_fsb(fsb,msg.a, msg.g, msg.m, (fer_cmd)msg.cmd);
  if (msg.stopDelay) {
    return fer_send_msg_with_stop(&fsb, msg.delay, msg.stopDelay, msg.repeats);
  } else {
    return fer_send_delayed_msg(&fsb, ::MSG_TYPE_PLAIN, msg.delay, msg.repeats);
  }

  return false;
}

bool Fer_Trx_API::send_rtc(u32 a, u8 g, u8 m, time_t rtc) {
  fer_sbT fsb;
  fill_fsb(fsb,a, g, m, fer_cmd_Program);

  return fer_send_rtc_message(&fsb, rtc);

  return false;
}

bool Fer_Trx_API::send_rtc(const Fer_MsgRtc &msg) {
  fer_sbT fsb;
  fill_fsb(fsb,msg.a, msg.g, msg.m, fer_cmd_Program);

  return fer_send_rtc_message(&fsb, msg.rtc);

  return false;
}


bool Fer_Trx_API::send_timer(u32 a, u8 g, u8 m, time_t rtc, const Fer_TimerData &td) {
  fer_sbT fsb;
  fill_fsb(fsb, a, g, m, fer_cmd_Program);

  return fer_send_timer_message(&fsb, rtc, &td);
}
bool Fer_Trx_API::send_timer(const Fer_MsgTimer &msg) {
  fer_sbT fsb;
  fill_fsb(fsb, msg.a, msg.g, msg.m, fer_cmd_Program);

  return fer_send_timer_message(&fsb, msg.rtc, &msg.td);
}

bool Fer_Trx_API::send_empty_timer(u32 a, u8 g, u8 m, time_t rtc) {
  fer_sbT fsb;
  fill_fsb(fsb, a, g, m, fer_cmd_Program);

  return fer_send_empty_timer_message(&fsb, rtc);

  return false;
}
bool Fer_Trx_API::send_empty_timer(const Fer_MsgRtc &msg) {
  fer_sbT fsb;
  fill_fsb(fsb, msg.a, msg.g, msg.m, fer_cmd_Program);

  return fer_send_empty_timer_message(&fsb, msg.rtc);

  return false;
}
