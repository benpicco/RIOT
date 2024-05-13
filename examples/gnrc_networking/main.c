/*
 * Copyright (C) 2015 Freie Universität Berlin
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
 * @brief       Example application for demonstrating the RIOT network stack
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "shell.h"
#include "msg.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#include "net/sock/udp.h"

static void _fill_buffer(void *buffer, size_t len)
{
    void *end = (char *)buffer + len;
    uint16_t *b16 = buffer;
    uint16_t count = 0;
    while ((void *)b16 < end) {
        *b16++ = count++;
    }
}

static int _udp_cmd(int argc, char **argv)
{
    if (argc < 2) {
        goto usage;
    }

    int len = atoi(argv[1]);
    if (!len) {
        goto usage;
    }

    if (len & 1) {
        ++len;
    }

    static uint8_t _buf[65536];
    if (len > (int)sizeof(_buf)) {
        puts("too big");
        return -ENOBUFS;
    }

    const sock_udp_ep_t remote = {
         .family = AF_INET6,
         .addr = IPV6_ADDR_ALL_NODES_LINK_LOCAL,
         .port = 1234,
    };

    _fill_buffer(_buf, len);
    int res = sock_udp_send(NULL, _buf, len, &remote);
    if (res > 0) {
        printf("%d bytes sent\n", res);
    } else {
        printf("error: %d\n", -res);
    }

    return 0;

usage:
    printf("usage: %s <bytes>\n", argv[0]);
    return 1;
}

SHELL_COMMAND(udp2, "send large UDP packets", _udp_cmd);

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
