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

#include "shell.h"
#include "shell_commands.h"
#include "range_test.h"

#define PORT_TEST   (2323)
#define QUEUE_SIZE  (8)

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

static void _dump(gnrc_pktsnip_t *pkt)
{
    int snips = 0;
    int size = 0;
    gnrc_pktsnip_t *snip = pkt;

    printf("RSSI: %d\n", _get_rssi(pkt));

    while (snip != NULL) {
        printf("~~ SNIP %2i - size: %3u byte, type: %d\n", snips,
               (unsigned int)snip->size, snip->type);
        ++snips;
        size += snip->size;
        snip = snip->next;
    }
}

static void* range_test_server(void *arg)
{
    msg_t msg;
    msg_t msg_queue[QUEUE_SIZE];

    /* setup the message queue */
    msg_init_queue(msg_queue, ARRAY_SIZE(msg_queue));

    gnrc_netreg_entry_t ctx = {
        .demux_ctx  = PORT_TEST,
        .target.pid = thread_getpid()
    };
    gnrc_netreg_register(GNRC_NETTYPE_UDP, &ctx);

    puts("listening…");

    while (1) {
        msg_receive(&msg);

        printf("got one: %p\n", msg.content.ptr);

        _dump(msg.content.ptr);
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
