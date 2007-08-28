/*
 * include/asm/keywest.h
 *
 * Hitachi KeyWest Platform support 
 *
 * Copyright (C) 2001 Lineo, Japan
 *
 * This file is subject to the terms and conditions of the GNU General Publi
c
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#ifndef _ASM_KEYWEST_H_
#define _ASM_KEYWEST_H_

#include <asm/irq.h>
#include <asm/hd64465.h>

/* 7751 Internal IRQ's used by external CPLD controller */
#define KEYWEST_IRQ_LOW	0
#define KEYWEST_IRQ_NUM  14         /* External CPLD level 1 IRQs */
#define KEYWEST_IRQ_HIGH (KEYWEST_IRQ_LOW + KEYWEST_IRQ_NUM)
#define KEYWEST_2NDLVL_IRQ_LOW   (HD64465_IRQ_BASE+HD64465_IRQ_NUM)  
#define KEYWEST_2NDLVL_IRQ_NUM   32 /* Level 2 IRQs = 4 regs * 8 bits */
#define KEYWEST_2NDLVL_IRQ_HIGH  (KEYWEST_2NDLVL_IRQ_LOW + \
                                 KEYWEST_2NDLVL_IRQ_NUM)


/* CPLD registers and external chip addresses */
//#define KEYWEST_HD64464_ADDR	0xB2000000
#define KEYWEST_DGDR	0xB1FFE000
#define KEYWEST_BIDR	0xB1FFD000
#define KEYWEST_CSLR	0xB1FFC000
#define KEYWEST_SW1R	0xB1FFB000
#define KEYWEST_DBGR	0xB1FFA000
#define KEYWEST_BDTR	0xB1FF9000
#define KEYWEST_BDRR	0xB1FF8000
#define KEYWEST_ETHR    0xB1FE0000

#define KEYWEST_V320USC_ADDR  0xB1000000
#define KEYWEST_HD64465_ADDR  0xB0000000
#define KEYWEST_INTERNAL_BASE 0xB0000000

/* SMC ethernet card parameters */
//#define KEYWEST_ETHER_IOPORT		0x220
#define KEYWEST_ETHER_IOPORT		0x0

/* IDE register paramters */
#define KEYWEST_IDECMD_IOPORT	0x1f0
#define KEYWEST_IDECTL_IOPORT	0x1f8

/* PCI: default LOCAL memory window sizes (seen from PCI bus) */
#define KEYWEST_LSR0_SIZE    (64*(1<<20)) //64MB
#define KEYWEST_LSR1_SIZE    (64*(1<<20)) //64MB

#endif /* _ASM_KEYWEST_H_ */
