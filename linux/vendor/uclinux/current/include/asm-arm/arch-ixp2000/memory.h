/*
 * linux/include/asm-arm/arch-ixdp2400/memory.h
 *
 * Copyright (c) 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#include <asm/hardware.h>

/*
 * Task size: 1GB
 */
#define TASK_SIZE	((unsigned long)(CONFIG_KERNEL_START & 0xffc00000))
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

#define PAGE_OFFSET	((unsigned long)(CONFIG_KERNEL_START & 0xffc00000))

/*
 * Physical DRAM offset.
 */
#define PHYS_OFFSET	(0x1c000000UL)

/*
 * physical vs virtual ram conversion
 */
#define __virt_to_phys__is_a_macro
#define __phys_to_virt__is_a_macro


#define __virt_to_phys(x)	((x) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt(x)	((x) - PHYS_OFFSET + PAGE_OFFSET)

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *		address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *		to an address that the kernel can use.
 */

// #define __virt_to_bus__is_a_macro
//#define __virt_to_bus(v)        

static inline unsigned int __virt_to_bus(unsigned v)
{
        return ((__virt_to_phys(v) - 0x0) + (*IXP2000_PCI_SDRAM_BAR & 0xfffffff0));
}

#define __bus_to_virt__is_a_macro
#define __bus_to_virt(b)        \
        __phys_to_virt((((b - (*IXP2000_PCI_SDRAM_BAR & 0xfffffff0)) + 0x0)))

#endif
