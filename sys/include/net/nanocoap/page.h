/*
 * Copyright (C) 2023 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_nanosock
 * @brief       NanoCoAP blockwise NON-multicast
 *
 * @{
 *
 * @file
 * @brief       NanoCoAP blockwise NON-multicast
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */
#ifndef NET_NANOCOAP_PAGE_H
#define NET_NANOCOAP_PAGE_H

#include "bitfield.h"
#include "net/nanocoap_sock.h"
#include "ztimer.h"

#ifdef MODULE_NANOCOAP_PAGE_FEC_RS
#include "rs.h"
#endif
#include "coding/xor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Static initializer for the link-local all CoAP nodes multicast IPv6
 *          address (ff02::c0ab)
 *
 * @see <a href="http://tools.ietf.org/html/rfc4291#section-2.7">
 *          RFC 4291, section 2.7
 *      </a>
 */
#define IPV6_ADDR_ALL_COAP_PAGE_LINK_LOCAL {{ 0xff, 0x02, 0x00, 0x00, \
                                              0x00, 0x00, 0x00, 0x00, \
                                              0x00, 0x00, 0x00, 0x00, \
                                              0x00, 0x00, 0xc0, 0xab }}

#ifndef CONFIG_NANOCOAP_FRAME_GAP_MS
#define CONFIG_NANOCOAP_FRAME_GAP_MS            10
#endif
#ifndef CONFIG_NANOCOAP_SHARD_BLOCKS_PAYLOAD
#define CONFIG_NANOCOAP_SHARD_BLOCKS_PAYLOAD    10
#endif
#ifndef CONFIG_NANOCOAP_SHARD_BLOCKS_FEC
#define CONFIG_NANOCOAP_SHARD_BLOCKS_FEC         6
#endif
#define NANOCOAP_SHARD_BLOCKS_MAX           (CONFIG_NANOCOAP_SHARD_BLOCKS_PAYLOAD \
                                              + CONFIG_NANOCOAP_SHARD_BLOCKS_FEC)

/**
 * @brief   Number of resend requests for a page before giving up.
 *          We try to re-request an incomplete page n times, if we
 *          don't receive a page block after that, we give up.
 */
#ifndef CONFIG_NANOCOAP_PAGE_RETRIES
#define CONFIG_NANOCOAP_PAGE_RETRIES            (8)
#endif

#ifndef CONFIG_NANOCOAP_SHARD_XFER_TIMEOUT_SECS
#define CONFIG_NANOCOAP_SHARD_XFER_TIMEOUT_SECS (30)
#endif

#ifndef CONFIG_NANOCOAP_SHARD_PATH_MAX
#define CONFIG_NANOCOAP_SHARD_PATH_MAX          (32)
#endif

typedef enum {
    STATE_IDLE,     /**< nothing happened yet */
    STATE_RX,       /**< receiving a page     */
    STATE_RX_WAITING, /**< waiting for next page */
    STATE_TX,       /**< sending a page       */
    STATE_TX_WAITING, /**< waiting for continue */
    STATE_ORPHAN,   /**< upstream is ahead of us */
} nanocoap_page_state_t;

typedef enum {
    CODING_NONE,
    CODING_XOR,
    CODING_REED_SOLOMON,
} nanocoap_page_coding_type_t;

#ifdef MODULE_NANOCOAP_PAGE_FEC_RS
typedef struct {
    uint8_t rs_buf[reed_solomon_bufsize(CONFIG_NANOCOAP_SHARD_BLOCKS_PAYLOAD,
                                        CONFIG_NANOCOAP_SHARD_BLOCKS_FEC)];
    uint8_t *blocks[NANOCOAP_SHARD_BLOCKS_MAX];
} nanocoap_page_rs_ctx_t;
#endif

typedef struct {
    union {
        char dummy;
#ifdef MODULE_NANOCOAP_PAGE_FEC_RS
        nanocoap_page_rs_ctx_t rs;
#endif
    } ctx;
    nanocoap_page_coding_type_t type;
} nanocoap_page_coding_ctx_t;

static inline void *nanocoap_page_coding_ctx_get_rs(nanocoap_page_coding_ctx_t *ctx)
{
#ifdef MODULE_NANOCOAP_PAGE_FEC_RS
    return ctx->ctx.rs.rs_buf;
#else
    (void)ctx;
    return NULL;
#endif
}

typedef struct {
    uint32_t page;
    uint8_t work_buf[NANOCOAP_SHARD_BLOCKS_MAX *
                     coap_szx2size(CONFIG_NANOCOAP_BLOCKSIZE_DEFAULT)];
    uint8_t token[COAP_TOKEN_LENGTH_MAX];
    BITFIELD(missing, NANOCOAP_SHARD_BLOCKS_MAX);
    nanocoap_page_state_t state;
    bool is_last;
    uint8_t blocks_data;
    uint8_t blocks_fec;
    uint8_t token_len;
    uint8_t wait_blocks;
} nanocoap_page_ctx_t;

typedef void(*nanocoap_page_handler_cb_t)(void *buf, size_t len, size_t offset,
                                          bool more, coap_request_ctx_t *context);

typedef struct {
    nanocoap_sock_t *sock;
    const char *path;
    uint8_t blksize;
} coap_shard_request_ctx_t;

typedef struct coap_page_handler_ctx {
    nanocoap_page_ctx_t ctx;
    coap_shard_request_ctx_t req;
#ifdef MODULE_NANOCOAP_PAGE_FEC
    nanocoap_page_coding_ctx_t fec;
#endif
    nanocoap_sock_t upstream;
    ztimer_t timer;
    event_t event_timeout;
    event_t event_page_done;
    uint32_t timeout;
    uint32_t offset_rx;
    nanocoap_page_handler_cb_t cb;
#ifdef MODULE_NANOCOAP_PAGE_FORWARD
    nanocoap_sock_t downstream;
    char path[CONFIG_NANOCOAP_SHARD_PATH_MAX];
    mutex_t fwd_lock;
    bool forward;                   /**< forwarding enabled */
#endif
    const coap_resource_t *resource;    /**< cache associated CoAP resource */
    uint8_t blksize;
} coap_shard_handler_ctx_t;

/**
 * @brief Blockwise non-request helper struct
 */
typedef struct {
    nanocoap_page_ctx_t ctx;
    coap_shard_request_ctx_t req;
#ifdef MODULE_NANOCOAP_PAGE_FEC
    nanocoap_page_coding_ctx_t fec;
#endif
} coap_shard_request_t;

static inline void *nanocoap_page_req_get(coap_shard_request_t *req, size_t *len)
{
    nanocoap_page_ctx_t *ctx = &req->ctx;

    if (len) {
        *len = sizeof(ctx->work_buf);

        if (req->ctx.blocks_fec) {
            size_t block_size = coap_szx2size(req->req.blksize);

            if (IS_USED(MODULE_NANOCOAP_PAGE_FEC_XOR)) {
                uint8_t b = *len / ((CONFIG_CODING_XOR_CHECK_BYTES + 1) * block_size);
                req->ctx.blocks_fec = b;
                req->ctx.blocks_data = CONFIG_CODING_XOR_CHECK_BYTES * b;
            }

            size_t fec_len = req->ctx.blocks_fec * block_size;
            assert(*len > fec_len);

            *len -= fec_len;
        }
    }

    return ctx->work_buf;
}

int nanocoap_shard_put(coap_shard_request_t *req, const void *data, size_t data_len,
                       bool more);

ssize_t nanocoap_page_block_handler(coap_pkt_t *pkt, uint8_t *buf, size_t len,
                                    coap_request_ctx_t *context);

int nanocoap_shard_netif_join(const netif_t *netif);

int nanocoap_shard_netif_join_all(void);

int nanocoap_shard_set_forward(coap_shard_handler_ctx_t *hdl, unsigned netif, bool on);

#ifdef __cplusplus
}
#endif
#endif /* NET_NANOCOAP_PAGE_H */
/** @} */
