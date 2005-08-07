/*
 * cop.S - coprocessor code
 *
 * Copyright (c) 2003-2005, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <asm/arch/hardware.h>

	.global cop_fiq
	.global cop_idle

cop_fiq:
	mov	r8, #PP_CPU_TYPE
	ldr	r8, [r8]
	cmp	r8, #0

	ldreq	r8, =PP5002_PROC_ID
	ldrne	r8, =PP5020_PROC_ID
	ldr	r8, [r8]
	and	r8, r8, #0xff
	cmp	r8, #0x55
	beq	1f

	stmdb   sp!, {r0-r7, lr}

	bl	ipod_handle_dma

	ldmia   sp!, {r0-r7, lr}
1:
	subs	pc, lr, #4


cop_idle:
	mov	r0, #0x0
	bl	ipod_set_process_dma

	mov	r0, #0x0
	bl	ipod_set_handle_cop

	/* enable FIQ */
	mrs     r0, cpsr
	bic     r0, r0, #0x40
	msr     cpsr_c, r0

cop_sleep:
	mov	r0, #PP_CPU_TYPE
	ldr	r0, [r0]
	cmp	r0, #0
	bne	pp5020

	/* PP5002 */
	/* put us (co-processor) to sleep */
	ldr	r1, =PP5002_COP_CTRL
	mov	r0, #0xca	
	strh	r0, [r1]
	b	1f

pp5020:
	/* PP5020 */
	/* put us (co-processor) to sleep */
	ldr	r1, =PP5020_COP_CTRL
	mov	r0, #0x80000000	
	str	r0, [r1]

1:
	nop
	nop


	ldr	r1, =COP_STATE
	mov	r0, #0x1
	str	r0, [r1]
	
	bl	ipod_handle_cop


	ldr	r1, =COP_STATE
	mov 	r0, #0x0
	str 	r0, [r1]


	b	cop_sleep

	.global cop_fiq_stack
	.global cop_svc_stack

	.lcomm cop_fiq_stack, 1024
	.lcomm cop_svc_stack, 1024

