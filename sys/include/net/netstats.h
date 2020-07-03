/*
 * Copyright (C) 2016 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_netstats Packet statistics per module
 * @ingroup     net
 * @brief       Each module may store information about sent and received packets
 * @{
 *
 * @file
 * @brief       Definition of net statistics
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 */

#include <stdint.h>

#ifndef NET_NETSTATS_H
#define NET_NETSTATS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Max length of a L2 address
 */
#ifndef CONFIG_L2ADDR_MAX_LEN
#define CONFIG_L2ADDR_MAX_LEN   (8)
#endif

/**
 * @brief   The max number of entries in the peer stats table
 */
#ifndef NETSTATS_NB_SIZE
#define NETSTATS_NB_SIZE        (8)
#endif

/**
 * @brief   The CIB size for tx correlation
 */
#ifndef NETSTATS_NB_QUEUE_SIZE
#define NETSTATS_NB_QUEUE_SIZE  (4)
#endif

/**
 * @name @ref net_netstats module names
 * @{
 */
#define NETSTATS_LAYER2     (0x01)
#define NETSTATS_IPV6       (0x02)
#define NETSTATS_RPL        (0x03)
#define NETSTATS_ALL        (0xFF)
/** @} */

/**
 * @brief       Global statistics struct
 */
typedef struct {
    uint32_t tx_unicast_count;  /**< packets sent via unicast */
    uint32_t tx_mcast_count;    /**< packets sent via multicast
                                     (including broadcast) */
    uint32_t tx_success;        /**< successful sending operations
                                     (either acknowledged or unconfirmed
                                     sending operation, e.g. multicast) */
    uint32_t tx_failed;         /**< failed sending operations */
    uint32_t tx_bytes;          /**< sent bytes */
    uint32_t rx_count;          /**< received (data) packets */
    uint32_t rx_bytes;          /**< received bytes */
} netstats_t;

/**
 * @brief       Stats per peer struct
 */
typedef struct netstats_nb {
    uint8_t  l2_addr[CONFIG_L2ADDR_MAX_LEN]; /**< Link layer address of the neighbor */
    uint8_t  l2_addr_len;   /**< Length of netstats_nb::l2_addr */
    uint8_t  freshness;     /**< Freshness counter */
#ifdef MODULE_NETSTATS_NEIGHBOR_ETX
    uint16_t etx;           /**< ETX of this peer */
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_TX_TIME
    uint32_t time_tx_avg;   /**< Average frame TX time in µs */
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_RSSI
    uint8_t  rssi;          /**< Average RSSI of received frames in abs([dBm]) */
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_LQI
    uint8_t  lqi;           /**< Average LQI of received frames */
#endif
#ifdef MODULE_NETSTATS_NEIGHBOR_COUNT
    uint16_t tx_count;      /**< Number of sent frames to this peer */
    uint16_t rx_count;      /**< Number of received frames */
#endif
    uint16_t last_updated;  /**< seconds timestamp of last update */
    uint16_t last_halved;   /**< seconds timestamp of last halving */
} netstats_nb_t;

#ifdef __cplusplus
}
#endif

#endif /* NET_NETSTATS_H */
/** @} */
