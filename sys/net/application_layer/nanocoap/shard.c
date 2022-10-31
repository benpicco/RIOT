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
#include "net/nanocoap/shard.h"
#include "net/gnrc/netif.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

int nanocoap_shard_netif_join(const netif_t *netif)
{
    gnrc_netif_t *gnrc_netif = container_of(netif, gnrc_netif_t, netif);
    const ipv6_addr_t group = IPV6_ADDR_ALL_COAP_SHARD_LINK_LOCAL;
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

static int _block_resp_cb(void *arg, coap_pkt_t *pkt)
{
    coap_shard_common_ctx_t *ctx = arg;

    switch (coap_get_code(pkt)) {
    case 408:
        break;
    case 429:
        DEBUG("client requested slowdown (still has %u blocks to send)\n",
              unaligned_get_u16(pkt->payload));
        /* payload contains number of blocks downstream still has to send */
        uint16_t block_numof = unaligned_get_u16(pkt->payload);
        uint32_t deadlline = ztimer_now(ZTIMER_MSEC)
                           + block_numof * CONFIG_NANOCOAP_FRAME_GAP_MS;
        if (block_numof <= NANOCOAP_SHARD_BLOCKS_MAX) {
            ctx->slowdown_deadline = MAX(ctx->slowdown_deadline, deadlline);
        }
        return NANOCOAP_SOCK_RX_AGAIN;
    default:
        DEBUG("unknown code: %d\n", coap_get_code(pkt));
        return NANOCOAP_SOCK_RX_AGAIN;
    }

    uint32_t shard_num = unaligned_get_u32(pkt->payload);
    coap_shard_ctx_t *shard = &ctx->shards[shard_num & 1];

    unsigned shard_blocks = shard->blocks_data + shard->blocks_fec;
    pkt->payload += sizeof(shard_num);

    if (ctx->active_tx == shard_num) {
        DEBUG("want %u blocks from shard %"PRIu32" (current)\n",
              bf_popcnt(pkt->payload, shard_blocks), shard_num);
    } else if (ctx->active_tx == shard_num + 1) {
        DEBUG("want %u blocks from shard %"PRIu32" (last)\n",
              bf_popcnt(pkt->payload, shard_blocks), shard_num);
        if (!(shard->state & SHARD_STATE_TX)) {
            DEBUG("but shard is already released\n");
            /* TODO: tell node to give up? */
            return NANOCOAP_SOCK_RX_AGAIN;
        }
        ctx->active_tx = shard_num;
    } else {
        DEBUG("lost blocks can't be satisfied (want %u blocks from shard %"PRIu32", have shard %"PRIu32")\n",
              bf_popcnt(pkt->payload, shard_blocks), shard_num, ctx->active_tx);
        return NANOCOAP_SOCK_RX_AGAIN;
    }

    if (ENABLE_DEBUG) {
        size_t num_bytes = DIV_ROUND_UP(shard_blocks, 8);
        print_bytes_hex(shard->to_send, num_bytes);
        print_str(" + ");
        print_bytes_hex(pkt->payload, num_bytes);
        print_str("\n");
    }

    bf_or_atomic(shard->to_send, shard->to_send, pkt->payload, shard_blocks);

    return NANOCOAP_SOCK_RX_AGAIN;
}

static uint32_t _deadline_left_us(uint32_t deadline)
{
    uint32_t now = ztimer_now(ZTIMER_MSEC) * US_PER_MS;
    deadline *= US_PER_MS;

    if (now > deadline) {
        return 0;
    }

    return deadline - now;
}

static uint32_t _timeout_us(bool last_block, bool last_shard)
{
    uint8_t mul;

    if (last_block && last_shard) {
        /* very last block of a transmisson */
        mul = 25;
    } else if (last_block) {
        /* last block of a shard */
        mul = 10;
    } else if (last_shard) {
        mul = 1;
    } else {
        mul = 1;
    }

    return CONFIG_NANOCOAP_FRAME_GAP_MS * US_PER_MS * mul;
}

static int _block_request(coap_shard_request_ctx_t *req, coap_shard_common_ctx_t *ctx,
                          coap_shard_ctx_t *shard,
                          unsigned i, bool more_blocks, bool more)
{
    const size_t len = coap_szx2size(req->blksize);

    int res;
    unsigned blknum = shard->first_block + i;
    bool more_shards = !shard->is_last;
    uint16_t id = nanocoap_sock_next_msg_id(req->sock);

    uint8_t buf[CONFIG_NANOCOAP_BLOCK_HEADER_MAX];
    iolist_t snip = {
        .iol_base = &shard->work_buf[i * len],
        .iol_len  = len,
    };

    /* last block of the transfer has a long timeout as the process ends with it */
    uint32_t timeout_us = _timeout_us(!more_blocks, !more_shards);

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
                                    (blknum << 4) | req->blksize | (more ? 0x8 : 0));
        lastonum = COAP_OPT_BLOCK1;
        if (shard->blocks_data) {
            pktpos += coap_opt_put_uint(pktpos, lastonum, COAP_OPT_NON_BLOCKS,
                                        (shard->blocks_data << 1) | more_shards);
            lastonum = COAP_OPT_NON_BLOCKS;
        }
        if (shard->blocks_fec) {
            pktpos += coap_opt_put_uint(pktpos, lastonum, COAP_OPT_NON_BLOCKS_FEC,
                                        (shard->blocks_fec << 1) | more_shards);
            lastonum = COAP_OPT_NON_BLOCKS_FEC;
        }
        /* all ACK responses (2.xx, 4.xx and 5.xx) are ignored */
        pktpos += coap_opt_put_uint(pktpos, lastonum, COAP_OPT_NO_RESPONSE, 26);

        /* set payload marker */
        *pktpos++ = 0xFF;
        pkt.payload = pktpos;

        ctx->slowdown_deadline = 0;

        DEBUG("send block %u (shard %"PRIu32")\n", blknum, ctx->active_tx);
#ifdef MODULE_NANOCOAP_SHARD_DEBUG
        shard->transmits[i]++;
#endif
        bf_unset_atomic(shard->to_send, i);
        do {
            res = nanocoap_sock_request_cb_timeout(req->sock, &pkt, _block_resp_cb, ctx,
                                                   timeout_us, ctx->slowdown_deadline);
            timeout_us = _deadline_left_us(ctx->slowdown_deadline);
            if (ctx->slowdown_deadline) {
                DEBUG("new timeout: %"PRIu32" µs\n", timeout_us);
            }
        } while (timeout_us);
    } while (ctx->slowdown_deadline);

    return res;
}

static inline unsigned _update_shard(coap_shard_common_ctx_t *state, coap_shard_ctx_t **shard,
                                     unsigned *total_blocks)
{
    unsigned current_shard = state->active_tx;

    *shard = &state->shards[current_shard & 1];
    *total_blocks = (*shard)->blocks_data + (*shard)->blocks_fec;

    return current_shard;
}

static bool _shard_put(coap_shard_common_ctx_t *state, coap_shard_request_ctx_t *req, bool fwd)
{
    uint32_t current_shard = state->active_tx;
    coap_shard_ctx_t *shard = &state->shards[current_shard & 1];
    coap_shard_ctx_t *shard_prev = current_shard
                                 ? &state->shards[!(current_shard & 1)]
                                 : NULL;
    unsigned total_blocks = shard->blocks_data + shard->blocks_fec;

    int i;
    uint32_t next_shard = current_shard + 1;
    int next_first_block = shard->first_block + total_blocks;
    bool is_last = shard->is_last;
    do {
        while ((i = bf_find_first_set(shard->to_send, total_blocks)) >= 0) {

            bool more = ((i + 1) < next_first_block) || !is_last;

            unsigned blocks_left = bf_popcnt(shard->to_send, total_blocks);
            _block_request(req, state, shard, i, blocks_left > 1, more);

            if (current_shard != state->active_tx) {
                DEBUG("lost blocks in shard %"PRIu32"\n", state->active_tx);

                /* re-set missing bits in current shard */
                /* (other side will have ignored them)  */
                bf_set_all_atomic(shard->to_send, total_blocks);

                /* rewind shard */
                current_shard = _update_shard(state, &shard, &total_blocks);
            } else if (shard_prev && (shard_prev != shard) && !is_last &&
                      (blocks_left < total_blocks / 3)) {
                /* free previous shard if more than 2/3 of the next shard has been transmitted */
                unsigned total_blocks_prev = shard_prev->blocks_data
                                           + shard_prev->blocks_fec;
                DEBUG("free shard %"PRIu32"\n",
                      shard_prev->first_block / total_blocks_prev);
#ifdef MODULE_NANOCOAP_SHARD_DEBUG
                for (unsigned j = 0; j < total_blocks_prev; ++j) {
                    DEBUG("\tblock %"PRIu32" was transmitted %u times\n",
                          j + shard_prev->first_block, shard_prev->transmits[j]);
                    shard_prev->transmits[j] = 0;
                }
#endif
                atomic_fetch_and_u8(&shard_prev->state, ~SHARD_STATE_TX);
                shard_prev = NULL;
            }
        }

        /* shard not complete yet */
        if (bf_find_first_set(shard->missing, total_blocks) >= 0) {
            /* TODO: this can never happen? */
            DEBUG("wait for missing blocks in shard %"PRIu32"\n", state->active_tx);
            ztimer_sleep(ZTIMER_MSEC, 100);
            continue;
        }

        state->active_tx++;
        shard_prev = shard;
        current_shard = _update_shard(state, &shard, &total_blocks);
        DEBUG("next shard: %"PRIu32" (limit: %"PRIu32")\n", current_shard, next_shard);
    } while (current_shard != next_shard);

    if (!fwd) {
        shard->first_block = next_first_block;
    }

    return is_last;
}

int nanocoap_shard_put(coap_shard_request_t *req, const void *data, size_t data_len,
                       const void *fec, size_t fec_len, bool more)
{
    const size_t len = coap_szx2size(req->req.blksize);

    coap_shard_common_ctx_t *state = &req->ctx;
    unsigned current_shard = state->active_tx;
    coap_shard_ctx_t *shard = &state->shards[current_shard & 1];

    // TODO: move this out of here
    assert(shard->work_buf == data);
    assert(fec == NULL); // TODO

    shard->blocks_data = DIV_ROUND_UP(data_len, len);
    shard->blocks_fec = DIV_ROUND_UP(fec_len, len);
    shard->is_last = !more;
    shard->state = SHARD_STATE_TX;
    shard->next_first_block = UINT32_MAX; /* ignore blocks from forwarder nodes */

    unsigned total_blocks = shard->blocks_data + shard->blocks_fec;

    /* initialize token */
    if (!state->token_len) {
        random_bytes(state->token, 4);
        state->token_len = 4;
    }

    /* mark all blocks to send */
    bf_set_all(shard->to_send, total_blocks);
    memset(shard->missing, 0, DIV_ROUND_UP(total_blocks, 8));

    _shard_put(&req->ctx, &req->req, false);

    return 0;
}

#ifdef MODULE_NANOCOAP_SHARD_FORWARD
static mutex_t _forwarder_thread_mtx;
static char _forwarder_thread_stack[THREAD_STACKSIZE_DEFAULT];
static void *_forwarder_thread(void *arg)
{
    coap_shard_handler_ctx_t *hdl = arg;

    while (hdl->forward) {
        mutex_lock(&hdl->fwd_lock);

        DEBUG("start forwarding shard (rx: %"PRIu32", tx: %"PRIu32")\n", hdl->ctx.active_rx, hdl->ctx.active_tx);
        if (_shard_put(&hdl->ctx, &hdl->req, true)) {
            DEBUG("forwarding done\n");
            break;
        }
    }

    hdl->done = true;
    mutex_unlock(&_forwarder_thread_mtx);

    return NULL;
}
#endif
static inline bool _do_forward(coap_shard_handler_ctx_t *hdl)
{
#ifdef MODULE_NANOCOAP_SHARD_FORWARD
    return hdl->forward;
#else
    (void)hdl;
    return false;
#endif
}

static void _timer_cb(void *arg)
{
    coap_shard_handler_ctx_t *ctx = arg;
    event_post(EVENT_PRIO_MEDIUM, &ctx->event_timeout);
}

static void _request_slowdown(coap_shard_handler_ctx_t *hdl, uint8_t *buf, size_t len)
{
    (void)len;

    coap_shard_common_ctx_t *ctx = &hdl->ctx;
    coap_shard_ctx_t *shard = &ctx->shards[ctx->active_tx & 1];

    uint16_t id = nanocoap_sock_next_msg_id(&hdl->upstream);
    uint16_t blocks_left = bf_popcnt(shard->to_send, shard->blocks_data + shard->blocks_fec);

    uint8_t *pktpos = buf;

    coap_pkt_t pkt = {
        .hdr = (void *)buf,
    };

    pktpos += coap_build_hdr(pkt.hdr, COAP_TYPE_NON, ctx->token, ctx->token_len,
                             COAP_CODE_TOO_MANY_REQUESTS, id);
    /* set payload marker */
    *pktpos++ = 0xFF;
    pkt.payload = pktpos;

    /* tell upstream how many blocks there are left to send */
    memcpy(pktpos, &blocks_left, sizeof(blocks_left));
    pkt.payload_len = sizeof(blocks_left);

    nanocoap_sock_request_cb(&hdl->upstream, &pkt, NULL, NULL);

    /* TODO: pause hdl->timer */
    ztimer_remove(ZTIMER_MSEC, &hdl->timer);
}

static void _request_missing(coap_shard_handler_ctx_t *hdl, uint8_t *buf, size_t len)
{
    (void)len;

    coap_shard_common_ctx_t *ctx = &hdl->ctx;
    coap_shard_ctx_t *shard = &ctx->shards[ctx->active_rx & 1];
    size_t shard_blocks = shard->blocks_data + shard->blocks_fec;
    size_t bitmap_len = DIV_ROUND_UP(shard_blocks, 8);

    uint8_t *pktpos = buf;
    uint16_t id = nanocoap_sock_next_msg_id(&hdl->upstream);

    iolist_t payload = {
        .iol_base = (void *)shard->missing,
        .iol_len  = bitmap_len,
    };

    coap_pkt_t pkt = {
        .hdr = (void *)buf,
        .snips = &payload,
    };

    if (!(shard->state & SHARD_STATE_RX)) {
        DEBUG("shard %"PRIu32" not yet started\n", ctx->active_rx);
        return;
    }

    if (bf_find_first_set(shard->missing, shard_blocks) < 0) {
        DEBUG("shard %"PRIu32" already complete\n", ctx->active_rx);
        return;
    }

    DEBUG("re-request shard %"PRIu32" (%u blocks)\n", ctx->active_rx, bf_popcnt(shard->missing, shard_blocks));
    pktpos += coap_build_hdr(pkt.hdr, COAP_TYPE_NON, ctx->token, ctx->token_len,
                             COAP_CODE_REQUEST_ENTITY_INCOMPLETE, id);

    /* set payload marker */
    *pktpos++ = 0xFF;
    memcpy(pktpos, &ctx->active_rx, sizeof(ctx->active_rx));

    pkt.payload = pktpos;
    pkt.payload_len = sizeof(ctx->active_rx);

    nanocoap_sock_request_cb(&hdl->upstream, &pkt, NULL, NULL);

    if (shard->is_last) {

        /* if we didn't hear from the server for a while, give up */
        uint32_t now = ztimer_now(ZTIMER_MSEC) / MS_PER_SEC;
        if (now > hdl->timeout) {
            DEBUG("giving up\n");
            return;
        }

        /* set timeout for response (or any) block */
        uint32_t timeout_ms = (2 * CONFIG_NANOCOAP_FRAME_GAP_MS)
                            + (random_uint32() & 0x1F);
        ztimer_set(ZTIMER_MSEC, &hdl->timer, timeout_ms);
    }
}

static void _timeout_event(event_t *evp)
{
    uint8_t buffer[CONFIG_NANOCOAP_BLOCK_HEADER_MAX];
    coap_shard_handler_ctx_t *ctx = container_of(evp, coap_shard_handler_ctx_t, event_timeout);

    _request_missing(ctx, buffer, sizeof(buffer));
}

int nanocoap_shard_set_forward(coap_shard_handler_ctx_t *hdl, unsigned netif, bool on)
{
#ifdef MODULE_NANOCOAP_SHARD_FORWARD
    const sock_udp_ep_t remote = {
        .family = AF_INET6,
        .addr = IPV6_ADDR_ALL_COAP_SHARD_LINK_LOCAL,
        .port = COAP_PORT,
        .netif = netif,
    };

    /* bring sock to known state */
    if (hdl->forward) {
        hdl->forward = false;
        nanocoap_sock_close(hdl->req.sock);
        hdl->req.sock = NULL;
    }

    if (!on) {
        return 0;
    }

    int res = nanocoap_sock_connect(&hdl->downstream, NULL, &remote);
    if (res == 0) {
        hdl->req.sock = &hdl->downstream;
        hdl->forward = true;
    }

    return res;
#else
    (void)hdl;
    (void)remote;
    return -ENOTSUP;
#endif
}

ssize_t nanocoap_shard_block_handler(coap_pkt_t *pkt, uint8_t *buf, size_t len,
                                     coap_request_ctx_t *context, coap_shard_result_t *out)
{
    coap_shard_handler_ctx_t *hdl = coap_request_ctx_get_context(context);
    const sock_udp_ep_t *remote = coap_request_ctx_get_remote_udp(context);

    coap_shard_common_ctx_t *ctx = &hdl->ctx;
    uint32_t now = ztimer_now(ZTIMER_MSEC) / MS_PER_SEC;

    out->len = 0;

    if (coap_get_token_len(pkt) == 0 ||
        coap_get_token_len(pkt) > sizeof(ctx->token)) {
        DEBUG("invalid token len: %u\n", coap_get_token_len(pkt));
        return 0;
    }

    /* request token must match */
    if ((ctx->token_len != coap_get_token_len(pkt)) ||
        memcmp(ctx->token, coap_get_token(pkt), ctx->token_len)) {
        if (hdl->done || now > hdl->timeout) {
            DEBUG("request done/timeout\n");
            ctx->token_len = 0;
        } else {
            DEBUG("token missmatch\n");
            return 0;
        }
    }

    if (ctx->token_len == 0) {
        memset(ctx, 0, sizeof(*ctx));

        /* cleanup stale connection */
        nanocoap_sock_close(&hdl->upstream);
    }

    coap_shard_ctx_t *shard = &ctx->shards[ctx->active_rx & 1];

    coap_block1_t block1;
    if (coap_get_block1(pkt, &block1) < 0) {
        DEBUG("no block option\n");
        return 0;
    }

    if ((block1.blknum < shard->first_block) ||
        (block1.blknum < shard->next_first_block)) {
        DEBUG("old shard received (%"PRIu32" < %"PRIu32"|%"PRIu32")\n", block1.blknum, shard->first_block, shard->next_first_block);
        return 0;
    }

    /* if the current transfer expired, replace it */
    if (ctx->token_len == 0) {
        DEBUG("new request\n");

        /* store new token */
        ctx->token_len = coap_get_token_len(pkt);
        memcpy(ctx->token, coap_get_token(pkt), ctx->token_len);

        /* reset shard state */
        ctx->shards[0].state = 0;
        ctx->shards[1].state = 0;

        hdl->timer.callback = _timer_cb;
        hdl->timer.arg = ctx;
        hdl->event_timeout.handler = _timeout_event;

#ifdef MODULE_NANOCOAP_SHARD_FORWARD
        if (hdl->forward) {
            const char *path = coap_request_ctx_get_path(context);
            strncpy(hdl->path, path, sizeof(hdl->path));
            hdl->req.path = hdl->path;

            uint8_t prio = thread_get_priority(thread_get_active()) + 1;
            if (mutex_trylock(&_forwarder_thread_mtx)) {
                /* make sure fwd thread is locked */
                mutex_trylock(&hdl->fwd_lock);

                thread_create(_forwarder_thread_stack, sizeof(_forwarder_thread_stack),
                              prio, THREAD_CREATE_STACKTEST, _forwarder_thread, hdl,
                              "shard_fwd");
            } else {
                DEBUG("forwarding already ongoing\n");
            }
        }
#endif

        DEBUG("connect upstream on %u\n", remote->netif);
        nanocoap_sock_connect(&hdl->upstream, NULL, remote);
        hdl->done = false;
    } else {
        /* use the last remote to request re-transfers */
        sock_udp_set_remote(&hdl->upstream.udp, remote);
    }

    /* set timeout for the transfer */
    hdl->timeout = now + CONFIG_NANOCOAP_SHARD_XFER_TIMEOUT_SECS;


    /* set timeout for the next block */
    uint32_t timeout_ms = (2 * CONFIG_NANOCOAP_FRAME_GAP_MS)
                        + (random_uint32() & 0x1F);
    ztimer_set(ZTIMER_MSEC, &hdl->timer, timeout_ms);

    const size_t block_len = coap_szx2size(block1.szx);

    bool new_transfer = (shard->blocks_data == 0) && (shard->blocks_fec == 0);

    uint32_t blocks, blocks_per_shard = 0;
    bool more_shards = false;

    /* get number of payload blocks */
    if (coap_opt_get_uint(pkt, COAP_OPT_NON_BLOCKS, &blocks) == 0) {
        more_shards = blocks & 1;
        blocks_per_shard += blocks >> 1;
        if (new_transfer) {
            shard->blocks_data = blocks >> 1;
        }
    }

    /* get number of error correction blocks */
    if (coap_opt_get_uint(pkt, COAP_OPT_NON_BLOCKS_FEC, &blocks) == 0) {
        more_shards = blocks & 1;
        blocks_per_shard += blocks >> 1;
        if (new_transfer) {
            shard->blocks_fec = blocks >> 1;
        }
    }

    switch (shard->state) {
    case SHARD_STATE_EMPTY:
        DEBUG("start shard %"PRIu32" (%"PRIu32" blocks)\n", ctx->active_rx, blocks_per_shard);
        shard->state = SHARD_STATE_RX;
        shard->is_last = !more_shards;
        shard->first_block = shard->next_first_block;
        bf_set_all(shard->missing, blocks_per_shard);
        bf_clear_all(shard->to_send, blocks_per_shard);
        break;
    case SHARD_STATE_TX:
        DEBUG("shard %"PRIu32" busy TX\n", ctx->active_tx);
        _request_slowdown(hdl, buf, len);
        return 0;
    default:
        break;
    }

    uint32_t block_in_shard = block1.blknum - shard->first_block;
    DEBUG("got block %"PRIu32" (%"PRIu32" / %"PRIu32")\n", block1.blknum, block_in_shard, blocks_per_shard - 1);

    if (block_in_shard >= (shard->blocks_data + shard->blocks_fec)) {

        sock_udp_ep_t upstream;
        sock_udp_get_remote(&hdl->upstream.udp, &upstream);
        if (memcmp(&remote->addr, &upstream.addr, sizeof(remote->addr))) {
            DEBUG("ignore block from other node outside current shard\n");
        } else {
            DEBUG("block from next shard received, old shard not complete\n");
            ztimer_sleep(ZTIMER_USEC, random_uint32() % 0x7FF);
            _request_missing(hdl, buf, len);
        }
        return 0;
    }

    if (!bf_isset(shard->missing, block_in_shard)) {
        DEBUG("old block received\n");
        return 0;
    }

    /* store the new block in the current shard */
    bf_unset(shard->missing, block_in_shard);
    memcpy(&shard->work_buf[block_len * block_in_shard],
           pkt->payload, pkt->payload_len);

#ifdef MODULE_NANOCOAP_SHARD_FORWARD
    if (hdl->forward) {
        /* use the same block size for forwarding */
        hdl->req.blksize = block1.szx;
        /* mark current block as to send */
        bf_set_atomic(shard->to_send, block_in_shard);

        shard->state = SHARD_STATE_RX_AND_TX;
    }
#endif

    /* check if there are any missing blocks in the current shard */
    if (bf_find_first_set(shard->missing, blocks_per_shard) < 0) {
        DEBUG("shard %"PRIu32" done%s, got %"PRIu32" blocks!\n",
              ctx->active_rx, more_shards ? "" : "(last shard)", blocks_per_shard);

        coap_shard_ctx_t *prev = ctx->active_rx
                               ? &ctx->shards[(ctx->active_rx - 1) & 1]
                               : shard;
        unsigned payload_blocks = shard->first_block - (ctx->active_rx * prev->blocks_fec);

        out->offset = payload_blocks * block_len;
        out->data = shard->work_buf;
        out->len = shard->blocks_data * block_len;
        out->more = more_shards;

        atomic_fetch_and_u8(&shard->state, ~SHARD_STATE_RX);

        uint32_t next_start_block = shard->first_block + blocks_per_shard;
        shard = &ctx->shards[++ctx->active_rx & 1];
        shard->next_first_block = next_start_block;

        if (!more_shards) {
            puts("last shard complete");
            ztimer_remove(ZTIMER_MSEC, &hdl->timer);
            nanocoap_sock_close(&hdl->upstream);
            if (!_do_forward(hdl)) {
                hdl->done = true;
            }
        }

#ifdef MODULE_NANOCOAP_SHARD_FORWARD
        mutex_unlock(&hdl->fwd_lock);
#endif
    }

    return 0;
}
