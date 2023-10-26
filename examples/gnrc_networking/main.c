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

#include "net/gnrc/ipv6/firewall.h"
GNRC_IPV6_FIREWALL_ADD(my_rule) = {
    .flags = GNRC_IPV6_FW_FLAG_MATCH_SRC,
    .prio = 42,
};
GNRC_IPV6_FIREWALL_ADD(my_rule2) = {
    .flags = GNRC_IPV6_FW_FLAG_MATCH_DST,
    .prio = 23,
};
GNRC_IPV6_FIREWALL_ADD(my_rule3) = {
    .flags = GNRC_IPV6_FW_FLAG_MATCH_DST,
    .prio = 17,
};

extern gnrc_ipv6_firewall_rule_t gnrc_ipv6_firewall_rules_const;
extern gnrc_ipv6_firewall_rule_t gnrc_ipv6_firewall_rules_const_end;

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    printf("struct size: %u\n", sizeof(gnrc_ipv6_firewall_rule_t));
    printf("gnrc_ipv6_firewall_rules_const: %p\n", (void *)&gnrc_ipv6_firewall_rules_const);
    printf("my_rule: %p\n", (void *)&my_rule);
    printf("my_rule2: %p\n", (void *)&my_rule2);
    printf("my_rule3: %p\n", (void *)&my_rule3);
    printf("gnrc_ipv6_firewall_rules_const_end: %p\n", (void *)&gnrc_ipv6_firewall_rules_const_end);

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
