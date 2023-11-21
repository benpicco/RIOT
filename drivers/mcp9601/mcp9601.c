/*
 * Copyright (C) 2021-2023 ML!PA Consulting GmbH
 */

/**
 * @ingroup     drivers_mcp9601
 * @{
 *
 * @file
 * @brief       Driver for Thermocouple EMF to Temperature Converter MCP9601
 *
 * @author      Alexandre Moguilevski <alexandre.moguilevski@ml-pa.com>
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include "board.h"
#include "byteorder.h"
#include "mcp9601.h"
#include "mcp9601_params.h"
#include "mcp9601_regs.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#define I2C_BUS     (dev->params->i2c_bus)
#define I2C_ADDR    (dev->params->i2c_addr)

static int _write_reg_checked(mcp9601_t *dev, uint16_t reg, uint8_t byte)
{
    int res = 0;
    uint8_t reply;

    if ((res = i2c_write_reg(I2C_BUS, I2C_ADDR, reg, byte, 0))) {
        return res;
    }

    i2c_read_byte(I2C_BUS, I2C_ADDR, &reply, 0);
    if (reply != byte) {
        res = -EFAULT;
    }
    return res;
}

int mcp9601_init(mcp9601_t *dev, const mcp9601_params_t *params)
{
    int res = 0;
    uint16_t device_id;

    dev->params = params;
    i2c_acquire(I2C_BUS);

    /* check device ID */
    if ((res = i2c_write_byte(I2C_BUS, I2C_ADDR, MCP9601_REGS_DEVICE_ID, 0))) {
        goto out;
    }
    i2c_read_bytes(I2C_BUS, I2C_ADDR, &device_id, sizeof(device_id), 0);

    if (device_id != htons(MCP9601_PARAMS_DEVICE_ID)) {
        DEBUG("mcp9601: invalid device id: %x\n", device_id);
        res = -EFAULT;
        goto out;
    }

    /* prepare sensor config byte */
    uint8_t sensor_config = params->tc_type << 4
                          | params->filter;

    if ((res = _write_reg_checked(dev, MCP9601_REGS_SENSOR_CONFIG, sensor_config))) {
        goto out;
    }

    /* prepare device config byte */
    uint8_t device_config =  params->cj_resolution  << 7
                          |  params->adc_resolution << 4
                          |  params->burst_mode     << 1
                          |  params->shutdown_mode;
    /* write device config byte */
    res = _write_reg_checked(dev, MCP9601_REGS_DEVICE_CONFIG, device_config);

out:
    i2c_release(I2C_BUS);
    return res;
}

int16_t mcp9601_get_temperature(const mcp9601_t *dev, uint8_t temp_reg_addr)
{
    uint8_t temp_bytes[2];

    i2c_acquire(I2C_BUS);
    i2c_write_byte(I2C_BUS, I2C_ADDR, temp_reg_addr, 0);
    i2c_read_bytes(I2C_BUS, I2C_ADDR, temp_bytes, sizeof(temp_bytes), 0);
    i2c_release(I2C_BUS);

    uint16_t upper = temp_bytes[0];
    uint16_t lower = temp_bytes[1];

    if (upper & 0x80) {
        /* Temperature < 0°C */
        return (upper * 100 * 16) + (lower * 100) / 16 - (4096 * 100);
    } else {
        /* Temperature >= 0°C */
        return (upper * 100 * 16) + (lower * 100) / 16;
    }
}
