/*
 *  linux/arch/arm/kernel/head-armv.S
 *
 *  Copyright (C) 1994-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  32-bit kernel startup code for all architectures
 *
 * added S3C4510 code by Mac Wang
 * added S3C4530 code by Arcturus Networks Inc.
 * added S3C2500 code by Arcturus Networks Inc.
 *
 */
#include <linux/config.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#define K(a,b,c)	((a) << 24 | (b) << 12 | (c))

/*
 * We place the page tables 16K below TEXTADDR.  Therefore, we must make sure
 * that TEXTADDR is correctly set.  Currently, we expect the least significant
 * "short" to be 0x8000, but we could probably relax this restriction to
 * TEXTADDR > PAGE_OFFSET + 0x4000
 *
 * Note that swapper_pg_dir is the virtual address of the page tables, and
 * pgtbl gives us a position-independent reference to these tables.  We can
 * do this because stext == TEXTADDR
 *
 * swapper_pg_dir, pgtbl and krnladr are all closely related.
 */

#ifndef CONFIG_UCLINUX
		.globl	SYMBOL_NAME(swapper_pg_dir)
		.equ	SYMBOL_NAME(swapper_pg_dir), TEXTADDR - 0x4000
		.macro	pgtbl, reg, rambase
		adr	\reg, stext
		sub	\reg, \reg, #0x4000
		.endm

/*
 * Since the page table is closely related to the kernel start address, we
 * can convert the page table base address to the base address of the section
 * containing both.
 */
		.macro	krnladr, rd, pgtable, rambase
		bic	\rd, \pgtable, #0x000ff000
		.endm
#endif

/*
 *  Kernel startup entry point.
 *
 * The rules are:
 *  r0      - should be 0
 *  r1      - unique architecture number
 *  MMU     - off
 *  I-cache - on or off
 *  D-cache - off
 *
 * See linux/arch/arm/tools/mach-types for the complete list of numbers
 * for r1.
 */


.section ".text.init",#alloc,#execinstr
		.type	stext, #function
ENTRY(stext)
#if defined(CONFIG_CPU_S3C44B0X)
/*
 * We assume that there is a bootloader that has jump instructions at all
 * exception vectors that lead to their position shifted to the start of
 * RAM. Further on we assume that the following code gets linked to the
 * start of RAM, so that all jumps match again.
 */
		b   	99f			/* 0x00 reset vector */
		b	vector_undefinstr	/* 0x04 undefined instruction  */
		b	vector_swi		/* 0x08 software interrupt */
		b	vector_prefetch		/* 0x0C prefetch abort */
		b	vector_data		/* 0x10 data abort */
		b	vector_addrexcptn	/* 0x14 address exception */
		b	vector_IRQ		/* 0x18 IRQ */
		b	vector_FIQ		/* 0x1C FIQ */
99:
#endif
                mov	r12, r0
/*
 * NOTE!  Any code which is placed here should be done for one of
 * the following reasons:
 *
 *  1. Compatability with old production boot firmware (ie, users
 *     actually have and are booting the kernel with the old firmware)
 *     and therefore will be eventually removed.
 *  2. Cover the case when there is no boot firmware.  This is not
 *     ideal, but in this case, it should ONLY set r0 and r1 to the
 *     appropriate value.
 */
#if defined(CONFIG_ARCH_NETWINDER)
/*
 * Compatability cruft for old NetWinder NeTTroms.  This
 * code is currently scheduled for destruction in 2.5.xx
 */
		.rept	8
		mov	r0, r0
		.endr

		adr	r2, 1f
		ldmdb	r2, {r7, r8}
		and	r3, r2, #0xc000
		teq	r3, #0x8000
		beq	__entry
		bic	r3, r2, #0xc000
		orr	r3, r3, #0x8000
		mov	r0, r3
		mov	r4, #64
		sub	r5, r8, r7
		b	1f

		.word	_stext
		.word	__bss_start

1:
		.rept	4
		ldmia	r2!, {r6, r7, r8, r9}
		stmia	r3!, {r6, r7, r8, r9}
		.endr
		subs	r4, r4, #64
		bcs	1b
		movs	r4, r5
		mov	r5, #0
		movne	pc, r0

		mov	r1, #MACH_TYPE_NETWINDER	@ (will go in 2.5)
		mov	r12, #2 << 24			@ scheduled for removal in 2.5.xx
		orr	r12, r12, #5 << 12
__entry:
#endif
#if defined(CONFIG_ARCH_L7200)
/*
 * FIXME - No bootloader, so manually set 'r1' with our architecture number.
 */
		mov	r1, #MACH_TYPE_L7200
#elif defined(CONFIG_ARCH_INTEGRATOR)
		mov	r1, #MACH_TYPE_INTEGRATOR
#elif defined(CONFIG_ARCH_P52)
		mov	r1, #MACH_TYPE_P52
#elif defined(CONFIG_ARCH_CX821XX)
		mov	r1, #MACH_TYPE_CX821XX
#elif defined(CONFIG_ARCH_SWARM)
		mov	r1, #MACH_TYPE_SWARM
#elif defined(CONFIG_BOARD_SNDS100)
		mov	r1, #MACH_TYPE_SNDS100
#elif defined (CONFIG_BOARD_SMDK40100)
		mov	r1, #MACH_TYPE_S3C3410
#elif defined (CONFIG_BOARD_MBA44)
		mov	r1, #MACH_TYPE_S3C44B0X
#elif defined(CONFIG_BOARD_SMDK2500)
		mov     r1, #MACH_TYPE_SMDK2500
#elif defined(CONFIG_BOARD_S3C2500REFRGP)
		mov	r1, #MACH_TYPE_S3C2500REFRGP
#endif

		mov	r0, #F_BIT | I_BIT | MODE_SVC	@ make sure svc mode
		msr	cpsr_c, r0			@ and all irqs disabled

#if defined(CONFIG_ARCH_ATMEL)

        adr	r5, LC0
        ldmia	r5, {r5, r6, r8, r9, sp}			@ Setup stack

        /*  Copy data sections to their new home.  */


        /*  Clear BSS */
        mov	r4, #0
1:      cmp	r5, r8					
        strcc	r4, [r5],#4
        bcc	1b

/* FIXME */
#if 0
        /*  Put initial values into stack.  This would normally be done
        by sched_init() in kernel/sched.c, but that would overwrite the
        stack we're already using.  That would be bad.
        */
        mov r5, sp
        sub r5, r5, #0x2000
        ldr r4, L_STACK_MAGIC
        str r4, [r5], #4
        ldr r4, L_STACK_UNTOUCHED_MAGIC
1:      cmp r5, sp
        strcc r4, [r5], #4
        bcc 1b
#endif
        /*  Pretend we know what our processor code is (for arm_id)   */

@@@	ldr     r2, =0x41000000
@@@	orr	r2, r2, #0x7000		@ FIXME --> 0x41007000

	ldr	r2, L_AT91_SF_CIDR
	ldr	r2, [r2]		@ read processor id

	str     r2, [r6]
	mov     r2, #MACH_TYPE_ATMEL
	str     r2, [r9]

        mov fp, #0
        b	start_kernel
        
LC0:	.long	__bss_start
        .long	processor_id
        .long	_end
	.long   __machine_arch_type
        .long	init_task_union+8192
#endif

#if defined(CONFIG_BOARD_SNDS100)

        adr	r5, LC0
        ldmia	r5, {r5, r6, r8, r9, sp}			@ Setup stack

        /*  Copy data sections to their new home.  */


        /*  Clear BSS */
        mov	r4, #0
1:      cmp	r5, r8					
        strcc	r4, [r5],#4
        bcc	1b

        /*  Pretend we know what our processor code is (for arm_id)   */

	ldr	r2, S3C4510B_PROCESSOR_TYPE

	str     r2, [r6]
	mov     r2, #MACH_TYPE_SNDS100
	str     r2, [r9]

        mov fp, #0
        b	start_kernel
        
LC0:	.long	__bss_start
        .long	processor_id
        .long	_end
	.long   __machine_arch_type
        .long	init_task_union+8192
	
S3C4510B_PROCESSOR_TYPE:
	.long	0x36365036
#endif

#if defined(CONFIG_ARCH_TA7S) || defined(CONFIG_ARCH_TA7V)
	b	a7Start

	.globl a7Start
	.org 0x1000
a7Start:
	/*
	 * Setup a temp. SP and then call the SDRAM init function.
	 */
	ldr     sp, L_INIT_SP
	bl	a7hal_sdram_init

	/*
	 * If the kernel needs to run from SDRAM
	 * we copy it from FLASH to SDRAM here.
	 */
#ifdef USE_A7HAL
	mov	r0,#0x00000000
	mov	r1,#0xc0000000
	ldr	r2, LC0
	mov	r3,#0
	BL	a7hal_dma_pollTransfer
#else
	mov	r0,#0x00000000
	mov	r1,#0xc0000000
	ldr	r2, LC0
copy_loop:
	ldr	r3,[r0],#4
	str	r3,[r1],#4
	cmp	r0,r2
	blt	copy_loop
#endif
	/*
	 * Jump up to the physical execution address so we can
	 * change the alias.
	 */
	ldr	pc, L_HIGH_JUMP

_jump_to_high:
	/*
	 * Map the SDRAM to address zero
	 */
	ldr	r4, L_REMAP_ALIAS_REG
	mov	r2, #0x08
	str	r2,[r4,#0]

 	ldr	pc, L_LOW_JUMP
__zentry:

        adr	r5, LC0
        ldmia	r5, {r5, r6, r8, r9, sp}			@ Setup stack

        /*  Clear BSS */
        mov	r4, #0
1:      cmp	r5, r8					
        strcc	r4, [r5],#4
        bcc	1b


	ldr	r2, L_REMAP_IDENT_REG
	ldr	r2, [r2]
	str	r2, [r6]
	ldr     r2, mach_type_ta7
	ldr	r2, [r2]
	/*
	 * r1 should hold the processor type when start_kernel
	 * is called.
	 */
	mov	r1,r2
	str     r2, [r9]


	mov	fp, #0
	b	start_kernel

mach_type_ta7:
#if defined(CONFIG_ARCH_TA7S)
	.long	MACH_TYPE_TA7S
#elif defined(CONFIG_ARCH_TA7V)
	.long	MACH_TYPE_TA7V
#endif

L_REMAP_IDENT_REG:
	.long	0xd1010410
L_INIT_SP:
	.long	0xd1034000
L_HIGH_JUMP:
	.long	(_jump_to_high + 0xc0000000)
L_LOW_JUMP:
	.long	__zentry
L_REMAP_ALIAS_REG:
	.long	0xd1010440
L_END_KERNEL:
	.long	_end_kernel

LC0:	.long	__bss_start
       	.long	processor_id
       	.long	_end
	.long   __machine_arch_type
       	.long	init_task_union+8192

#endif /* CONFIG_ARCH_TA7S || CONFIG_ARCH_TA7V */

#if defined(CONFIG_CPU_S3C3410) || defined(CONFIG_CPU_S3C4530) || defined(CONFIG_CPU_S3C44B0X)

                adr     r2, LC0

#if     defined(CONFIG_ROMKERNEL)
                @  Copy data sections to their new home
                ldmia   r2, {r4-r11,sp}
                cmp     r9, r11
                beq     2f
1:              cmp     r11, r10
                ldrcc   r3, [r9], #4
                strcc   r3, [r11], #4
                bcc     1b
#else
                ldmia   r2, {r4-r8,sp}
#endif

		@ clear BSS
2:              mov     r3, #0
3:              cmp     r7, r8
                strcc   r3, [r7], #4
                bcc     3b

#if     defined(CONFIG_CPU_S3C3410)
		ldr	r3, S3C3410_PROCESSOR_TYPE
		str	r3, [r5]
		
		mov	r3, #MACH_TYPE_S3C3410
		str	r3, [r6]
#endif
#if     defined(CONFIG_CPU_S3C44B0X)
		ldr	r3, S3C44B0X_PROCESSOR_TYPE
		str	r3, [r5]
		
		mov	r3, #MACH_TYPE_S3C44B0X
		str	r3, [r6]
#endif
#if     defined(CONFIG_CPU_S3C4530)
                ldr     r3, [r4]                @ read system configuration register
                bic     r3, r3, #0x83FFFFFF     @ get product identifier
                str     r3, [r5]                @ store it
#endif
#if     defined(CONFIG_BOARD_EVS3C4530HEI)
                mov     r3, #MACH_TYPE_EVS3C4530HEI
                str     r3, [r6]
#endif
                mov     fp, #0
                b       start_kernel

#if defined(CONFIG_CPU_S3C3410)
S3C3410_PROCESSOR_TYPE:
		.long	0x34103410
#endif
#if defined(CONFIG_CPU_S3C44B0X)
S3C44B0X_PROCESSOR_TYPE:
		.long	0x44B044B0
#endif
		
LC0:
#if             defined(CONFIG_CPU_S3C4530)
                .long   0x3ff0000               @ r4
#endif
#if             defined(CONFIG_CPU_S3C3410) || defined (CONFIG_CPU_S3C44B0X)
                .long   0x07ff0000              @ r4
#endif
                .long   processor_id            @ r5
                .long   __machine_arch_type     @ r6
                .long   __bss_start             @ r7   bss start
                .long   _end                    @ r8   bss end

#if     defined (CONFIG_ROMKERNEL)
#if     defined (CONFIG_BOARD_SMDK40100) || defined (CONFIG_BOARD_MBA44)
                .long   _etext                  @ r9   data start in ROM (src)
#else
                .long   _end_romfs              @ r9   data start in ROM (src)
#endif
                .long   _edata                  @ r10  data end in RAM
                .long   __data_start            @ r11  data start in RAM (dst)
#endif  /* CONFIG_ROMKERNEL */
                .long   init_task_union+8192    @ sp
#endif  /* CONFIG_ARCH_SAMSUNG */

#if	defined(CONFIG_BOARD_S3C2500REFRGP) || \
	defined(CONFIG_BOARD_SMDK2500)
		bl	get_system_clock
#endif		

		bl	__lookup_processor_type
		teq	r10, #0				@ invalid processor?
		moveq	r0, #'p'			@ yes, error 'p'
		beq	__error
		bl	__lookup_architecture_type
#if !defined(CONFIG_ARCTURUS)
		teq	r7, #0				@ invalid architecture?
		moveq	r0, #'a'			@ yes, error 'a'
		beq	__error
#endif
#ifndef CONFIG_UCLINUX
		bl	__create_page_tables
#endif
		adr	lr, __ret			@ return address
		add	pc, r10, #12			@ initialise processor
							@ (return control reg)

__switch_data:	.long	__mmap_switched
		.long	SYMBOL_NAME(compat)
		.long	SYMBOL_NAME(__bss_start)
		.long	SYMBOL_NAME(_end)
		.long	SYMBOL_NAME(processor_id)
		.long	SYMBOL_NAME(__machine_arch_type)
		.long	SYMBOL_NAME(cr_alignment)
		.long	SYMBOL_NAME(init_task_union)+8192

__ret:		ldr	lr, __switch_data
#ifdef CONFIG_CPU_WITH_CACHE
# ifdef CONFIG_CPU_WITH_MCR_INSTRUCTION
		mcr	p15, 0, r0, c1, c0
		mov	r0, r0
		mov	r0, r0
		mov	r0, r0
# else
#  warning 	"FIXME: Enable Cache, Other settings without MCR Instruction"
		@ Note r0 is initialized suitably to enable Cache
		@ and additional settings if any in proc_armX.S file
		@ A possible code here
		@ldr	r2, CACHE_CONTROL_MEM_LOCATION
		@str	r0, [r2]
# endif
#endif
		mov	pc, lr

		/*
		 * This code follows on after the page
		 * table switch and jump above.
		 *
		 * r0  = processor control register
		 * r1  = machine ID
		 * r9  = processor ID
		 */
		.align	5
__mmap_switched:
		adr	r3, __switch_data + 4
		ldmia	r3, {r2, r4, r5, r6, r7, r8, sp}@ r2 = compat
							@ sp = stack pointer
		str	r12, [r2]

		mov	fp, #0				@ Clear BSS (and zero fp)
1:		cmp	r4, r5
		strcc	fp, [r4],#4
		bcc	1b

		str	r9, [r6]			@ Save processor ID
		str	r1, [r7]			@ Save machine type
#ifdef CONFIG_ALIGNMENT_TRAP
		orr	r0, r0, #2			@ ...........A.
#endif
		bic	r2, r0, #2			@ Clear 'A' bit
		stmia	r8, {r0, r2}			@ Save control register values
		b	SYMBOL_NAME(start_kernel)


#ifndef CONFIG_UCLINUX
/*
 * Setup the initial page tables.  We only setup the barest
 * amount which are required to get the kernel running, which
 * generally means mapping in the kernel code.
 *
 * We only map in 4MB of RAM, which should be sufficient in
 * all cases.
 *
 * r5 = physical address of start of RAM
 * r6 = physical IO address
 * r7 = byte offset into page tables for IO
 * r8 = page table flags
 */
__create_page_tables:
		pgtbl	r4, r5				@ page table address

		/*
		 * Clear the 16K level 1 swapper page table
		 */
		mov	r0, r4
		mov	r3, #0
		add	r2, r0, #0x4000
1:		str	r3, [r0], #4
		str	r3, [r0], #4
		str	r3, [r0], #4
		str	r3, [r0], #4
		teq	r0, r2
		bne	1b

		/*
		 * Create identity mapping for first MB of kernel to
		 * cater for the MMU enable.  This identity mapping
		 * will be removed by paging_init()
		 */
		krnladr	r2, r4, r5			@ start of kernel
		add	r3, r8, r2			@ flags + kernel base
		str	r3, [r4, r2, lsr #18]		@ identity mapping

		/*
		 * Now setup the pagetables for our kernel direct
		 * mapped region.  We round TEXTADDR down to the
		 * nearest megabyte boundary.
		 */
		add	r0, r4, #(TEXTADDR & 0xff000000) >> 18 @ start of kernel
		add	r0, r0, #(TEXTADDR & 0x00f00000) >> 18
		str	r3, [r0], #4			@ PAGE_OFFSET + 0MB
		add	r3, r3, #1 << 20
		str	r3, [r0], #4			@ PAGE_OFFSET + 1MB
		add	r3, r3, #1 << 20
		str	r3, [r0], #4			@ PAGE_OFFSET + 2MB
		add	r3, r3, #1 << 20
		str	r3, [r0], #4			@ PAGE_OFFSET + 3MB

		/*
		 * Ensure that the first section of RAM is present.
		 * we assume that:
		 *  1. the RAM is aligned to a 256MB boundary
		 *  2. the kernel is executing in the same 256MB chunk
		 *     as the start of RAM.
		 */
		bic	r0, r0, #0x0ff00000 >> 18	@ round down
		and	r2, r5, #0xf0000000		@ round down
		add	r3, r8, r2			@ flags + rambase
		str	r3, [r0]

		bic	r8, r8, #0x0c			@ turn off cacheable
							@ and bufferable bits
#ifdef CONFIG_DEBUG_LL
		/*
		 * Map in IO space for serial debugging.
		 * This allows debug messages to be output
		 * via a serial console before paging_init.
		 */
		add	r0, r4, r7
		rsb	r3, r7, #0x4000	@ PTRS_PER_PGD*sizeof(long)
		cmp	r3, #0x0800
		addge	r2, r0, #0x0800
		addlt	r2, r0, r3
		orr	r3, r6, r8
1:		str	r3, [r0], #4
		add	r3, r3, #1 << 20
		teq	r0, r2
		bne	1b
#ifdef CONFIG_ARCH_NETWINDER
		/*
		 * If we're using the NetWinder, we need to map in
		 * the 16550-type serial port for the debug messages
		 */
		teq	r1, #MACH_TYPE_NETWINDER
		bne	1f
		add	r0, r4, #0x3fc0
		mov	r3, #0x7c000000
		orr	r3, r3, r8
		str	r3, [r0], #4
		add	r3, r3, #1 << 20
		str	r3, [r0], #4
1:
#endif
#endif
#ifdef CONFIG_ARCH_RPC
		/*
		 * Map in screen at 0x02000000 & SCREEN2_BASE
		 * Similar reasons here - for debug.  This is
		 * only for Acorn RiscPC architectures.
		 */
		add	r0, r4, #0x80			@ 02000000
		mov	r3, #0x02000000
		orr	r3, r3, r8
		str	r3, [r0]
		add	r0, r4, #0x3600			@ d8000000
		str	r3, [r0]
#endif
		mov	pc, lr
#endif


/*
 * Exception handling.  Something went wrong and we can't
 * proceed.  We ought to tell the user, but since we
 * don't have any guarantee that we're even running on
 * the right architecture, we do virtually nothing.
 * r0 = ascii error character:
 *	a = invalid architecture
 *	p = invalid processor
 *	i = invalid calling convention
 *
 * Generally, only serious errors cause this.
 */
__error:
#ifdef CONFIG_DEBUG_LL
		mov	r8, r0				@ preserve r0
		adr	r0, err_str
		bl	printascii
		mov	r0, r8
		bl	printch
#endif
#ifdef CONFIG_ARCH_RPC
/*
 * Turn the screen red on a error - RiscPC only.
 */
		mov	r0, #0x02000000
		mov	r3, #0x11
		orr	r3, r3, r3, lsl #8
		orr	r3, r3, r3, lsl #16
		str	r3, [r0], #4
		str	r3, [r0], #4
		str	r3, [r0], #4
		str	r3, [r0], #4
#endif
1:		mov	r0, r0
		b	1b

#ifdef CONFIG_DEBUG_LL
err_str:	.asciz	"\nError: "
		.align
#endif

/*
 * Read processor ID register (CP#15, CR0), and look up in the linker-built
 * supported processor list.  Note that we can't use the absolute addresses
 * for the __proc_info lists since we aren't running with the MMU on
 * (and therefore, we are not in the correct address space).  We have to
 * calculate the offset.
 *
 * Returns:
 *	r5, r6, r7 corrupted
 *	r8  = page table flags
 *	r9  = processor ID
 *	r10 = pointer to processor structure
 */
__lookup_processor_type:
		adr	r5, 2f
		ldmia	r5, {r7, r9, r10}
		sub	r5, r5, r10			@ convert addresses
		add	r7, r7, r5			@ to our address space
		add	r10, r9, r5
#ifdef CONFIG_CPU_WITH_MCR_INSTRUCTION
		mrc	p15, 0, r9, c0, c0		@ get processor id
#else
# warning 	"FIXME: Get Processor ID without MCR Instruction"
		@ A possible code
		@ldr	r9, PROCESSOR_ID_MEM_LOCATION
		@ldr	r9, [r9]
#endif
1:		ldmia	r10, {r5, r6, r8}		@ value, mask, mmuflags
		and	r6, r6, r9			@ mask wanted bits
		teq	r5, r6
		moveq	pc, lr
		add	r10, r10, #36			@ sizeof(proc_info_list)
		cmp	r10, r7
		blt	1b
		mov	r10, #0				@ unknown processor
		mov	pc, lr

/*
 * Look in include/asm-arm/procinfo.h and arch/arm/kernel/arch.[ch] for
 * more information about the __proc_info and __arch_info structures.
 */
2:		.long	__proc_info_end
		.long	__proc_info_begin
		.long	2b
		.long	__arch_info_begin
		.long	__arch_info_end

/*
 * Lookup machine architecture in the linker-build list of architectures.
 * Note that we can't use the absolute addresses for the __arch_info
 * lists since we aren't running with the MMU on (and therefore, we are
 * not in the correct address space).  We have to calculate the offset.
 *
 *  r1 = machine architecture number
 * Returns:
 *  r2, r3, r4 corrupted
 *  r5 = physical start address of RAM
 *  r6 = physical address of IO
 *  r7 = byte offset into page tables for IO
 */
__lookup_architecture_type:
		adr	r4, 2b
		ldmia	r4, {r2, r3, r5, r6, r7}	@ throw away r2, r3
		sub	r5, r4, r5			@ convert addresses
		add	r4, r6, r5			@ to our address space
		add	r7, r7, r5
1:		ldr	r5, [r4]			@ get machine type
		teq	r5, r1
		beq	2f
		add	r4, r4, #SIZEOF_MACHINE_DESC
		cmp	r4, r7
		blt	1b
		mov	r7, #0				@ unknown architecture
		mov	pc, lr
2:		ldmib	r4, {r5, r6, r7}		@ found, get results
		mov	r7, r7, lsr #18			@ pagetable byte offset
		mov	pc, lr

L_AT91_SF_CIDR: .long	0xfff00000

#if defined(CONFIG_ARCH_TA7S) || defined(CONFIG_ARCH_TA7V)
#ifdef	CONFIG_BLK_DEV_RAMDISK_DATA
.section ".rodata"
	.align	12
	.globl	__ramdisk_data
__ramdisk_data:
#include "ramdisk.inc"
	.globl	__ramdisk_data_end
__ramdisk_data_end:
	.align	
#endif
#endif

