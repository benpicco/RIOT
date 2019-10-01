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
 * @brief       Dummy implementation for AT86RF215 hardware access
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */
#ifndef MOCK_AT86RF215_H
#define MOCK_AT86RF215_H

#include <stdint.h>
#include "at86rf215.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t (*reg_callback_t)(at86rf215_t *dev, uint16_t reg, uint8_t val, void *ctx);

void at86rf215_mock_reg_on_read_cb(uint16_t reg, reg_callback_t cb, void *arg);
void at86rf215_mock_reg_on_write_cb(uint16_t reg, reg_callback_t cb, void *arg);

void at86rf215_mock_init(const at86rf215_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_AT86RF215_H */
