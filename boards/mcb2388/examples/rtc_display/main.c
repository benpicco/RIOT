/*
 * Copyright (C) 2020 Beuth Hochschule für Technik Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_mcb2388
 * @{
 *
 * @file
 * @brief       Test application for Arduino LCM1602C LCD
 *
 * @author      Benjamin Valentin <benpicco@beuth-hochschule.de>
 *
 * @}
 */

#include <stdio.h>

#include "xtimer.h"
#include "hd44780.h"
#include "hd44780_params.h"

#include "periph/adc.h"
#include "periph/rtc.h"

#include "sound.h"

#define FRAME_TIME (1000000/50)

static void print_num(hd44780_t *dev, unsigned num, bool print)
{
    char buffer[5];
    int res = snprintf(buffer, sizeof(buffer), "%02d", num);
    if (print) {
        hd44780_print(dev, buffer);
    } else while (res--) {
        hd44780_write(dev, ' ');
    }
}

static uint8_t read_adc_knob(uint8_t max_val)
{
    int sample = adc_sample(0, ADC_RES_6BIT);
    return (max_val * sample) >> 16;
}

static void input_time(hd44780_t *dev, struct tm *now, unsigned comp, bool blink)
{
    hd44780_clear(dev);
    hd44780_set_cursor(dev, 0, 0);

    print_num(dev, now->tm_hour, comp != 0 || blink);
    hd44780_write(dev, ':');
    print_num(dev, now->tm_min, comp != 1 || blink);
    hd44780_write(dev, ':');
    print_num(dev, now->tm_sec, comp != 2 || blink);

    hd44780_set_cursor(dev, 0, 1);
    hd44780_print(dev, "Zeit eingeben");
}

static void input_date(hd44780_t *dev, struct tm *now, unsigned comp, bool blink)
{
    hd44780_clear(dev);
    hd44780_set_cursor(dev, 0, 0);

    print_num(dev, now->tm_mday, comp != 3 || blink);
    hd44780_write(dev, '.');
    print_num(dev, now->tm_mon + 1, comp != 4 || blink);
    hd44780_write(dev, '.');
    print_num(dev, now->tm_year + 1900, comp != 5 || blink);

    hd44780_set_cursor(dev, 0, 1);
    hd44780_print(dev, "Datum eingeben");
}

int main(void)
{
    hd44780_t dev;
    /* init display */
    if (hd44780_init(&dev, &hd44780_params[0]) != 0) {
        puts("init display failed");
        return 1;
    }

    gpio_init(BTN0_PIN, BTN0_MODE);

    adc_init(0);
    sound_init();

    struct tm rtc_now;
    rtc_get_time(&rtc_now);

    xtimer_ticks32_t now = xtimer_now();

    sound_play_greeting();

    uint8_t cooldown  = 0;
    uint8_t component = 0;
    for (unsigned i = 0; component < 6; ++i) {
        bool blink = (i >> 4) & 0x1;

        switch (component) {
        case 0:
            rtc_now.tm_hour = read_adc_knob(24);
            input_time(&dev, &rtc_now, component, blink);
            break;
        case 1:
            rtc_now.tm_min = read_adc_knob(60);
            input_time(&dev, &rtc_now, component, blink);
            break;
        case 2:
            rtc_now.tm_sec = read_adc_knob(60);
            input_time(&dev, &rtc_now, component, blink);
            break;
        case 3:
            rtc_now.tm_mday = read_adc_knob(31) + 1;
            input_date(&dev, &rtc_now, component, blink);
            break;
        case 4:
            rtc_now.tm_mon = read_adc_knob(12);
            input_date(&dev, &rtc_now, component, blink);
            break;
        case 5:
            rtc_now.tm_year = read_adc_knob(100) + 100;
            input_date(&dev, &rtc_now, component, blink);
            break;
        }

        if (cooldown) {
            --cooldown;
        }

        if (!gpio_read(BTN0_PIN) && !cooldown) {
            cooldown = 10;
            ++component;
            sound_play_short_blip();
        }

        xtimer_periodic_wakeup(&now, FRAME_TIME);
    }

    sound_play_blip();

    return 0;
}
