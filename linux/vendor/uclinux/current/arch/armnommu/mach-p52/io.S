/*
 *  linux/arch/arm/mach-ebsa110/io.S
 *
 *  Copyright (C) 1995 - 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
		.text

/*
 * These make no sense on these machines.  Print a warning message.
 */
ENTRY(insl)
		adr	r1, .insl_name
		b	.warn

ENTRY(outsl)
		adr	r1, .outsl_name
.warn:		adr	r0, .io_warning
		mov	r2, lr
		b	SYMBOL_NAME(printk)

.insl_name:	.asciz	"insl"
.insw_name:	.asciz	"insw"
.outsl_name:	.asciz	"outsl"
.outsw_name:	.asciz	"outsw"
.io_warning:	.asciz	"<4>%s: not implemented on this architecture (lr = 0x%p)"
		.align

@ These are mainly to satisfy the parport code

ENTRY(insb)
		cmp	r0, #0x00c00000
		movge	r3, #0
		movlt	r3, #0xf0000000
		add	r0, r3, r0, lsl #2
		b	__arch_readsb

ENTRY(outsb)
		cmp	r0, #0x00c00000
		movge	r3, #0
		movlt	r3, #0xf0000000
		add	r0, r3, r0, lsl #2
		b	__arch_writesb

@ And these for the imm/ppa code

ENTRY(insw)
		cmp	r0, #0x00c00000
		movge	r3, #0
		movlt	r3, #0xf0000000
		add	r0, r3, r0, lsl #2
		b	__arch_readsw

ENTRY(outsw)
		cmp	r0, #0x00c00000
		movge	r3, #0
		movlt	r3, #0xf0000000
		add	r0, r3, r0, lsl #2
		b	__arch_writesw

