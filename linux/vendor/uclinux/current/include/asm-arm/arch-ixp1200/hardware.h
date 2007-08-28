/*
 * linux/include/asm-arm/arch-ixp1200/hardware.h
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2002 MontaVista Software, Inc.
 * Copyright (C) 1998-1999 Russell King.
 *
 *
 * IXP1200 hardware definitions. Based on original 2.3.99-pre3 Intel code
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <linux/config.h>
#include <asm/arch/memory.h>

/*
 * Generic IXP1200 support
 */

#include "ixp1200.h"


#define UNCACHEABLE_ADDR	(ARMCSR_BASE+0x180)

#define	PCIBIOS_MIN_IO		0x54000000
#define PCIBIOS_MIN_MEM		0x60000000

#define PCIO_BASE		0x9c700000

#define pcibios_assign_all_busses() 1

#endif
