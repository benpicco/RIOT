/*
 * Copyright (C) 2021 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_auto_init_saul
 * @{
 *
 * @file
 * @brief       Auto initialization of mcp9601 compatible driver.
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include "assert.h"
#include "log.h"
#include "saul_reg.h"
#include "mcp9601.h"
#include "mcp9601_params.h"

/**
 * @brief   Define the number of configured sensors
 */
#define MCP9601_NUMOF      ARRAY_SIZE(mcp9601_params)

/**
 * @brief   Allocation of memory for device descriptors
 */
static mcp9601_t mcp9601_devs[MCP9601_NUMOF];

/**
 * @brief   Memory for the SAUL registry entries
 */
static saul_reg_t saul_entries[MCP9601_NUMOF];

/**
 * @brief   Reference the driver structs.
 */
extern const saul_driver_t mcp9601_temperature_saul_driver;

void auto_init_mcp9601(void)
{
    for (unsigned i = 0; i < MCP9601_NUMOF; i++) {
        LOG_DEBUG("[auto_init_saul] initializing mcp9601 #%u\n", i);

        if (mcp9601_init(&mcp9601_devs[i], &mcp9601_params[i]) < 0) {
            LOG_ERROR("[auto_init_saul] error initializing mcp9601 #%u\n", i);
            continue;
        }

        /* temperature */
        saul_entries[i].dev = &mcp9601_devs[i];
        saul_entries[i].name = "mcp9601";
        saul_entries[i].driver = &mcp9601_temperature_saul_driver;

        /* register to saul */
        saul_reg_add(&(saul_entries[i]));
    }
}
