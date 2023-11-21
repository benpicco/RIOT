/*
 * Copyright (C) 2023 ML!PA Consulting GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_mcp9601
 * @{
 *
 * @file
 * @brief       SAUL adaption for mcp9601 compatible device
 *
 * @author      Benjamin Valentin <benjamin.valentin@ml-pa.com>
 *
 * @}
 */

#include <string.h>

#include "saul.h"
#include "mcp9601.h"
#include "mcp9601_regs.h"

static int read_temperature(const void *dev, phydat_t *res)
{
    res->val[0] = mcp9601_get_temperature(dev, MCP9601_REGS_TH);
    res->val[1] = mcp9601_get_temperature(dev, MCP9601_REGS_TD);
    res->val[2] = mcp9601_get_temperature(dev, MCP9601_REGS_TC);
    res->unit = UNIT_TEMP_C;
    res->scale = -2;
    return 3;
}

const saul_driver_t mcp9601_temperature_saul_driver = {
    .read = read_temperature,
    .write = saul_write_notsup,
    .type = SAUL_SENSE_TEMP
};
