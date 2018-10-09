/*
 * Copyright (C) 2018 Gunar Schorcht
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @brief       Test application for InvenSense ITG320X 3-axis gyroscope
 * @author      Gunar Schorcht <gunar@schorcht.net>
 * @file
 *
 * The test application demonstrates the use of the ITG320X. It uses the
 * default configuration parameters:
 *
 * - Low pass filter bandwidth (LPF_BW) of 5 Hz (#ITG320X_LPF_BW_5)
 * - Internal sample rate (ISR) of 1 kHz which results from LPF_BW of 5 Hz
 * - Internal sample rate divider (ISR_DIV) of 9 (#ITG320X_PARAM_ISR_DIV)
 * - Output data rate (ODR) of 100 Hz resulting from
 *
 *      ODR = ISR / (ISR_DIV + 1)
 *
 * The application may use two different approaches to retrieve new data,
 * either
 *
 * - periodically fetching the data at a rate lower than the sensor's
 *   output data rate (ODR), or
 * - fetching the data when the data-ready interrupt is triggered.
 *
 * To use the latter approach, the according GPIO to which the sensor's
 * **INT** output pin is connected has to be configured and initialized
 * by the application. In this test application, this is done by defining
 * ```USE_ITG320X_DRDY``` and overrding the default configuration
 * parameter ```ITG320X_PARAM_INT_PIN``` if necessary, for example:
 *
 * ```
 * CFLAGS="-DUSE_ITG320X_DRDY -DITG320X_PARAM_INT_PIN=\(GPIO_PIN\(0,3\)\)" \
 * make flash -C tests/driver_itg320x BOARD=...
 * ```
*/

#include <stdio.h>

#include "thread.h"
#include "xtimer.h"

#include "itg320x.h"
#include "itg320x_params.h"

#define SLEEP   (100 * US_PER_MS)

kernel_pid_t p_main;

#if USE_ITG320X_DRDY
static void itg320x_isr_data_ready (void *arg)
{
    (void)arg;
    /* send a message to trigger main thread to handle the interrupt */
    msg_t msg;
    msg_send(&msg, p_main);
}
#endif


int main(void)
{
    itg320x_t dev;

    p_main = sched_active_pid;

    puts("ITG320X gyroscope driver test application\n");
    puts("Initializing ITG320X sensor");

    /* initialize the sensor with default configuration parameters */
    if (itg320x_init(&dev, &itg320x_params[0]) == ITG320X_OK) {
        puts("[OK]\n");
    }
    else {
        puts("[Failed]");
        return 1;
    }

    #if USE_ITG320X_DRDY
    /* init INT2/DRDY signal pin and enable the interrupt */
    gpio_init_int(itg320x_params[0].int_pin, GPIO_IN, GPIO_RISING,
                  itg320x_isr_data_ready, 0);
    itg320x_enable_int(&dev, true);
    #endif /* USE_ITG320X_DRDY */

    while (1) {
        #if USE_ITG320X_DRDY
        /* wait for data ready interrupt */
        msg_t msg;
        msg_receive(&msg);
        #else
        /* wait longer than period of ITG320X DOR */
        xtimer_usleep(SLEEP);
        #endif

        /* read and print data in any case */
        itg320x_data_t data;
        if (itg320x_read(&dev, &data) == ITG320X_OK) {
            printf("gyro [mdps] x: "
                    "%+8" PRIi32 " y: %+8" PRIi32 " z: %+8" PRIi32 "\n",
                    data.x, data.y, data.z);
        }
    }

    return 0;
}
