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
 * @brief       Configuration of the MR-FSK PHY on the AT86RF215 chip
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include "at86rf215.h"
#include "at86rf215_internal.h"

#include "debug.h"

/* IEEE Std 802.15.4g™-2012 Amendment 3
 * Table 68d—Total number of channels and first channel center frequencies for SUN PHYs */
#define FSK_CHANNEL_SPACING            (200U)     /* kHz */
#define FSK_CENTER_FREQUENCY_SUBGHZ    (863125U)  /* Hz  */
#define FSK_CENTER_FREQUENCY_24GHZ     (2400200U - CCF0_24G_OFFSET) /* Hz  */

/* also used by at86rf215_netdev.c */
const uint8_t at86rf215_fsk_srate_10kHz[] = {
    [FSK_SRATE_50K]  = 5,
    [FSK_SRATE_100K] = 10,
    [FSK_SRATE_150K] = 15,
    [FSK_SRATE_200K] = 20,
    [FSK_SRATE_300K] = 30,
    [FSK_SRATE_400K] = 40
};

/* Table 6-57, index is symbol rate */
static const uint8_t FSKPE_Val[3][6] = {
    { 0x02, 0x0E, 0x3E, 0x74, 0x05, 0x13 }, /* FSKPE0 */
    { 0x03, 0x0F, 0x3F, 0x7F, 0x3C, 0x29 }, /* FSKPE1 */
    { 0xFC, 0xF0, 0xC0, 0x80, 0xC3, 0xC7 }  /* FSKPE2 */
};

/* table 6-51: Symbol Rate Settings */
static uint8_t _TXDFE_SR(at86rf215_t *dev, uint8_t srate)
{
    const uint8_t version = at86rf215_reg_read(dev, RG_RF_VN);

    switch (srate) {
    case FSK_SRATE_50K:
        return version == 1 ? RF_SR_400K : RF_SR_500K;
    case FSK_SRATE_100K:
        return version == 1 ? RF_SR_800K : RF_SR_1000K;
    case FSK_SRATE_150K:
    case FSK_SRATE_200K:
        return RF_SR_2000K;
    case FSK_SRATE_300K:
    case FSK_SRATE_400K:
        return RF_SR_4000K;
    }

    return 0;
}

/* table 6-51: Symbol Rate Settings */
static uint8_t _RXDFE_SR(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return RF_SR_400K;
    case FSK_SRATE_100K:
        return RF_SR_800K;
    case FSK_SRATE_150K:
    case FSK_SRATE_200K:
        return RF_SR_1000K;
    case FSK_SRATE_300K:
    case FSK_SRATE_400K:
        return RF_SR_2000K;
    }

    return 0;
}

/* table 6-53 Recommended Configuration of the Transmitter Frontend */
static uint8_t _TXCUTC_PARAMP(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return RF_PARAMP32U;
    case FSK_SRATE_100K:
    case FSK_SRATE_150K:
    case FSK_SRATE_200K:
        return RF_PARAMP16U;
    case FSK_SRATE_300K:
    case FSK_SRATE_400K:
        return RF_PARAMP8U;
    }

    return 0;
}

/* Table 6-53. Recommended Configuration of the Transmitter Frontend (modulation index 0.5) */
static uint8_t _TXCUTC_LPFCUT_half(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return RF_FLC80KHZ;
    case FSK_SRATE_100K:
        return RF_FLC100KHZ;
    case FSK_SRATE_150K:
        return RF_FLC160KHZ;
    case FSK_SRATE_200K:
        return RF_FLC200KHZ;
    case FSK_SRATE_300K:
        return RF_FLC315KHZ;
    case FSK_SRATE_400K:
        return RF_FLC400KHZ;
    }
    return 0;
}

/* Table 6-54. Recommended Configuration of the Transmitter Frontend (modulation index 1) */
static uint8_t _TXCUTC_LPFCUT_full(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return RF_FLC80KHZ;
    case FSK_SRATE_100K:
        return RF_FLC160KHZ;
    case FSK_SRATE_150K:
        return RF_FLC250KHZ;
    case FSK_SRATE_200K:
        return RF_FLC315KHZ;
    case FSK_SRATE_300K:
        return RF_FLC500KHZ;
    case FSK_SRATE_400K:
        return RF_FLC625KHZ;
    }
    return 0;
}

static inline uint8_t _TXCUTC_LPFCUT(uint8_t srate, bool half)
{
    return half ? _TXCUTC_LPFCUT_half(srate) : _TXCUTC_LPFCUT_full(srate);
}

/* Table 6-60. Recommended Configuration values of the sub-1GHz Receiver Frontend (modulation index 1/2) */
static uint8_t _RXBWC_BW_subGHz_half(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return RF_BW160KHZ_IF250KHZ;
    case FSK_SRATE_100K:
        return RF_BW200KHZ_IF250KHZ;
    case FSK_SRATE_150K:
    case FSK_SRATE_200K:
        return RF_BW320KHZ_IF500KHZ;
    case FSK_SRATE_300K:
        return (RF_BW500KHZ_IF500KHZ | (1 << RXBWC_IFS_SHIFT));
    case FSK_SRATE_400K:
        return RF_BW630KHZ_IF1000KHZ;
    }

    return 0;
}

/* Table 6-61. Recommended Configuration values of the 2.4GHz Receiver Frontend (modulation index 1/2) */
static uint8_t _RXBWC_BW_2dot4GHz_half(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return RF_BW160KHZ_IF250KHZ;
    case FSK_SRATE_100K:
        return RF_BW200KHZ_IF250KHZ;
    case FSK_SRATE_150K:
        return RF_BW320KHZ_IF500KHZ;
    case FSK_SRATE_200K:
        return RF_BW400KHZ_IF500KHZ;
    case FSK_SRATE_300K:
        return RF_BW630KHZ_IF1000KHZ;
    case FSK_SRATE_400K:
        return RF_BW800KHZ_IF1000KHZ;
    }

    return 0;
}

/* Table 6-62. Recommended Configuration values of the sub-1GHz Receiver Frontend (modulation index 1) */
static uint8_t _RXBWC_BW_subGHz_full(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return RF_BW160KHZ_IF250KHZ;
    case FSK_SRATE_100K:
        return RF_BW320KHZ_IF500KHZ;
    case FSK_SRATE_150K:
        return RF_BW400KHZ_IF500KHZ;
    case FSK_SRATE_200K:
        return (RF_BW500KHZ_IF500KHZ | (1 << RXBWC_IFS_SHIFT));
    case FSK_SRATE_300K:
        return RF_BW630KHZ_IF1000KHZ;
    case FSK_SRATE_400K:
        return (RF_BW1000KHZ_IF1000KHZ | (1 << RXBWC_IFS_SHIFT));
    }

    return 0;
}

/* Table 6-63. Recommended Configuration values of the 2.4 GHz Receiver Frontend (modulation index 1) */
static uint8_t _RXBWC_BW_2dot4GHz_full(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return RF_BW200KHZ_IF250KHZ;
    case FSK_SRATE_100K:
        return RF_BW400KHZ_IF500KHZ;
    case FSK_SRATE_150K:
    case FSK_SRATE_200K:
        return RF_BW630KHZ_IF1000KHZ;
    case FSK_SRATE_300K:
        return RF_BW800KHZ_IF1000KHZ;
    case FSK_SRATE_400K:
        return (RF_BW1000KHZ_IF1000KHZ | (1 << RXBWC_IFS_SHIFT));
    }

    return 0;
}

static inline uint8_t _RXBWC_BW(uint8_t srate, bool subGHz, bool half)
{
    if (subGHz) {
        return half ? _RXBWC_BW_subGHz_half(srate) : _RXBWC_BW_subGHz_full(srate);
    } else {
        return half ? _RXBWC_BW_2dot4GHz_half(srate) : _RXBWC_BW_2dot4GHz_full(srate);
    }
}

static uint8_t _RXDFE_RCUT_half(uint8_t srate, bool subGHz)
{
    if (srate == FSK_SRATE_200K) {
        return RF_RCUT_FS_BY_5P3;
    }

    if (srate == FSK_SRATE_400K && !subGHz) {
        return RF_RCUT_FS_BY_5P3;
    }

    return RF_RCUT_FS_BY_8;
}

static uint8_t _RXDFE_RCUT_full(uint8_t srate, bool subGHz)
{
    switch (srate) {
    case FSK_SRATE_50K:
    case FSK_SRATE_100K:
    case FSK_SRATE_300K:
        return RF_RCUT_FS_BY_5P3;
    case FSK_SRATE_150K:
    case FSK_SRATE_400K:
        return subGHz ? RF_RCUT_FS_BY_5P3 : RF_RCUT_FS_BY_4;
    case FSK_SRATE_200K:
        return subGHz ? RF_RCUT_FS_BY_4 : RF_RCUT_FS_BY_2P6;
    }

    return 0;
}

/* Table 6-64. Minimum Preamble Length */
static uint8_t _FSKPL(uint8_t srate)
{
    switch (srate) {
    case FSK_SRATE_50K:
        return 2;
    case FSK_SRATE_100K:
        return 3;
    case FSK_SRATE_150K:
    case FSK_SRATE_200K:
    case FSK_SRATE_300K:
        return 8;
    case FSK_SRATE_400K:
        return 10;
    }

    return 0;
}

/* fsk modulation indices / 8 */
static const uint8_t fsk_mod_idx[] = {
    3, 4, 6, 8, 10, 12, 14, 16
};

/* FSK modulation scale / 8 */
static const uint8_t fsk_mod_idx_scale[] = {
    7, 8, 9, 10
};

static void _fsk_mod_idx_get(uint8_t num, uint8_t *idx, uint8_t *scale)
{
    uint8_t diff = 0xFF;
    for (uint8_t i = 0; i < ARRAY_SIZE(fsk_mod_idx_scale); ++i) {
        for (uint8_t j = 0; j < ARRAY_SIZE(fsk_mod_idx); ++j) {
            if (abs(num - fsk_mod_idx_scale[i] * fsk_mod_idx[j]) < diff) {
                diff   = abs(num - fsk_mod_idx_scale[i] * fsk_mod_idx[j]);
                *idx   = j;
                *scale = i;
            }
        }
    }
}

static inline uint8_t _RXDFE_RCUT(uint8_t srate, bool subGHz, bool half)
{
    return half ? _RXDFE_RCUT_half(srate, subGHz) : _RXDFE_RCUT_full(srate, subGHz);
}

static void _set_srate(at86rf215_t *dev, uint8_t srate, bool mod_idx_half)
{
    /* Set Receiver Bandwidth: fBW = 160 kHz, fIF = 250 kHz */
    at86rf215_reg_write(dev, dev->RF->RG_RXBWC, _RXBWC_BW(srate, is_subGHz(dev), mod_idx_half));
    /* fS = 400 kHz; fCUT = fS/5.333 = 75 kHz */
    at86rf215_reg_write(dev, dev->RF->RG_RXDFE, _RXDFE_SR(srate)
                                              | _RXDFE_RCUT(srate, is_subGHz(dev), mod_idx_half));
    /* Power Amplifier Ramp Time = 32 µs; fLPCUT = 80 kHz */
    at86rf215_reg_write(dev, dev->RF->RG_TXCUTC, _TXCUTC_PARAMP(srate)
                                               | _TXCUTC_LPFCUT(srate, mod_idx_half));
    /* fS = 500 kHz; fCUT = fS/2 = 250 kHz */
    at86rf215_reg_write(dev, dev->RF->RG_TXDFE, _TXDFE_SR(dev, srate)
                                              | (mod_idx_half ? RF_RCUT_FS_BY_8 : RF_RCUT_FS_BY_2)
                                              | TXDFE_DM_MASK);

    /* configure pre-emphasis */
    at86rf215_reg_write(dev, dev->BBC->RG_FSKPE0, FSKPE_Val[0][srate]);
    at86rf215_reg_write(dev, dev->BBC->RG_FSKPE1, FSKPE_Val[1][srate]);
    at86rf215_reg_write(dev, dev->BBC->RG_FSKPE2, FSKPE_Val[2][srate]);

    /* set preamble length in octets */
    at86rf215_reg_write(dev, dev->BBC->RG_FSKPLL, _FSKPL(srate));

    /* set symbol rate, preamble is less than 256 so set high bits 0 */
    at86rf215_reg_write(dev, dev->BBC->RG_FSKC1, srate);
}

static void _set_ack_timeout(at86rf215_t *dev, uint8_t srate, bool mord4, bool fec)
{
    dev->ack_timeout_usec =  10 * AT86RF215_ACK_PERIOD_IN_SYMBOLS * 100 / at86rf215_fsk_srate_10kHz[srate];
    /* forward error correction halves data rate */
    dev->ack_timeout_usec <<= fec;
    /* 4-FSK doubles data rate */
    dev->ack_timeout_usec >>= mord4;

    DEBUG("[%s] ACK timeout: %"PRIu32" µs\n", "FSK", dev->ack_timeout_usec);
}

void at86rf215_configure_FSK(at86rf215_t *dev, uint8_t srate, uint8_t mod_idx, uint8_t mod_order, uint8_t fec)
{
    if (srate > FSK_SRATE_400K) {
        DEBUG("[%s] invalid symbol rate: %d\n", __func__, srate);
        return;
    }

    /* make sure we are in state TRXOFF */
    uint8_t old_state = at86rf215_set_state(dev, CMD_RF_TRXOFF);

    bool mod_idx_half = mod_idx <= 32;
    uint8_t _mod_idx, _mod_idx_scale;
    _fsk_mod_idx_get(mod_idx, &_mod_idx, &_mod_idx_scale);

    /* disable radio */
    at86rf215_reg_write(dev, dev->BBC->RG_PC, 0);

    _set_srate(dev, srate, mod_idx_half);

    /* set receiver gain target according to data sheet */
    at86rf215_reg_write(dev, dev->RF->RG_AGCS, 1 << AGCS_TGT_SHIFT);
    /* enable automatic receiver gain */
    at86rf215_reg_write(dev, dev->RF->RG_AGCC, AGCC_EN_MASK);

    /* set channel spacing, same for both sub-GHz & 2.4 GHz */
    at86rf215_reg_write(dev, dev->RF->RG_CS, FSK_CHANNEL_SPACING / 25);

    /* set center frequency */
    if (is_subGHz(dev)) {
        at86rf215_reg_write16(dev, dev->RF->RG_CCF0L, FSK_CENTER_FREQUENCY_SUBGHZ / 25);
    } else {
        at86rf215_reg_write16(dev, dev->RF->RG_CCF0L, FSK_CENTER_FREQUENCY_24GHZ / 25);
    }

    /* set Bandwidth Time Product, Modulation Index & Modulation Order */
    /* effective modulation index = MIDXS * MIDX */
    at86rf215_reg_write(dev, dev->BBC->RG_FSKC0, FSK_BT_20
                                               | (_mod_idx << FSKC0_MIDX_SHIFT)
                                               | (_mod_idx_scale << FSKC0_MIDXS_SHIFT)
                                               | mod_order);

    /* enable direct modulation */
    at86rf215_reg_write(dev, dev->BBC->RG_FSKDM, FSKDM_EN_MASK | FSKDM_PE_MASK);

    /* set forward error correction */
    at86rf215_FSK_set_fec(dev, fec);

    at86rf215_enable_radio(dev, BB_MRFSK);

    /* assume channel spacing for mode #1 */
    dev->num_chans = is_subGHz(dev) ? 34 : 416;
    dev->netdev.chan = at86rf215_chan_valid(dev, dev->netdev.chan);
    at86rf215_reg_write16(dev, dev->RF->RG_CNL, dev->netdev.chan);

    _set_ack_timeout(dev, srate, mod_order, fec);

    at86rf215_set_state(dev, old_state);
}

uint8_t at86rf215_FSK_get_mod_order(at86rf215_t *dev)
{
    return at86rf215_reg_read(dev, dev->BBC->RG_FSKC0) & FSKC0_MORD_MASK;
}

int at86rf215_FSK_set_mod_order(at86rf215_t *dev, uint8_t mod_order) {
    if (mod_order) {
        at86rf215_reg_or(dev, dev->BBC->RG_FSKC0, FSK_MORD_4SFK);
    } else {
        at86rf215_reg_and(dev, dev->BBC->RG_FSKC0, ~FSK_MORD_4SFK);
    }

    _set_ack_timeout(dev, at86rf215_FSK_get_srate(dev),
                     mod_order,
                     at86rf215_FSK_get_fec(dev));
    return 0;
}

uint8_t at86rf215_FSK_get_mod_idx(at86rf215_t *dev)
{
    uint8_t fskc0 = at86rf215_reg_read(dev, dev->BBC->RG_FSKC0);
    uint8_t _mod_idx = (fskc0 & FSKC0_MIDX_MASK) >> FSKC0_MIDX_SHIFT;
    uint8_t _mod_idx_scale = (fskc0 & FSKC0_MIDXS_MASK) >> FSKC0_MIDXS_SHIFT;

    return fsk_mod_idx[_mod_idx] * fsk_mod_idx_scale[_mod_idx_scale];
}

int at86rf215_FSK_set_mod_idx(at86rf215_t *dev, uint8_t mod_idx)
{
    uint8_t _mod_idx, _mod_idx_scale;

    _set_srate(dev, at86rf215_FSK_get_srate(dev), mod_idx <= 32);

    _fsk_mod_idx_get(mod_idx, &_mod_idx, &_mod_idx_scale);
    at86rf215_reg_write(dev, dev->BBC->RG_FSKC0, FSK_BT_20
                                               | (_mod_idx << FSKC0_MIDX_SHIFT)
                                               | (_mod_idx_scale << FSKC0_MIDXS_SHIFT)
                                               | at86rf215_FSK_get_mod_order(dev));
    return 0;
}

uint8_t at86rf215_FSK_get_srate(at86rf215_t *dev)
{
    return at86rf215_reg_read(dev, dev->BBC->RG_FSKC1) & FSKC1_SRATE_MASK;
}

int at86rf215_FSK_set_srate(at86rf215_t *dev, uint8_t srate)
{
    if (srate > FSK_SRATE_400K) {
        return -1;
    }

    _set_srate(dev, srate, at86rf215_FSK_get_mod_idx(dev) <= 32);
    _set_ack_timeout(dev, srate,
                     at86rf215_FSK_get_mod_order(dev),
                     at86rf215_FSK_get_fec(dev));
    return 0;
}

int at86rf215_FSK_set_fec(at86rf215_t *dev, uint8_t mode)
{
    (void) dev;
    switch (mode) {
    case IEEE802154_FEC_NONE:
        at86rf215_reg_and(dev, dev->BBC->RG_FSKPHRTX, ~FSKPHRTX_SFD_MASK);
        break;
    case IEEE802154_FEC_NRNSC:
        at86rf215_reg_or(dev, dev->BBC->RG_FSKPHRTX, FSKPHRTX_SFD_MASK);
        at86rf215_reg_and(dev, dev->BBC->RG_FSKC2, ~FSKC2_FECS_MASK);
        break;
    case IEEE802154_FEC_RSC:
        at86rf215_reg_or(dev, dev->BBC->RG_FSKPHRTX, FSKPHRTX_SFD_MASK);
        at86rf215_reg_or(dev, dev->BBC->RG_FSKC2, FSKC2_FECS_MASK);
        break;
    default:
        return -1;
    }

    _set_ack_timeout(dev, at86rf215_FSK_get_srate(dev),
                     mode,
                     at86rf215_FSK_get_mod_order(dev));

    return 0;
}

uint8_t at86rf215_FSK_get_fec(at86rf215_t *dev)
{
    /* SFD0 -> Uncoded IEEE mode */
    /* SFD1 -> Coded IEEE mode */
    if (!(at86rf215_reg_read(dev, dev->BBC->RG_FSKPHRTX) & FSKPHRTX_SFD_MASK)) {
        return IEEE802154_FEC_NONE;
    }

    if (at86rf215_reg_read(dev, dev->BBC->RG_FSKC2) & FSKC2_FECS_MASK) {
        return IEEE802154_FEC_RSC;
    } else {
        return IEEE802154_FEC_NRNSC;
    }
}
