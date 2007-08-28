/*****************************************************************************/

/*
 *	crt0_rom.S -- startup code for Motorola M5249C3 eval board.
 *
 *	(C) Copyright 1999-2003, Greg Ungerer (gerg@snapgear.com).
 */

/*****************************************************************************/

#include "linux/autoconf.h"
#include "asm/coldfire.h"
#include "asm/mcfsim.h"

/*****************************************************************************/

/*
 *	Motorola M5249C3 ColdFire eval board, chip select and memory setup.
 */
#define	MEM_BASE	0x00000000	/* Memory base at address 0 */
#define	MEM_SIZE	0x00800000	/* Memory size 8MB */
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
 *	Size of RAM is determined then an initial stack set up at the end.
 */
_rambase:
.long	0
_ramvec:
.long	0
_ramstart:
.long	0
_ramend:
.long	0

#if CONFIG_BLK_DEV_INITRD
/*
 *	Setup initial RAM disk limits of using INITRD.
 */
.extern	initrd_start
.extern initrd_end
#endif

/*****************************************************************************/

.text

_vectors:

.long	0x00000000, _start
.long	_fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault

.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault

.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault

.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault
.long	_fault, _fault, _fault, _fault, _fault, _fault, _fault, _fault

/*****************************************************************************/

/*
 *	This is the codes first entry point. This is where it all
 *	begins...
 */

_start:
	nop					/* Filler */
	move.w	#0x2700, %sr			/* No interrupts */

	/*
	 *	Set RAMBAR0 and RAMBAR1, just incase they are not set.
	 */
	move.l	#0x20000001, %a1
	movec	%a1, #0XC05			/* Map RAMBAR1 region */
	move.l	#0x20010001, %a0
	movec	%a0, %RAMBAR0			/* Map RAMBAR region */

	/*
	 *	Set MBAR1 and MBAR2, just incase they are not set.
	 */
	move.l  #0x10000001, %a0
	movec   %a0, %MBAR			/* Map MBAR region */
	subq.l  #1, %a0				/* Get MBAR address in a0 */

	move.l  #0x80000001, %a1
	movec   %a1, #0xc0e			/* Map MBAR2 region */
	subq.l  #1, %a1				/* Get MBAR2 address in a1 */


	/*
	 *	Flash chip select setup.
	 */
	move.l	#0xfff00000, %d0	/* Flash on CS0 */
	move.l	%d0, MCFSIM_CSAR0(%a0)
	move.l	#0x001f0021, %d0
	move.l	%d0, MCFSIM_CSMR0(%a0)
	move.w	#0x1180, %d0
	move.w	%d0, MCFSIM_CSCR0(%a0)


	/*
	 *	SDRAM initialization. Magic sequence!
	 */
	/* STEP 1 */
	move.w	#0x800a, %d0		/* Sync, 6 RTIM, 30h Refresh */
	move.w	%d0, MCFSIM_DCR(%a0)	/* DCR setup */

	/* STEP 2 */
	move.l	#0x00002224, %d0
	move.l	%d0, MCFSIM_DACR0(%a0)	/* DACR0 setup */

	/* STEP 3 */
	move.l	#0x007c0001, %d0
	move.l	%d0, MCFSIM_DMR0(%a0)	/* DCMR0 setup */

	/* STEP 4 */
	move.l	#0x0000222c, %d0
	move.l	%d0, MCFSIM_DACR0(%a0)	/* DACR0 setup PRECHARGE-ALL */

	/* STEP 5 */
	move.l	0x00000800, %d0		/* Read from SDRAM... */

	nop
	nop
	nop
	nop
	nop

	/* STEP 6 */
	move.l	#0x0000a224, %d0
	move.l	%d0, MCFSIM_DACR0(%a0)	/* DACR0 setup REFRESH ENABLE */

	move.l	#0x20000800, %a2
	move.l	#0x000fffff, %d0	/* Big delay */
delay2:
	move.l	%d0, (%a2)
	sub.l	#1, %d0
	bne	delay2


	/* STEP 7 */
	move.l	#0x0000a264, %d0
	move.l	%d0, MCFSIM_DACR0(%a0)	/* DACR0 setup MODE CMD */

	/* STEP 8 */
	clr.l	%d0
	move.l	%d0, 0x00000800		/* Write to set MODE */


	/*
	 *      Move secondary interrupts to base at 128.
	 */
	move.b  #0x80, %d0
	move.b  %d0, 0x16b(%a1)			/* Interrupt base register */

#if 1
	/*
	 *      Work around broken CSMR0/DRAM vector problem.
	 */
	move.l  #0x001F0021, %d0		/* Disable C/I bit */
	move.l  %d0, 0x84(%a0)			/* Set CSMR0 */
#endif

	/*
	 *	Disable the PLL firstly. (Who knows what state it is
	 *	in here!).
	 */
	move.l  0x180(%a1), %d0			/* Get current PLL value */
	and.l   #0xfffffffe, %d0		/* PLL bypass first */
	move.l  %d0, 0x180(%a1)			/* Set PLL register */
	nop

#if CONFIG_CLOCK_140MHz
	/*
	 *	Set initial clock frequency. This assumes M5249C3 board
	 *	is fitted with 11.2896MHz crystal. It will program the
	 *	PLL for 140MHz. Lets go fast :-)
	 */
	move.l  #0x125a40f0, %d0		/* Set for 140MHz */
	move.l  %d0, 0x180(%a1)			/* Set PLL register */
	or.l    #0x1, %d0
	move.l  %d0, 0x180(%a1)			/* Set PLL register */
#endif

	/*
	 *	Setup CS1 for ethernet controller.
	 *	(Setup as per M5249C3 doco).
	 */
	move.l  #0xe0000000, %d0		/* CS1 mapped at 0xe0000000 */
	move.l  %d0, 0x8c(%a0)
	move.l  #0x001f0021, %d0		/* CS1 size of 1Mb */
	move.l  %d0, 0x90(%a0)
	move.w  #0x0080, %d0			/* CS1 = 16bit port, AA */
	move.w  %d0, 0x96(%a0)

	/*
	 *	Setup CS2 for IDE interface.
	 */
	move.l	#0x50000000, %d0		/* CS2 mapped at 0x50000000 */
	move.l	%d0, 0x98(%a0)
	move.l	#0x001f0001, %d0		/* CS2 size of 1MB */
	move.l	%d0, 0x9c(%a0)
	move.w	#0x0080, %d0			/* CS2 = 16bit, TA */
	move.w	%d0, 0xa2(%a0)

	move.l	#0x00107020, %d0		/* IDEconfig1 */
	move.l	%d0, 0x18c(%a1)
	move.l	#0x000c0400, %d0		/* IDEconfig2 */
	move.l	%d0, 0x190(%a1)

	move.l	#0x00080000, %d0		/* GPIO19, IDE reset bit */
	or.l	%d0, 0xc(%a1)			/* Function GPIO19 */
	or.l	%d0, 0x8(%a1)			/* Enable GPIO19 as output */
        or.l	%d0, 0x4(%a1)			/* De-assert IDE reset */


	/*
	 *	Enable CPU internal cache.
	 */
	move.l  #0x01000000, %d0		/* Invalidate whole cache */
	movec   %d0, %CACR
	nop

	move.l  #0x0000c000, %d0		/* Set SDRAM cached only */
	movec   %d0, %ACR0
	move.l  #0xfff0c000, %d0		/* No other regions cached */
	movec   %d0, %ACR1

	move.l  #0xa0000200, %d0		/* Enable cache... */
	movec   %d0, %CACR
	nop


	/*
	 *	Copy initialized data region to RAM.
	 */
	lea.l	_sdata, %a0			/* Get start of data */
	lea.l	__init_end, %a1			/* Get end of data+init */
	lea.l	_etext, %a2			/* Get end of text */
_data_copy:
	move.l	(%a2)+, %d0			/* Get next word */
	move.l	%d0, (%a0)+			/* Copy to RAM */
	cmp.l	%a0, %a1			/* Check if at end */
	bne	_data_copy

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


	/*
	 *	Setup VBR as per eval board.
	 */
	move.l	#VBR_BASE, %a0			/* Note VBR can't be read */
	movec   %a0, %VBR
	move.l	%a0, _ramvec			/* Set up vector addr */
	move.l	%a0, _rambase			/* Set up base RAM addr */

	lea.l	_end, %a0			/* Get end of kernel mem */
	move.l	%a0, _ramstart

	/*
	 *	Set the memory size, and then set a temporary stack.
	 */
	move.l	#MEM_SIZE, %a0
	move.l	%a0, %d0			/* Mem end addr is in a0 */
	move.l	%d0, %sp			/* Set up initial stack ptr */
	move.l	%d0, _ramend			/* Set end ram addr */


	/*
	 *	Load the current task pointer and stack.
	 */
	lea	init_task_union, %a0
	movel	%a0, _current_task
	lea	0x2000(%a0), %sp

	/*
	 *	Assember start up done, start code proper.
	 */
	jsr	start_kernel			/* Start Linux kernel */

_fault:
_exit:
	jmp	_exit				/* Should never get here */

/*****************************************************************************/
