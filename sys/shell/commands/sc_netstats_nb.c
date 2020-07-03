/*
 * Copyright (C) Koen Zandberg <koen@bergzand.net>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_shell_commands
 * @{
 *
 * @file
 * @brief       Shell commands for displaying neighbor statistics
 *
 * @author      Koen Zandberg <koen@bergzand.net>
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 *
 * @}
 */

#include <stdio.h>

#include "net/gnrc/netif.h"
#include "net/netstats.h"
#include "net/netstats/neighbor.h"

static void _print_neighbors(netstats_nb_t *stats)
{
    unsigned header_len = 0;
    char l2addr_str[3 * CONFIG_L2ADDR_MAX_LEN];
    puts("Neighbor link layer stats:");
    header_len += printf("L2 address               fresh");
#ifdef MODULE_NETSTATS_NEIGHBOR_ETX
    header_len += printf("  etx");
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_COUNT
    header_len += printf(" sent received");
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_RSSI
    header_len += printf("   rssi ");
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_LQI
    header_len += printf(" lqi");
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_TX_TIME
    header_len += printf(" avg tx time");
#endif
    printf("\n");

    while (header_len--) {
        printf("-");
    }
    printf("\n");

    for (netstats_nb_t *entry = stats;
        entry != NULL;
        entry = netstats_nb_get_next(stats, entry)) {

        if (entry->l2_addr_len == 0) {
            continue;
        }

        printf("%-24s ",
               gnrc_netif_addr_to_str(entry->l2_addr, entry->l2_addr_len, l2addr_str));
        if (netstats_nb_isfresh(entry)) {
            printf("%5u", (unsigned)entry->freshness);
        } else {
            printf("STALE");
        }

#ifdef MODULE_NETSTATS_NEIGHBOR_ETX
        printf(" %3u%%", (100 * entry->etx) / NETSTATS_NB_ETX_DIVISOR);
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_COUNT
        printf(" %4"PRIu16" %8"PRIu16, entry->tx_count, entry->rx_count);
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_RSSI
        printf(" %4i dBm", (int8_t) entry->rssi);
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_LQI
        printf(" %u", entry->lqi);
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_TX_TIME
        printf(" %7"PRIu32" µs", entry->time_tx_avg);
#endif
        printf("\n");
    }
}

int _netstats_nb(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    gnrc_netif_t *netif = NULL;
    while ((netif = gnrc_netif_iter(netif))) {
        _print_neighbors(&netif->netif.pstats[0]);
    }

    return 0;
}
