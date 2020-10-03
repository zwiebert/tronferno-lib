/*
 * fer_api.h
 *
 *  Created on: 18.02.2020
 *      Author: bertw
 */

#pragma once

#include "fer_app_cfg.h"
#include "fernotron/trx/raw/fer_msg_plain.h"
#include "fernotron/trx/timer_data.h"
#include "fernotron/trx/raw/fer_fsb.h"
#include "time.h"

bool fer_timerString2bcd(const char *src, uint8_t *dst, uint16_t size_dst);
bool fer_fill_timer_buf(fer_sbT *fsb, time_t rtc, const Fer_TimerData *tdr);
bool fer_fill_rtc_buf(fer_sbT *fsb, time_t rtc);

#include "fernotron/astro.h"
#include "fernotron/trx/raw/fer_msg_attachment.h"
// calculate and fill in astro data
void fer_astro_write_data(fer_astro_byte_data d, int mint_offset);