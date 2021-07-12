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

#include "net/gcoap_helper.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

extern int udp_cmd(int argc, char **argv);

static const shell_command_t shell_commands[] = {
    { "udp", "send data over UDP and listen on UDP ports", udp_cmd },
    { NULL, NULL, NULL }
};

static char coap_thread_stack[THREAD_STACKSIZE_DEFAULT];
static void *coap_thread(void *ctx)
{
    (void)ctx;

    if (gnrc_netif_wait_for_prefix(NULL, 5 * US_PER_SEC)) {
        puts("got no prefix\n");
        return NULL;
    }

    uint32_t delay_us = 100 * US_PER_MS;
    while (true) {
        char respone[64];
        size_t len = sizeof(respone);
        if (coap_get("fdea:dbee:f::1", "/time", false, respone, &len) > 0) {
            puts(respone);

            /* hack to control interval from CoAP sever */
            if (len > strlen(respone)) {
                memcpy(&delay_us, &respone[len-4], sizeof(delay_us));
            }
        } else {
            puts("can't perform GET request");
        }

        xtimer_usleep(delay_us);
    }

    return NULL;
}

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    thread_create(coap_thread_stack, sizeof(coap_thread_stack),
                  THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                  coap_thread, NULL, "CoAP");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
