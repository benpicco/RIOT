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

#define RTT_SECOND_MAX  (RTT_MAX_VALUE%RTT_FREQUENCY)

#define _RTT(n) ((n) & RTT_MAX_VALUE)

static uint32_t alarm_time;
static unsigned alarm_overflows;

static uint32_t _get_offset;
static uint32_t _set_offset = RTT_MAX_VALUE;

static uint32_t rtc_now;

static rtc_alarm_cb_t alarm_cb;
static void *alarm_cb_arg;

static int _set_alarm(uint32_t alarm, rtc_alarm_cb_t cb, void *arg);

static void _rtt_alarm(void *arg) {
    if (alarm_cb) {
        alarm_cb(arg);
    }

    rtt_clear_alarm();
}

static void _rtt_overflow(void *arg) {
    (void) arg;

    DEBUG("%s(%u), set: %u, get: %u)\n", __func__, (unsigned)rtc_now, (unsigned)_set_offset, (unsigned)_get_offset);

    DEBUG("adding %u s\n", _RTT(RTT_MAX_VALUE - _get_offset)/RTT_SECOND);

    rtc_now    += _RTT(RTT_MAX_VALUE - _get_offset)/RTT_SECOND + 1;
    _set_offset = _RTT(RTT_MAX_VALUE - _get_offset)%RTT_SECOND;
    _get_offset = 0;

    printf("new _set_offset: %u\n", _set_offset);

    if (alarm_overflows && --alarm_overflows == 0) {
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

    DEBUG("%s(%u, %u)\n", __func__, (unsigned) now, (unsigned)rtc_now);

    rtc_now    += rtc_mktime(time);
    _get_offset = now;
   _set_offset  = RTT_MAX_VALUE - now;

    if (alarm_cb) {
        _set_alarm(alarm_time, alarm_cb, alarm_cb_arg);
    }

    return 0;
}

int rtc_get_time(struct tm *time)
{

    uint32_t now = rtt_get_counter();
    uint32_t tmp = rtc_now + _RTT(now - _get_offset) / RTT_SECOND;

    DEBUG("%s(%u, %u)\n", __func__, (unsigned)now, (unsigned)tmp);

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
    alarm_cb = cb;
    rtt_set_alarm(alarm, _rtt_alarm, arg);

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
    alarm_overflows = 0;
}

void rtc_poweron(void)
{
    rtt_poweron();
}

void rtc_poweroff(void)
{
    rtt_poweroff();
}
