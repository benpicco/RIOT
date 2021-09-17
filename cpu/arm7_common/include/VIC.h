/* Copyright (C) 2005, 2006, 2007, 2008 by Thomas Hillebrandt and Heiko Will
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

#ifndef VIC_H
#define VIC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup         cpu_arm7_common
 * @{
 */

#define I_Bit           0x80
#define F_Bit           0x40

#define SYS32Mode       0x1F
#define IRQ32Mode       0x12
#define FIQ32Mode       0x11

#define INTMode         (FIQ32Mode | IRQ32Mode)

/**
 * @name    IRQ Priority Mapping
 */
/** @{ */
#define HIGHEST_PRIORITY    0x01
#define IRQP_RTIMER         1   /* FIQ_PRIORITY // TODO: investigate problems with rtimer and FIQ */
#define IRQP_TIMER1         1
#define IRQP_WATCHDOG       1
#define IRQP_CLOCK          3
#define IRQP_GPIO           4
#define IRQP_RTC            8
#define LOWEST_PRIORITY     0x0F
/** @} */

#define VECT_ADDR_INDEX 0x100
#define VECT_CNTL_INDEX 0x200

#include <stdbool.h>
#include "cpu.h"

bool cpu_install_irq(int IntNumber, void *HandlerAddr, int Priority);

#ifdef __cplusplus
}
#endif

/** @} */
#endif /* VIC_H */
