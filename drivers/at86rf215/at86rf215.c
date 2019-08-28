/*
 * Copyright (C) 2019 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_at86rf215
 * @{
 *
 * @file
 * @brief       Implementation of public functions for AT86RF215 driver
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */


#include "luid.h"
#include "byteorder.h"
#include "net/ieee802154.h"
#include "net/gnrc.h"
#include "at86rf215_internal.h"
#include "at86rf215_netdev.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static void _setup_interface(at86rf215_t *dev, const at86rf215_params_t *params)
{
    netdev_t *netdev = (netdev_t *)dev;

    netdev->driver = &at86rf215_driver;
    dev->params = *params;
    dev->state = AT86RF215_STATE_OFF;
}

void at86rf215_setup(at86rf215_t *dev_09, at86rf215_t *dev_24, const at86rf215_params_t *params)
{
    /* configure the sub-GHz interface */
    if (dev_09) {
        dev_09->RF = &RF09_regs;
        dev_09->BBC = &BBC0_regs;
        _setup_interface(dev_09, params);
        dev_09->flags |= AT86RF215_OPT_SUBGHZ;
        dev_09->sibling = dev_24;
    }

    /* configure the 2.4 GHz interface */
    if (dev_24) {
        dev_24->RF = &RF24_regs;
        dev_24->BBC = &BBC1_regs;
        _setup_interface(dev_24, params);
        dev_24->sibling = dev_09;
    }
}

void at86rf215_reset_cfg(at86rf215_t *dev)
{
    netdev_ieee802154_reset(&dev->netdev);

    eui64_t addr_long;
    /* get an 8-byte unique ID to use as hardware address */
    luid_get(addr_long.uint8, IEEE802154_LONG_ADDRESS_LEN);

    /* make sure we mark the address as non-multicast and not globally unique */
    addr_long.uint8[0] &= ~(0x01);
    addr_long.uint8[0] |=  (0x02);

    if (is_subGHz(dev)) {
        dev->page = 2; /* O-QPSK, legacy */
        dev->netdev.chan = AT86RF215_DEFAULT_SUBGHZ_CHANNEL;
    } else {
        dev->page = 0; /* O-QPSK, legacy */
        dev->netdev.chan = AT86RF215_DEFAULT_CHANNEL;

        /* make sure both IFs don't have the same address */
        addr_long.uint8[1]++;
    }

    memcpy(dev->netdev.short_addr, addr_long.uint8, IEEE802154_SHORT_ADDRESS_LEN);
    memcpy(dev->netdev.long_addr, addr_long.uint8, IEEE802154_LONG_ADDRESS_LEN);
    dev->netdev.pan = IEEE802154_DEFAULT_PANID;

    /* apply the configuration */
    at86rf215_reset(dev);

    /* set default options */
    dev->retries_max = 3;
    dev->csma_retries_max = 4;

    dev->flags |= AT86RF215_OPT_AUTOACK
               |  AT86RF215_OPT_CSMA;

    const netopt_enable_t enable = NETOPT_ENABLE;
    netdev_ieee802154_set(&dev->netdev, NETOPT_ACK_REQ,
                          &enable, sizeof(enable));
}

void at86rf215_reset(at86rf215_t *dev)
{
    dev->state = AT86RF215_STATE_OFF;

    /* Reset state machine to ensure a known state */
    at86rf215_set_state(dev, RF_STATE_TRXOFF);

    if (!dev->sibling) {
        /* disable 2.4-GHz IRQs if the interface is not enabled */
        if (is_subGHz(dev)) {
            at86rf215_reg_write(dev, RG_BBC1_IRQM, 0);
            at86rf215_reg_write(dev, RG_RF24_IRQM, 0);

        /* disable sub-GHz IRQs if the interface is not enabled */
        } else {
            at86rf215_reg_write(dev, RG_BBC0_IRQM, 0);
            at86rf215_reg_write(dev, RG_RF09_IRQM, 0);
        }
    }

    /* disable clock output */
#if AT86RF215_USE_CLOCK_OUTPUT == 0
    at86rf215_reg_write(dev, RG_RF_CLKO, 0);
#endif

    /* enable TXFE & RXFE IRQ */
    at86rf215_reg_write(dev, dev->BBC->RG_IRQM, BB_IRQ_TXFE | BB_IRQ_RXFE);

    /* enable EDC IRQ */
    at86rf215_reg_write(dev, dev->RF->RG_IRQM, RF_IRQ_EDC);

    /* set energy detect thresholt to -84 dBm */
    at86rf215_reg_write(dev, dev->BBC->RG_AMEDT, -84);

    /* enable address filter 0 */
    at86rf215_reg_write(dev, dev->BBC->RG_AFC0, 0x1);
    at86rf215_reg_write(dev, dev->BBC->RG_AMAACKPD, 0x1);

    /* enable auto-ACK with Frame Checksum & Data Rate derived from RX frame */
    at86rf215_reg_write(dev, dev->BBC->RG_AMCS, AMCS_AACK_MASK
                                              | AMCS_AACKFA_MASK
                                              | AMCS_AACKDR_MASK);

    /* set default channel page */
    at86rf215_set_page(dev, dev->page);

    /* set default channel */
    at86rf215_set_chan(dev, dev->netdev.chan);

    /* set short and long address */
    uint64_t long_addr;
    memcpy(&long_addr, dev->netdev.long_addr, sizeof(long_addr));
    at86rf215_set_addr_long(dev, long_addr);
    at86rf215_set_addr_short(dev, *(uint16_t *)&dev->netdev.short_addr[0]);

    /*** set default PAN id ***/
    at86rf215_set_pan(dev, dev->netdev.pan);

    /* set default TX power */
    at86rf215_set_txpower(dev, AT86RF215_DEFAULT_TXPOWER);

    at86rf215_set_state(dev, RF_STATE_RX);
    dev->state = AT86RF215_STATE_IDLE;
}

ssize_t at86rf215_send(at86rf215_t *dev, const void *data, size_t len)
{
    /* check data length */
    if (len > AT86RF215_MAX_PKT_LENGTH) {
        DEBUG("[at86rf215] Error: data to send exceeds max packet size\n");
        return -EOVERFLOW;
    }

    if (at86rf215_tx_prepare(dev)) {
        return -EBUSY;
    }

    at86rf215_tx_load(dev, data, len, 0);
    at86rf215_tx_exec(dev);
    return len;
}

static void _enable_tx2rx(at86rf215_t *dev)
{
    uint8_t amcs = at86rf215_reg_read(dev, dev->BBC->RG_AMCS);

    /* disable AACK, enable TX2RX */
    amcs |=  AMCS_TX2RX_MASK;
    amcs &= ~AMCS_AACK_MASK;

    at86rf215_reg_write(dev, dev->BBC->RG_AMCS, amcs);
}

void at86rf215_tx_done(at86rf215_t *dev)
{
    uint8_t amcs = at86rf215_reg_read(dev, dev->BBC->RG_AMCS);

    /* enable AACK, disable TX2RX */
    amcs &= ~AMCS_TX2RX_MASK;
    if (dev->flags & AT86RF215_OPT_AUTOACK) {
        amcs |= AMCS_AACK_MASK;
    }

    at86rf215_reg_write(dev, dev->BBC->RG_AMCS, amcs);
}

int at86rf215_tx_prepare(at86rf215_t *dev)
{
    if (dev->state != AT86RF215_STATE_IDLE) {
        DEBUG("[%s] TX while %s\n", __func__, at86rf215_sw_state2a(dev->state));
        return -EBUSY;
    }

    /* disable baseband for energy detection */
    at86rf215_disable_baseband(dev);

    dev->state = AT86RF215_STATE_TX_PREP;

    /* automatically switch to RX when TX is done */
    _enable_tx2rx(dev);

    /* prepare for TX */
    at86rf215_set_state(dev, CMD_RF_TXPREP);

    dev->tx_frame_len = IEEE802154_FCS_LEN;

    return 0;
}

size_t at86rf215_tx_load(at86rf215_t *dev, const uint8_t *data,
                         size_t len, size_t offset)
{
    if (dev->state != AT86RF215_STATE_TX_PREP) {
        DEBUG("[%s] TX while %s\n", __func__, at86rf215_sw_state2a(dev->state));
        return 0;
    }

    /* set bit if ACK was requested */
    if (offset == 0 && (data[0] & IEEE802154_FCF_ACK_REQ) && dev->retries_max) {
        dev->flags |= AT86RF215_OPT_ACK_REQUESTED;
    }

    at86rf215_reg_write_bytes(dev, dev->BBC->RG_FBTXS + offset, data, len);
    dev->tx_frame_len += (uint16_t) len;

    return offset + len;
}

int at86rf215_tx_exec(at86rf215_t *dev)
{
    if (dev->state != AT86RF215_STATE_TX_PREP) {
        DEBUG("[%s] TX while %s\n", __func__, at86rf215_sw_state2a(dev->state));
        return -EBUSY;
    }

    netdev_t *netdev = (netdev_t *)dev;

    /* write frame length */
    at86rf215_reg_write16(dev, dev->BBC->RG_TXFLL, dev->tx_frame_len);

    dev->retries = dev->retries_max;
    dev->csma_retries = dev->csma_retries_max;

    /* only listen for ACK frames */
    if (dev->flags & AT86RF215_OPT_ACK_REQUESTED) {
        at86rf215_filter_ack(dev, true);
    }

    /* wait for TRXRDY - we should already be in that state by now */
    while (!(at86rf215_get_rf_state(dev) & RF_IRQ_TRXRDY)) {}

    dev->state = AT86RF215_STATE_TX;

    if (dev->flags & AT86RF215_OPT_CSMA) {
        /* switch to state RX for energy detection */
        at86rf215_set_state(dev, CMD_RF_RX);

        /* start energy measurement */
        at86rf215_reg_write(dev, dev->RF->RG_EDC, 1);
    } else {
        /* no CSMA - send directly */
        at86rf215_enable_baseband(dev);
        at86rf215_rf_cmd(dev, CMD_RF_TX);
    }

    /* transmission will start when energy detection is finished. */
    if (netdev->event_callback &&
        (dev->flags & AT86RF215_OPT_TELL_TX_START)) {
        netdev->event_callback(netdev, NETDEV_EVENT_TX_STARTED);
    }

    return 0;
}

void at86rf215_tx_abort(at86rf215_t *dev)
{
    if (dev->state != AT86RF215_STATE_TX_PREP) {
        DEBUG("[%s] TX while %s\n", __func__, at86rf215_sw_state2a(dev->state));
        return;
    }

    at86rf215_tx_done(dev);
    at86rf215_enable_baseband(dev);
    at86rf215_rf_cmd(dev, CMD_RF_TX);

    dev->state = AT86RF215_STATE_IDLE;
}

bool at86rf215_cca(at86rf215_t *dev)
{
    bool clear;
    uint8_t old_state;

    if (dev->state != AT86RF215_STATE_IDLE) {
        return false;
    }

    old_state = at86rf215_set_state(dev, RF_STATE_RX);

    /* disable ED IRQ, baseband */
    at86rf215_reg_and(dev, dev->RF->RG_IRQM, ~RF_IRQ_EDC);
    at86rf215_reg_and(dev, dev->BBC->RG_PC, ~PC_BBEN_MASK);

    /* start energy detect */
    at86rf215_reg_write(dev, dev->RF->RG_EDC, 1);
    while (!(at86rf215_reg_read(dev, dev->RF->RG_IRQS) & RF_IRQ_EDC)) {}

    clear = !(at86rf215_reg_read(dev, dev->BBC->RG_AMCS) & AMCS_CCAED_MASK);

    /* enable ED IRQ, baseband */
    at86rf215_reg_or(dev, dev->RF->RG_IRQM, RF_IRQ_EDC);
    at86rf215_reg_or(dev, dev->BBC->RG_PC, PC_BBEN_MASK);

    at86rf215_set_state(dev, old_state);

    return clear;
}
