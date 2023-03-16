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

#include "net/nanocoap_sock.h"
#include "net/nanocoap/page.h"

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
int nanocoap_vfs_get_url(const char *url, const char *dst);

/**
 * @brief   Downloads the resource behind @p path via blockwise
 *          GET and stores it in the file @p dst.
 *
 * @param[in]   sock    Connection to the server
 * @param[in]   path    Remote query path to the resource
 * @param[in]   dst     Local path to the destination file
 *
 * @returns     0 on success
 * @returns     <0 on error
 */
int nanocoap_vfs_get(nanocoap_sock_t *sock, const char *path, const char *dst);

/**
 * @brief   Uploads the @p file to @p url via blockwise PUT.
 *
 * @param[in]   url          URL to the resource
 * @param[in]   src          Path to the source file
 * @param[in]   work_buf     Buffer to read file blocks into
 * @param[in]   work_buf_len Size of the buffer. Should be 1 byte more
 *                           than the desired CoAP blocksize.
 *
 * @returns     0 on success
 * @returns     <0 on error
 */
int nanocoap_vfs_put_url(const char *url, const char *src,
                         void *work_buf, size_t work_buf_len);

/**
 * @brief   Uploads the @p file to @p path via blockwise PUT.
 *
 * @param[in]   sock         Connection to the server
 * @param[in]   path         Remote query path to the resource
 * @param[in]   src          Path to the source file
 * @param[in]   work_buf     Buffer to read file blocks into
 * @param[in]   work_buf_len Size of the buffer. Should be 1 byte more
 *                           than the desired CoAP blocksize.
 *
 * @returns     0 on success
 * @returns     <0 on error
 */
int nanocoap_vfs_put(nanocoap_sock_t *sock, const char *path, const char *src,
                     void *work_buf, size_t work_buf_len);

int nanocoap_vfs_put_multicast(nanocoap_sock_t *sock, const char *path, const char *src);

typedef struct {
    coap_shard_handler_ctx_t super; /**< parent struct           */
    const char *path;               /**< destination path on VFS */
} coap_vfs_shard_ctx_t;

void nanocoap_vfs_page_handler(void *buf, size_t len, size_t offset,
                               bool more, coap_request_ctx_t *context);

#ifdef __cplusplus
}
#endif
#endif /* NET_NANOCOAP_VFS_H */
/** @} */
