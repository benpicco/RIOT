/*
 * Copyright (C) 2018 Gunar Schorcht
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_itg320x
 * @brief       Default configuration for InvenSense ITG320X 3-axis gyroscope
 * @author      Gunar Schorcht <gunar@schorcht.net>
 * @file
 * @{
 */

#ifndef ITG320X_PARAMS_H
#define ITG320X_PARAMS_H

#include "board.h"
#include "itg320x.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    Set default configuration parameters
 * @{
 */
#ifndef ITG320X_PARAM_DEV
#define ITG320X_PARAM_DEV          I2C_DEV(0)
#endif
#ifndef ITG320X_PARAM_ADDR
#define ITG320X_PARAM_ADDR         (ITG320X_I2C_ADDRESS_1)
#endif
#ifndef ITG320X_PARAM_LPF_BW
#define ITG320X_PARAM_LPF_BW       (ITG320X_LPF_BW_5)
#endif
#ifndef ITG320X_PARAM_ISR_DIV
#define ITG320X_PARAM_ISR_DIV      (9)
#endif
#ifndef ITG320X_PARAM_INT_PIN
#define ITG320X_PARAM_INT_PIN      (GPIO_PIN(0, 0))
#endif

#ifndef ITG320X_PARAMS
#define ITG320X_PARAMS             { \
                                     .dev  = ITG320X_PARAM_DEV,  \
                                     .addr = ITG320X_PARAM_ADDR, \
                                     .int_pin = ITG320X_PARAM_INT_PIN, \
                                     .lpf_bw  = ITG320X_PARAM_LPF_BW,  \
                                     .isr_div = ITG320X_PARAM_ISR_DIV, \
                                   }
#endif

#ifndef ITG320X_SAUL_INFO
#define ITG320X_SAUL_INFO          { .name = "itg320x" }
#endif
/**@}*/

/**
 * @brief   Allocate some memory to store the actual configuration
 */
static const itg320x_params_t itg320x_params[] =
{
    ITG320X_PARAMS
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t itg320x_saul_info[] =
{
    ITG320X_SAUL_INFO
};

#ifdef __cplusplus
}
#endif

#endif /* ITG320X_PARAMS_H */
/** @} */
