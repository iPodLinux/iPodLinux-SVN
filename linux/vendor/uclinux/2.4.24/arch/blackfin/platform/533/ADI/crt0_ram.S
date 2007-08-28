/* linux/arch/frio/kernel/head.S:  A startup file for the Blackfin 
 *
 * Copyright (c) 2003  Bas Vermeulen <bas@buyways.nl>,
 *                     BuyWays B.V. (www.buyways.nl)
 * Copyright (c) 2003  Metrowerks/Motorola
 * Copyright (C) 1998  D. Jeff Dionne <jeff@ryeham.ee.ryerson.ca>,
 *                     Kenneth Albanowski <kjahds@kjahds.com>,
 *                     The Silver Hammer Group, Ltd.
 *
 * (c) 1995, Dionne & Associates
 * (c) 1995, DKG Display Tech.
 */

#define ASSEMBLY

#include <linux/config.h>

.global _stext;
.global __bss_start;

.global start;
.global _start;

.global _rambase;
.global __ramvec;
.global _ramvec;
.global _ramstart;
.global _ramend;

#define	MEM_BASE	0x00000000
#define VBR_BASE	MEM_BASE
#define MEM_SIZE	0x1000000

/*********************************************************************/

.data

/*
 *      Set up the usable of RAM stuff. Size of RAM is determined then
 *      an initial stack set up at the end.
 */
.align 4
_ramvec:
.long   0
_rambase:
.long   0
_ramstart:
.long   0
_ramend:
.long   0

.text
	
_start:
start:
_stext:

	/*
	 * It seems that the value of SP reg is unpredicable after reset.
	 * We'd better set SP before using "reti" and "[--sp] = reti" 
	 *
	 * HuTao May26, 2003 3:02PM
	 */
	sp.h = 0xffb0;	/* 0x1000(4KB) L1 RAM as system stack */
	sp.l = 0x1000;

	// This next section keeps the processor in supervisor mode
        // during kernel boot.  Switches to user mode at end of boot.
	// See page 3-9 of Hardware Reference manual for documentation.

	// EVT15 = _real_start

	p0.l = 0x203c
	p0.h = 0xffe0;
	p1.l = _real_start;
	p1.h = _real_start;
	[p0] = p1;

	// IMASK = 0x8000

	p0.l = 0x2104;
	p0.h = 0xffe0;
	p1.l = 0x8000;
	p1.h = 0x0;
	[p0] = p1;

	raise 15;
	p0.l = WAIT_HERE;
	p0.h = WAIT_HERE;
	reti = p0;
	rti;
	
WAIT_HERE:
	jump WAIT_HERE;

.global _real_start;
_real_start:
	[ -- sp ] = reti;
	p0.l = 0x0200;
	p0.h = 0xFFC0;
	r0 = 0xD6(z);
	w[p0] = r0;	/* watchdog off for now */

	/* Code update for BSS size == 0
	 * Zero out the bss region.
	 */
	p1.l = _sbss;
	p1.h = _sbss;
	p2.l = _ebss;
	p2.h = _ebss;
	lsetup (_clear_bss, _clear_bss_end ) lc1 = p2 - p1;
	CC = p2<=p1;
	if CC jump _clear_bss_skip;
	r0 = 0;
_clear_bss:
_clear_bss_end:
	[p1++] = r0;
_clear_bss_skip:	/* to skip bss clear */

	p1.l = _rambase;
	p1.h = _rambase;
	r0.l = _sdata ;
	r0.h = _sdata ;
	[p1] = r0;

	p1.l = _ramstart;
	p1.h = _ramstart;
	r0.l = _end;
	r0.h = _end;
	[p1] = r0;

	r0.l = 0x0000;
	r0.h = 0x100;	/* MEM_SIZE = 0x01000000 */
	p1.l = _ramend;
	p1.h = _ramend;
	[p1] = r0;

	/*
	 *  load the current task pointer and stack
	 */
	r1.l = init_task_union;
	r1.h = init_task_union;
	p1.l = _current_task;
	p1.h = _current_task;
	[p1] = r1;

/* Use kernel stack instead of an l1 stack. l1 stack might overflow.
 * For this time being this should be sufficient.
 */
	r2 = 0x2000(z);
	r1 = r1 + r2;
	sp = r1;

	call	start_kernel;
_exit:

	jump.s	_exit;

	.data
env:
	.long	0;
