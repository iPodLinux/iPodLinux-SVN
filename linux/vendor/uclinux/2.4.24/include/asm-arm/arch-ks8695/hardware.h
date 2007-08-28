/*
 *  linux/include/asm-arm/arch-ks8695/hardware.h
 *
 *  This file contains the hardware definitions of the KS8695.
 *
 *  Copyright (C) 2002 Micrel Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <asm/sizes.h>
#include <asm/arch/platform.h>

/*
 * Where in virtual memory the IO devices (timers, system controllers
 * and so on)
 */
#define IO_BASE			0xF0000000                 // VA of IO 
#define IO_SIZE			KS8695_IO_SIZE             // 0x0000 - 0xFFFF
#define IO_START		KS8695_IO_BASE            // PA of IO

/*
 * No PCI with KS8695, here to avoid compiler complain
 */
#define PCI_MEMORY_VADDR        0xe8000000
#define PCI_IO_VADDR            0xee000000

/* macro to get at IO space when running virtually */
#define IO_ADDRESS(x) (((x) >> 4) + IO_BASE) 

/*
 * Use SDRAM for memory
 */
#define MEM_SIZE		KS8695_MEM_SIZE

#endif

