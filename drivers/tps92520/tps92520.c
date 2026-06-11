/*
 * SPDX-FileCopyrightText: 2026 ML!PA Consulting GmbH
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/**
 * @ingroup     drivers_tps92520
 * @{
 *
 * @file
 * @brief       TPS92520 Constant Current Source
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include "tps92520.h"
#include "byteorder.h"

#define ENABLE_DEBUG 1
#include "debug.h"

#define SPI_PARAM(dev) dev->params->spi, dev->params->cs_pin
#define SPI_ACQUIRE(dev) spi_acquire(dev->params->spi, dev->params->cs_pin, SPI_MODE_0, dev->params->spi_clk);
#define SPI_RELEASE(dev) spi_release(dev->params->spi)

#define CMD_WRITE       0x80
#define CMD_READ        0x00

#define REG_SYSCFG1     0x00
#define REG_SYSCFG2     0x01
#define REG_CMWTAP      0x02
#define REG_STATUS1     0x03
#define REG_STATUS2     0x04
#define REG_STATUS3     0x05
#define REG_TWLMT       0x06
#define REG_SLEEP       0x07
#define REG_CH1IADJL    0x08
#define REG_CH1IADJH    0x09
#define REG_CH2IADJL    0x0a
#define REG_CH2IADJH    0x0b
#define REG_PWMDIV      0x0c

#define REG_TEMPL       0x1b
#define REG_TEMPH       0x1c
#define REG_V5D         0x1d

#define SYSCFG1_CH1EN   (1 << 0)
#define SYSCFG1_CH2EN   (1 << 2)

static uint8_t _read_reg(tps92520_t *dev, uint8_t addr)
{
    uint16_t tx_buf = CMD_READ | (addr << 1);
    tx_buf = (tx_buf | !__builtin_parity(tx_buf));

    spi_transfer_bytes(SPI_PARAM(dev), false, &tx_buf, &tx_buf, sizeof(tx_buf));

    tx_buf = 0;
    spi_transfer_bytes(SPI_PARAM(dev), false, &tx_buf, &tx_buf, sizeof(tx_buf));

    uint8_t meta = tx_buf & 0xff;
    uint8_t data = tx_buf >> 8;
    DEBUG("_read_reg: got %02x (data: %02x)\n", meta, data);

    return data;
}

static void _write_reg(tps92520_t *dev, uint8_t addr, uint8_t write_data)
{
    uint16_t tx_buf = CMD_WRITE | (addr << 1);
    tx_buf = (tx_buf | !__builtin_parity(tx_buf));
    tx_buf |= write_data << 8;

    spi_transfer_bytes(SPI_PARAM(dev), false, &tx_buf, NULL, sizeof(tx_buf));
}

int tps92520_set_current(tps92520_t *dev, uint8_t chan, uint16_t val)
{
    assume(chan < 2);

    uint8_t reg = chan
                ? REG_CH2IADJL
                : REG_CH1IADJL;

    SPI_ACQUIRE(dev);

    _write_reg(dev, reg, val & 0x3);
    _write_reg(dev, reg + 1, val >> 2);

    SPI_RELEASE(dev);

    return 0;
}

int tps92520_enable(tps92520_t *dev, uint8_t chan)
{
    assume(chan < 2);

    uint8_t mask = chan
                 ? SYSCFG1_CH2EN
                 : SYSCFG1_CH1EN;

    SPI_ACQUIRE(dev);

    uint8_t val = _read_reg(dev, REG_SYSCFG1);
    val |= mask;
    _write_reg(dev, REG_SYSCFG1, val);

    SPI_RELEASE(dev);

    return 0;
}

int tps92520_disable(tps92520_t *dev, uint8_t chan)
{
    assume(chan < 2);

    uint8_t mask = chan
                 ? SYSCFG1_CH2EN
                 : SYSCFG1_CH1EN;

    SPI_ACQUIRE(dev);

    uint8_t val = _read_reg(dev, REG_SYSCFG1);
    val &= ~mask;
    _write_reg(dev, REG_SYSCFG1, val);

    SPI_RELEASE(dev);

    return 0;
}

int tps92520_get_temperature(tps92520_t *dev)
{
    uint16_t val;

    SPI_ACQUIRE(dev);

    val = _read_reg(dev, REG_TEMPL) & 0x3;
    val |= _read_reg(dev, REG_TEMPH) << 2;

    SPI_RELEASE(dev);

    return (val * 448 * 100) / 625 - 27151;
}

int tps92520_get_5V(tps92520_t *dev)
{
    uint8_t val;

    SPI_ACQUIRE(dev);

    val = _read_reg(dev, REG_V5D);

    SPI_RELEASE(dev);

    return (val * 533) / 255;
}

int tps92520_get_led_voltage(tps92520_t *dev, tps92520_chan_t chan)
{
    uint8_t val;

    SPI_ACQUIRE(dev);

    val = _read_reg(dev, chan);

    SPI_RELEASE(dev);

    return (val * 6500) / 255;
}

int tps92520_init(tps92520_t *dev, const tps92520_params_t *params)
{
    dev->params = params;

    spi_init_cs(params->spi, params->cs_pin);

    SPI_ACQUIRE(dev);

    /* read to clear power cycle bit */
    _read_reg(dev, REG_STATUS3);

    /* reset !FLT, disable watchdog */
    _write_reg(dev, REG_SYSCFG1, 0x0);

    SPI_RELEASE(dev);

    return 0;
}
