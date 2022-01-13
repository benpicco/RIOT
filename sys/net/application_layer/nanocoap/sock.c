/*
 * Copyright (C) 2016-18 Kaspar Schleiser <kaspar@schleiser.de>
 *               2018 Inria
 *               2018 Freie Universität Berlin
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
 * @brief       Nanocoap sock helpers
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "net/nanocoap_sock.h"
#include "net/sock/util.h"
#include "net/sock/udp.h"
#include "sys/uio.h"
#include "timex.h"

#define ENABLE_DEBUG 0
#include "debug.h"

typedef struct {
    coap_blockwise_cb_t callback;
    void *arg;
    bool more;
} _block_ctx_t;

int nanocoap_connect(sock_udp_t *sock, sock_udp_ep_t *local, sock_udp_ep_t *remote)
{
    if (!remote->port) {
        remote->port = COAP_PORT;
    }

    return sock_udp_create(sock, local, remote, 0);
}

enum {
    STATE_SEND_REQUEST,
    STATE_AWAIT_RESPONSE,
};

static bool _expect_response(const coap_pkt_t *pkt)
{
    switch (pkt->hdr->code) {
    case COAP_METHOD_PUT:
    case COAP_METHOD_DELETE:
        return false;
    }

    return true;
}

ssize_t nanocoap_request_cb(sock_udp_t *sock, coap_pkt_t *pkt, coap_request_cb_t cb, void *arg)
{
    ssize_t tmp, res = 0;
    size_t pdu_len = (pkt->payload - (uint8_t *)pkt->hdr) + pkt->payload_len;
    uint8_t *buf = (uint8_t*)pkt->hdr;
    uint32_t id = coap_get_id(pkt);
    void *payload, *ctx = NULL;

    unsigned state = STATE_SEND_REQUEST;

    /* TODO: timeout random between between ACK_TIMEOUT and (ACK_TIMEOUT *
     * ACK_RANDOM_FACTOR) */
    uint32_t timeout = CONFIG_COAP_ACK_TIMEOUT_MS * US_PER_MS;

    /* add 1 for initial transmit */
    unsigned tries_left = CONFIG_COAP_MAX_RETRANSMIT + 1;

    /* check if we expect a reply */
    const bool confirmable = coap_get_type(pkt) == COAP_TYPE_CON;
    const bool response = _expect_response(pkt);

    while (1) {
        switch (state) {
        case STATE_SEND_REQUEST:
            if (--tries_left == 0) {
                DEBUG("nanocoap: maximum retries reached\n");
                return -ETIMEDOUT;
            }

            res = sock_udp_send(sock, buf, pdu_len, NULL);
            if (res <= 0) {
                DEBUG("nanocoap: error sending coap request, %d\n", (int)res);
                return res;
            }

            if (confirmable || response) {
                state = STATE_AWAIT_RESPONSE;
            } else {
                return 0;
            }
            /* fall-through */
        case STATE_AWAIT_RESPONSE:
            tmp = sock_udp_recv_buf(sock, &payload, &ctx, timeout, NULL);
            if (tmp == 0) {
                /* no more data */
                return res;
            }
            res = tmp;
            if (res == -ETIMEDOUT) {
                DEBUG("nanocoap: timeout\n");
                timeout *= 2;
                tries_left--;
                state = STATE_SEND_REQUEST;
                break;
            }
            if (res < 0) {
                DEBUG("nanocoap: error receiving coap response, %d\n", (int)res);
                return res;
            }

            /* parse response */
            if (coap_parse(pkt, payload, res) < 0) {
                DEBUG("nanocoap: error parsing packet\n");
                res = -EBADMSG;
            }
            else if (coap_get_id(pkt) != id) {
                res = -EBADMSG;
                continue;
            }

            switch (coap_get_type(pkt)) {
            case COAP_TYPE_RST:
                /* TODO: handle different? */
                return -EBADMSG;
            case COAP_TYPE_CON:
                /* TODO: send ACK response */
                /* fall-through */
            case COAP_TYPE_NON:
                if (pkt->payload_len == 0) {
                    return 0;
                }
                /* fall-through */
            case COAP_TYPE_ACK:
                if (pkt->payload_len == 0 && response) {
                    state = STATE_AWAIT_RESPONSE;
                    /* TODO: adjust timeout */
                    break;
                }
                /* call user callback */
                res = cb(arg, pkt);
                break;
            }
        }
    }

    return res;
}

static int _request_cb(void *arg, coap_pkt_t *pkt)
{
    struct iovec *buf = arg;
    size_t pkt_len = coap_get_total_len(pkt);

    if (pkt_len > buf->iov_len) {
        return -ENOBUFS;
    }

    memcpy(buf->iov_base, pkt->hdr, pkt_len);

    pkt->hdr = buf->iov_base;
    pkt->token  = (uint8_t*)pkt->hdr + sizeof(coap_hdr_t);
    pkt->payload = (uint8_t*)pkt->hdr + (pkt_len - pkt->payload_len);

    return pkt_len;
}

ssize_t nanocoap_request(sock_udp_t *sock, coap_pkt_t *pkt, size_t len)
{
    struct iovec buf = {
        .iov_base = pkt->hdr,
        .iov_len  = len,
    };
    return nanocoap_request_cb(sock, pkt, _request_cb, &buf);
}

static int _get_cb(void *arg, coap_pkt_t *pkt)
{
    struct iovec *buf = arg;

    if (pkt->payload_len > buf->iov_len) {
        return -ENOBUFS;
    }

    memcpy(buf->iov_base, pkt->payload, pkt->payload_len);

    return pkt->payload_len;
}

ssize_t nanocoap_get(sock_udp_t *sock, const char *path, void *buf, size_t len)
{
    ssize_t res;
    coap_pkt_t pkt;
    uint8_t *pktpos = buf;

    struct iovec ctx = {
        .iov_base = buf,
        .iov_len  = len,
    };

    pkt.hdr = buf;
    pktpos += coap_build_hdr(pkt.hdr, COAP_TYPE_CON, NULL, 0, COAP_METHOD_GET, 1);
    pktpos += coap_opt_put_uri_path(pktpos, 0, path);
    pkt.payload = pktpos;
    pkt.payload_len = 0;

    res = nanocoap_request_cb(sock, &pkt, _get_cb, &ctx);
    if (res < 0) {
        return res;
    }

    if (coap_get_code(&pkt) != 205) {
        return -ENOENT;
    }

    return res;
}

static int _block_cb(void *arg, coap_pkt_t *pkt)
{
    _block_ctx_t *ctx = arg;

    coap_block1_t block2;
    coap_get_block2(pkt, &block2);

    ctx->more = block2.more;

    return ctx->callback(ctx->arg, block2.offset, pkt->payload, pkt->payload_len, block2.more);
}

static int _fetch_block(coap_pkt_t *pkt, sock_udp_t *sock,
                        const char *path, coap_blksize_t blksize, size_t num,
                        _block_ctx_t *ctx)
{
    uint8_t *pktpos = (void *)pkt->hdr;
    uint16_t lastonum = 0;

    pktpos += coap_build_hdr(pkt->hdr, COAP_TYPE_CON, NULL, 0, COAP_METHOD_GET,
                             num);
    pktpos += coap_opt_put_uri_pathquery(pktpos, &lastonum, path);
    pktpos += coap_opt_put_uint(pktpos, lastonum, COAP_OPT_BLOCK2,
                                (num << 4) | blksize);

    pkt->payload = pktpos;
    pkt->payload_len = 0;

    int res = nanocoap_request_cb(sock, pkt, _block_cb, ctx);
    if (res < 0) {
        return res;
    }

    res = coap_get_code(pkt);
    DEBUG("code=%i\n", res);
    if (res != 205) {
        return -res;
    }

    return 0;
}

int nanocoap_get_blockwise(sock_udp_t *sock, const char *path,
                           coap_blksize_t blksize,
                           coap_blockwise_cb_t callback, void *arg)
{
    uint8_t buf[CONFIG_NANOCOAP_BLOCK_HEADER_MAX];
    coap_pkt_t pkt = {
        .hdr = (void *)buf,
    };

    _block_ctx_t ctx = {
        .callback = callback,
        .arg = arg,
        .more = true,
    };

    size_t num = 0;
    while (ctx.more) {
        DEBUG("fetching block %u\n", (unsigned)num);
        int res = _fetch_block(&pkt, sock, path, blksize, num, &ctx);

        if (res) {
            DEBUG("error fetching block %u: %d\n", num, res);
            return -1;
        }

        num += 1;
    }

    return 0;
}

int nanocoap_get_blockwise_url(const char *url,
                               coap_blksize_t blksize,
                               coap_blockwise_cb_t callback, void *arg)
{
    char hostport[CONFIG_SOCK_HOSTPORT_MAXLEN];
    char urlpath[CONFIG_SOCK_URLPATH_MAXLEN];
    sock_udp_ep_t remote;
    sock_udp_t sock;
    int res;

    if (strncmp(url, "coap://", 7)) {
        DEBUG("nanocoap: URL doesn't start with \"coap://\"\n");
        return -EINVAL;
    }

    if (sock_urlsplit(url, hostport, urlpath) < 0) {
        DEBUG("nanocoap: invalid URL\n");
        return -EINVAL;
    }

    if (sock_udp_str2ep(&remote, hostport) < 0) {
        DEBUG("nanocoap: invalid URL\n");
        return -EINVAL;
    }

    res = nanocoap_connect(&sock, NULL, &remote);
    if (res) {
        return res;
    }

    res = nanocoap_get_blockwise(&sock, urlpath, blksize, callback, arg);
    nanocoap_close(&sock);

    return res;
}

int nanocoap_server(sock_udp_ep_t *local, uint8_t *buf, size_t bufsize)
{
    sock_udp_t sock;
    sock_udp_ep_t remote;

    if (!local->port) {
        local->port = COAP_PORT;
    }

    ssize_t res = sock_udp_create(&sock, local, NULL, 0);
    if (res != 0) {
        return -1;
    }

    while (1) {
        res = sock_udp_recv(&sock, buf, bufsize, -1, &remote);
        if (res < 0) {
            DEBUG("error receiving UDP packet %d\n", (int)res);
        }
        else if (res > 0) {
            coap_pkt_t pkt;
            if (coap_parse(&pkt, (uint8_t *)buf, res) < 0) {
                DEBUG("error parsing packet\n");
                continue;
            }
            if ((res = coap_handle_req(&pkt, buf, bufsize)) > 0) {
                sock_udp_send(&sock, buf, res, &remote);
            }
            else {
                DEBUG("error handling request %d\n", (int)res);
            }
        }
    }

    return 0;
}
