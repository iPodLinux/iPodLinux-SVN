
#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/hardware.h>
#include <asm/cpu-single.h>
#include <asm/xscale-lock.h>
#include <asm/proc/cache.h>
#include <asm/io.h>

static __init u32 data_A = 0xDEADBEEF;
static __init u32 data_B = 0xFACEFACE;

static int __init cache_test(void)
{
	u32 *uncached_addr = 0;
	u32 *real_uncached = 0;
	dma_addr_t dma_handle;	
	u32 *cached_alias = 0;
	int i = 0;
	u8 cached[128], cached1[128], uncached[128], uncached1[128] = {0};
	u32 control;

	real_uncached = (u32*)consistent_alloc(GFP_KERNEL, 136, &dma_handle);
	cached_alias = (u32*)bus_to_virt(dma_handle);
	uncached_addr = real_uncached + 4;
	cached_alias += 4;

	printk("TESTING XSCALE CACHE COHERENCY\n");
	cli();
	asm ("mrc p15, 0, %0, c1, c0, 0" : "=r" (control));
	sti();
	printk("Control = %#010x\n", control);
	cli();
	asm ("mrc p15, 0, %0, c1, c0, 1" : "=r" (control));
	sti();
	printk("Control1 = %#010x\n", control);
	printk("uncached_addr @ %#010x\n", (u32)uncached_addr);
	printk("dma_handle @ %#010x\n", dma_handle);
	printk("cached alias & %#010x\n", (u32)cached_alias);
	printk("data_A = %#010x\n", data_A);
	printk("data_B = %#010x\n", data_B);

	for(i = 0; i < 128/sizeof(u32); i++)
		cached_alias[i] = uncached_addr[i] = 0;

	cli();

	cpu_cache_clean_invalidate_all();

	/*
	 * Force a cache preload since we read-allocate
	 */
	for(i = 0; i < 128/sizeof(u32); i++)
		cached[i] = (u8)cached_alias[i];


	for(i = 0; i < 128/sizeof(u32); i++)
	{
		uncached_addr[i] = data_A;
		cached_alias[i] = data_B;
	}

	for(i = 0; i < 128/sizeof(u32); i++)
	{
		if(cached_alias[i] == data_B)
			cached[i] = 0;
		else if(cached_alias[i] == data_A)
			cached[i] = 1;
		else
			panic("BAD");

		if(uncached_addr[i] == data_B)
			uncached[i] = 0;
		else if(uncached_addr[i] == data_A)
			uncached[i] = 1;
		else
			panic("BAD");
	}

	consistent_sync(cached_alias, 128, PCI_DMA_FROMDEVICE);

	for(i = 0; i < 128/sizeof(u32); i++)
	{
		if(cached_alias[i] == data_B)
			cached1[i] = 0;
		else if(cached_alias[i] == data_A)
			cached1[i] = 1;
		else
			panic("BAD");

		if(uncached_addr[i] == data_B)
			uncached1[i] = 0;
		else if(uncached_addr[i] == data_A)
			uncached1[i] = 1;
		else
			panic("BAD");
	}

	sti();

	printk("Ran cache invalidation test\n");
	printk("Pre-sync:\n");
	for(i = 0; i < 128/sizeof(u32); i++)
	{
		if(uncached[i])
			printk("   data_A in uncached_addr[%d]\n", i);
		else
			printk("   data_B in uncached_addr[%d]\n", i);
		if(cached[i])
			printk("   data_A in cached_alias[%d]\n", i);
		else
			printk("   data_B in cached_alias[%d]\n", i);
	}

	printk("Post-sync:\n");
	for(i = 0; i < 128/sizeof(u32); i++)
	{
		if(uncached1[i])
			printk("   data_A in uncached_addr[%d]\n", i);
		else
			printk("   data_B in uncached_addr[%d]\n", i);
		if(cached1[i])
			printk("   data_A in cached_alias[%d]\n", i);
		else
			printk("   data_B in cached_alias[%d]\n", i);
	}

	for(i = 0; i < 128/sizeof(u32); i++)
	{
		uncached_addr[i] = 0;
		cached_alias[i] = 0;
		cached[i] = 0;
		uncached[i] = 0;
		cached1[i] = 0;
		uncached1[i] = 0;
	}

	cli();

	cpu_cache_clean_invalidate_all();

	/*
	 * Force a cache preload since we read-allocate
	 */
	for(i = 0; i < 128/sizeof(u32); i++)
		cached[i] = (u8)cached_alias[i];

	for(i = 0; i < 128/sizeof(u32); i++)
	{
		cached_alias[i] = data_A;
		uncached_addr[i] = data_B;
	}

	for(i = 0; i < 128/sizeof(u32); i++)
	{
		if(cached_alias[i] == data_A)
			cached[i] = 1;
		else if(cached_alias[i] == data_B)
		  	cached[i] = 0;
		else
			panic("BAD");

		if(uncached_addr[i] == data_A)
			uncached[i] = 1;
		else if(uncached_addr[i] == data_B)
			uncached[i] = 0;
		else
			panic("BAD");
	}

	consistent_sync(cached_alias, 128, PCI_DMA_TODEVICE);

	for(i = 0; i < 128/sizeof(u32); i++)
	{
		if(cached_alias[i] == data_A)
			cached1[i] = 1;
		else if(cached_alias[i] == data_B)
		  	cached1[i] = 0;
		else
			panic("BAD");

		if(uncached_addr[i] == data_A)
			uncached1[i] = 1;
		else if(uncached_addr[i] == data_B)
			uncached1[i] = 0;
		else
			panic("BAD");
	}

	sti();

	printk("Ran Cache Writeback test\n");
	printk("Pre-sync:\n");
	for(i = 0; i < 128/sizeof(u32); i++)
	{
		if(cached[i])
			printk("   data_A in cached_alias[%d]\n", i);
		else
			printk("   data_B in cached_alias[%d]\n", i);

		if(uncached[i])
			printk("   data_A in uncached_addr[%d]\n", i);
		else
			printk("   data_B in uncached_addr[%d]\n", i);
	}
	printk("Post-sync:\n");
	for(i = 0; i < 128/sizeof(u32); i++)
	{
		if(cached1[i])
			printk("   data_A in cached_alias[%d]\n", i);
		else
			printk("   data_B in cached_alias[%d]\n", i);
		if(uncached1[i])
			printk("   data_A in uncached_addr[%d]\n", i);
		else
			printk("   data_B in uncached_addr[%d]\n", i);
	}

	consistent_free(real_uncached, 136, (u32)&dma_handle);

	return 0;
}

// __initcall(cache_test);
