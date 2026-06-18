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

int tps92520_get_temperature(tps92520_t *dev);

int tps92520_get_5V(tps92520_t *dev);

typedef enum {
    TPS92520_CH1VIN      = 0x13,
    TPS92520_CH1VLED     = 0x14,
    TPS92520_CH1VLED_ON  = 0x15,
    TPS92520_CH1VLED_OFF = 0x16,
    TPS92520_CH2VIN      = 0x17,
    TPS92520_CH2VLED     = 0x18,
    TPS92520_CH2VLED_ON  = 0x19,
    TPS92520_CH2VLED_OFF = 0x1a,
} tps92520_chan_t;

int tps92520_get_led_voltage(tps92520_t *dev, tps92520_chan_t chan);

int tps92520_enable(tps92520_t *dev, uint8_t chan);
int tps92520_disable(tps92520_t *dev, uint8_t chan);

int tps92520_set_current(tps92520_t *dev, uint8_t chan, uint16_t val);

void tps92520_set_sleep(tps92520_t *dev, bool sleep);

void tps92520_get_state(tps92520_t *dev, uint8_t state[3]);

#ifdef __cplusplus
}
#endif

/** @} */
