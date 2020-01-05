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

#define ENABLE_DEBUG (0)
#include "debug.h"

/**
 * @brief State of the I2C master state machine
 */
typedef enum {
    I2C_IDLE = 0,
    I2C_STARTED,
    I2C_RESTARTED,
    I2C_REPEATED_START,
    DATA_ACK,
    DATA_NACK,
} i2c_state_t;

#if I2C_NUMOF > 0
static void I2C0_IRQHandler(void) __attribute__((interrupt("IRQ")));
#endif
#if I2C_NUMOF > 1
static void I2C1_IRQHandler(void) __attribute__((interrupt("IRQ")));
#endif
#if I2C_NUMOF > 2
static void I2C2_IRQHandler(void) __attribute__((interrupt("IRQ")));
#endif

static struct i2c_ctx {
    i2c_t dev;
    uint8_t addr;
    mutex_t lock;
    mutex_t tx_done;
    uint8_t *buf;
    uint8_t *cur;
    uint8_t *end;
    int res;
} ctx[I2C_NUMOF];

int i2c_acquire(i2c_t dev)
{
    assert(dev < I2C_NUMOF);
    mutex_lock(&ctx[dev].lock);
    return 0;
}

void i2c_release(i2c_t dev)
{
    assert(dev < I2C_NUMOF);
    mutex_unlock(&ctx[dev].lock);
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

static void _enable_irq(lpc23xx_i2c_t *i2c)
{
    switch ((uint32_t)i2c) {
    case I2C0_BASE_ADDR:
        VICIntEnable = 1 << I2C0_INT;
        break;
    case I2C1_BASE_ADDR:
        VICIntEnable = 1 << I2C1_INT;
        break;
    case I2C2_BASE_ADDR:
        VICIntEnable = 1 << I2C2_INT;
        break;
    }
}

static void _disable_irq(lpc23xx_i2c_t *i2c)
{
    switch ((uint32_t)i2c) {
    case I2C0_BASE_ADDR:
        VICIntEnClr = 1 << I2C0_INT;
        break;
    case I2C1_BASE_ADDR:
        VICIntEnClr = 1 << I2C1_INT;
        break;
    case I2C2_BASE_ADDR:
        VICIntEnClr = 1 << I2C2_INT;
        break;
    }
}

static void _set_baudrate(lpc23xx_i2c_t *i2c, uint32_t baud)
{
    (void) baud;
    uint32_t pclksel, prescale;
    lpc2387_pclk_scale(CLOCK_CORECLOCK, baud, &pclksel, &prescale);

    switch ((uint32_t)i2c) {
    case I2C0_BASE_ADDR:
        PCLKSEL0 &= ~(BIT14 | BIT15);
        PCLKSEL0 |= pclksel << 14;
        I20SCLL = prescale / 2;
        I20SCLH = prescale / 2;
        break;
    case I2C1_BASE_ADDR:
        PCLKSEL1 &= ~(BIT6 | BIT7);
        PCLKSEL1 |= pclksel << 6;
        I21SCLL = prescale / 2;
        I21SCLH = prescale / 2;
        break;
    case I2C2_BASE_ADDR:
        PCLKSEL1 &= ~(BIT20 | BIT21);
        PCLKSEL1 |= pclksel << 20;
        I22SCLL = prescale / 2;
        I22SCLH = prescale / 2;
        break;
    }
}

static bool _install_irq(i2c_t dev)
{
    switch ((uint32_t)i2c_config[dev].dev) {
#if I2C_NUMOF > 0
    case I2C0_BASE_ADDR:
        return install_irq(I2C0_INT, I2C0_IRQHandler, i2c_config[dev].irq_prio);
#endif
#if I2C_NUMOF > 1
    case I2C1_BASE_ADDR:
        return install_irq(I2C1_INT, I2C1_IRQHandler, i2c_config[dev].irq_prio);
#endif
#if I2C_NUMOF > 2
    case I2C2_BASE_ADDR:
        return install_irq(I2C2_INT, I2C2_IRQHandler, i2c_config[dev].irq_prio);
#endif
    }

    return false;
}

void i2c_init(i2c_t dev)
{
    assert(dev < I2C_NUMOF);

    /* Initialize mutex */
    mutex_init(&ctx[dev].lock);
    mutex_init(&ctx[dev].tx_done);
    mutex_lock(&ctx[dev].tx_done);

    const i2c_conf_t *cfg = &i2c_config[dev];
    lpc23xx_i2c_t *i2c = cfg->dev;

    poweron(i2c);

    /* configure SDA & SCL pins */
    *(&PINSEL0 + cfg->pinsel_sda) |= cfg->pinsel_msk_sda;
    *(&PINSEL0 + cfg->pinsel_scl) |= cfg->pinsel_msk_scl;

    /* clear control register */
    i2c->CONCLR = I2CONCLR_AAC | I2CONCLR_SIC | I2CONCLR_STAC
                | I2CONCLR_I2ENC;

    _set_baudrate(i2c, cfg->speed);

    _install_irq(dev);

    /* enable the interface */
    i2c->CONSET = I2CONSET_I2EN;
}

static void _end_tx(i2c_t dev, unsigned res)
{
//    printf("irq end (%lx)\n", i2c_config[dev].dev->CONSET);
    ctx[dev].res = res;
    mutex_unlock(&ctx[dev].tx_done);

    _disable_irq(i2c_config[dev].dev);
}

static void irq_handler(i2c_t dev)
{
    lpc23xx_i2c_t *i2c = i2c_config[dev].dev;

    unsigned stat = i2c->STAT;

//    printf("<%x>\n", stat);

    switch (stat) {
    case 0x00:
        puts("Bus Error");
        break;
    case 0x08: /* A Start Condition is issued. */
    case 0x10: /* A repeated Start Condition is issued */
        ctx[dev].cur = ctx[dev].buf;
        ctx[dev].res = 0;
        i2c->DAT  = ctx[dev].addr;
        i2c->CONSET = I2CONSET_AA;
        i2c->CONCLR = I2CONCLR_STAC | I2CONCLR_SIC;
        break;

    case 0x20:  /* Address NACK received */
    case 0x48:
        /* send STOP */
        i2c->CONSET = I2CONSET_STO | I2CONSET_AA;
        i2c->CONCLR = I2CONCLR_SIC;
        _end_tx(dev, -ENXIO);
        break;

    case 0x58:
        if (ctx[dev].cur == ctx[dev].end) {
            _disable_irq(i2c);
            break;
        }

//        puts("got last byte");
        *ctx[dev].cur++ = i2c->DAT;
        i2c->CONCLR = I2CONCLR_AAC;
        _end_tx(dev, 0);
        break;

    case 0x18:
        i2c->DAT = *ctx[dev].buf;
        ctx[dev].cur = ctx[dev].buf + 1;
        i2c->CONCLR = I2CONCLR_SIC;

        break;

    case 0x28: /* Data byte has been transmitted */
    case 0x30:

        /* last byte transmitted */
        if (ctx[dev].cur == ctx[dev].end) {
            i2c->CONCLR = I2CONCLR_AAC;
            _end_tx(dev, 0);
        } else {
            i2c->DAT = *ctx[dev].cur++;
            i2c->CONSET = I2CONSET_AA;
            i2c->CONCLR = I2CONCLR_SIC;
        }

        break;

    case 0x38: /* Arbitration has been lost */
        i2c->CONSET = I2CONSET_STA | I2CONSET_AA;
        i2c->CONCLR = I2CONCLR_SIC;
        break;

    case 0x40: /* Master Receive, SLA_R has been sent */
        ctx[dev].res = 0;

        /* if we only read one byte, send NACK here already */
        if (ctx[dev].cur + 1 == ctx[dev].end) {
            i2c->CONCLR = I2CONCLR_AAC;
        } else {
            i2c->CONSET = I2CONSET_AA;
        }
        i2c->CONCLR = I2CONCLR_SIC;
        break;

    case 0x50: /* Data byte has been received */

        *ctx[dev].cur++ = i2c->DAT;

        /* last byte received */
        if (ctx[dev].cur + 1 == ctx[dev].end) {
            i2c->CONCLR = I2CONCLR_AAC;
        } else {
            i2c->CONSET = I2CONSET_AA;
        }

        i2c->CONCLR = I2CONCLR_SIC;
        break;
    }
}

static void _init_buffer(i2c_t dev, uint8_t *data, size_t len)
{
    ctx[dev].buf = data;
    ctx[dev].cur = data;
    ctx[dev].end = data + len;
    ctx[dev].res = -ETIMEDOUT;
}

static void _stop(lpc23xx_i2c_t *i2c)
{
//    puts("send stop");

    /* set Stop flag */
    i2c->CONSET = I2CONSET_STO;

    /* clear interrupt flag */
    i2c->CONCLR = I2CONCLR_SIC;

    while (i2c->CONSET & I2CONSET_STO) {}
}

int i2c_read_bytes(i2c_t dev, uint16_t addr,
                   void *data, size_t len, uint8_t flags)
{
    assert(dev < I2C_NUMOF);
    lpc23xx_i2c_t *i2c = i2c_config[dev].dev;

    /* Check for wrong arguments given */
    if (data == NULL || len == 0) {
        return -EINVAL;
    }

    _init_buffer(dev, data, len);
    ctx[dev].addr = (addr << 1) | 1;

    if (flags & I2C_NOSTART) {
        i2c->CONCLR = I2CONCLR_SIC;
    } else {
        /* set Start flag */
        i2c->CONSET = I2CONSET_STA;
    }

    _enable_irq(i2c);

    mutex_lock(&ctx[dev].tx_done);

    if ((ctx[dev].res == 0) && !(flags & I2C_NOSTOP)) {
        _stop(i2c);
    }

    return ctx[dev].res;
}

int i2c_write_bytes(i2c_t dev, uint16_t addr, const void *data, size_t len,
                    uint8_t flags)
{
    assert(dev < I2C_NUMOF);
    lpc23xx_i2c_t *i2c = i2c_config[dev].dev;

    /* Check for wrong arguments given */
    if (data == NULL || len == 0) {
        return -EINVAL;
    }

    _init_buffer(dev, (void*) data, len);
    ctx[dev].addr = addr << 1;

    if (flags & I2C_NOSTART) {
        i2c->CONCLR = I2CONCLR_SIC;
    } else {
        /* set Start flag */
        i2c->CONSET = I2CONSET_STA;
    }

    _enable_irq(i2c);
    mutex_lock(&ctx[dev].tx_done);

    if ((ctx[dev].res == 0) && !(flags & I2C_NOSTOP)) {
        _stop(i2c);
    }

    return ctx[dev].res;
}

#if I2C_NUMOF > 0
static void I2C0_IRQHandler(void)
{
    irq_handler(0);
    VICVectAddr = 0;                    /* Acknowledge Interrupt */
}
#endif
#if I2C_NUMOF > 1
static void I2C1_IRQHandler(void)
{
    irq_handler(1);
    VICVectAddr = 0;                    /* Acknowledge Interrupt */
}
#endif
#if I2C_NUMOF > 2
static void I2C2_IRQHandler(void)
{
    irq_handler(2);
    VICVectAddr = 0;                    /* Acknowledge Interrupt */
}
#endif
