/*
 * cop.S - coprocessor code
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

	.global cop_fiq
	.global cop_end
	.global cop_idle

	.equ C_PROCESSOR_ID, 0xc4000000
	.equ C_CPU_ID, 0x55

	.equ C_CFG_COPROC, 0xcf004058
	.equ C_CPU_SLEEP, 0xca

cop_fiq:
	stmdb   sp!, {r0, lr}

.ifdef NOTDEF
	ldr	r0, L_lcd_base
	ldr	r1, [r0]
	orr	r1, r1, #0x2
	str	r1, [r0]
.endif
	ldr	r0, L_C_PROCESSOR_ID
	ldr	r0, [r0]
	and	r0, r0, #0xff
	cmp	r0, #C_CPU_ID
	beq	1f

	mov	lr, pc
	ldr	pc, L_ipod_handle_dma

	nop
	nop
	nop

1:
	ldmia   sp!, {r0, lr}
	subs	pc, lr, #4

L_C_PROCESSOR_ID:	.word C_PROCESSOR_ID

cop_fiq_end:

cop_idle:
	/* enable FIQ */
	mrs     r0, cpsr
	bic     r0, r0, #0x40
	msr     cpsr_c, r0

cop_sleep:
	/* put us (co-processor) to sleep */
	ldr	r4, L_CFG_COPROC
	mov	r3, #C_CPU_SLEEP
	strh	r3, [r4]

	nop
	nop

	b	cop_sleep

	.global cop_fiq_stack
	.global cop_svc_stack

	.lcomm cop_fiq_stack, 1024
	.lcomm cop_svc_stack, 1024

L_CFG_COPROC:	.word C_CFG_COPROC

L_ipod_handle_dma:.word ipod_handle_dma

L_lcd_base:	.word 0xc0001000

/* these must match those in audio.c */
L_DMA_READ_OFF:	.word 0x40000000
L_DMA_WRITE_OFF:.word 0x40000004
L_DMA_ACTIVE:	.word 0x40000008
L_DMA_BASE:	.word 0x4000000c

cop_end:

