/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
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
#ifndef NET_NANOCOAP_NON_BLOCK_H
#define NET_NANOCOAP_NON_BLOCK_H

#include "bitfield.h"
#include "net/nanocoap_sock.h"
#include "ztimer.h"

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
#define IPV6_ADDR_ALL_COAP_SHARD_LINK_LOCAL {{ 0xff, 0x02, 0x00, 0x00, \
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

#ifndef CONFIG_NANOCOAP_SHARD_XFER_TIMEOUT_SECS
#define CONFIG_NANOCOAP_SHARD_XFER_TIMEOUT_SECS (30)
#endif

#ifndef CONFIG_NANOCOAP_SHARD_PATH_MAX
#define CONFIG_NANOCOAP_SHARD_PATH_MAX          (32)
#endif

typedef enum {
    SHARD_STATE_EMPTY,
    SHARD_STATE_RX = 1,
    SHARD_STATE_TX = 2,
    SHARD_STATE_RX_AND_TX = 3,
} coap_shard_state_t;

typedef struct {
    uint8_t work_buf[NANOCOAP_SHARD_BLOCKS_MAX *
                     coap_szx2size(CONFIG_NANOCOAP_BLOCKSIZE_DEFAULT)];
    BITFIELD(missing, NANOCOAP_SHARD_BLOCKS_MAX);
    BITFIELD(to_send, NANOCOAP_SHARD_BLOCKS_MAX);
#ifdef MODULE_NANOCOAP_SHARD_DEBUG
    uint8_t transmits[NANOCOAP_SHARD_BLOCKS_MAX];
#endif
    uint32_t first_block;
    uint32_t next_first_block;  // can we get rid of this?
    uint8_t blocks_data;
    uint8_t blocks_fec;
    uint8_t state;
    bool is_last;
} coap_shard_ctx_t;

typedef struct {
    uint8_t token[COAP_TOKEN_LENGTH_MAX];
    coap_shard_ctx_t shards[2];
    uint32_t active_tx;             /**< currently active shard */
    uint32_t active_rx;             /**< currently active shard */
    uint32_t slowdown_deadline;
    uint8_t token_len;
} coap_shard_common_ctx_t;

typedef struct {
    nanocoap_sock_t *sock;
    const char *path;
    uint8_t blksize;
} coap_shard_request_ctx_t;

typedef struct {
    coap_shard_common_ctx_t ctx;
    coap_shard_request_ctx_t req;
    nanocoap_sock_t upstream;
    ztimer_t timer;
    event_t event_timeout;
    uint32_t timeout;
#ifdef MODULE_NANOCOAP_SHARD_FORWARD
    nanocoap_sock_t downstream;
    char path[CONFIG_NANOCOAP_SHARD_PATH_MAX];
    mutex_t fwd_lock;
    bool forward;                   /**< forwarding enabled */
#endif
    bool done;                      /**< transfer complete */
} coap_shard_handler_ctx_t;

/**
 * @brief Blockwise non-request helper struct
 */
typedef struct {
    coap_shard_common_ctx_t ctx;
    coap_shard_request_ctx_t req;
} coap_shard_request_t;

typedef struct {
    uintptr_t offset;
    void *data;
    size_t len;
    bool more;
} coap_shard_result_t;

static inline void *nanocoap_shard_req_get(coap_shard_request_t *req, size_t *len)
{
    coap_shard_common_ctx_t *state = &req->ctx;
    coap_shard_ctx_t *shard = &state->shards[state->active_tx & 1];

    if (len) {
        *len = sizeof(shard->work_buf);
    }

    return shard->work_buf;
}

int nanocoap_shard_put(coap_shard_request_t *req, const void *data, size_t data_len,
                             const void *fec, size_t fec_len, bool more);

ssize_t nanocoap_shard_block_handler(coap_pkt_t *pkt, uint8_t *buf, size_t len,
                                     coap_request_ctx_t *context,
                                     coap_shard_result_t *out);

int nanocoap_shard_netif_join(const netif_t *netif);

int nanocoap_shard_netif_join_all(void);

int nanocoap_shard_set_forward(coap_shard_handler_ctx_t *hdl, unsigned netif, bool on);

#ifdef __cplusplus
}
#endif
#endif /* NET_NANOCOAP_NON_BLOCK_H */
/** @} */
