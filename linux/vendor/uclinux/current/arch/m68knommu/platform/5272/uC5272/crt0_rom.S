/*****************************************************************************/

/*
 *	crt0_ram.S -- startup code for Arcturus uCdimm uC5272 ColdFire board. 
 *
 *	(C) Copyright 2000, Lineo (www.lineo.com).
 *	(C) Copyright 1999, Greg Ungerer (gerg@snapgear.com).
 *
 * 	1999/02/24 Modified for the 5307 processor David W. Miller
 */

/*****************************************************************************/

#include "linux/autoconf.h"
#include "asm/coldfire.h"
#include "asm/mcfsim.h"

/*****************************************************************************/

/*
 *	Motorola M5272C3 ColdFire eval board, chip select and memory setup.
 */

#define	MEM_BASE	0x00000000	/* Memory base at address 0 */
#define	MEM_SIZE	0x003ff000	/* Memory size 4Mb */
#define	VBR_BASE	MEM_BASE	/* Vector address */

/*****************************************************************************/

.global	_start
.global _rambase
.global _ramvec
.global	_ramstart
.global	_ramend

/*****************************************************************************/

.data

/*
 *	Set up the usable of RAM stuff. Size of RAM is determined then
 *	an initial stack set up at the end.
 */
_rambase:
.long	0
_ramvec:
.long	0
_ramstart:
.long	0
_ramend:
.long	0

/*****************************************************************************/

.text

/*
 *	This is the codes first entry point. This is where it all
 *	begins...
 */

_start:
	nop					/* Filler */
	move.w	#0x2700, %sr			/* No interrupts */

	/*
	 * Setup VBR here, otherwise buserror remap will not work.
	 * if dBug was active before (on my SBC with dBug 1.1 of Dec 16 1996)
	 *
	 * bkr@cut.de 19990306
	 *
	 * Note: this is because dBUG points VBR to ROM, making vectors read
	 * only, so the bus trap can't be changed. (RS)
	 */
	move.l	#VBR_BASE, %a7			/* Note VBR can't be read */
	movec   %a7, %VBR

#if 1
	/*
	 *	Enable CPU internal cache.
	 */
	move.l	#0x01000000, %d0		/* Invalidate cache cmd */
	movec	%d0, %CACR			/* Invalidate cache */
	move.l	#0x80000100, %d0		/* Setup cache mask */
	movec	%d0, %CACR			/* Enable cache */
#endif
	
	/*
	 *      Copy data segment from ROM to RAM
	 */
	lea	__data_rom_start, %a0
	lea	_sdata, %a1
	lea	_edata, %a2
_copy_data:
	movel	%a0@+, %a1@+
	cmpal	%a1, %a2
	bhi	_copy_data

	/*
	 *	Zero out the bss region.
	 */
	lea.l	_sbss, %a0			/* Get start of bss */
	lea.l	_ebss, %a1			/* Get end of bss */
	clr.l	%d0				/* Set value */
_clear_bss:
	move.l	%d0, (%a0)+			/* Clear each word */
	cmp.l	%a0, %a1			/* Check if at end */
	bne	_clear_bss

        movel   #VBR_BASE, %d0    
        movel   %d0, _rambase
	       
        movel   #_ebss, %d0
        movel   %d0, _ramstart
	
        movel   #MEM_SIZE, %d0
        movel   %d0, _ramend
	
        movel   #VBR_BASE, %d0
        movel   %d0, _ramvec

	/*
	 * load the current task pointer and stack
	 */
	lea		init_task_union, %a0
	movel		%a0, _current_task
	lea		0x2000(%a0), %sp
	
	/*
	 *	Assember start up done, start code proper.
	 */
	jsr	start_kernel			/* Start Linux kernel */

_exit:
	jmp	_exit				/* Should never get here */

/*****************************************************************************/
