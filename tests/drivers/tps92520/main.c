/*
 * SPDX-FileCopyrightText: 2026 ML!PA Consulting GmbH
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief      Test application for the TPS92520
 *
 * @author     Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include <stdio.h>
#include "tps92520.h"
#include "ztimer.h"

#include "macros/units.h"

int main(void)
{
    const tps92520_params_t params = {
        .spi     = SPI_DEV(1),
        .spi_clk = KHZ(250),
        .cs_pin  = GPIO_PIN(PC, 6),
    };

    gpio_init(GPIO_PIN(PB, 0), GPIO_OUT);
    gpio_init(GPIO_PIN(PB, 6), GPIO_OUT);
    gpio_set(GPIO_PIN(PB, 6));
    gpio_set(GPIO_PIN(PB, 0));

    ztimer_sleep(ZTIMER_MSEC, 10);

    tps92520_t dev;
    int res = tps92520_init(&dev, &params);

    int temp = tps92520_get_temperature(&dev);

    printf("%d m°C\n", temp);

    printf("5V: %d mV\n", tps92520_get_5V(&dev));
    printf("Vin[0]: %d mV\n", tps92520_get_led_voltage(&dev, TPS92520_CH1VIN));
    printf("Vin[1]: %d mV\n", tps92520_get_led_voltage(&dev, TPS92520_CH2VIN));

    /* turn off chip */
    gpio_clear(GPIO_PIN(PB, 0));

    return res;
}
