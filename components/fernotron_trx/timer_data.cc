/**
 * \file   fernotron_trx/timer_data.cc
 * \brief  vocabulary types to program Fernotron automatic (timers)
 */

#include <fernotron_trx/timer_data.h>
#include <fernotron_trx/raw/fer_msg_attachment.h>

#include <stdio.h>


static int bcdtime2string(char *dp, int dst_len, const fer_time &du) {
  if (du) {
    return snprintf(dp, dst_len, "%02u%02u", bcd2dec(du.hour), bcd2dec(du.mint));
  }

  *dp++ = '-';
  *dp++ = '\0';
  return 1;
}

static int bcdtime2string(char *dp, int dst_len, const fer_timer &tmr, const fer_timer *prev = 0) {
  if (prev && tmr == *prev) {
    *dp++ = '+';
    *dp++ = '\0';
    return 1;
  }

  int ret = bcdtime2string(dp, dst_len, tmr.up);
  ret += bcdtime2string(dp + ret, dst_len - ret, tmr.down);

  return ret;
}

Fer_TimerData::Fer_TimerData(const struct fer_raw_msg &m) {
  putRandom(m.rtc.sd.flags.bits.random);
  putRandom(m.rtc.sd.flags.bits.sunAuto);
  putAstro(m.astro.rows[0].bd[0] != 0x0f); // XXX: should we calculate/guess the minute offset for astro from the timer data?

  char *dp = daily;
  dp += bcdtime2string(dp, 5, m.wdtimer.days.daily.up);
  dp += bcdtime2string(dp, 5, m.wdtimer.days.daily.down);

  char *wp = weekly;
  wp += bcdtime2string(wp, 9, m.wdtimer.days.mon);
  wp += bcdtime2string(wp, 9, m.wdtimer.days.tue, &m.wdtimer.days.mon);
  wp += bcdtime2string(wp, 9, m.wdtimer.days.wed, &m.wdtimer.days.tue);
  wp += bcdtime2string(wp, 9, m.wdtimer.days.thu, &m.wdtimer.days.wed);
  wp += bcdtime2string(wp, 9, m.wdtimer.days.fri, &m.wdtimer.days.thu);
  wp += bcdtime2string(wp, 9, m.wdtimer.days.sat, &m.wdtimer.days.fri);
  wp += bcdtime2string(wp, 9, m.wdtimer.days.sun, &m.wdtimer.days.sat);

}

bool Fer_TimerData::validateTime(const char *t) {
  if (strlen(t) < 4)
    return false;
  const int h10 = t[0] - '0';
  const int h1 = t[1] - '0';
  const int m10 = t[2] - '0';
  const int m1 = t[3] - '0';

  return (0 <= h10 && h10 <= 2) && (0 <= h1 && h1 <= 9) && (h10 != 2 || h1 <= 3) //
      && (0 <= m10 && m10 <= 5) && (0 <= m1 && m1 <= 9);
}

bool Fer_TimerData::validateTimePair(const char *tp) {
  if (strncmp(tp, "--", 2) == 0)
    return false;
  if (strlen(tp) < 5)
    return false;

  if (tp[0] == '-') {
    return validateTime(tp + 1);
  }
  if (!validateTime(tp))
    return false;

  return tp[4] == '-' || validateTime(tp + 4);
}

/**
 * \brief Set daily timer.
 *
 * T  - T is a 8 digit time string like 07302000. The four left digits are the up-time. The four on the right the down-time. A minus sign can replace 4 digits, which means the timer is cleared.
 *
 * timer daily=07302000;   up 07:30, down 20:00
 * timer daily=0730-;      up 07:30, not down
 * timer daily=-2000;      not up,   down 20:00
 */

bool Fer_TimerData::putDaily(const char *dt) {
  precond(!dt || strlen(dt) <= DAILY_MAX_LEN);
  if (!dt || !*dt || strcmp(dt, "null") == 0 || strcmp(dt, "--") == 0) {
    daily[0] = '\0';
    return true;
  }
  if (!validateDaily(dt))
    return false;

  STRLCPY(daily, dt, sizeof daily);
  return true;
}
bool Fer_TimerData::validateDaily(const char *dt) {
  int count_minus = 0;
  int count_digit = 0;

  int i = 0;
  for (const char *p = dt; *p; ++p, ++i) {
    if (*p == '-') {
      ++count_minus;
    } else if (isdigit(*p)) {
      if ((count_digit & 3) == 0 && !validateTime(p))
        return false;

      ++count_digit;
    } else {
      return false;
    }
  }
  if (DAILY_MAX_LEN != (count_digit + 4 * count_minus))
    return false;

  if (count_digit == 0)
    return false;

  if (!validateTimePair(dt))
    return false;

  return true;
}

/**
 * \brief Set weekly timer.
 *
 * weekly=TTTTTTT - sets a timer for each week day. week days are from left to right: Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday
 *
 * T - Each T is a 8 digit time string like described above with daily option.  A plus sign repeats the previous T.  So you can copy the values from Monday to Tuesday and so on.
 *
 * timer weekly=0730-++++0900-+;    up Monday-Friday at 07:30, and Saturday-Sunday at 09:00
 *
 *
 */
bool Fer_TimerData::putWeekly(const char *wt) {
  precond(!wt || strlen(wt) <= WEEKLY_MAX_LEN);

  if (!wt || !*wt || strcmp(wt, "null") == 0 || strcmp(wt, "--++++++") == 0) {
    weekly[0] = '\0';
    return true;
  }

  if (!validateWeekly(wt))
    return false;

  STRLCPY(weekly, wt, sizeof weekly);
  return true;
}

bool Fer_TimerData::validateWeekly(const char *wt) {
  int count_minus = 0;
  int count_plus = 0;
  int count_digit = 0;

  int i = 0;
  for (const char *p = wt; *p; ++p, ++i) {
    if (*p == '-') {
      ++count_minus;
    } else if (*p == '+') {
      if (i == 0)
        return false;
      else if ((count_digit + 4 * count_minus) & 7)
        return false;

      ++count_plus;
    } else if (isdigit(*p)) {
      ++count_digit;
    } else {
      return false;
    }
  }
  if (WEEKLY_MAX_LEN != (count_digit + 4 * count_minus + 8 * count_plus))
    return false;

  return true;
}
