/*
 * Copyright (C) 2019 Otto-von-Guericke-Universität Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_at24cxxx
 * @{
 *
 * @file
 * @brief       Constants for AT24CXXX EEPROM devices.
 *
 * @author      Fabian Hüßler <fabian.huessler@ovgu.de>
 */

#ifndef AT24CXXX_DEFINES_H
#define AT24CXXX_DEFINES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    AT24CXXX constants
 * @{
 */

/**
 * @brief 14 bit for AT24C128, 15 bit for AT24C256
 */
#define AT24CXXX_ADDR_LEN          (2UL)
/**
 * @brief Page size
 */
#define AT24CXXX_PAGE_SIZE         (64UL)
/**
 * @brief Number of poll attempts
 */
#define AT24CXXX_MAX_POLLS         (6UL)
/**
 * @brief Delay between two poll attempts
 */
#define AT24CXXX_POLL_DELAY_US     (1000UL)
/**
 * @brief Clear byte
 */
#define AT24CXXX_CLEAR_BYTE        (0x00)
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* AT24CXXX_DEFINES_H */
/** @} */
