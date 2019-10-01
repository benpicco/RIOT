/*
 * Copyright (C) 2019 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @addtogroup  unittests
 * @{
 *
 * @file
 * @brief       Dummy implementation for GPIO functions
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */


#include "periph/gpio.h"

int gpio_init_int(gpio_t pin, gpio_mode_t mode, gpio_flank_t flank,
                  gpio_cb_t cb, void *arg)
{
    (void) pin;
    (void) mode;
    (void) flank;
    (void) cb;
    (void) arg;

    return 0;
}

void gpio_irq_disable(gpio_t pin)
{
    (void) pin;
}
