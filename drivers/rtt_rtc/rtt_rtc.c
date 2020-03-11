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

#define RTT_SECOND_MAX  (RTT_MAX_VALUE/RTT_FREQUENCY)

#define _RTT(n) ((n) & RTT_MAX_VALUE)

static uint32_t alarm_time;     /*< The RTC timestamp of the next alarm */
static uint32_t rtc_now;        /*< The last time the RTT overflowed */

static uint32_t _get_offset;    /*< RTT ticks that don't count into the current period */
static uint32_t _set_offset = RTT_MAX_VALUE;    /* RTT ticks that count into the current period */

static rtc_alarm_cb_t alarm_cb; /*< RTC alarm callback */
static void *alarm_cb_arg;      /*< RTC alarm callback argument */

static int _set_alarm(uint32_t alarm, rtc_alarm_cb_t cb, void *arg);

/* convert RTT counter into RTC timestamp */
static inline uint32_t _rtt_now(uint32_t now)
{
    return rtc_now + _RTT(now - _get_offset) / RTT_SECOND;
}

/* we have to wrap the RTT alarm callback because we have to turn off
   the RTT alarm again. */
static void _rtt_alarm(void *arg) {
    DEBUG("Alarm!\n");

    rtc_alarm_cb_t tmp = alarm_cb;
    rtc_clear_alarm();

    if (tmp) {
        tmp(arg);
    }
}

/* The overflow callback increments the RTC counter by however much seconds have passed since
   the last increment. */
static void _rtt_overflow(void *arg) {
    (void) arg;

    /* Increment RTC timestamp by the time passed since the last period.
       Add one second to make up for the second lost by the wrap-around. */
    rtc_now    += (RTT_MAX_VALUE - _get_offset)/RTT_SECOND + 1;
    _set_offset = (RTT_MAX_VALUE - _get_offset)%RTT_SECOND;
    _get_offset = 0;

    if (alarm_cb) {
        DEBUG("[%u] alarm in %u, max: %u\n", rtc_now, alarm_time - rtc_now, RTT_SECOND_MAX);
    }

    /* If the alarm would occur in the current period, set the RTT alarm. */
    if (alarm_cb && (alarm_time - rtc_now <= RTT_SECOND_MAX)) {
        _set_alarm(alarm_time, alarm_cb, alarm_cb_arg);
    }
}

void rtc_init(void)
{
    rtt_set_overflow_cb(_rtt_overflow, NULL);
}

int rtc_set_time(struct tm *time)
{
    rtc_tm_normalize(time);

    uint32_t now = rtt_get_counter();

    rtc_now     = rtc_mktime(time);
    _get_offset = now;
   _set_offset  = RTT_MAX_VALUE - now;

    /* update the RTT alarm if nececcary */
    if (alarm_cb) {
        _set_alarm(alarm_time, alarm_cb, alarm_cb_arg);
    }

    return 0;
}

int rtc_get_time(struct tm *time)
{

    uint32_t now = rtt_get_counter();
    uint32_t tmp = _rtt_now(now);

    rtc_localtime(tmp, time);

    return 0;
}

int rtc_get_alarm(struct tm *time)
{
    rtc_localtime(alarm_time, time);

    return 0;
}

static int _set_alarm(uint32_t alarm, rtc_alarm_cb_t cb, void *arg)
{
    alarm_cb     = cb;
    alarm_cb_arg = arg;

    uint32_t now = rtt_get_counter();

    DEBUG("%s(%u, %u)\n", __func__, alarm, _rtt_now(now));
    DEBUG("now: %u\n", now);
    DEBUG("_get_offset: %u\n", _get_offset);

    if (alarm <= _rtt_now(now)) {
        return -1;
    }

    uint32_t diff_sec = alarm - _rtt_now(now);
    uint32_t secs_left = (RTT_MAX_VALUE - now) / RTT_SECOND;

    DEBUG("alarm: %u\n", alarm);
    DEBUG("diff_sec: %u\n", diff_sec);
    DEBUG("period remain %u s\n", secs_left);

    if (diff_sec <= secs_left) {
        DEBUG("set alarm in %u ticks\n", diff_sec * RTT_SECOND);
        rtt_set_alarm(now + diff_sec * RTT_SECOND, _rtt_alarm, arg);
    } else {
        rtt_clear_alarm();
    }

    return 0;
}

int rtc_set_alarm(struct tm *time, rtc_alarm_cb_t cb, void *arg)
{
    alarm_time = rtc_mktime(time);

    return _set_alarm(alarm_time, cb, arg);
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
