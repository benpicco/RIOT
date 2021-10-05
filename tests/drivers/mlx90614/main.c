/*
 * Copyright (C) 2021 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_mlx90614
 *
 * @{
 * @file
 * @brief       Test program for the driver of the MLX90614 temperature sensor
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include "mlx90614.h"
#include "mlx90614_params.h"
#include "test_utils/expect.h"
#include "fmt.h"
#include "ztimer.h"

#define MLX90614_NUMOF  ARRAY_SIZE(mlx90614_params)

int main(void)
{
    mlx90614_t dev[MLX90614_NUMOF];

    /* initialize all sensors */
    for (unsigned i = 0; i < MLX90614_NUMOF; ++i) {
        mlx90614_init(&dev[i], &mlx90614_params[i]);
    }

    while (1) {
        for (unsigned i = 0; i < MLX90614_NUMOF; ++i) {
            int res;
            uint32_t temp;
            char outstr[8];

            printf("DEV%u:", i);

            res = mlx90614_read_temperature(&dev[i], MLX90614_TEMP_AMBIENT, &temp);
            expect(res == 0);
            fmt_s32_dfp(outstr, temp, -2);
            printf("\tambient: %s K", outstr);

            res = mlx90614_read_temperature(&dev[i], MLX90614_TEMP_OBJ1, &temp);
            expect(res == 0);
            fmt_s32_dfp(outstr, temp, -2);
            printf("\tOBJ1: %s K", outstr);

            res = mlx90614_read_temperature(&dev[i], MLX90614_TEMP_OBJ2, &temp);
            expect(res == 0);
            fmt_s32_dfp(outstr, temp, -2);
            printf("\tOBJ2: %s K", outstr);
        }
        ztimer_sleep(ZTIMER_MSEC, 250);
    }

    return 0;
}
