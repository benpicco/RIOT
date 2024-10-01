/*
 * Copyright (C) 2024 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     sys_stdio_null
 *
 * This module a wrapper for the stdio.h header intended to remove all calls
 * to stdout when stdio_null is used.
 *
 * This needs to reside in `core/` so it gets included early.
 *
 * @{
 *
 * @file
 * @brief       stdio wrapper to replace the C libs stdio
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#ifndef CORE_STDIO_H
#define CORE_STDIO_H
#include_next "stdio.h"

#ifdef MODULE_STDIO_NULL

#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

static inline int printf_null(const char *restrict format, ...)
{
    (void)format;
    return 0;
}

static inline int vprintf_null(const char *restrict format, va_list ap)
{
    (void)format;
    (void)ap;
    return 0;
}

#undef putchar
#undef puts
#undef printf
#undef vprintf

#define puts(s)     (void)s
#define putchar(c)  (void)c
#define printf(...) printf_null(__VA_ARGS__)
#define vprintf(format, ap) vprintf_null(format, ap)

#ifdef __cplusplus
}
#endif

#endif /* MODULE_STDIO_NULL */

#endif /* CORE_STDIO_H */
/** @} */
