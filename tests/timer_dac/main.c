/*
 * Copyright (C) 2015 Freie Universität Berlin
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
 * @brief       Peripheral timer test application
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
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

#if 0
int32_t isin(int32_t x)
{
    int c, y;
    static const int qN= 13, qA= 12, B=19900, C=3516;

    c= x<<(30-qN);              // Semi-circle info into carry.
    x -= 1<<qN;                 // sine -> cosine calc

    x= x<<(31-qN);              // Mask with PI
    x= x>>(31-qN);              // Note: SIGNED shift! (to qN)
    x= x*x>>(2*qN-14);          // x=x^2 To Q14

    y= B - (x*C>>14);           // B - x^2*C
    y= (1<<qA)-(x*y>>16);       // A - x^2*(B-x^2*C)

    return c>=0 ? y : -y;
}
#endif

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

static void _fill_buf(uint8_t b, unsigned iteration)
{
    uint8_t shift = (iteration & 0xF);

    for (uint16_t i = 0; i < buf_size; ++i) {
        buf[b][i] = (isin(i << shift) + 4096) >> 5;
    }
}

#if 0
static void _fill_buf(uint8_t b, unsigned iteration)
{
    uint32_t t = iteration * buf_size;
    for (uint16_t i = 0; i < buf_size; ++i, ++t) {
        buf[b][i] = t * (( (t>>9) | (t>>13)) & 25 & (t>>6));
    }

}
#endif

static void play_blip(void)
{
    mutex_t lock = MUTEX_INIT_LOCKED;
    uint8_t cur_buf = 0;

    for (unsigned iteration = 0; iteration < 0x10; ++iteration) {
        _fill_buf(cur_buf, iteration);
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

    puts("button pressed");

    msg_send_int(&m, pid);
}

int main(void)
{
    adc_init(0);
    dac_init(0);

    kernel_pid_t main_pid = thread_getpid();
    gpio_init_int(BTN0_PIN, BTN0_MODE, BTN0_INT_FLANK, btn_cb, &main_pid);

    msg_t m;
    while (msg_receive(&m)) {
        puts("message received");
        play_blip();
    }

    return 0;
}
