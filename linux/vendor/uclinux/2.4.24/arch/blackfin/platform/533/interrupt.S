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
	r1 = SP;
	call	process_int; // process_int(vec, pt_regs);
	RESTORE_CONTEXT
	rti; 
	nop;

	 /* interrupt routine for evt7 - 7 */	 
.global evt_evt7
evt_evt7:
	SAVE_CONTEXT

	r0 = 7;
	r1 = SP;
	call	process_int; // process_int(vec, pt_regs);
evt_evt7_exit:
	RESTORE_CONTEXT
	rti; 

	/* interrupt routine for evt8 - 8 */
.global evt_evt8
evt_evt8:
	SAVE_CONTEXT

	r0 = 8;
	r1 = SP;
	 
	call	process_int; // process_int(vec, pt_regs);
evt_evt8_exit:
	RESTORE_CONTEXT
	rti; 

	/* interrupt routine for evt9 - 9 */	 
.global evt_evt9
evt_evt9:
	SAVE_CONTEXT

	r0 = 9;
	r1 = SP;

	call	process_int; // process_int(vec, pt_regs);
evt_evt9_exit:
	RESTORE_CONTEXT
	rti; 

	/* interrupt routine for evt10	- 10 */	 
.global evt_evt10
evt_evt10:
	SAVE_CONTEXT

	r0 = 10;
	r1 = SP;
	call	process_int; // process_int(vec, pt_regs);
evt_evt10_exit:
	RESTORE_CONTEXT
	rti; 

	 /* interrupt routine for evt11	- 11 */	 
.global evt_evt11
evt_evt11:
	 SAVE_CONTEXT

	r0 = 11;
	r1 = SP;
	call	process_int; // process_int(vec, pt_regs);
evt_evt11_exit:
	RESTORE_CONTEXT
	rti; 

	/* interrupt routine for evt12	- 12 */	 
.global evt_evt12
evt_evt12:
	SAVE_CONTEXT

	r0 = 12;
	r1 = SP;
	call	process_int; // process_int(vec, pt_regs);
evt_evt12_exit:
	RESTORE_CONTEXT
	rti; 

	/* interrupt routine for evt13	- 13 */
.global evt_evt13
evt_evt13:
	SAVE_CONTEXT

	r0 = 13;
	r1 = SP;
	 
	call	process_int; // process_int(vec, pt_regs);
evt_evt13_exit:
	RESTORE_CONTEXT
	rti; 

	/* interrupt routine for system_call - 14 */	 
.global evt_system_call
evt_system_call:
	SAVE_CONTEXT
	call system_call;
	RESTORE_CONTEXT
	rti;
.global evt_soft_int1
evt_soft_int1:
	SAVE_CONTEXT

	r0 = IRQ_SW_INT1;	// Software Interrupt 1 
	r1 = SP;
	call	process_int; // process_int(vec, pt_regs);
evt_soft_int1_exit:
	RESTORE_CONTEXT
	rti; 
