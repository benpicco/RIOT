/*
 * SPDX-FileCopyrightText: 2018 Gunar Schorcht
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#pragma once

/**
 * @ingroup     boards_esp32_wroom-32
 * @brief       Board specific definitions for generic ESP32-WROOM-32 boards
 * @{
 *
 * This configuration can be used for a large set of ESP32 boards that
 * use an ESP32-WROOM-32 module and simply break out all GPIOs to external
 * pads without having any special hardware or interfaces on-board.
 * Examples are Espressif's EPS32-DEVKIT or NodeMCU-ESP32S and a large
 * number of clones.
 *
 * For detailed information about the configuration of ESP32 boards, see
 * section \ref esp32_peripherals "Common Peripherals".
 *
 * @note
 * Most definitions can be overridden by an \ref esp32_application_specific_configurations
 * "application-specific board configuration".
 *
 * @file
 * @author      Gunar Schorcht <gunar@schorcht.net>
 */

#include <stdint.h>

/**
 * @name    Button pin definitions
 * @{
 */

#define BTN0_PIN        GPIO26
#define BTN0_MODE       GPIO_IN_PU
#define BTN0_INT_FLANK  GPIO_FALLING

#define BTN1_PIN        GPIO27
#define BTN1_MODE       GPIO_IN_PU
#define BTN1_INT_FLANK  GPIO_FALLING

#define BTN2_PIN        GPIO14
#define BTN2_MODE       GPIO_IN_PU
#define BTN2_INT_FLANK  GPIO_FALLING

/** @} */


#define WS281X_PARAM_PIN    GPIO32
#define WS281X_PARAM_NUMOF  (32*8)

#define SHT3X_PARAM_I2C_ADDR SHT3X_I2C_ADDR_1

/**
 * @name    LED (on-board) configuration
 *
 * Generic ESP32 boards usually do not have on-board LEDs.
 * @{
 */
/** @} */

/* include common board definitions as last step */
#include "board_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
} /* end extern "C" */
#endif

/** @} */
