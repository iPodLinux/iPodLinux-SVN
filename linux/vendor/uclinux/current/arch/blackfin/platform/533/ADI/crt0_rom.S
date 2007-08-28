/* linux/arch/frionommu/platform/533/ADI/crt0_rom.S:  A startup file for the BlackFin BF533 
 *
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
#define MEM_SIZE	0x00400000

/*****************************************************************************/

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
	r7 = cli;
	sp.l = 0x0000;
	sp.h = 0x0500;
	usp = sp;

	r0 = 0;		/* VBR_BASE */
	p1.l = _ramvec;
	p1.h = _ramvec;
	[p1] = r0;
	p1.l = _rambase;
	p1.h = _rambase;
	[p1] = r0;

	r0.l = 0x0000;
	r0.h = 0x0040;	/* MEM_SIZE = 0x00400000 */
	p1.l = _ramend;
	p1.h = _ramend;
	[p1] = r0;

#if 0
	/*
	 * Zero out the bss region.
	 */
	p1.l = _sbss;
	p1.h = _sbss;
	p2.l = _ebss;
	p2.h = _ebss;
	r0 = 0;
_clear_bss:
	[p1++] = r0;
	CC = p1 == p2;
	if !cc jump _clear_bss;
#endif
	/*
	 *  load the current task pointer and stack
	 */
	p1.l = init_task_union;
	p1.h = init_task_union;
	p2.l = _current_task;
	p2.h = _current_task;
	[p2] = p1;
	[p2 + 0x2000] = sp;

	sti r7;
	sp.l = 0x0000;
	sp.h = 0x0500;
	usp = sp;
	call	start_kernel;
_exit:

	jump.s	_exit;

	.data
env:
	.long	0;
