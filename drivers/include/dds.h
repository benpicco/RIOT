/*
 * Copyright 2019 Otto-von-Guericke-Universität Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup    drivers_dds Direct Digital Synthesis (DDS) driver
 * @ingroup     drivers_netdev
 *
 * This module allows to generate (low quality) audio output using a PWM pin, a
 * low-pass filter (e.g. using a capacitor and a resistor), and a speaker (e.g.
 * a cheap piezo speaker).
 *
 * @{
 *
 * @file
 * @brief       DDS interface definition
 *
 * @author      Marian Buschsieweke <marian.buschsieweke@ovgu.de>
 */

#ifndef DDS_H
#define DDS_H

#include <stdint.h>

#include "mutex.h"
#include "periph/pwm.h"
#include "periph/timer.h"
#include "xtimer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DDS_FLAG_POWERSAFE  = 0x01, /**< Turn of PWM device when not used */
    DDS_FLAG_BLOCKING   = 0x02, /**< The caller of dds_play() need to be unblocked */
};

typedef enum {
    DDS_MODE_ASYNC,         /**< Return right away, generate wave in background */
    DDS_MODE_BLOCK,         /**< Block until playback completed */
    DDS_MODE_NUMOF          /**< Number of DDS modes */
} dds_mode_t;

/**
 * @brief   Initialization parameters for the DDS driver
 */
typedef struct {
    uint32_t pwm_freq;      /**< PWM frequency to use */
    uint32_t timer_freq;    /**< Timer frequency to use */
    pwm_t pwm;              /**< PWM device to use for sound generation */
    pwm_mode_t mode;        /**< PWM mode to use */
    tim_t timer;            /**< Timer to use to generate sound wave */
    uint8_t channel;        /**< PWM channel to use for sound generation */
    uint8_t ticks_overhead; /**< Timer ticks to of computation overhead */
    /**
     * @brief   If `true`, the PWM device is powered off when not generating
     *          sound.
     *
     * All other PWM channels on the same device will also be turned off. Set
     * it to `false` if you intend to use the other PWM channels as well.
     * Otherwise a value of `true` will result in lower power consumption.
     */
    bool power_safe;
} dds_params_t;

/**
 * @brief   DDS device handle
 */
typedef struct {
    pwm_t pwm;              /**< PWM device to use for sound generation */
    tim_t timer;            /**< Timer to use to generate sound wave */
    uint8_t channel;        /**< PWM channel to use for sound generation */
    uint8_t flags;          /**< Flags */
    const uint8_t *sample;  /**< Sample to play */
    size_t sample_len;      /**< Length of the sample */
    unsigned timeout;       /**< Delay between updating the PWM state */
    uint16_t pos;           /**< Current position in the sample * 2^4 */
    uint16_t loops;         /**< Remaining loops of the sample */
    uint16_t step;          /**< Step in the sample / 2^4 */
    mutex_t signal;         /**< Mutex abused to block caller until playback completes */
} dds_t;

/**
 * @brief   Initialize a DDS "device"
 *
 * @param   dev         "device" to initialize
 * @param   params      Initialization parameters
 *
 * @retval  0           Success
 * @retval  -EIO        Failed to set up PWM device
 */
int dds_init(dds_t *dev, const dds_params_t *params);

/**
 * @brief   Start playing the given audio sample
 *
 * @param   dev         DDS device to start playback on
 * @param   sample      Audio sample to play
 * @param   len         Length of the audio sample to play in bytes
 * @param   freq        Playback speed. 440 is normal playback
 * @param   duration_ms Duration of the sound in ms
 * @param   mode        DDS mode
 *
 * The audio sample must be an 8-bit mono PWM sample sampled at 14080 Hz.
 * The sample will restart until @p duration_ms has passed. Thus, the playback
 * will always end at the end of the sample.
 */
void dds_play(dds_t *dev, const void *sample, size_t len, uint16_t freq,
              uint16_t duration_ms, dds_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* DDS_H */
/** @} */
