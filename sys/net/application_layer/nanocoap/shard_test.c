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
 * @brief       Nanocoap VFS mock handler
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <stdio.h>
#include "fmt.h"
#include "fcntl.h"
#include "vfs_default.h"
#include "net/nanocoap/shard_test.h"

static void md5_final_print(md5_ctx_t *ctx)
{
    uint8_t digest[16];
    md5_final(ctx, digest);

    print_str("\n");
    print_str("hash: ");
    print_bytes_hex(digest, sizeof(digest));
    print_str("\n");
}

ssize_t nanocoap_shard_block_handler_md5(coap_pkt_t *pkt, uint8_t *buf, size_t len,
                                         coap_request_ctx_t *context)
{
    coap_shard_result_t out;

    const char *path = coap_request_ctx_get_path(context);
    coap_shard_test_ctx_t *ctx = coap_request_ctx_get_context(context);
    md5_ctx_t *md5 = &ctx->md5;

    int res = nanocoap_shard_block_handler(pkt, buf, len, context, &out);

    if (out.len) {
        if (out.offset == 0) {
            if (ctx->fd) {
                vfs_close(ctx->fd);
            }
            char filename[64];
            snprintf(filename, sizeof(filename), "%s%s", VFS_DEFAULT_DATA, path);

            ctx->fd = vfs_open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
            if (ctx->fd < 0) {
                ctx->fd = 0;
            }
            md5_init(md5);
        }

        md5_update(md5, out.data, out.len);

        fwrite(out.data, out.len, 1, stdout);

        if (ctx->fd) {
            vfs_write(ctx->fd, out.data, out.len);
        }

        if (!out.more) {
             md5_final_print(md5);

            if (ctx->fd) {
                vfs_close(ctx->fd);
                ctx->fd = 0;
            }
        }
    }

    return res;
}
