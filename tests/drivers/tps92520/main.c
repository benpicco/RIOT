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

#include "macros/units.h"

int main(void)
{
    const tps92520_params_t params = {
        .spi     = SPI_DEV(0),
        .spi_clk = MHZ(1),
        .cs_pin  = GPIO_PIN(PA, 13),
    };

    tps92520_t dev;
    int res = tps92520_init(&dev, &params);

    return res;
}
