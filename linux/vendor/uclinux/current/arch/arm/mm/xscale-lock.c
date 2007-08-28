/*
 * xscale-lock.c
 *
 * This file implements the APIs that can be used by kernel developers 
 * to lock entries into the caches and TLBs on processors based on 
 * Intel's XScale * Microarchitecture.  For information on using the APIs see 
 * Documentation/arm/XScale/cache-lock.txt and tlb_lock.txt.
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/hardware.h>
#include <asm/cpu-single.h>
#include <asm/xscale-lock.h>
#include <asm/proc/cache.h>


extern void* consistent_alloc(int, size_t, dma_addr_t);

#undef DEBUG 
#ifdef DEBUG
#define DBG_LEVEL 3
#define DBG(n, args...) do { if (DBG_LEVEL>=(n)) printk(args); } while (0)
#else
#define DBG(n, args...) do { } while (0)
#endif

/*
 * Future XScale cores may have different cache sizes
 */
#define CACHE_SIZE 32
#define CACHE_LOCK 28
#define CACHE_LINES CACHE_LOCK * 1024 / L1_CACHE_BYTES

#define TLB_ENTRIES 28


/*
 * Cache Locking Implementation
 */

static dma_addr_t dma_handle;
static void *low_level_page = NULL;

/*
 * Internal structure used to track cache usage
 */
struct cache_entry
{
	void *addr;
	int lines;
	struct cache_entry *next;
	struct cache_entry *prev;
};

static struct cache_capabilities cache =
{
	CACHE_LOCKABLE,
	CACHE_SIZE,
	CACHE_LOCK
};

static int cache_lock_init(void);

static int cache_lines[2] = { CACHE_LINES, CACHE_LINES };
static struct cache_entry *cache_data[2] = {NULL, NULL};

static int alloc_cache_entry(void *, int, u8);
static struct cache_entry* unlink_cache_entry(void *, u8);

extern int xscale_icache_lock(void*, int);
extern int xscale_dcache_lock(void*, int);
extern int xscale_icache_unlock(void);
extern int xscale_dcache_unlock(void);
extern void xscale_cache_dummy(void);

typedef int (*cache_lock_fn)(void *, int);
typedef int (*cache_unlock_fn)(void);

static cache_lock_fn icache_lock_fn;
static cache_lock_fn dcache_lock_fn;
static cache_unlock_fn icache_unlock_fn;
static cache_unlock_fn dcache_unlock_fn;

static cache_lock_fn lock_fns[2];
static cache_unlock_fn unlock_fns[2];

/*
 * Cache Lock API functions
 */

int cache_query_capabilities(u8 cache_type, struct cache_capabilities *pcap)
{
	if(!pcap)
		return -EINVAL;

	/*
	 * Both caches are same on current xscale 
	 */
	memcpy(pcap, &cache, sizeof(struct cache_capabilities));

	return 0;
}

int cache_lock(void *addr, u32 len, u8 cache_type, const char *desc)
{
	u32 address = (u32)addr;

	DBG(1, "cache_lock entered\n");
	DBG(2, "   Addr: %#10x\n", (u32)addr);
	DBG(2, "   Type: %s\n", cache_type == ICACHE ? "ICACHE" : "DCACHE");
	DBG(2, "    Len: %#6x\n", len);


	if(address & (L1_CACHE_BYTES - 1)) {
		DBG(1, "   Non-aligned lock request: %p\n", addr);
		return -EINVAL;	
	}

	DBG(1, "   calling alloc_cache_entry\n");
	return alloc_cache_entry(addr, len, cache_type);
}

int cache_unlock(void *addr)
{
	struct cache_entry *entry;
	u8 cache_type;

	DBG(1, "cache_unlock entered\n");

	if((entry = unlink_cache_entry(addr, ICACHE)))
		cache_type = ICACHE;
	else if((entry = unlink_cache_entry(addr, DCACHE)))
		cache_type = DCACHE;
	else 
	{
		DBG(1, "   No entry!\n");
		return -ENOENT;
	}

	DBG(2, "   Addr: %#10x\n", (u32)addr);
	DBG(2, "   Type: %s\n", cache_type == ICACHE ? "ICACHE" : "DCACHE");
	DBG(2, "    Len: %#6x\n", entry->lines);

	cache_lines[cache_type] += entry->lines;
	kfree(entry);

	/*
	 * Unlock
	 * Invalidate
	 * Relock
	 */
	DBG(2, "   Calling unlock_fn\n");
	unlock_fns[cache_type]();
	DBG(2, "   unlock_fn returned\n");
	cpu_cache_clean_invalidate_all();


	for(entry = cache_data[cache_type]; entry; entry = entry->next)
	{
		DBG(2, "   Invalidating DCACHE\n");
		cpu_cache_clean_invalidate_all();
		DBG(2, "   Cleaning DCACHE\n");

		DBG(2, "   Calling lock_fn\n");
		lock_fns[cache_type](entry->addr, 
			(u32)(entry->addr + entry->lines * L1_CACHE_BYTES));
		DBG(2, "   lock_fn returned\n");
	}

	DBG(1, "   cache_unlock exit\n");
	return 0;
}

/*
 * Cache Lock internals
 */

static struct cache_entry* unlink_cache_entry(void *addr, u8 cache_type)
{
	struct cache_entry *p;

	p = cache_data[cache_type];

	if(!p) return NULL;

	if(p->addr == addr)
	{
		cache_data[cache_type] = p->next;
		if(p->next)
			p->next->prev = NULL;
		return p;
	}

	for(p = cache_data[cache_type]; p; p = p->next)
	{
		if(p->addr == addr)
		{
			if(p->prev)
				p->prev->next = p->next;
			if(p->next)
				p->next->prev = p->prev;
			break;
		}
	}

	return p;
}


static int alloc_cache_entry(void *addr, int len, u8 cache_type)
{
	struct cache_entry *new = NULL; 
	u32 end = L1_CACHE_ALIGN((u32)addr + len);
	int lines = 0;

	DBG(2, "   addr: %#10x\n", (u32)addr);
	DBG(2, "    len: %#6x\n", len);
	DBG(2, "   type: %s\n", cache_type == ICACHE ? "ICACHE" : "DCACHE");

	new = kmalloc(sizeof(struct cache_entry), GFP_ATOMIC);
	DBG(1, "alloc_cache_entry entered\n");
	if(!new)
	{
		DBG(2, "   Could not allocate cache_entry\n");
		return -ENOMEM;
	}

	lines = ((u32)end - (u32)addr) / L1_CACHE_BYTES;
	if(lines > cache_lines[cache_type]) 
	{
		DBG(2, "   No space left in cache\n");
		return -ENOSPC;	
	}

	new->addr = addr;
	new->lines = lines;
	new->next = NULL;
	new->prev = NULL;

	if(cache_data[cache_type] == NULL) 
	{
		cache_data[cache_type] = new;
	}
	else
	{
		new->next = cache_data[cache_type];
		cache_data[cache_type]->prev = new;
		new->prev = NULL;
		cache_data[cache_type] = new;
	}

	cache_lines[cache_type] -= lines;

	DBG(2, "   Invalidating/cleaning caches\n");
	cpu_cache_clean_invalidate_all();
	DBG(2, "   Invalidate/clean complete\n");

	DBG(2, "   Calling lock_fn @ %#10x\n", (u32)lock_fns[cache_type]);
	DBG(3, "      start: %#10x end: %#10x\n", (u32)addr, (u32)end);
	lock_fns[cache_type](addr, end);
	DBG(2, "   lock_fn returned\n");
	DBG(1, "alloc_cache_entry exit\n");

	return 0;
}


/*
 * This function basically sets up the cache locking code so
 * it meets the requirements in section 4.3.4 of the 80200
 * manual.
 *
 * 1) The routine used to lock cache can't be cacheble
 * 2) Code used to do locking not reside closer than 128 bytes
 *    to a cache/non-cache boundary.
 *
 * Since the compiler has no understanding of such things, we 
 * allocate uncacheble memory by calling consistent_alloc and 
 * copy the relavent low level code to the new memory and than
 * create pointers into this memory. Ick.
 *
 */
static int __init cache_lock_init(void)
{
	u32 len;

	DBG(1, "XScale cache_lock_init called\n");
	len = (u32)xscale_cache_dummy - (u32)xscale_icache_lock + 256;

	/*
	 * This is an ugly way to get a non-cached page
	 */
	DBG(1, "   Calling consistent alloc\n");
	low_level_page = consistent_alloc(GFP_KERNEL, len, (u32)&dma_handle);
	DBG(1, "   low_level_page initialized\n");

	if(!low_level_page)
	{
		printk(KERN_ERR 
			"   Could not allocate memory for cache locking\n");
		printk(KERN_ERR "   Cache locking disabled\n");

		return -1;
	}

	memcpy(low_level_page + 128, xscale_icache_lock, len - 256);

	DBG(1, "   low_level_page @ %#10x\n", (u32)low_level_page);
	icache_lock_fn = (cache_lock_fn)(low_level_page + 128);
	dcache_lock_fn = 
		(cache_lock_fn)(low_level_page + 128 + 
				((u32)xscale_dcache_lock - (u32)xscale_icache_lock));

	DBG(1, "      icache_lock_fn @ %#10x\n", (u32)icache_lock_fn);
	DBG(1, "      dcache_lock_fn @ %#10x\n", (u32)dcache_lock_fn);
	lock_fns[ICACHE] = icache_lock_fn;
	lock_fns[DCACHE] = dcache_lock_fn;

	icache_unlock_fn = (cache_unlock_fn)(low_level_page + 128 +
				((u32)xscale_icache_unlock - (u32)xscale_icache_lock));
	dcache_unlock_fn = (cache_unlock_fn)(low_level_page + 128 +
				((u32)xscale_dcache_unlock - (u32)xscale_icache_lock));

	DBG(1, "      icache_unlock_fn @ %#10x\n", (u32)icache_unlock_fn);
	DBG(1, "      dcache_unlock_fn @ %#10x\n", (u32)dcache_unlock_fn);
	unlock_fns[ICACHE] = icache_unlock_fn;
	unlock_fns[DCACHE] = dcache_unlock_fn;

	return 0;
}


/*
 * TLB Locking Implementation
 */

static u32 tlb_entries[TLB_ENTRIES][TLB_ENTRIES];
static u32 tlb_count[2] = {TLB_ENTRIES, TLB_ENTRIES};

extern void xscale_itlb_lock(u32);
extern void xscale_dtlb_lock(u32);

extern void xscale_itlb_unlock(void);
extern void xscale_dtlb_unlock(void);

typedef void (*tlb_lock)(u32);
typedef void (*tlb_unlock)(void);

static tlb_lock tlb_lock_fn[2]  = 
	{xscale_itlb_lock, xscale_dtlb_lock};

static tlb_unlock tlb_unlock_fn[2] = 
	{xscale_itlb_unlock, xscale_dtlb_unlock};

int xscale_tlb_lock(u8 tlb_type, u32 addr)
{
	int i = 0;

	if(tlb_type != ITLB && tlb_type != DTLB)
		return -EIO;

	if(!tlb_count[tlb_type])
		return -ENOSPC;

	if(addr & 0xff)
		return -EINVAL;

	tlb_lock_fn[tlb_type](addr);

	for(i = 0; i < TLB_ENTRIES; i++)
	{
		if(tlb_entries[tlb_type][i] == 0xDEADBEEF)
		{
			tlb_entries[tlb_type][i] = addr;	
			break;
		}
	}

	tlb_count[tlb_type]--;
					
	return 0;
}

int xscale_tlb_unlock(u8 tlb_type, u32 addr)
{
	int i = 0;

	if(tlb_type != ITLB && tlb_type != DTLB)
		return -EIO;

	for(i = 0; i < TLB_ENTRIES; i++)
	{
		if(tlb_entries[tlb_type][i] == addr)
		{
			tlb_entries[tlb_type][i] = 0xDEADBEEF;
			break;
		}
	}

	if(i == TLB_ENTRIES)
		return -ENOENT;

	/*
	 * There's no unlock 1 entry command on XScale.  So we unlock
	 * the whole tlb and than lock each entry in again.
	 */
	tlb_unlock_fn[tlb_type]();

	for(i = 0; i < TLB_ENTRIES; i++)
	{
		if(tlb_entries[tlb_type][i] != 0xDEADBEEF)
			tlb_lock_fn[tlb_type](addr);
	}

	return 0;
}


static void __init tlb_lock_init(void)
{
	int i = 0;

	DBG(1, "Initializing TLB locking\n");
	for(i = 0; i < TLB_ENTRIES; i++)
		tlb_entries[ITLB][i] = tlb_entries[DTLB][i] = 0xDEADBEEF;
	DBG(1, "TLB locking initialized\n");
}	


/*
 * Initialization code
 */
void __init xscale_locking_init(void)
{
	printk(KERN_INFO "XScale Cache/TLB Locking Copyright(c) 2001 MontaVista Software, Inc.\n");

	if(cache_lock_init())
		return;

	tlb_lock_init();
}
