/*
 * linux/include/asm-arm/arch-iop310/io.h
 *
 *  Copyright (C) 2001  MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

#define __io_pci(a)		(PCIO_BASE + (a))
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		((unsigned long)(a))

#define __ioaddr(p)		__io_pci(p)
#define __io(p)			__io_pci(p)

/*
 * We have to define our own here b/c XScale has some sideaffects
 * dealing with data aborts that we have to workaround here.
 * Basically after each I/O access, we do a NOP to protect
 * the access from imprecise aborts.  See section 2.3.4.4 of the
 * XScale developer's manual for more info.
 *
 */

/*
#undef __arch_getb
#undef __arch_getl

extern __inline__ unsigned char __arch_getb(unsigned long a)
{
	unsigned int value;
	__asm__ __volatile__(
		"ldr%?b	%0, [%1, #0]	@ getw"
		"mov	r0, r0"
		: "=&r" (value) : "r" (a));
	return (unsigned char)value;
}

extern __inline__ unsigned int __arch_getl(unsigned long a)
{
	unsigned int value;
	__asm__ __volatile__(
		"ldr%?	%0, [%1, #0]	@ getw"
		"mov	r0, r0"
		: "=&r" (value) : "r" (a));
	return value;
}
*/

extern __inline__ unsigned int __arch_getw(unsigned long a)
{
	unsigned int value;
	__asm__ __volatile__(
		"ldr%?h	%0, [%1, #0]	@ getw"
		"mov	r0, r0"
		: "=&r" (value) : "r" (a));
	return value;
}


/*
#undef __arch_putb(v,a)	
#undef __arch_putl(v,a)	

extern __inline__ void __arch_putb(unsigned int value, unsigned long a)
{
	__asm__ __volatile__(
		"str%?b	%0, [%1, #0]	@ putw"
		"mov	r0, r0"
		: : "r" (value), "r" (a));
}

extern __inline__ void __arch_putl(unsigned int value, unsigned long a)
{
	__asm__ __volatile__(
		"str%?	%0, [%1, #0]	@ putw"
		"mov	r0, r0"
		: : "r" (value), "r" (a));
}
*/

extern __inline__ void __arch_putw(unsigned int value, unsigned long a)
{
	__asm__ __volatile__(
		"str%?h	%0, [%1, #0]	@ putw"
		"mov	r0, r0"
		: : "r" (value), "r" (a));
}

#define __arch_ioremap		__ioremap
#define __arch_iounmap		__iounmap

#endif
