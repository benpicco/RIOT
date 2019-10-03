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
 * @brief       Getter and setter functions for the AT86RF215 driver
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include <string.h>

#include "at86rf215.h"
#include "at86rf215_internal.h"
#include "periph/spi.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/* we can still go +3 dBm higher by increasing PA current */
#define PAC_DBM_MIN                             (-31)     /* dBm */

uint16_t at86rf215_get_addr_short(const at86rf215_t *dev)
{
    return at86rf215_reg_read16(dev, dev->BBC->RG_MACSHA0F0);
}

void at86rf215_set_addr_short(at86rf215_t *dev, uint16_t addr)
{
    dev->netdev.short_addr[0] = (uint8_t)(addr);
    dev->netdev.short_addr[1] = (uint8_t)(addr >> 8);

    at86rf215_reg_write16(dev, dev->BBC->RG_MACSHA0F0, addr);
}

uint64_t at86rf215_get_addr_long(const at86rf215_t *dev)
{
    uint64_t addr;
    at86rf215_reg_read_bytes(dev, dev->BBC->RG_MACEA0, &addr, sizeof(addr));
    return addr;
}

void at86rf215_set_addr_long(at86rf215_t *dev, uint64_t addr)
{
    memcpy(dev->netdev.long_addr, &addr, sizeof(addr));
    addr = ntohll(addr);
    at86rf215_reg_write_bytes(dev, dev->BBC->RG_MACEA0, &addr, sizeof(addr));
}

uint8_t at86rf215_get_chan(const at86rf215_t *dev)
{
    return at86rf215_reg_read16(dev, dev->RF->RG_CNL);
}

void at86rf215_set_chan(at86rf215_t *dev, uint16_t channel)
{
    /* frequency has to be updated in TRXOFF status (datatsheet: 6.3.2) */
    uint8_t old_state = at86rf215_set_state(dev, CMD_RF_TRXOFF);

    at86rf215_reg_write16(dev, dev->RF->RG_CNL, channel);
    dev->netdev.chan = channel;

    /* enable the radio again */
    at86rf215_set_state(dev, old_state);
}

uint16_t at86rf215_get_channel_spacing(at86rf215_t *dev) {
    return 25 * at86rf215_reg_read(dev, dev->RF->RG_CS);
}

uint8_t at86rf215_get_page(const at86rf215_t *dev)
{
    return dev->page;
}

/* TODO: find out how pages 9 & 10 are to be configured */
void at86rf215_set_page(at86rf215_t *dev, uint8_t page)
{
    if (is_subGHz(dev) && page == 2) {
        at86rf215_configure_OQPSK(dev, BB_FCHIP1000, IEEE802154_OQPSK_FLAG_LEGACY);
        dev->page = page;
    }

    if (!is_subGHz(dev) && page == 0) {
        at86rf215_configure_OQPSK(dev, BB_FCHIP2000, IEEE802154_OQPSK_FLAG_LEGACY);
        dev->page = page;
    }
}

uint8_t at86rf215_get_phy_mode(at86rf215_t *dev)
{
    switch (at86rf215_reg_read(dev, dev->BBC->RG_PC) & PC_PT_MASK) {
    case 0x1: return IEEE802154_PHY_FSK;
    case 0x2: return IEEE802154_PHY_OFDM;
    case 0x3: return IEEE802154_PHY_OQPSK;
    default:  return IEEE802154_PHY_DISABLED;
    }
}

uint16_t at86rf215_get_pan(const at86rf215_t *dev)
{
    return at86rf215_reg_read16(dev, dev->BBC->RG_MACPID0F0);
}

void at86rf215_set_pan(at86rf215_t *dev, uint16_t pan)
{
    dev->netdev.pan = pan;
    at86rf215_reg_write16(dev, dev->BBC->RG_MACPID0F0, pan);
}

// TODO: take modulation into account
int16_t at86rf215_get_txpower(const at86rf215_t *dev)
{
    uint8_t pac = at86rf215_reg_read(dev, dev->RF->RG_PAC);

    /* almost linear, each PACUR step adds ~1 dBm */
    return PAC_DBM_MIN + (pac & PAC_TXPWR_MASK) +
           ((pac & PAC_PACUR_MASK) >> PAC_PACUR_SHIFT);
}

// TODO: take modulation into account
void at86rf215_set_txpower(const at86rf215_t *dev, int16_t txpower)
{
    uint8_t pacur = 0;

    txpower -= PAC_DBM_MIN;

    if (txpower < 0) {
            txpower = 0;
    }

    if (txpower > PAC_TXPWR_MASK) {
        switch (txpower - PAC_TXPWR_MASK) {
            case 1:
                pacur = 1 << PAC_PACUR_SHIFT;
                break;
            case 2:
                pacur = 2 << PAC_PACUR_SHIFT;
                break;
            default:
                pacur = 3 << PAC_PACUR_SHIFT;
                break;
        }

        txpower = PAC_TXPWR_MASK;
    }

    at86rf215_reg_write(dev, dev->RF->RG_PAC, pacur | txpower);
}

int8_t at86rf215_get_cca_threshold(const at86rf215_t *dev)
{
    return at86rf215_reg_read(dev, dev->BBC->RG_AMEDT);
}

void at86rf215_set_cca_threshold(const at86rf215_t *dev, int8_t value)
{
    at86rf215_reg_write(dev, dev->BBC->RG_AMEDT, value);
}

int8_t at86rf215_get_ed_level(at86rf215_t *dev)
{
    return at86rf215_reg_read(dev, dev->RF->RG_EDV);
}

void at86rf215_set_option(at86rf215_t *dev, uint16_t option, bool state)
{
    /* set option field */
    dev->flags = (state) ? (dev->flags |  option)
                         : (dev->flags & ~option);

    switch (option) {
        case AT86RF215_OPT_TELL_RX_START:
            if (state){
                at86rf215_reg_or(dev, dev->BBC->RG_IRQM, BB_IRQ_RXAM);
            } else {
                at86rf215_reg_and(dev, dev->BBC->RG_IRQM, ~BB_IRQ_RXAM);
            }

            break;
        case AT86RF215_OPT_PROMISCUOUS:
            if (state){
                at86rf215_reg_or(dev, dev->BBC->RG_AFC0, AFC0_PM_MASK);
            } else {
                at86rf215_reg_and(dev, dev->BBC->RG_AFC0, ~AFC0_PM_MASK);
            }

            break;
        case AT86RF215_OPT_AUTOACK:
            if (state){
                at86rf215_reg_or(dev, dev->BBC->RG_AMCS, AMCS_AACK_MASK);
            } else {
                at86rf215_reg_and(dev, dev->BBC->RG_AMCS, ~AMCS_AACK_MASK);
            }

            break;

        default:
            /* do nothing */
            break;
    }
}

static void _set_state(at86rf215_t *dev, uint8_t state)
{
    at86rf215_rf_cmd(dev, state);
    while (at86rf215_get_rf_state(dev) != state) {}
}

uint8_t at86rf215_set_state(at86rf215_t *dev, uint8_t cmd)
{
    uint8_t old_state;

    /* device might not respond in deep sleep */
    if (dev->state == AT86RF215_STATE_SLEEP) {
        old_state = RF_STATE_RESET;
    } else do {
        old_state = at86rf215_get_rf_state(dev);
    } while (old_state == RF_STATE_TRANSITION);

    if (cmd == old_state) {
        return old_state;
    }

    if (cmd == CMD_RF_SLEEP && old_state == RF_STATE_RESET) {
        return old_state;
    }

    if ((old_state == RF_STATE_TX && cmd == CMD_RF_RX) ||
        (old_state == RF_STATE_RX && cmd == CMD_RF_TX)) {
        _set_state(dev, CMD_RF_TXPREP);
    }

    if (old_state == RF_STATE_RESET) {
        /* wake the transceiver */
        _set_state(dev, CMD_RF_TRXOFF);
        /* config is lost after SLEEP */
        at86rf215_reset(dev);

        /* if both transceivers were sleeping, the chip entered DEEP_SLEEP.
           Waking one device in that mode wakes the other one too. */
        if (dev->sibling && dev->sibling->state == AT86RF215_STATE_SLEEP) {
            dev->sibling->state = AT86RF215_STATE_OFF;
            at86rf215_set_state(dev->sibling, CMD_RF_SLEEP);
        }
    }

    if (cmd == CMD_RF_SLEEP) {
        /* first we must transition to TRXOFF */
        _set_state(dev, CMD_RF_TRXOFF);

        /* clear IRQ */
        at86rf215_reg_read(dev, dev->BBC->RG_IRQS);
        at86rf215_reg_read(dev, dev->RF->RG_IRQS);
        at86rf215_rf_cmd(dev, cmd);

        dev->state = AT86RF215_STATE_SLEEP;
    } else {
        _set_state(dev, cmd);
    }

    return old_state;
}
