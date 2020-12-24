/*
 * Copyright 2019 Otto-von-Guericke-Universität Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_dds
 * @{
 *
 * @file
 * @brief       DDS implementation
 *
 * @author      Marian Buschsieweke <marian.buschsieweke@ovgu.de>
 *
 * @}
 */

#include <errno.h>
#include <string.h>

#include "assert.h"
#include "dds.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static void dds_cb(void *_dev, int channel)
{
    dds_t *dev = _dev;
    (void)channel;
    if (!dev->loops) {
        if (dev->flags & DDS_FLAG_POWERSAFE) {
            pwm_poweroff(dev->pwm);
        }
        else {
            pwm_set(dev->pwm, dev->channel, 0);
        }
        if (dev->flags & DDS_FLAG_BLOCKING) {
            /* unblock caller of dds_play() */
            mutex_unlock(&dev->signal);
        }
    }
    else {
        size_t pos = dev->pos >> 8;
        pwm_set(dev->pwm, dev->channel, dev->sample[pos] << 2);
        dev->pos += dev->step;
        timer_set(dev->timer, 0, dev->timeout);
        if ((dev->pos >> 8) >= dev->sample_len) {
            dev->pos = 0;
            dev->loops--;
        }
    }
}

int dds_init(dds_t *dev, const dds_params_t *params)
{
    assert(dev && params);
    uint32_t freq = pwm_init(params->pwm, params->mode, params->pwm_freq, 256);

    if (!freq) {
        DEBUG_PUTS("[dds] Failed to init PWM");
        return -EIO;
    }

    if (timer_init(params->timer, params->timer_freq, dds_cb, dev)) {
        DEBUG_PUTS("[dds] Failed to init timer");
        return -EIO;
    }

    if (dev->flags & DDS_FLAG_POWERSAFE) {
        /* Keep the PWM device off until playback is requested */
        pwm_poweroff(params->pwm);
    }

    memset(dev, 0x00, sizeof(dds_t));
    dev->pwm = params->pwm;
    dev->timer = params->timer;
    dev->channel = params->channel;
    dev->flags = params->power_safe ? DDS_FLAG_POWERSAFE : 0;
    dev->timeout = params->timer_freq / 14080 - params->ticks_overhead;
    const mutex_t locked_mutex = MUTEX_INIT_LOCKED;
    dev->signal = locked_mutex;

    DEBUG("[dds] PWM: %" PRIu32 "Hz, Timer: %" PRIu32 "Hz, timeout: %u\n",
          freq, params->timer_freq, dev->timeout);

    return 0;
}

void dds_play(dds_t *dev, const void *sample, size_t len, uint16_t freq,
              uint16_t duration_ms, dds_mode_t mode)
{
    assert(mode < DDS_MODE_NUMOF);
    timer_clear(dev->timer, 0);
    if (dev->flags & DDS_FLAG_POWERSAFE) {
        pwm_poweron(dev->pwm);
    }

    dev->sample = sample;
    dev->sample_len = len;
    dev->pos = 0;
    dev->loops = ((uint32_t)duration_ms * freq * 32LU) / (len * 1000LU);
    dev->step = ((uint32_t)freq << 8) / 440;
    DEBUG("[dds] Playing %u loops of sample at %p (%uB) with step %u\n",
          (unsigned)dev->loops, sample, (unsigned)len, (unsigned)dev->step);

    if (mode == DDS_MODE_ASYNC) {
        dev->flags &= ~DDS_FLAG_BLOCKING;
        timer_set(dev->timer, 0, dev->timeout);
    }
    else {
        dev->flags |= DDS_FLAG_BLOCKING;
        timer_set(dev->timer, 0, dev->timeout);
        mutex_lock(&dev->signal);
    }
}
