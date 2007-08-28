/*
 *  linux/arch/arm/mm/proc-xscale.S
 *
 *  Author:	Nicolas Pitre
 *  Created:	November 2000
 *  Copyright:	(C) 2000, 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MMU functions for the Intel XScale CPUs
 *
 * 2001 Aug 21:	
 *	some contributions by Brett Gaines <brett.w.gaines@intel.com>
 *	Copyright 2001 by Intel Corp.
 *
 * 2001 Sep 08:
 *	Completely revisited, many important fixes
 *	Nicolas Pitre <nico@cam.org>
 */

#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>
#include <asm/proc/pgtable.h>

/*
 * Some knobs for cache allocation policy.
 * Allocate on write may or may not be beneficial depending on the memory
 * usage pattern of your main application.  Write through cache is definitely
 * a performance loss in most cases, but might be used for special purposes.
 */
#define PMD_CACHE_WRITE_ALLOCATE 0
#define PTE_CACHE_WRITE_ALLOCATE 0
#define CACHE_WRITE_THROUGH 0

/*
 * Ther are errata that says that dirty status bits in the cache may get
 * corrupted.  The workaround significantly affects performances, and the bug
 * _might_ just not be that visible or critical to you, so it is configurable.
 * Let's hope a future core revision will tell us this was only a bad dream.
 * But in the mean time the risk and tradeoff is yours to decide....
 */
#ifdef CONFIG_XSCALE_CACHE_ERRATA
#undef CACHE_WRITE_THROUGH
#define CACHE_WRITE_THROUGH 1
#endif

/* 
 * This is the maximum size of an area which will be flushed.  If the area
 * is larger than this, then we flush the whole cache
 */
#define MAX_AREA_SIZE	32768

/*
 * the cache line size of the I and D cache
 */
#define CACHELINESIZE	32

/*
 * the size of the data cache
 */
#define CACHESIZE	32768

/*
 * and the page size
 */
#define PAGESIZE	4096

/*
 * Virtual address used to allocate the cache when flushed
 *
 * This must be an address range which is _never_ used.  It should 
 * apparently have a mapping in the corresponding page table for 
 * compatibility with future CPUs that _could_ require it.  For instance we
 * don't care.
 *
 * This must be aligned on a 2*CACHESIZE boundary.  The code selects one of
 * the 2 areas in alternance each time the clean_d_cache macro is used.
 * Without this the XScale core exhibits cache eviction problems and no one
 * knows why.  
 *
 * Reminder: the vector table is located at 0xffff0000-0xffff0fff.
 */
#define CLEAN_ADDR	0xfffe0000

/*
 * This macro is used to wait for a CP15 write and is needed
 * when we have to ensure that the last operation to the co-pro
 * was completed before continuing with operation.
 */
	.macro	cpwait, rd
	mrc	p15, 0, \rd, c2, c0, 0		@ arbitrary read of cp15
	mov	\rd, \rd			@ wait for completion
	sub 	pc, pc, #4			@ flush instruction pipeline
	.endm	

	.macro	cpwait_ret, lr, rd
	mrc	p15, 0, \rd, c2, c0, 0		@ arbitrary read of cp15
	sub	pc, \lr, \rd, LSR #32		@ wait for completion and
						@ flush instruction pipeline
	.endm

#if !CACHE_WRITE_THROUGH

/*
 * This macro cleans the entire dcache using line allocate.
 * The main loop has been unrolled to reduce loop overhead.
 * rd and rs are two scratch registers.
 */
	.macro  clean_d_cache, rd, rs
	ldr	\rs, =clean_addr
	ldr	\rd, [\rs]
	eor	\rd, \rd, #CACHESIZE
	str	\rd, [\rs]
	add	\rs, \rd, #CACHESIZE
1:	mcr	p15, 0, \rd, c7, c2, 5		@ allocate D cache line
	add	\rd, \rd, #CACHELINESIZE
	mcr	p15, 0, \rd, c7, c2, 5		@ allocate D cache line
	add	\rd, \rd, #CACHELINESIZE
	mcr	p15, 0, \rd, c7, c2, 5		@ allocate D cache line
	add	\rd, \rd, #CACHELINESIZE
	mcr	p15, 0, \rd, c7, c2, 5		@ allocate D cache line
	add	\rd, \rd, #CACHELINESIZE
	teq	\rd, \rs
	bne	1b
	.endm

	.macro	clean_d_line,	rd
	mcr	p15, 0, \rd, c7, c10, 1
	.endm

	.data
clean_addr:	.word	CLEAN_ADDR

#else

/*
 * If cache is write-through, there is no need to clean it.
 * Simply invalidating will do.
 */

	.macro  clean_d_cache, rd, rs
	mcr	p15, 0, \rd, c7, c6, 0
	.endm

	/* let's try to skip this needless operations at least within loops */
	.macro	clean_d_line,	rd
	.endm

#endif

	.text

/*
 * cpu_xscale_data_abort()
 *
 * obtain information about current aborted instruction.
 * Note: we read user space.  This means we might cause a data
 * abort here if the I-TLB and D-TLB aren't seeing the same
 * picture.  Unfortunately, this does happen.  We live with it.
 *
 *  r2 = address of aborted instruction
 *  r3 = cpsr
 *
 * Returns:
 *  r0 = address of abort
 *  r1 != 0 if writing
 *  r3 = FSR
 *  r4 = corrupted
 */
	.align	5
ENTRY(cpu_xscale_data_abort)
	mrc	p15, 0, r3, c5, c0, 0		@ get FSR
	mrc	p15, 0, r0, c6, c0, 0		@ get FAR
	ldr	r1, [r2]			@ read aborted instruction
	tst	r1, r1, lsr #21			@ C = bit 20
	sbc	r1, r1, r1			@ r1 = C - 1
	mov	pc, lr

/*
 * cpu_xscale_check_bugs()
 */
ENTRY(cpu_xscale_check_bugs)
	mrs	ip, cpsr
	bic	ip, ip, #F_BIT
	msr	cpsr, ip
	mov	pc, lr

#ifndef CONFIG_XSCALE_CACHE_ERRATA
/*
 * cpu_xscale_proc_init()
 *
 * Nothing too exciting at the moment
 */
ENTRY(cpu_xscale_proc_init)
	mov	pc, lr
#else
/*
 * We enable the cache here, but we make sure all the status bits for dirty
 * lines are cleared as well (see PXA250 erratum #120).
 */
ENTRY(cpu_xscale_proc_init)
	@ enable data cache
	ldr	r0, cr_p
	ldmia	r0, {r1, r2}
	orr	r1, r1, #0x4
	orr	r2, r2, #0x4
	stmia	r0, {r1, r2}
	mcr	p15, 0, r1, c1, c0, 0
	cpwait	r0

	@ invalidate data cache
	mcr	p15, 0, r0, c7, c6, 0

	@ fill main cache with write-through lines
	bic	r0, pc, #0x1f
	add	r1, r0, #CACHESIZE
1:	ldr	r2, [r0], #32
	cmp	r0, r1
	bne	1b

	@ enable test feature to force all fills to the mini-cache
	mov	r1, #0x8
	mcr	p15, 0, r1, c15, c15, 3

	@ fill mini-cache with write-through lines (2kbytes, 64 lines)
	add	r1, r0, #2048
2:	ldr	r2, [r0], #32
	cmp	r0, r1
	bne	2b

	@ disable test feature to force all fills to the mini-cache
	mov	r1, #0x0
	mcr	p15, 0, r1, c15, c15, 3

	@ invalidate data cache again
	mcr	p15, 0, r1, c7, c6, 0
	mov	pc, lr

cr_p:	.long	SYMBOL_NAME(cr_alignment)
#endif

/*
 * cpu_xscale_proc_fin()
 */
ENTRY(cpu_xscale_proc_fin)
	str	lr, [sp, #-4]!
	mov	r0, #F_BIT|I_BIT|SVC_MODE
	msr	cpsr_c, r0
	mrc	p15, 0, r0, c1, c0, 0		@ ctrl register
	bic	r0, r0, #0x1800			@ ...IZ...........
	bic	r0, r0, #0x0006			@ .............CA.
	mcr	p15, 0, r0, c1, c0, 0		@ disable caches
	bl	cpu_xscale_cache_clean_invalidate_all	@ clean caches
	ldr	pc, [sp], #4

/*
 * cpu_xscale_reset(loc)
 *
 * Perform a soft reset of the system.  Put the CPU into the
 * same state as it would be if it had been reset, and branch
 * to what would be the reset vector.
 *
 * loc: location to jump to for soft reset
 */
	.align	5
ENTRY(cpu_xscale_reset)
	mov	r1, #F_BIT|I_BIT|SVC_MODE
	msr	cpsr_c, r1			@ reset CPSR
	mrc	p15, 0, r1, c1, c0, 0		@ ctrl register
	bic	r1, r1, #0x0086			@ ........B....CA.
	bic	r1, r1, #0x3900			@ ...IZ..S........
	mcr	p15, 0, r1, c1, c0, 0		@ ctrl register
	mcr	p15, 0, ip, c7, c7, 0		@ invalidate I,D caches & BTB
	bic	r1, r1, #0x0001			@ ...............M
	mcr	p15, 0, r1, c1, c0, 0		@ ctrl register
	@ CAUTION: MMU turned off from this point. We count on the pipeline 
	@ already containing those two last instructions to survive.
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I & D TLBs
	mov	pc, r0

/*
 * cpu_xscale_do_idle(type)
 *
 * Cause the processor to idle
 *
 * type: 
 *   0 = slow idle
 *   1 = fast idle
 *   2 = switch to slow processor clock
 *   3 = switch to fast processor clock
 *
 * For now we do nothing but go to idle mode for every case
 *
 * XScale supports clock switching, but using idle mode support
 * allows external hardware to react to system state changes.
 */
	.align	5

ENTRY(cpu_xscale_do_idle)
	mov	r0, #1
	mcr	p14, 0, r0, c7, c0, 0		@ Go to IDLE
	mov	pc, lr

/* ================================= CACHE ================================ */

/*
 * cpu_xscale_cache_clean_invalidate_all (void)
 *
 * clean and invalidate all cache lines
 *
 * Note:
 *  1. We should preserve r0 at all times.
 *  2. Even if this function implies cache "invalidation" by its name,
 *     we don't need to actually use explicit invalidation operations
 *     since the goal is to discard all valid references from the cache
 *     and the cleaning of it already has that effect.
 *  3. Because of 2 above and the fact that kernel space memory is always
 *     coherent across task switches there is no need to worry about
 *     inconsistencies due to interrupts, ence no irq disabling.
 */
	.align	5
ENTRY(cpu_xscale_cache_clean_invalidate_all)
	mov	r2, #1
cpu_xscale_cache_clean_invalidate_all_r2:
	clean_d_cache r0, r1
	teq	r2, #0
	mcrne	p15, 0, ip, c7, c5, 0		@ Invalidate I cache & BTB
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

/*
 * cpu_xscale_cache_clean_invalidate_range(start, end, flags)
 *
 * clean and invalidate all cache lines associated with this area of memory
 *
 * start: Area start address
 * end:   Area end address
 * flags: nonzero for I cache as well
 */
	.align	5
ENTRY(cpu_xscale_cache_clean_invalidate_range)
	bic	r0, r0, #CACHELINESIZE - 1	@ round down to cache line
	sub	r3, r1, r0
	cmp	r3, #MAX_AREA_SIZE
	bhi	cpu_xscale_cache_clean_invalidate_all_r2
1:	clean_d_line r0				@ Clean D cache line
	mcr	p15, 0, r0, c7, c6, 1		@ Invalidate D cache line
	add	r0, r0, #CACHELINESIZE
	cmp	r0, r1
	blo	1b
	teq	r2, #0
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	moveq	pc, lr
	sub	r0, r0, r3
1:	mcr	p15, 0, r0, c7, c5, 1		@ Invalidate I cache line
	add	r0, r0, #CACHELINESIZE
	cmp	r0, r1
	blo	1b
	mcr	p15, 0, ip, c7, c5, 6		@ Invalidate BTB
	mov	pc, lr

/*
 * cpu_xscale_flush_ram_page(page)
 *
 * clean all cache lines associated with this memory page
 *
 * page: page to clean
 */
	.align	5
ENTRY(cpu_xscale_flush_ram_page)
#if !CACHE_WRITE_THROUGH
	mov	r1, #PAGESIZE
1:	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	subs	r1, r1, #2 * CACHELINESIZE
	bne	1b
#endif
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

/* ================================ D-CACHE =============================== */

/*
 * cpu_xscale_dcache_invalidate_range(start, end)
 *
 * throw away all D-cached data in specified region without an obligation
 * to write them back.  Note however that on XScale we must clean all
 * entries also due to hardware errata (80200 A0 & A1 only).
 *
 * start: virtual start address
 * end:   virtual end address
 */
	.align	5
ENTRY(cpu_xscale_dcache_invalidate_range)
	mrc	p15, 0, r2, c0, c0, 0		@ Read part no.
	eor	r2, r2, #0x69000000
	eor	r2, r2, #0x00052000		@ 80200 XX part no.
	bics	r2, r2, #0x1			@ Clear LSB in revision field
	moveq	r2, #0
	beq	cpu_xscale_cache_clean_invalidate_range	@ An 80200 A0 or A1

	tst	r0, #CACHELINESIZE - 1
	mcrne	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	tst	r1, #CACHELINESIZE - 1
	mcrne	p15, 0, r1, c7, c10, 1		@ Clean D cache line
	bic	r0, r0, #CACHELINESIZE - 1	@ round down to cache line
1:	mcr	p15, 0, r0, c7, c6, 1		@ Invalidate D cache line
	add	r0, r0, #CACHELINESIZE
	cmp	r0, r1
	blo	1b
	mov	pc, lr

/*
 * cpu_xscale_dcache_clean_range(start, end)
 *
 * For the specified virtual address range, ensure that all caches contain
 * clean data, such that peripheral accesses to the physical RAM fetch
 * correct data.
 *
 * start: virtual start address
 * end:   virtual end address
 */
	.align	5
ENTRY(cpu_xscale_dcache_clean_range)
#if !CACHE_WRITE_THROUGH
	bic	r0, r0, #CACHELINESIZE - 1
	sub	r2, r1, r0
	cmp	r2, #MAX_AREA_SIZE
	movhi	r2, #0
	bhi	cpu_xscale_cache_clean_invalidate_all_r2

1:	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	cmp	r0, r1
	blo	1b
#endif
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

/*
 * cpu_xscale_clean_dcache_page(page)
 *
 * Cleans a single page of dcache so that if we have any future aliased
 * mappings, they will be consistent at the time that they are created.
 *
 * Note:
 *  1. we don't need to flush the write buffer in this case. [really? -Nico]
 *  2. we don't invalidate the entries since when we write the page
 *     out to disk, the entries may get reloaded into the cache.
 */
	.align	5
ENTRY(cpu_xscale_dcache_clean_page)
#if !CACHE_WRITE_THROUGH
	mov	r1, #PAGESIZE
1:	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	subs	r1, r1, #4 * CACHELINESIZE
	bne	1b
#endif
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

/*
 * cpu_xscale_dcache_clean_entry(addr)
 *
 * Clean the specified entry of any caches such that the MMU
 * translation fetches will obtain correct data.
 *
 * addr: cache-unaligned virtual address
 */
	.align	5
ENTRY(cpu_xscale_dcache_clean_entry)
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

/* ================================ I-CACHE =============================== */

/*
 * cpu_xscale_icache_invalidate_range(start, end)
 *
 * invalidate a range of virtual addresses from the Icache
 *
 * start: virtual start address
 * end:   virtual end address
 *
 * Note1: This is vaguely defined as supposed to bring the dcache and the 
 *       icache in sync by the way this function is used.
 *
 * Note2: single I-cache line invalidation isn't used here since
 *        it also trashes the mini I-cache used by JTAG debuggers.
 */
	.align	5
ENTRY(cpu_xscale_icache_invalidate_range)
	bic	r0, r0, #CACHELINESIZE - 1
1:	clean_d_line r0				@ Clean D cache line
	add	r0, r0, #CACHELINESIZE
	cmp	r0, r1
	blo	1b
	mcr	p15, 0, ip, c7, c5, 0		@ Invalidate I cache & BTB
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

/*
 * cpu_xscale_icache_invalidate_page(page)
 *
 * invalidate all Icache lines associated with this area of memory
 *
 * page: page to invalidate
 */
	.align	5
ENTRY(cpu_xscale_icache_invalidate_page)
	mov	r1, #PAGESIZE
1:	mcr	p15, 0, r0, c7, c5, 1		@ Invalidate I cache line
	add	r0, r0, #CACHELINESIZE
	mcr	p15, 0, r0, c7, c5, 1		@ Invalidate I cache line
	add	r0, r0, #CACHELINESIZE
	mcr	p15, 0, r0, c7, c5, 1		@ Invalidate I cache line
	add	r0, r0, #CACHELINESIZE
	mcr	p15, 0, r0, c7, c5, 1		@ Invalidate I cache line
	add	r0, r0, #CACHELINESIZE
	subs	r1, r1, #4 * CACHELINESIZE
	bne	1b
	mcr	p15, 0, r0, c7, c5, 6		@ Invalidate BTB
	mov	pc, lr

/* ================================ CACHE LOCKING============================ 
 *
 * The XScale MicroArchitecture implements support for locking entries into
 * the data and instruction cache.  The following functions implement the core
 * low level instructions needed to accomplish the locking.  The developer's
 * manual states that the code that performs the locking must be in non-cached
 * memory.  To accomplish this, the code in xscale-cache-lock.c copies the
 * following functions from the cache into a non-cached memory region that
 * is allocated through consistent_alloc().
 *
 */
	.align	5
/*
 * xscale_icache_lock
 *
 * r0: starting address to lock
 * r1: end address to lock
 */
ENTRY(xscale_icache_lock)

iLockLoop:
	bic	r0, r0, #CACHELINESIZE - 1
	mcr	p15, 0, r0, c9, c1, 0	@ lock into cache
	cmp	r0, r1			@ are we done?
	add	r0, r0, #CACHELINESIZE	@ advance to next cache line
	bls	iLockLoop
	mov	pc, lr

/*
 * xscale_icache_unlock
 */
ENTRY(xscale_icache_unlock)
	mcr	p15, 0, r0, c9, c1, 1	@ Unlock icache
	mov	pc, lr
	
/*
 * xscale_dcache_lock
 *
 * r0: starting address to lock
 * r1: end address to lock
 */
ENTRY(xscale_dcache_lock)
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	r2, #1
	mcr	p15, 0, r2, c9, c2, 0	@ Put dcache in lock mode
	cpwait	ip			@ Wait for completion

	mrs	r2, cpsr
	orr	r3, r2, #F_BIT | I_BIT
dLockLoop:
	msr	cpsr_c, r3
	mcr	p15, 0, r0, c7, c10, 1	@ Write back line if it is dirty 
	mcr	p15, 0, r0, c7, c6, 1	@ Flush/invalidate line
	msr	cpsr_c, r2
	ldr	ip, [r0], #CACHELINESIZE @ Preload 32 bytes into cache from 
					@ location [r0]. Post-increment
					@ r3 to next cache line
	cmp	r0, r1			@ Are we done?
	bls	dLockLoop

	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	r2, #0
	mcr	p15, 0, r2, c9, c2, 0	@ Get out of lock mode
	cpwait_ret lr, ip

/*
 * xscale_dcache_unlock
 */
ENTRY(xscale_dcache_unlock)
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mcr	p15, 0, ip, c9, c2, 1	@ Unlock cache
	mov	pc, lr

/*
 * Needed to determine the length of the code that needs to be copied.
 */
	.align	5
ENTRY(xscale_cache_dummy)
	mov	pc, lr

/* ================================== TLB ================================= */

/*
 * cpu_xscale_tlb_invalidate_all()
 *
 * Invalidate all TLB entries
 */
	.align	5
ENTRY(cpu_xscale_tlb_invalidate_all)
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I & D TLBs
	cpwait_ret lr, ip

/*
 * cpu_xscale_tlb_invalidate_range(start, end)
 *
 * invalidate TLB entries covering the specified range
 *
 * start: range start address
 * end:   range end address
 */
	.align	5
ENTRY(cpu_xscale_tlb_invalidate_range)
	bic	r0, r0, #(PAGESIZE - 1) & 0x00ff
	bic	r0, r0, #(PAGESIZE - 1) & 0xff00
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
1:	mcr	p15, 0, r0, c8, c6, 1		@ invalidate D TLB entry
	mcr	p15, 0, r0, c8, c5, 1		@ invalidate I TLB entry
	add	r0, r0, #PAGESIZE
	cmp	r0, r1
	blo	1b
	cpwait_ret lr, ip

/*
 * cpu_xscale_tlb_invalidate_page(page, flags)
 *
 * invalidate the TLB entries for the specified page.
 *
 * page:  page to invalidate
 * flags: non-zero if we include the I TLB
 */
	.align	5
ENTRY(cpu_xscale_tlb_invalidate_page)
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	teq	r1, #0
	mcr	p15, 0, r0, c8, c6, 1		@ invalidate D TLB entry
	mcrne	p15, 0, r3, c8, c5, 1		@ invalidate I TLB entry
	cpwait_ret lr, ip

/* ================================ TLB LOCKING==============================
 *
 * The XScale MicroArchitecture implements support for locking entries into
 * the Instruction and Data TLBs.  The following functions provide the
 * low level support for supporting these under Linux.  xscale-lock.c 
 * implements some higher level management code.  Most of the following
 * is taken straight out of the Developer's Manual.
 */

/*
 * Lock I-TLB entry
 *
 * r0: Virtual address to translate and lock
 */
	.align	5
ENTRY(xscale_itlb_lock)
	mrs	r2, cpsr
	orr	r3, r2, #F_BIT | I_BIT
	msr	cpsr_c, r3			@ Disable interrupts
	mcr	p15, 0, r0, c8, c5, 1		@ Invalidate I-TLB entry
	mcr	p15, 0, r0, c10, c4, 0		@ Translate and lock
	msr	cpsr_c, r2			@ Restore interrupts
	cpwait_ret lr, ip

/*
 * Lock D-TLB entry
 *
 * r0: Virtual address to translate and lock
 */
	.align	5
ENTRY(xscale_dtlb_lock)
	mrs	r2, cpsr
	orr	r3, r2, #F_BIT | I_BIT
	msr	cpsr_c, r3			@ Disable interrupts
	mcr	p15, 0, r0, c8, c6, 1		@ Invalidate D-TLB entry
	mcr	p15, 0, r0, c10, c8, 0		@ Translate and lock
	msr	cpsr_c, r2			@ Restore interrupts
	cpwait_ret lr, ip

/*
 * Unlock all I-TLB entries
 */
	.align	5
ENTRY(xscale_itlb_unlock)
	mcr	p15, 0, ip, c10, c4, 1		@ Unlock I-TLB
	mcr	p15, 0, ip, c8, c5, 0		@ Invalidate I-TLB
	cpwait_ret lr, ip

/*
 * Unlock all D-TLB entries
 */
ENTRY(xscale_dtlb_unlock)
	mcr	p15, 0, ip, c10, c8, 1		@ Unlock D-TBL
	mcr	p15, 0, ip, c8, c6, 0		@ Invalidate D-TLB
	cpwait_ret lr, ip

/* =============================== PageTable ============================== */

/*
 * cpu_xscale_set_pgd(pgd)
 *
 * Set the translation base pointer to be as described by pgd.
 *
 * pgd: new page tables
 */
	.align	5
ENTRY(cpu_xscale_set_pgd)
	clean_d_cache r1, r2
	mcr	p15, 0, ip, c7, c5, 0		@ Invalidate I cache & BTB
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mcr	p15, 0, r0, c2, c0, 0		@ load page table pointer
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I & D TLBs
	cpwait_ret lr, ip

/*
 * cpu_xscale_set_pmd(pmdp, pmd)
 *
 * Set a level 1 translation table entry, and clean it out of
 * any caches such that the MMUs can load it correctly.
 *
 * pmdp: pointer to PMD entry
 * pmd:  PMD value to store
 */
	.align	5
ENTRY(cpu_xscale_set_pmd)
#if PMD_CACHE_WRITE_ALLOCATE && !CACHE_WRITE_THROUGH
	and	r2, r1, #PMD_TYPE_MASK|PMD_SECT_CACHEABLE|PMD_SECT_BUFFERABLE
	cmp	r2, #PMD_TYPE_SECT|PMD_SECT_CACHEABLE|PMD_SECT_BUFFERABLE
	orreq	r1, r1, #PMD_SECT_TEX(1)
#elif CACHE_WRITE_THROUGH
	and	r2, r1, #PMD_TYPE_MASK|PMD_SECT_CACHEABLE|PMD_SECT_BUFFERABLE
	cmp	r2, #PMD_TYPE_SECT|PMD_SECT_CACHEABLE|PMD_SECT_BUFFERABLE
	biceq	r1, r1, #PMD_SECT_BUFFERABLE
#endif
#ifdef CONFIG_ARCH_IXP2000
      tst    r1, #PMD_SECT_TEX(1)
      andeq  r2, r1,  #PMD_TYPE_MASK | PMD_SECT_CACHEABLE | PMD_SECT_BUFFERABLE
      cmpeq  r2, #PMD_TYPE_SECT
      orreq  r1, r1, #PMD_SECT_TEX(1)
      orreq  r1, r1, #PMD_SECT_BUFFERABLE
#endif

	str	r1, [r0]
	mov	ip, #0
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

/*
 * cpu_xscale_set_pte(ptep, pte)
 *
 * Set a PTE and flush it out
 *
 * Errata 40: must set memory to write-through for user read-only pages.
 */
	.align	5
ENTRY(cpu_xscale_set_pte)
	str	r1, [r0], #-1024		@ linux version

	bic	r2, r1, #0xff0
	orr	r2, r2, #PTE_TYPE_EXT		@ extended page

	eor	r3, r1, #L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_WRITE | L_PTE_DIRTY

	tst	r3, #L_PTE_USER | L_PTE_EXEC	@ User or Exec?
	orrne	r2, r2, #PTE_EXT_AP_URO_SRW	@ yes -> user r/o, system r/w

	tst	r3, #L_PTE_WRITE | L_PTE_DIRTY	@ Write and Dirty?
	orreq	r2, r2, #PTE_EXT_AP_UNO_SRW	@ yes -> user n/a, system r/w
						@ combined with user -> user r/w

	@
	@ Handle the X bit.  We want to set this bit for the minicache
	@ (U = E = B = W = 0, C = 1) or when write allocate is enabled,
	@ and we have a writeable, cacheable region.  If we ignore the
	@ U and E bits, we can allow user space to use the minicache as
	@ well.
	@
	@  X = C & ~W & ~B
	@      | C & W & B & write_allocate
	@
	eor	ip, r1, #L_PTE_CACHEABLE
	tst	ip, #L_PTE_CACHEABLE | L_PTE_WRITE | L_PTE_BUFFERABLE
#if PTE_CACHE_WRITE_ALLOCATE && !CACHE_WRITE_THROUGH
	eorne	ip, r1, #L_PTE_CACHEABLE | L_PTE_WRITE | L_PTE_BUFFERABLE
	tstne	ip, #L_PTE_CACHEABLE | L_PTE_WRITE | L_PTE_BUFFERABLE
#endif
	orreq	r2, r2, #PTE_EXT_TEX(1)

#if CACHE_WRITE_THROUGH
	bic	r2, r2, #L_PTE_BUFFERABLE
#else
	@
	@ Errata 40: The B bit must be cleared for a user read-only
	@ cacheable page.
	@
	@  B = B & ~((U|E) & C & ~W)
	@
	and	ip, r1, #L_PTE_USER | L_PTE_EXEC | L_PTE_WRITE | L_PTE_CACHEABLE
	teq	ip, #L_PTE_USER | L_PTE_CACHEABLE
	teqne	ip, #L_PTE_EXEC | L_PTE_CACHEABLE
	teqne	ip, #L_PTE_USER | L_PTE_EXEC | L_PTE_CACHEABLE
	biceq	r2, r2, #PTE_BUFFERABLE
#endif

#ifdef CONFIG_ARCH_IXP2000
      and    ip, r2, #PTE_TYPE_MASK | PTE_EXT_TEX(1) | L_PTE_CACHEABLE | L_PTE_BUFFERABLE
      cmp    ip, #PTE_TYPE_EXT
      orreq  r2, r2, #PTE_EXT_TEX(1) | L_PTE_BUFFERABLE
#endif

	tst	r3, #L_PTE_PRESENT | L_PTE_YOUNG	@ Present and Young?
	movne	r2, #0				@ no -> fault

	str	r2, [r0]			@ hardware version

 	@ We try to map 64K page entries when possible.  
 	@ We do that for kernel space only since the usage pattern from
 	@ the setting of VM area is quite simple.  User space is not worth
 	@ the implied complexity because of ever randomly changing PTEs 
 	@ (page aging, swapout, etc) requiring constant coherency checks.
 	@ Since PTEs are usually set in increasing order, we test the
 	@ possibility for a large page only when given the last PTE of a
 	@ 64K boundary.
 	tsteq	r1, #L_PTE_USER
 	andeq	r1, r0, #(15 << 2)
 	teqeq	r1, #(15 << 2)
 	beq	1f

	mov	r0, r0
	mov	ip, #0
	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mov	pc, lr

 	@ See if we have 16 identical PTEs but with consecutive base addresses
 1:	bic	r3, r2, #0x0000f000
 	mov	r1, #0x0000f000
 2:	eor	r2, r2, r3
	teq	r2, r1
 	bne	4f
 	subs	r1, r1, #0x00001000
 	ldr	r2, [r0, #-4]!
 	bne	2b
 	eors	r2, r2, r3
 	bne	4f
 
 	@ Now create our LARGE PTE from the current EXT one.
 	bic	r3, r3, #PTE_TYPE_MASK
 	orr	r3, r3, #PTE_TYPE_LARGE
 	and	r2, r3, #0x30			@ EXT_AP --> LARGE_AP0
 	orr	r2, r2, r2, lsl #2		@ add LARGE_AP1
 	orr	r2, r2, r2, lsl #4		@ add LARGE_AP3 + LARGE_AP2
 	and	r1, r3, #0x3c0			@ EXT_TEX
 	bic	r3, r3, #0x3c0
 	orr	r2, r2, r1, lsl #(12 - 6)	@ --> LARGE_TEX
 	orr	r2, r2, r3			@ add remaining bits
 
 	@ then put it in the pagetable
 	mov	r3, r2
 3:	strd	r2, [r0], #8
 	tst	r0, #(15 << 2)
 	bne	3b
 
 	@ Then sync the 2 corresponding cache lines
 	sub	r0, r0, #(16 << 2)
 	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
 4:	orr	r0, r0, #(15 << 2)
 	mcr	p15, 0, r0, c7, c10, 1		@ Clean D cache line
 	mov	ip, #0
 	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
 	mov	pc, lr

	.ltorg

cpu_manu_name:
	.asciz	"Intel"

cpu_80200_name:
	.asciz	"XScale-80200"

cpu_80321_name:
	.asciz	"XScale-IOP80321"

cpu_pxa250_name:
	.asciz	"XScale-PXA250"

cpu_ixp425_name:
	.asciz	"XScale-IXP4xx/IXC11xx"

cpu_ixp2400_name:
	.asciz	"XScale-IXP2400"

cpu_ixp2800_name:
	.asciz	"XScale-IXP2800"

	.align

	.section ".text.init", #alloc, #execinstr

__xscale_setup:
	mov	r0, #F_BIT|I_BIT|SVC_MODE
	msr	cpsr_c, r0
	mcr	p15, 0, ip, c7, c7, 0		@ invalidate I, D caches & BTB
	mcr	p15, 0, ip, c7, c10, 4		@ Drain Write (& Fill) Buffer
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I, D TLBs
	mcr	p15, 0, r4, c2, c0, 0		@ load page table pointer
	mov	r0, #0x1f			@ Domains 0, 1 = client
	mcr	p15, 0, r0, c3, c0, 0		@ load domain access register
	mov	r0, #1				@ Allow access to CP0
	orr	r0, r0, #1 << 13		@ and CP13
	orr	r0, r0, #1 << 6			@ cp6 for IOP3xx 
	mcr	p15, 0, r0, c15, c1, 0		
#if CACHE_WRITE_THROUGH
	mov	r0, #0x20
#else
	mov	r0, #0x00
#endif
	mcr	p15, 0, r0, c1, c1, 0		@ set auxiliary control reg
	mrc	p15, 0, r0, c1, c0, 0		@ get control register
	bic	r0, r0, #0x0200			@ ......R.........
	bic	r0, r0, #0x0002			@ ..............A.
	orr	r0, r0, #0x0005			@ .............C.M
	orr	r0, r0, #0x3900			@ ..VIZ..S........
#ifdef CONFIG_XSCALE_CACHE_ERRATA
	bic	r0, r0, #0x0004			@ see cpu_xscale_proc_init
#endif
	mov	pc, lr

	.text

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */

	.type	xscale_processor_functions, #object
ENTRY(xscale_processor_functions)
	.word	cpu_xscale_data_abort
	.word	cpu_xscale_check_bugs
	.word	cpu_xscale_proc_init
	.word	cpu_xscale_proc_fin
	.word	cpu_xscale_reset
	.word	cpu_xscale_do_idle

	/* cache */
	.word	cpu_xscale_cache_clean_invalidate_all
	.word	cpu_xscale_cache_clean_invalidate_range
	.word	cpu_xscale_flush_ram_page

	/* dcache */
	.word	cpu_xscale_dcache_invalidate_range
	.word	cpu_xscale_dcache_clean_range
	.word	cpu_xscale_dcache_clean_page
	.word	cpu_xscale_dcache_clean_entry

	/* icache */
	.word	cpu_xscale_icache_invalidate_range
	.word	cpu_xscale_icache_invalidate_page

	/* tlb */
	.word	cpu_xscale_tlb_invalidate_all
	.word	cpu_xscale_tlb_invalidate_range
	.word	cpu_xscale_tlb_invalidate_page

	/* pgtable */
	.word	cpu_xscale_set_pgd
	.word	cpu_xscale_set_pmd
	.word	cpu_xscale_set_pte
	.size	xscale_processor_functions, . - xscale_processor_functions

	.type	cpu_80200_info, #object
cpu_80200_info:
	.long	cpu_manu_name
	.long	cpu_80200_name
	.size	cpu_80200_info, . - cpu_80200_info

	.type	cpu_80321_info, #object
cpu_80321_info:
	.long	cpu_manu_name
	.long	cpu_80321_name
	.size	cpu_80321_info, . - cpu_80321_info

	.type	cpu_pxa250_info, #object
cpu_pxa250_info:
	.long	cpu_manu_name
	.long	cpu_pxa250_name
	.size	cpu_pxa250_info, . - cpu_pxa250_info

cpu_ixp425_info:
	.long	cpu_manu_name
	.long	cpu_ixp425_name
	.size	cpu_ixp425_info, . - cpu_ixp425_info

        .type   cpu_ixp2400_info, #object
cpu_ixp2400_info:
        .long   cpu_manu_name
        .long   cpu_ixp2400_name
        .size   cpu_ixp2400_info, . - cpu_ixp2400_info

	.type	cpu_ixp2800_info, #object
cpu_ixp2800_info:
	.long	cpu_manu_name
	.long	cpu_ixp2800_name
	.size	cpu_ixp2800_info, . - cpu_ixp2800_info

	.type	cpu_arch_name, #object
cpu_arch_name:
	.asciz	"armv5"
	.size	cpu_arch_name, . - cpu_arch_name

	.type	cpu_elf_name, #object
cpu_elf_name:
	.asciz	"v5"
	.size	cpu_elf_name, . - cpu_elf_name
	.align

	.section ".proc.info", #alloc, #execinstr

	.type	__80200_proc_info,#object
__80200_proc_info:
	.long	0x69052000
	.long	0xffffe3f0
#if CACHE_WRITE_THROUGH
	.long	0x00000c0a
#else
	.long	0x00000c0e
#endif
	b	__xscale_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
	.long	cpu_80200_info
	.long	xscale_processor_functions
	.size	__80200_proc_info, . - __80200_proc_info

	.type	__80321_proc_info,#object
__80321_proc_info:
	.long	0x69052420
	.long	0xfffff7e0
#if CACHE_WRITE_THROUGH
	.long	0x00000c0a
#else
	.long	0x00000c0e
#endif
	b	__xscale_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
	.long	cpu_80321_info
	.long	xscale_processor_functions
	.size	__80321_proc_info, . - __80321_proc_info
 
	.type	__pxa250_proc_info,#object
__pxa250_proc_info:
	.long	0x69052100
	.long	0xfffff7f0
#if CACHE_WRITE_THROUGH
	.long	0x00000c0a
#else
	.long	0x00000c0e
#endif
	b	__xscale_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
	.long	cpu_pxa250_info
	.long	xscale_processor_functions
	.size	__pxa250_proc_info, . - __pxa250_proc_info

	.type	__ixp425_proc_info, #object
__ixp425_proc_info:
#if 0
        .long   0x69054000
	.long   0xfffffe00
#endif
	.long   0x690541c0
	.long   0xffffffc0
#if CACHE_WRITE_THROUGH
	.long   0x00000c0a
#else
	.long   0x00000c0e
#endif        
	b       __xscale_setup
	.long   cpu_arch_name
	.long   cpu_elf_name
	.long   HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
	.long   cpu_ixp425_info
	.long   xscale_processor_functions
	.size   __ixp425_proc_info, . - __ixp425_proc_info                

       .type   __ixp2400_proc_info,#object
__ixp2400_proc_info:
        .long   0x69054190
        .long   0xfffffff0
#if CACHE_WRITE_THROUGH
	.long	0x00000c0a
#else
	.long	0x00000c0e
#endif
        b       __xscale_setup
        .long   cpu_arch_name
        .long   cpu_elf_name
        .long   HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
        .long   cpu_ixp2400_info
        .long   xscale_processor_functions
        .size   __ixp2400_proc_info, . - __ixp2400_proc_info
#
	.type	__ixp2800_proc_info,#object
__ixp2800_proc_info:
	.long	0x690541a0
	.long	0xfffffff0
#if CACHE_WRITE_THROUGH
	.long	0x00000c0a
#else
	.long	0x00000c0e
#endif
	b	__xscale_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP
	.long	cpu_ixp2800_info
	.long	xscale_processor_functions
	.size	__ixp2800_proc_info, . - __ixp2800_proc_info

