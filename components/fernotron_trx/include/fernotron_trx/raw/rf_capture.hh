#include <stdint.h>

void rfCapture_sample_level(bool level);
bool rfCapture_init(unsigned buf_size);
bool rfCapture_get(uint8_t **buf, unsigned *buf_size);
