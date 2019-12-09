/*
 * Copyright (C) 2019 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     drivers_periph
 * @{
 *
 * @file
 * @brief       Low-level ADC driver implementation for lpc23xx family
 *
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 *
 * @}
 */

#include "cpu.h"
#include "kernel_defines.h"
#include "periph/adc.h"
#include "periph_conf.h"

int adc_init(adc_t line)
{
    /* check if the line is valid */
    if (line >= ADC_NUMOF) {
        return -1;
    }

    /* enable clock/power for ADC */
    PCONP |= BIT12;

    /* set ADC PCLK to CCLK/8 */
    PCLKSEL0 |= BIT25 + BIT25;

    /* configure AF for ADC pin */
    *(&PINSEL0 + adc_config[line].pinsel) |= adc_config[line].pinsel_msk;

    /* power down ADC */
    AD0CR = 0;

    return 0;
}

int adc_sample(adc_t line, adc_res_t res)
{
    uint32_t val;

    /* check if the line is valid */
    if (line >= ADC_NUMOF) {
        return -1;
    }

    /* enable & configure ADC */
    AD0CR = (1   << adc_config[line].chan)  /* select channel      */
          | (1   << 8)                      /* clocked at 4.5 MHz  */
          | (res << 17)                     /* select resolution   */
          | (1   << 21)                     /* enable ADC          */
          | (1   << 24);                    /* start conversion    */

    do {
        val = *(&AD0DR0 + adc_config[line].chan);
    } while (!(val & BIT31));

    /* power down ADC */
    AD0CR = 0;

    return val & 0xFFFF;
}
