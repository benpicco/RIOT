/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_nanosock
 * @brief       VFS NanoCoAP helper functions
 *
 * @{
 *
 * @file
 * @brief       VFS NanoCoAP helper functions
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */
#ifndef NET_NANOCOAP_VFS_H
#define NET_NANOCOAP_VFS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Downloads the resource behind @p url via blockwise
 *          GET and stores it in the file @p dst.
 *
 * @param[in]   url     URL to the resource
 * @param[in]   dst     Path to the destination file
 *
 * @returns     0 on success
 * @returns     <0 on error
 */
int nanocoap_vfs_get(const char *url, const char *dst);

#ifdef __cplusplus
}
#endif
#endif /* NET_NANOCOAP_VFS_H */
/** @} */
