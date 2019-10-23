/*
 * Copyright (C) 2019 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     boards_mcb2388
 * @{
 *
 * @file
 * @brief       MCB2388 peripheral configuration
 *
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 */

#ifndef PERIPH_CONF_H
#define PERIPH_CONF_H

#include "periph_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    Clock configuration
 * @{
 */
#define XTAL_HZ             (12000000U)         /* the board provides a 12 MHz XTAL */

#define CLOCK_CORECLOCK     (72000000U)         /* the lpc2388 runs with 72MHz */

#define CLOCK_PCLK          (CLOCK_CORECLOCK)
/** @} */

/**
 * @name    Configuration for the 16x2 character LCD display
 * @{
 */
#define HD44780_PARAMS {            \
    .cols   = 16,                   \
    .rows   = 2,                    \
    .rs     = GPIO_PIN(1,28),       \
    .rw     = GPIO_PIN(1,29),       \
    .enable = GPIO_PIN(1,31),       \
    .data   = {                     \
                GPIO_PIN(1,24), GPIO_PIN(1,25), GPIO_PIN(1,26), GPIO_PIN(1,27), \
                GPIO_UNDEF, GPIO_UNDEF, GPIO_UNDEF, GPIO_UNDEF, \
              }                     \
    }
/** @} */

/**
 * @name    Timer configuration, select a number from 1 to 4
 * @{
 */
#define TIMER_NUMOF         (1U)
/** @} */

/**
 * @name    PWM device and pinout configuration
 *
 * Currently, we only support a single device and 3 channels, the implementation
 * is fixed on PWM1.
 * @{
 */
#define PWM_NUMOF           (1U)

/* PWM_0 device configuration */
#define PWM_CHANNELS      (3)
#define PWM_CH0           (3)
#define PWM_CH0_MR        PWM1MR3
#define PWM_CH1           (4)
#define PWM_CH1_MR        PWM1MR4
#define PWM_CH2           (5)
#define PWM_CH2_MR        PWM1MR5
/* PWM_0 pin configuration */
#define PWM_PORT          PINSEL4
#define PWM_CH0_PIN       (2)
#define PWM_CH1_PIN       (3)
#define PWM_CH2_PIN       (4)
#define PWM_FUNC          (1)
/** @} */

/**
 * @name    Real Time Clock configuration
 * @{
 */
#define RTC_NUMOF           (1)
/** @} */

/**
 * @name    UART configuration
 * @{
 */
static const uart_conf_t uart_config[] = {
    {
        .dev = UART0,
        .irq_prio_rx = 6,
        .pinsel_rx   = 0,
        .pinsel_tx   = 0,
        .pinsel_msk_rx = BIT4,
        .pinsel_msk_tx = BIT6,
    },
    {
        .dev = UART1,
        .irq_prio_rx = 6,
        .pinsel_rx   = 1,
        .pinsel_tx   = 0,
        .pinsel_msk_rx = BIT0,
        .pinsel_msk_tx = BIT30,
    }
};

#define UART_NUMOF          (2)
/** @} */

/**
 * @name    SPI configuration
 *
 * The SPI implementation is very much fixed, so we don't need to configure
 * anything besides the mandatory SPI_NUMOF.
 * @{
 */
#define SPI_NUMOF           (1)
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* PERIPH_CONF_H */
/** @} */
