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


#define SYSCFG_CH1EN    (1 << 0)
#define SYSCFG_CH2EN    (1 << 2)

static int _read_reg(tps92520_t *dev, uint8_t addr)
{
    uint16_t tx_buf = CMD_READ | (addr << 1);
    tx_buf = (tx_buf | __builtin_parity(tx_buf)) << 8;

    spi_transfer_bytes(SPI_PARAM(dev), false, &tx_buf, NULL, sizeof(tx_buf));

    tx_buf = 0;
    spi_transfer_bytes(SPI_PARAM(dev), false, &tx_buf, &tx_buf, sizeof(tx_buf));

    uint8_t meta = tx_buf >> 8;
    DEBUG("_read_reg: got %02x (data: %02x)\n", meta, tx_buf & 0xFF);

    return tx_buf & 0xFF;
}

static int _write_reg(tps92520_t *dev, uint8_t addr, uint8_t write_data)
{
    uint16_t tx_buf = CMD_WRITE | (addr << 1);
    tx_buf = (tx_buf | __builtin_parity(tx_buf)) << 8;
    tx_buf |= write_data;

    spi_transfer_bytes(SPI_PARAM(dev), false, &tx_buf, NULL, sizeof(tx_buf));

    return 0;
}

int tps92520_init(tps92520_t *dev, const tps92520_params_t *params)
{
    int res;

    dev->params = params;

    spi_init_cs(params->spi, params->cs_pin);

    SPI_ACQUIRE(dev);

    /* read to clear power cycle bit */
    res = _read_reg(dev, REG_STATUS3);
    if (res < 0) {
        goto out;
    }

    /* reset !FLT, disable watchdog */
    res = _write_reg(dev, REG_SYSCFG1, 0x0);
    if (res < 0) {
        goto out;
    }


    res = 0;
out:
    SPI_RELEASE(dev);
    return res;
}
