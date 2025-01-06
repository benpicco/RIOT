/*
 * Copyright 2019 Marian Buschsieweke
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief   Test application for the WS281x RGB LED driver
 *
 * @author  Marian Buschsieweke <marian.buschsieweke@ovgu.de>
 *
 * @}
 */

#include <stdio.h>

#include "periph/gpio.h"
#include "mutex.h"
#include "ws281x.h"
#include "ws281x_params.h"
#include "xtimer.h"

#ifndef BTN0_INT_FLANK
#define BTN0_INT_FLANK  GPIO_FALLING
#endif

#ifndef BTN1_INT_FLANK
#define BTN1_INT_FLANK  GPIO_FALLING
#endif

#ifndef BTN2_INT_FLANK
#define BTN2_INT_FLANK  GPIO_FALLING
#endif

static const color_rgb_t rainbow[] = {
    {.r = 0xff, .g = 0xff, .b = 0xff},
    {.r = 0x94, .g = 0x00, .b = 0xd3},
    {.r = 0x4b, .g = 0x00, .b = 0x82},
    {.r = 0x00, .g = 0x00, .b = 0xff},
    {.r = 0x00, .g = 0xff, .b = 0x00},
    {.r = 0xff, .g = 0xff, .b = 0x00},
    {.r = 0xff, .g = 0x7f, .b = 0xd3},
    {.r = 0xff, .g = 0x00, .b = 0x00},
};

#define RAINBOW_LEN     ARRAY_SIZE(rainbow)

static struct {
    mutex_t lock;
    uint8_t idx;
    uint8_t brightness;
} _ctx = {
    .brightness = 127,
};

static inline void _inc(void)
{
    if (_ctx.brightness < 0xff) {
        ++_ctx.brightness;
    }
}

static inline void _dec(void)
{
    if (_ctx.brightness > 0) {
        --_ctx.brightness;
    }
}

static void _unlock(void *ctx)
{
    mutex_unlock(ctx);
}

static void _btn2_cb(void *ctx)
{
    _ctx.idx = (_ctx.idx + 1) % RAINBOW_LEN;
    mutex_unlock(ctx);
}

int main(void)
{
    ws281x_t dev;
    ws281x_init(&dev, &ws281x_params[0]);

    gpio_init_int(BTN0_PIN, BTN0_MODE, BTN0_INT_FLANK, _unlock, &_ctx.lock);
    gpio_init_int(BTN1_PIN, BTN1_MODE, BTN1_INT_FLANK, _unlock, &_ctx.lock);
    gpio_init_int(BTN2_PIN, BTN2_MODE, BTN2_INT_FLANK, _btn2_cb, &_ctx.lock);


    while (1) {
        color_rgb_t color;
        color_rgb_set_brightness(&rainbow[_ctx.idx], &color, _ctx.brightness);

        ws281x_set_all(&dev, color);
        ws281x_write(&dev);

        if (!gpio_read(BTN0_PIN)) {
            _inc();
            goto sleep;
        }
        if (!gpio_read(BTN1_PIN)) {
            _dec();
            goto sleep;
        }

        mutex_lock(&_ctx.lock);
sleep:
        xtimer_msleep(5);
    }

    return 0;
}
