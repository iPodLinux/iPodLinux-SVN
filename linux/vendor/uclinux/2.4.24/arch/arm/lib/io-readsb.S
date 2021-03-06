/*
 *  linux/arch/arm/lib/io-readsb.S
 *
 *  Copyright (C) 1995-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>

.insb_align:	rsb	ip, ip, #4
		cmp	ip, r2
		movgt	ip, r2
		cmp	ip, #2
		ldrb	r3, [r0]
		strb	r3, [r1], #1
		ldrgeb	r3, [r0]
		strgeb	r3, [r1], #1
		ldrgtb	r3, [r0]
		strgtb	r3, [r1], #1
		subs	r2, r2, ip
		bne	.insb_aligned

ENTRY(__raw_readsb)
		teq	r2, #0		@ do we have to check for the zero len?
		moveq	pc, lr
		ands	ip, r1, #3
		bne	.insb_align

.insb_aligned:	stmfd	sp!, {r4, r5, lr}

		subs	r2, r2, #16
		bmi	.insb_no_16

.insb_16_lp:	ldrb	r3, [r0]
		mov	r3, r3, lsl #byte(0)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(1)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(2)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(3)
		ldrb	r4, [r0]
		mov	r4, r4, lsl #byte(0)
		ldrb	r5, [r0]
		orr	r4, r4, r5, lsl #byte(1)
		ldrb	r5, [r0]
		orr	r4, r4, r5, lsl #byte(2)
		ldrb	r5, [r0]
		orr	r4, r4, r5, lsl #byte(3)
		ldrb	r5, [r0]
		mov	r5, r5, lsl #byte(0)
		ldrb	ip, [r0]
		orr	r5, r5, ip, lsl #byte(1)
		ldrb	ip, [r0]
		orr	r5, r5, ip, lsl #byte(2)
		ldrb	ip, [r0]
		orr	r5, r5, ip, lsl #byte(3)
		ldrb	ip, [r0]
		mov	ip, ip, lsl #byte(0)
		ldrb	lr, [r0]
		orr	ip, ip, lr, lsl #byte(1)
		ldrb	lr, [r0]
		orr	ip, ip, lr, lsl #byte(2)
		ldrb	lr, [r0]
		orr	ip, ip, lr, lsl #byte(3)
		stmia	r1!, {r3 - r5, ip}

		subs	r2, r2, #16
		bpl	.insb_16_lp

		tst	r2, #15
		LOADREGS(eqfd, sp!, {r4, r5, pc})

.insb_no_16:	tst	r2, #8
		beq	.insb_no_8

		ldrb	r3, [r0]
		mov	r3, r3, lsl #byte(0)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(1)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(2)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(3)
		ldrb	r4, [r0]
		mov	r4, r4, lsl #byte(0)
		ldrb	r5, [r0]
		orr	r4, r4, r5, lsl #byte(1)
		ldrb	r5, [r0]
		orr	r4, r4, r5, lsl #byte(2)
		ldrb	r5, [r0]
		orr	r4, r4, r5, lsl #byte(3)
		stmia	r1!, {r3, r4}

.insb_no_8:	tst	r2, #4
		beq	.insb_no_4

		ldrb	r3, [r0]
		mov	r3, r3, lsl #byte(0)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(1)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(2)
		ldrb	r4, [r0]
		orr	r3, r3, r4, lsl #byte(3)
		str	r3, [r1], #4

.insb_no_4:	ands	r2, r2, #3
		LOADREGS(eqfd, sp!, {r4, r5, pc})

		cmp	r2, #2
		ldrb	r3, [r0]
		strb	r3, [r1], #1
		ldrgeb	r3, [r0]
		strgeb	r3, [r1], #1
		ldrgtb	r3, [r0]
		strgtb	r3, [r1]

		LOADREGS(fd, sp!, {r4, r5, pc})
