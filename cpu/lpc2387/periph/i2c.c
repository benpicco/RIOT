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
static i2c_state_t i2c_state[I2C_NUMOF];

#if I2C_NUMOF > 0
static void I2C0_IRQHandler(void) __attribute__((interrupt("IRQ")));
#endif
#if I2C_NUMOF > 1
static void I2C1_IRQHandler(void) __attribute__((interrupt("IRQ")));
#endif
#if I2C_NUMOF > 2
static void I2C2_IRQHandler(void) __attribute__((interrupt("IRQ")));
#endif

/**
 * @brief Array holding one pre-initialized mutex for each I2C device
 */
static mutex_t locks[I2C_NUMOF];

struct i2c_ctx {
    i2c_t dev;
    i2c_state_t state;
    mutex_t lock;
    mutex_t tx_done;
};

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

static void _enable_irq(i2c_t dev)
{
    switch ((uint32_t)i2c_config[dev].dev) {
#if I2C_NUMOF > 0
    case I2C0_BASE_ADDR:
        install_irq(I2C0_INT, I2C0_IRQHandler, i2c_config[dev].irq_prio);
        break;
#endif
#if I2C_NUMOF > 1
    case I2C1_BASE_ADDR:
        install_irq(I2C1_INT, I2C1_IRQHandler, i2c_config[dev].irq_prio);
        break;
#endif
#if I2C_NUMOF > 2
    case I2C2_BASE_ADDR:
        install_irq(I2C2_INT, I2C2_IRQHandler, i2c_config[dev].irq_prio);
        break;
    }
#endif
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

    _enable_irq(dev);
}

static i2c_state_t _i2c_do(lpc23xx_i2c_t *i2c, i2c_state_t state, uint8_t data)
{
   switch (i2c->STAT) {
        case 0x08: /* A Start condition is issued. */
            //puts("A Start condition is issued\n");
            i2c->DAT = data;
            //printf("I22DAT = %lu\n", I22DAT);
            i2c->CONCLR = (I2CONCLR_SIC | I2CONCLR_STAC);
            return I2C_STARTED;

        case 0x10: /* A repeated started is issued */
            //puts("A repeated Start is issued\n");
            //  if ( I2CCmd == L3DG420_WHO_AM_I)
            //  {
            //    I22DAT = I2CMasterBuffer[2];
            //  }
            i2c->DAT = data; // ??
            i2c->CONCLR = (I2CONCLR_SIC | I2CONCLR_STAC);
            return I2C_RESTARTED;

        case 0x18: /* Regardless, it's a ACK */

            //puts("got an Ack\n");
            if (state == I2C_STARTED) {
                i2c->DAT = data;
                return DATA_ACK;
            }

            i2c->CONCLR = I2CONCLR_SIC;
            return state;

        case 0x28: /* Data byte has been transmitted, regardless ACK or NACK */
        case 0x30:

            //puts("Data byte has been transmitted\n");
            if (wr_index != i2c_write_length) {

                // this should be the last one
                I20DAT = i2c_master_buffer[1 + wr_index];

                if (wr_index != i2c_write_length) {
                    i2c_master_state = DATA_ACK;
                }
                else {
                    i2c_master_state = DATA_NACK;
                    I20CONSET = I2CONSET_STO; /* Set Stop flag */

                    if (i2c_read_length != 0) {
                        I20CONSET = I2CONSET_STA; /* Set Repeated-start flag */
                        i2c_master_state = I2C_REPEATED_START;
                    }
                }

                wr_index++;
            }
            else {
                if (i2c_read_length != 0) {
                    I20CONSET = I2CONSET_STA; /* Set Repeated-start flag */
                    i2c_master_state = I2C_REPEATED_START;
                }
                else {
                    i2c_master_state = DATA_NACK;
                    I20CONSET = I2CONSET_STO; /* Set Stop flag */
                }
            }

            I20CONCLR = I2CONCLR_SIC;
            break;

        case 0x40: /* Master Receive, SLA_R has been sent */

            //puts("Master Receive, SLA_R has been sent!\n");
            if (i2c_read_length >= 2) {
                I20CONSET = I2CONSET_AA; /* assert ACK after data is received */
            }

            I20CONCLR = I2CONCLR_SIC;
            break;

            // Data byte has been received, regardless following ACK or NACK
        case 0x50:
        case 0x58:
            //puts("Data received\n");
            i2c_master_buffer[3 + rd_index] = I20DAT;
            rd_index++;

            if (rd_index < (i2c_read_length - 1)) {
                i2c_master_state = DATA_ACK;
                I20CONSET = I2CONSET_AA; /* assert ACK after data is received */
            }
            else {
                I20CONCLR = I2CONCLR_AAC; /* NACK after data is received */
            }

            if (rd_index == i2c_read_length) {
                rd_index = 0;
                i2c_master_state = DATA_NACK;
            }

            I20CONCLR = I2CONCLR_SIC;
            break;

        case 0x20: /* regardless, it's a NACK */
        case 0x48:
            I20CONCLR = I2CONCLR_SIC;
            i2c_master_state = DATA_NACK;
            break;

        case 0x38: /*
                    * Arbitration lost, in this example, we don't
                    *  deal with multiple master situation
                    **/

            //puts("Arbritration lost!\n");
        default:
            I20CONCLR = I2CONCLR_SIC;
            break;
    }
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
    lpc23xx_i2c_t *i2c = i2c_config[dev].dev;


    /* Check for wrong arguments given */
    if (data == NULL || len == 0) {
        return -EINVAL;
    }

    if (!(flags & I2C_NOSTART)) {
        /* set Start flag */
        i2c->CONSET = I2CONSET_STA;
    }

    _i2c_do(i2c, i2c_state[dev], addr << 1);

    if (!(flags & I2C_NOSTOP)) {
        /* set Stop flag */
        i2c->CONSET = I2CONSET_STO;
        i2c->CONCLR = I2CONCLR_SIC;
    }

    /* return 0 on success */
    return 0;
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
