/**
 * Copyright (C) 2017 Kaspar Schleiser <kaspar@schleiser.de>
 *               2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

 /**
 * @ingroup     sys_random
 * @{
 * @file
 *
 * @brief       PRNG seeding
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @}
 */

#include <stdint.h>
#include <assert.h>

#include "log.h"
#include "random.h"
#include "bitarithm.h"

#ifdef MODULE_PUF_SRAM
#include "puf_sram.h"
#endif
#ifdef MODULE_PERIPH_HWRNG
#include "periph/hwrng.h"
#endif
#ifdef MODULE_PERIPH_CPUID
#include "luid.h"
#endif

#define ENABLE_DEBUG (0)
#include "debug.h"

#define ROUND_UP_PTR(n) (void*)(4 + (~3 & ((intptr_t)(n) - 1)))

static inline void copy_bytewise(void *dest, const void *src, size_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;

    switch (n) {
    case 3:
        *d++ = *s++;
        /* fall-through */
    case 2:
        *d++ = *s++;
        /* fall-through */
    case 1:
        *d++ = *s++;
        /* fall-through */
    }
}

void auto_init_random(void)
{
    uint32_t seed;
#ifdef MODULE_PUF_SRAM
    /* TODO: hand state to application? */
    if (puf_sram_state) {
        LOG_WARNING("random: PUF SEED not fresh\n");
    }
    seed = puf_sram_seed;
#elif defined (MODULE_PERIPH_HWRNG)
    hwrng_read(&seed, 4);
#elif defined (MODULE_PERIPH_CPUID)
    luid_get(&seed, 4);
#else
    LOG_WARNING("random: NO SEED AVAILABLE!\n");
    seed = RANDOM_SEED_DEFAULT;
#endif
    DEBUG("random: using seed value %u\n", (unsigned)seed);
    random_init(seed);
}

void random_bytes(uint8_t *target, size_t n)
{
    uint32_t tmp;
    uint32_t *b32;
    size_t unaligned;

    /* directly jump to fast path if destination buffer
       is properly aligned */
    if (!((intptr_t)target & 0x3)) {
        b32 = (void*)target;
        goto fast;
    }

    b32 = ROUND_UP_PTR(target);
    unaligned = (uintptr_t)b32 - (uintptr_t)target;

    tmp = random_uint32();
    if (n < unaligned) {
        unaligned = n;
    }

    copy_bytewise(target, &tmp, unaligned);
    n -= unaligned;

fast:
    while (n >= sizeof(uint32_t)) {
        *b32++ = random_uint32();
        n -= sizeof(uint32_t);
    }

    if (!n) {
        return;
    }

    tmp = random_uint32();
    copy_bytewise(b32, &tmp, n);
}

uint32_t random_uint32_range(uint32_t a, uint32_t b)
{
    assert(a < b);

    uint32_t divisor, rand_val, range = b - a;
    uint8_t range_msb = bitarithm_msb(range);

    /* check if range is a power of two */
    if (!(range & (range - 1))) {
        divisor = (1 << range_msb) - 1;
    }
    else if (range_msb < 31) {
        /* leftshift for next power of two interval */
        divisor = (1 << (range_msb + 1)) -1;
    }
    else {
        /* disable modulo operation in loop below */
        divisor = UINT32_MAX;
    }
    /* get random numbers until value is smaller than range */
    do {
        rand_val = (random_uint32() & divisor);
    } while (rand_val >= range);
    /* return random in range [a,b] */
    return (rand_val + a);
}
