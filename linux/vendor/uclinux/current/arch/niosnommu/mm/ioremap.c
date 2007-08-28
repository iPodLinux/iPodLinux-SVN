/*  linux/arch/niosnommu/mm/ioremap.c, based on:
 *
 *  linux/arch/m68knommu/mm/kmap.c
 *
 *  Copyright (C) 2000 Lineo, <davidm@lineo.com>
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/malloc.h>
#include <linux/vmalloc.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/io.h>
#include <asm/system.h>

/*
 * Map some physical address range into the kernel address space.
 */

void *__ioremap(unsigned long physaddr, unsigned long size, int cacheflag)
{
	return (void *)physaddr;
}

/*
 * Unmap a ioremap()ed region again
 */
void iounmap(void *addr)
{
}

/*
 * __iounmap unmaps nearly everything, so be careful
 * it doesn't free currently pointer/page tables anymore but it
 * wans't used anyway and might be added later.
 */
void __iounmap(void *addr, unsigned long size)
{
}

