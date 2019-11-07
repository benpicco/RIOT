/*
 * Copyright (C) 2019 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_lpc2387
 * @ingroup     drivers_periph_pm
 * @{
 *
 * @file
 * @brief       Implementation of the kernels power management interface
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include "periph/pm.h"

#define ENABLE_DEBUG (0)

#if ENABLE_DEBUG
#define DEBUG(s) puts(s)
#else
#define DEBUG(s)
#endif

void pm_set(unsigned mode)
{
    switch (mode) {
        case 0:
            DEBUG("pm_set(): setting Deep Power Down mode.");
            PCON |= PM_DEEP_POWERDOWN;
            break;
        case 1:
            DEBUG("pm_set(): setting Power Down mode.");
            PCON |= PM_POWERDOWN;
            break;
        case 2:
            DEBUG("pm_set(): setting Sleep mode.");
            PCON |= PM_SLEEP;
            break;
        default: /* Falls through */
        case 3:
            DEBUG("pm_set(): setting Idle mode.");
            PCON |= PM_IDLE;
            break;
    }
}
