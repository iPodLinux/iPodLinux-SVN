/* linux/arch/frionommu/platform/frio/interrupt.S: 
 *	 H/W interrupts
 *	 - assumes default interrupt configuration for the interrupts
 * 
 * Copyright (C) 2003  Metrowerks, Inc. <mwaddel@metrowerks.com>
 * Copyright (C) 2002  Arcturus Networks Ltd. Ted Ma <mated@sympatico.ca>
 * Copyright (C) 1998  D. Jeff Dionne <jeff@ryeham.ee.ryerson.ca>,
 *                     Kenneth Albanowski <kjahds@kjahds.com>,
 *                     The Silver Hammer Group, Ltd.
 *
 * (c) 1995, Dionne & Associates
 * (c) 1995, DKG Display Tech.
 *
 */

#define ASSEMBLY
	 
#include <asm/hw_irq.h>

#include <linux/autoconf.h>

#include <asm/entry.h>
#include "frio_defs.h"

// Global references
.global _stext;
.global __bss_start;
.global frio_irq_panic;		 // go to panic in ints.c
	 
/*
 * Code to save processor context.
 *  We even save the register which are preserved by a function call
 *	 - r4, r5, r6, r7, p3, p4, p5
 */
#define SAVE_CONTEXT save_context
.macro save_context
	[--sp] = ( R7:0, P5:0 );
	[--sp] = fp;
	[--sp] = usp;

	[--sp] = i0;
	[--sp] = i1;
	[--sp] = i2;
	[--sp] = i3;

	[--sp] = m0;
	[--sp] = m1;
	[--sp] = m2;
	[--sp] = m3;

	[--sp] = l0;
	[--sp] = l1;
	[--sp] = l2;
	[--sp] = l3;

	[--sp] = b0;
	[--sp] = b1;
	[--sp] = b2;
	[--sp] = b3;
	[--sp] = a0.x;
	[--sp] = a0.w;
	[--sp] = a1.x;
	[--sp] = a1.w;

	[--sp] = LC0;
	[--sp] = LC1;
	[--sp] = LT0;
	[--sp] = LT1;
	[--sp] = LB0;
	[--sp] = LB1;

	[--sp] = ASTAT;

	[--sp] = RETS;
	[--sp] = RETI;
	[--sp] = RETX;	/* get breakpoint from here, former pc */
	[--sp] = RETN;
	[--sp] = RETE;
	[--sp] = SYSCFG;
	[--sp] = SEQSTAT;
.endm
	 
#define RESTORE_CONTEXT restore_context
.macro restore_context
	 SEQSTAT = [sp++];
	 SYSCFG = [sp++];
	 RETE = [sp++];
	 RETN = [sp++];
	 RETX = [sp++];
	 RETI = [sp++];
	 RETS = [sp++];

	 ASTAT = [sp++];

	 LB1 = [sp++];
	 LB0 = [sp++];
	 LT1 = [sp++];
	 LT0 = [sp++];
	 LC1 = [sp++];
	 LC0 = [sp++];

	 a1.w = [sp++];
	 a1.x = [sp++];
	 a0.w = [sp++];
	 a0.x = [sp++];
	 b3 = [sp++];
	 b2 = [sp++];
	 b1 = [sp++];
	 b0 = [sp++];

	 l3 = [sp++];
	 l2 = [sp++];
	 l1 = [sp++];
	 l0 = [sp++];

	 m3 = [sp++];
	 m2 = [sp++];
	 m1 = [sp++];
	 m0 = [sp++];

	 i3 = [sp++];
	 i2 = [sp++];
	 i1 = [sp++];
	 i0 = [sp++];

	 usp = [sp++];
	 fp = [sp++];

	 ( R7 : 0, P5 : 0) = [ SP ++ ];
.endm
	 
/*****************************************************************************/
// Data areas
.bss

/*
 *      Set up the usable of RAM stuff. Size of RAM is determined then
 *      an initial stack set up at the end.
 */
.align 4

.data

	 
.text
.align 2 	// just in case
/*
 * initial interrupt handlers
 */
#ifndef CONFIG_KGDB	// installs own handler
 	 /* interrupt routine for emulation - 0 */
	 /* Currently used only if GDB stub is not in - invalid	  */
	 /* gdb-stub set the evt itself */
	 /* save registers for post-mortem only */
.global evt_emulation
evt_emulation:
	 SAVE_CONTEXT

	 r0 = IRQ_EMU;
	 r1 = seqstat; 
	 call	frio_irq_panic;	// this should go to an invalid - panic
	// - GDB stub fills this in by itself (if defined)
	  rte; 
#endif

	/* interrupt routine for nmi - 2 */
.global evt_nmi
evt_nmi:
	 SAVE_CONTEXT

	r0 = IRQ_NMI;
	r1 = RETN;	     // get return address (but not going back)  
	call frio_irq_panic; // this should end up at a panic - for now 
_evt_nmi_exit:
	 rtn; 
			
irq_panic:
	 r0 = IRQ_EVX;
	
	/* Changed by HuTao, May 21, 2003 2:44PM */
	 r1 =  sp;
	 call	frio_irq_panic;
	 /* panic doesn't return, so no restoration of context. */
	 
	 /* interrupt routine for ivhw - 5 */
	 /* Hardware Error */
.global evt_ivhw
evt_ivhw:
	 SAVE_CONTEXT

	 r1 = seqstat;	  // reason code is it bit 18:14
	 r0 = IRQ_HWERR; 
	 call	frio_irq_panic;
	 rti; 

	 /* interrupt routine for core timer - 6 */	 
.global evt_timer
evt_timer:

	SAVE_CONTEXT
	r0 = IRQ_CORETMR;  
	call process_int;  /* call do_timer; */
	RESTORE_CONTEXT
	rti; 
	nop;

	 /* interrupt routine for evt7 - 7 */	 
.global evt_evt7
evt_evt7:
	 SAVE_CONTEXT
	 
//	 r0 = IRQ_RTC;	// Real Time Clock 
//	 r0 = IRQ_USB;	// Universal Serial Bus 
//	 r0 = IRQ_PCI;	// Personal Computer Interface ?? 
	 r0 = 7;


	 call	process_int; 
evt_evt7_exit:
	 RESTORE_CONTEXT
	 rti; 

	 /* interrupt routine for evt8 - 8 */
.global evt_evt8
evt_evt8:
	 SAVE_CONTEXT

// 	 r0 = IRQ_SPORT0; // Synchronous port 0 
// 	 r0 = IRQ_SPORT1; // Synchronous POrt 1
	 r0 = 8;
	 
	 call	process_int; 
evt_evt8_exit:
	 RESTORE_CONTEXT
	 rti; 

	 /* interrupt routine for evt9 - 9 */	 
.global evt_evt9
evt_evt9:
	 SAVE_CONTEXT

// 	 r0 = IRQ_SPI0;	// Serial Peripheral Interface 0
// 	 r0 = IRQ_SPI1;	// Serial Peripheral Interface 1
	 r0 = 9;

	 call	process_int; 
evt_evt9_exit:
	 RESTORE_CONTEXT
	 rti; 

	 /* interrupt routine for evt10	- 10 */	 
.global evt_evt10
evt_evt10:
	 SAVE_CONTEXT

// 	 r0 = IRQ_UART1; // UART 
	 r0 = 10;
	 
	 call	process_int; 
evt_evt10_exit:
	 RESTORE_CONTEXT
	 rti; 

	 /* interrupt routine for evt11	- 11 */	 
.global evt_evt11
evt_evt11:
	 SAVE_CONTEXT

// 	 r0 = IRQ_TMR0;	// TImer 0
// 	 r0 = IRQ_TMR1;	// TImer 1
// 	 r0 = IRQ_TMR2;	// TImer 2
	 r0 = 11;

	 call	process_int; 
evt_evt11_exit:
	 RESTORE_CONTEXT
	 rti; 

	 /* interrupt routine for evt12	- 12 */	 
.global evt_evt12
evt_evt12:
	 SAVE_CONTEXT

// 	 r0 = IRQ_PROG_INTA;	// Programmable interrupts flag A
// 	 r0 = IRQ_PROG_INTB;	// Programmable interrupts flag B
	 r0 = 12;
	 
	 call	process_int; 
evt_evt12_exit:
	 RESTORE_CONTEXT
	 rti; 

	 /* interrupt routine for evt13	- 13 */
.global evt_evt13
evt_evt13:
	 SAVE_CONTEXT

// 	 r0 = IRQ_WATCH;	// Watch dog interrupt
	 r0 = 13;
	 
	 call	process_int; 
evt_evt13_exit:
	 RESTORE_CONTEXT
	 rti; 

	 /* interrupt routine for system_call - 14 */	 
.global evt_system_call
evt_system_call:
	jump.l system_call;

	 /* interrupt routine for soft_int1 - 15 */	 
.global evt_soft_int1
evt_soft_int1:
	 SAVE_CONTEXT

	 r0 = IRQ_SW_INT1;	// Software Interrupt 1 
	 
	 call	process_int; 
evt_soft_int1_exit:
	 RESTORE_CONTEXT
	 rti; 
