/*
 *  linux/arch/arm/mm/consistent.c
 *
 *  Copyright (C) 2000-2002 Russell King
 *  Modified by Hyok S. Choi, 2004, Samsung Electrocs Co.,Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  DMA uncached mapping support.
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

/*
 * Since we have the DMA mask available to us here, we could try to do
 * a normal allocation, and only fall back to a "DMA" allocation if the
 * resulting bus address does not satisfy the dma_mask requirements.
 */
void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle, int gfp){
	return consistent_alloc(gfp, size, handle, 0);
}

EXPORT_SYMBOL(dma_alloc_coherent);

/*
 * This allocates one page of cache-coherent memory space and returns
 * both the virtual and a "dma" address to that space.  It is not clear
 * whether this could be called from an interrupt context or not.  For
 * now, we expressly forbid it, especially as some of the stuff we do
 * here is not interrupt context safe.
 *
 * Note that this does *not* zero the allocated area!
 */
void *consistent_alloc(int gfp, size_t size, dma_addr_t *dma_handle, unsigned long flags)
{
	struct page *page, *end, *free;
	unsigned long order;
	void *ret, *virt;

	if (in_interrupt())
		BUG();

	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = alloc_pages(gfp, order);
	if (!page)
		goto no_page;

	/*
	 * We could do with a page_to_phys and page_to_bus here.
	 */
	virt = page_address(page);
	*dma_handle = virt_to_bus(virt);
	ret = __ioremap(virt_to_phys(virt), size, 0, 0);
	if (!ret)
		goto no_remap;

#if 0 /* ioremap_does_flush_cache_all */
	/*
	 * we need to ensure that there are no cachelines in use, or
	 * worse dirty in this area.  Really, we don't need to do
	 * this since __ioremap does a flush_cache_all() anyway. --rmk
	 */
	invalidate_dcache_range(virt, virt + size);
#endif

	/*
	 * free wasted pages.  We skip the first page since we know
	 * that it will have count = 1 and won't require freeing.
	 * We also mark the pages in use as reserved so that
	 * remap_page_range works.
	 */
	page = virt_to_page(virt);
	free = page + (size >> PAGE_SHIFT);
	end  = page + (1 << order);

	for (; page < end; page++) {
		set_page_count(page, 1);
		if (page >= free)
			__free_page(page);
		else
			SetPageReserved(page);
	}
	return ret;

no_remap:
	__free_pages(page, order);
no_page:
	return NULL;
}

/*
 * free a page as defined by the above mapping.  We expressly forbid
 * calling this from interrupt context.
 */
void consistent_free(void *vaddr, size_t size, dma_addr_t handle)
{
	struct page *page, *end;
	void *virt;

	if (in_interrupt())
		BUG();

	virt = bus_to_virt(handle);

	/*
	 * More messing around with the MM internals.  This is
	 * sick, but then so is remap_page_range().
	 */
	size = PAGE_ALIGN(size);
	page = virt_to_page(virt);
	end = page + (size >> PAGE_SHIFT);

	for (; page < end; page++)
		ClearPageReserved(page);

	__iounmap(vaddr);
}

/*
 * Make an area consistent for devices.
 */
void consistent_sync(void *vaddr, size_t size, int direction)
{
	unsigned long start = (unsigned long)vaddr;
	unsigned long end   = start + size;

	switch (direction) {
	case DMA_FROM_DEVICE:		/* invalidate only */
		dmac_inv_range(start, end);
		break;
	case DMA_TO_DEVICE:		/* writeback only */
		dmac_clean_range(start, end);
		break;
	case DMA_BIDIRECTIONAL:		/* writeback and invalidate */
		dmac_flush_range(start, end);
		break;
	default:
		BUG();
	}
}

