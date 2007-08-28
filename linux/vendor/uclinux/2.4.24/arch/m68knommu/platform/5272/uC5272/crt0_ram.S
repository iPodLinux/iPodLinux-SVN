/*****************************************************************************/

/*
 *	crt0_ram.S -- startup code for Arcturus uCdimm uC5272 ColdFire board.
 *
 *	(C) Copyright 2003, Phil Wilshire <philwil@sydcs.com>
 *	(C) Copyright 2002, Michael Leslie <mleslie@arcturusnetworks.com>
 *	(C) Copyright 2000, Lineo (www.lineo.com).
 *	(C) Copyright 1999, Greg Ungerer (gerg@snapgear.com).
 *
 * 	2003/04/22 Modified for use with and without Cramfs Phil Wilshire
 * 	1999/02/24 Modified for the 5307 processor David W. Miller
 */

/*****************************************************************************/

#include "linux/autoconf.h"
#include "asm/coldfire.h"
#include "asm/mcfsim.h"

/*****************************************************************************/

/* Chip select and memory setup for Arcturus Networks' uC5272 dimm. */

#define	MEM_BASE	0x00000000	/* Memory base at address 0 */
#define	MEM_SIZE	0x00800000	/* Memory size 8Mb */
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

	move.l	#VBR_BASE, %a7			/* Note VBR can't be read */
	movec   %a7, %VBR

	
	move.l	%a7, _ramvec			/* Set up vector addr */
	move.l	%a7, _rambase			/* Set up base RAM addr */

/* Set to 8 meg for the uC5272 dimm: */
	move.l	#MEM_SIZE, %a0

	move.l	%a0, %d0			/* Mem end addr is in a0 */
	move.l	%d0, %sp			/* Set up initial stack ptr */
	move.l	%d0, _ramend			/* Set end ram addr */
	
	/* jsr	disablecache */
	 jsr	enablecache 


#ifdef CONFIG_ROMFS_FS
	/*
	 *	Move ROM filesystem above bss :-)
	 */
	lea.l	_sbss, %a0			/* Get start of bss */
	lea.l	_ebss, %a1			/* Set up destination  */
	move.l	%a0, %a2			/* Copy of bss start */

	move.l	8(%a0), %d0			/* Get size of ROMFS */
	addq.l	#8, %d0				/* Allow for rounding */
	and.l	#0xfffffffc, %d0		/* Whole words */

	add.l	%d0, %a0			/* Copy from end */
	add.l	%d0, %a1			/* Copy from end */
	move.l	%a1, _ramstart			/* Set start of ram */

_copy_romfs:
	move.l	-(%a0), %d0			/* Copy dword */
	move.l	%d0, -(%a1)
	cmp.l	%a0, %a2			/* Check if at end */
	bne	_copy_romfs
#endif

/* set leds on test */	
_setup_port_c:	
/* set up port c */
	move.l #0x10000094, %a0                 /* ddr */
	move.b #0xff,(%a0)
	move.l #0x10000096, %a0                 /* data */
	move.b #0x12,(%a0)
	/*jmp _exit */                              /* thassall */
	

#ifdef  CONFIG_UC5272_CRAMFS
/* Tell setup.c where the heap starts:	 */
	lea.l	_end_romfs, %a0			/* Get end of romfs */
	move.l	%a0,%d0
	addq.l	#8, %d0				/* Allow for rounding */
	and.l	#0xfffffffc, %d0		/* Whole words */
	move.l	%d0, _ramstart			/* Set start of heap */

#endif
	
/* Clear .bss: */
	lea.l	_sbss, %a0			/* Get start of bss */
	lea.l	_ebss, %a1			/* Get end of bss */
	clr.l	%d0				/* Set value */
_clear_bss:
	move.l	%d0, (%a0)+			/* Clear each word */
	cmp.l	%a0, %a1			/* Check if at end */
	bne	_clear_bss

	/* Load the current task pointer and stack */
	lea		init_task_union, %a0
	movel		%a0, _current_task
	lea		0x2000(%a0), %sp

	/* Enable cache: */
	/* jsr	enablecache */

	/* Assember start up done, start code proper: */
	/* pea	0       */
	/* pea	env     */
	/* pea	%sp@(4) */
	/* pea	0       */
	jsr	start_kernel

	
_exit:
	jmp	_exit				/* Should never get here */

/*****************************************************************************/

/* Disable the cpu cache: (clobbers %d0) */
disablecache:
	move.l	#0x00000000, %d0
	move.c	%d0,%CACR
	move.c	%d0, %ACR0
	move.c	%d0, %ACR1
	rts

/* Enable the cpu cache: (clobbers %d0) */
enablecache:
	move.l	#0x01000000, %d0		/* Invalidate cache cmd */
	movec	%d0, %CACR			/* Invalidate cache */
	move.l	#0x80000100, %d0		/* Setup cache mask */
	movec	%d0, %CACR			/* Enable cache */
	rts
/*****************************************************************************/
