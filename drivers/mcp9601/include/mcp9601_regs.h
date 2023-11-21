/*
 * Copyright (C) ML!PA Consulting GmbH
 */

/**
 * @file        mcp9601_regs.h
 * @ingroup     drivers_mcp9601
 * @{
 *
 * @brief       Driver for Thermocouple EMF to Temperature Converter
 *              MCP9601
 *
 * @author      Alexandre Moguilevski <alexandre.moguilevski@ml-pa.com>
 */

#ifndef MCP9601_REGS_H
#define MCP9601_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
* @name   Register Pointer Bits
* @{
*/
#define MCP9601_REGS_TH            (0x00) // Thermocouple Hot-Junction register
#define MCP9601_REGS_TD            (0x01) // Junctions Temperature Delta register
#define MCP9601_REGS_TC            (0x02) // Cold-Junction Temperature register
#define MCP9601_REGS_ADC           (0x03) // Raw ADC Data register
#define MCP9601_REGS_STATUS        (0x04) // Status register
#define MCP9601_REGS_SENSOR_CONFIG (0x05) // Thermocouple Sensor Configuration register
#define MCP9601_REGS_DEVICE_CONFIG (0x06) // Device Configuration register
#define MCP9601_REGS_ALERT1_CONFIG (0x08) // Alert 1 Configuration register
#define MCP9601_REGS_ALERT2_CONFIG (0x09) // Alert 2 Configuration register
#define MCP9601_REGS_ALERT3_CONFIG (0x0A) // Alert 3 Configuration register
#define MCP9601_REGS_ALERT4_CONFIG (0x0B) // Alert 4 Configuration register
#define MCP9601_REGS_THYST1        (0x0C) // Alert 1 Hysteresis register
#define MCP9601_REGS_THYST2        (0x0D) // Alert 2 Hysteresis register
#define MCP9601_REGS_THYST3        (0x0E) // Alert 3 Hysteresis register
#define MCP9601_REGS_THYST4        (0x0F) // Alert 4 Hysteresis register
#define MCP9601_REGS_TALERT1_LIM   (0x10) // Temperature Alert 1 Limit register
#define MCP9601_REGS_TALERT2_LIM   (0x11) // Temperature Alert 2 Limit register
#define MCP9601_REGS_TALERT3_LIM   (0x12) // Temperature Alert 3 Limit register
#define MCP9601_REGS_TALERT4_LIM   (0x13) // Temperature Alert 4 Limit register
#define MCP9601_REGS_DEVICE_ID     (0x20) // Device ID/Revision register
/** @} */

/**
 * @name Thermocouple Type Select bits
 * @{
 */
enum {
    MCP9601_REGS_TC_TYPE_K   = 0,
    MCP9601_REGS_TC_TYPE_J,
    MCP9601_REGS_TC_TYPE_T,
    MCP9601_REGS_TC_TYPE_N,
    MCP9601_REGS_TC_TYPE_S,
    MCP9601_REGS_TC_TYPE_E,
    MCP9601_REGS_TC_TYPE_B,
    MCP9601_REGS_TC_TYPE_R
};
/** @} */

/**
 * @name Filter Coefficient bits
 * @{
 */
enum {
    MCP9601_REGS_FILTER_OFF   = 0,
    MCP9601_REGS_FILTER_1,
    MCP9601_REGS_FILTER_2,
    MCP9601_REGS_FILTER_3,
    MCP9601_REGS_FILTER_4,
    MCP9601_REGS_FILTER_5,
    MCP9601_REGS_FILTER_6,
    MCP9601_REGS_FILTER_7
};
/** @} */

/**
 * @name Cold-Junction Resolution bit
 * @{
 */
enum {
    MCP9601_REGS_COLD_JUNCTION_RES_HIGH   = 0, // 0.0625°C
    MCP9601_REGS_COLD_JUNCTION_RES_LOW         // 0.25°C
};
/** @} */

/**
 * @name ADC Measurement Resolution bits
 * @{
 */
enum {
    MCP9601_REGS_ADC_RES_18_BITS   = 0,
    MCP9601_REGS_ADC_RES_16_BITS,
    MCP9601_REGS_ADC_RES_14_BITS,
    MCP9601_REGS_ADC_RES_12_BITS
};
/** @} */

/**
 * @name Burst Mode Temperature Samples bits: number of samples
 * @{
 */
enum {
    MCP9601_REGS_BURST_1   = 0,
    MCP9601_REGS_BURST_2,
    MCP9601_REGS_BURST_4,
    MCP9601_REGS_BURST_8,
    MCP9601_REGS_BURST_16,
    MCP9601_REGS_BURST_32,
    MCP9601_REGS_BURST_64,
    MCP9601_REGS_BURST_128
};
/** @} */

/**
 * @name Shutdown Mode bits
 * @{
 */
enum {
    MCP9601_REGS_MODE_NORMAL   = 0,
    MCP9601_REGS_MODE_SHUTDOWN,
    MCP9601_REGS_MODE_BURST,
};
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MCP9601_REGS_H */
/** @} */
