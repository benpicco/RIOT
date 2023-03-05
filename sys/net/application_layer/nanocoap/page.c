/*
 * Copyright (C) 2023 ML!PA Consulting GmbH
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
 * @brief       NanoCoAP blockwise NON-multicast helpers
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include "atomic_utils.h"
#include "event/thread.h"
#include "fmt.h"
#include "random.h"
#include "macros/math.h"
#include "macros/utils.h"
#include "net/nanocoap/page.h"
#include "net/gnrc/netif.h"

#define ENABLE_DEBUG 1
#include "debug.h"

static bool _is_sending;

static int _block_resp_cb(void *arg, coap_pkt_t *pkt)
{
    nanocoap_page_ctx_t *ctx = arg;

    uint32_t shard_blocks = ctx->blocks_data + ctx->blocks_fec;
    uint32_t shard_num = unaligned_get_u32(pkt->payload);
    pkt->payload += sizeof(shard_num);
    
    switch (coap_get_code(pkt)) {
    case 408:
        if (shard_num != ctx->page) {
            DEBUG("lost blocks can't be satisfied (want %u blocks from shard %"PRIu32", have shard %"PRIu32")\n",
              bf_popcnt(pkt->payload, shard_blocks), shard_num, ctx->page);
            break;
        }
        
        bf_or_atomic(ctx->missing, ctx->missing, pkt->payload, shard_blocks);
        break;
    case 429:
        DEBUG("client requested slowdown (still has %u blocks to send)\n",
              unaligned_get_u16(pkt->payload));

        return NANOCOAP_SOCK_RX_AGAIN;
    default:
        DEBUG("unknown code: %d\n", coap_get_code(pkt));
        return NANOCOAP_SOCK_RX_AGAIN;
    }

    return NANOCOAP_SOCK_RX_AGAIN;
}

static inline uint32_t _deadline_left_us(uint32_t deadline)
{
    uint32_t now = ztimer_now(ZTIMER_MSEC) * US_PER_MS;
    deadline *= US_PER_MS;

    if (now > deadline) {
        return 0;
    }

    return deadline - now;
}

static int _block_request(coap_shard_request_ctx_t *req, nanocoap_page_ctx_t *ctx,
                          unsigned i, bool more)
{
    const size_t len = coap_szx2size(req->blksize);

    int res;
    bool more_shards = !ctx->is_last;
    uint16_t id = nanocoap_sock_next_msg_id(req->sock);

    uint8_t buf[CONFIG_NANOCOAP_BLOCK_HEADER_MAX];
    iolist_t snip = {
        .iol_base = &ctx->work_buf[i * len],
        .iol_len  = len,
    };

    /* last block of the transfer has a long timeout as the process ends with it */
    uint32_t timeout_us = CONFIG_NANOCOAP_FRAME_GAP_MS * US_PER_MS;

    do {
        /* build CoAP header */
        coap_pkt_t pkt = {
            .hdr = (void *)buf,
            .snips = &snip,
        };

        uint8_t *pktpos = (void *)pkt.hdr;
        uint16_t lastonum = 0;

        pktpos += coap_build_hdr(pkt.hdr, COAP_TYPE_NON, ctx->token, ctx->token_len,
                                 COAP_METHOD_PUT, id);
        pktpos += coap_opt_put_uri_pathquery(pktpos, &lastonum, req->path);
        pktpos += coap_opt_put_uint(pktpos, lastonum, COAP_OPT_BLOCK1,
                                    (i << 4) | req->blksize | (more ? 0x8 : 0));
        pktpos += coap_opt_put_page(pktpos, COAP_OPT_BLOCK1, ctx->page, ctx->blocks_data,
                                    ctx->blocks_fec, more_shards);

        /* all ACK responses (2.xx, 4.xx and 5.xx) are ignored */
        pktpos += coap_opt_put_uint(pktpos, COAP_OPT_PAGE, COAP_OPT_NO_RESPONSE, 26);

        /* set payload marker */
        *pktpos++ = 0xFF;
        pkt.payload = pktpos;

        DEBUG("send block %"PRIu32".%u\n", ctx->page, i);
        bf_unset(ctx->missing, i);
        res = nanocoap_sock_request_cb_timeout(req->sock, &pkt, _block_resp_cb, ctx,
                                               timeout_us, timeout_us);
    } while (0);

    return res;
}

static bool _shard_put(nanocoap_page_ctx_t *ctx, coap_shard_request_ctx_t *req)
{
    unsigned total_blocks = ctx->blocks_data + ctx->blocks_fec;

    int i;

    while ((i = bf_find_first_set(ctx->missing, total_blocks)) >= 0) {
        bool more = !ctx->is_last || ((i + 1) < (int)total_blocks) ;
        _block_request(req, ctx, i, more);
    }

    ++ctx->page;

    return ctx->is_last;
}

int nanocoap_shard_put(coap_shard_request_t *req, const void *data, size_t data_len,
                       const void *fec, size_t fec_len, bool more)
{
    const size_t len = coap_szx2size(req->req.blksize);
    nanocoap_page_ctx_t *ctx = &req->ctx;

    // TODO: move this out of here
    assert(ctx->work_buf == data);
    assert(fec == NULL); // TODO
    
    ctx->blocks_data = DIV_ROUND_UP(data_len, len);
    ctx->blocks_fec = DIV_ROUND_UP(fec_len, len);
    ctx->is_last = !more;
    ctx->state = STATE_TX;

    _is_sending = true;

    unsigned total_blocks = ctx->blocks_data + ctx->blocks_fec;

    /* initialize token */
    if (!ctx->token_len) {
        random_bytes(ctx->token, 4);
        ctx->token_len = 4;
    }

    /* mark all blocks to send */
    bf_set_all(ctx->missing, total_blocks);

    _shard_put(ctx, &req->req);
    _is_sending = false;

    return 0;
}

int nanocoap_shard_netif_join(const netif_t *netif)
{
    gnrc_netif_t *gnrc_netif = container_of(netif, gnrc_netif_t, netif);
    const ipv6_addr_t group = IPV6_ADDR_ALL_COAP_PAGE_LINK_LOCAL;
    return gnrc_netif_ipv6_group_join(gnrc_netif, &group);
}

int nanocoap_shard_netif_join_all(void)
{
    netif_t *netif = NULL;
    while ((netif = netif_iter(netif))) {
        int res = nanocoap_shard_netif_join(netif);
        if (res < 0) {
            return res;
        }
    }

    return 0;
}
