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

#define __io_pci(a)		(PCIO_BASE + (a))
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		((unsigned long)(a))

/*
 * We don't need this for now. If we get to the point where
 * we need to have > 64K I/O access, we need to fix this so
 * that it saves/restores the previous I/O window setting.
 */
#if 0
static inline unsigned __ixp_io(unsigned long p)
{
	unsigned long ext = *IXP2000_PCI_ADDR_EXT & 0xffff;

	ext |= p & 0xffff0000;

	*IXP2000_PCI_ADDR_EXT = ext;

	return __io_pci(p);
}
#endif

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
 * IXP2000 currently does not do bytelane or data swapping on
 * PCI I/O cycles, so we need to override the default functions.
 *
 * Will retest once we have "working" silicon.
 */
#define alignb(addr) ((addr & ~3) + (3 - (addr & 3)))
#define alignw(addr) ((addr & ~2) + (2 - (addr & 2)))

#define outb(v,p)			__raw_writeb(v,alignb(__io_pci(p)))
#define outw(v,p)			__raw_writew((v),alignw(__io_pci(p)))
#define outl(v,p)			__raw_writel((v),__io_pci(p))

#define inb(p)		({ unsigned int __v = __raw_readb(alignb(__io_pci(p))); __v; })
#define inw(p)		\
	({ unsigned int __v = (__raw_readw(alignw(__io_pci(p)))); __v; })
#define inl(p)		\
	({ unsigned int __v = (__raw_readl(__io_pci(p))); __v; })

#define outsb(p,d,l)			__raw_writesb(alignb(__io_pci(p)),d,l)
#define outsw(p,d,l)			__raw_writesw(alignw(__io_pci(p)),d,l)
#define outsl(p,d,l)			__raw_writesl(__io_pci(p),d,l)

#define insb(p,d,l)			__raw_readsb(alignb(__io_pci(p)),d,l)
#define insw(p,d,l)			__raw_readsw(alignw(__io_pci(p)),d,l)
#define insl(p,d,l)			__raw_readsl(__io_pci(p),d,l)

#define __arch_ioremap	__ioremap
#define __arch_iounmap	__iounmap

#endif
