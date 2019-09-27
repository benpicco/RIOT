/*
 * Copyright (C) 2015 Freie Universität Berlin
 *               2015 FreshTemp, LLC.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_sam0_common
 * @ingroup     drivers_periph_uart
 * @{
 *
 * @file
 * @brief       Low-level UART driver implementation
 *
 * @author      Thomas Eichinger <thomas.eichinger@fu-berlin.de>
 * @author      Troels Hoffmeyer <troels.d.hoffmeyer@gmail.com>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Dylan Laduranty <dylanladuranty@gmail.com>
 *
 * @}
 */

#include "cpu.h"

#include "periph/uart.h"
#include "periph/gpio.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/**
 * @brief   Allocate memory to store the callback functions
 */
static uart_isr_ctx_t uart_ctx[UART_NUMOF];

/**
 * @brief   Get the pointer to the base register of the given UART device
 *
 * @param[in] dev       UART device identifier
 *
 * @return              base register address
 */
static inline SercomUsart *dev(uart_t dev)
{
    return uart_config[dev].dev;
}

int uart_init(uart_t uart, uint32_t baudrate, uart_rx_cb_t rx_cb, void *arg)
{
    if (uart >= UART_NUMOF) {
        return UART_NODEV;
    }

    /* must disable here first to ensure idempotency */
    dev(uart)->CTRLA.reg &= ~(SERCOM_USART_CTRLA_ENABLE);

    /* configure pins */
    if (uart_config[uart].rx_pin != GPIO_UNDEF) {
        gpio_init(uart_config[uart].rx_pin, GPIO_IN);
        gpio_init_mux(uart_config[uart].rx_pin, uart_config[uart].mux);
    }
    gpio_init(uart_config[uart].tx_pin, GPIO_OUT);
    gpio_set(uart_config[uart].tx_pin);
    gpio_init_mux(uart_config[uart].tx_pin, uart_config[uart].mux);

    /* enable peripheral clock */
    sercom_clk_en(dev(uart));

    /* reset the UART device */
    dev(uart)->CTRLA.reg = SERCOM_USART_CTRLA_SWRST;
    while (dev(uart)->SYNCBUSY.bit.SWRST) {}

    /* configure clock generator */
    sercom_set_gen(dev(uart), uart_config[uart].gclk_src);

    /* set asynchronous mode w/o parity, LSB first, TX and RX pad as specified
     * by the board in the periph_conf.h, x16 sampling and use internal clock */
    dev(uart)->CTRLA.reg = (SERCOM_USART_CTRLA_DORD |
                            SERCOM_USART_CTRLA_SAMPR(0x0) |
                            SERCOM_USART_CTRLA_TXPO(uart_config[uart].tx_pad) |
                            SERCOM_USART_CTRLA_RXPO(uart_config[uart].rx_pad) |
                            SERCOM_USART_CTRLA_MODE(0x1));
    /* Set run in standby mode if enabled */
    if (uart_config[uart].flags & UART_FLAG_RUN_STANDBY) {
        dev(uart)->CTRLA.reg |= SERCOM_USART_CTRLA_RUNSTDBY;
    }

    /* calculate and set baudrate */
    uint64_t tmp = (uint64_t)baudrate << 20;
    tmp = (tmp + (CLOCK_CORECLOCK >> 1)) / CLOCK_CORECLOCK;

    /* Verify that the calculated result is within range */
    if (tmp < 1 || tmp > UINT16_MAX) {
        return -UART_NOBAUD;
    }

    dev(uart)->BAUD.reg = 0x10000 - (uint16_t)tmp;

    /* enable transmitter, and configure 8N1 mode */
    dev(uart)->CTRLB.reg = SERCOM_USART_CTRLB_TXEN;
    /* enable receiver and RX interrupt if configured */
    if ((rx_cb) && (uart_config[uart].rx_pin != GPIO_UNDEF)) {
        uart_ctx[uart].rx_cb = rx_cb;
        uart_ctx[uart].arg = arg;
#if defined (CPU_SAML1X) || defined (CPU_SAMD5X)
        NVIC_EnableIRQ(SERCOM0_2_IRQn + (sercom_id(dev(uart)) * 4));
#else
        NVIC_EnableIRQ(SERCOM0_IRQn + sercom_id(dev(uart)));
#endif
        dev(uart)->CTRLB.reg |= SERCOM_USART_CTRLB_RXEN;
        dev(uart)->INTENSET.reg |= SERCOM_USART_INTENSET_RXC;
        /* set wakeup receive from sleep if enabled */
        if (uart_config[uart].flags & UART_FLAG_WAKEUP) {
            dev(uart)->CTRLB.reg |= SERCOM_USART_CTRLB_SFDE;
        }
    }
    while (dev(uart)->SYNCBUSY.bit.CTRLB) {}

    /* and finally enable the device */
    dev(uart)->CTRLA.reg |= SERCOM_USART_CTRLA_ENABLE;

    return UART_OK;
}

void uart_write(uart_t uart, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        while (!dev(uart)->INTFLAG.bit.DRE) {}
        dev(uart)->DATA.reg = data[i];
    }
    while (!dev(uart)->INTFLAG.bit.TXC) {}
}

void uart_poweron(uart_t uart)
{
    sercom_clk_en(dev(uart));
    dev(uart)->CTRLA.reg |= SERCOM_USART_CTRLA_ENABLE;
}

void uart_poweroff(uart_t uart)
{
    dev(uart)->CTRLA.reg &= ~(SERCOM_USART_CTRLA_ENABLE);
    sercom_clk_dis(dev(uart));
}

#ifdef MODULE_PERIPH_UART_MODECFG
int uart_mode(uart_t uart, uart_data_bits_t data_bits, uart_parity_t parity,
              uart_stop_bits_t stop_bits)
{
    if (uart >= UART_NUMOF) {
        return UART_NODEV;
    }

    if (stop_bits != UART_STOP_BITS_1 && stop_bits != UART_STOP_BITS_2) {
        return UART_NOMODE;
    }

    if (parity != UART_PARITY_NONE && parity != UART_PARITY_EVEN &&
            parity != UART_PARITY_ODD) {
        return UART_NOMODE;
    }

    /* Disable UART first to remove write protect */
    dev(uart)->CTRLA.bit.ENABLE = 0;
    while (dev(uart)->SYNCBUSY.bit.ENABLE) {}

    dev(uart)->CTRLB.bit.CHSIZE = data_bits;

    if (parity == UART_PARITY_NONE) {
        dev(uart)->CTRLA.bit.FORM = 0x0;
    }
    else {
        dev(uart)->CTRLA.bit.FORM = 0x1;
        dev(uart)->CTRLB.bit.PMODE = (parity == UART_PARITY_ODD) ? 1 : 0;
    }

    dev(uart)->CTRLB.bit.SBMODE = (stop_bits == UART_STOP_BITS_1) ? 0 : 1;

    /* Enable UART again */
    dev(uart)->CTRLA.bit.ENABLE = 1;
    while (dev(uart)->SYNCBUSY.bit.ENABLE) {}

    return UART_OK;
}
#endif

static inline void irq_handler(unsigned uartnum)
{
    if (dev(uartnum)->INTFLAG.bit.RXC) {
        /* interrupt flag is cleared by reading the data register */
        uart_ctx[uartnum].rx_cb(uart_ctx[uartnum].arg,
                                (uint8_t)(dev(uartnum)->DATA.reg));
    }
    else if (dev(uartnum)->INTFLAG.bit.ERROR) {
        /* clear error flag */
        dev(uartnum)->INTFLAG.reg = SERCOM_USART_INTFLAG_ERROR;
    }

    cortexm_isr_end();
}

#ifdef UART_0_ISR
void UART_0_ISR(void)
{
    irq_handler(0);
}
#endif

#ifdef UART_1_ISR
void UART_1_ISR(void)
{
    irq_handler(1);
}
#endif

#ifdef UART_2_ISR
void UART_2_ISR(void)
{
    irq_handler(2);
}
#endif

#ifdef UART_3_ISR
void UART_3_ISR(void)
{
    irq_handler(3);
}
#endif

#ifdef UART_4_ISR
void UART_4_ISR(void)
{
    irq_handler(4);
}
#endif

#ifdef UART_5_ISR
void UART_5_ISR(void)
{
    irq_handler(5);
}
#endif
