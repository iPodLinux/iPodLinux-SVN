/*
 * linux/include/asm-arm/arch-ixp1200/memory.h
 *
 * Copyright (C) 1996-1999 Russell King.
 * Copyright (C) 2000 Intel Corporation, Inc.
 * Copyright (C) 2001-2002 MontaVista Software, Inc.
 *
 * 20-Mar-2000 Uday Naik 
 * 	Created for IXP1200
 * 26-Sep-2001 dsaxena 
 * 	Cleanup/port to 2.4.x kernel
 * 	virt_to_bus/bus_to_virt are now the same in host and agent mode
 * 	by reading the PCI Inbound XLation window.
 */

#ifndef __IXP1200_MEMORY_H_
#define __IXP1200_MEMORY_H_

#define TASK_SIZE	((unsigned long)(CONFIG_KERNEL_START & 0xffc00000))
#define TASK_SIZE_26	(0x04000000UL)

/*
 *  * This decides where the kernel will search for a free chunk of vm
 *   * space during mmap's.
 *    */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)


#define PAGE_OFFSET	((unsigned long)(CONFIG_KERNEL_START & 0xffc00000))


/*
 * Physical offset
 */
#define PHYS_OFFSET	(CONFIG_IXP1200_SDRAM_BASE)

#define __virt_to_phys__is_a_macro
#define __virt_to_phys(x) (((x - PAGE_OFFSET) + PHYS_OFFSET))
#define __phys_to_virt__is_a_macro
#define __phys_to_virt(x) (((x - PHYS_OFFSET) + PAGE_OFFSET))

/*
 * Bus addresses are based on the PCI inbound window which
 * is configured either by the external host or by us 
 * if we're running as the system controller
 */
#define __virt_to_bus__is_a_macro
#define __virt_to_bus(v)	\
	((__virt_to_phys(v) - 0xc0000000) + (*CSR_PCISDRAMBASE & 0xfffffff0))
#define __bus_to_virt__is_a_macro
#define __bus_to_virt(b)	\
	__phys_to_virt((((b - (*CSR_PCISDRAMBASE & 0xfffffff0)) + 0xc0000000)))

#endif

