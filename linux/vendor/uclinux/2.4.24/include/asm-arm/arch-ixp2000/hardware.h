/*
 * linux/include/asm-arm/arch-ixdp2400/hardware.h
 *
 * Hardware definitions for IXP2000 based systems
 *
 * Author: Naeem M Afzal <naeem.m.afzal@intel.com>
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2001-2002 Intel Corp.
 * Copyright (C) 2003 MontaVista Software, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef __ASM_ARCH_HARDWARE_H__
#define __ASM_ARCH_HARDWARE_H__

/*
 * Generic chipset bits
 */
#include "ixp2000.h"

/* PCI IO info */
//#define PCIO_BASE    (VIRT_PCI_IO - PHY_PCI_IO)
#define PCIO_BASE      PCI_IO_VIRT_BASE

#define PCIBIOS_MIN_IO          0x00000000
#define PCIBIOS_MIN_MEM         0x00000000

#define pcibios_assign_all_busses() 0

/*
 * board specific definitions
 */
#include "ixdp2400.h"

#endif  /* _ASM_ARCH_HARDWARE_H__ */
