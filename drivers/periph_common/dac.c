/*
 * Copyright (C) 2020 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_periph_dac
 * @{
 *
 * @file
 * @brief       Common DAC function fallback implementations
 *
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 *
 * @}
 */

#include "board.h"
#include "periph/dac.h"
#include "periph/timer.h"

#ifndef PERIPH_DAC_PROVIDES_PLAY

#ifndef DAC0_TIMER
#define DAC0_TIMER   (TIMER_NUMOF - 1)
#endif
#ifndef DAC1_TIMER
#define DAC1_TIMER   (TIMER_NUMOF - 2)
#endif

static const tim_t _dac_timer[DAC_NUMOF] = {
#if DAC_NUMOF > 0
    DAC0_TIMER,
#endif
#if DAC_NUMOF > 1
    DAC1_TIMER,
#endif
};

static struct dac_ctx {
    const uint8_t *buffers[2];
    size_t buffer_len[2];
    size_t idx;
    uint8_t cur;
    uint8_t playing;
} _ctx[DAC_NUMOF];

static void _timer_cb(void *arg, int chan)
{
    const dac_cfg_t *cfg = arg;
    const dac_t dac      = cfg->line;
    struct dac_ctx *ctx  = &_ctx[dac];

    const uint8_t cur  = ctx->cur;
    const uint8_t *buf = ctx->buffers[cur];
    const size_t len   = ctx->buffer_len[cur];

    if (cfg->flags & DAC_FLAG_16BIT) {
        size_t idx_real = 2 * ctx->idx;
        dac_set(0, (buf[idx_real + 1] << 8) | buf[idx_real]);
    } else {
        dac_set(0, buf[ctx->idx] << 8);
    }

    if (++ctx->idx >= len) {

        /* invalidate old buffer */
        ctx->buffer_len[cur] = 0;

        ctx->idx = 0;
        ctx->cur = !cur;

        if (ctx->buffer_len[!cur] == 0) {
            ctx->playing = 0;
            timer_clear(_dac_timer[dac], chan);
        } else if (cfg->cb) {
            cfg->cb(cfg->cb_arg);
        }
    }
}

void dac_play(const void *buf, size_t len, const dac_cfg_t *cfg)
{
    const unsigned timer_freq  = 1000000;
    const unsigned sample_rate = cfg->sample_rate * 8;
    const dac_t dac = cfg->line;

    uint8_t next = !_ctx[dac].cur;

    _ctx[dac].buffers[next]    = buf;
    _ctx[dac].buffer_len[next] = len;

    if (_ctx[dac].playing) {
        return;
    }

    _ctx[dac].playing = 1;

    timer_init(_dac_timer[dac], timer_freq, _timer_cb, (void*) cfg);
    timer_set_periodic(_dac_timer[dac], 0, timer_freq/sample_rate,
                       TIM_FLAG_RESET_ON_MATCH | TIM_FLAG_RESET_ON_SET);
}

#endif
