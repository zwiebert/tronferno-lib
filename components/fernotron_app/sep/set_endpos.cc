
#include "fernotron/sep/set_endpos.h"
#include "fernotron_trx/fer_trx_api.hh"
#include "fernotron_uout/fer_uo_publish.h"
#include <fernotron_trx/fer_trx_c_api.h>

#include <utils_time/time_out_secs.hh>

#include "sep.hh"

#ifdef CONFIG_FERNOTRON_APP_DEBUG
#define DEBUG
#define DT(x) x
#define D(x) x
#else
#define DT(x)
#define D(x)
#endif
#define logtag "ferno.app"

void (*fer_sep_enable_disable_cb)(bool enable);



static Sep sep;

bool fer_sep_move_up(uint32_t auth_key) {
  return sep.move_up(auth_key);
}
bool fer_sep_move_down(uint32_t auth_key) {
  return sep.move_down(auth_key);
}
bool fer_sep_move_continue(uint32_t auth_key) {
  return sep.move_continue(auth_key);
}
bool fer_sep_move_stop(uint32_t auth_key) {
  return sep.move_stop(auth_key);
}
bool fer_sep_move_test() {
  return sep.move_test();
}

bool fer_sep_authenticate(class UoutWriter &td, uint32_t auth_key, int sep_mode_timeout_secs, int button_timeout_secs) {
  return sep.authenticate(auth_key, sep_mode_timeout_secs, button_timeout_secs);
}

bool fer_sep_deauthenticate(class UoutWriter &td, uint32_t auth_key) {
  return sep.deauthenticate(auth_key);
}

void fer_sep_disable(void) {
  sep.disable();
}

bool fer_sep_enable(class UoutWriter &td, uint32_t auth_key, const uint32_t a, const uint8_t g, const uint8_t m, int enable_timeout_secs, int button_timeout_secs) {
  return sep.enable(auth_key, a, g, m, enable_timeout_secs, button_timeout_secs);
}

void fer_sep_loop(void) {
  sep.work_loop();
}

bool fer_sep_is_enabled() {
  return sep.isEnabled();
}

