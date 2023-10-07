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

#include "event/periodic_callback.h"

#include "log.h"

#define ENABLE_DEBUG 0
//#include "debug.h"
#define DEBUG(...) do { if (ENABLE_DEBUG) { log_debug_write(__VA_ARGS__); } } while (0)

#include <stdarg.h>
#include <fcntl.h>
#include "periph/rtc.h"
#include "vfs_default.h"

static char addr_str[IPV6_ADDR_MAX_STR_LEN];
static char *_remote_to_string(const nanocoap_sock_t *sock)
{
    return ipv6_addr_to_str(addr_str, (ipv6_addr_t *)&sock->udp.remote.addr, sizeof(addr_str));
}

static unsigned _get_node_id(void)
{
    extern pid_t _native_id;
    return _native_id;
}

static void _fd_write(int log_fd, const char *format, va_list args)
{
    static bool print_timestamp = true;
    static char buffer[128];
    int res;

    if (print_timestamp) {
        struct tm now;
        uint16_t ms;
        rtc_get_time_ms(&now, &ms);

        res = snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d| ",
                       now.tm_hour, now.tm_min, now.tm_sec, ms);
        vfs_write(log_fd, buffer, res);
    }

    res = vsnprintf(buffer, sizeof(buffer), format, args);

    vfs_write(log_fd, buffer, res);

    print_timestamp = strchr(format, '\n');
}

static void log_fd_write(const char *format, ...)
{
    static int log_fd;
    if (log_fd <= 0) {
        log_fd = vfs_open(VFS_DEFAULT_DATA "/coap.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    va_list args;
    va_start(args, format);
    _fd_write(log_fd, format, args);
    va_end(args);
}

static void log_debug_write(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    _fd_write(STDOUT_FILENO, format, args);
    va_end(args);
}

static const char *_state_to_string(nanocoap_page_state_t state)
{
    switch (state) {
    case STATE_IDLE:
        return "I";
    case STATE_RX:
        return "RX";
    case STATE_RX_WAITING:
        return "rw";
    case STATE_TX:
        return "TX";
    case STATE_TX_WAITING:
        return "tw";
    case STATE_ORPHAN:
        return "O";
    default:
        return "??";
    }
}

static void _debug_event_cb(void *ctx)
{
    nanocoap_page_ctx_t *page_ctx = ctx;
    static char buffer[128];

    if (ctx == NULL) {
        return;
    }

    static int log_fd;
    if (log_fd <= 0) {
        log_fd = vfs_open(VFS_DEFAULT_DATA "/page.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    if (log_fd < 0) {
        return;
    }

    coap_shard_handler_ctx_t *hdl = container_of(page_ctx, coap_shard_handler_ctx_t, ctx);
    struct tm now;
    int res;

    rtc_get_time(&now);
    res = snprintf(buffer, sizeof(buffer), "n%02u\t%02d:%02d:%02d\t%u\t%s\t%s\n",
                   _get_node_id(),
                   now.tm_hour, now.tm_min, now.tm_sec,
                   page_ctx->page,
                   _remote_to_string(&hdl->upstream),
                   _state_to_string(page_ctx->state));
    vfs_write(log_fd, buffer, res);
}

static event_periodic_callback_t _debug_log_event;
static void _debug_output_set_ctx(void *ctx)
{
    _debug_log_event.event.arg = ctx;
}

static void _debug_output_init(void)
{
    event_periodic_callback_init(&_debug_log_event, ZTIMER_MSEC, EVENT_PRIO_MEDIUM,
                                 _debug_event_cb, NULL);
    event_periodic_callback_start(&_debug_log_event, 1000);
}

static bool _is_sending;

static void _request_slowdown(coap_shard_handler_ctx_t *hdl, uint8_t *buf, size_t len);

static bool _addr_match(const coap_shard_handler_ctx_t *hdl, const sock_udp_ep_t *remote)
{
    return !memcmp(&remote->addr, &hdl->upstream.udp.remote.addr, sizeof(remote->addr));
}

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
            LOG_ERROR("[%x] lost blocks can't be satisfied (want %u blocks from shard %"PRIu32", have shard %"PRIu32")\n",
                      coap_get_id(pkt), bf_popcnt(pkt->payload, shard_blocks), shard_num, ctx->page);
            break;
        }

        bf_or_atomic(ctx->missing, ctx->missing, pkt->payload, shard_blocks);
        DEBUG("[%x] neighbor re-requested %u blocks, total to send: %u\n",
              coap_get_id(pkt), bf_popcnt(pkt->payload, shard_blocks), bf_popcnt(ctx->missing, shard_blocks));

        ctx->state = STATE_TX;
        break;
    case 429:   /* too many requests */
        DEBUG("neighbor requested slowdown (still has %u blocks of page %"PRIu32" to send) we are at %"PRIu32"\n",
              unaligned_get_u16(pkt->payload), shard_num, ctx->page);

        if (shard_num + 1 < ctx->page) {
            DEBUG("neighbor is too far behind, ignore request\n");
            return NANOCOAP_SOCK_RX_AGAIN;
        }

        ctx->state = STATE_TX_WAITING;
        ctx->wait_blocks = MAX(ctx->wait_blocks, shard_blocks + unaligned_get_u16(pkt->payload));

        /* we have to re-transmit all blocks if neighbor is not yet ready to receive them */
        if (shard_num != ctx->page) {
            bf_set_all(ctx->missing, shard_blocks);
        }

        /* upstream should slowdown too */
        if (!_is_sending) {
            uint8_t buffer[32];
            coap_shard_handler_ctx_t *hdl = container_of(ctx, coap_shard_handler_ctx_t, ctx);
            _request_slowdown(hdl, buffer, sizeof(buffer));
        }

        return -EAGAIN;
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

    int res = 0;
    bool more_shards = !ctx->is_last;
    uint16_t id = nanocoap_sock_next_msg_id(req->sock);

    uint8_t buf[CONFIG_NANOCOAP_BLOCK_HEADER_MAX];
    iolist_t snip = {
        .iol_base = &ctx->work_buf[i * len],
        .iol_len  = len,
    };


        /* build CoAP header */
        coap_pkt_t pkt = {
            .hdr = (void *)buf,
            .snips = &snip,
        };

        uint8_t *pktpos = (void *)pkt.hdr;
        uint16_t lastonum = 0;

        bf_unset(ctx->missing, i);

        pktpos += coap_build_hdr(pkt.hdr, COAP_TYPE_NON, ctx->token, ctx->token_len,
                                 COAP_METHOD_PUT, id);
        pktpos += coap_opt_put_uri_pathquery(pktpos, &lastonum, req->path);
        pktpos += coap_opt_put_uint(pktpos, lastonum, COAP_OPT_BLOCK1,
                                    (i << 4) | req->blksize | (more ? 0x8 : 0));
        pktpos += coap_opt_put_page(pktpos, COAP_OPT_BLOCK1, ctx->page, ctx->blocks_data,
                                    ctx->blocks_fec, ctx->missing, more_shards);

        /* all ACK responses (2.xx, 4.xx and 5.xx) are ignored */
        pktpos += coap_opt_put_uint(pktpos, COAP_OPT_PAGE, COAP_OPT_NO_RESPONSE, 26);

        /* set payload marker */
        *pktpos++ = 0xFF;
        pkt.payload = pktpos;

        DEBUG("send block %"PRIu32".%u (%u / %u left)\n", ctx->page, i, blocks_left, total_blocks);
        nanocoap_sock_send_pkt(req->sock, &pkt);

    nanocoap_response_state_t state = 0;
    uint32_t deadline_ms = ztimer_now(ZTIMER_MSEC) + CONFIG_NANOCOAP_FRAME_GAP_MS;

    if (blocks_left == 0) {
        ctx->state = STATE_TX_WAITING;
        res = -EAGAIN;
        ctx->wait_blocks = 2 * total_blocks;
    }

    do {
        uint32_t timeout_us;

        if (res == -EAGAIN) {
            /* downstream node requested slowdown */
//            ctx->wait_blocks *= 2;
//            ctx->wait_blocks = (ctx->wait_blocks * 3) / 2;
            deadline_ms = ztimer_now(ZTIMER_MSEC)
                        + ctx->wait_blocks * CONFIG_NANOCOAP_FRAME_GAP_MS;
            DEBUG("wait blocks: %u\n", ctx->wait_blocks);
            ctx->wait_blocks = total_blocks; // hack - don't reuse this var!
        }

        timeout_us = _deadline_left_us(deadline_ms);
        res = nanocoap_sock_handle_response(req->sock, id, ctx->token, ctx->token_len,
                                            _block_resp_cb, ctx, timeout_us,
                                            &state);
        if (blocks_left == 0) {
            DEBUG("state = %d, res = %d, timeout = %lu µs\n", state, res, timeout_us);
        }
    } while (state != NANOCOAP_RESPONSE_TIMEOUT);
    ctx->state = STATE_TX;

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

    _debug_output_set_ctx(ctx);

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
        _debug_output_set_ctx(NULL);
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
    nanocoap_page_ctx_t *ctx = &hdl->ctx;

    while (hdl->forward) {
        mutex_lock(&hdl->fwd_lock);

        DEBUG("start forwarding page %"PRIu32"\n", ctx->page);
        if (_shard_put(&hdl->ctx, &hdl->req)) {

            if (ctx->is_last) {
                DEBUG("forwarding done\n");
                ctx->state = STATE_IDLE;
                ctx->token_len = 0;
                ctx->page = 0;
            } else {
                /* hack: disable forwarding - branch not used currently */
                hdl->forward = false;
                nanocoap_sock_close(hdl->req.sock);
                hdl->req.sock = NULL;
                ctx->state = STATE_RX_WAITING;
            }

            break;
        } else {
            uint8_t blocks_per_shard = ctx->blocks_data + ctx->blocks_fec;
            ctx->state = STATE_RX_WAITING;
            ctx->wait_blocks = CONFIG_NANOCOAP_PAGE_RETRIES;
            bf_set_all(ctx->missing, blocks_per_shard);
            event_post(EVENT_PRIO_MEDIUM, &hdl->event_timeout);
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

    int res = nanocoap_sock_connect(&hdl->downstream, NULL, &remote);
    if (res == 0) {
        DEBUG("enable forwarding\n");
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

    DEBUG("request slowdown (%u blocks of page %u left to send)\n", blocks_left, ctx->page);

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

static bool _request_missing(coap_shard_handler_ctx_t *hdl, uint8_t *buf, size_t len)
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
        DEBUG("wrong state (%x) to request missing blocks\n", ctx->state);
        return false;
    }

    unsigned missing = bf_popcnt(ctx->missing, shard_blocks);

    if (missing == 0) {
        DEBUG("page %"PRIu32" already complete\n", ctx->page);
        return true;
    }

    if (_fec_decode(hdl)) {
        DEBUG("reconstructed all missing blocks\n");
        event_post(EVENT_PRIO_MEDIUM, &hdl->event_page_done);
        return true;
    }

    DEBUG("[%x] re-request page %"PRIu32" (%u blocks)\n",
          id, ctx->page, bf_popcnt(ctx->missing, shard_blocks));
    pktpos += coap_build_hdr(pkt.hdr, COAP_TYPE_NON, ctx->token, ctx->token_len,
                             COAP_CODE_REQUEST_ENTITY_INCOMPLETE, id);
    /* set payload marker */
    *pktpos++ = 0xFF;

    /* TODO: use CBOR */
    memcpy(pktpos, &ctx->page, sizeof(ctx->page));

    pkt.payload = pktpos;
    pkt.payload_len = sizeof(ctx->page);

    nanocoap_sock_request_cb(&hdl->upstream, &pkt, NULL, NULL);

    hdl->req_sent = true;

    /* set timeout for retransmission request */
    uint32_t timeout_ms = ((2 * shard_blocks) / 3) * CONFIG_NANOCOAP_FRAME_GAP_MS
                        + (random_uint32() & 0x1F) + 1;
    ztimer_set(ZTIMER_MSEC, &hdl->timer, timeout_ms);

    return false;
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

    if (ctx->state != STATE_RX) {
        DEBUG("stale page done event\n");
        return;
    }

    DEBUG("page done event\n");

    assert(hdl->cb);
    hdl->cb(ctx->work_buf, ctx->blocks_data * block_len, hdl->offset_rx,
            !ctx->is_last, &context);

    if (ctx->is_last) {
        DEBUG("last page done\n");
        nanocoap_sock_close(&hdl->upstream);
        if (!_do_forward(hdl)) {
            ctx->state = STATE_IDLE;
            ctx->token_len = 0;
            ctx->page = 0;
        }
    } else {
        hdl->offset_rx += ctx->blocks_data * block_len;
        if (!_do_forward(hdl)) {
            ctx->state = STATE_RX_WAITING;
            ++ctx->page;
            bf_set_all(ctx->missing, blocks_per_shard);
            DEBUG("new page: %u\n", ctx->page);
        }
    }

#ifdef MODULE_NANOCOAP_PAGE_FORWARD
    if (_do_forward(hdl)) {
        DEBUG("entering forwarding state\n");

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

static uint8_t _missing_blocks(const uint8_t *missing, const uint8_t *to_send, uint8_t size, uint8_t cur)
{
    uint8_t tmp[4];
    bf_and(tmp, missing, to_send, size);

    /* previous blocks might as well have been missed */
    for (unsigned i = 0; i <= cur; ++i) {
        if (bf_isset(missing, i)) {
            bf_set(tmp, i);
        }
    }

    return bf_popcnt(missing, size) - bf_popcnt(tmp, size);
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
    uint8_t *to_send;
    int more_shards = coap_get_page(pkt, &page_rx, &ndata_rx, &nfec_rx, &to_send);

    if (more_shards < 0) {
        DEBUG("no page option\n");
        return 0;
    }

    blocks_per_shard = ndata_rx + nfec_rx;
    blocks_left = bf_popcnt(to_send, blocks_per_shard);

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
        if (!_addr_match(hdl, remote)) {
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
            LOG_WARNING("upstream is ahead, we are orphan now\n");
            sock_udp_ep_t tmp;
            sock_udp_get_remote(&hdl->upstream.udp, &tmp);

            ztimer_remove(ZTIMER_MSEC, &hdl->timer);
            sock_udp_set_remote(&hdl->upstream.udp, &bcast);
            ctx->state = STATE_ORPHAN;
            ctx->wait_blocks = CONFIG_NANOCOAP_PAGE_RETRIES;
            if (_request_missing(hdl, buf, len) && (page_rx <= ctx->page + 1)) {
                sock_udp_set_remote(&hdl->upstream.udp, &tmp);
                DEBUG("no longer orphan\n");
            }
            break;
        case STATE_TX_WAITING:
        case STATE_TX:
            if (page_rx <= ctx->page + 1) {
                _request_slowdown(hdl, buf, len);
            } else {
                /* TODO: properly handle this */
                LOG_WARNING("upstrem is ahead, still sending old page!\n");
                ctx->state = STATE_ORPHAN;
            }
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

        event_timeout_ztimer_init(&hdl->page_done_delay, ZTIMER_MSEC,
                                  EVENT_PRIO_MEDIUM, &hdl->event_page_done);

#ifdef MODULE_NANOCOAP_PAGE_FORWARD
        const char *path = coap_request_ctx_get_path(context);
        strncpy(hdl->path, path, sizeof(hdl->path));
        hdl->req.path = hdl->path;
        hdl->req.blksize = block1.szx;
#endif
        DEBUG("connect upstream on %u\n", remote->netif);
        nanocoap_sock_connect(&hdl->upstream, NULL, remote);

        /* start collecting data */
        _debug_output_set_ctx(ctx);
    }

    switch (ctx->state) {
    case STATE_ORPHAN:
        sock_udp_set_remote(&hdl->upstream.udp, remote);
        DEBUG("re-connect upstream on %u\n", remote->netif);
        ctx->wait_blocks = CONFIG_NANOCOAP_PAGE_RETRIES;
        ctx->state = STATE_RX;
        event_post(EVENT_PRIO_MEDIUM, &hdl->event_timeout);
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
        ctx->wait_blocks = CONFIG_NANOCOAP_PAGE_RETRIES;
        ctx->state = STATE_RX;
        /* fall-through */
    case STATE_RX:
        break;
    case STATE_TX_WAITING:
    case STATE_TX:
        if (_addr_match(hdl, remote)) {
            /* we are still sending the current frame */
            _request_slowdown(hdl, buf, len);
        }
        return 0;
    }

    if (blocks_per_shard != ctx->blocks_data + ctx->blocks_fec) {
        DEBUG("unexpected change of page blocks\n");
        return 0;
    }

    DEBUG("got block %"PRIu32".%"PRIu32"/%"PRIu32" - %"PRIu32" left to send",
          page_rx, block1.blknum, blocks_per_shard - 1, blocks_left);

    /* switch upstream if it is fresh or our upstream does not satisfy requests */
    bool addr_match = _addr_match(hdl, remote);
    bool fresh_block = bf_isset(ctx->missing, block1.blknum);

    if (!addr_match) {

        ++hdl->foreign_blocks;
        if (fresh_block && (hdl->foreign_blocks > 3)) {
            ++hdl->should_switch;
        } else if (hdl->req_sent && (hdl->foreign_blocks > 6)) {
            ++hdl->should_switch;
        } else if (hdl->foreign_blocks > 10) {
            ++hdl->should_switch;
        }

        if (hdl->should_switch) {
            DEBUG(" SWITCH UPSTREAM\n");
            sock_udp_set_remote(&hdl->upstream.udp, remote);
            addr_match = true;
            hdl->should_switch = 0;
        }
    }

    /* we accept stray blocks, but can't take them into account for page timing */
    if (addr_match) {
        uint8_t missing = 0;
        hdl->foreign_blocks = 0;

        if (hdl->req_sent) {
            hdl->req_sent = false;
            missing = _missing_blocks(ctx->missing, to_send, blocks_per_shard, block1.blknum);

            DEBUG(" %u blocks were ignored by upstream, %u skipped\n", missing, block1.blknum);
            if (missing) {
                ++hdl->should_switch;
            }
        }

        /* set timeout for retransmission request */
        uint32_t timeout_ms;
        if (missing) {
            timeout_ms = (random_uint32() & 0x3) + 1;
        } else {
            timeout_ms = 1 + blocks_left * CONFIG_NANOCOAP_FRAME_GAP_MS
                         + (random_uint32() & 0x3);
        }
        ztimer_set(ZTIMER_MSEC, &hdl->timer, timeout_ms);
        ctx->wait_blocks = CONFIG_NANOCOAP_PAGE_RETRIES;
        DEBUG(" (retry in %"PRIu32" ms)", timeout_ms);
    } else {
        DEBUG(" (%uth block of foreign upstream)", hdl->foreign_blocks);
    }

    if (!fresh_block) {
        DEBUG(" - old block received\n");
        return 0;
    }

    /* store the new block in the current shard */
    bf_unset(ctx->missing, block1.blknum);
    memcpy(&ctx->work_buf[block_len * block1.blknum],
           pkt->payload, pkt->payload_len);

    /* check if there are any missing data blocks in the current shard */
    if (bf_find_first_set(ctx->missing, ctx->blocks_data) < 0) {
        DEBUG("\npage %"PRIu32" done%s, got %"PRIu32" blocks!\n",
              ctx->page, more_shards ? "" : "(last page)", blocks_per_shard);

        ztimer_remove(ZTIMER_MSEC, &hdl->timer);

        if (!ztimer_is_set(hdl->page_done_delay.clock, &hdl->page_done_delay.timer)) {
            event_timeout_set(&hdl->page_done_delay, blocks_left * CONFIG_NANOCOAP_FRAME_GAP_MS);
        }
//        event_post(EVENT_PRIO_MEDIUM, &hdl->event_page_done);
    } else {
        DEBUG(" - %"PRIu32" still missing\n", bf_popcnt(ctx->missing, ctx->blocks_data));
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
    _debug_output_init();

    netif_t *netif = NULL;
    while ((netif = netif_iter(netif))) {
        int res = nanocoap_shard_netif_join(netif);
        if (res < 0) {
            return res;
        }
    }

    return 0;
}
