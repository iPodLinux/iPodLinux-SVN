/*
 *  linux/arch/e1nommu/mm/memory.c
 *
 *  Derived from <linux/arch/m68knommu/mm/memory.c>
 *  Copyright (C) 2002-2003 GDT, Yannis Mitsos <yannis.mitsos@gdt.gr>
 *                           	 George Thanos <george.thanos@gdt.gr>
 *  Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>,
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
/* #include <asm/traps.h>    */
/* #include <asm/shglcore.h> */
#include <asm/virtconvert.h>

#ifndef NO_MM

#error "!NO_MM NOT PORTED"
/* check m68knommu/mm/memory.c */

#else /* !NO_MM */

/*
 * 040: Hit every page containing an address in the range paddr..paddr+len-1.
 * (Low order bits of the ea of a CINVP/CPUSHP are "don't care"s).
 * Hit every page until there is a page or less to go. Hit the next page,
 * and the one after that if the range hits it.
 */
/* ++roman: A little bit more care is required here: The CINVP instruction
 * invalidates cache entries WITHOUT WRITING DIRTY DATA BACK! So the beginning
 * and the end of the region must be treated differently if they are not
 * exactly at the beginning or end of a page boundary. Else, maybe too much
 * data becomes invalidated and thus lost forever. CPUSHP does what we need:
 * it invalidates the page after pushing dirty data to memory. (Thanks to Jes
 * for discovering the problem!)
 */
/* ... but on the '060, CPUSH doesn't invalidate (for us, since we have set
 * the DPI bit in the CACR; would it cause problems with temporarily changing
 * this?). So we have to push first and then additionally to invalidate.
 */

/*
 * cache_clear() semantics: Clear any cache entries for the area in question,
 * without writing back dirty entries first. This is useful if the data will
 * be overwritten anyway, e.g. by DMA to memory. The range is defined by a
 * _physical_ address.
 */

void cache_clear (unsigned long paddr, int len)
{
}

/*
 *	Define cache invalidate functions. The ColdFire 5407 is really
 *	the only processor that needs to do some work here. Anything
 *	that has separate data and instruction caches will be a problem.
 */
#ifdef CONFIG_E132XS

static __inline__ void cache_invalidate_lines(unsigned long paddr, int len)
{
		printk("Inside cache_invalidate_lines \n");
		idle();
		/* FIXME
	unsigned long	sset, eset;

	sset = (paddr & 0x00000ff0);
	eset = ((paddr + len) & 0x0000ff0) + 0x10;

	__asm__ __volatile__ (
		"nop\n\t"
		"clrl	%%d0\n\t"
		"1:\n\t"
		"movel	%0,%%a0\n\t"
		"addl	%%d0,%%a0\n\t"
		"2:\n\t"
		".word	0xf4e8\n\t"
		"addl	#0x10,%%a0\n\t"
		"cmpl	%1,%%a0\n\t"
		"blt	2b\n\t"
		"addql	#1,%%d0\n\t"
		"cmpil	#4,%%d0\n\t"
		"bne	1b"
		: : "a" (sset), "a" (eset) : "d0", "a0" );
		*/
}

#else
#define	cache_invalidate_lines(a,b)
#endif /*  CONFIG_E132XS */


/*
 * cache_push() semantics: Write back any dirty cache data in the given area,
 * and invalidate the range in the instruction cache. It needs not (but may)
 * invalidate those entries also in the data cache. The range is defined by a
 * _physical_ address.
 */

void cache_push (unsigned long paddr, int len)
{
	cache_invalidate_lines(paddr, len);
}


/*
 * cache_push_v() semantics: Write back any dirty cache data in the given
 * area, and invalidate those entries at least in the instruction cache. This
 * is intended to be used after data has been written that can be executed as
 * code later. The range is defined by a _user_mode_ _virtual_ address  (or,
 * more exactly, the space is defined by the %sfc/%dfc register.)
 */

void cache_push_v (unsigned long vaddr, int len)
{
	cache_invalidate_lines(vaddr, len);
}

/* Map some physical address range into the kernel address space. The
 * code is copied and adapted from map_chunk().
 */

unsigned long kernel_map(unsigned long paddr, unsigned long size,
			 int nocacheflag, unsigned long *memavailp )
{
	return paddr;
}


#ifdef MAGIC_ROM_PTR


int is_in_rom(unsigned long addr)
{

extern int _ramstart, _ramend;

#ifdef CONFIG_E132XS
	if ((addr < _ramstart) && (addr >= _ramend))
		return(1);
#else
#error "check is_in_rom()"
#endif


	return(0); /* default case, not in ROM */
}

#endif /* MAGIC_ROM_PTR */

#endif /* NO_MM */
