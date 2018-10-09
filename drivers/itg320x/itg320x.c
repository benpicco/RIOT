/*
 * Copyright (C) 2018 Gunar Schorcht
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_itg320x
 * @brief       Device driver for the InvenSense ITG320X 3-axis gyroscope
 * @author      Gunar Schorcht <gunar@schorcht.net>
 * @file
 * @{
 */

#include <string.h>
#include <stdlib.h>

#include "itg320x_regs.h"
#include "itg320x.h"

#include "log.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if ENABLE_DEBUG

#define ASSERT_PARAM(cond) \
    if (!(cond)) { \
        DEBUG("[itg320x] %s: %s\n", \
              __func__, "parameter condition (" # cond ") not fulfilled"); \
        assert(cond); \
    }

#define DEBUG_DEV(f, d, ...) \
        DEBUG("[itg320x] %s i2c dev=%d addr=%02x: " f "\n", \
              __func__, d->params.dev, d->params.addr, ## __VA_ARGS__);

#else /* ENABLE_DEBUG */

#define ASSERT_PARAM(cond) assert(cond);
#define DEBUG_DEV(f, d, ...)

#endif /* ENABLE_DEBUG */

#define ERROR_DEV(f, d, ...) \
        LOG_ERROR("[itg320x] %s i2c dev=%d addr=%02x: " f "\n", \
                  __func__, d->params.dev, d->params.addr, ## __VA_ARGS__);

#define EXEC_RET(f) { \
    int _r; \
    if ((_r = f) != ITG320X_OK) { \
        DEBUG("[itg320x] %s: error code %d\n", __func__, _r); \
        return _r; \
    } \
}

#define EXEC_RET_CODE(f, c) { \
    int _r; \
    if ((_r = f) != ITG320X_OK) { \
        DEBUG("[itg320x] %s: error code %d\n", __func__, _r); \
        return c; \
    } \
}

/** Forward declaration of functions for internal use */

static int _is_available(const itg320x_t *dev);
static int _reset(itg320x_t *dev);

// static uint8_t _get_reg_bit(uint8_t byte, uint8_t mask);
static void _set_reg_bit(uint8_t *byte, uint8_t mask, uint8_t bit);
static int _reg_read(const itg320x_t *dev, uint8_t reg, uint8_t *data, uint16_t len);
static int _reg_write(const itg320x_t *dev, uint8_t reg, uint8_t *data, uint16_t len);
static int _update_reg(const itg320x_t *dev, uint8_t reg, uint8_t mask, uint8_t val);

int itg320x_init(itg320x_t *dev, const itg320x_params_t *params)
{
    ASSERT_PARAM(dev != NULL);
    ASSERT_PARAM(params != NULL);
    DEBUG_DEV("params=%p", dev, params);

    /* init sensor data structure */
    dev->params = *params;

    /* check availability of the sensor */
    EXEC_RET(_is_available(dev))

    /* reset the sensor */
    EXEC_RET(_reset(dev));

    /* set internal sample rate divider (ISR) from parameters */
    uint8_t reg = params->isr_div;
    EXEC_RET(_reg_write(dev, ITG320X_REG_SMPLRT_DIV, &reg, 1));

    /* set full scale always to +-2000 and LPF bandwidth from parameters */
    reg = 0;
    _set_reg_bit(&reg, ITG320X_REG_DLPFS_FS_SEL, 3);
    _set_reg_bit(&reg, ITG320X_REG_DLPFS_DLPF_CFG, params->lpf_bw);
    EXEC_RET(_reg_write(dev, ITG320X_REG_DLPFS, &reg, 1));

    /*
     * set interrupt configuration register
     * - Logic level and drive type used from parameters
     * - Latching interrupt always enabled
     * - Latch clear method is reading status register
     * - ITG ready and RAW data ready interrupt are disabled
     */
    reg = 0;
    _set_reg_bit(&reg, ITG320X_REG_INT_CFG_ACTL, params->int_level);
    _set_reg_bit(&reg, ITG320X_REG_INT_CFG_OPEN, params->int_drive);
    _set_reg_bit(&reg, ITG320X_REG_INT_CFG_LATCH_INT, 1);
    EXEC_RET(_reg_write(dev, ITG320X_REG_INT_CFG, &reg, 1));

    return ITG320X_OK;
}

/* scale factors for conversion of raw sensor data to units */
static const double _gyro_scale = 1.0/14.375; /* degrees per second / LSb */
static const float  _temp_scale = 1.0/280.0; /* degree Celsius / LSb */

int itg320x_read(const itg320x_t *dev, itg320x_data_t *data)
{
    ASSERT_PARAM(dev != NULL);
    ASSERT_PARAM(data != NULL);
    DEBUG_DEV("data=%p", dev, data);

    itg320x_raw_data_t raw;

    EXEC_RET(itg320x_read_raw (dev, &raw));

    data->x = ((double)raw.x * _gyro_scale) * 1000;
    data->y = ((double)raw.y * _gyro_scale) * 1000;
    data->z = ((double)raw.z * _gyro_scale) * 1000;

    return ITG320X_OK;
}

int itg320x_read_raw(const itg320x_t *dev, itg320x_raw_data_t *raw)
{
    ASSERT_PARAM(dev != NULL);
    ASSERT_PARAM(raw != NULL);
    DEBUG_DEV("raw=%p", dev, raw);

    uint8_t data[6];

    /* read raw data sample */
    EXEC_RET_CODE(_reg_read(dev, ITG320X_REG_GYRO_XOUT_H, data, 6),
                  -ITG320X_ERROR_RAW_DATA);

    /* data MSB @ lower address */
    raw->x = (data[0] << 8) | data[1];
    raw->y = (data[2] << 8) | data[3];
    raw->z = (data[4] << 8) | data[5];

    /* read status register to clear interrupt */
    EXEC_RET(_reg_read(dev, ITG320X_REG_INT_STATUS, data, 1));

    return ITG320X_OK;
}

int itg320x_read_temp(const itg320x_t *dev, int16_t *temp)
{
    ASSERT_PARAM(dev != NULL);
    ASSERT_PARAM(temp != NULL);
    DEBUG_DEV("raw=%p", dev, temp);

    uint8_t data[2];

    /* read raw data sample */
    EXEC_RET_CODE(_reg_read(dev, ITG320X_REG_TEMP_OUT_H, data, 2),
                  -ITG320X_ERROR_RAW_DATA);

    /* data MSB @ lower address */
    *temp = (data[0] << 8) | data[1];
    /* convert raw templ data to centi-degree */
    *temp = ((double)(-13200 - *temp) * _temp_scale + 35) * 10;

    return ITG320X_OK;
}

int itg320x_power_down (itg320x_t *dev)
{
    ASSERT_PARAM(dev != NULL);
    DEBUG_DEV("", dev);

    return _update_reg(dev, ITG320X_REG_PWR_MGM, ITG320X_REG_PWR_MGM_SLEEP, 1);
}

int itg320x_power_up (itg320x_t *dev)
{
    ASSERT_PARAM(dev != NULL);
    DEBUG_DEV("", dev);

    EXEC_RET(_update_reg(dev, ITG320X_REG_PWR_MGM, ITG320X_REG_PWR_MGM_SLEEP, 0));

    /* wait 20 ms after power-up */
    xtimer_usleep(20 * US_PER_MS);

    return ITG320X_OK;
}

int itg320x_enable_int(const itg320x_t *dev, bool enable)
{
    ASSERT_PARAM(dev != NULL);
    DEBUG_DEV("", dev);

    return _update_reg(dev, ITG320X_REG_INT_CFG,
                            ITG320X_REG_INT_CFG_RAW_RDY_EN, enable);
}

/** Functions for internal use only */

static int _reset(itg320x_t *dev)
{
    DEBUG_DEV("", dev);

    /* set the reset flag, it automatically reset by the device */
    EXEC_RET(_update_reg(dev, ITG320X_REG_PWR_MGM, ITG320X_REG_PWR_MGM_H_RESET, 1));

    /* wait 20 ms after reset */
    xtimer_usleep(20 * US_PER_MS);

    return ITG320X_OK;
}

/**
 * @brief   Check the chip ID to test whether sensor is available
 */
static int _is_available(const itg320x_t *dev)
{
    DEBUG_DEV("", dev);

    uint8_t reg;

    /* read the chip id from ITG320X_REG_ID_X */
    EXEC_RET(_reg_read(dev, ITG320X_REG_WHO_AM_I, &reg,1));

    if (reg != ITG320X_ID) {
        DEBUG_DEV("sensor is not available, wrong id %02x, should be %02x",
                  dev, reg, ITG320X_ID);
        return -ITG320X_ERROR_WRONG_ID;
    }

    return ITG320X_OK;
}

static void _set_reg_bit(uint8_t *byte, uint8_t mask, uint8_t bit)
{
    ASSERT_PARAM(byte != NULL);

    uint8_t shift = 0;
    while (!((mask >> shift) & 0x01)) {
        shift++;
    }
    *byte = ((*byte & ~mask) | ((bit << shift) & mask));
}

static int _update_reg(const itg320x_t *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    DEBUG_DEV("reg=%02x mask=%02x val=%02x", dev, reg, mask, val);

    uint8_t reg_val;
    uint8_t shift = 0;

    while (!((mask >> shift) & 0x01)) {
        shift++;
    }

    /* read current register value */
    EXEC_RET(_reg_read(dev, reg, &reg_val, 1));

    /* set masked bits to the given value  */
    reg_val = (reg_val & ~mask) | ((val << shift) & mask);

    /* write back new register value */
    EXEC_RET(_reg_write(dev, reg, &reg_val, 1));

    return ITG320X_OK;
}

static int _reg_read(const itg320x_t *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    ASSERT_PARAM(dev != NULL);
    ASSERT_PARAM(data != NULL);
    ASSERT_PARAM(len != 0);

    DEBUG_DEV("read %d byte from sensor registers starting at addr 0x%02x",
              dev, len, reg);

    if (i2c_acquire(dev->params.dev)) {
        DEBUG_DEV("could not aquire I2C bus", dev);
        return -ITG320X_ERROR_I2C;
    }

    int res = i2c_read_regs(dev->params.dev, dev->params.addr, reg, data, len, 0);
    i2c_release(dev->params.dev);

    if (res == ITG320X_OK) {
        if (ENABLE_DEBUG) {
            printf("[itg320x] %s i2c dev=%d addr=%02x: read following bytes: ",
                   __func__, dev->params.dev, dev->params.addr);
            for (int i = 0; i < len; i++) {
                printf("%02x ", data[i]);
            }
            printf("\n");
        }
    }
    else {
        DEBUG_DEV("could not read %d bytes from sensor registers "
                  "starting at addr %02x, reason %d (%s)",
                  dev, len, reg, res, strerror(res * -1));
        return -ITG320X_ERROR_I2C;
    }

    return res;
}

static int _reg_write(const itg320x_t *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    ASSERT_PARAM(dev != NULL);
    ASSERT_PARAM(data != NULL);
    ASSERT_PARAM(len != 0);

    DEBUG_DEV("write %d bytes to sensor registers starting at addr 0x%02x",
              dev, len, reg);

    if (ENABLE_DEBUG) {
        printf("[itg320x] %s i2c dev=%d addr=%02x: write following bytes: ",
               __func__, dev->params.dev, dev->params.addr);
        for (int i = 0; i < len; i++) {
            printf("%02x ", data[i]);
        }
        printf("\n");
    }

    if (i2c_acquire(dev->params.dev)) {
        DEBUG_DEV("could not aquire I2C bus", dev);
        return -ITG320X_ERROR_I2C;
    }

    int res;

    if (!data || !len) {
        res = i2c_write_byte(dev->params.dev, dev->params.addr, reg, 0);
    }
    else {
        res = i2c_write_regs(dev->params.dev, dev->params.addr, reg, data, len, 0);
    }
    i2c_release(dev->params.dev);

    if (res != ITG320X_OK) {
        DEBUG_DEV("could not write %d bytes to sensor registers "
                  "starting at addr 0x%02x, reason %d (%s)",
                  dev, len, reg, res, strerror(res * -1));
        return -ITG320X_ERROR_I2C;
    }

    return res;
}
