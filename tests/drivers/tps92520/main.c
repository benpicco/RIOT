/*
 * SPDX-FileCopyrightText: 2026 ML!PA Consulting GmbH
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief      Test application for the TPS92520
 *
 * @author     Benjamin Valentin <benjamin.valentin@ml-pa.com>
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include "shell.h"
#include "tps92520.h"
#include "ztimer.h"

#include "macros/units.h"

static tps92520_t dev;

int main(void)
{
    const tps92520_params_t params = {
        .spi     = SPI_DEV(1),
        .spi_clk = KHZ(250),
        .cs_pin  = GPIO_PIN(PC, 6),
    };

    gpio_init(GPIO_PIN(PD, 0), GPIO_OUT);
    gpio_init(GPIO_PIN(PB, 1), GPIO_OUT);
    gpio_init(GPIO_PIN(PB, 6), GPIO_OUT);

    ztimer_sleep(ZTIMER_MSEC, 100);

    gpio_set(GPIO_PIN(PB, 6));
    gpio_set(GPIO_PIN(PB, 1));
    gpio_set(GPIO_PIN(PD, 0));

    ztimer_sleep(ZTIMER_MSEC, 10);

    int res = tps92520_init(&dev, &params);

    int temp = tps92520_get_temperature(&dev);

    printf("%d m°C\n", temp);

    printf("5V: %d mV\n", tps92520_get_5V(&dev));
    printf("Vin[0]: %d mV\n", tps92520_get_led_voltage(&dev, TPS92520_CH1VIN));
    printf("Vin[1]: %d mV\n", tps92520_get_led_voltage(&dev, TPS92520_CH2VIN));

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run_once(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* turn off chip */
    gpio_clear(GPIO_PIN(PB, 0));

    return res;
}

static int cmd_set_current(int argc, char **argv)
{
    uint8_t chan;
    uint16_t current;

    chan = atoi(argv[1]);
    current = atoi(argv[2]);

    if (argc < 3 || chan > 1) {
        printf("usage: %s <chan> <current>\n", argv[0]);
        return -1;
    }

    tps92520_disable(&dev, chan);

    if (current) {

        tps92520_set_current(&dev, chan, current);
        tps92520_enable(&dev, chan);
    }

    return 0;
}

static int cmd_get_vled(int argc, char **argv)
{
    uint8_t chan;

    chan = atoi(argv[1]);
    const char *types[] = {
        "in",
        "led",
        "led_on",
        "led_off",
    };

    int type = -1;
    for (unsigned i = 0; i < ARRAY_SIZE(types); ++i) {
        if (!strcmp(types[i], argv[2])) {
            type = i;
            break;
        }
    }

    if (argc < 3 || chan > 1 || type < 0) {
        printf("usage: %s <chan> <type>\n", argv[0]);
        printf("\twhere <type> can be '%s'", types[0]);
        for (unsigned i = 1; i < ARRAY_SIZE(types); ++i) {
            printf(",'%s' ", types[i]);
        }
        puts("");
        return -1;
    }

    tps92520_chan_t adc = (chan ? TPS92520_CH2VIN : TPS92520_CH1VIN)
                        + type;
    int v = tps92520_get_led_voltage(&dev, adc);
    printf("CH%d%s: %d mV\n", chan, types[type], v);

    return 0;
}

static int cmd_get_temp(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("%d m°C\n", tps92520_get_temperature(&dev));

    return 0;
}

static int cmd_get_state(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint8_t state[3];

    tps92520_get_state(&dev, state);
    printf("state: %02x %02x %02x\n", state[0], state[1], state[2]);

    return 0;
}

SHELL_COMMAND(set_current, "set LED current", cmd_set_current);
SHELL_COMMAND(get_vled, "get LED voltage", cmd_get_vled);
SHELL_COMMAND(get_temp, "get temperature", cmd_get_temp);
SHELL_COMMAND(get_state, "", cmd_get_state);
