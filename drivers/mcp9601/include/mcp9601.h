/*
 * Copyright (C) ML!PA Consulting GmbH
 */

/**
 * @file        mcp9601.h
 * @defgroup    drivers_mcp9601 MCP9691 Driver
 * @ingroup     drivers_sensors
 * @{
 *
 * @brief       Driver for Thermocouple EMF to Temperature Converter
 *              MCP9601
 *
 * @author      Alexandre Moguilevski <alexandre.moguilevski@ml-pa.com>
 */

#ifndef MCP9601_H
#define MCP9601_H

#include "periph/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name MCP9601 configuration struct type
 * @{
 */
typedef struct {
    i2c_t   i2c_bus;
    uint8_t i2c_addr;
    uint8_t tc_type;
    uint8_t filter;
    uint8_t cj_resolution;
    uint8_t adc_resolution;
    uint8_t burst_mode;
    uint8_t shutdown_mode;
} mcp9601_params_t;
/** @} */

/**
 * @name MCP9601 device decriptor
 * @{
 */
typedef struct {
    const mcp9601_params_t *params;
} mcp9601_t;
/** @} */

/**
 * @brief Write configuration parameters to the configuration
 *        registers of MCP9601
 *
 * @param[in] config                MCP9601 configuration struct
 *
 * @return    MCP9601_ERROR_I2C     I2C data transfer error
 *            MCP9601_ERROR         configuration failed
 *            MCP9601_SUCCESS       configuration successful
 */
int mcp9601_init(mcp9601_t *dev, const mcp9601_params_t *init_params);

/**
 * @brief Read the device and revision id of MCP9601 and
 *        compares the result with the expected values
 *        given in the data sheet
 *
 * @param[in] dev                   MCP9601 device descriptor
 * @param[in] init_params           initial configuration of the device
 *
 * @return    MCP9601_ERROR_I2C     I2C data transfer error
 *            MCP9601_ERROR         comparison failed
 *            MCP9601_SUCCESS       comparison successful
 */
int mcp9601_check_device_id(mcp9601_t *dev);

/**
 * @brief Read temperature value from the hot-junction, cold-junction
 *        or junction delta temperature registers
 *
 * @param[in] dev                   MCP9601 device descriptor
 * @param[in] temp_reg_addr         temperature register address,
 *                                  following register addresses can be used:
 *                                      MCP9601_REGS_TH
 *                                      MCP9601_REGS_TD
 *                                      MCP9601_REGS_TC
 *
 * @return    temperature (* 16 degrees C)
 */
int16_t mcp9601_get_temperature(const mcp9601_t *dev, uint8_t temp_reg_addr);

#ifdef __cplusplus
}
#endif

#endif /* MCP9601_H */
/** @} */
