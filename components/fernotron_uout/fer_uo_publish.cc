#include "fernotron_uout/fer_uo_publish.h"

#include <uout/uo_callbacks.h>

// app
#include <fernotron_trx/fer_trx_api.hh>
#include <fernotron_trx/timer_data.h>
#include "fernotron_trx/raw/fer_fsb.h"
#include "fernotron_trx/raw/fer_msg_attachment.h"
#include <fernotron/fer_main.h>
#include <fernotron/auto/fau_tminutes.h>

// shared
#include <utils_misc/int_macros.h>
#include <utils_misc/cstring_utils.hh>
#include <uout/uout_builder_json.hh>
#include <debug/dbg.h>

// sys
#include <stdio.h>
#include <string.h>

void uoApp_publish_pctChange_gmp(const so_arg_gmp_t a[], size_t len, uo_flagsT tgtFlags) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_pctChange = true;
  flags.evt.gen_app_state_change = true;

  flags.fmt.raw = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    {
      for (auto i = 0; i < len; ++i) {
        uoCb_publish(idxs, &a[i], flags);
      }
    }
  }

  flags.fmt.raw = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    UoutBuilderJson sj;  // use heap instead stack, because this may need up to 400 bytes, if all  possible 49 receivers are moving

    if (sj.open_root_object("tfmcu")) {
      sj.add_object("pct");
      for (int i = 0; i < len; ++i) {
        char buf[] = "00";
        buf[0] += a[i].g;
        buf[1] += a[i].m;
        sj.add_key_value_pair_d(buf, a[i].p);
      }
      sj.close_object();
      sj.close_root_object();

      uo_flagsT flags;
      flags.fmt.json = true;
      flags.evt.uo_evt_flag_pctChange = true;

      uoCb_publish(idxs, sj.get_json(), flags);
    }
  }

#ifdef CONFIG_TRONFERNO_PUBLISH_PCT_AS_TXT
  flags.fmt.json = false;
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[64];
    for (auto i = 0; i < len; ++i) {
      snprintf(buf, sizeof buf, "A:position: g=%d m=%d p=%d;", a[i].g, a[i].m, a[i].p);
      uoCb_publish(idxs, buf, flags);
    }
  }
#endif

}

void uoApp_publish_pctChange_gmp(const so_arg_gmp_t a, uo_flagsT tgtFlags) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_pctChange = true;
  flags.evt.gen_app_state_change = true;

  flags.fmt.raw = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    uoCb_publish(idxs, &a, flags);
  }

  flags.fmt.raw = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char sj_buf[256];
    UoutBuilderJson sj = { sj_buf, sizeof sj_buf };

    if (sj.open_root_object("tfmcu")) {
      sj.add_object("pct");
      char buf[] = "00";
      buf[0] += a.g;
      buf[1] += a.m;
      sj.add_key_value_pair_d(buf, a.p);
      sj.close_object();
      sj.close_root_object();

      uoCb_publish(idxs, sj.get_json(), flags);
    }
  }

#ifdef CONFIG_TRONFERNO_PUBLISH_PCT_AS_TXT
  flags.fmt.json = false;
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[64];
    snprintf(buf, sizeof buf, "A:position: g=%d m=%d p=%d;", a.g, a.m, a.p);
    uoCb_publish(idxs, buf, flags);
  }
#endif

}

void uoApp_publish_timer_json(const char *json, bool fragment) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_timerChange = true;
  flags.evt.gen_app_state_change = true;

  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {

    if (!fragment) {
      uoCb_publish(idxs, json, flags);
    } else {
      size_t json_len = strlen(json);
      char buf[json_len + 4] = "{";
      strcat(buf, json);
      buf[json_len] = '}'; // overwrite trailing comma
      buf[json_len + 1] = '\0';

      uoCb_publish(idxs, buf, flags);
    }
  }
}

static void timer_json(UoutWriter &td, uint8_t g, uint8_t m, struct Fer_TimerData &tdr) {
  {
    char dict[] = "autoGM";
    dict[4] = '0' + g;
    dict[5] = '0' + m;
    td.sj().add_object(dict);
  }

  {
    bool f_manual = manual_bits.getMember(g, m);
    char flags[10], *p = flags;
    *p++ = f_manual ? 'M' : 'm';
    *p++ = tdr.getRandom() ? 'R' : 'r';
    *p++ = tdr.getSunAuto() ? 'S' : 's';
    *p++ = tdr.hasAstro() ? 'A' : 'a';
    *p++ = tdr.hasDaily() ? 'D' : 'd';
    *p++ = tdr.hasWeekly() ? 'W' : 'w';
    *p++ = '\0';
    td.so().print("f", flags);
  }

  if (tdr.hasDaily()) {
    td.so().print("daily", tdr.getDaily());
  }

  if (tdr.hasWeekly()) {
    td.so().print("weekly", tdr.getWeekly());
  }

  if (tdr.hasAstro()) {
    td.so().print("astro", tdr.getAstro());

    Fer_TimerMinutes tmi;
    fer_au_get_timer_minutes_from_timer_data_tm(&tmi, &tdr);
    td.so().print("asmin", tmi.minutes[FER_MINTS_ASTRO]);
  }

  td.so().x_close();
}

void uoApp_publish_timer_json(uint8_t g, uint8_t m, struct Fer_TimerData *tda) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_timerChange = true;
  flags.evt.gen_app_state_change = true;

  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    UoutWriterBuilder td { SO_TGT_HTTP | SO_TGT_FLAG_JSON };
    td.sj().open_root_object("tfmcu.publish");
    td.sj().add_object("auto");
    timer_json(td, g, m, *tda);
    td.sj().close_object();
    td.sj().close_root_object();

    uoCb_publish(idxs, td.sj().get_json(), flags);
  }
}

struct cmdInfo {
  const char *cs = 0;
  const char *fdt = 0;
};
static cmdInfo cmdString_fromPlainCmd(const Fer_MsgPlainCmd &m) {
  struct cmdInfo r;

  if ((FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_PlainSender) && (r.fdt = "plain")) || (FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_CentralUnit) && (r.fdt = "central"))
      || (FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_Receiver) && (r.fdt = "receiver"))) {
    switch (m.cmd) {
    case fer_if_cmd_DOWN:
      r.cs = "down";
      break;
    case fer_if_cmd_UP:
      r.cs = "up";
      break;
    case fer_if_cmd_STOP:
      r.cs = "stop";
      break;
    case fer_if_cmd_SunDOWN:
      r.cs = "sun-down";
      break;
    case fer_if_cmd_SunUP:
      r.cs = "sun-up";
      break;
    case fer_if_cmd_SunINST:
      r.cs = "sun-pos";
      break;
    case fer_if_cmd_EndPosDOWN:
      r.cs = "sep-down";
      break;
    case fer_if_cmd_EndPosUP:
      r.cs = "sep-up";
      break;
    case fer_if_cmd_ToggleRotationDirection:
      r.cs = "rot-dir";
      break;
    default:
      r.cs = 0;
      break;
    }
  } else if (FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_SunSensor) && (r.fdt = "sun")) {
    switch (m.cmd) {
    case fer_if_cmd_SunDOWN:
      r.cs = "sun-down";
      break;
    case fer_if_cmd_SunUP:
      r.cs = "sun-up";
      break;
    case fer_if_cmd_SunINST:
      r.cs = "sun-pos";
      break;
    case fer_if_cmd_Program:
      r.cs = "sun-test";
      break;
    default:
      r.cs = 0;
      break;
    }
  }
  return r;
}

void uoApp_publish_fer_msgSent(const struct Fer_MsgTimer *msg) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_msgSent = true;
  flags.evt.gen_app_state_change = true;

  const Fer_MsgTimer &m = *msg;
  if (!FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_CentralUnit))
    return;

  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];

    snprintf(buf, sizeof buf, "SA:type=central: a=%06lx g=%d m=%d;",(long unsigned) m.a, m.g, m.m);
    uoCb_publish(idxs, buf, flags);
  }

  flags.fmt.txt = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];

    snprintf(buf, sizeof buf, "{\"sa\":{\"type\":\"central\",\"a\":\"%06lx\",\"g\":%d,\"m\":%d}}",(long unsigned) m.a, m.g, m.m);
    uoCb_publish(idxs, buf, flags);
  }
}

void uoApp_publish_fer_msgSent(const struct Fer_MsgRtc *msg) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_msgSent = true;
  flags.evt.gen_app_state_change = true;

  const Fer_MsgRtc &m = *msg;
  if (!FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_CentralUnit))
    return;

  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];

    snprintf(buf, sizeof buf, "ST:type=central: a=%06lx g=%d m=%d;",(long unsigned) m.a, m.g, m.m);
    uoCb_publish(idxs, buf, flags);
  }

  flags.fmt.txt = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];

    snprintf(buf, sizeof buf, "{\"st\":{\"type\":\"central\",\"a\":\"%06lx\",\"g\":%d,\"m\":%d}}",(long unsigned) m.a, m.g, m.m);
    uoCb_publish(idxs, buf, flags);
  }
}


void uoApp_publish_fer_msgSent(const struct Fer_MsgPlainCmd *msg) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_msgSent = true;
  flags.evt.gen_app_state_change = true;

  const Fer_MsgPlainCmd &m = *msg;
  cmdInfo ci = cmdString_fromPlainCmd(m);
  if (!ci.cs)
    return; // unsupported command

#ifdef CONFIG_TRONFERNO_PUBLISH_TRX_AS_TXT
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];
    if (FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_CentralUnit)) {
      snprintf(buf, sizeof buf, "SC:type=central: a=%06lx g=%d m=%d c=%s;",(long unsigned) m.a, m.g, m.m, ci.cs);
    } else {
      snprintf(buf, sizeof buf, "SC:type=%s: a=%06lx g=%d m=%d c=%s;", ci.fdt,(long unsigned) m.a, m.g, m.m, ci.cs);
    }
    uoCb_publish(idxs, buf, flags);
  }
#endif

  flags.fmt.txt = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];
    if (FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_CentralUnit)) {
      snprintf(buf, sizeof buf, "{\"sc\":{\"type\":\"central\",\"a\":\"%06lx\",\"g\":%d,\"m\":%d,\"c\":\"%s\"}}",(long unsigned) m.a, m.g, m.m, ci.cs);
    } else {
      snprintf(buf, sizeof buf, "{\"sc\":{\"type\":\"%s\",\"a\":\"%06lx\",\"c\":\"%s\"}}", ci.fdt,(long unsigned) m.a, ci.cs);
    }
    uoCb_publish(idxs, buf, flags);
  }
}

void uoApp_publish_fer_msgReceived(const struct Fer_MsgPlainCmd *msg) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_rfMsgReceived = true;
  flags.evt.gen_app_state_change = true;

  const int rssi = Fer_Trx_API::get_rssi();

  const Fer_MsgPlainCmd &m = *msg;
  cmdInfo ci = cmdString_fromPlainCmd(m);
  if (!ci.cs)
    return; // unsupported command

#ifdef CONFIG_TRONFERNO_PUBLISH_TRX_AS_TXT
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[64];
    if (FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_CentralUnit)) {
      snprintf(buf, sizeof buf, "RC:type=central: a=%06lx g=%d m=%d c=%s rssi=%d;",(long unsigned) m.a, m.g, m.m, ci.cs, rssi);
    } else {
      snprintf(buf, sizeof buf, "RC:type=%s: a=%06lx g=%d m=%d c=%s rssi=%d;", ci.fdt,(long unsigned) m.a, m.g, m.m, ci.cs, rssi);
    }
    uoCb_publish(idxs, buf, flags);
  }
#endif

  flags.fmt.txt = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[128];
    if (FER_U32_TEST_TYPE(m.a, FER_ADDR_TYPE_CentralUnit)) {
      snprintf(buf, sizeof buf, "{\"rc\":{\"type\":\"central\",\"a\":\"%06lx\",\"g\":%d,\"m\":%d,\"c\":\"%s\",\"rssi\":%d}}",(long unsigned) m.a, m.g, m.m, ci.cs, rssi);
    } else {
      snprintf(buf, sizeof buf, "{\"rc\":{\"type\":\"%s\",\"a\":\"%06lx\",\"c\":\"%s\",\"rssi\":%d}}", ci.fdt,(long unsigned) m.a, ci.cs, rssi);
    }
    uoCb_publish(idxs, buf, flags);
  }
}

void uoApp_publish_fer_msgRtcReceived(const Fer_MsgPlainCmd &c, const struct fer_raw_msg &m) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_msgSent = true;
  flags.evt.gen_app_state_change = true;


  const struct fer_rtc_sd &r = m.rtc.sd;

  const int rssi = Fer_Trx_API::get_rssi();

#ifdef CONFIG_TRONFERNO_PUBLISH_TRX_AS_TXT
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];

    snprintf(buf, sizeof buf, "RC:type=central: a=%06lx g=%d m=%d c=rtc rssi=%d t=%02x-%02xT%02x:%02x:%02x;", (long unsigned)c.a, c.g, c.m, rssi, r.mont, r.mday, r.hour, r.mint, r.secs);
    uoCb_publish(idxs, buf, flags);
  }
#endif

  flags.fmt.txt = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[128];

    snprintf(buf, sizeof buf, "{\"rc\":{\"type\":\"central\",\"a\":\"%06lx\",\"g\":%d,\"m\":%d,\"c\":\"rtc\",\"rssi\":%d,\"t\":\"%02x-%02xT%02x:%02x:%02x\"}}", (long unsigned)c.a, c.g, c.m, rssi, r.mont, r.mday, r.hour, r.mint, r.secs);
    uoCb_publish(idxs, buf, flags);
  }

}

void uoApp_publish_fer_msgAutoReceived(const struct Fer_MsgPlainCmd &c, const struct fer_raw_msg &m) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_msgSent = true;
  flags.evt.gen_app_state_change = true;


  const struct fer_rtc_sd &r = m.rtc.sd;
  const int rssi = Fer_Trx_API::get_rssi();

  char af[] = "rsdw";

  if (r.flags.bits.random)
    af[0] = 'R';
  if (r.flags.bits.sunAuto)
    af[1] = 'S';

  for (int i = 0; i < 4; ++i)
    for (int k = 0; k < 8; k += 2) {
      if (m.wdtimer.bd[i][k] != 0xff) {
        if (i == 3 && k >= 4) {
          af[2] = 'D';
        } else {
          af[3] = 'W';
        }
      }
    }

#ifdef CONFIG_TRONFERNO_PUBLISH_TRX_AS_TXT
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];

    snprintf(buf, sizeof buf, "RC:type=central: a=%06lx g=%d m=%d c=auto rssi=%d t=%02x-%02xT%02x:%02x:%02x f=%s;", (long unsigned)c.a, c.g, c.m, rssi, r.mont, r.mday, r.hour,
        r.mint, r.secs, af);
    uoCb_publish(idxs, buf, flags);
  }
#endif

  flags.fmt.txt = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[128];

    snprintf(buf, sizeof buf,
        "{\"rc\":{\"type\":\"central\",\"a\":\"%06lx\",\"g\":%d,\"m\":%d,\"c\":\"auto\",\"rssi\":%d,\"t\":\"%02x-%02xT%02x:%02x:%02x\",\"f\":\"%s\"}}", (long unsigned)c.a, c.g,
        c.m, rssi, r.mont, r.mday, r.hour, r.mint, r.secs, af);
    uoCb_publish(idxs, buf, flags);
  }

}

void uoApp_publish_fer_msgRtcSent(const struct Fer_MsgPlainCmd &c, const struct fer_raw_msg &m) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_msgSent = true;
  flags.evt.gen_app_state_change = true;

  const struct fer_rtc_sd &r = m.rtc.sd;

#ifdef CONFIG_TRONFERNO_PUBLISH_TRX_AS_TXT
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];

    snprintf(buf, sizeof buf, "SC:type=central: a=%06lx g=%d m=%d c=rtc t=%02x-%02xT%02x:%02x:%02x;", (long unsigned)c.a, c.g, c.m, r.mont, r.mday, r.hour, r.mint, r.secs);
    uoCb_publish(idxs, buf, flags);
  }
#endif

  flags.fmt.txt = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[128];

    snprintf(buf, sizeof buf, "{\"sc\":{\"type\":\"central\",\"a\":\"%06lx\",\"g\":%d,\"m\":%d,\"c\":\"rtc\",\"t\":\"%02x-%02xT%02x:%02x:%02x\"}}", (long unsigned)c.a, c.g, c.m, r.mont,
        r.mday, r.hour, r.mint, r.secs);
    uoCb_publish(idxs, buf, flags);
  }

}

void uoApp_publish_fer_msgAutoSent(const struct Fer_MsgPlainCmd &c, const struct fer_raw_msg &m) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_msgSent = true;
  flags.evt.gen_app_state_change = true;

  const struct fer_rtc_sd &r = m.rtc.sd;

  char af[] = "rsdw";

  if (r.flags.bits.random)
    af[0] = 'R';
  if (r.flags.bits.sunAuto)
    af[1] = 'S';

  for (int i = 0; i < 4; ++i)
    for (int k = 0; k < 8; k += 2) {
      if (m.wdtimer.bd[i][k] != 0xff) {
        if (i == 3 && k >= 4) {
          af[2] = 'D';
        } else {
          af[3] = 'W';
        }
      }
    }

#ifdef CONFIG_TRONFERNO_PUBLISH_TRX_AS_TXT
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[80];

    snprintf(buf, sizeof buf, "SC:type=central: a=%06lx g=%d m=%d c=auto t=%02x-%02xT%02x:%02x:%02x f=%s;", (long unsigned)c.a, c.g, c.m, r.mont, r.mday, r.hour, r.mint,
        r.secs, af);
    uoCb_publish(idxs, buf, flags);
  }
#endif

  flags.fmt.txt = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char buf[128];

    snprintf(buf, sizeof buf, "{\"sc\":{\"type\":\"central\",\"a\":\"%06lx\",\"g\":%d,\"m\":%d,\"c\":\"auto\",\"t\":\"%02x-%02xT%02x:%02x:%02x\",\"f\":\"%s\"}}",
        (long unsigned)c.a, c.g, c.m, r.mont, r.mday, r.hour, r.mint, r.secs, af);
    uoCb_publish(idxs, buf, flags);
  }
}

void uoApp_publish_fer_prasState(const so_arg_pras_t args) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_PRAS = true;
  flags.evt.gen_app_state_change = true;

  flags.fmt.raw = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    uoCb_publish(idxs, &args, flags);
  }

  flags.fmt.raw = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char a_hex[20];
    ltoa(args.a, a_hex, 16);
    char sj_buf[256];
    UoutBuilderJson sj = { sj_buf, sizeof sj_buf };

    if (sj.open_root_object("tfmcu")) {

      sj.add_object("pras");
      sj.add_key_value_pair_s("a", a_hex);
      sj.add_key_value_pair_d("scanning", args.scanning);
      sj.add_key_value_pair_d("success", args.success);
      sj.add_key_value_pair_d("timeout", args.timeout);
      sj.add_key_value_pair_d("pairing", args.pairing);
      sj.close_object();

      sj.close_root_object();

      uoCb_publish(idxs, sj.get_json(), flags);
    }
  }
}

void uoApp_publish_fer_cuasState(const so_arg_cuas_t args) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_CUAS = true;
  flags.evt.gen_app_state_change = true;

  flags.fmt.raw = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    uoCb_publish(idxs, &args, flags);
  }

  flags.fmt.raw = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    char a_hex[20];
    ltoa(args.a, a_hex, 16);

    char sj_buf[256];
    UoutBuilderJson sj = { sj_buf, sizeof sj_buf };

    if (sj.open_root_object("tfmcu")) {
      sj.add_object("cuas");
      sj.add_key_value_pair_s("a", a_hex);
      sj.add_key_value_pair_d("scanning", args.scanning);
      sj.add_key_value_pair_d("success", args.success);
      sj.add_key_value_pair_d("timeout", args.timeout);
      sj.close_object();

      int state = args.scanning ? 1 : args.timeout ? 2 : args.success ? 3 : 0;
      sj.add_object("config");
      sj.add_key_value_pair_d("cuas", state);
      sj.close_object();

      sj.close_root_object();

      uoCb_publish(idxs, sj.get_json(), flags);
    }
  }

  flags.fmt.json = false;
  flags.fmt.txt = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    const char *msg = args.success ? "tf:event:cuas=ok:;" : args.timeout ? "tf:event:cuas=time-out:;" : "tf:event:cuas=scanning:;";

    uoCb_publish(idxs, msg, flags);
  }
}

void uoApp_publish_fer_sepState(const so_arg_sep_t args, char tag) {
  uo_flagsT flags;
  flags.evt.uo_evt_flag_SEP = true;
  flags.evt.gen_app_state_change = true;

  flags.fmt.raw = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {
    uoCb_publish(idxs, &args, flags);
  }

  flags.fmt.raw = false;
  flags.fmt.json = true;
  if (auto idxs = uoCb_filter(flags); idxs.size) {

    char sj_buf[256];
    UoutBuilderJson sj = { sj_buf, sizeof sj_buf };

    if (sj.open_root_object("tfmcu")) {
      sj.add_object("sep");
      if (args.auth_success)
        sj.add_key_value_pair_d("auth-success", 1);
      if (args.auth_terminated)
        sj.add_key_value_pair_d("auth-terminated", 1);
      if (args.auth_button_timeout)
        sj.add_key_value_pair_d("auth-button-timeout", 1);
      if (args.auth_button_error)
        sj.add_key_value_pair_d("auth-button-error", 1);
      if (args.auth_timeout)
        sj.add_key_value_pair_d("auth-timeout", 1);
      if (args.ui_timeout)
        sj.add_key_value_pair_d("ui-timeout", 1);
      sj.close_object();

      sj.close_root_object();

      uoCb_publish(idxs, sj.get_json(), flags);
    }
  }
}
