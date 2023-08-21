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

#include "random.h"
#include "ws281x.h"
#include "ws281x_params.h"
#include "ztimer.h"

#define INTERVAL_MS 20

typedef enum {
    STATE_FLASH_RANDOM,
    STATE_COLOR_LOOP,
    STATES_NUMOF,
} app_state_t;

static color_rgb_t leds[WS281X_PARAM_NUMOF];

static void _rand_color(color_rgb_t *led)
{
    uint32_t rand = random_uint32();
    led->r = rand;
    led->g = rand >> 8;
    led->b = rand >> 16;
}

static void _rand_color_idx(color_rgb_t *led)
{
    static const color_rgb_t colors[] = {
        { .r = 0xff, .g = 0x00, .b = 0x00 },
        { .r = 0x00, .g = 0xff, .b = 0x00 },
        { .r = 0x00, .g = 0x00, .b = 0xff },
        { .r = 0xff, .g = 0xff, .b = 0x00 },
        { .r = 0x00, .g = 0xff, .b = 0xff },
        { .r = 0xff, .g = 0x00, .b = 0xff },
    };

    unsigned idx = random_uint32() % ARRAY_SIZE(colors);
    *led = colors[idx];
}

static void _flash_random(void)
{
    if ((random_uint32() & 0xF) == 0xF) {
        unsigned i = random_uint32() % ARRAY_SIZE(leds);
        _rand_color(&leds[i]);
    }
}

/* Tracers effect  */

static unsigned random_range(unsigned lower, unsigned upper)
{
    unsigned range = upper - lower + 1;
    return lower + rand() % range;
}

typedef struct {
    color_rgb_t color;
    uint16_t ttl;
    int16_t speed_100;
    int16_t next_led;
    uint8_t idx;
} tracer_t;

static void _tracers(void)
{
    static tracer_t tracers[3];

    if ((random_uint32() & 0xFF) == 0) {
        for (unsigned i = 0; i < ARRAY_SIZE(tracers); ++i) {
            if (tracers[i].ttl == 0) {
                tracers[i].idx = 0;
                tracers[i].ttl = random_uint32() >> 22;
                tracers[i].speed_100 = random_range(500, 800);

                if (random_uint32() & 1) {
                    tracers[i].speed_100 *= -1;
                }

//                if (random_uint32() & 1) {
                    _rand_color_idx(&tracers[i].color);
//                } else {
//                    _rand_color(&tracers[i].color);
//                }

                printf("new tracer at %u, speed = %d\n", tracers[i].idx, tracers[i].speed_100);
                break;
            }
        }
    }

    for (unsigned i = 0; i < ARRAY_SIZE(tracers); ++i) {
        if (tracers[i].ttl == 0) {
            continue;
        }

        --tracers[i].ttl;
        tracers[i].next_led += tracers[i].speed_100;

        if (tracers[i].next_led > 1000) {
            tracers[i].idx = (tracers[i].idx + 1) % WS281X_PARAM_NUMOF;
            tracers[i].next_led = 0;
        }

        if (tracers[i].next_led < -1000) {
            tracers[i].idx = (tracers[i].idx - 1) % WS281X_PARAM_NUMOF;
            tracers[i].next_led = 0;
        }

        if (tracers[i].next_led == 0) {
            color_rgb_t *dst = &leds[tracers[i].idx];
            color_rgb_add(dst, dst, &tracers[i].color);
        }
    }
}

#ifdef BTN0_PIN
static void _gpio_cb(void *ctx)
{
    app_state_t *state = ctx;
    static unsigned last_press;
    unsigned now = ztimer_now(ZTIMER_MSEC);
    if (now - last_press < 10) {
        return;
    }
    last_press = now;
    *state = (*state + 1) % STATES_NUMOF;

    printf("new state: %u\n", *state);
}
#endif

int main(void)
{
    ws281x_t dev;
    int retval;
//    app_state_t state = STATE_FLASH_RANDOM;
    app_state_t state = STATE_COLOR_LOOP;

    printf("Have %u LEDs\n", WS281X_PARAM_NUMOF);

    if (0 != (retval = ws281x_init(&dev, &ws281x_params[0]))) {
        printf("Initialization failed with error code %d\n", retval);
        return retval;
    }

#ifdef BTN0_PIN
    if (IS_USED(MODULE_PERIPH_GPIO_IRQ)) {
        gpio_init_int(BTN0_PIN, BTN0_MODE, GPIO_RISING, _gpio_cb, &state);
    }
#endif

    uint32_t last_wakeup = ztimer_now(ZTIMER_MSEC);
    int8_t shift = 0;
    while (1) {

        switch (state) {
        case STATE_FLASH_RANDOM:
            _flash_random();
            shift = -1;
            break;
        case STATE_COLOR_LOOP:
            _tracers();
            shift = -3;
            break;
        case STATES_NUMOF:
            assume(0);
            break;
        }

        for (unsigned i = 0; i < ARRAY_SIZE(leds); ++i) {
            ws281x_set(&dev, i, leds[i]);
            color_rgb_shift(&leds[i], &leds[i], shift);
        }

        ws281x_write(&dev);

        /* wait some time to allow testers to verify the result */
        ztimer_periodic_wakeup(ZTIMER_MSEC, &last_wakeup, INTERVAL_MS);
    }

    return 0;
}
