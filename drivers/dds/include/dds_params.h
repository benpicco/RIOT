/*
 * Copyright (C) 2018 Otto-von-Guericke-Universität Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_dds
 * @{
 *
 * @file
 * @brief       dds board specific configuration
 *
 * @author      Marian Buschsieweke <marian.buschsieweke@ovgu.de>
 */

#ifndef DDS_PARAMS_H
#define DDS_PARAMS_H

#include "dds.h"
#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    Default parameters for the dds driver
 * @{
 */
#ifndef DDS_PARAM_PWM_FREQ
#define DDS_PARAM_PWM_FREQ      (UINT32_MAX)    /**< PWM frequency to use */
#endif

#ifndef DDS_PARAM_TIMER_FREQ
#define DDS_PARAM_TIMER_FREQ    (CLOCK_CORECLOCK / 8) /**< Timer frequency to use */
#endif

#ifndef DDS_PARAM_PWM
#define DDS_PARAM_PWM           PWM_DEV(0)      /**< PWM device to use */
#endif

#ifndef DDS_PARAM_TIMER
#define DDS_PARAM_TIMER         TIMER_DEV(0)    /**< Timer device to use */
#endif

#ifndef DDS_PARAM_MODE
#define DDS_PARAM_MODE          (PWM_LEFT)      /**< PWM device to use */
#endif

#ifndef DDS_PARAM_CHANNEL
#define DDS_PARAM_CHANNEL       (0U)            /**< PWM channel to use */
#endif

#ifndef DDS_PARAM_OVERHEAD
#define DDS_PARAM_OVERHEAD      (25U)            /**< Overhead in timer ticks*/
#endif

#ifndef DDS_PARAM_POWER_SAFE
#define DDS_PARAM_POWER_SAFE    (true)          /**< Power off PWM device when unused */
#endif

#ifndef DDS_PARAMS
/**
 * @brief   Default initialization parameters of the CC110x driver
 */
#define DDS_PARAMS {                                                           \
        .pwm_freq       = DDS_PARAM_PWM_FREQ,                                  \
        .timer_freq     = DDS_PARAM_TIMER_FREQ,                                \
        .pwm            = DDS_PARAM_PWM,                                       \
        .mode           = DDS_PARAM_MODE,                                      \
        .timer          = DDS_PARAM_TIMER,                                     \
        .channel        = DDS_PARAM_CHANNEL,                                   \
        .ticks_overhead = DDS_PARAM_OVERHEAD,                                  \
        .power_safe     = DDS_PARAM_POWER_SAFE,                                \
}

#endif
/** @} */

/**
 * @brief   DDS initialization parameters
 */
static const dds_params_t dds_params[] = {
    DDS_PARAMS
};

#ifdef __cplusplus
}
#endif
#endif /* DDS_PARAMS_H */
/** @} */
