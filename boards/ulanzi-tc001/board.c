/*
 * SPDX-FileCopyrightText: 2020 ML!PA Consulting GmbH
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/**
 * @ingroup     boards_samd10-xmini
 * @{
 *
 * @file
 * @brief       Board specific implementations for the Atmel SAM D10 Xplained
 *              Mini board
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include "board.h"
#include "periph/gpio.h"

void board_init(void)
{
    /* silence buzzer */
    gpio_init(GPIO15, GPIO_OUT);
}
