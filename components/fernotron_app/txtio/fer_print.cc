#include <stdint.h>

#include "fernotron_trx/raw/fer_fsb.h"
#include "fernotron_trx/raw/fer_msg_attachment.h"
#include "utils_misc/int_macros.h"
#include <fernotron_trx/fer_trx_c_api.h>
#include <fernotron_trx/fer_trx_api.hh>

static void
io_putd(int n) {
  printf("%d", n);
}

static void
printBCD(uint8_t bcd) {
  printf("%x%x", GET_HIGH_NIBBLE(bcd), GET_LOW_NIBBLE(bcd));
}
#define io_puts(s)  fputs((s), stdout)
#define io_putlf()  fputs("\r\n", stdout)
#include <utils_misc/itoa.h>
static void
io_print_hex(uint32_t n, bool prefix) {
  char s[10];
  ltoa(n, s, 16);
  if (prefix)
    io_puts("0x");
  io_puts(s);
}

static void
io_print_hex_8(uint8_t n, bool comma) {
  char s[3];
  itoa(n, s, 16);
  io_puts((n & 0xF0) ? "0x" : "0x0");
  io_puts(s);
  if (comma)
    io_puts(", ");
}

static void
print_array_8(const uint8_t *src, int len) {
  int i;

  for (i = 0; i < len; ++i) {
    io_print_hex_8(src[i], true);
  }
  io_putlf();
}

// diagnostic output
static void frb_printPacket(const union fer_cmd_row *cmd);
static void fpr_printPrgPacketInfo(uint8_t d[FER_PRG_PACK_CT][FER_PRG_BYTE_CT], bool rtc_only);

static void frb_printPacket(const union fer_cmd_row *cmd) {
  int i;

  for (i = 0; i < FER_CMD_BYTE_CT; ++i) {
    io_print_hex_8(cmd->bd[i], true);
  }
  io_putlf();
}

static void
printTimerStamp(uint8_t d[18][9], int row, int col) {
  printBCD(d[row][col+1]);
  io_puts(":");
  printBCD(d[row][col]);
}

static void  fpr_printPrgPacketInfo(uint8_t d[FER_PRG_PACK_CT][FER_PRG_BYTE_CT], bool rtc_only) {
  int row, col;

const char *wdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

  io_puts("RTC: ");
  printBCD(d[0][fpr0_RTC_days]);
  io_puts(".");
  printBCD(d[0][fpr0_RTC_mont]);
  io_puts(".  ");
  io_puts(wdays[7 & d[0][fpr0_RTC_wday]]);
  io_puts("  ");
  printBCD(d[0][fpr0_RTC_hour]);
  io_puts(":");
  printBCD(d[0][fpr0_RTC_mint]);
  io_puts(":");
  printBCD(d[0][fpr0_RTC_secs]);
  if (GET_BIT(d[0][fpr0_FlagBits], flag_DST))
	  io_puts(" DST");
 io_putlf();



  if (rtc_only)
  return;

  if (GET_BIT(d[0][fpr0_FlagBits], flag_SunAuto)) {
    io_puts("Sun Automatic is on\n");
  }

  if (GET_BIT(d[0][fpr0_FlagBits], flag_Random)) {
    io_puts("Random Mode is on\n");
  }


  io_puts("Timer data:\n");
  for (row = FER_FPR_TIMER_START_ROW; row < (FER_FPR_TIMER_START_ROW + FER_FPR_TIMER_HEIGHT); ++row) {
    for (col = 0; (col+1) < FER_FPR_TIMER_WIDTH; col += 2) {
      printTimerStamp(d, row, col);
      io_puts(", ");
    }
    io_putlf();
  }

  io_puts("Astro data:\n");
  for (row = FER_FPR_ASTRO_START_ROW; row < (FER_FPR_ASTRO_START_ROW + FER_FPR_ASTRO_HEIGHT); ++row) {
    for (col = 0; (col+1) < FER_FPR_ASTRO_WIDTH; col += 2) {
      //    for (col = (FPR_ASTRO_WIDTH - 2); col >= 0; col -= 2) {
      printTimerStamp(d, row, col);
      io_puts(", ");
    }
    io_putlf();
  }

}

//cast data (message after cmd) to byte array
#define fer_msg_get_data(msg) ((uint8_t(*)[FER_PRG_BYTE_CT])(msg)->rtc.bd)
typedef uint8_t(*fer_msg_data)[FER_PRG_BYTE_CT];

void  fer_msg_print(const char *tag, const fer_rawMsg *msg, fer_msg_kindT t, bool verbose) {
  io_puts(tag);
  frb_printPacket(&msg->cmd);

#ifndef FER_RECEIVER_MINIMAL
  if (t == fer_msg_kindT::MSG_TYPE_RTC || t == fer_msg_kindT::MSG_TYPE_TIMER) {
    int i, used_lines;
    fer_msg_data prg = fer_msg_get_data(msg);

    used_lines = t == fer_msg_kindT::MSG_TYPE_RTC ? FER_RTC_PACK_CT : FER_PRG_PACK_CT;
    if (verbose) {
      for (i = 0; i < used_lines; ++i) {
        print_array_8(prg[i], FER_PRG_BYTE_CT);
      }
    } else {
      fpr_printPrgPacketInfo(prg, used_lines == 1);
    }

  }
#endif
}

void  fer_msg_print_as_cmdline(const char *tag, const fer_rawMsg *msg, fer_msg_kindT t) {

  const fer_sbT *fsb = (fer_sbT*) msg;

  if (t != fer_msg_kindT::MSG_TYPE_PLAIN && t !=  fer_msg_kindT::MSG_TYPE_PLAIN_DOUBLE)
    return; // ignore long messages for now



  fer_if_cmd c = (fer_if_cmd)FER_SB_GET_CMD(fsb);
  uint32_t id = FER_SB_GET_DEVID(fsb);

  const char *cs = 0;
  const char *fdt = 0;

  if ((FER_SB_ADDR_IS_PLAIN(fsb) && (fdt = "plain"))
      || (FER_SB_ADDR_IS_CENTRAL(fsb) && (fdt = "central"))) {
    switch (c) {
    case fer_if_cmd_DOWN:
      cs = "down";
      break;
    case fer_if_cmd_UP:
      cs = "up";
      break;
    case fer_if_cmd_STOP:
      cs = "stop";
      break;
    default:
      cs = 0;
      break;
    }
  } else if (FER_SB_ADDR_IS_SUNSENS(fsb) && (fdt = "sun")) {
    switch (c) {
    case fer_if_cmd_SunDOWN:
      cs = "sun-down";
      break;
    case fer_if_cmd_SunUP:
      cs = "sun-up";
      break;
    case fer_if_cmd_SunINST:
      cs = "sun-pos";
      break;
    case fer_if_cmd_Program:
      cs = "sun-test";
      break;
    default:
      cs = 0;
      break;
    }
  }

  if (!cs)
    return; // unsupported command

  io_puts(tag);

  io_puts("type="), io_puts(fdt),
  io_puts(" a="), io_print_hex(id, false);
  if (FER_SB_ADDR_IS_CENTRAL(fsb)) {
    uint8_t g = FER_SB_GET_GRP(fsb);
    uint8_t m = FER_SB_GET_MEMB(fsb);
    if (g != 0) {
      io_puts(" g="), io_putd(g);
      if (m != 0) {
        m -= 7;
        io_puts(" m="), io_putd(m);
      }
    }
  }
  io_puts( " c="), io_puts(cs);
  io_puts(";\n");
}


