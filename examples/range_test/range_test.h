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
} test_result_t;

bool range_test_set_modulation(unsigned idx);

void range_test_begin_measurement(unsigned idx);
void range_test_add_measurement(unsigned idx, int rssi_local, int rssi_remote);
void range_test_print_results(void);

#endif
