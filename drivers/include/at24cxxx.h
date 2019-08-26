/*
 * Copyright (C) 2019 Otto-von-Guericke-Universität Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_at24cxxx AT24CXXX EEPROM unit
 * @brief       Device driver interface for the AT24CXXX EEPROM units
 *
 * This implementation works with AT24C128 and AT24C256.
 *
 * @{
 *
 * @file
 * @brief       Device driver interface for AT24CXXX EEPROM units.
 *
 * @author      Fabian Hüßler <fabian.huessler@ovgu.de>
 *
 */

#ifndef AT24CXXX_H
#define AT24CXXX_H

#include <stdint.h>

#include "periph/gpio.h"
#include "periph/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   AT24C128 16kB memory
 */
#define AT24C128_EEPROM_SIZE      (16384U)

/**
 * @brief   AT24C256 32kB memory
 */
#define AT24C256_EEPROM_SIZE      (32768U)

#if defined (MODULE_AT24C256) || defined (MODULE_AT24C256C)
 /**
  * @brief  EEPROM memory capacity in bytes
  */
#define AT24CXXX_EEPROM_SIZE     (AT24C256_EEPROM_SIZE)
#elif defined (MODULE_AT24C128)
/**
 * @brief  EEPROM memory capacity in bytes
 */
#define AT24CXXX_EEPROM_SIZE     (AT24C128_EEPROM_SIZE)
#else
#pragma message( \
    "[at24cxxx] Module name was neither at24c256 nor at24c256c nor at24c128")
#pragma message( \
    "[at24cxxx] Using AT24C128_EEPROM_SIZE as a default parameter")

/**
 * @brief  EEPROM memory capacity in bytes
 */
#define AT24CXXX_EEPROM_SIZE     (AT24C128_EEPROM_SIZE)
#endif

/**
 * @brief   Return values
 */
enum {
    AT24CXXX_OK,
    AT24CXXX_I2C_ERROR
};

/**
 * @brief   7 bit I2C device address: 10100 A1 A0
 */
enum {
    AT24CXXX_DEV_ADDR_00    = (0x50),   /**< A1 = 0, A0 = 0 */
    AT24CXXX_DEV_ADDR_01    = (0x51),   /**< A1 = 0, A0 = 1 */
    AT24CXXX_DEV_ADDR_10    = (0x52),   /**< A1 = 1, A0 = 0 */
    AT24CXXX_DEV_ADDR_11    = (0x53)    /**< A1 = 1, A0 = 1 */
};

/**
 * @brief   Struct that holds initialization parameters
 */
typedef struct at24cxxx_params {
    i2c_t i2c;                      /**< I2C bus number */
    uint8_t dev_addr;               /**< I2C device address */
    gpio_t pin_wp;                  /**< write protect pin */
    size_t eeprom_size;             /**< EEPROM memory capacity */
} at24cxxx_params_t;

/**
 * @brief   Struct that represents an AT24CXXX device
 */
typedef struct {
    at24cxxx_params_t params;     /**< parameters */
} at24cxxx_t;

/**
 * @brief   Initialize an AT24CXXX device handle with AT24CXXX parameters
 *
 * @param[in, out]  dev     AT24CXXX device handle
 * @param[in]       params  AT24CXXX parameters
 *
 * @return          AT24CXXX_OK on success
 * @return          -AT24CXXX_I2C_ERROR if i2c could not be acquired
 * @return          -EINVAL if input parameters are NULL
 */
int at24cxxx_init(at24cxxx_t *dev, const at24cxxx_params_t *params);

/**
 * @brief   Read a byte at a given position @p pos
 *
 * @param[in]       dev      AT24CXXX device handle
 * @param[in]       pos      Position in EEPROM memory
 *
 * @return          Read byte on success
 * @return          -ERANGE if @p pos is out of bounds
 * @return          @see i2c_read_regs
 */
int32_t at24cxxx_read_byte(const at24cxxx_t *dev, uint16_t pos);

/**
 * @brief Sequentially read @p len bytes from a given position @p pos
 *
 * @param[in]       dev     AT24CXXX device handle
 * @param[in]       pos     Position in EEPROM memory
 * @param[out]      data    Read buffer
 * @param[in]       len     Requested length to be read
 *
 * @return          @p len on success
 * @return          -ERANGE if @p pos + @p len is out of bounds
 * @return          @see i2c_read_regs
 */
int32_t at24cxxx_read(const at24cxxx_t *dev, uint16_t pos, void *data,
                      size_t len);

/**
 * @brief   Write a byte at a given position @p pos
 *
 * @param[in]       dev      AT24CXXX device handle
 * @param[in]       pos      Position in EEPROM memory
 * @param[in]       data     Value to be written
 *
 * @return          1 on success
 * @return          -ERANGE if @p pos is out of bounds
 * @return          @see i2c_write_regs
 */
int32_t at24cxxx_write_byte(const at24cxxx_t *dev, uint16_t pos, uint8_t data);

/**
 * @brief   Sequentially write @p len bytes from a given position @p pos
 *
 * Writing is performed in chunks of size AT24CXXX_PAGE_SIZE.
 *
 * @param[in]       dev       AT24CXXX device handle
 * @param[in]       pos       Position in EEPROM memory
 * @param[in]       data      Write buffer
 * @param[in]       len       Requested length to be written
 *
 * @return          Number of bytes that have been written
 * @return          -ERANGE if @p pos + @p len is out of bounds
 * @return          @see i2c_write_regs
 */
int32_t at24cxxx_write(const at24cxxx_t *dev, uint16_t pos, const void *data,
                       size_t len);

/**
 * @brief   Set @p len bytes from a given position @p pos to the
 * value @p val
 *
 * Writing is performed in chunks of size AT24CXXX_SET_BUFFER_SIZE.
 *
 * @param[in]       dev       AT24CXXX device handle
 * @param[in]       pos       Position in EEPROM memory
 * @param[in]       val       Value to be set
 * @param[in]       len       Requested length to be written
 *
 * @return          Number of bytes that have been written
 * @return          -ERANGE if @p pos + @p len is out of bounds
 * @return          @see i2c_write_byte
 */
int32_t at24cxxx_set(const at24cxxx_t *dev, uint16_t pos, uint8_t val,
                     size_t len);

/**
 * @brief Set @p len bytes from position @p pos to
 * AT24CXXX_CLEAR_BYTE
 *
 * This is a wrapper around @see at24cxxx_set.
 *
 * @param[in]       dev       AT24CXXX device handle
 * @param[in]       pos       Position in EEPROM memory
 * @param[in]       len       Requested length to be written
 *
 * @return          @see at24cxxx_set
 */
int32_t at24cxxx_clear(const at24cxxx_t *dev, uint16_t pos, size_t len);

/**
 * @brief Set the entire EEPROM memory to AT24CXXX_CLEAR_BYTE
 *
 * This is a wrapper around @see at24cxxx_clear.
 *
 * @param[in]       dev       AT24CXXX device handle
 *
 * @return          @see at24cxxx_set
 */
int32_t at24cxxx_erase(const at24cxxx_t *dev);

/**
 * @brief Enable write protection
 *
 * @param[in]       dev       AT24CXXX device handle
 *
 * @return          AT24CXXX_OK on success
 * @return          -ENOTSUP if pin_wp was initialized with GPIO_UNDEF
 */
int at24cxxx_enable_write_protect(const at24cxxx_t *dev);

/**
 * @brief Disable write protection
 *
 * @param[in]       dev       AT24CXXX device handle
 *
 * @return          AT24CXXX_OK on success
 * @return          -ENOTSUP if pin_wp was initialized with GPIO_UNDEF
 */
int at24cxxx_disable_write_protect(const at24cxxx_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* AT24CXXX_H */
/** @} */
