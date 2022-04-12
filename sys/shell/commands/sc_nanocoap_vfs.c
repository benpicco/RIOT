/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_shell_commands
 * @{
 *
 * @file
 * @brief       NanoCoAP commands that interact with the filesystem
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "vfs_default.h"
#include "net/nanocoap_vfs.h"
#include "net/nanocoap_sock.h"

static bool _is_dir(char *url)
{
    int len = strlen(url);
    return url[len - 1] == '/';
}

static int _print(void *arg, size_t offset, uint8_t *buf, size_t len, int more)
{
    (void)arg;
    (void)offset;

    write(STDOUT_FILENO, buf, len);
    if (!more) {
        write(STDOUT_FILENO, "\n", 1);
    }

    return 0;
}

static int _print_dir(const char *url)
{
    return nanocoap_get_blockwise_url(url, CONFIG_NANOCOAP_BLOCKSIZE_DEFAULT,
                                      _print, NULL);
}

int _nanocoap_get_handler(int argc, char **argv)
{
    char buffer[64];
    char *dst, *url = argv[1];

    if (argc < 2) {
        printf("Usage: %s <url> [destination]\n", argv[0]);
        return -EINVAL;
    }

    if (_is_dir(url)) {
        return _print_dir(url);
    }

    if (argc < 3) {
        dst = strrchr(url, '/');
        if (dst == NULL) {
            printf("invalid url: '%s'\n", url);
            return -EINVAL;
        }
        if (snprintf(buffer, sizeof(buffer), "%s%s",
                     VFS_DEFAULT_DATA, dst) >= (int)sizeof(buffer)) {
            printf("destination truncated\n");
            return -ENOBUFS;
        }
        dst = buffer;
    } else {
        dst = argv[2];
    }

    return nanocoap_vfs_get(url, dst);
}
