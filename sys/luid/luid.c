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

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

static void _nhash(const void *_in, size_t in_len,
                   void *_out, size_t out_len)
{
    const uint8_t *in = _in;
    uint8_t *out = _out;

    static const uint8_t primes[] = { 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41,
                                     43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113};

    uint32_t sum = 0xAA;
    size_t iterations = MAX(in_len, out_len);
    for (size_t i = 0; i < iterations; ++i) {
        uint8_t c = in[i % in_len];
        sum += c * primes[c % ARRAY_SIZE(primes)] + 1;
        out[i % out_len] = sum & 0xFF;
    }
}

void __attribute__((weak)) luid_base(void *buf, size_t len)
{
    memset(buf, LUID_BACKUP_SEED, len);

#if CPUID_LEN
    uint8_t in[CPUID_LEN];
    cpuid_get(in);
#else
    uint8_t in[] = { LUID_BACKUP_SEED };
#endif

    _nhash(&in, sizeof(in), buf, len);
}

static uint8_t lastused;

void luid_get(void *buf, size_t len)
{
    luid_base(buf, len);

    ((uint8_t *)buf)[0] ^= lastused++;
}

void luid_custom(void *buf, size_t len, int gen)
{
    luid_base(buf, len);

    for (size_t i = 0; i < sizeof(gen); i++) {
        ((uint8_t *)buf)[i % len] ^= ((gen >> (i * 8)) & 0xff);
    }
}

void luid_get_short(network_uint16_t *addr)
{
    luid_base(addr, sizeof(*addr));
    addr->u8[1] ^= lastused++;

    /* https://tools.ietf.org/html/rfc4944#section-12 requires the first bit to
     * 0 for unicast addresses */
    addr->u8[0] &= 0x7F;
}

void luid_get_eui48(eui48_t *addr)
{
    luid_base(addr, sizeof(*addr));
    addr->uint8[5] ^= lastused++;

    eui48_set_local(addr);
    eui48_clear_group(addr);
}

void luid_get_eui64(eui64_t *addr)
{
    luid_base(addr, sizeof(*addr));
    addr->uint8[7] ^= lastused++;

    eui64_set_local(addr);
    eui64_clear_group(addr);
}
