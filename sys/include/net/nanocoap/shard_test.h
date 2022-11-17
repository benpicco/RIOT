/*
 * Copyright (C) 2022 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_nanosock
 * @brief       NanoCoAP shard debug functions
 *
 * @{
 *
 * @file
 * @brief       NanoCoAP shard debug functions
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 */
#ifndef NET_NANOCOAP_SHARD_TEST_H
#define NET_NANOCOAP_SHARD_TEST_H

#include "net/nanocoap_sock.h"
#include "net/nanocoap/shard.h"
#include "hashes/md5.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    coap_shard_handler_ctx_t super;
    int fd;
    md5_ctx_t md5;
} coap_shard_test_ctx_t;

ssize_t nanocoap_shard_block_handler_md5(coap_pkt_t *pkt, uint8_t *buf, size_t len,
                                         coap_request_ctx_t *context);

#ifdef __cplusplus
}
#endif
#endif /* NET_NANOCOAP_SHARD_TEST_H */
/** @} */
