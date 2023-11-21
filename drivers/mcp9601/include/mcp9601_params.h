/*
 * Copyright (C) ML!PA Consulting GmbH
 */

/**
 * @file        mcp9601_params.h
 * @ingroup     drivers_mcp9601
 * @{
 *
 * @brief       Driver parameters for Thermocouple EMF to Temperature Converter
 *              MCP9601
 *
 * @author      Alexandre Moguilevski <alexandre.moguilevski@ml-pa.com>
 */

#ifndef MCP9601_PARAMS_H
#define MCP9601_PARAMS_H

#include "board.h"
#include "mcp9601_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MCP9601_PARAMS_I2C
#define MCP9601_PARAMS_I2C    I2C_DEV(0)
#endif

/** 7-bit I2C address: 1-1-0-0-A2-A1-A0, where
   the last three bits A2, A1, A0 are defined
   by the voltage level on the ADDR pin */
#ifndef MCP9601_PARAMS_I2C_ADDR
#define MCP9601_PARAMS_I2C_ADDR   (0x67)
#endif

/** Content of the Device ID and Revision ID register
   [8 bits: Device ID][8 bits: Revision ID] */
#ifndef MCP9601_PARAMS_DEVICE_ID
#define MCP9601_PARAMS_DEVICE_ID  (0x4110)
#endif

/**
 * @name Sensor configuration
 * @{
 */
/* Select thermocouple type */
#ifndef MCP9601_PARAMS_TC_TYPE
#define MCP9601_PARAMS_TC_TYPE           (MCP9601_REGS_TC_TYPE_K)
#endif
/* Select filter coefficient */
#ifndef MCP9601_PARAMS_FILTER
#define MCP9601_PARAMS_FILTER            (MCP9601_REGS_FILTER_OFF)
#endif
/** @} */

/**
 * @name Device configuration
 * @{
 */
/* Select cold-junction resolution */
#ifndef MCP9601_PARAMS_COLD_JUNCTION_RES
#define MCP9601_PARAMS_COLD_JUNCTION_RES  (MCP9601_REGS_COLD_JUNCTION_RES_LOW)
#endif
/* Select ADC measurement resolution */
#ifndef MCP9601_PARAMS_ADC_RES
#define MCP9601_PARAMS_ADC_RES            (MCP9601_REGS_ADC_RES_18_BITS)
#endif
/* Select number of samples for the burst mode */
#ifndef MCP9601_PARAMS_BURST_SAMPLES
#define MCP9601_PARAMS_BURST_SAMPLES      (MCP9601_REGS_BURST_1)
#endif
/* Select shutdown mode */
#ifndef MCP9601_PARAMS_SHUTDOWN_MODE
#define MCP9601_PARAMS_SHUTDOWN_MODE      (MCP9601_REGS_MODE_NORMAL)
#endif
/***/
#ifndef MCP9601_PARAMS
#define MCP9601_PARAMS       { .i2c_bus        = MCP9601_PARAMS_I2C, \
                               .i2c_addr       = MCP9601_PARAMS_I2C_ADDR, \
                               .tc_type        = MCP9601_PARAMS_TC_TYPE, \
                               .filter         = MCP9601_PARAMS_FILTER, \
                               .cj_resolution  = MCP9601_PARAMS_COLD_JUNCTION_RES, \
                               .adc_resolution = MCP9601_PARAMS_ADC_RES, \
                               .burst_mode     = MCP9601_PARAMS_BURST_SAMPLES, \
                               .shutdown_mode  = MCP9601_PARAMS_SHUTDOWN_MODE }
#endif
/** @} */

/**
 * @brief   MCP9601 initial configuration
 * @{
 */
static const mcp9601_params_t mcp9601_params[] =
{
   MCP9601_PARAMS,
};
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MCP9601_PARAMS_H */
/** @} */
