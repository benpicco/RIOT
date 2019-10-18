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
        .name = "OFDM-BPSKx4; opt=1",
        .opt = {
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
        .name = "OFDM-BPSKx4; opt=2",
        .opt = {
        {
            .opt  = NETOPT_OFDM_OPTION,
            .data = 2,
            .data_len = 1
        },
        },
        .opt_num = 1
    }
};

static void _rtc_alarm(void* ctx) {
    mutex_unlock(ctx);
}

static void _netapi_set_forall(netopt_t opt, const uint32_t *data, size_t data_len) {
    for (gnrc_netif_t *netif = gnrc_netif_iter(NULL); netif; netif = gnrc_netif_iter(netif)) {
        gnrc_netapi_set(netif->pid, opt, 0, data, data_len);
    }
}

static int _range_test_cmd(int argc, char** argv) {
    (void) argc;
    (void) argv;

    mutex_t mutex = MUTEX_INIT_LOCKED;

    struct tm alarm;
    rtc_get_time(&alarm);
    alarm.tm_sec += 10;
    rtc_set_alarm(&alarm, _rtc_alarm, &mutex);

    for (unsigned i = 0; i < ARRAY_SIZE(settings); ++i) {
        mutex_lock(&mutex);
        printf("using %s\n", settings[i].name);
        for (unsigned j = 0; j < settings[i].opt_num; ++j) {
            _netapi_set_forall(settings[i].opt[j].opt, &settings[i].opt[j].data, settings[i].opt[j].data_len);
        }

        alarm.tm_sec += 10;
        rtc_set_alarm(&alarm, _rtc_alarm, &mutex);
    }

    rtc_clear_alarm();

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "range_test", "Iterates over radio settings", _range_test_cmd },
    { NULL, NULL, NULL }
};

int main(void)
{
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
