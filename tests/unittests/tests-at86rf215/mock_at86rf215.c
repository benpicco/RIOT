/*
 * Copyright (C) 2019 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @addtogroup  unittests
 * @{
 *
 * @file
 * @brief       Dummy implementation for AT86RF215 hardware access
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include <stdlib.h>
#include "at86rf215_internal.h"
#include "mock_at86rf215.h"

struct reg_cb {
    reg_callback_t cb;
    void* ctx;
};

static uint8_t *_devmem;
static struct reg_cb *write_cbs;
static struct reg_cb *read_cbs;

int at86rf215_hardware_reset(at86rf215_t *dev)
{
    (void) dev;
    return 0;
}

void at86rf215_reg_write(const at86rf215_t *dev, uint16_t reg, uint8_t value)
{
    (void) dev;

    if (write_cbs[reg].cb) {
        value = write_cbs[reg].cb((void*) dev, reg, value, write_cbs[reg].ctx);
    }

    _devmem[reg] = value;
}

uint8_t at86rf215_reg_read(const at86rf215_t *dev, uint16_t reg)
{
    (void) dev;

    if (read_cbs[reg].cb) {
        return read_cbs[reg].cb((void*) dev, reg, _devmem[reg], read_cbs[reg].ctx);
    }

    return _devmem[reg];
}

void at86rf215_reg_write_bytes(const at86rf215_t *dev, uint16_t reg, const void *data, size_t len)
{
    const uint8_t *in = data;

    while (len--) {
        at86rf215_reg_write(dev, reg++, *in++);
    }
}

void at86rf215_reg_read_bytes(const at86rf215_t *dev, uint16_t reg, void *data, size_t len)
{
    uint8_t *out = data;

    while (len--) {
        *out++ = at86rf215_reg_read(dev, reg++);
    }
}

void at86rf215_mock_reg_on_write_cb(uint16_t reg, reg_callback_t cb, void *ctx)
{
    write_cbs[reg].cb = cb;
    write_cbs[reg].ctx = ctx;
}

void at86rf215_mock_reg_on_read_cb(uint16_t reg, reg_callback_t cb, void *ctx)
{
    read_cbs[reg].cb = cb;
    read_cbs[reg].ctx = ctx;
}

void at86rf215_mock_init(const at86rf215_t *dev)
{
    if (_devmem != NULL) {
        free(_devmem);
    }

    if (write_cbs != NULL) {
        free(write_cbs);
    }

    if (read_cbs != NULL) {
        free(read_cbs);
    }

    _devmem = calloc(dev->BBC->RG_FBTXE, 1);
    write_cbs = calloc(dev->BBC->RG_FBTXE, sizeof(*write_cbs));
    read_cbs = calloc(dev->BBC->RG_FBTXE, sizeof(*read_cbs));
}
