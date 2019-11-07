/*
 * Copyright (C) 2019 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Application to test different PHY modulations
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "thread.h"
#include "mutex.h"
#include "periph/rtt.h"
#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/udp.h"

#include "shell.h"
#include "shell_commands.h"
#include "range_test.h"

int at86rf215_debug(int argc, char** argv);

#define HELLO_TIMEOUT_US    (50*1000)
#define HELLO_RETRIES       (100)

#define TEST_PERIOD (5 * RTT_FREQUENCY)
#define TEST_PORT   (2323)
#define QUEUE_SIZE  (4)

enum {
    TEST_HELLO,
    TEST_HELLO_ACK,
    TEST_PING,
    TEST_PONG
};

typedef struct {
    uint8_t type;
    uint32_t now;
} test_hello_t;

typedef struct {
    uint8_t type;
    int8_t rssi;
    uint16_t seq_no;
    uint32_t ticks;
} test_pingpong_t;

static char test_server_stack[THREAD_STACKSIZE_MAIN];
static char test_sender_stack[GNRC_NETIF_NUMOF][THREAD_STACKSIZE_MAIN];

static volatile uint32_t last_alarm;

static void _rtt_alarm(void* ctx)
{
    last_alarm += TEST_PERIOD;
    rtt_set_alarm(last_alarm, _rtt_alarm, ctx);

    mutex_unlock(ctx);
}

static int _get_rssi(gnrc_pktsnip_t *pkt, kernel_pid_t *pid)
{
    gnrc_netif_hdr_t *netif_hdr;
    gnrc_pktsnip_t *netif = gnrc_pktsnip_search_type(pkt, GNRC_NETTYPE_NETIF);

    if (netif == NULL) {
        return 0;
    }

    netif_hdr = netif->data;

    if (pid) {
        *pid = netif_hdr->if_pid;
    }

    return netif_hdr->rssi;
}

static bool _udp_send(int netif, const ipv6_addr_t* addr, uint16_t port, const void* data, size_t len)
{
    gnrc_pktsnip_t *pkt_out;

    if (!(pkt_out = gnrc_pktbuf_add(NULL, data, len, GNRC_NETTYPE_UNDEF))) {
        return false;
    }
    if (!(pkt_out = gnrc_udp_hdr_build(pkt_out, port, port))) {
        goto error;
    }
    if (!(pkt_out = gnrc_ipv6_hdr_build(pkt_out, NULL, addr))) {
        goto error;
    }

    if (netif) {
        gnrc_pktsnip_t *netif_hdr = gnrc_netif_hdr_build(NULL, 0, NULL, 0);
        gnrc_netif_hdr_set_netif(netif_hdr->data, gnrc_netif_get_by_pid(netif));
        LL_PREPEND(pkt_out, netif_hdr);
    }

    return gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL, pkt_out);
error:
    gnrc_pktbuf_release(pkt_out);
    return false;
}

static bool _udp_reply(gnrc_pktsnip_t *pkt_in, void* data, size_t len)
{
    gnrc_pktsnip_t *snip_udp = pkt_in->next;
    gnrc_pktsnip_t *snip_ip  = snip_udp->next;
    gnrc_pktsnip_t *snip_if  = snip_ip->next;

    udp_hdr_t *udp = snip_udp->data;
    ipv6_hdr_t *ip = snip_ip->data;
    gnrc_netif_hdr_t *netif = snip_if->data;

    return _udp_send(netif->if_pid, &ip->src, byteorder_ntohs(udp->src_port), data, len);
}

static bool _send_ping(int netif, const ipv6_addr_t* addr, uint16_t port)
{
    test_pingpong_t ping = {
        .type = TEST_PING,
        .ticks = xtimer_now().ticks32
    };

    return _udp_send(netif, addr, port, &ping, sizeof(ping));
}

static kernel_pid_t sender_pid;
static bool _send_hello(int netif, const ipv6_addr_t* addr, uint16_t port)
{
    test_hello_t hello = {
        .type = TEST_HELLO,
    };

    sender_pid = thread_getpid();
    hello.now = rtt_get_counter();

    return _udp_send(netif, addr, port, &hello, sizeof(hello));
}

struct sender_ctx {
    bool running;
    mutex_t mutex;
    uint16_t netif;
};

static void* range_test_sender(void *arg)
{

    struct sender_ctx *ctx = arg;
    while (ctx->running) {

        mutex_lock(&ctx->mutex);

        if (!ctx->running) {
            break;
        }

        if (!_send_ping(ctx->netif, &ipv6_addr_all_nodes_link_local, TEST_PORT)) {
            puts("UDP send failed!");
            break;
        }

        range_test_begin_measurement(ctx->netif);

        mutex_unlock(&ctx->mutex);
//        printf("[%d] will sleep for %ld µs\n", ctx->netif, xtimer_usec_from_ticks(range_test_get_timeout(ctx->netif)));
        xtimer_tsleep32(range_test_get_timeout(ctx->netif));
    }

    return arg;
}

static int _range_test_cmd(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    mutex_t mutex = MUTEX_INIT_LOCKED;

    msg_t m;

    unsigned tries = HELLO_RETRIES;

    while (--tries) {
        _send_hello(0, &ipv6_addr_all_nodes_link_local, TEST_PORT);

        if (xtimer_msg_receive_timeout(&m, HELLO_TIMEOUT_US) > 0) {
            break;
        }
    }

    if (!tries) {
        puts("handshake failed");
        return -1;
    }

    printf("Handshake complete after %d tries\n", HELLO_RETRIES - tries);

    struct sender_ctx ctx[GNRC_NETIF_NUMOF];

    for (int i = 0; i < GNRC_NETIF_NUMOF; ++i) {
        mutex_init(&ctx[i].mutex);
        mutex_lock(&ctx[i].mutex);
        ctx[i].netif = 6 + i; // XXX
        ctx[i].running = true;
        thread_create(test_sender_stack[i], sizeof(test_sender_stack[i]),
                      THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                      range_test_sender, &ctx[i], "pinger");
    }

    last_alarm = rtt_get_counter() + TEST_PERIOD;
    rtt_set_alarm(last_alarm, _rtt_alarm, &mutex);

    range_test_start();

    do {
        for (int i = 0; i < GNRC_NETIF_NUMOF; ++i)
            mutex_unlock(&ctx[i].mutex);

        mutex_lock(&mutex);

        for (int i = 0; i < GNRC_NETIF_NUMOF; ++i)
            mutex_lock(&ctx[i].mutex);

        /* can't change the modulation if the radio is still sending */
        xtimer_usleep(250000);
    } while (range_test_set_next_modulation());


    for (int i = 0; i < GNRC_NETIF_NUMOF; ++i) {
        ctx[i].running = false;
        mutex_unlock(&ctx[i].mutex);
    }

    rtt_clear_alarm();

    range_test_print_results();

    xtimer_sleep(1);

    return 0;
}

#define CUSTOM_MSG_TYPE_NEXT_SETTING    (0x0001)

static void _rtt_next_setting(void* arg)
{
    gnrc_netreg_entry_t *ctx = arg;
    msg_t m = {
        .type = CUSTOM_MSG_TYPE_NEXT_SETTING
    };

    last_alarm += TEST_PERIOD;
    rtt_set_alarm(last_alarm, _rtt_next_setting, arg);

    msg_send(&m, ctx->target.pid);
}

static void* range_test_server(void *arg)
{
    msg_t msg, reply = {
        .type = GNRC_NETAPI_MSG_TYPE_ACK,
        .content.value = -ENOTSUP
    };

    gnrc_netreg_entry_t ctx = {
        .demux_ctx  = TEST_PORT,
        .target.pid = thread_getpid()
    };

    msg_t msg_queue[QUEUE_SIZE];

    /* setup the message queue */
    msg_init_queue(msg_queue, ARRAY_SIZE(msg_queue));

    /* register thread for UDP traffic on this port */
    gnrc_netreg_register(GNRC_NETTYPE_UDP, &ctx);

    puts("listening…");

    while (1) {
        msg_receive(&msg);
        gnrc_pktsnip_t *pkt = msg.content.ptr;

        test_hello_t *hello = pkt->data;
        test_pingpong_t *pp = pkt->data;

        /* handle netapi messages */
        switch (msg.type) {
        case GNRC_NETAPI_MSG_TYPE_SET:
        case GNRC_NETAPI_MSG_TYPE_GET:
            msg_reply(&msg, &reply);    /* fall-through */
        case GNRC_NETAPI_MSG_TYPE_SND:
            continue;
        case CUSTOM_MSG_TYPE_NEXT_SETTING:
            if (!range_test_set_next_modulation()) {
                rtt_clear_alarm();
                puts("Test done.");
            }
            continue;
        }

        switch (pp->type) {
        case TEST_HELLO:
            rtt_set_counter(hello->now);

            pp->type = TEST_HELLO_ACK;
            _udp_reply(pkt, pkt->data, pkt->size);

            range_test_start();
            last_alarm = rtt_get_counter() + TEST_PERIOD;
            rtt_set_alarm(last_alarm, _rtt_next_setting, &ctx);

            break;
        case TEST_HELLO_ACK:
            puts("got HELLO-ACK");
            rtt_set_counter(hello->now);
            msg_send(&msg, sender_pid);
            break;
        case TEST_PING:
            pp->type = TEST_PONG;
            pp->rssi = _get_rssi(pkt, NULL);
            _udp_reply(pkt, pkt->data, pkt->size);
            break;
        case TEST_PONG:
        {
            kernel_pid_t netif = 0;
            int rssi = _get_rssi(pkt, &netif);
            range_test_add_measurement(netif, rssi, pp->rssi, xtimer_now().ticks32 - pp->ticks);
            break;
        }
        default:
            printf("got '%s'\n", (char*) pkt->data);
        }

        gnrc_pktbuf_release(pkt);
    }

    return arg;
}

static int _do_ping(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    return !_send_ping(0, &ipv6_addr_all_nodes_link_local, TEST_PORT);
}

static const shell_command_t shell_commands[] = {
    { "range_test", "Iterates over radio settings", _range_test_cmd },
    { "ping", "send single ping to all nodes", _do_ping },
#ifdef MODULE_AT86RF215
    { "rf215", "at86rf215 debugging", at86rf215_debug },
#endif
    { NULL, NULL, NULL }
};

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

int main(void)
{
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    thread_create(test_server_stack, sizeof(test_server_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  range_test_server, NULL, "range test");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
