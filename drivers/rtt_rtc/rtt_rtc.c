/*
 * Copyright (C) 2020 Benjamin Valentin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     drivers_periph_rtc
 * @{
 *
 * @file
 * @brief       Basic RTC implementation based on a RTT
 *
 * @note        Unlike a real RTC, this emulated version is not guaranteed to keep
 *              time across reboots or deep sleep.
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <stdlib.h>
#include <string.h>

#include "periph/rtc.h"
#include "periph/rtt.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

#define RTT_SECOND (RTT_FREQUENCY)
#define RTT_MINUTE (RTT_SECOND * 60UL)
#define RTT_HOUR   (RTT_MINUTE * 60UL)
#define RTT_DAY    (RTT_HOUR   * 24UL)

#define _RTT(n) ((n) & RTT_MAX_VALUE)

#define RTT_SECOND_MAX  (RTT_MAX_VALUE/RTT_FREQUENCY)
#define RTT_PERIOD_MAX  (RTT_SECOND_MAX * RTT_SECOND)

#define TICKS(x)        ((x) * RTT_SECOND)
#define SECONDS(x)      (_RTT(x) / RTT_SECOND)

static uint32_t alarm_time;     /*< The RTC timestamp of the next alarm */
static uint32_t rtc_now;        /*< The last time the RTT overflowed */

static uint32_t last_alarm;     /*< last time the RTC was updated */

static rtc_alarm_cb_t alarm_cb; /*< RTC alarm callback */
static void *alarm_cb_arg;      /*< RTC alarm callback argument */

static void _rtt_alarm(void *arg);

/* convert RTT counter into RTC timestamp */
static inline uint32_t _rtc_now(uint32_t now)
{
    return rtc_now + SECONDS(now - last_alarm);
}

static inline void _set_alarm(uint32_t now, uint32_t next_alarm)
{
    DEBUG("Next alarm in %u ticks (%u)\n", next_alarm, _RTT(now + next_alarm));
    rtt_set_alarm(now + next_alarm, _rtt_alarm, NULL);
}

static void _rtt_alarm(void *arg) {
    (void) arg;

    uint32_t next_alarm;
    uint32_t now = rtt_get_counter();

    DEBUG("%u seconds (%u ticks) passed since last alarm\n", SECONDS(now - last_alarm), now - last_alarm);

    rtc_now = _rtc_now(now);
    last_alarm = now;

    DEBUG("[%u] now: %u\n", rtc_now, now);

    if (alarm_cb && (rtc_now == alarm_time)) {
        rtc_alarm_cb_t cb = alarm_cb;
        alarm_cb = NULL;

        cb(alarm_cb_arg);
    }

    if (alarm_cb && (alarm_time - rtc_now <= RTT_SECOND_MAX)) {
        next_alarm = TICKS(alarm_time - rtc_now);
    } else {
        next_alarm = RTT_PERIOD_MAX;
    }

    _set_alarm(now, next_alarm);
}

static int _update_alarm(uint32_t now)
{
    if (alarm_time < rtc_now) {
        return -1;
    }

    if (alarm_time - rtc_now > RTT_SECOND_MAX) {
        return 0;
    }

    uint32_t next_alarm = TICKS(alarm_time - rtc_now);
    _set_alarm(now, next_alarm);

    return 0;
}

void rtc_init(void)
{
    last_alarm = rtt_get_counter();
    _set_alarm(last_alarm, RTT_PERIOD_MAX);
}

int rtc_set_time(struct tm *time)
{
    rtc_tm_normalize(time);

    uint32_t now = rtt_get_counter();
    rtc_now      = rtc_mktime(time);
    last_alarm   = TICKS(SECONDS(now));

    /* update the RTT alarm if nececcary */
    if (alarm_cb) {
        _update_alarm(now);
    }

    return 0;
}

int rtc_get_time(struct tm *time)
{
    uint32_t now = rtt_get_counter();
    uint32_t tmp = _rtc_now(now);

    rtc_localtime(tmp, time);

    return 0;
}

int rtc_get_alarm(struct tm *time)
{
    rtc_localtime(alarm_time, time);

    return 0;
}

int rtc_set_alarm(struct tm *time, rtc_alarm_cb_t cb, void *arg)
{
    uint32_t now = rtt_get_counter();
    alarm_time   = rtc_mktime(time);
    alarm_cb     = cb;
    alarm_cb_arg = arg;

    return _update_alarm(now);
}

void rtc_clear_alarm(void)
{
    rtt_clear_alarm();
    alarm_cb = NULL;
}

void rtc_poweron(void)
{
    rtt_poweron();
}

void rtc_poweroff(void)
{
    rtt_poweroff();
}
