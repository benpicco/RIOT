/*
 * Copyright (C) Koen Zandberg <koen@bergzand.net>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net
 * @file
 * @brief       Neighbor level stats for netdev
 *
 * @author      Koen Zandberg <koen@bergzand.net>
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 * @}
 */

#include <errno.h>

#include "net/netdev.h"
#include "net/netstats/neighbor.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static bool l2_addr_equal(const uint8_t *a, uint8_t a_len, const uint8_t *b, uint8_t b_len)
{
    if (a_len != b_len) {
        return false;
    }

    return memcmp(a, b, a_len) == 0;
}

static void netstats_nb_half_freshness(netstats_nb_t *stats, timex_t *cur)
{
    uint16_t now = cur->seconds;
    uint8_t diff = (now - stats->last_halved) / NETSTATS_NB_FRESHNESS_HALF;
    stats->freshness >>= diff;

    if (diff) {
        /* Set to the last time point where this should have been halved */
        stats->last_halved = now - diff;
    }
}

static void netstats_nb_incr_freshness(netstats_nb_t *stats)
{
    timex_t cur;
    xtimer_now_timex(&cur);

    /* First halve the freshness if applicable */
    netstats_nb_half_freshness(stats, &cur);

    /* Increment the freshness capped at FRESHNESS_MAX */
    if (stats->freshness < NETSTATS_NB_FRESHNESS_MAX) {
        stats->freshness++;
    }

    stats->last_updated = cur.seconds;
}

bool netstats_nb_isfresh(netstats_nb_t *stats)
{
    timex_t cur;
    xtimer_now_timex(&cur);
    uint16_t now = cur.seconds;

    /* Half freshness if applicable to update to current freshness */
    netstats_nb_half_freshness(stats, &cur);

    return (stats->freshness >= NETSTATS_NB_FRESHNESS_TARGET) &&
           (now - stats->last_updated < NETSTATS_NB_FRESHNESS_EXPIRATION);
}

void netstats_nb_init(netif_t *dev)
{
    memset(dev->pstats, 0, sizeof(netstats_nb_t) * NETSTATS_NB_SIZE);
    cib_init(&dev->stats_idx, NETSTATS_NB_QUEUE_SIZE);
}

static void netstats_nb_create(netstats_nb_t *entry, const uint8_t *l2_addr, uint8_t l2_len)
{
    memset(entry, 0, sizeof(netstats_nb_t));
    memcpy(entry->l2_addr, l2_addr, l2_len);
    entry->l2_addr_len = l2_len;

#ifdef MODULE_NETSTATS_NEIGHBOR_ETX
    entry->etx = NETSTATS_NB_ETX_INIT * NETSTATS_NB_ETX_DIVISOR;
#endif
}

netstats_nb_t *netstats_nb_get(netif_t *dev, const uint8_t *l2_addr, uint8_t len)
{
    netstats_nb_t *stats = dev->pstats;

    for (int i = 0; i < NETSTATS_NB_SIZE; i++) {

        /* Check if this is the matching entry */
        if (l2_addr_equal(stats[i].l2_addr, stats[i].l2_addr_len, l2_addr, len)) {
            return &stats[i];
        }
    }

    return NULL;
}

/* find the oldest inactive entry to replace. Empty entries are infinity old */
static netstats_nb_t *netstats_nb_get_or_create(netif_t *dev, const uint8_t *l2_addr, uint8_t len)
{
    netstats_nb_t *old_entry = NULL;
    netstats_nb_t *stats = dev->pstats;

    timex_t cur;
    xtimer_now_timex(&cur);

    for (int i = 0; i < NETSTATS_NB_SIZE; i++) {

        /* Check if this is the matching entry */
        if (l2_addr_equal(stats[i].l2_addr, stats[i].l2_addr_len, l2_addr, len)) {
            return &stats[i];
        }

        /* Entry is oldest if it is empty */
        if (stats[i].l2_addr_len == 0) {
            old_entry = &stats[i];
            continue;
        }

        /* Check if the entry is expired */
        if ((netstats_nb_isfresh(&stats[i]))) {
            continue;
        }

        /* Entry is oldest if it is expired */
        if (old_entry == NULL) {
            old_entry = &stats[i];
            continue;
        }

        /* don't replace old entry if there are still empty ones */
        if (old_entry->l2_addr_len == 0) {
            continue;
        }

        /* Check if current entry is older than current oldest entry */
        old_entry = netstats_nb_comp(old_entry, &stats[i], cur.seconds);
    }

    /* if there is no matching entry,
     * create a new entry if we have an expired one */
    if (old_entry) {
        netstats_nb_create(old_entry, l2_addr, len);
    }

    return old_entry;
}

netstats_nb_t *netstats_nb_get_next(netstats_nb_t *first, netstats_nb_t *current)
{
    netstats_nb_t *last = first + NETSTATS_NB_SIZE;

    for (++current; current < last; ++current) {
        if (current->l2_addr_len) {
            return current;
        }
    }

    return NULL;
}

void netstats_nb_record(netif_t *dev, const uint8_t *l2_addr, uint8_t len)
{
    int idx = cib_put(&dev->stats_idx);

    if (idx < 0) {
        DEBUG("%s: put buffer empty\n", __func__);
        return;
    }

    DEBUG("put %d\n", idx);

    if (len == 0) {
        /* Fill queue with a NOP */
        dev->stats_queue[idx] = NULL;
    } else {
        dev->stats_queue[idx] = netstats_nb_get_or_create(dev, l2_addr, len);
        dev->stats_queue_time_tx[idx] = xtimer_now_usec();
    }
}

/* Get the first available neighbor in the transmission queue
 * and increment pointer. */
static netstats_nb_t *netstats_nb_get_recorded(netif_t *dev, uint32_t *time_tx)
{
    netstats_nb_t *res;
    int idx = cib_get(&dev->stats_idx);

    if (idx < 0) {
        DEBUG("%s: can't get record\n", __func__);
        return NULL;
    }

    DEBUG("get %d (%d left)\n", idx, cib_avail(&dev->stats_idx));

    res = dev->stats_queue[idx];
    dev->stats_queue[idx] = NULL;

    *time_tx = dev->stats_queue_time_tx[idx];

    return res;
}

__attribute__((unused))
static uint32_t _ewma(bool fresh, uint32_t old_val, uint32_t new_val)
{
    uint8_t ewma_alpha;

    /* If the stats are not fresh, use a larger alpha to average aggressive */
    if (fresh) {
        ewma_alpha = NETSTATS_NB_EWMA_ALPHA;
    } else {
        ewma_alpha = NETSTATS_NB_EWMA_ALPHA_RAMP;
    }

    /* Exponential weighted moving average */
    return (old_val * (NETSTATS_NB_EWMA_SCALE - ewma_alpha)
         +  new_val * ewma_alpha) / NETSTATS_NB_EWMA_SCALE;
}

static void netstats_nb_update_etx(netstats_nb_t *stats, netstats_nb_result_t result,
                                   uint8_t transmissions, bool fresh)
{
#ifdef MODULE_NETSTATS_NEIGHBOR_ETX

    /* don't do anything if driver does not report ETX */
    if (transmissions == 0) {
        return;
    }

    if (result != NETSTATS_NB_SUCCESS) {
        transmissions = NETSTATS_NB_ETX_NOACK_PENALTY;
    }

    stats->etx = _ewma(fresh, stats->etx, transmissions * NETSTATS_NB_ETX_DIVISOR);

#else
    (void)stats;
    (void)result;
    (void)transmissions;
    (void)fresh;
#endif
}

static void netstats_nb_update_time(netstats_nb_t *stats, netstats_nb_result_t result,
                                    uint32_t duration, bool fresh)
{
#if MODULE_NETSTATS_NEIGHBOR_TX_TIME

    /* TX time already got a penalty due to retransmissions */
    if (result != NETSTATS_NB_SUCCESS) {
        duration *= 2;
    }

    if (stats->time_tx_avg == 0) {
        stats->time_tx_avg = duration;
    } else {
        stats->time_tx_avg = _ewma(fresh, stats->time_tx_avg, duration);
    }
#else
    (void)stats;
    (void)result;
    (void)duration;
    (void)fresh;
#endif
}

static void netstats_nb_update_rssi(netstats_nb_t *stats, uint8_t rssi, bool fresh)
{
#ifdef MODULE_NETSTATS_NEIGHBOR_RSSI
    if (stats->rssi == 0) {
        stats->rssi = rssi;
    } else {
        stats->rssi = _ewma(fresh, stats->rssi, rssi);
    }
#else
    (void)stats;
    (void)rssi;
    (void)fresh;
#endif
}

static void netstats_nb_update_lqi(netstats_nb_t *stats, uint8_t lqi, bool fresh)
{
#ifdef MODULE_NETSTATS_NEIGHBOR_LQI
    if (stats->lqi == 0) {
        stats->lqi = lqi;
    } else {
        stats->lqi = _ewma(fresh, stats->lqi, lqi);
    }
#else
    (void)stats;
    (void)lqi;
    (void)fresh;
#endif
}

static void netstats_nb_incr_count_tx(netstats_nb_t *stats)
{
#ifdef MODULE_NETSTATS_NEIGHBOR_COUNT
    stats->tx_count++;
#else
    (void)stats;
#endif
}

static void netstats_nb_incr_count_rx(netstats_nb_t *stats)
{
#ifdef MODULE_NETSTATS_NEIGHBOR_COUNT
    stats->rx_count++;
#else
    (void)stats;
#endif
}

netstats_nb_t *netstats_nb_update_tx(netif_t *dev, netstats_nb_result_t result, uint8_t transmissions)
{
    uint32_t now = xtimer_now_usec();
    netstats_nb_t *stats;
    uint32_t time_tx = 0;

    /* Buggy drivers don't always generate TX done events.
     * Discard old events to prevent the tx start <-> tx done correlation
     * from getting out of sync. */
    do {
        stats = netstats_nb_get_recorded(dev, &time_tx);
    } while (cib_avail(&dev->stats_idx) &&
            ((now - time_tx) > NETSTATS_NB_TX_TIMEOUT_MS * US_PER_MS));

    /* Nothing to do for multicast or if packet was not sent */
    if (result == NETSTATS_NB_BUSY || stats == NULL) {
        return stats;
    }

    bool fresh = netstats_nb_isfresh(stats);

    netstats_nb_update_time(stats, result, now - time_tx, fresh);
    netstats_nb_update_etx(stats, result, transmissions, fresh);
    netstats_nb_incr_freshness(stats);
    netstats_nb_incr_count_tx(stats);

    return stats;
}

netstats_nb_t *netstats_nb_update_rx(netif_t *dev, const uint8_t *l2_addr,
                                     uint8_t l2_addr_len, uint8_t rssi, uint8_t lqi)
{
    netstats_nb_t *stats = netstats_nb_get_or_create(dev, l2_addr, l2_addr_len);

    if (stats == NULL) {
        return NULL;
    }

    bool fresh = netstats_nb_isfresh(stats);

    netstats_nb_update_rssi(stats, rssi, fresh);
    netstats_nb_update_lqi(stats, lqi, fresh);

    netstats_nb_incr_freshness(stats);
    netstats_nb_incr_count_rx(stats);

    return stats;
}
