/*
 * Copyright (C) 2023 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_math_utils	Math helper functions
 * @ingroup     sys
 * @brief       A collection of common mathematical functions
 *
 * @{
 *
 * @file
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * @brief   This function returns the value of @p base raised to the power of @p exp.
 *
 * @param[in] base  Number to multiply
 * @param[in] exp   How many times @p base should be multiplied with itself
 *
 * @return    base ^ exp
 */
int math_pow(int base, unsigned exp);

/**
 * @brief   Calculate the greatest common divisor of @p a and @p b
 *
 * @param[in] a first integer
 * @param[in] b second integer
 *
 * @return     highest common factor of @p a and @p b
 */
unsigned math_gcd(unsigned a, unsigned b);

#ifdef __cplusplus
}
#endif

#endif /* MATH_UTILS_H */
/** @} */
