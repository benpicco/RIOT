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
#include "periph/rtc.h"
#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/udp.h"

#include "shell.h"
#include "shell_commands.h"
#include "range_test.h"

#define PORT_TEST   (2323)
#define QUEUE_SIZE  (8)

enum {
    TEST_HELLO,
    TEST_PING,
    TEST_PONG
};

typedef struct {
    uint8_t type;
    int8_t rssi_recv;
    uint16_t seq_no;
} test_data_t;

static void _rtc_alarm(void* ctx)
{
    mutex_unlock(ctx);
}

static int _range_test_cmd(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    mutex_t mutex = MUTEX_INIT_LOCKED;

    struct tm alarm;
    rtc_get_time(&alarm);
    alarm.tm_sec += 10;
    rtc_set_alarm(&alarm, _rtc_alarm, &mutex);

    unsigned i = 0;
    do {
        mutex_lock(&mutex);

        alarm.tm_sec += 10;
        rtc_set_alarm(&alarm, _rtc_alarm, &mutex);
    } while (range_test_set_modulation(i++));

    rtc_clear_alarm();

    return 0;
}

static int _get_rssi(gnrc_pktsnip_t *pkt)
{
    gnrc_netif_hdr_t *netif_hdr;
    gnrc_pktsnip_t *netif = gnrc_pktsnip_search_type(pkt, GNRC_NETTYPE_NETIF);

    if (netif == NULL) {
        return 0;
    }

    netif_hdr = netif->data;
    return netif_hdr->rssi;
}

static bool _udp_reply(gnrc_pktsnip_t *pkt_in, void* data, size_t len)
{
    gnrc_pktsnip_t *pkt_out;

    gnrc_pktsnip_t *snip_udp = pkt_in->next;
    gnrc_pktsnip_t *snip_ip  = snip_udp->next;
    gnrc_pktsnip_t *snip_if  = snip_ip->next;

    udp_hdr_t *udp = snip_udp->data;
    ipv6_hdr_t *ip = snip_ip->data;
    gnrc_netif_hdr_t *netif = snip_if->data;

    if (!(pkt_out = gnrc_pktbuf_add(NULL, data, len, GNRC_NETTYPE_UNDEF))) {
        return false;
    }
    if (!(pkt_out = gnrc_udp_hdr_build(pkt_out,
                                       byteorder_ntohs(udp->dst_port),
                                       byteorder_ntohs(udp->src_port)))) {
        goto error;
    }
    if (!(pkt_out = gnrc_ipv6_hdr_build(pkt_out, &ip->dst, &ip->src))) {
        goto error;
    }

    /* make sure we send out the reply on the same interface */
    gnrc_pktsnip_t *netif_hdr = gnrc_netif_hdr_build(NULL, 0, NULL, 0);
    gnrc_netif_hdr_set_netif(netif_hdr->data, gnrc_netif_get_by_pid(netif->if_pid));
    LL_PREPEND(pkt_out, netif_hdr);

    return gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL, pkt_out);
error:
    gnrc_pktbuf_release(pkt_out);
    return false;
}

static void* range_test_server(void *arg)
{
    msg_t msg, reply = {
        .type = GNRC_NETAPI_MSG_TYPE_ACK,
        .content.value = -ENOTSUP
    };

    gnrc_netreg_entry_t ctx = {
        .demux_ctx  = PORT_TEST,
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

        /* handle netapi messages */
        switch (msg.type) {
        case GNRC_NETAPI_MSG_TYPE_SET:
        case GNRC_NETAPI_MSG_TYPE_GET:
            msg_reply(&msg, &reply);    /* fall-through */
        case GNRC_NETAPI_MSG_TYPE_SND:
            continue;
        }

        printf("'%s' (rssi: %d)\n", (char*) pkt->data, _get_rssi(pkt));

        if (!_udp_reply(pkt, pkt->data, pkt->size)) {
            puts("can't reply");
        }
    }

    return arg;
}

static const shell_command_t shell_commands[] = {
    { "range_test", "Iterates over radio settings", _range_test_cmd },
    { NULL, NULL, NULL }
};


#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

static char test_server_stack[THREAD_STACKSIZE_MAIN];

int main(void)
{
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    thread_create(test_server_stack, sizeof(test_server_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  range_test_server, NULL, "range test");

    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
