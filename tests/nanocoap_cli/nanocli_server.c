/*
 * Copyright (c) 2018 Ken Bannister. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       nanocoap test server
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "net/nanocoap_sock.h"
#include "net/netif.h"
#include "net/sock/udp.h"

#include "net/gnrc/ipv6/nib/ft.h"

#include "event/periodic_callback.h"
#include "event/thread.h"

#define ENABLE_DEBUG 0
#include "debug.h"

static char _server_stack[THREAD_STACKSIZE_DEFAULT];

extern void nanotest_enable_forward(unsigned netif, bool on);

/*
 * Customized implementation of nanocoap_server() to ignore a count of
 * requests. Allows testing confirmable messaging.
 */
static int _nanocoap_server(sock_udp_ep_t *local, uint8_t *buf, size_t bufsize,
                            int ignore_count)
{
    sock_udp_t sock;
    sock_udp_ep_t remote;

    if (!local->port) {
        local->port = COAP_PORT;
    }

    ssize_t res = sock_udp_create(&sock, local, NULL, 0);
    if (res == -1) {
        return -1;
    }

    int recv_count = 0;
    while (1) {
        res = sock_udp_recv(&sock, buf, bufsize, -1, &remote);
        if (++recv_count <= ignore_count) {
            DEBUG("ignoring request\n");
            continue;
        }
        if (res == -1) {
            DEBUG("nanocoap: error receiving coap request, \n");
            return -1;
        }
        else {
            coap_pkt_t pkt;
            coap_request_ctx_t ctx = {
                .remote = &remote,
            };

            if (coap_parse(&pkt, (uint8_t *)buf, res) < 0) {
                DEBUG("nanocoap: error parsing packet\n");
                continue;
            }
            if ((res = coap_handle_req(&pkt, buf, bufsize, &ctx)) > 0) {
                res = sock_udp_send(&sock, buf, res, &remote);
            }
        }
    }

    return 0;
}

static void _start_server(uint16_t port, int ignore_count)
{
    uint8_t buf[128];
    sock_udp_ep_t local = { .port=port, .family=AF_INET6 };
    _nanocoap_server(&local, buf, sizeof(buf), ignore_count);
}

typedef struct {
    int ignore_count;
    uint16_t port;
} nanotest_server_ctx_t;

static void *_server_thread(void *arg)
{
    nanotest_server_ctx_t *ctx = arg;

    printf("starting server on port %u\n", ctx->port);
    _start_server(ctx->port, ctx->ignore_count);

    /* server executes run loop; never reaches this point*/
    return NULL;
}

static void _downstream_check_cb(void *arg)
{
    gnrc_netif_t *downstream = gnrc_ipv6_nib_ft_iter_downstream(NULL);

    if (downstream) {
        printf("enable shard forwarding on %u\n", downstream->pid);
        nanotest_enable_forward(downstream->pid, true);
        event_periodic_callback_stop(arg);
    }
}

int nanotest_server_cmd(int argc, char **argv)
{
    if (argc < 2) {
        goto error;
    }

    if (strncmp("start", argv[1], 5) != 0) {
        goto error;
    }

    int arg_pos = 2;
    nanotest_server_ctx_t ctx = {
        .port = COAP_PORT,
    };

    if ((argc >= (arg_pos+1)) && (strcmp(argv[arg_pos], "-i") == 0)) {
        /* need count of requests to ignore*/
        if (argc == 3) {
            goto error;
        }
        arg_pos++;

        ctx.ignore_count = atoi(argv[arg_pos]);
        if (ctx.ignore_count <= 0) {
            puts("nanocli: unable to parse ignore_count");
            goto error;
        }
        arg_pos++;
    }

    if (argc == (arg_pos+1)) {
        ctx.port = atoi(argv[arg_pos]);
        if (ctx.port == 0) {
            puts("nanocli: unable to parse port");
            goto error;
        }
    }

    static event_periodic_callback_t downstream_cb;
    event_periodic_callback_init(&downstream_cb, ZTIMER_MSEC, EVENT_PRIO_MEDIUM,
                                 _downstream_check_cb, &downstream_cb);
    event_periodic_callback_start(&downstream_cb, 500);

    thread_create(_server_stack, sizeof(_server_stack),
                  THREAD_PRIORITY_MAIN, THREAD_CREATE_STACKTEST, _server_thread, &ctx,
                  "nanotest_server");
    ztimer_sleep(ZTIMER_MSEC, 100);
    return 0;

    error:
    printf("usage: %s start [-i ignore_count] [port]\n", argv[0]);
    printf("Options\n");
    printf("    -i  ignore a number of requests\n");
    printf("  port  defaults to %u\n", COAP_PORT);
    return 1;
}
