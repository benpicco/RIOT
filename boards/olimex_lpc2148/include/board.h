/*
 * Copyright (C) 2019 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_mcb2388
 * @{
 *
 * @file
 * @brief       Basic definitions for the MCB2388 board
 *
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 */

#ifndef BOARD_H
#define BOARD_H

#include "lpc23xx.h"
#include "mtd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    LED pin definitions and handlers
 * @{
 */
#define LED0_PIN            GPIO_PIN(0, 10)
#define LED1_PIN            GPIO_PIN(0, 11)

#define LED0_MASK           (BIT10)
#define LED1_MASK           (BIT11)

#define LED0_OFF            (FIO0SET  = LED0_MASK)
#define LED0_ON             (FIO0CLR  = LED0_MASK)
#define LED0_TOGGLE         (FIO0PIN ^= LED0_MASK)

#define LED1_OFF            (FIO0SET  = LED1_MASK)
#define LED1_ON             (FIO0CLR  = LED1_MASK)
#define LED1_TOGGLE         (FIO0PIN ^= LED1_MASK)
/** @} */

/**
 * @name    INT0 (Button) pin definitions
 * @{
 */
#define BTN0_PIN            GPIO_PIN(2, 10)
#define BTN0_MODE           GPIO_IN
#define BTN0_INT_FLANK      GPIO_FALLING
/** @} */

/**
 * @name MTD configuration
 * @{
 */
#ifdef MODULE_MTD_MCI
extern mtd_dev_t *mtd0;
#define MTD_0 mtd0
#endif
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
/** @} */
