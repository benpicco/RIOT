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

#include "log.h"

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
    case 231:   /* continue */
        return NANOCOAP_SOCK_RX_MORE;
    case 408:   /* request entity incomplete */
        if (shard_num != ctx->page) {
            DEBUG("lost blocks can't be satisfied (want %u blocks from shard %"PRIu32", have shard %"PRIu32")\n",
                   bf_popcnt(pkt->payload, shard_blocks), shard_num, ctx->page);
            break;
        }

        bf_or_atomic(ctx->missing, ctx->missing, pkt->payload, shard_blocks);
        DEBUG("neighbor re-requested %u blocks, total to send: %u\n",
              bf_popcnt(pkt->payload, shard_blocks), bf_popcnt(ctx->missing, shard_blocks));

        ctx->state = STATE_TX;
        break;
    case 429:   /* too many requests */
        DEBUG("neighbor requested slowdown (still has %u blocks to send)\n",
              unaligned_get_u16(pkt->payload));

        ctx->state = STATE_TX_WAITING;
        ctx->wait_blocks = MAX(ctx->wait_blocks, shard_blocks + unaligned_get_u16(pkt->payload));
        bf_set_all(ctx->missing, shard_blocks);

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
    const unsigned total_blocks = ctx->blocks_data + ctx->blocks_fec;
    unsigned blocks_left = bf_popcnt(ctx->missing, total_blocks) - 1;

    int res;
    bool more_shards = !ctx->is_last;
    uint16_t id = nanocoap_sock_next_msg_id(req->sock);

    uint8_t buf[CONFIG_NANOCOAP_BLOCK_HEADER_MAX];
    iolist_t snip = {
        .iol_base = &ctx->work_buf[i * len],
        .iol_len  = len,
    };

    /* TODO: longer timeout for last page block / waiting state */
    uint32_t timeout_us = CONFIG_NANOCOAP_FRAME_GAP_MS * US_PER_MS;

    if (blocks_left == 0) {
        ctx->state = STATE_TX_WAITING;
    }

    if (ctx->state == STATE_TX_WAITING) {
        timeout_us = ctx->wait_blocks * CONFIG_NANOCOAP_FRAME_GAP_MS
                   * US_PER_MS;
        DEBUG("wait blocks: %u\n", ctx->wait_blocks);
    }

    ctx->state = STATE_TX;
    ctx->wait_blocks = total_blocks;

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
                                    ctx->blocks_fec, blocks_left, more_shards);

        /* all ACK responses (2.xx, 4.xx and 5.xx) are ignored */
        pktpos += coap_opt_put_uint(pktpos, COAP_OPT_PAGE, COAP_OPT_NO_RESPONSE, 26);

        /* set payload marker */
        *pktpos++ = 0xFF;
        pkt.payload = pktpos;

        DEBUG("send block %"PRIu32".%u\n", ctx->page, i);
        bf_unset(ctx->missing, i);

        res = nanocoap_sock_request_cb_timeout(req->sock, &pkt, _block_resp_cb, ctx,
                                               timeout_us, false);
    } while (0);

    return res;
}

static bool _shard_put(nanocoap_page_ctx_t *ctx, coap_shard_request_ctx_t *req)
{
    const unsigned total_blocks = ctx->blocks_data + ctx->blocks_fec;
    ctx->wait_blocks = total_blocks;

    while (bf_find_first_set(ctx->missing, total_blocks) >= 0) {
        /* spread out blocks more evenly */
        for (unsigned i = 0; i < total_blocks; ++i) {
            if (!bf_isset(ctx->missing, i)) {
                continue;
            }
            bool more = !ctx->is_last || ((i + 1) < total_blocks);
            _block_request(req, ctx, i, more);
        }
    }

    DEBUG("page %"PRIu32" done\n", ctx->page);
    ++ctx->page;

    return ctx->is_last;
}

static inline bool _do_forward(coap_shard_handler_ctx_t *hdl)
{
#ifdef MODULE_NANOCOAP_PAGE_FORWARD
    return hdl->forward;
#else
    (void)hdl;
    return false;
#endif
}

#ifdef MODULE_NANOCOAP_PAGE_FEC_RS
static void _fec_rs_init(nanocoap_page_ctx_t *ctx, nanocoap_page_coding_ctx_t *fec, size_t blk_len)
{
    uint8_t *buf = ctx->work_buf;

    DEBUG("init reed-solomon codec, data blocks: %u, parity blocks: %u, block size: %u\n",
            ctx->blocks_data, ctx->blocks_fec, blk_len);
    reed_solomon *rs = reed_solomon_new_static(fec->ctx.rs.rs_buf,
                                               sizeof(fec->ctx.rs.rs_buf),
                                               ctx->blocks_data, ctx->blocks_fec);
    assert(rs);

    /* set up block pointers */
    for (unsigned i = 0; i < ctx->blocks_data + ctx->blocks_fec; ++i) {
        fec->ctx.rs.blocks[i] = &buf[i * blk_len];
        assert(fec->ctx.rs.blocks[i] + blk_len <= &ctx->work_buf[sizeof(ctx->work_buf)]);
    }
}

static void _fec_rs_encode(coap_shard_request_t *req)
{
    const size_t len = coap_szx2size(req->req.blksize);

    reed_solomon *rs = nanocoap_page_coding_ctx_get_rs(&req->fec);
    int res = reed_solomon_encode(rs, req->fec.ctx.rs.blocks,
                                  ARRAY_SIZE(req->fec.ctx.rs.blocks), len);
    assert(res == 0);
}

static bool _fec_rs_decode(coap_shard_handler_ctx_t *req)
{
    nanocoap_page_ctx_t *ctx = &req->ctx;
    const size_t len = coap_szx2size(req->blksize);
    const unsigned total_blocks = ctx->blocks_data + ctx->blocks_fec;

    if (ctx->blocks_fec == 0) {
        return false;
    }

    unsigned missing = bf_popcnt(ctx->missing, total_blocks);

    if (missing == 0) {
        return true;
    }

    uint8_t marks[NANOCOAP_SHARD_BLOCKS_MAX];

    for (unsigned i = 0; i < total_blocks; ++i) {
        marks[i] = bf_isset(ctx->missing, i);
    }

    DEBUG("try to reconstruct %u / %u blocks (%u byte each)…\n", missing, total_blocks, len);

    reed_solomon *rs = nanocoap_page_coding_ctx_get_rs(&req->fec);

    if (reed_solomon_decode(rs, req->fec.ctx.rs.blocks, marks, total_blocks, len) == 0) {
        bf_clear_all(ctx->missing, ctx->blocks_data);
        DEBUG("success!\n");
        return true;
    }

    return false;
}
#endif
#ifdef MODULE_NANOCOAP_PAGE_FEC_XOR
static void _fec_xor_init(nanocoap_page_ctx_t *ctx, nanocoap_page_coding_ctx_t *fec, size_t blk_len)
{
    (void)fec;
    (void)blk_len;
    ctx->blocks_fec = CODING_XOR_PARITY_LEN(ctx->blocks_data);
}

static void _fec_xor_encode(coap_shard_request_t *req)
{
    nanocoap_page_ctx_t *ctx = &req->ctx;
    const size_t len = coap_szx2size(req->req.blksize);
    size_t data_len = len * ctx->blocks_data;

    coding_xor_generate(ctx->work_buf, data_len, &ctx->work_buf[data_len]);
}

static bool _fec_xor_decode(coap_shard_handler_ctx_t *req)
{
    nanocoap_page_ctx_t *ctx = &req->ctx;
    const size_t len = coap_szx2size(req->blksize);
    size_t data_len = len * ctx->blocks_data;

    return coding_xor_recover(ctx->work_buf, data_len, &ctx->work_buf[data_len],
                              ctx->missing, len, _do_forward(req));
}
#endif

#ifdef MODULE_NANOCOAP_PAGE_FEC
static bool _fec_init(nanocoap_page_ctx_t *ctx, nanocoap_page_coding_ctx_t *fec, size_t blk_len)
{
    if (ctx->blocks_fec == 0) {
        return false;
    }

#if defined(MODULE_NANOCOAP_PAGE_FEC_RS)
    _fec_rs_init(ctx, fec, blk_len);
#elif defined(MODULE_NANOCOAP_PAGE_FEC_XOR)
    _fec_xor_init(ctx, fec, blk_len);
#endif
    return true;
}

static void _fec_encode(coap_shard_request_t *req)
{
#if defined(MODULE_NANOCOAP_PAGE_FEC_RS)
    _fec_rs_encode(req);
#elif defined(MODULE_NANOCOAP_PAGE_FEC_XOR)
    _fec_xor_encode(req);
#endif
}

static bool _fec_decode(coap_shard_handler_ctx_t *req)
{
#if defined(MODULE_NANOCOAP_PAGE_FEC_RS)
    return _fec_rs_decode(req);
#elif defined(MODULE_NANOCOAP_PAGE_FEC_XOR)
    return _fec_xor_decode(req);
#endif
}
#else
static inline void _fec_encode(coap_shard_request_t *req) { (void)req; }
static inline bool _fec_decode(coap_shard_handler_ctx_t *req) { (void)req; return false; }
#endif

int nanocoap_shard_put(coap_shard_request_t *req, const void *data, size_t data_len,
                       bool more)
{
    const size_t len = coap_szx2size(req->req.blksize);
    nanocoap_page_ctx_t *ctx = &req->ctx;

    // TODO: move this out of here
    assert(ctx->work_buf == data);

    ctx->blocks_data = DIV_ROUND_UP(data_len, len);
    ctx->is_last = !more;

#ifdef MODULE_NANOCOAP_PAGE_FEC
    if (_fec_init(&req->ctx, &req->fec, len)) {
        _fec_encode(req);
    }
#endif

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

    if (!more) {
        _is_sending = false;
    }

    return 0;
}

/*
 * forwarder section
 */

#ifdef MODULE_NANOCOAP_PAGE_FORWARD
static mutex_t _forwarder_thread_mtx;
static char _forwarder_thread_stack[THREAD_STACKSIZE_DEFAULT];
static void *_forwarder_thread(void *arg)
{
    coap_shard_handler_ctx_t *hdl = arg;

    while (hdl->forward) {
        mutex_lock(&hdl->fwd_lock);

        DEBUG("start forwarding page %"PRIu32"\n", hdl->ctx.page);
        if (_shard_put(&hdl->ctx, &hdl->req)) {
            DEBUG("forwarding done\n");
            hdl->ctx.state = STATE_IDLE;
            hdl->ctx.token_len = 0;
            break;
        } else {
            hdl->ctx.state = STATE_RX_WAITING;
        }
    }

    DEBUG("forwarder thread done\n");
    mutex_unlock(&_forwarder_thread_mtx);

    return NULL;
}
#endif

int nanocoap_shard_set_forward(coap_shard_handler_ctx_t *hdl, unsigned netif, bool on)
{
#ifdef MODULE_NANOCOAP_PAGE_FORWARD
    const sock_udp_ep_t remote = {
        .family = AF_INET6,
        .addr = IPV6_ADDR_ALL_COAP_PAGE_LINK_LOCAL,
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

static bool _invalid_request(coap_pkt_t *pkt, coap_shard_handler_ctx_t *hdl,
                             nanocoap_page_ctx_t *ctx)
{
    /* hack */
    if (_is_sending) {
        return true;
    }

    if (coap_get_token_len(pkt) == 0 ||
        coap_get_token_len(pkt) > sizeof(ctx->token)) {
        DEBUG("invalid token len: %u\n", coap_get_token_len(pkt));
        return true;
    }

    uint32_t now = ztimer_now(ZTIMER_MSEC) / MS_PER_SEC;

    /* request token must match */
    if ((ctx->token_len != coap_get_token_len(pkt)) ||
        memcmp(ctx->token, coap_get_token(pkt), ctx->token_len)) {
        if (ctx->state == STATE_IDLE || now > hdl->timeout) {
            DEBUG("request done/timeout - reset state\n");
            memset(ctx, 0, sizeof(*ctx));
            return false;
        } else {
            DEBUG("token missmatch\n");
            return true;
        }
    }

    /* set timeout for the transfer */
    hdl->timeout = now + CONFIG_NANOCOAP_SHARD_XFER_TIMEOUT_SECS;

    return false;
}

/*
 * request handler section
 */

static void _request_slowdown(coap_shard_handler_ctx_t *hdl, uint8_t *buf, size_t len)
{
    (void)len;

    nanocoap_page_ctx_t *ctx = &hdl->ctx;

    uint16_t id = nanocoap_sock_next_msg_id(&hdl->upstream);
    uint16_t blocks_left = bf_popcnt(ctx->missing, ctx->blocks_data + ctx->blocks_fec);

    DEBUG("request slowdown (%u blocks left to send)\n", blocks_left);

    uint8_t *pktpos = buf;

    coap_pkt_t pkt = {
        .hdr = (void *)buf,
    };

    pktpos += coap_build_hdr(pkt.hdr, COAP_TYPE_NON, ctx->token, ctx->token_len,
                             COAP_CODE_TOO_MANY_REQUESTS, id);
    /* set payload marker */
    *pktpos++ = 0xFF;
    pkt.payload = pktpos;

    memcpy(pktpos, &ctx->page, sizeof(ctx->page));
    pktpos += sizeof(ctx->page);

    /* tell upstream how many blocks there are left to send */
    memcpy(pktpos, &blocks_left, sizeof(blocks_left));
    pktpos += sizeof(blocks_left);

    pkt.payload_len = pktpos - pkt.payload;

    nanocoap_sock_request_cb(&hdl->upstream, &pkt, NULL, NULL);
}

static void _request_missing(coap_shard_handler_ctx_t *hdl, uint8_t *buf, size_t len)
{
    (void)len;

    nanocoap_page_ctx_t *ctx = &hdl->ctx;
    size_t shard_blocks = ctx->blocks_data + ctx->blocks_fec;
    size_t bitmap_len = DIV_ROUND_UP(shard_blocks, 8);

    uint8_t *pktpos = buf;
    uint16_t id = nanocoap_sock_next_msg_id(&hdl->upstream);

    iolist_t payload = {
        .iol_base = (void *)ctx->missing,
        .iol_len  = bitmap_len,
    };

    coap_pkt_t pkt = {
        .hdr = (void *)buf,
        .snips = &payload,
    };

    if ((ctx->state != STATE_RX) && (ctx->state != STATE_ORPHAN)) {
        return;
    }

    unsigned missing = bf_popcnt(ctx->missing, shard_blocks);

    if (missing == 0) {
        DEBUG("page %"PRIu32" already complete\n", ctx->page);
        return;
    }

    if (_fec_decode(hdl)) {
        DEBUG("reconstructed all missing blocks\n");
        event_post(EVENT_PRIO_MEDIUM, &hdl->event_page_done);
        return;
    }

    DEBUG("re-request page %"PRIu32" (%u blocks)\n",
          ctx->page, bf_popcnt(ctx->missing, shard_blocks));
    pktpos += coap_build_hdr(pkt.hdr, COAP_TYPE_NON, ctx->token, ctx->token_len,
                             COAP_CODE_REQUEST_ENTITY_INCOMPLETE, id);
    /* set payload marker */
    *pktpos++ = 0xFF;
    memcpy(pktpos, &ctx->page, sizeof(ctx->page));

    pkt.payload = pktpos;
    pkt.payload_len = sizeof(ctx->page);

    nanocoap_sock_request_cb(&hdl->upstream, &pkt, NULL, NULL);

    /* set timeout for retransmission request */
    uint32_t timeout_ms = (shard_blocks / 2) * CONFIG_NANOCOAP_FRAME_GAP_MS
                        + (random_uint32() & 0x7);
    ztimer_set(ZTIMER_MSEC, &hdl->timer, timeout_ms);
}

static void _timeout_event(event_t *evp)
{
    uint8_t buffer[CONFIG_NANOCOAP_BLOCK_HEADER_MAX];
    coap_shard_handler_ctx_t *ctx = container_of(evp, coap_shard_handler_ctx_t, event_timeout);

    _request_missing(ctx, buffer, sizeof(buffer));
}

static void _timer_cb(void *arg)
{
    coap_shard_handler_ctx_t *hdl = arg;

    /* don't retry indefinitely */
    if (!hdl->ctx.wait_blocks) {
        DEBUG("retries exhausted\n");
        return;
    }
    --hdl->ctx.wait_blocks;

    event_post(EVENT_PRIO_MEDIUM, &hdl->event_timeout);
}

static void _page_done_event(event_t *evp)
{
    coap_shard_handler_ctx_t *hdl = container_of(evp, coap_shard_handler_ctx_t, event_page_done);
    nanocoap_page_ctx_t *ctx = &hdl->ctx;
    size_t block_len = coap_szx2size(hdl->blksize);
    uint8_t blocks_per_shard = ctx->blocks_data + ctx->blocks_fec;

    ztimer_remove(ZTIMER_MSEC, &hdl->timer);

    coap_request_ctx_t context = {
        .resource = hdl->resource,
    };

    assert(hdl->cb);
    hdl->cb(ctx->work_buf, ctx->blocks_data * block_len, hdl->offset_rx,
            !ctx->is_last, &context);

    if (ctx->is_last) {
        nanocoap_sock_close(&hdl->upstream);
        if (!_do_forward(hdl)) {
            ctx->state = STATE_IDLE;
            ctx->token_len = 0;
        }
    } else {
        hdl->offset_rx += ctx->blocks_data * block_len;
        if (!_do_forward(hdl)) {
            ctx->state = STATE_RX_WAITING;
            ++ctx->page;
        }
    }

#ifdef MODULE_NANOCOAP_PAGE_FORWARD
    if (_do_forward(hdl)) {
        ctx->state = STATE_TX;
        /* we need to re-encode if we lost FEC blocks */
        if (bf_find_first_set(ctx->missing, blocks_per_shard) >= 0) {
            _fec_encode((void *)hdl);
        }
        bf_set_all(ctx->missing, blocks_per_shard);
        mutex_unlock(&hdl->fwd_lock);
    }
#endif
}

ssize_t nanocoap_page_block_handler(coap_pkt_t *pkt, uint8_t *buf, size_t len,
                                    coap_request_ctx_t *context)
{
    coap_shard_handler_ctx_t *hdl = coap_request_ctx_get_context(context);
    const sock_udp_ep_t *remote = coap_request_ctx_get_remote_udp(context);
    nanocoap_page_ctx_t *ctx = &hdl->ctx;

    if (_invalid_request(pkt, hdl, ctx)) {
        return 0;
    }

    uint32_t page_rx, ndata_rx, nfec_rx, blocks_per_shard, blocks_left;
    int more_shards = coap_get_page(pkt, &page_rx, &ndata_rx, &nfec_rx, &blocks_left);
    blocks_per_shard = ndata_rx + nfec_rx;

    if (more_shards < 0) {
        DEBUG("no page option\n");
        return 0;
    }

    coap_block1_t block1;
    if (coap_get_block1(pkt, &block1) < 0) {
        DEBUG("no block option\n");
        return 0;
    }

    if (block1.blknum >= NANOCOAP_SHARD_BLOCKS_MAX) {
        DEBUG("block index out of bounds\n");
        return 0;
    }

    const size_t block_len = coap_szx2size(block1.szx);

    if (page_rx != ctx->page) {
        if ((ctx->state == STATE_IDLE) || (ctx->state == STATE_ORPHAN)) {
            return 0;
        }

        if (page_rx < ctx->page) {
            return 0;
        }

        /* ignore wrong page by node that is not our upstream */
        if (memcmp(remote, &hdl->upstream.udp.remote, sizeof(*remote))) {
            return 0;
        }

        DEBUG("wrong page received (got %"PRIu32", have %"PRIu32")\n", page_rx, ctx->page);

        const sock_udp_ep_t bcast = {
            .family = AF_INET6,
            .addr = IPV6_ADDR_ALL_COAP_PAGE_LINK_LOCAL,
            .port = COAP_PORT,
            .netif = remote->netif,
        };

        switch (ctx->state) {
        case STATE_RX_WAITING:
        case STATE_RX:
            LOG_WARNING("upstrem is ahead, we are orphan now\n");
            ztimer_remove(ZTIMER_MSEC, &hdl->timer);
            sock_udp_set_remote(&hdl->upstream.udp, &bcast);
            ctx->state = STATE_ORPHAN;
            ctx->wait_blocks = CONFIG_NANOCOAP_PAGE_RETRIES;
            _request_missing(hdl, buf, len);
            break;
        case STATE_TX_WAITING:
        case STATE_TX:
            _request_slowdown(hdl, buf, len);
            break;
        default:
            break;
        }

        return 0;
    }

    /* new request */
    if (ctx->token_len == 0) {

        DEBUG("new request\n");
        hdl->offset_rx = 0;

        /* cleanup stale connection */
        nanocoap_sock_close(&hdl->upstream);

        /* store new token */
        ctx->token_len = coap_get_token_len(pkt);
        memcpy(ctx->token, coap_get_token(pkt), ctx->token_len);

        /* initialize timers */
        hdl->timer.callback = _timer_cb;
        hdl->timer.arg = ctx;
        hdl->event_timeout.handler = _timeout_event;
        hdl->event_page_done.handler = _page_done_event;
        hdl->resource = context->resource;

#ifdef MODULE_NANOCOAP_PAGE_FORWARD
        if (hdl->forward) {
            const char *path = coap_request_ctx_get_path(context);
            strncpy(hdl->path, path, sizeof(hdl->path));
            hdl->req.path = hdl->path;
            hdl->req.blksize = block1.szx;

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
    }

    switch (ctx->state) {
    case STATE_ORPHAN:
        sock_udp_set_remote(&hdl->upstream.udp, remote);
        DEBUG("re-connect upstream on %u\n", remote->netif);
        ctx->state = STATE_RX;
        break;
    case STATE_IDLE:
    case STATE_RX_WAITING:
        ctx->blocks_data = ndata_rx;
        ctx->blocks_fec = nfec_rx;
        ctx->is_last = !more_shards;
        hdl->blksize = block1.szx;
        bf_set_all(ctx->missing, blocks_per_shard);
#ifdef MODULE_NANOCOAP_PAGE_FEC
        _fec_init(ctx, &hdl->fec, block_len);
#endif
        ctx->state = STATE_RX;
        /* fall-through */
    case STATE_RX:
        break;
    case STATE_TX_WAITING:
    case STATE_TX:
        if (!memcmp(remote, &hdl->upstream.udp.remote, sizeof(*remote))) {
            _request_slowdown(hdl, buf, len);
        }
        return 0;
    }

    if (blocks_per_shard != ctx->blocks_data + ctx->blocks_fec) {
        DEBUG("unexpected change of page blocks\n");
        return 0;
    }

    DEBUG("got block %"PRIu32".%"PRIu32"/%"PRIu32" - %"PRIu32" left\n",
          page_rx, block1.blknum, blocks_per_shard - 1, blocks_left);

    /* we accept stray blocks, but can't take them into account for page timing */
    if (!memcmp(remote, &hdl->upstream.udp.remote, sizeof(*remote))) {
        /* set timeout for retransmission request */
        uint32_t timeout_ms = 1 + blocks_left * CONFIG_NANOCOAP_FRAME_GAP_MS
                            + (random_uint32() & 0x7);
        ztimer_set(ZTIMER_MSEC, &hdl->timer, timeout_ms);
        ctx->wait_blocks = CONFIG_NANOCOAP_PAGE_RETRIES;
    }

    if (!bf_isset(ctx->missing, block1.blknum)) {
        DEBUG("old block received\n");
        return 0;
    }

    /* store the new block in the current shard */
    bf_unset(ctx->missing, block1.blknum);
    memcpy(&ctx->work_buf[block_len * block1.blknum],
           pkt->payload, pkt->payload_len);

    /* check if there are any missing data blocks in the current shard */
    if (bf_find_first_set(ctx->missing, ctx->blocks_data) < 0) {
        DEBUG("page %"PRIu32" done%s, got %"PRIu32" blocks!\n",
              ctx->page, more_shards ? "" : "(last page)", blocks_per_shard);

        ztimer_remove(ZTIMER_MSEC, &hdl->timer);
        event_post(EVENT_PRIO_MEDIUM, &hdl->event_page_done);
    }

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
