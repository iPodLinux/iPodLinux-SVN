
#include <linux/config.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/arch/hardware.h>

/*
 * Reset condition :
 *      R14_svc         = unknown
 *      SPSR_svc        = CPSR
 *      CPSR            = 0b1101_0011 ; not IRQ, no FIQ, SVC mode
 *      PC              = 0x0
 */
	.section ".text.init",#alloc,#execinstr
	.type   stext, #function
ENTRY(stext)
	mov     r0, #0xd3
	msr     CPSR_c, r0

	/* setup memory mapping */
	ldr	r1, =0x40010000
	ldr	r2, =install_reloc_start
	ldr	r3, =install_reloc_end

	/*
	 * this code is compiled for 0x0 but running at 0x28000000
	 * or 0x10000000 so we need to adjust
	 */
	and	r5, pc, #0xff000000	/* adjust for execute address */
	orr	r2, r2, r5
	orr	r3, r3, r5

	/* copy the code to 0x40010000 */
1:
	ldr	r4, [r2], #4
	str	r4, [r1], #4
	cmp	r2, r3
	ble	1b

	ldr	r3, =0x3f84
	orr	r3, r3, r5		/* adjust for execute address */
	ldr	r2, =0xf000f014
	mov	r1, #0x3a00
	ldr	r0, =0xf000f010
	ldr	r4, =0x40010000
	mov	pc, r4

install_reloc_start:
	str	r1, [r0]
	str	r3, [r2]
	ldr	r0, L_post_reloc_start
	mov	pc, r0

L_post_reloc_start:	.word post_reloc_start

install_reloc_end:

post_reloc_start:
	cmp	r5, #0x28000000
	mov	r0, #PP_CPU_TYPE
	moveq	r1, #0x0	/* we are a PP5002 */
	movne	r1, #0x1	/* we are a PP5020 */
	str	r1, [r0]
	bne	pp5020

pp5002:
	/* check if we are cpu or co-processor */
	ldr	r0, =PP5002_PROC_ID
	ldr	r0, [r0]
	and	r0, r0, #0xff
	cmp	r0, #0x55
	beq	cpu_startup

	/* we are the co-processor, go to sleep until cpu wakes us up */
	ldr	r0, =PP5002_COP_CTRL
	mov	r1, #0xca
	str	r1, [r0]
	b	cop_startup

pp5020:
	/* check if we are cpu or co-processor */
	ldr	r0, =PP5020_PROC_ID
	ldr	r0, [r0]
	and	r0, r0, #0xff
	cmp	r0, #0x55
	beq	cpu_startup

	/* we are the co-processor, go to sleep until cpu wakes us up */
	ldr	r0, =PP5020_COP_CTRL
	mov	r1, #0x80000000
	str	r1, [r0]
	b	cop_startup

cop_startup:
	/* set stack pointer for FIQ mode */
	mov	r0, #0xd1
	msr	CPSR_c, r0
	ldr	sp, fiq_sp

	/* set stack pointer for SVC mode */
	mov	r0, #0xd3
	msr	CPSR_c, r0
	ldr	sp, svc_sp

	bl	ipod_init_cache

	/* jump to our idle loop */
	b	cop_idle

svc_sp:		.word cop_svc_stack
fiq_sp:		.word cop_fiq_stack

	.ltorg	/* dump literal pool to avoid section problems */

cpu_startup:
	/* get important "magic" values */
	adr	r5, LC0
	ldmia	r5, {r5, r6, r8, r9, sp}

	/* initialise bss */
	mov	r4, #0
1:
	cmp	r5, r8
	strcc	r4, [r5], #4
	bcc	1b

	bl	ipod_set_sys_info
	bl	ipod_init_cache
	bl	ipod_set_cpu_speed

	/* put initial values into stack?? */
	mov	r5, sp
	sub	r5, r5, #0x1000
	ldr	r4, L_STACK_MAGIC
	str	r4, [r5], #4
	ldr	r4, L_STACK_UNTOUCHED_MAGIC
1:
	cmp	r5, sp
	strcc	r4, [r5], #4
	bcc	1b

	/* hard code the ARMID */
	ldr	r2, L_ARMID
	str	r2, [r6]

	/* hard code the processor code */
	mov	r2, #MACH_TYPE_IPOD
	str	r2, [r9]

	/* jump to the kernel */
	mov	fp, #0x0
	b	SYMBOL_NAME(start_kernel)

/*  These values should stay in sync with linux/kernel.h */
L_STACK_MAGIC:
		.long 0xdeadbeef
L_STACK_UNTOUCHED_MAGIC:
		.long 0xfeef1ef0

L_ARMID:	.long 0x41007700


	.align 8
LC0:	.long	SYMBOL_NAME(__bss_start)
	.long	SYMBOL_NAME(processor_id)
	.long	SYMBOL_NAME(_end)
	.long	SYMBOL_NAME(__machine_arch_type)
	.long	SYMBOL_NAME(init_task_union)+8192

