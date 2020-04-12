/*
 * Copyright (C) 2019 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_lpc2387
 * @{
 *
 * @file
 * @brief       Low-level DAC driver implementation
 *
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 *
 * @}
 */

#include "cpu.h"
#include "periph/dac.h"

#include "periph/timer.h"
extern int timer_set_periodic(tim_t tim, int channel, unsigned int value);

int8_t dac_init(dac_t line)
{
    (void) line;

    /* P0.26 is the only valid DAC pin */
    PINSEL1 |=  BIT21;
    PINSEL1 &= ~BIT20;

    return 0;
}

void dac_set(dac_t line, uint16_t value)
{
    (void) line;

    /* Bits 5:0 are reserved for future, higher-resolution D/A converters. */
    DACR = value & 0xFFE0;
}

void dac_poweron(dac_t line)
{
    /* The DAC is always on. */
    (void) line;
}

void dac_poweroff(dac_t line)
{
    /* The DAC is always on. */
    (void) line;
}

static bool dac_playing;
static uint8_t cur;
static uint8_t *buffers[2];
static size_t buffer_len[2];

static dac_cb_t dac_cb;
static void *dac_cb_arg;

static void _timer_cb(void *arg, int chan)
{
    (void) arg;
    (void) chan;

    static unsigned idx;

    uint8_t *buf = buffers[cur];
    size_t len   = buffer_len[cur];

    DACR = buf[idx] << 6;

    if (++idx >= len) {
        idx = 0;
        cur = !cur;

        if (dac_cb) {
            dac_cb(dac_cb_arg);
        }
    }
}

void dac_play(void *buf, size_t len, dac_cb_t cb, void *cb_arg)
{
    uint8_t idx = !cur;

    buffers[idx]    = buf;
    buffer_len[idx] = len;

    if (dac_playing) {
        return;
    }

    dac_cb = cb;
    dac_cb_arg = cb_arg;

    buffers[cur]    = buf;
    buffer_len[cur] = len;

    dac_playing = true;
    timer_init(3, 2000000, _timer_cb, NULL);
    timer_set_periodic(3, 0, 25); /* 8 kHz */
}
