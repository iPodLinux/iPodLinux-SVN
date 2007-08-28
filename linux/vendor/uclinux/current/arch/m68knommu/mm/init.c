/*
 *  linux/arch/m68knommu/mm/init.c
 *
 *  Copyright (C) 1998  D. Jeff Dionne <jeff@uClinux.org>,
 *                      Kenneth Albanowski <kjahds@kjahds.com>,
 *  Copyright (C) 2000  Lineo, Inc.  (www.lineo.com) 
 *
 *  Based on:
 *
 *  linux/arch/m68k/mm/init.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 *
 *  JAN/1999 -- hacked to support ColdFire (gerg@snapgear.com)
 *  DEC/2000 -- linux 2.4 support <davidm@lineo.com>
 */

#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#ifdef CONFIG_BLK_DEV_RAM
#include <linux/blk.h>
#endif
#include <linux/bootmem.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/machdep.h>
#include <asm/shglcore.h>
#include <asm/virtconvert.h>

#undef DEBUG

static unsigned long totalram_pages = 0;

extern void die_if_kernel(char *,struct pt_regs *,long);

extern void free_initmem(void);

/*
 * BAD_PAGE is the page that is used for page faults when linux
 * is out-of-memory. Older versions of linux just did a
 * do_exit(), but using this instead means there is less risk
 * for a process dying in kernel mode, possibly leaving a inode
 * unused etc..
 *
 * BAD_PAGETABLE is the accompanying page-table: it is initialized
 * to point to BAD_PAGE entries.
 *
 * ZERO_PAGE is a special page that is used for zero-initialized
 * data and COW.
 */
static unsigned long empty_bad_page_table;

static unsigned long empty_bad_page;

unsigned long empty_zero_page;

extern unsigned long rom_length;

void show_mem(void)
{
    unsigned long i;
    int free = 0, total = 0, reserved = 0, shared = 0;

    printk("\nMem-info:\n");
    show_free_areas();
    i = max_mapnr;
    while (i-- > 0) {
	total++;
	if (PageReserved(mem_map+i))
	    reserved++;
	else if (!page_count(mem_map+i))
	    free++;
	else
	    shared += page_count(mem_map+i) - 1;
    }
    printk("%d pages of RAM\n",total);
    printk("%d free pages\n",free);
    printk("%d reserved pages\n",reserved);
    printk("%d pages shared\n",shared);
    show_buffers();
}

extern unsigned long memory_start;
extern unsigned long memory_end;

/*
 * paging_init() continues the virtual memory environment setup which
 * was begun by the code in arch/head.S.
 * The parameters are pointers to where to stick the starting and ending
 * addresses  of available kernel virtual memory.
 */
void paging_init(void)
{
	/*
	 * make sure start_mem is page aligned,  otherwise bootmem and
	 * page_alloc get different views og the world
	 */
#ifdef DEBUG
	unsigned long start_mem = PAGE_ALIGN(memory_start);
#endif
	unsigned long end_mem   = memory_end & PAGE_MASK;

#ifdef DEBUG
	printk ("start_mem is %#lx\nvirtual_end is %#lx\n",
		start_mem, end_mem);
#endif

	/*
	 * initialize the bad page table and bad page to point
	 * to a couple of allocated pages
	 */
	empty_bad_page_table = (unsigned long)alloc_bootmem_pages(PAGE_SIZE);
	empty_bad_page = (unsigned long)alloc_bootmem_pages(PAGE_SIZE);
	empty_zero_page = (unsigned long)alloc_bootmem_pages(PAGE_SIZE);
	memset((void *)empty_zero_page, 0, PAGE_SIZE);

	/*
	 * Set up SFC/DFC registers (user data space)
	 */
	set_fs (USER_DS);

#ifdef DEBUG
	printk ("before free_area_init\n");

	printk ("free_area_init -> start_mem is %#lx\nvirtual_end is %#lx\n",
		start_mem, end_mem);
#endif

	{
		unsigned long zones_size[MAX_NR_ZONES] = {0, 0, 0};

		zones_size[ZONE_DMA]     = 0 >> PAGE_SHIFT;
		zones_size[ZONE_NORMAL]  = (end_mem - PAGE_OFFSET) >> PAGE_SHIFT;
#ifdef CONFIG_HIGHMEM
		zones_size[ZONE_HIGHMEM] = 0;
#endif
		free_area_init_node(0, NULL, NULL, zones_size, PAGE_OFFSET, NULL);
	}
}

void mem_init(void)
{
	int codek = 0, datak = 0, initk = 0;
	/* DAVIDM look at setup memory map generically with reserved area */
	unsigned long tmp;
#ifdef CONFIG_UCLINUX
	extern char _etext, _stext, _sdata, _ebss, __init_begin, __init_end;
	extern unsigned int _ramend, _rambase;
	unsigned long len = _ramend - _rambase;
#else
	extern char _etext, _romvec, __data_start;
	unsigned long len = end_mem-(unsigned long)&__data_start;
	int datapages = 0;
#endif
	unsigned long start_mem = memory_start; /* DAVIDM - these must start at end of kernel */
	unsigned long end_mem   = memory_end; /* DAVIDM - this must not include kernel stack at top */

	/* Bloody watchdog... */
#ifdef CONFIG_SHGLCORE
       (*((volatile unsigned char*)0xFFFA21)) = 128 | 64/* | 32 | 16*/;
       (*((volatile unsigned short*)0xFFFA24)) &= ~512;
       (*((volatile unsigned char*)0xFFFA27)) = 0x55;
       (*((volatile unsigned char*)0xFFFA27)) = 0xAA;
       
       /*printk("Initiated watchdog, SYPCR = %x\n", *(volatile char*)0xFFFA21);*/
#endif	                

#ifdef DEBUG
	printk("Mem_init: start=%lx, end=%lx\n", start_mem, end_mem);
#endif

	end_mem &= PAGE_MASK;
	high_memory = (void *) end_mem;

	start_mem = PAGE_ALIGN(start_mem);
	max_mapnr = num_physpages = MAP_NR(high_memory);

	/* this will put all memory onto the freelists */
	totalram_pages = free_all_bootmem();

#ifdef CONFIG_UCLINUX
	codek = (&_etext - &_stext) >> 10;
	datak = (&_ebss - &_sdata) >> 10;
	initk = (&__init_begin - &__init_end) >> 10;
#else
	for (tmp = PAGE_OFFSET ; tmp < end_mem ; tmp += PAGE_SIZE) {
		if (PageReserved(mem_map+MAP_NR(tmp))) {
			datapages++;
			continue;
		}
	}
	
	codek = (&_etext - &_romvec) >> 10;
	datak = datapages << (PAGE_SHIFT-10);
	initk = 0;
#endif

	tmp = nr_free_pages() << PAGE_SHIFT;
	printk("Memory available: %luk/%luk RAM, %luk/%luk ROM (%dk kernel code, %dk data)\n",
	       tmp >> 10,
	       len >> 10,
	       (rom_length > 0) ? ((rom_length >> 10) - codek) : 0,
	       rom_length >> 10,
	       codek,
	       datak
	       );
}


#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	int pages = 0;
	for (; start < end; start += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(start));
		set_page_count(virt_to_page(start), 1);
		free_page(start);
		totalram_pages++;
		pages++;
	}
	printk("Freeing initrd memory: %dk freed\n", (pages * PAGE_SIZE) >> 10);
}
#endif


void si_meminfo(struct sysinfo *val)
{
    unsigned long i;

    i = max_mapnr;
    val->totalram = totalram_pages;
    val->sharedram = 0;
    val->freeram = nr_free_pages();
    val->bufferram = atomic_read(&buffermem_pages);
    val->totalhigh = 0;
    val->freehigh = 0;
    val->mem_unit = PAGE_SIZE;
    return;
}


void
free_initmem()
{
#if defined(CONFIG_RAMKERNEL) && !defined(CONFIG_UCDIMM)
	unsigned long addr;
	extern char __init_begin, __init_end;
/*
 *	the following code should be cool even if these sections
 *	are not page aligned.
 */
	addr = PAGE_ALIGN((unsigned long)(&__init_begin));
	/* next to check that the page we free is not a partial page */
	for (; addr + PAGE_SIZE < (unsigned long)(&__init_end); addr +=PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		set_page_count(virt_to_page(addr), 1);
		free_page(addr);
		totalram_pages++;
	}
	printk("Freeing unused kernel memory: %ldk freed (0x%x - 0x%x)\n",
			(addr - PAGE_ALIGN((long) &__init_begin)) >> 10,
			PAGE_ALIGN((unsigned long)(&__init_begin)),
			addr - PAGE_SIZE);
#endif
}

