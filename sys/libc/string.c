/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include "string_utils.h"

ssize_t strscpy(char *dest, const char *src, size_t count)
{
    const char *start = dest;

    if (!count) {
        return -E2BIG;
    }

    while (--count && *src) {
        *dest++ = *src++;
    }

    *dest = 0;

    if (*src == 0) {
        return dest - start;
    } else {
        return -E2BIG;
    }
}

const void *memchk(const void *data, uint8_t c, size_t len)
{
    const uint8_t *end = (uint8_t *)data + len;
    for (const uint8_t *d = data; d != end; ++d) {
        if (c != *d) {
            return d;
        }
    }

    return NULL;
}

int swprintf(string_writer_t *sw, const char *restrict format, ...)
{
    va_list args;
    int res;

    /* make sure flash_vsnprintf() is not used */
    #if HAS_FLASH_UTILS_ARCH
    #undef vsnprintf
    #endif

    va_start(args, format);
    res = vsnprintf(sw->position, sw->capacity, format, args);
    va_end(args);

    if (res <= (int)sw->capacity) {
        sw->capacity -= res;
        sw->position += res;
    } else {
        sw->position += sw->capacity;
        sw->capacity = 0;
        res = -E2BIG;
    }

    return res;
}

/** @} */
