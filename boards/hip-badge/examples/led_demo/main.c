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

static color_rgb_t leds[WS281X_PARAM_NUMOF];

int main(void)
{
    ws281x_t dev;
    int retval;

    printf("Have %u LEDs\n", WS281X_PARAM_NUMOF);

    if (0 != (retval = ws281x_init(&dev, &ws281x_params[0]))) {
        printf("Initialization failed with error code %d\n", retval);
        return retval;
    }

    uint32_t last_wakeup = ztimer_now(ZTIMER_MSEC);
    while (1) {

        if ((random_uint32() & 0xF) == 0xF) {
            unsigned i = random_uint32() % ARRAY_SIZE(leds);
            uint32_t rand = random_uint32();
            leds[i].r = rand;
            leds[i].g = rand >> 8;
            leds[i].b = rand >> 16;
        }

        for (unsigned i = 0; i < ARRAY_SIZE(leds); ++i) {
            ws281x_set(&dev, i, leds[i]);
            color_rgb_shift(&leds[i], &leds[i], -1);
        }

        ws281x_write(&dev);

        /* wait some time to allow testers to verify the result */
        ztimer_periodic_wakeup(ZTIMER_MSEC, &last_wakeup, INTERVAL_MS);
    }

    return 0;
}
