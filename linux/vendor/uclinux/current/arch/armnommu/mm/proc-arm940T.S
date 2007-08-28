/*
 *  linux/arch/arm/mm/arm940T.S: Protection Unit (PU) functions for ARM940T
 *
 * Copyright (C) 2002-2003 Arcturus Networks Inc.
 *                         by Oleksandr Zhadan <www.ArcturusNetworks.com>
 *
 */

#include <linux/linkage.h>
#include <linux/config.h>
#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>

/*
 * This definitions are not used but it may answer some
 * questions about ARM940T architecture.
 *
 * The size of the I or D cache:
 * #define MAX_CACHE_SIZE	4096
 *
 * the cache line size of the I or D cache:
 * #define CACHELINESIZE	16
 *
 * the segment size:
 * #define SEG_SIZE		1024
 *
 * the number of segments:
 * #define SEG_NUM		4
 *
 * the number of entries
 * #define ENTRIES_NUM		64
 *
 */

	.text

/*
 * cpu_arm940_data_abort()
 *
 * "obtain information about current aborted instruction
 * Note: we read user space.  This means we might cause a data
 * abort here if the I-TLB and D-TLB aren't seeing the same
 * picture.  Unfortunately, this does happen.  We live with it."
 *				Doesn't work for ARM940.   OZH.
 *  Input:
 *  r2 = address of aborted instruction
 *  r3 = cpsr
 *
 * Returns:
 *  r0 = address of aborted instruction
 *  r1 != 0 if writing
 *  r3 = FSR
 *  r4 = corrupted
 */
 
	.align	5
ENTRY(cpu_arm940_data_abort)
	mov	r3, #0			@ FIXME: ARM940 does not have the fault
					@ status register. We set 0 status to
					@ get "Unhandled fault" with SIGKILL
					@ signal if it is not from the kernel.
	mov	r0, r2			@ FAR = real address
	ldr	r1, [r2]		@ read aborted instruction
	and	r3, r3, #255
	tst	r1, r1, lsr #21		@ C = bit 20
	sbc	r1, r1, r1		@ r1 = C - 1
	mov	pc, lr

/*
 * cpu_arm940_check_bugs()
 */
ENTRY(cpu_arm940_check_bugs)
	mrs	ip, cpsr
	bic	ip, ip, #F_BIT
	msr	cpsr, ip
	mov	pc, lr

/*
 * cpu_arm940_proc_init()
 */
ENTRY(cpu_arm940_proc_init)
	mov	pc, lr

/*
 * cpu_arm940_proc_fin()
 */
ENTRY(cpu_arm940_proc_fin)
	stmfd	sp!, {lr}
	mov	ip, #F_BIT | I_BIT | SVC_MODE
	msr	cpsr_c, ip
	bl	cpu_arm940_cache_clean_invalidate_all
	mrc	p15, 0, r0, c1, c0, 0		@ ctrl register
	bic	r0, r0, #0x1000			@ ...i............
	bic	r0, r0, #0x0004			@ .............d..
	mcr	p15, 0, r0, c1, c0, 0		@ disable caches
	ldmfd	sp!, {pc}

/*
 * cpu_arm940_reset(loc)
 *
 * Perform a soft reset of the system.  Put the CPU into the
 * same state as it would be if it had been reset, and branch
 * to what would be the reset vector.
 *
 * loc: location to jump to for soft reset
 */
	.align	5
ENTRY(cpu_arm940_reset)
	mov	ip, #0
	mcr	p15, 0, ip, c7, c5, 0		@ invalidate(flush) I cache
	mcr	p15, 0, ip, c7, c6, 0		@ invalidate(flush) D cache
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mrc	p15, 0, ip, c1, c0, 0		@ ctrl register
	bic	ip, ip, #0x1000			@ ...i............
	bic	ip, ip, #0x0004			@ .............d..
	bic	ip, ip, #0x0001			@ ...............p
	mcr	p15, 0, ip, c1, c0, 0		@ ctrl register
	mov	pc, r0				@ Start from r0 address

/*
 * cpu_arm940_do_idle()
 */
	.align	5
ENTRY(cpu_arm940_do_idle)
#if defined(CONFIG_CPU_ARM940_CPU_IDLE)
	mcr	p15, 0, r0, c7, c0, 4		@ Wait for interrupt
#endif
	mov	pc, lr

/* ================================= CACHE ================================ */
/*
 * cpu_arm940_cache_clean_invalidate_all()
 *
 * clean and invalidate all cache lines
 *
 * Note:
 *  1. we should preserve r0 at all times
 */
	.align	5
ENTRY(cpu_arm940_cache_clean_invalidate_all)
ENTRY(cpu_arm940_flush_ram_page)
	mov	r2, #1

cpu_arm940_cache_clean_invalidate_all_r2:

	mrc	p15, 0, ip, c3, c0, 0
	tst	ip, #0xff			@ is Wbuffer enabled?
	beq	3f
/*
 * 'Clean & Invalidate whole DCache'
 * Re-written to use Index Ops.
 * Uses registers r1, r3 and ip
 */
	mov	r1, #3 << 4			@ 4 segments
1:	orr	r3, r1, #63 << 26		@ 64 entries
2:	mcr	p15, 0, r3, c7, c14, 2		@ clean & invalidate(flush) D entry
	subs	r3, r3, #1 << 26
	bcs	2b				@ entries 63 to 0
	subs	r1, r1, #1 << 3
	bcs	1b				@ segments 3 to 0
	mov	ip, #0
	b	4f
3:
	mov	ip, #0
	mcr	p15, 0, ip, c7, c6, 0		@ invalidate(flush) D cache
	nop;nop;nop
4:
	teq	r2, #0
	mcrne	p15, 0, ip, c7, c5, 0		@ invalidate(flush) I cache
	nop;nop;nop
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

/*
 * cpu_arm940_cache_clean_invalidate_range(start, end, flags)
 *
 * clean and invalidate all cache lines associated with this area of memory
 *
 * start: Area start address
 * end:   Area end address
 * flags: nonzero for I cache as well
 */
		.align	5
ENTRY(cpu_arm940_cache_clean_invalidate_range)
	b	cpu_arm940_cache_clean_invalidate_all_r2
	nop;nop;nop

/* ================================ D-CACHE =============================== */

/*
 * cpu_arm940_dcache_invalidate_all()
 *
 */
	.align	5
ENTRY(cpu_arm940_dcache_invalidate_clean_all)
ENTRY(cpu_arm940_dcache_invalidate_all)
ENTRY(cpu_arm940_dcache_invalidate_range)
	mov	r2, #0
	b	cpu_arm940_cache_clean_invalidate_all_r2
	nop;nop;nop

/*
 * cpu_arm940_dcache_clean_all()
 *
 */
	.align	5
ENTRY(cpu_arm940_dcache_clean_all)
ENTRY(cpu_arm940_dcache_clean_range)
ENTRY(cpu_arm940_dcache_clean_page)
ENTRY(cpu_arm940_dcache_clean_entry)
	mrc	p15, 0, r1, c3, c0, 0
	tst	r1, #0xff			@ is Wbuffer enabled?
	beq	3f
	mov	r1, #3 << 4			@ 4 segments
1:	orr	r2, r1, #63 << 26		@ 64 entries
2:	mcr	p15, 0, r2, c7, c10, 2		@ clean D entry
	subs	r2, r2, #1 << 26
	bcs	2b				@ entries 63 to 0
	subs	r1, r1, #1 << 3
	bcs	1b				@ segments 3 to 0
3:
	mov	pc, lr

	.align	5
ENTRY(cpu_arm940_dcache_enable)
	mrc	15, 0, r0, c1, c0, 0
	orr	r0, r0, #0x4
	mcr	15, 0, r0, c1, c0, 0
	mov	pc, lr

ENTRY(cpu_arm940_dcache_disable)
	mrc	15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x4
	mcr	15, 0, r0, c1, c0, 0
	mov	pc, lr

ENTRY(cpu_arm940_dcache_flush)
	mov	r0, #0
	mcr	15, 0, r0, c7, c6, 0
	nop;nop;nop
	mov	pc, lr

/* ================================ I-CACHE =============================== */

	.align	5
ENTRY(cpu_arm940_icache_invalidate_all)
ENTRY(cpu_arm940_icache_invalidate_range)
ENTRY(cpu_arm940_icache_invalidate_page)

	b	cpu_arm940_cache_clean_invalidate_all
	nop;nop;nop

/* ================================== TLB & PageTable ===================== */

	.align	5
ENTRY(cpu_arm940_tlb_invalidate_all)
ENTRY(cpu_arm940_tlb_invalidate_range)
ENTRY(cpu_arm940_tlb_invalidate_page)
ENTRY(cpu_arm940_set_pgd)
ENTRY(cpu_arm940_set_pmd)
ENTRY(cpu_arm940_set_pte)

	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	mov	pc, lr

/* =============================== PU functions ===========================
 *
 * void arm940_uncache_region_x( unsigned int baseandsize, unsigned int flag);
 *
 */
ENTRY(arm940_uncache_region_4)
	teq	r1, #0
	bne	1f
	mcr	p15, 0, r1, c7, c6, 0		@ invalidate D cache
	mcr	p15, 0, r0, c6, c4, 0		@ For data

	mrc	p15, 0, r0, c2, c0, 0		@ Make uncacheble
	bic	r0, r0, #0x10
	mcr	p15, 0, r0, c2, c0, 0

	mrc	p15, 0, r0, c3, c0, 0		@ Make unbufferable
	bic	r0, r0, #0x10
	mcr	p15, 0, r0, c3, c0, 0
	mov	pc, lr
1:
	mov	r1, #0
	mcr	p15, 0, r1, c7, c5, 0		@ invalidate(flush) I cache
	mcr	p15, 0, r0, c6, c4, 1		@ For instruction
	mrc	p15, 0, r0, c2, c0, 1		@ Make uncacheble
	bic	r0, r0, #0x10
	mcr	p15, 0, r0, c2, c0, 1
	mov	pc, lr

ENTRY(arm940_uncache_region_5)
	teq	r1, #0
	bne	1f
	mcr	p15, 0, r1, c7, c6, 0		@ invalidate D cache
	mcr	p15, 0, r0, c6, c5, 0		@ For data

	mrc	p15, 0, r0, c2, c0, 0		@ Make uncacheble
	bic	r0, r0, #0x20
	mcr	p15, 0, r0, c2, c0, 0

	mrc	p15, 0, r0, c3, c0, 0		@ Make unbufferable
	bic	r0, r0, #0x20
	mcr	p15, 0, r0, c3, c0, 0
	mov	pc, lr
1:
	mov	r1, #0
	mcr	p15, 0, r1, c7, c5, 0		@ invalidate(flush) I cache
	mcr	p15, 0, r0, c6, c5, 1		@ For instruction
	mrc	p15, 0, r0, c2, c0, 1		@ Make uncacheble
	bic	r0, r0, #0x20
	mcr	p15, 0, r0, c2, c0, 1
	mov	pc, lr

ENTRY(arm940_uncache_region_6)
	teq	r1, #0
	bne	1f
	mcr	p15, 0, r1, c7, c6, 0		@ invalidate D cache
	mcr	p15, 0, r0, c6, c6, 0		@ For data

	mrc	p15, 0, r0, c2, c0, 0		@ Make uncacheble
	bic	r0, r0, #0x40
	mcr	p15, 0, r0, c2, c0, 0

	mrc	p15, 0, r0, c3, c0, 0		@ Make unbufferable
	bic	r0, r0, #0x40
	mcr	p15, 0, r0, c3, c0, 0
	mov	pc, lr
1:
	mov	r1, #0
	mcr	p15, 0, r1, c7, c5, 0		@ invalidate(flush) I cache
	mcr	p15, 0, r0, c6, c6, 1		@ For instruction
	mrc	p15, 0, r0, c2, c0, 1		@ Make uncacheble
	bic	r0, r0, #0x40
	mcr	p15, 0, r0, c2, c0, 1
	mov	pc, lr

ENTRY(arm940_uncache_region_7)
	teq	r1, #0
	bne	1f
	mcr	p15, 0, r1, c7, c6, 0		@ invalidate D cache
	mcr	p15, 0, r0, c6, c7, 0		@ For data

	mrc	p15, 0, r0, c2, c0, 0		@ Make uncacheble
	bic	r0, r0, #0x80
	mcr	p15, 0, r0, c2, c0, 0

	mrc	p15, 0, r0, c3, c0, 0		@ Make unbufferable
	bic	r0, r0, #0x80
	mcr	p15, 0, r0, c3, c0, 0
	mov	pc, lr
1:	
	mov	r1, #0
	mcr	p15, 0, r1, c7, c5, 0		@ invalidate(flush) I cache
	mcr	p15, 0, r0, c6, c7, 1		@ For instruction
	mrc	p15, 0, r0, c2, c0, 1		@ Make uncacheble
	bic	r0, r0, #0x80
	mcr	p15, 0, r0, c2, c0, 1
	mov	pc, lr

	.global arm940_get_creg_0
arm940_get_creg_0:
	mrc	p15, 0, r0, c0, c0, 0
	mov	pc, lr

	.global arm940_get_creg_1
arm940_get_creg_1:
	mrc	p15, 0, r0, c1, c0, 0
	mov	pc, lr

	.global arm940_get_creg_2
arm940_get_creg_2:
	teq	r0, #0
	mrceq	p15, 0, r0, c2, c0, 0
	mrcne	p15, 0, r0, c2, c0, 1
	mov	pc, lr

	.global arm940_get_creg_3
arm940_get_creg_3:
	mrc	p15, 0, r0, c3, c0, 0
	mov	pc, lr

	.global arm940_get_creg_5
arm940_get_creg_5:
	teq	r0, #0
	mrceq	p15, 0, r0, c5, c0, 0
	mrcne	p15, 0, r0, c5, c0, 1
	mov	pc, lr

	.global arm940_get_creg_6_0
arm940_get_creg_6_0:
	teq	r0, #0
	mrceq	p15, 0, r0, c6, c0, 0
	mrcne	p15, 0, r0, c6, c0, 1
	mov	pc, lr

	.global arm940_get_creg_6_1
arm940_get_creg_6_1:
	teq	r0, #0
	mrceq	p15, 0, r0, c6, c1, 0
	mrcne	p15, 0, r0, c6, c1, 1
	mov	pc, lr

	.global arm940_get_creg_6_2
arm940_get_creg_6_2:
	teq	r0, #0
	mrceq	p15, 0, r0, c6, c2, 0
	mrcne	p15, 0, r0, c6, c2, 1
	mov	pc, lr

	.global arm940_get_creg_6_3
arm940_get_creg_6_3:
	teq	r0, #0
	mrceq	p15, 0, r0, c6, c3, 0
	mrcne	p15, 0, r0, c6, c3, 1
	mov	pc, lr

	.global arm940_get_creg_6_4
arm940_get_creg_6_4:
	teq	r0, #0
	mrceq	p15, 0, r0, c6, c4, 0
	mrcne	p15, 0, r0, c6, c4, 1
	mov	pc, lr

	.global arm940_get_creg_6_5
arm940_get_creg_6_5:
	teq	r0, #0
	mrceq	p15, 0, r0, c6, c5, 0
	mrcne	p15, 0, r0, c6, c5, 1
	mov	pc, lr

	.global arm940_get_creg_6_6
arm940_get_creg_6_6:
	teq	r0, #0
	mrceq	p15, 0, r0, c6, c6, 0
	mrcne	p15, 0, r0, c6, c6, 1
	mov	pc, lr

	.global arm940_get_creg_6_7
arm940_get_creg_6_7:
	teq	r0, #0
	mrceq	p15, 0, r0, c6, c7, 0
	mrcne	p15, 0, r0, c6, c7, 1
	mov	pc, lr

cpu_manu_name:
	.asciz	"Samsung"
ENTRY(cpu_arm940_name)
	.ascii	"Arm940T"
#if defined(CONFIG_CPU_ARM940_CPU_IDLE)
	.ascii	"s"
#endif

#if defined(CONFIG_UCBOOTSTRAP)

icache_ascii:
	.ascii	"-"
dcache_ascii:
	.ascii	"-"
wbuffer_ascii:
	.ascii	"(wt)"
	.ascii	"\0"
	.align

	.section ".text.init", #alloc, #execinstr

__arm940_setup:
	mov	r0, #0
	MCR	p15, 0, r0, c7, c5, 0		@ invalidate ICache on v4
	MCR	p15, 0, r0, c7, c6, 0		@ invalidate DCache on v4
	MCR	p15, 0, r0, c7, c10, 4		@ drain write buffer on v4
	MRC	p15, 0, r0, c1, c0, 0		@ get control register v4
	
	ldr	r1, ptr_icache_ascii
	tst	r0, #0x1000			@ is Icache enable?
	movne	r2, #0x69
	moveq	r2, #0x2d
	strneb	r2, [r1]
	nop;nop;nop

	ldr	r1, ptr_dcache_ascii
	mov	r2, #0x64
	tst	r0, #0x0004			@ is Dcache enable?
	movne	r2, #0x64
	moveq	r2, #0x2d
	strneb	r2, [r1]
	nop;nop;nop
	
	ldr	r1, ptr_wbuffer_ascii
	mrc	p15, 0, r2, c3, c0, 0
	tst	r2, #0xff			@ is Wbuffer enable?
	movne	r2, #0x62
	moveq	r2, #0x74
	strneb	r2, [r1]

	orr	r0, r0, #1			@ Enable PU
	mov	pc, lr
	nop;nop;nop

ptr_icache_ascii:
	.long	icache_ascii
ptr_dcache_ascii:
	.long	dcache_ascii
ptr_wbuffer_ascii:
	.long	wbuffer_ascii+2
	
#else	/* CONFIG_UCBOOTSTRAP */

#if defined(CONFIG_CPU_ARM940_I_CACHE_ON)
	.ascii	"i"
#endif
#if defined(CONFIG_CPU_ARM940_D_CACHE_ON)
	.ascii	"d"
#if defined(CONFIG_CPU_ARM940_WRITETHROUGH)
	.ascii	"(wt)"
#else
	.ascii	"(wb)"
#endif
#endif
	.ascii	"\0"
	.align

	.section ".text.init", #alloc, #execinstr

__arm940_setup:
	mov	r0, #0
	MCR	p15, 0, r0, c7, c5, 0		@ invalidate ICache on v4
	MCR	p15, 0, r0, c7, c6, 0		@ invalidate DCache on v4
	MCR	p15, 0, r0, c7, c10, 4		@ drain write buffer on v4
	MRC	p15, 0, r0, c1, c0, 0		@ get control register v4
#if defined(CONFIG_CPU_ARM940_I_CACHE_ON)
	orr	r0, r0, #0x1000
#else
	bic	r0, r0, #0x1000
#endif
#if defined(CONFIG_CPU_ARM940_D_CACHE_ON)
	orr	r0, r0, #0x4
#else
	bic	r0, r0, #0x4
#endif
	orr	r0, r0, #1			@ Enable PU
	mov	pc, lr
	nop;nop;nop

#endif	/* CONFIG_UCBOOTSTRAP */

	
	.text

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */
	.type	arm940_processor_functions, #object
arm940_processor_functions:
	.word	cpu_arm940_data_abort
	.word	cpu_arm940_check_bugs
	.word	cpu_arm940_proc_init
	.word	cpu_arm940_proc_fin
	.word	cpu_arm940_reset
	.word   cpu_arm940_do_idle

	/* cache */
	.word	cpu_arm940_cache_clean_invalidate_all
	.word	cpu_arm940_cache_clean_invalidate_range
	.word	cpu_arm940_flush_ram_page

	/* dcache */
	.word	cpu_arm940_dcache_invalidate_range
	.word	cpu_arm940_dcache_clean_range
	.word	cpu_arm940_dcache_clean_page
	.word	cpu_arm940_dcache_clean_entry

	/* icache */
	.word	cpu_arm940_icache_invalidate_range
	.word	cpu_arm940_icache_invalidate_page

	/* tlb */
	.word	cpu_arm940_tlb_invalidate_all
	.word	cpu_arm940_tlb_invalidate_range
	.word	cpu_arm940_tlb_invalidate_page

	/* pgtable */
	.word	cpu_arm940_set_pgd
	.word	cpu_arm940_set_pmd
	.word	cpu_arm940_set_pte
	.size	arm940_processor_functions, . - arm940_processor_functions

	.type	cpu_arm940_info, #object
cpu_arm940_info:
	.long	cpu_manu_name
	.long	cpu_arm940_name
	.size	cpu_arm940_info, . - cpu_arm940_info

	.type	cpu_arch_name, #object
cpu_arch_name:
	.asciz	"armv4"
	.size	cpu_arch_name, . - cpu_arch_name

	.type	cpu_elf_name, #object
cpu_elf_name:
	.asciz	"v4"
	.size	cpu_elf_name, . - cpu_elf_name
	.align

	.section ".proc.info", #alloc, #execinstr

	.type	__arm940_proc_info,#object
__arm940_proc_info:
	.long	0x41009400
	.long	0xff00fff0
	.long	0x00000c1e			@ mmuflags
	b	__arm940_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP | HWCAP_HALF | HWCAP_26BIT
	.long	cpu_arm940_info
	.long	arm940_processor_functions
	.size	__arm940_proc_info, . - __arm940_proc_info
