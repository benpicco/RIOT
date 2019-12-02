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
 * @brief       Configuration of the MR-O-QPSK PHY on the AT86RF215 chip
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include "at86rf215.h"
#include "at86rf215_internal.h"

#include "debug.h"

/* IEEE Std 802.15.4g™-2012 Amendment 3
 * Table 68d—Total number of channels and first channel center frequencies for SUN PHYs
 * Table 68e—Center frequencies for the MR-O-QPSK PHY operating in the 868–870 MHz band
 */

// FIXME: center frequency / spacing is not uniform in sub-GHz band
#define QPSK_CHANNEL_SPACING_SUBGHZ     (650U)     /* kHz */
#define QPSK_CENTER_FREQUENCY_SUBGHZ    (868300U)  /* Hz  */

#define QPSK_CHANNEL_SPACING_24GHZ      (5000U)    /* kHz */
#define QPSK_CENTER_FREQUENCY_24GHZ     (2350000U - CCF0_24G_OFFSET) /* Hz  */

/* Table 6-103. O-QPSK Transmitter Frontend Configuration */
static uint8_t _TXCUTC_PARAMP(uint8_t chips)
{
    switch (chips) {
    case BB_FCHIP100:
        return RF_PARAMP32U;
    case BB_FCHIP200:
        return RF_PARAMP16U;
    case BB_FCHIP1000:
    case BB_FCHIP2000:
        return RF_PARAMP4U;
    }

    return 0;
}

/* Table 6-103. O-QPSK Transmitter Frontend Configuration */
static uint8_t _TXCUTC_LPFCUT(uint8_t chips)
{
    switch (chips) {
    case BB_FCHIP100:
    case BB_FCHIP200:
        return RF_FLC400KHZ;
    case BB_FCHIP1000:
    case BB_FCHIP2000:
        return RF_FLC1000KHZ;
    }

    return 0;
}

/* Table 6-103. O-QPSK Transmitter Frontend Configuration */
static uint8_t _TXDFE_SR(uint8_t chips)
{
    switch (chips) {
    case BB_FCHIP100:
        return RF_SR_400K;
    case BB_FCHIP200:
        return RF_SR_800K;
    case BB_FCHIP1000:
        return RF_SR_4000K;
    case BB_FCHIP2000:
        return RF_SR_4000K;
    }

    return 0;
}

/* Table 6-103. O-QPSK Transmitter Frontend Configuration */
static uint8_t _TXDFE_RCUT(uint8_t chips)
{
    return (chips == BB_FCHIP2000 ? RF_RCUT_FS_BY_2 : RF_RCUT_FS_BY_2P6);
}

/* Table 6-105. O-QPSK Receiver Frontend Configuration (Filter Settings) */
static uint8_t _RXBWC_BW(uint8_t chips)
{
    switch (chips) {
    case BB_FCHIP100:
        return RF_BW160KHZ_IF250KHZ;
    case BB_FCHIP200:
        return RF_BW250KHZ_IF250KHZ;
    case BB_FCHIP1000:
        return RF_BW1000KHZ_IF1000KHZ;
    case BB_FCHIP2000:
        return RF_BW2000KHZ_IF2000KHZ;
    }

    return 0;
}

/* Table 6-105. O-QPSK Receiver Frontend Configuration (Filter Settings) */
static uint8_t _RXDFE_SR(uint8_t chips)
{
    switch (chips) {
    case BB_FCHIP100:
        return RF_SR_400K;
    case BB_FCHIP200:
        return RF_SR_800K;
    case BB_FCHIP1000:
    case BB_FCHIP2000:
        return RF_SR_4000K;
    }

    return 0;
}

/* Table 6-105. O-QPSK Receiver Frontend Configuration (Filter Settings) */
static uint8_t _RXDFE_RCUT(uint8_t chips)
{
    switch (chips) {
    case BB_FCHIP100:
    case BB_FCHIP200:
        return RF_RCUT_FS_BY_5P3;
    case BB_FCHIP1000:
        return RF_RCUT_FS_BY_8;
    case BB_FCHIP2000:
        return RF_RCUT_FS_BY_4;
    }

    return 0;
}

/* Table 6-106. O-QPSK Receiver Frontend Configuration (AGC Settings) */
static inline uint8_t _AGCC(uint8_t chips)
{
    if (chips > BB_FCHIP200) {
        return (2 << AGCC_AVGS_SHIFT) | AGCC_EN_MASK;
    } else {
        return AGCC_EN_MASK;
    }
}

/* Table 6-100. MR-O-QPSK Modes */
static uint32_t _get_bitrate(uint8_t chips, uint8_t mode)
{
    switch (chips) {
    case BB_FCHIP100:
        return  6250 * (1 << mode);
    case BB_FCHIP200:
        return 12500 * (1 << mode);
    case BB_FCHIP1000:
    case BB_FCHIP2000:
        return mode ? 125000 * (1 << (mode - 1)) : 31250;
    }

    return 0;
}

static uint8_t _set_mode(at86rf215_t *dev, uint8_t mode)
{
    mode = AT86RF215_MR_OQPSK_MODE(mode);

    /* TX with selected rate mode */
    at86rf215_reg_write(dev, dev->BBC->RG_OQPSKPHRTX, mode);

    /* power save mode only works when not listening to legacy frames */
    /* listening to both uses ~1mA more that just listening to legacy */
    uint8_t rxm = RXM_MR_OQPSK;

    if (dev->flags & AT86RF215_OPT_RPC) {
        rxm |= OQPSKC2_RPC_MASK;                /* enable Reduced Power Consumption */
    }

    at86rf215_reg_write(dev, dev->BBC->RG_OQPSKC2,
                         rxm                    /* receive mode, MR-O-QPSK */
                       | OQPSKC2_FCSTLEG_MASK   /* 16 bit frame checksum */
                       | OQPSKC2_ENPROP_MASK);  /* enable RX of proprietary modes */

    at86rf215_switch_mode(dev, AT86RF215_MODE_MR_OQPSK);

    return mode;
}

static void _set_chips(at86rf215_t *dev, uint8_t chips)
{
    /* enable direct modulation if the chip supports it */
    uint8_t direct_modulation;
    if (chips < BB_FCHIP1000 && at86rf215_reg_read(dev, RG_RF_VN) >= 3) {
        direct_modulation = 1 << 4;
    } else {
        direct_modulation = 0;
    }

    /* Set Receiver Bandwidth */
    at86rf215_reg_write(dev, dev->RF->RG_RXBWC, _RXBWC_BW(chips));
    /* Set fS; fCUT for RX */
    at86rf215_reg_write(dev, dev->RF->RG_RXDFE, _RXDFE_SR(chips)
                                              | _RXDFE_RCUT(chips));
    /* Set Power Amplifier Ramp Time; fLPCUT */
    at86rf215_reg_write(dev, dev->RF->RG_TXCUTC, _TXCUTC_PARAMP(chips)
                                               | _TXCUTC_LPFCUT(chips));
    /* Set fS; fCUT for TX */
    at86rf215_reg_write(dev, dev->RF->RG_TXDFE, _TXDFE_SR(chips)
                                              | _TXDFE_RCUT(chips)
                                              | direct_modulation);

    /* set receiver gain target according to data sheet */
    at86rf215_reg_write(dev, dev->RF->RG_AGCS, 3 << AGCS_TGT_SHIFT);
    at86rf215_reg_write(dev, dev->RF->RG_AGCC, _AGCC(chips));

    /* use RC-0.8 shaping */
    at86rf215_reg_write(dev, dev->BBC->RG_OQPSKC0, chips | direct_modulation);
}

static void _set_legacy(at86rf215_t *dev, bool high_rate)
{
    uint8_t chips;

    /* enable/disable legacy high data rate */
    if (high_rate) {
        at86rf215_reg_write(dev, dev->BBC->RG_OQPSKC3, OQPSKC3_HRLEG_MASK);
    } else {
        at86rf215_reg_write(dev, dev->BBC->RG_OQPSKC3, 0);
    }

    if (is_subGHz(dev)) {
        chips = BB_FCHIP1000;
    } else {
        chips = BB_FCHIP2000;
    }

    _set_chips(dev, chips);

    at86rf215_reg_write(dev, dev->BBC->RG_OQPSKPHRTX, AT86RF215_OQPSK_MODE_LEGACY);

    at86rf215_reg_write(dev, dev->BBC->RG_OQPSKC2,
                         RXM_LEGACY_OQPSK       /* receive mode, legacy O-QPSK */
                       | OQPSKC2_FCSTLEG_MASK   /* 16 bit frame checksum */
                       | OQPSKC2_ENPROP_MASK);  /* enable RX of proprietary modes */

    at86rf215_switch_mode(dev, AT86RF215_MODE_LEGACY_OQPSK);
}

static void _set_ack_timeout(at86rf215_t *dev, uint8_t chips, uint8_t mode)
{
    dev->ack_timeout_usec = AT86RF215_ACK_PERIOD_IN_BITS * 1000000UL / _get_bitrate(chips, mode);
    DEBUG("[%s] ACK timeout: %"PRIu32" µs\n", "O-QPSK", dev->ack_timeout_usec);
}

void _end_configure_OQPSK(at86rf215_t *dev)
{
    /* set channel spacing */
    if (is_subGHz(dev)) {
        at86rf215_reg_write(dev, dev->RF->RG_CS, QPSK_CHANNEL_SPACING_SUBGHZ / 25);
        at86rf215_reg_write16(dev, dev->RF->RG_CCF0L, QPSK_CENTER_FREQUENCY_SUBGHZ / 25);
    } else {
        at86rf215_reg_write(dev, dev->RF->RG_CS, QPSK_CHANNEL_SPACING_24GHZ / 25);
        at86rf215_reg_write16(dev, dev->RF->RG_CCF0L, QPSK_CENTER_FREQUENCY_24GHZ / 25);
    }

    /* lowest preamble detection sensitivity */
    at86rf215_reg_write(dev, dev->BBC->RG_OQPSKC1, 0);

    /* make sure the channel config is still valid */
    dev->num_chans = is_subGHz(dev) ? 3 : 16;
    dev->netdev.chan = at86rf215_chan_valid(dev, dev->netdev.chan);
    at86rf215_reg_write16(dev, dev->RF->RG_CNL, dev->netdev.chan);

    at86rf215_enable_radio(dev, BB_MROQPSK);
}

int at86rf215_configure_OQPSK(at86rf215_t *dev, uint8_t chips, uint8_t mode)
{
    if (chips > BB_FCHIP2000) {
        DEBUG("[%s] invalid chips: %d\n", __func__, chips);
        return -EINVAL;
    }

    if (mode > 4) {
        DEBUG("[%s] invalid mode: %d\n", __func__, mode);
        return -EINVAL;
    }

    /* mode 4 only supports 2000 kchip/s */
    if (mode == 4 && chips != BB_FCHIP2000) {
        DEBUG("[%s] mode 4 only supports 2000 kChip/s\n", __func__);
        return -EINVAL;
    }

    at86rf215_await_state_end(dev, RF_STATE_TX);

    /* disable radio */
    at86rf215_reg_write(dev, dev->BBC->RG_PC, 0);

    _set_mode(dev, mode);
    _set_chips(dev, chips);
    _set_ack_timeout(dev, chips, mode);

    _end_configure_OQPSK(dev);

    return 0;
}

int at86rf215_configure_legacy_OQPSK(at86rf215_t *dev, bool high_rate)
{
    /* select 'mode' that would result in the approprate MR-O-QPSK data rate */
    uint8_t mode  = high_rate ? 3 : 2;
    uint8_t chips = is_subGHz(dev) ? BB_FCHIP1000 : BB_FCHIP2000;

    at86rf215_await_state_end(dev, RF_STATE_TX);

    /* disable radio */
    at86rf215_reg_write(dev, dev->BBC->RG_PC, 0);

    _set_legacy(dev, high_rate);
    _set_ack_timeout(dev, chips, mode);

    _end_configure_OQPSK(dev);

    return 0;
}

int at86rf215_OQPSK_set_chips(at86rf215_t *dev, uint8_t chips)
{
    uint8_t mode;

    mode = at86rf215_reg_read(dev, dev->BBC->RG_OQPSKPHRTX);

    if (mode & AT86RF215_OQPSK_MODE_LEGACY) {
        DEBUG("[%s] can't set chip rate in legacy mode\n", __func__);
        return -1;
    }

    at86rf215_await_state_end(dev, RF_STATE_TX);

    _set_chips(dev, chips);
    _set_ack_timeout(dev, chips, mode >> OQPSKPHRTX_MOD_SHIFT);

    return 0;
}

uint8_t at86rf215_OQPSK_get_chips(at86rf215_t *dev)
{
    return at86rf215_reg_read(dev, dev->BBC->RG_OQPSKC0) & OQPSKC0_FCHIP_MASK;
}

int at86rf215_OQPSK_set_mode(at86rf215_t *dev, uint8_t mode)
{
    if (mode > 4) {
        return -1;
    }

    uint8_t chips = at86rf215_OQPSK_get_chips(dev);

    at86rf215_await_state_end(dev, RF_STATE_TX);

    if (mode == 4 && chips != BB_FCHIP2000) {
        _set_chips(dev, BB_FCHIP2000);
    }

    _set_mode(dev, mode);
    _set_ack_timeout(dev, chips, mode);

    return 0;
}

uint8_t at86rf215_OQPSK_get_mode(at86rf215_t *dev)
{
    uint8_t mode = at86rf215_reg_read(dev, dev->BBC->RG_OQPSKPHRTX);
    return (mode & OQPSKPHRTX_MOD_MASK) >> OQPSKPHRTX_MOD_SHIFT;
}

int at86rf215_OQPSK_set_mode_legacy(at86rf215_t *dev, bool high_rate)
{
    /* enable/disable legacy high data rate */
    if (high_rate) {
        at86rf215_reg_write(dev, dev->BBC->RG_OQPSKC3, OQPSKC3_HRLEG_MASK);
    } else {
        at86rf215_reg_write(dev, dev->BBC->RG_OQPSKC3, 0);
    }

    /* select 'mode' that would result in the approprate MR-O-QPSK data rate */
    uint8_t mode  = high_rate ? 3 : 2;
    uint8_t chips = is_subGHz(dev) ? BB_FCHIP1000 : BB_FCHIP2000;

    _set_ack_timeout(dev, chips, mode);

    return 0;
}

uint8_t at86rf215_OQPSK_get_mode_legacy(at86rf215_t *dev)
{
    if (at86rf215_reg_read(dev, dev->BBC->RG_OQPSKC3) & OQPSKC3_HRLEG_MASK) {
        return 1;
    }

    return 0;
}
