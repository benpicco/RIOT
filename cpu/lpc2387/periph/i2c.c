/*
 * Copyright (C) 2019 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_lpc2387
 * @ingroup     drivers_periph_i2c
 * @{
 *
 * @file
 * @brief       Low-level I2C driver implementation for lpc23xx
 *
 * @author      Zakaria Kasmi <zkasmi@inf.fu-berlin.de>
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 *
 * @}
 */

#include <assert.h>
#include <stdint.h>
#include <errno.h>

#include "cpu.h"
#include "board.h"
#include "periph_conf.h"
#include "periph/i2c.h"

#include "sched.h"
#include "thread.h"
#include "mutex.h"

/**
 * @brief Array holding one pre-initialized mutex for each I2C device
 */
static mutex_t locks[I2C_NUMOF];

int i2c_acquire(i2c_t dev)
{
    assert(dev < I2C_NUMOF);
    mutex_lock(&locks[dev]);
    return 0;
}

void i2c_release(i2c_t dev)
{
    assert(dev < I2C_NUMOF);
    mutex_unlock(&locks[dev]);
}

static void poweron(lpc23xx_i2c_t *i2c)
{
    switch ((uint32_t)i2c) {
    case I2C0_BASE_ADDR:
        PCONP |= BIT7;
        break;
    case I2C1_BASE_ADDR:
        PCONP |= BIT19;
        break;
    case I2C2_BASE_ADDR:
        PCONP |= BIT26;
        break;
    }
}

static void _set_baudrate(lpc23xx_i2c_t *i2c, uint32_t baud)
{
    uint32_t pclksel, prescale;
    lpc2387_pclk_scale(CLOCK_CORECLOCK, baud, &pclksel, &prescale);

    switch ((uint32_t)i2c) {
    case I2C0_BASE_ADDR:
        PCLKSEL0 |= pclksel << 14;
        I20SCLL = prescale / 2;
        I20SCLH = prescale / 2;
        break;
    case I2C1_BASE_ADDR:
        PCLKSEL1 |= pclksel << 6;
        I21SCLL = prescale / 2;
        I21SCLH = prescale / 2;
        break;
    case I2C2_BASE_ADDR:
        PCLKSEL1 |= pclksel << 20;
        I22SCLL = prescale / 2;
        I22SCLH = prescale / 2;
        break;
    }
}

void i2c_init(i2c_t dev)
{
    assert(dev < I2C_NUMOF);

    /* Initialize mutex */
    mutex_init(&locks[dev]);

    const i2c_conf_t *cfg = &i2c_config[dev];
    lpc23xx_i2c_t *i2c = cfg->dev;

    /* configure SDA & SCL pins */
    *(&PINSEL0 + cfg->pinsel_sda) |= cfg->pinsel_msk_sda;
    *(&PINSEL0 + cfg->pinsel_scl) |= cfg->pinsel_msk_scl;

    poweron(i2c);

    /* clear control register */
    i2c->CONCLR = I2CONCLR_AAC | I2CONCLR_SIC | I2CONCLR_STAC
                | I2CONCLR_I2ENC;

    _set_baudrate(i2c, cfg->speed);

    /* enable the interface */
    i2c->CONSET = I2CONSET_I2EN;
}

int i2c_read_bytes(i2c_t dev, uint16_t addr,
                   void *data, size_t len, uint8_t flags)
{
    assert(dev < I2C_NUMOF);

    /* Check for wrong arguments given */
    if (data == NULL || len == 0) {
        return -EINVAL;
    }

    // TODO handle I2C_NOSTART, I2C_NOSTOP

    /* return 0 on success */
    return 0;
}

int i2c_write_bytes(i2c_t dev, uint16_t addr, const void *data, size_t len,
                    uint8_t flags)
{
    assert(dev < I2C_NUMOF);

    /* Check for wrong arguments given */
    if (data == NULL || len == 0) {
        return -EINVAL;
    }

    if (!(flags & I2C_NOSTART)) {
        /* set Start flag */
        dev->dev->CONSET = I2CONSET_STA;
    }

    // TODO: write

    if (!(flags & I2C_NOSTOP)) {
        /* set Stop flag */
        dev->dev->CONSET = I2CONSET_STO;
    }

    /* return 0 on success */
    return 0;
}

