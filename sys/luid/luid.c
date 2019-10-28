/*
 * Copyright (C) 2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_luid
 * @{
 *
 * @file
 * @brief       LUID module implementation
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdint.h>
#include <string.h>

#include "assert.h"
#include "periph/cpuid.h"

#include "luid.h"

#if CPUID_LEN
void __attribute__((weak)) luid_base(void *buf, size_t len)
{
    uint8_t *out = (uint8_t *)buf;
    uint8_t cid[CPUID_LEN];

    cpuid_get(cid);
    for (size_t i = 0; i < len; i++) {
        out[i] = cid[i % CPUID_LEN];
    }
}
#else
void __attribute__((weak)) luid_base(void *buf, size_t len)
{
    memset(buf, LUID_BACKUP_SEED, len);
}
#endif

static uint8_t lastused = 1;

void luid_get(void *buf, size_t len)
{
    luid_base(buf, len);

    uint8_t *out = (uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        out[i] ^= lastused;
    }

    ++lastused;
}

void luid_custom(void *buf, size_t len, int gen)
{
    luid_base(buf, len);

    for (size_t i = 0; i < sizeof(gen); i++) {
        ((uint8_t *)buf)[i % len] ^= ((gen >> (i * 8)) & 0xff);
    }
}
