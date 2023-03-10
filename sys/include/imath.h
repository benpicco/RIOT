/*
 * Copyright (C) 2023 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_imath   imath: Integer Math functions
 * @ingroup     sys
 *
 * @brief       This modules provides some integer-only math functions.
 *              They can be used when no FPU is available or no exact
 *              precision is needed.
 * @{
 */

#ifndef IMATH_H
#define IMATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ISIN_PERIOD      0x7FFF /**< Period of the isin() function */
#define ISIN_MAX         0x1000 /**< Max value of the isin() function */
#define ISIN_MIN        -0x1000 /**< Min value of the isin() function */

/**
 * @brief A sine approximation via a fourth-order cosine approx.
 *        source: https://www.coranac.com/2009/07/sines/
 *
 * @param x     angle (with 2^15 units/circle)
 * @return      sine value (Q12)
 */
int32_t isin(int32_t x);

#ifdef __cplusplus
}
#endif

#endif /* IMATH_H */
/** @} */
