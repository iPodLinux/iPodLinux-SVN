	.global _start
	.global _stext

	.global _rambase
	.global _ramvec
	.global _ramstart
	.global _ramend
	.global splash_bits

#include <linux/config.h>
#define IMR 0xFFFFF304
/* #define INIT_LCD */
#undef INIT_LCD

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

splash_bits:
#include "bootlogo.rh"
	.text
_start:
_stext:

	nop

	moveb	#0x00,   0xfffffb0b	/* Watchdog off */
	moveb	#0x10,   0xfffff000	/* SCR */

	movew   #0x2483, 0xfffff200	/* PLLCR */
	movew   #0x0347, 0xfffff202	/* PLLFSR */

        /* Set special purpose bits of ports*/
	moveb	#0xc0,   0xfffff40b	/* Chip select */
	moveb	#0x00,   0xfffff413	/* LCD */
        moveb	#0xcf,   0xfffff423	/* RXD1 and TXD1 */
        moveb	#0x02,   0xfffff42b	/* A,CSA and CLKO */
        moveb   #0xfd,   0xfffff443     /* R/W enable for flash*/
        /*moveb	#0x3c,   0xfffff433*/	/* A0 and DTACK */
	moveb 	#0x3f,   0xfffff433   	/* Port G as GPIO */
        moveb	#0x20,   0xfffff44b	/* SDRAM */
	moveb	#0x08,   0xfffffd0d	/* disable hardmap */
	moveb	#0x07,   0xfffffd0e	/* level 7 interrupt clear */

        movew   #0x8000, 0xfffff108     /* Full decode A31:A29=0 */

        /* set flash size */
        movew   #0x1000, 0xfffff100     /* Start at 0x2000000 with flash */
#if defined ( CONFIG_FLASH2MB )
        #ifdef CONFIG_FLASH2WS
                movew	#0x0197, 0xfffff110	/* 2x1Meg, 16bit, enable, 2ws */
        #else
	        movew	#0x0187, 0xfffff110	/* 2x1Meg, 16bit, enable, 0ws */
        #endif
#elif defined ( CONFIG_FLASH4MB )
        #ifdef CONFIG_FLASH2WS
                movew	#0x0199, 0xfffff110	/* 2x2Meg, 16bit, enable, 2ws */
        #else
	        movew	#0x0189, 0xfffff110	/* 2x2Meg, 16bit, enable, 0ws */
        #endif
#elif defined ( CONFIG_FLASH8MB )
        #ifdef CONFIG_FLASH2WS
                movew	#0x019B, 0xfffff110	/* 2x4Meg, 16bit, enable, 2ws */
        #else
	        movew	#0x018B, 0xfffff110	/* 2x4Meg, 16bit, enable, 0ws */
        #endif
#elif defined ( CONFIG_FLASH16MB )
        #ifdef CONFIG_FLASH2WS
                movew	#0x019D, 0xfffff110	/* 2x8Meg, 16bit, enable, 2ws */
        #else
	        movew	#0x018D, 0xfffff110	/* 2x8Meg, 16bit, enable, 0ws */
        #endif
#else
        #error "No flash size defined"
#endif

        movew   #0x2000, 0xfffff102     /* Start at 0x4000000 with FPGA and ethernet */

        /*movew   #0x01ff, 0xfffff112*/     /* use 16MB size, external DTACK */
	movew   #0x0191, 0xfffff112     /* use 128kB size, 16bit, enable, 2ws */
#if defined ( CONFIG_RAM8MB )
        movew   #0x0000, 0xfffff106     /* CSD init at $0 */
        movew   #0x0281, 0xfffff116     /* enable SDRAM cs */
        movew   #0x0040, 0xfffff10a     /* chip select control */
        movew   #0x0000, 0xfffffc02     /* DRAMC disable */
        movew   #0xc03f, 0xfffffc04     /* SDCTRL use PA22 and PA21, CAS=1, RACL=3CLK */
        movew   #0x4020, 0xfffffc00     /* DRAMMC double CLK32 for refresh */
        movew   #0x8000, 0xfffffc02     /* DRAMC */
        movew   #0xc83f, 0xfffffc04     /* SDCTRL precharge */
	movew	#32767, %d0  /* PLL settle wait loop */
SDL0:
	subw	#1, %d0
	bne	SDL0
        movew   #0xD03f, 0xfffffc04     /* SDCTRL enable refresh */
	movew	#32767, %d0  /* PLL settle wait loop */
SDL1:
	subw	#1, %d0
	bne	SDL1
        movew   #0xD43f, 0xfffffc04     /* SDCTRL set mode registers in SDRAM */
        movew	#32767, %d0  /* PLL settle wait loop */
SDL2:
	subw	#1, %d0
	bne	SDL2
#elif defined ( CONFIG_RAM32MB )
        movew   #0x0000, 0xfffff106     /* CSD init at $0 */
        movew   #0x0683, 0xfffff116     /* enable SDRAM cs */
        movew   #0x0040, 0xfffff10a     /* chip select control */
        movew   #0x0000, 0xfffffc02     /* DRAMC disable */
        movew   #0xc03f, 0xfffffc04     /* SDCTRL use PA24 and PA23, CAS=1, RACL=3CLK */
        movew   #0x0F80, 0xfffffc00     /* DRAMMC double CLK32 for refresh */
        movew   #0x8000, 0xfffffc02     /* DRAMC */
        movew   #0xc83f, 0xfffffc04     /* SDCTRL precharge */
	movew	#32767, %d0  /* PLL settle wait loop */
SDL0:
	subw	#1, %d0
	bne	SDL0
        movew   #0xD03f, 0xfffffc04     /* SDCTRL enable refresh */
	movew	#32767, %d0  /* PLL settle wait loop */
SDL1:
	subw	#1, %d0
	bne	SDL1
        movew   #0xD43f, 0xfffffc04     /* SDCTRL set mode registers in SDRAM */
        movew	#32767, %d0  /* PLL settle wait loop */
SDL2:
	subw	#1, %d0
	bne	SDL2
#else
        #error "No sdram size defined"
#endif

	moveb	#0x40,   0xfffff300	/* IVR */
	movel	#0x007FFFFF, %d0	/* IMR */
	movel	%d0,     0xfffff304

	/*moveb   0xfffff42b, %d0
	andb    #0xe0,   %d0
	moveb   %d0,     0xfffff42b*/

	moveb	#0x08,   0xfffff907	/* Ignore CTS */
	movew	#0x0138, 0xfffff902	/* BAUD to 115.2k */
	movew	#0xe100, 0xfffff900	/* enable */

	moveb   #0,      0xfffffA27	/* LCKCON - LCD is off */
#ifdef INIT_LCD
        /* FIXME add code for display here */
	movel	#__vram_start, 0xfffffA00 /* LSSA */
	moveb	#15,   0xfffffA05	/* LVPW */
	movew	#240,  0xFFFFFa08	/* LXMAX */
	movew	#320,  0xFFFFFa0a	/* LYMAX */
	moveb	#0,      0xfffffa29	/* LBAR */
	moveb	#0,      0xfffffa25	/* LPXCD */
	moveb	#0x08,   0xFFFFFa20	/* LPICF */
	moveb	#0x00,   0xFFFFFA21	/* +ve pol (without backlight)*/
        moveb   #0x0,    0xFFFFFA23     /* LACD toggle rate*/
	moveb	#0x80,   0xfffffA27	/* LCKCON */
	movew	#0xff00, 0xfffff412	/* LCD pins */
        moveb	#0xc0,   0xfffff408	/* DispEn, BLEn both Outputs */
        moveb	#0xc0,   0xfffff409	/* Enable Display, no backlight */
        /* Copy data segment from ROM to RAM */
	moveal	#splash_bits, %a0
	moveal	#__vram_start, %a1
	moveal	#__vram_end, %a2

	/* Copy %a0 to %a1 until %a1 == %a2 */
DC1:
	movel	%a0@+, %d0
	movel	%d0, %a1@+
	cmpal	%a1, %a2
	bhi	DC1
#endif

	/*moveb	#0xf0,	0xfffff41b
	moveb	#0xff,	0xfffff418
	moveb	#0x00,	0xfffff419*/

#ifdef CONFIG_68328_SERIAL_UART2
	/* Enable RXD TXD port bits to enable UART2 */
	moveb	#0xcf,	0xfffff43b
#endif

#if defined ( CONFIG_RAM8MB )
	moveal  #__ramend - 0x10, %sp
#elif defined ( CONFIG_RAM32MB )
        moveal  #__ramend - 0x10, %sp
#else
        #error "No sdram size defined"
#endif
	movew	#0x2700, %sr

	moveq	#13, %d7		/* '\r' */
	jsr putc

	moveq	#10, %d7		/* '\n' */
	jsr putc

	moveq	#65, %d7		/* 'A' */
	jsr putc

	movew	#32767, %d0  /* PLL settle wait loop */
L0:
	subw	#1, %d0
	bne	L0

	moveq	#66, %d7		/* 'B' */
	jsr	putc

#if 1
	/* Copy data segment from ROM to RAM */
	moveal	#__data_rom_start, %a0
	moveal	#_sdata, %a1
	moveal	#_edata, %a2
	moveq	#67, %d7		/* 'C' */
	jsr	putc

	/* Copy %a0 to %a1 until %a1 == %a2 */
LD1:
	movel	%a0@+, %d0
	movel	%d0, %a1@+
	cmpal	%a1, %a2
	bhi	LD1
#endif

	moveq	#68, %d7		/* 'D' */
	jsr	putc

	moveal	#_sbss, %a0
	moveal	#_ebss, %a1
	/* Copy 0 to %a0 until %a0 == %a1 */

_clear_bss:
	movel	#0, %a0@+
	cmpal	%a0, %a1
	bhi	_clear_bss

        movel   #_sdata, %d0
        movel   %d0,    _rambase
        movel   #_ebss,  %d0
        movel   %d0,    _ramstart
#if defined ( CONFIG_RAM8MB )
        movel	#__ramend, %d0
#elif defined ( CONFIG_RAM32MB )
        movel	#__ramend, %d0
#else
        #error "No sdram size defined"
#endif
        
	movel	%d0,	_ramend
	movel	#__ramvec,	%d0
	movel	%d0,	_ramvec

	moveq	#69, %d7		/* 'E' */
	jsr	putc

        /*
         * load the current task pointer and stack
         */
        lea     init_task_union,%a0
        movel   %a0, _current_task
        lea     0x2000(%a0),%sp

	/*
	pea	0
	pea	env
	pea	%sp@(4)
	pea	0
	*/

	moveq	#70, %d7		/* 'F' */
	jsr	putc

 	moveq	#13, %d7		/* '\r' */
	jsr putc

	moveq	#10, %d7		/* '\n' */
	jsr putc

lp:
	jsr	start_kernel
        jmp lp
_exit:

	jmp	_exit


putc:
	moveb	%d7,0xfffff907
pclp:
	movew	0xfffff906, %d7
	andw	#0x2000, %d7
	beq	pclp
	rts

	.data
env:
	.long	0
	.text

