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

#ifndef RANGE_TEST_H
#define RANGE_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t pkts_send;
    uint16_t pkts_rcvd;
    int32_t rssi_sum[2];
    uint32_t rtt_ticks;
} test_result_t;

void range_test_start(void);
bool range_test_set_next_modulation(void);
xtimer_ticks32_t range_test_get_timeout(kernel_pid_t netif);

void range_test_begin_measurement(kernel_pid_t netif);
void range_test_add_measurement(kernel_pid_t netif, int rssi_local, int rssi_remote, uint32_t ticks);
void range_test_print_results(void);

#endif
