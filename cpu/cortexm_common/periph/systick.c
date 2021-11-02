/*
 * Copyright (C) 2020 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_periph_systick
 * @{
 *
 * @file        systick.c
 * @brief       Low-level timer driver implementation based on SysTick
 *
 *              SysTick is a 24-bit Down-Counting Timer
 *              It runs at the same frequency as the CPU
 *              The count (VAL) register can only be set to 0 on write
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include <assert.h>
#include "board.h"
#include "periph_conf.h"
#include "systick.h"
#include "timex.h"

/* SysTick CTRL configuration */
#define ENABLE_MASK     (SysTick_CTRL_CLKSOURCE_Msk |\
                         SysTick_CTRL_ENABLE_Msk    |\
                         SysTick_CTRL_TICKINT_Msk)

#define TICKS_PER_US    (CLOCK_CORECLOCK / US_PER_SEC)

static bool one_shot;
static countdown_cb_t systick_cb;
static void *systick_cb_arg;

void systick_init(countdown_cb_t cb, void *arg)
{
    systick_cb = cb;
    systick_cb_arg = arg;

    NVIC_SetPriority(SysTick_IRQn, CPU_DEFAULT_IRQ_PRIO);
    NVIC_EnableIRQ(SysTick_IRQn);
}

void systick_set(unsigned int timeout_us, uint8_t flags)
{
    uint32_t alarm = timeout_us * TICKS_PER_US;

    /* stop timer to prevent race condition */
    SysTick->CTRL = 0;

    /* or should we return error instead? */
    if (alarm > SysTick_LOAD_RELOAD_Msk) {
        alarm = SysTick_LOAD_RELOAD_Msk;
    }

    /* set alarm value */
    SysTick->LOAD = alarm;

    one_shot = !(flags & COUNTDOWN_FLAG_PERIODIC);
}

void systick_start(void)
{
    /* force reload */
    SysTick->VAL = 0;

    /* start the timer again */
    SysTick->CTRL = ENABLE_MASK;
}

void systick_cancel(void)
{
    SysTick->CTRL = 0;
}

void isr_systick(void)
{
    /* no further alarms */
    if (one_shot) {
        SysTick->CTRL = 0;
    }

    systick_cb(systick_cb_arg);

    cortexm_isr_end();
}
