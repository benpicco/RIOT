/*
 * Copyright (C) 2020 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_mcb2388
 * @{
 *
 * @file
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
#include "periph/adc.h"
#include "periph/dac.h"
#include "periph/gpio.h"
#include "periph/timer.h"
#include "xtimer.h"

#include "blob/hello.raw.h"

#define ENABLE_GREETING (0)
#define DAC_BUF_SIZE (2048)

static uint8_t buf[2][DAC_BUF_SIZE];
static kernel_pid_t audio_pid;

enum {
    PLAY_BLIP,
    PLAY_SHORT_BLIP,
    PLAY_GREETING,
};

static int32_t isin(int32_t x)
{
    // S(x) = x * ( (3<<p) - (x*x>>r) ) >> s
    // n : Q-pos for quarter circle             13
    // A : Q-pos for output                     12
    // p : Q-pos for parentheses intermediate   15
    // r = 2n-p                                 11
    // s = A-1-p-n                              17

    static const int qN = 13, qA= 12, qP= 15, qR= 2*qN-qP, qS= qN+qP+1-qA;

    x <<= 30 - qN;          // shift to full s32 range (Q13->Q30)

    if ((x^(x<<1)) < 0) {   // test for quadrant 1 or 2
        x = (1<<31) - x;
    }

    x >>= 30 - qN;

    return x * ((3<<qP) - (x*x>>qR)) >> qS;
}

static void _fill_buf(uint8_t b, unsigned pitch)
{
    for (uint16_t i = 0; i < DAC_BUF_SIZE; ++i) {
        buf[b][i] = (isin(i << pitch) + 4096) >> 5;
    }
}

static void do_play_blip(uint8_t start, uint8_t end)
{
    mutex_t lock = MUTEX_INIT_LOCKED;
    uint8_t cur_buf = 0;

    for (unsigned i = start; i <= end; ++i) {
        _fill_buf(cur_buf, i);
        dac_play(buf[cur_buf], DAC_BUF_SIZE, (dac_cb_t) mutex_unlock, &lock);
        mutex_lock(&lock);
        cur_buf = !cur_buf;
    }

    dac_stop();
}

static char audio_stack[THREAD_STACKSIZE_DEFAULT];

static void* audio_thread(void *ctx)
{
    msg_t m;
    while (msg_receive(&m)) {
        switch (m.type) {
        case PLAY_BLIP:
            do_play_blip(0, 16);
            break;
        case PLAY_SHORT_BLIP:
            do_play_blip(8, 16);
            break;
        case PLAY_GREETING:
#if ENABLE_GREETING
            dac_play(hello_raw, hello_raw_len, NULL, NULL);
            dac_stop();
#endif
            break;
        }
    }

    return ctx;
}

void sound_play_greeting(void)
{
    msg_t m = {
        .type = PLAY_GREETING
    };

    msg_send(&m, audio_pid);
}

void sound_play_blip(void)
{
    msg_t m = {
        .type = PLAY_BLIP
    };

    msg_send(&m, audio_pid);
}

void sound_play_short_blip(void)
{
    msg_t m = {
        .type = PLAY_SHORT_BLIP
    };

    msg_send(&m, audio_pid);
}

void sound_init(void) {
    dac_init(0);

    audio_pid = thread_create(audio_stack, sizeof(audio_stack),
                              THREAD_PRIORITY_MAIN - 1, 0,
                               audio_thread, NULL, "audio");
}
