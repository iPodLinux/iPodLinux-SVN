/*
 * linux/include/asm-arm/arch-ixdp2000/io.h
 *
 * Author: Naeem M Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright (C) 2002  Intel Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

#define __ixp_io_pci(a)		(PCIO_BASE + (a))
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		((unsigned long)(a))

static inline unsigned __ixp_io(unsigned long p)
{
	unsigned long ext = *IXP2000_PCI_ADDR_EXT & 0xffff;

	ext |= p & 0xffff0000;

	*IXP2000_PCI_ADDR_EXT = ext;

	return __ixp_io_pci(p);
}

/*
 * Generic virtual read/write
 */

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

/*
 * IXP2400 has a broken IO unit that does not do bytelane swapping.
 * For this reason, we need to override the default IO functions 
 * so that we just do raw access...
 */
#define __ixp_ioaddr(p) __ixp_io(p)

#define outb(v,p)			__raw_writeb(v,__ixp_io(p))
#define outw(v,p)			__raw_writew(v,__ixp_io(p))
#define outl(v,p)			__raw_writel(v,__ixp_io(p))

#define inb(p)		({ unsigned int __v = __raw_readb(__ixp_io(p)); __v; })
#define inw(p)		\
	({ unsigned int __v = (__raw_readw(__ixp_io(p))); __v; })
#define inl(p)		\
	({ unsigned int __v = (__raw_readl(__ixp_io(p))); __v; })

#define outsb(p,d,l)			__raw_writesb(__ixp_io(p),d,l)
#define outsw(p,d,l)			__raw_writesw(__ixp_io(p),d,l)
#define outsl(p,d,l)			__raw_writesl(__ixp_io(p),d,l)

#define insb(p,d,l)			__raw_readsb(__ixp_io(p),d,l)
#define insw(p,d,l)			__raw_readsw(__ixp_io(p),d,l)
#define insl(p,d,l)			__raw_readsl(__ixp_io(p),d,l)

#define __arch_ioremap	__ioremap
#define __arch_iounmap	__iounmap

#endif
