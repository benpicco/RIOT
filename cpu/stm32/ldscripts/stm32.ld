/*
 * Copyright (C) 2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @addtogroup      cpu_cortexm_common
 * @{
 *
 * @file
 * @brief           Memory definitions for the STM32 family
 *
 * @author          Francisco Acosta <francisco.acosta@inria.fr>
 *
 * @}
 */

ccmram_length = DEFINED( _ccmram_length ) ? _ccmram_length : 0x0 ;

MEMORY
{
    ccmram  : ORIGIN = 0x10000000, LENGTH = ccmram_length
}

SECTIONS
{
    .ccmram.bss (NOLOAD) : ALIGN(4) {
        _sccmram_bss = .;
        *(.ccmram.bss)
        _eccmram_bss = .;
        /* Round size so that we can use 4 byte copy in init */
        . = ALIGN(4);
    } > ccmram

    .heap2 ALIGN(4) (NOLOAD) :
    {
        _sheap2 = . ;
        _eheap2 = ORIGIN(ccmram) + LENGTH(ccmram);
    } > ccmram
}

INCLUDE cortexm.ld
