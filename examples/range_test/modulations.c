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
#include "net/gnrc.h"

#include "shell.h"
#include "shell_commands.h"
#include "range_test.h"

typedef struct {
    netopt_t opt;
    uint32_t data;
    size_t   data_len;
} netopt_val_t;

typedef struct {
    const char name[32];
    size_t opt_num;
    netopt_val_t opt[6];
} netopt_setting_t;

static const netopt_setting_t settings[] = {
    {
        .name = "OFDM-BPSKx4, opt=1",
        .opt =
        {
            {
                .opt  = NETOPT_IEEE802154_PHY,
                .data = IEEE802154_PHY_OFDM,
                .data_len = 1
            },
            {
                .opt  = NETOPT_OFDM_MCS,
                .data = 0,
                .data_len = 1
            },
            {
                .opt  = NETOPT_OFDM_OPTION,
                .data = 1,
                .data_len = 1
            },
        },
        .opt_num = 3
    },
    {
        .name = "OFDM-BPSKx4, opt=2",
        .opt =
        {
            {
                .opt  = NETOPT_OFDM_OPTION,
                .data = 2,
                .data_len = 1
            },
        },
        .opt_num = 1
    },
    {
        .name = "OFDM-BPSKx2, opt=1",
        .opt =
        {
            {
                .opt  = NETOPT_OFDM_MCS,
                .data = 1,
                .data_len = 1
            },
            {
                .opt  = NETOPT_OFDM_OPTION,
                .data = 1,
                .data_len = 1
            },
        },
        .opt_num = 2
    },
    {
        .name = "OFDM-BPSKx2, opt=2",
        .opt = {
            {
                .opt  = NETOPT_OFDM_OPTION,
                .data = 2,
                .data_len = 1
            },
        },
        .opt_num = 1
    },
    {
        .name = "OFDM-BPSKx2, opt=3",
        .opt = {
            {
                .opt  = NETOPT_OFDM_OPTION,
                .data = 3,
                .data_len = 1
            },
        },
        .opt_num = 1
    },
    {
        .name = "FSK, 50 kHz",
        .opt =
        {
            {
                .opt  = NETOPT_IEEE802154_PHY,
                .data = IEEE802154_PHY_FSK,
                .data_len = 1
            },
            {
                .opt  = NETOPT_FSK_SRATE,
                .data = 50,
                .data_len = 2
            },
        },
        .opt_num = 2
    },
    {
        .name = "FSK, 150 kHz",
        .opt =
        {
            {
                .opt  = NETOPT_FSK_SRATE,
                .data = 150,
                .data_len = 2
            },
        },
        .opt_num = 1
    },
    {
        .name = "FSK, 400 kHz",
        .opt =
        {
            {
                .opt  = NETOPT_FSK_SRATE,
                .data = 400,
                .data_len = 2
            },
        },
        .opt_num = 1
    },
    {
        .name = "O-QPSK, rate mode 4",
        .opt =
        {
            {
                .opt  = NETOPT_IEEE802154_PHY,
                .data = IEEE802154_PHY_OQPSK,
                .data_len = 1
            },
            {
                .opt  = NETOPT_OQPSK_RATE,
                .data = 4,
                .data_len = 1
            },
        },
        .opt_num = 2
    },
    {
        .name = "O-QPSK, rate mode 3",
        .opt =
        {
            {
                .opt  = NETOPT_OQPSK_RATE,
                .data = 3,
                .data_len = 1
            },
        },
        .opt_num = 1
    },
    {
        .name = "O-QPSK, rate mode 2",
        .opt =
        {
            {
                .opt  = NETOPT_OQPSK_RATE,
                .data = 2,
                .data_len = 1
            },
        },
        .opt_num = 1
    },
    {
        .name = "O-QPSK, rate mode 1",
        .opt =
        {
            {
                .opt  = NETOPT_OQPSK_RATE,
                .data = 1,
                .data_len = 1
            },
        },
        .opt_num = 1
    },
    {
        .name = "O-QPSK, rate mode 0",
        .opt =
        {
            {
                .opt  = NETOPT_OQPSK_RATE,
                .data = 0,
                .data_len = 1
            },
        },
        .opt_num = 1
    },
    {
        .name = "O-QPSK legacy",
        .opt =
        {
            {
                .opt  = NETOPT_OQPSK_RATE,
                .data = IEEE802154_OQPSK_FLAG_LEGACY,
                .data_len = 1
            },
        },
        .opt_num = 1
    },
};

static unsigned idx;
static test_result_t results[GNRC_NETIF_NUMOF][ARRAY_SIZE(settings)];

static void _netapi_set_forall(netopt_t opt, const void *data, size_t data_len)
{
    for (gnrc_netif_t *netif = gnrc_netif_iter(NULL); netif; netif = gnrc_netif_iter(netif)) {
        gnrc_netapi_set(netif->pid, opt, 0, data, data_len);
    }
}

void range_test_begin_measurement(kernel_pid_t netif)
{
    netif -= 6; // XXX
    results[netif][idx].pkts_send++;
    if (results[netif][idx].rtt_ticks == 0) {
        results[netif][idx].rtt_ticks = xtimer_ticks_from_usec(500000).ticks32;
    }
}

xtimer_ticks32_t range_test_get_timeout(kernel_pid_t netif)
{
    netif -= 6; // XXX
    xtimer_ticks32_t t = {
        .ticks32 = results[netif][idx].rtt_ticks + results[netif][idx].rtt_ticks / 10
    };

    return t;
}

void range_test_add_measurement(kernel_pid_t netif, int rssi_local, int rssi_remote, uint32_t ticks)
{
    netif -= 6; // XXX
    results[netif][idx].pkts_rcvd++;
    results[netif][idx].rssi_sum[0] += rssi_local;
    results[netif][idx].rssi_sum[1] += rssi_remote;
    results[netif][idx].rtt_ticks = (results[netif][idx].rtt_ticks + ticks) / 2;
}

void range_test_print_results(void)
{
    printf("modulation;iface;sent;received;RSSI_local;RSSI_remote;RTT\n");
    for (unsigned i = 0; i < ARRAY_SIZE(settings); ++i) {
        for (int j = 0; j < GNRC_NETIF_NUMOF; ++j) {
            xtimer_ticks32_t ticks = {
                .ticks32 = results[j][i].rtt_ticks
            };

            printf("\"%s\";", settings[i].name);
            printf("%d;", j);
            printf("%d;", results[j][i].pkts_send);
            printf("%d;", results[j][i].pkts_rcvd);
            printf("%ld;", results[j][i].rssi_sum[0] / results[j][i].pkts_rcvd);
            printf("%ld;", results[j][i].rssi_sum[1] / results[j][i].pkts_rcvd);
            printf("%ld\n", xtimer_usec_from_ticks(ticks));
        }
    }
    memset(results, 0, sizeof(results));
}

static void _set_modulation(void) {
    printf("switching to %s\n", settings[idx].name);

    for (unsigned j = 0; j < settings[idx].opt_num; ++j) {
        _netapi_set_forall(settings[idx].opt[j].opt, &settings[idx].opt[j].data, settings[idx].opt[j].data_len);
    }
}

bool range_test_set_next_modulation(void)
{
    if (++idx >= ARRAY_SIZE(settings)) {
        return false;
    }

    _set_modulation();

    return true;
}

void range_test_start(void)
{
    idx = 0;
    netopt_enable_t disable = NETOPT_DISABLE;
    _netapi_set_forall(NETOPT_ACK_REQ, &disable, sizeof(disable));

    _set_modulation();
}
