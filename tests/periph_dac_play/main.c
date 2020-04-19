/*
 * Copyright (C) 2020 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       DAC (audio) test application
 *
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "board.h"
#include "thread.h"

#include "msg.h"
#include "mutex.h"
#include "periph/dac.h"
#include "periph/gpio.h"
#include "periph/timer.h"
#include "xtimer.h"

#include "blob/hello.raw.h"

#ifndef DAC_CHAN
#define DAC_CHAN DAC_LINE(0)
#endif

#ifndef ENABLE_GREETING
#define ENABLE_GREETING (1)
#endif

#define DAC_BUF_SIZE (2048)

static uint8_t buf[2][DAC_BUF_SIZE];

static int32_t isin(int32_t x)
{
    // S(x) = x * ( (3<<p) - (x*x>>r) ) >> s
    // n : Q-pos for quarter circle             13
    // A : Q-pos for output                     12
    // p : Q-pos for parentheses intermediate   15
    // r = 2n-p                                 11
    // s = A-1-p-n                              17

    static const int qN = 13,
                     qA = 12,
                     qP = 15,
                     qR = 2*qN-qP,
                     qS = qN+qP+1-qA;

    x <<= (30-qN);          // shift to full s32 range (Q13->Q30)

    if ((x^(x<<1)) < 0) {   // test for quadrant 1 or 2
        x = (1<<31) - x;
    }

    x >>= (30-qN);

    return x * ((3<<qP) - (x*x>>qR)) >> qS;
}

static void _unlock(void *arg)
{
    mutex_unlock(arg);
}

static void _play_sin_sample(const dac_cfg_t *cfg, uint8_t b, uint8_t pitch)
{
    for (uint16_t i = 0; i < DAC_BUF_SIZE; ++i) {
        buf[b][i] = (isin(i << pitch) + 4096) >> 5;
    }

    dac_play(buf[b], DAC_BUF_SIZE, cfg);
    mutex_lock(cfg->cb_arg);
}

static void play_blip(uint8_t start, uint8_t end)
{
    mutex_t lock = MUTEX_INIT_LOCKED;
    uint8_t cur_buf = 0;

    const dac_cfg_t cfg = {
        .line        = DAC_CHAN,
        .flags       = DAC_FLAG_8BIT,
        .sample_rate = 8000,
        .cb          = _unlock,
        .cb_arg      = &lock,
    };

    for (unsigned i = start; i <= end; ++i) {
        _play_sin_sample(&cfg, cur_buf, i);
        cur_buf = !cur_buf;
    }
}

enum {
    MSG_BTN0
};

static void btn_cb(void *ctx)
{
    kernel_pid_t pid = *(kernel_pid_t*) ctx;
    msg_t m = {
        .type = MSG_BTN0
    };

    msg_send_int(&m, pid);
}

int main(void)
{
    dac_init(DAC_CHAN);

    kernel_pid_t main_pid = thread_getpid();
    gpio_init_int(BTN0_PIN, BTN0_MODE, BTN0_INT_FLANK, btn_cb, &main_pid);

#if ENABLE_GREETING
    const dac_cfg_t cfg = {
        .line        = DAC_CHAN,
        .flags       = DAC_FLAG_8BIT,
        .sample_rate = 8000,
    };

    puts("Play Greeting…");
    dac_play(hello_raw, hello_raw_len, &cfg);
#else
    puts("Greeting disabled.");
#endif

    msg_t m;
    while (msg_receive(&m)) {
        play_blip(8, 16);
    }

    return 0;
}
