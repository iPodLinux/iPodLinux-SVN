#include <linux/config.h>

#if defined(CONFIG_HIMEMKERNEL)
#if defined(CONFIG_RAM32MB)
#define	MEM_END	0x01e00000	/* Memory size 30Mb */
#elif defined(CONFIG_RAM16MB)
#define	MEM_END	0x00e00000	/* Memory size 14Mb */
#else
#define	MEM_END	0x00600000	/* Memory size 6Mb */
#endif
#else
#if defined(CONFIG_RAM32MB)
#define	MEM_END	0x02000000	/* Memory size 32Mb */
#elif defined(CONFIG_RAM16MB)
#define	MEM_END	0x01000000	/* Memory size 16Mb */
#else
#define	MEM_END	0x00800000	/* Memory size 8Mb */
#endif
#endif

#undef CRT_DEBUG

.macro	PUTC CHAR
#ifdef CRT_DEBUG
	moveq	#\CHAR, %d7
	jsr	putc
#endif
.endm

	.global	_start
	.global _rambase
	.global _ramvec
	.global	_ramstart
	.global	_ramend

	.data

/*
 *	Set up the usable of RAM stuff
 */
_rambase:
	.long	0
_ramvec:
	.long	0
_ramstart:
	.long	0
_ramend:
	.long	0

	.text

_start:

/*
 * Setup registers, initial stack
 */
	movew	#0x2700, %sr
	movel	#0xffffffff, 0xfffff304	/* disable all interrupts */
	movel	#MEM_END-4, %sp

	PUTC	'\r'
	PUTC	'\n'
	PUTC	'A'

#ifndef CONFIG_RAMKERNEL
/*
 * Copy data segment from ROM to RAM
 */
	lea	_etext, %a0
	lea	_sdata, %a1
	lea	_edata, %a2

1:	cmpal  %a1, %a2
	beq	2f
	movel	(%a0)+, (%a1)+
	bra	1b
2:

/*
 * Set start of ram
 */
	lea	_ebss, %a1
	movel	%a1, _ramstart

	PUTC	'B'

#endif

/*
 *	Determine end of RAM
 */
	movel	#MEM_END, %a0
	movel	%a0, _ramend

	PUTC	'C'

#ifdef CONFIG_RAMKERNEL
/*
 *	Move ROM filesystem above bss :-)
 */
	moveal	#_sbss, %a0			/* romfs at the start of bss */
	moveal	#_ebss, %a1			/* Set up destination  */
	movel	%a0, %a2			/* Copy of bss start */

	movel	8(%a0), %d1			/* Get size of ROMFS */
	addql	#8, %d1				/* Allow for rounding */
	andl	#0xfffffffc, %d1	/* Whole words */

	addl	%d1, %a0			/* Copy from end */
	addl	%d1, %a1			/* Copy from end */
	movel	%a1, _ramstart		/* Set start of ram */

1:
	movel	-(%a0), %d0			/* Copy dword */
	movel	%d0, -(%a1)
	cmpl	%a0, %a2			/* Check if at end */
	bne	1b

	PUTC	'D'

#endif

/*
 * Initialize BSS segment to 0
 */
	lea	_sbss, %a0
	lea	_ebss, %a1

	/* Copy 0 to %a0 until %a0 == %a1 */
2:	cmpal	%a0, %a1
	beq	1f
	clrl	(%a0)+
	bra	2b
1:

	PUTC	'E'

/*
 * Load the current task pointer and stack
 */

	lea	init_task_union, %a0
	movel	%a0, _current_task
	lea	0x2000(%a0), %sp

	PUTC	'F'
	PUTC	'\r'
	PUTC	'\n'

/*
 * Go
 */

	jmp	start_kernel

/*
 * Local functions
 */
 
#ifdef CRT_DEBUG
putc:
	moveb	%d7, 0xfffff907
1:
	movew	0xfffff906, %d7
	andw	#0x2000, %d7
	beq	1b
	rts
#endif
