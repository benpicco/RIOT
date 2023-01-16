/*
 * Copyright (C) 2023 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_math
 * @{
 *
 * @file
 * @brief       Math helper module implementation
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include "math.h"

int math_pow(int base, unsigned exp)
{
    /* A simple implementation of Algorithm A from section 4.6.3 of TAOCP */
    int tmp, res = 1;

    while (1) {
        tmp = exp % 2;
        exp = exp / 2;

        if (tmp == 1) {
            res *= base;
        }
        if (exp == 0) {
            break;
        }

        base *= base;
    }

    return res;
}

unsigned math_gcd(unsigned a, unsigned b)
{
    if (a == 0 || b == 0) {
        return 0;
    }

    do {
        unsigned r = a % b;
        a = b;
        b = r;
    } while (b);

    return a;
}
