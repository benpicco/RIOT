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

#include "hd44780.h"
#include "hd44780_params.h"

#include "periph/adc.h"
#include "periph/rtc.h"

#include "shell.h"
#include "shell_commands.h"
#include "sound.h"
#include "thread.h"
#include "xtimer.h"

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

static void print_time_and_date(hd44780_t *dev, struct tm *now)
{
    hd44780_clear(dev);
    hd44780_set_cursor(dev, 0, 0);

    print_num(dev, now->tm_hour, 1);
    hd44780_write(dev, ':');
    print_num(dev, now->tm_min, 1);
    hd44780_write(dev, ':');
    print_num(dev, now->tm_sec, 1);

    hd44780_set_cursor(dev, 0, 1);
    print_num(dev, now->tm_mday, 1);
    hd44780_write(dev, '.');
    print_num(dev, now->tm_mon + 1, 1);
    hd44780_write(dev, '.');
    print_num(dev, now->tm_year + 1900, 1);
}

static inline void _led_display(uint8_t val)
{
    FIO_PORTS[2].CLR = 0xFF;
    FIO_PORTS[2].SET = val;
}

/* show blink pattern on the LEDs */
static void blinky(void)
{
    static uint8_t cur;
    static int8_t inc = 1;
    const uint8_t pattern[] = {
        0x81, 0x42, 0x24, 0x18
    };

    _led_display(pattern[cur]);

    if ((uint8_t)(cur + inc) >= sizeof(pattern)) {
        inc = -inc;
    }

    cur += inc;
}

static void* display_thread(void *ctx)
{
    hd44780_t dev;
    /* init display */
    if (hd44780_init(&dev, &hd44780_params[0]) != 0) {
        puts("init display failed");
        return NULL;
    }

    gpio_init(BTN0_PIN, BTN0_MODE);

    adc_init(0);

    struct tm rtc_now;
    rtc_get_time(&rtc_now);

    xtimer_ticks32_t now = xtimer_now();

    uint8_t cooldown  = 0;
    uint8_t state = 0;
    for (unsigned i = 0; state < 7; ++i) {
        bool blink = (i >> 4) & 0x1;

        switch (state) {
        case 0:
            rtc_now.tm_hour = read_adc_knob(24);
            input_time(&dev, &rtc_now, state, blink);
            break;
        case 1:
            rtc_now.tm_min = read_adc_knob(60);
            input_time(&dev, &rtc_now, state, blink);
            break;
        case 2:
            rtc_now.tm_sec = read_adc_knob(60);
            input_time(&dev, &rtc_now, state, blink);
            break;
        case 3:
            rtc_now.tm_mday = read_adc_knob(31) + 1;
            input_date(&dev, &rtc_now, state, blink);
            break;
        case 4:
            rtc_now.tm_mon = read_adc_knob(12);
            input_date(&dev, &rtc_now, state, blink);
            break;
        case 5:
            rtc_now.tm_year = read_adc_knob(100) + 100;
            input_date(&dev, &rtc_now, state, blink);
            break;
        default:
            rtc_get_time(&rtc_now);
            print_time_and_date(&dev, &rtc_now);
        }

        if (cooldown) {
            --cooldown;
        }

        if (!gpio_read(BTN0_PIN) && !cooldown) {
            cooldown = 10;

            if (state == 5) {
                rtc_set_time(&rtc_now);
                sound_play_blip();
            } else {
                sound_play_short_blip();
            }

            if (state < 6) {
                ++state;
            }
        }

        if (state > 5) {
            _led_display(rtc_now.tm_sec);
        } else if ((i & 0x7) == 0x7) {
            blinky();
        }

        xtimer_periodic_wakeup(&now, FRAME_TIME);
    }

    return ctx;
}

static char display_stack[THREAD_STACKSIZE_DEFAULT];

int main(void)
{
    sound_init();
    sound_play_greeting();

    thread_create(display_stack, sizeof(display_stack),
                  THREAD_PRIORITY_MAIN, 0, display_thread,
                  NULL, "display");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
