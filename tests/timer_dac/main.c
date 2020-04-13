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
#include "periph/adc.h"
#include "periph/dac.h"
#include "periph/gpio.h"
#include "periph/timer.h"
#include "xtimer.h"

#include "blob/hello.raw.h"

static const uint16_t buf_size = 2048;
static uint8_t buf[2][2048];

static int32_t isin(int32_t x)
{
    // S(x) = x * ( (3<<p) - (x*x>>r) ) >> s
    // n : Q-pos for quarter circle             13
    // A : Q-pos for output                     12
    // p : Q-pos for parentheses intermediate   15
    // r = 2n-p                                 11
    // s = A-1-p-n                              17

    static const int qN = 13, qA= 12, qP= 15, qR= 2*qN-qP, qS= qN+qP+1-qA;

    x = x<<(30-qN);          // shift to full s32 range (Q13->Q30)

    if ((x^(x<<1)) < 0) {   // test for quadrant 1 or 2
        x = (1<<31) - x;
    }

    x = x >> (30-qN);

    return x * ((3<<qP) - (x*x>>qR)) >> qS;
}

static void blinky(void)
{
    static uint8_t cur;
    const uint8_t pattern[] = {
        0x81, 0x42, 0x24, 0x18, 0x0
    };

    FIO_PORTS[2].CLR = 0xFF;
    FIO_PORTS[2].SET = pattern[cur];

    if (++cur >= sizeof(pattern)) {
        cur = 0;
    }
}

static void _fill_buf(uint8_t b, unsigned pitch)
{
    for (uint16_t i = 0; i < buf_size; ++i) {
        buf[b][i] = (isin(i << pitch) + 4096) >> 5;
    }
}

static void play_blip(void)
{
    mutex_t lock = MUTEX_INIT_LOCKED;
    uint8_t cur_buf = 0;

    for (unsigned i = 0; i <= 0x10; ++i) {
        blinky();
        _fill_buf(cur_buf, i);
        dac_play(buf[cur_buf], buf_size, (dac_cb_t) mutex_unlock, &lock);
        mutex_lock(&lock);
        cur_buf = !cur_buf;
    }

    dac_stop();
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
    adc_init(0);
    dac_init(0);

    kernel_pid_t main_pid = thread_getpid();
    gpio_init_int(BTN0_PIN, BTN0_MODE, BTN0_INT_FLANK, btn_cb, &main_pid);

#if ENABLE_GREETING
    dac_play(hello_raw, hello_raw_len, NULL, NULL);
    dac_stop();
#endif

    msg_t m;
    while (msg_receive(&m)) {
        play_blip();
    }

    return 0;
}
