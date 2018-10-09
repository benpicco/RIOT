/*
 * Copyright (C) 2018 Gunar Schorcht
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_itg320x ITG320X 3-axis gyroscope
 * @ingroup     drivers_sensors
 * @ingroup     drivers_saul
 * @brief       Device driver for InvenSense ITG320X 3-axis gyroscope
 *
 * The driver can be used with InvenSense ITG3200, ITG3205, and MPU3050.
 * The latter one can be used only with reduced feature set.
 *
 * The driver implements polling mode as well as interrupt mode. Thus,
 * the application may use two different approaches to retrieve new data,
 * either
 *
 * - periodically fetching the data at a rate lower than the sensor's
 *   output data rate (ODR), or
 * - fetching the data when the data-ready interrupt is triggered.
 *
 * For the latter approach, the according GPIO to which the sensor's
 * **INT** output pin is connected has to be configured and initialized
 * by the application.
 *
 * This driver provides @ref drivers_saul capabilities.
 *
 * @{
 *
 * @author      Gunar Schorcht <gunar@schorcht.net>
 * @file
 */

#ifndef ITG320X_H
#define ITG320X_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

#include "periph/gpio.h"
#include "periph/i2c.h"

/**
 * @name ITG320X I2C addresses
 */
#define ITG320X_I2C_ADDRESS_1    (0x68)     /**< AD0 pin low */
#define ITG320X_I2C_ADDRESS_2    (0x69)     /**< AD0 pin high */

/** ITG320X chip id defined in Who Am I */
#define ITG320X_ID               (0x68)

/** Definition of error codes */
typedef enum {
    ITG320X_OK,                   /**< success */
    ITG320X_ERROR_I2C,            /**< I2C communication error */
    ITG320X_ERROR_WRONG_ID,       /**< wrong id read */
    ITG320X_ERROR_NO_DATA,        /**< no data are available */
    ITG320X_ERROR_RAW_DATA,       /**< reading raw data failed */
} itg320x_error_codes_t;

/**
 * @brief   Low pass filter bandwith
 *
 * @note Low pass filter bandwidth determines the internal sample rate (ISR).
 * The internal sample rate (ISR) together with sample rate devider
 * (ISR_DIV) determines the output data rate (ODR) as following:
 *
 * ODR = ISR / (ISR_DIV + 1)
 *
 * where internal sample rate (ISR) is 8 kHz for #ITG320X_LPF_BW_256,
 * or 1 kHz otherwise.
 */
typedef enum {
    ITG320X_LPF_BW_256 = 0, /**< 256 Hz, ISR = 8 kHz */
    ITG320X_LPF_BW_188,     /**< 188 Hz, ISR = 1 kHz */
    ITG320X_LPF_BW_98,      /**<  98 Hz, ISR = 1 kHz */
    ITG320X_LPF_BW_42,      /**<  42 Hz, ISR = 1 kHz */
    ITG320X_LPF_BW_20,      /**<  20 Hz, ISR = 1 kHz */
    ITG320X_LPF_BW_10,      /**<  10 Hz, ISR = 1 kHz */
    ITG320X_LPF_BW_5,       /**<   5 Hz, ISR = 1 kHz (default) */
} itg320x_lpf_bw_t;

/**
 * @brief   Logic level for INT output pin
 */
typedef enum {
    ITG320X_INT_HIGH = 0,   /**< INT output is active high (default) */
    ITG320X_INT_LOW,        /**< INT output is active low */
} itg320x_int_level_t;

/**
 * @brief   Drive type for INT output pin
 */
typedef enum {
    ITG320X_INT_PUSH_PULL = 0,  /**< INT output is of type push/pull (default) */
    ITG320X_INT_OPEN_DRAIN,     /**< INT output is of type open drain */
} itg320x_int_drive_t;

/**
 * @brief   Angular rate values in micro-Gauss (uGs)
 */
typedef struct {

    int32_t x;  /**< angular rate x-axis (roll) */
    int32_t y;  /**< angular rate y-axis (pitch) */
    int32_t z;  /**< angular rate y-axis (yaw) */

} itg320x_data_t;

/**
 * @brief   Raw data set as two complements
 */
typedef struct {

    int16_t x; /**< angular rate x-axis as 16 bit two's complements (roll) */
    int16_t y; /**< angular rate y-axis as 16 bit two's complements (pitch) */
    int16_t z; /**< angular rate z-axis as 16 bit two's complements (yaw) */

} itg320x_raw_data_t;

/**
 * @brief   ITG320X device initialization parameters
 */
typedef struct {

    unsigned  dev;   /**< I2C device (default I2C_DEV(0)) */
    uint8_t   addr;  /**< I2C slave address (default #ITG320X_I2C_ADDRESS_1) */

    gpio_t  int_pin; /**< DRDY interrupt pin: #GPIO_UNDEF if not used */
    uint8_t isr_div; /**< Internal sample rate divider ISR_DIV (default 9)
                          ODR = ISR / (ISR_DIV + 1) */

    itg320x_lpf_bw_t     lpf_bw; /**< Low pass filter bandwith
                                      (default #ITG320X_LPF_BW_5, ISR 1 kHz) */

    itg320x_int_level_t  int_level; /**< Logic level for INT output pin
                                         (default #ITG320X_INT_LOW) */
    itg320x_int_drive_t  int_drive; /**< Drive type for INT output pin
                                         (default #ITG320X_INT_PUSH_PULL */
} itg320x_params_t;

/**
 * @brief   ITG320X sensor device data structure type
 */
typedef struct {

    itg320x_params_t params;    /**< device initialization parameters */

} itg320x_t;

/**
 * @brief	Initialize the ITG320X sensor device
 *
 * This function resets the sensor and initializes the sensor according to
 * given intialization parameters. All registers are reset to default values.
 *
 * @param[in]   dev     device descriptor of ITG320X sensor to be initialized
 * @param[in]   params  ITG320X initialization parameters
 *
 * @retval  ITG320X_OK      on success
 * @retval  ITG320X_ERROR_* a negative error code on error,
 *                          see #itg320x_error_codes_t
 */
int itg320x_init(itg320x_t *dev, const itg320x_params_t *params);

/**
 * @brief   Read one sample of angular rates in milli-degrees per second (mdps)
 *
 * Raw magnetometer data are read from the sensor and normalized with
 * respect to full scale +-2000 dps. Angular rate values are given in
 * milli-degrees per second (mdps) to preserve full resolution:
 *
 * @param[in]   dev     device descriptor of ITG320X sensor
 * @param[out]  data    result vector in mdps per axis
 *
 * @retval  ITG320X_OK      on success
 * @retval  ITG320X_ERROR_* a negative error code on error,
 *                          see #itg320x_error_codes_t
 */
int itg320x_read(const itg320x_t *dev, itg320x_data_t *data);

/**
 * @brief   Read one sample of raw sensor data as 16 bit two's complements
 *
 * @param[in]   dev     device descriptor of ITG320X sensor
 * @param       raw     raw data vector
 *
 * @retval  ITG320X_OK      on success
 * @retval  ITG320X_ERROR_* a negative error code on error,
 *                          see #itg320x_error_codes_t
 */
int itg320x_read_raw(const itg320x_t *dev, itg320x_raw_data_t *raw);

/**
 * @brief   Read temperature in centi-degree Celsius
 *
 * @param[in]   dev     device descriptor of ITG320X sensor
 * @param[out]  temp    temperature in centi-degree Celsius
 *
 * @retval  ITG320X_OK      on success
 * @retval  ITG320X_ERROR_* a negative error code on error,
 *                          see #itg320x_error_codes_t
 */
int itg320x_read_temp(const itg320x_t *dev, int16_t* temp);

/**
 * @brief   Power down the sensor
 *
 * Changes the sensor operation mode to sleep mode in which  almost all
 * including the gyros are switched off.
 *
 * @param[in]   dev     Device descriptor of ITG320X device to read from
 *
 * @retval  ITG320X_OK      on success
 * @retval  ITG320X_ERROR_* a negative error code on error,
 *                          see #itg320x_error_codes_t
 */
int itg320x_power_down (itg320x_t *dev);

/**
 * @brief   Power up the sensor
 *
 * Swichtes the sensor back into active operation mode.  It takes
 * up to 20 ms since the gyros have to be switched on again.
 *
 * @param[in]   dev     Device descriptor of ITG320X device to read from
 *
 * @retval  ITG320X_OK      on success
 * @retval  ITG320X_ERROR_* a negative error code on error,
 *                          see #itg320x_error_codes_t
 */
int itg320x_power_up (itg320x_t *dev);

/**
 * @brief   Enable or disable data ready interrupt (DRDY)
 *
 * @param[in]   dev     device descriptor of ITG320X sensor
 * @param[in]   enable  enable the interrupt if true, otherwise disable it
 *
 * @retval  ITG320X_OK                on success
 * @retval  ITG320X_ERROR_NO_INT_PIN  no new data available
 * @retval  ITG320X_ERROR_*           negative error code on error,
 *                                    see #itg320x_error_codes_t
 */
int itg320x_enable_int(const itg320x_t *dev, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* ITG320X_H */
/** @} */
