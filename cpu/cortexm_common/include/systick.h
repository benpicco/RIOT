/*
 * Copyright (C) 2020 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_periph_systick SysTick Timer
 * @ingroup     cpu_cortexm_common
 * @ingroup     drivers_periph
 * @brief       Low-level timer based on SysTick
 *
 * SysTick is a 24 bit down-counting timer that is implemented on every Cortex-M
 * processor. It runs at the same frequency as the CPU and will generate an
 * interrupt when it reaches zero.
 *
 * To comply with the RIOT timer interface that expects an up-counting, monotonic
 * timer, SysTick is augmented by software to fulfill these requirements.
 * This also adds a software prescaler to simulate arbitrary frequencies and introduces
 * a virtual, 32 bit counter.
 * That allows for longer intervals than would be possible with the SysTick hardware
 * peripheral alone.
 *
 * @{
 *
 * @file
 * @brief       Low-level SysTick timer peripheral driver interface definitions
 *              The API makes no guarantees about thread-safety.
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COUNTDOWN_FLAG_PERIODIC (1 << 0)

typedef void (*countdown_cb_t)(void *arg);

void systick_init(countdown_cb_t cb, void *arg);

void systick_set(unsigned int timeout_us, uint8_t flags);
void systick_start(void);
void systick_cancel(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTICK_H */
/** @} */
