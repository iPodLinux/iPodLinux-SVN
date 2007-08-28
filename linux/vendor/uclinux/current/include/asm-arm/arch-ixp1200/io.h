/*
 * linux/include/asm-arm/arch-ixp1200/io.h
 *
 * Copyright (C) 2001 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff
// #define IO_SPACE_LIMIT 0x5400ffff

/*
 * Translation of various region addresses to virtual addresses
 */
#define __io_pci(a)		(PCIO_BASE + (a))
#define __mem_pci(a)		((unsigned long)(a))

#define __ioaddr(p)		__io_pci(p)
#define __io(p)			__io_pci(p)

extern __inline__ unsigned int __arch_getw(unsigned long a)
{
	unsigned int value;
	__asm__ __volatile__("ldr%?h	%0, [%1, #0]	@ getw"
		: "=&r" (value)
		: "r" (a));
	return value;
}

extern __inline__ void __arch_putw(unsigned int value, unsigned long a)
{
	__asm__ __volatile__("str%?h	%0, [%1, #0]	@ putw"
		: : "r" (value), "r" (a));
}

#define __arch_ioremap          __ioremap
#define __arch_iounmap          __iounmap

#endif
