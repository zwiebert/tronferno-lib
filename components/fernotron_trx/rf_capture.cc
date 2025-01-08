#include <fernotron_trx/raw/rf_capture.hh>
#include <utils_misc/int_macros.h>

#include <esp_attr.h>
#include <string.h>

static volatile uint8_t *c_buf;
static volatile unsigned cap_buf_size;
static volatile unsigned bit_idx;
static volatile unsigned byte_idx;

void IRAM_ATTR rfCapture_sample_level(bool level) {
  if (!c_buf)
    return;
  if (byte_idx >= cap_buf_size)
    return;

  static uint8_t b_buf;

  if (bit_idx >= 8) {
    bit_idx = 0;
    c_buf[byte_idx] = b_buf;
    byte_idx = byte_idx + 1;
    b_buf = 0;
  }

  PUT_BIT(b_buf, bit_idx, level);

  bit_idx = bit_idx + 1;
}

bool rfCapture_init(unsigned buf_size) {
  cap_buf_size = 0;
  bit_idx = 0;
  byte_idx = 0;

  if (void *p = realloc((void*) c_buf, buf_size); p) {
    memset(p, 0, buf_size);
    cap_buf_size = buf_size;
    c_buf = (volatile uint8_t*) p;
    return true;
  }

  free((void*) c_buf);
  c_buf = 0;
  return false;
}

bool rfCapture_get(uint8_t **buf, unsigned *buf_size) {
  if (!c_buf)
    return false;
  if (byte_idx < cap_buf_size)
    return false;
  *buf = (uint8_t *)c_buf;
  *buf_size = cap_buf_size;
  return true;
}
