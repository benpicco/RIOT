/*
 * Copyright (C) 2017 Koen Zandberg <koen@bergzand.net>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for
 * more details.
 */

/**
 * @ingroup     net_netstats
 * @brief       Records statistics about link layer neighbors
 * @{
 *
 * @file
 * @brief       Neighbor stats definitions
 *
 * @author      Koen Zandberg <koen@bergzand.net>
 */
#ifndef NET_NETSTATS_NEIGHBOR_H
#define NET_NETSTATS_NEIGHBOR_H

#include <string.h>
#include "net/netif.h"
#include "xtimer.h"
#include "timex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result of the transmission
 * @{
 */
typedef enum {
    NETSTATS_NB_BUSY,       /**< Failed due to medium busy */
    NETSTATS_NB_NOACK,      /**< Failed due to no ack received */
    NETSTATS_NB_SUCCESS,    /**< Successful transmission */
} netstats_nb_result_t;
/** @} */

/**
 * @name @ref EWMA parameters
 * @{
 */
#define NETSTATS_NB_EWMA_SCALE            100   /**< Multiplication factor of the EWMA */
#define NETSTATS_NB_EWMA_ALPHA             15   /**< Alpha factor of the EWMA */
#define NETSTATS_NB_EWMA_ALPHA_RAMP        30   /**< Alpha factor of the EWMA when stats are not fresh */
/** @} */

/**
 * @name @ref ETX parameters
 * @{
 */
#define NETSTATS_NB_ETX_NOACK_PENALTY       6   /**< ETX penalty for not receiving any ACK */
#define NETSTATS_NB_ETX_DIVISOR           128   /**< ETX fixed point divisor (rfc 6551) */
#define NETSTATS_NB_ETX_INIT                2   /**< Initial ETX, assume a mediocre link */
/** @} */

/**
 * @name @ref Freshness parameters
 * @{
 */
#define NETSTATS_NB_FRESHNESS_HALF        600   /**< seconds after the freshness counter is halved */
#define NETSTATS_NB_FRESHNESS_TARGET        4   /**< freshness count needed before considering the statistics fresh */
#define NETSTATS_NB_FRESHNESS_MAX          16   /**< Maximum freshness */
#define NETSTATS_NB_FRESHNESS_EXPIRATION 1200   /**< seconds after statistics have expired */
/** @} */
/**
 * @name @ref Timeout Parameters
 * @{
 */
#define NETSTATS_NB_TX_TIMEOUT_MS         100   /**< milliseconds without TX done notification after which
                                                     a TX event is discarded */
/** @} */

/**
 * @brief Initialize the neighbor stats
 *
 * @param[in] dev  ptr to netdev device
 *
 */
void netstats_nb_init(netif_t *dev);

/**
 * @brief Find a neighbor stat by the mac address.
 *
 * @param[in] dev       ptr to netdev device
 * @param[in] l2_addr   ptr to the L2 address
 * @param[in] len       length of the L2 address
 *
 * @return ptr to the statistics record, NULL if not found
 */
netstats_nb_t *netstats_nb_get(netif_t *dev, const uint8_t *l2_addr, uint8_t len);

/**
 * @brief Iterator over the recorded neighbors, returns the next non-zero record. NULL if none remaining
 *
 * @param[in] first     ptr to the first record
 * @param[in] prev      ptr to the previous record
 *
 * @return ptr to the record
 * @return NULL if no records remain
 */
netstats_nb_t *netstats_nb_get_next(netstats_nb_t *first, netstats_nb_t *prev);

/**
 * @brief Store this neighbor as next in the transmission queue.
 *
 * Set @p len to zero if a nop record is needed, for example if the
 * transmission has a multicast address as a destination.
 *
 * @param[in] dev       ptr to netdev device
 * @param[in] l2_addr   ptr to the L2 address
 * @param[in] len       length of the L2 address
 *
 */
void netstats_nb_record(netif_t *dev, const uint8_t *l2_addr, uint8_t len);

/**
 * @brief Update the next recorded neighbor with the provided numbers
 *
 * This only increments the statistics if the length of the l2-address of the retrieved record
 * is non-zero. See also @ref netstats_nb_record. The numbers indicate the number of transmissions
 * the radio had to perform before a successful transmission was performed. For example: in the case
 * of a single send operation needing 3 tries before an ACK was received, there are 2 failed
 * transmissions and 1 successful transmission.
 *
 * @param[in] dev      ptr to netdev device
 * @param[in] result   Result of the transmission
 * @param[in] transmissions  Number of times the packet was sent over the air
 *
 * @return ptr to the record
 */
netstats_nb_t *netstats_nb_update_tx(netif_t *dev, netstats_nb_result_t result, uint8_t transmissions);

/**
 * @brief Record rx stats for the l2_addr
 *
 * @param[in] dev          ptr to netdev device
 * @param[in] l2_addr ptr  to the L2 address
 * @param[in] l2_addr_len  length of the L2 address
 * @param[in] rssi         RSSI of the received transmission in abs([dBm])
 * @param[in] lqi          Link Quality Indication provided by the radio
 *
 * @return ptr to the updated record
 */
netstats_nb_t *netstats_nb_update_rx(netif_t *dev, const uint8_t *l2_addr,
                                     uint8_t l2_addr_len, uint8_t rssi, uint8_t lqi);

/**
 * @brief Check if a record is fresh
 *
 * Freshness half time is checked and updated before verifying freshness.
 *
 * @param[in] stats  ptr to the statistic
 */
bool netstats_nb_isfresh(netstats_nb_t *stats);

/**
 * @brief Compare the freshness of two records
 *
 * @param[in] a     ptr to the first record
 * @param[in] b     ptr to the second record
 * @param[in] now   current timestamp in seconds
 *
 * @return       ptr to the least fresh record
 */
static inline netstats_nb_t *netstats_nb_comp(const netstats_nb_t *a,
                                              const netstats_nb_t *b,
                                              uint16_t now)
{
    return (netstats_nb_t *)((now - a->last_updated > now - b->last_updated) ? a : b);
}

#ifdef __cplusplus
}
#endif

#endif /* NET_NETSTATS_NEIGHBOR_H */
/**
 * @}
 */
