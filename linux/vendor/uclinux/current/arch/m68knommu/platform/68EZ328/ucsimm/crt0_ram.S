
#define CONFIG_RELOCATE
#define DEBUG

#include <linux/config.h>
	
	.global _start
	.global _stext

	.global _rambase
	.global _ramvec
	.global _ramstart
	.global _ramend

#ifdef CONFIG_INIT_LCD
	.global splash_bits
#endif

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

#ifdef CONFIG_INIT_LCD
splash_bits:
#include "bootlogo.rh"
#endif
	
	.text
_start:
_stext:	movew	#0x2700,%sr

#ifdef DEBUG
/* 'K' */
        moveq   #'K', %d7
        moveb   %d7,0xfffff907          /* No absolute addresses */
0:
        movew   0xfffff906, %d7
        andw    #0x2000, %d7
        beq     0b
#endif	

#ifdef CONFIG_RELOCATE
	/* Copy me to RAM */
	moveal	#__rom_start, %a0
	moveal	#__ram_start, %a1
	moveal	#__data_end, %a2

	/* Copy %a0 to %a1 until %a1 == %a2 */
LD1:
	movel	%a0@+, %d0
	movel	%d0, %a1@+
	cmpal	%a1, %a2
	bhi	LD1
	
#ifdef DEBUG
	moveq	#74, %d7		/* 'J' */
	moveb	%d7,0xfffff907          /* No absolute addresses */
pclp2:
	movew	0xfffff906, %d7
	andw	#0x2000, %d7
	beq	pclp2
#endif /* DEBUG */
	/* jump into the RAM copy */
	jmp     ram_jump
ram_jump:

#endif /* CONFIG_RELOCATE */


#ifdef CONFIG_INIT_LCD
	movel	#splash_bits, 0xfffffA00 /* LSSA */
	moveb	#0x1e,   0xfffffA05	/* LVPW */
	movew	#0x1e0,  0xFFFFFa08	/* LXMAX */
	movew	#0xf0,  0xFFFFFa0a	/* LYMAX */
	moveb	#0,      0xfffffa29	/* LBAR */
	moveb	#0,      0xfffffa25	/* LPXCD */
	moveb	#0x08,   0xFFFFFa20	/* LPICF */
	moveb	#0x00,   0xFFFFFA21	/* -ve pol */
	moveb	#0x81,   0xfffffA27	/* LCKCON */
	movew	#0xff00, 0xfffff412	/* LCD pins */
#endif
	moveal  #__ramend-CONFIG_MEMORY_RESERVE*0x100000 - 0x10, %sp
	movew	#32767, %d0  /* PLL settle wait loop */
1:	subq	#1, %d0
	bne	1b

#ifdef DEBUG
/* 'L' */
        moveq   #'L', %d7
        moveb   %d7,0xfffff907          /* No absolute addresses */
0:
        movew   0xfffff906, %d7
        andw    #0x2000, %d7
        beq     0b
#endif


/* Clear BSS  */

	moveal	#_sbss, %a0
	moveal	#_ebss, %a1
	
1:
	clrl	%a0@+
	cmpal	%a0, %a1
	bhi	1b

#ifdef DEBUG
/* 'D' */
        moveq   #'D', %d7
        moveb   %d7,0xfffff907          /* No absolute addresses */
0:
        movew   0xfffff906, %d7
        andw    #0x2000, %d7
        beq     0b
#endif

        movel   #_sdata, %d0    
        movel   %d0,    _rambase        
        movel   #_ebss,  %d0
        movel   %d0,    _ramstart
	movel	#__ramend-CONFIG_MEMORY_RESERVE*0x100000, %d0
	movel	%d0,	_ramend
	movel	#__ramvec,	%d0
	movel	%d0,	_ramvec

#ifdef DEBUG
/* 'S' */
        moveq   #'M', %d7
        moveb   %d7,0xfffff907 
0:
        movew   0xfffff906, %d7 
        andw    #0x2000, %d7
        beq     0b
#endif  
	
/*
 * load the current task pointer and stack
 */
        lea     init_task_union,%a0
        movel   %a0, _current_task
        lea     0x2000(%a0),%sp

#ifdef DEBUG
/* 'S' */
        moveq   #'S', %d7
        moveb   %d7,0xfffff907          /* No absolute addresses */
0:
        movew   0xfffff906, %d7
        andw    #0x2000, %d7
        beq     0b
#endif

1:	jsr	start_kernel
#ifdef DEBUG
/* '!' */
        moveq   #'!', %d7
        moveb   %d7,0xfffff907          /* No absolute addresses */
0:
        movew   0xfffff906, %d7
        andw    #0x2000, %d7
        beq     0b
#endif
        bra 1b
_exit:

	jmp	_exit


putc:
	moveb	%d7,0xfffff907
1:
	movew	0xfffff906, %d7
	andw	#0x2000, %d7
	beq	1b
	rts

	.data
env:
	.long	0
	.text

