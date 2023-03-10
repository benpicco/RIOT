/*
 * Copyright (C) 2023 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup sys_imath
 * @{
 * @file
 * @brief   integer math functions
 * @}
 */

#include "imath.h"

/**
 * @brief A sine approximation via a fourth-order cosine approx.
 *        source: https://www.coranac.com/2009/07/sines/
 *
 * @param x     angle (with 2^15 units/circle)
 * @return      sine value (Q12)
 */
int32_t isin(int32_t x)
{
    int32_t c, y;
    static const int32_t qN = 13,
                         qA = 12,
                          B = 19900,
                          C = 3516;

    c = x << (30 - qN);         /* Semi-circle info into carry. */
    x -= 1 << qN;               /* sine -> cosine calc          */

    x = x << (31 - qN);         /* Mask with PI                 */
    x = x >> (31 - qN);         /* Note: SIGNED shift! (to qN)  */
    x = x * x >> (2 * qN - 14); /* x=x^2 To Q14                 */

    y = B - (x * C >> 14);      /* B - x^2*C                    */
    y = (1 << qA)               /* A - x^2*(B-x^2*C)            */
      - (x * y >> 16);

    return c >= 0 ? y : -y;
}
