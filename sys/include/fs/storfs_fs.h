/*
 * Copyright (C) 2017 OTA keys S.A.
 * Copyright (C) 2020 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_littlefs2 littlefs v2 integration
 * @ingroup     pkg_littlefs2
 * @brief       RIOT integration of littlefs version 2.x.y
 *
 * @{
 *
 * @file
 * @brief       littlefs v2 integration with vfs
 *
 * @author      Vincent Dupont <vincent@otakeys.com>
 * @author      Koen Zandberg <koen@bergzand.net>
 */

#ifndef FS_LITTLEFS2_FS_H
#define FS_LITTLEFS2_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vfs.h"
#include "storfs.h"
#include "mutex.h"
#include "mtd.h"

/**
 * @brief   littlefs descriptor for vfs integration
 */
typedef struct {
    storfs_t fs;                /**< storfs descriptor */
    mtd_dev_t *dev;             /**< mtd device to use */
} storfs_desc_t;

/** The storfs vfs driver */
extern const vfs_file_system_t storfs_file_system;

#ifdef __cplusplus
}
#endif

#endif /* FS_LITTLEFS2_FS_H */
/** @} */
