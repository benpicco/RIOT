/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_mtd_sdcard
 * @{
 *
 * @file
 * @brief       Auto configure SD cards and mount the FAT file system
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */

#ifdef MODULE_MTD_SDCARD_AUTO_VFS

#include "mtd_sdcard.h"
#include "sdcard_spi_params.h"

#ifndef SDCARD_SPI_NUMOF
#define SDCARD_SPI_NUMOF    1
#endif

#define SDCARD_MTD_DEV(i)                   \
    {                                       \
        .base = {                           \
            .driver = &mtd_sdcard_driver    \
        },                                  \
        .sd_card = &sdcard_spi_devs[i],     \
        .params = &sdcard_spi_params[i],    \
    }

/* SD card devices are provided by drivers/sdcard_spi/sdcard_spi.c */
extern sdcard_spi_t sdcard_spi_devs[SDCARD_SPI_NUMOF];

/* Configure MTD device for the first SD card */
static mtd_sdcard_t mtd_sdcard_dev[SDCARD_SPI_NUMOF] = {
#if SDCARD_SPI_NUMOF > 0
    SDCARD_MTD_DEV(0),
#endif
#if SDCARD_SPI_NUMOF > 1
    SDCARD_MTD_DEV(1),
#endif
#if SDCARD_SPI_NUMOF > 2
    SDCARD_MTD_DEV(2),
#endif
#if SDCARD_SPI_NUMOF > 3
    SDCARD_MTD_DEV(3),
#endif
};

#if defined(MODULE_FATFS_VFS) && defined(MODULE_VFS_DEFAULT)
#include "fs/fatfs.h"
#if SDCARD_SPI_NUMOF > 0
VFS_AUTO_MOUNT(fatfs, VFS_MTD(mtd_sdcard_dev[0]), "/sd0", 10);
#endif
#if SDCARD_SPI_NUMOF > 1
VFS_AUTO_MOUNT(fatfs, VFS_MTD(mtd_sdcard_dev[1]), "/sd1", 11);
#endif
#if SDCARD_SPI_NUMOF > 2
VFS_AUTO_MOUNT(fatfs, VFS_MTD(mtd_sdcard_dev[2]), "/sd2", 12);
#endif
#if SDCARD_SPI_NUMOF > 3
VFS_AUTO_MOUNT(fatfs, VFS_MTD(mtd_sdcard_dev[3]), "/sd3", 13);
#endif
#endif

#else
typedef int dont_be_pedantic;
#endif
