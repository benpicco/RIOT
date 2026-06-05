/*
 * SPDX-FileCopyrightText: 2026 ML!PA Consulting GmbH
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#pragma once

/**
 * @defgroup   drivers_tps92520 TPS92520 ADC device driver
 * @ingroup    drivers_sensors
 * @brief      SPI Analog-to-Digital Converter device driver
 *
 * @{
 *
 * @file
 * @brief      tps92520 ADC device driver
 *
 * @author     Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "mutex.h"
#include "periph/gpio.h"
#include "periph/spi.h"

/**
 * @brief   TPS92520 params
 */
typedef struct {
    spi_t spi;              /**< SPI bus the device is connected to */
    spi_clk_t spi_clk;      /**< SPI clock speed to use */
    spi_cs_t cs_pin;        /**< GPIO pin connected to chip select */
    gpio_t ready_pin;       /**< GPIO pin conected to DRDY */
    gpio_t sync_pin;        /**< GPIO pin conected to SYNC / RESET */
} tps92520_params_t;

typedef struct {
    const tps92520_params_t *params;   /**< device driver configuration */
} tps92520_t;

/**
 * @brief   Initialize an TPS92520 ADC device
 *
 * @param[in,out] dev  device descriptor
 * @param[in] params   device configuration
 *
 * @return zero on successful initialization, non zero on error
 */
int tps92520_init(tps92520_t *dev, const tps92520_params_t *params);

#ifdef __cplusplus
}
#endif

/** @} */
