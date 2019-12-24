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

static bool _enable_irq(i2c_t dev)
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

    _enable_irq(dev);

    /* enable the interface */
    i2c->CONSET = I2CONSET_I2EN;
}

static void irq_handler(i2c_t dev)
{
    lpc23xx_i2c_t *i2c = i2c_config[dev].dev;

    unsigned stat = i2c->STAT;

//    printf("STAT: %x\n", stat);

    /* clear interrupt flag */
    i2c->CONCLR = I2CONCLR_SIC;

    switch (stat) {
    case 0x08: /* A Start Condition is issued. */
    case 0x10: /* A repeated Start Condition is issued */
        ctx[dev].cur = ctx[dev].buf;
        ctx[dev].res = 0;
        i2c->DAT  = ctx[dev].addr;
        i2c->CONCLR = I2CONCLR_STAC;
        i2c->CONSET = I2CONSET_AA;
        break;

    case 0x20:  /* Address NACK received */
    case 0x48:
        /* send STOP */
        i2c->CONSET = I2CONSET_STO | I2CONSET_AA;
        ctx[dev].res = -ENXIO;
        mutex_unlock(&ctx[dev].tx_done);
        break;

    case 0x30:  /* Data NACK received */
    case 0x58:
        /* send STOP */
        i2c->CONSET = I2CONSET_STO | I2CONSET_AA;
        ctx[dev].res = -EIO;
        mutex_unlock(&ctx[dev].tx_done);
        break;

    case 0x18:
    case 0x28: /* Data byte has been transmitted */

        /* last byte transmitted */
        if (ctx[dev].cur == ctx[dev].end) {
            ctx[dev].res = 0;
            mutex_unlock(&ctx[dev].tx_done);
            break;
        }

        i2c->DAT = *ctx[dev].cur++;
        i2c->CONSET = I2CONSET_AA;
        break;

    case 0x38: /* Arbitration has been lost */
        i2c->CONSET = I2CONSET_STA | I2CONSET_AA;
        break;

    case 0x40: /* Master Receive, SLA_R has been sent */
        ctx[dev].res = 0;
        i2c->CONSET = I2CONSET_AA;
        break;

    case 0x50: /* Data byte has been received */

        *ctx[dev].cur = i2c->DAT;
        ++ctx[dev].cur;

        /* last byte received */
        if (ctx[dev].cur == ctx[dev].end) {
            i2c->CONCLR = I2CONCLR_AAC;
//            i2c->CONSET = I2CONSET_STO;
            ctx[dev].res = 0;
            mutex_unlock(&ctx[dev].tx_done);
            break;
        }

        i2c->CONSET = I2CONSET_AA;
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

int i2c_read_bytes(i2c_t dev, uint16_t addr,
                   void *data, size_t len, uint8_t flags)
{
    assert(dev < I2C_NUMOF);
    lpc23xx_i2c_t *i2c = i2c_config[dev].dev;

//    printf("R(%x, %d, %lx, %lx)\n", addr, len, i2c->STAT, i2c->CONSET);

    /* Check for wrong arguments given */
    if (data == NULL || len == 0) {
        return -EINVAL;
    }

    _init_buffer(dev, data, len);

    if (!(flags & I2C_NOSTART)) {
        ctx[dev].addr = (addr << 1) | 1;

        /* set Start flag */
        i2c->CONSET = I2CONSET_STA;
    }

    mutex_lock(&ctx[dev].tx_done);
    DEBUG("RX done!\n");

    while (i2c->CONSET & I2CONSET_STO) {}

#if 0
    if (!(ctx[dev].res || (flags & I2C_NOSTOP))) {
        /* set Stop flag */
        i2c->CONSET = I2CONSET_STO | I2CONSET_AA;
        while (i2c->CONSET & I2CONSET_STO) {}
        i2c->CONCLR = I2CONCLR_SIC;
        printf("[%lx]\n", i2c->CONSET);
    }
#endif
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

    if (!(flags & I2C_NOSTART)) {
        ctx[dev].addr = addr << 1;

        /* set Start flag */
        i2c->CONSET = I2CONSET_STA;
    }

    mutex_lock(&ctx[dev].tx_done);
    DEBUG("TX done!\n");

#if 0
    if (!(ctx[dev].res || (flags & I2C_NOSTOP))) {
        /* set Stop flag */
        puts("send STOP");
    }
#endif

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
