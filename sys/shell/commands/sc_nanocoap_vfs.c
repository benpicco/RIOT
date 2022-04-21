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

#ifndef CONFIG_COAP_PATH_MAX_LEN
#define CONFIG_COAP_PATH_MAX_LEN (64)
#endif

struct dir_list_ctx {
    char *buf;
    char *cur;
    char *end;
    int (*cb)(char *path, void *ctx);
    void *ctx;
};

struct dl_ctx {
    nanocoap_sock_t sock;
    char *dst;
    size_t len;
};

static bool _is_dir(char *url)
{
    int len = strlen(url);
    return url[len - 1] == '/';
}

static int _print(void *arg, size_t offset, uint8_t *buf, size_t len, int more)
{
    (void)offset;
    (void)more;

    struct dir_list_ctx *ctx = arg;

    char *end = (char *)buf + len;
    for (char *c = (char *)buf; c < end; ++c) {
        if (ctx->cur) {
            if (*c == '>' || ctx->cur == ctx->end) {
                *ctx->cur = 0;
                ctx->cb(ctx->buf, ctx->ctx);
                ctx->cur = NULL;
            } else {
                *ctx->cur++ = *c;
            }
        } else if (*c == '<') {
            ctx->cur = ctx->buf;
        }
    }

    return 0;
}

static int _do_print(char *url, void *arg)
{
    (void) arg;

    puts(url);

    return 0;
}

static int _do_download(char *url, void *arg)
{
    struct dl_ctx *ctx = arg;

    if (_is_dir(url)) {
        return 0;
    }
    char *basename = strrchr(url, '/');
    size_t len = strlen(ctx->dst);
    strcat(ctx->dst, basename);

    printf("getting %s to '%s'\n", url, ctx->dst);

    int res = nanocoap_vfs_get(&ctx->sock, url, ctx->dst);

    ctx->dst[len] = 0;
    return res;
}

static int _print_dir(const char *url, char *buf, size_t len)
{
    struct dir_list_ctx ctx = {
        .buf = buf,
        .end = buf + len,
        .cb = _do_print,
    };
    return nanocoap_get_blockwise_url(url, CONFIG_NANOCOAP_BLOCKSIZE_DEFAULT,
                                      _print, &ctx);
}

static int _download_dir(const char *url, char *buf, size_t len, struct dl_ctx *arg)
{
    struct dir_list_ctx ctx = {
        .buf = buf,
        .end = buf + len,
        .cb = _do_download,
        .ctx = arg,
    };
    return nanocoap_get_blockwise_url(url, CONFIG_NANOCOAP_BLOCKSIZE_DEFAULT,
                                      _print, &ctx);
}

int _nanocoap_get_handler(int argc, char **argv)
{
    int res;
    char buffer[CONFIG_COAP_PATH_MAX_LEN];
    char *dst, *url = argv[1];

    if (argc < 2) {
        printf("Usage: %s <url> [destination]\n", argv[0]);
        printf("Default destination: %s\n", VFS_DEFAULT_DATA);
        return -EINVAL;
    }

    if (_is_dir(url)) {
        res = _print_dir(url, buffer, sizeof(buffer));
        if (res) {
            printf("Request failed: %s\n", strerror(-res));
        }
        return res;
    }

    if (argc < 3) {
        dst = strrchr(url, '/');
        if (dst == NULL) {
            printf("invalid url: '%s'\n", url);
            return -EINVAL;
        }
        if (snprintf(buffer, sizeof(buffer), "%s%s",
                     VFS_DEFAULT_DATA, dst) >= (int)sizeof(buffer)) {
            printf("Output file path too long\n");
            return -ENOBUFS;
        }
        dst = buffer;
    } else {
        dst = argv[2];
    }

    res = nanocoap_vfs_get_url(url, dst);
    if (res < 0) {
        printf("Download failed: %s\n", strerror(-res));
    } else {
        printf("Saved as %s\n", dst);
    }
    return res;
}

int _nanocoap_mirror_dir(int argc, char **argv)
{
    int res;
    char buffer[CONFIG_COAP_PATH_MAX_LEN];
    char dst[CONFIG_COAP_PATH_MAX_LEN];
    char *url = argv[1];

    struct dl_ctx ctx = {
        .dst = dst,
        .len = sizeof(dst),
    };

    if (argc < 2) {
        printf("Usage: %s <url> [destination]\n", argv[0]);
        printf("Default destination: %s\n", VFS_DEFAULT_DATA);
        return -EINVAL;
    }

    if (argc < 3) {
        strncpy(dst, VFS_DEFAULT_DATA, sizeof(dst));
    } else {
        strncpy(dst, argv[2], sizeof(dst));
    }

    nanocoap_sock_url_connect(url, &ctx.sock);
    res = _download_dir(url, buffer, sizeof(buffer), &ctx);
    nanocoap_sock_close(&ctx.sock);

    return res;
}
