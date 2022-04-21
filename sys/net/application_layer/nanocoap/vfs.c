/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_nanocoap
 * @{
 *
 * @file
 * @brief       Nanocoap VFS helpers
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <fcntl.h>
#include "net/nanocoap_sock.h"
#include "vfs.h"

#define ENABLE_DEBUG 0
#include "debug.h"

static int _2file(void *arg, size_t offset, uint8_t *buf, size_t len, int more)
{
    (void)more;
    int *fd = arg;

    vfs_lseek(*fd, offset, SEEK_SET);
    return vfs_write(*fd, buf, len);
}

int nanocoap_vfs_get(nanocoap_sock_t *sock, const char *path, const char *dst)
{
    int res, fd = vfs_open(dst, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return fd;
    }

    res = nanocoap_sock_get_blockwise(sock, path, CONFIG_NANOCOAP_BLOCKSIZE_DEFAULT,
                                      _2file, &fd);
    vfs_close(fd);

    /* don't keep incomplete file around */
    if (res < 0) {
        vfs_unlink(dst);
    }

    return res;
}

int nanocoap_vfs_get_url(const char *url, const char *dst)
{
    DEBUG("nanocoap: downloading %s to %s\n", url, dst);

    int res, fd = vfs_open(dst, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return fd;
    }

    res = nanocoap_get_blockwise_url(url, CONFIG_NANOCOAP_BLOCKSIZE_DEFAULT,
                                     _2file, &fd);
    vfs_close(fd);

    /* don't keep incomplete file around */
    if (res < 0) {
        vfs_unlink(dst);
    }

    return res;
}
