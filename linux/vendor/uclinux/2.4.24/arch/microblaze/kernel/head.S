/*
 * arch/microblaze/kernel/head.S -- Lowest-level startup code
 *
 *  Copyright (C) 2004       Brett Boren <borenb@eng.uah.edu>
 *  Copyright (C) 2003       John Williams <jwilliams@itee.uq.edu.au>
 *                           based upon v850 version
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#include <asm/clinkage.h>
#include <asm/current.h>
#include <asm/entry.h>
#include <asm/irq.h>


/* Make a slightly more convenient alias for C_SYMBOL_NAME.  */
#define CSYM	C_SYMBOL_NAME 

.global CSYM(_ramstart)

	.data
CSYM(_ramstart):
	.long 0

	.text

	// Define `mach_early_init' as a weak symbol
	.global	CSYM(mach_early_init)
	.weak	CSYM(mach_early_init)

C_ENTRY(start):
	// Make sure interrupts are turned off, just in case
	mfs	r1, rmsr
	andi	r1, r1, ~2
	mts	rmsr, r1

	// Set Small Data Anchors
	la	r13, r0, _KERNEL_SDA_BASE_
	la	r2, r0, _KERNEL_SDA2_BASE_

	// Initialise the stack pointer
	// Initial stack is shrunk by 4 bytes (1 word) to permit use of
	// standard sp->task ptr conversion functions later on
	la	r1, r0, CSYM(init_task_union)+KERNEL_STACK_SIZE-4;

#ifdef CONFIG_MBVANILLA_CMDLINE
	// BAB 1/7/04 - Copy off cmdline pointer passed in by bootloader
	swi	r5, r0, CSYM(bootloader_buf_addr)
#endif	
	// See if there's a platform-specific early-initialization routine
	// defined; it's a weak symbol, so it will have an address of zero if
	// there's not.
	la	r6, r0, CSYM(mach_early_init)
	beqi	r6, 3f

	// There is one, so call it.  If this function is written in C, it
	// should be very careful -- the stack pointer is valid, but very
	// little else is (e.g., bss is not zeroed yet).
	brald	r15, r6			// do call
	nop				// delay slot

3:
	// Kernel stack pointer save location
	swi	r1, r0, KSP

	// Assert that we're in `kernel mode'
	addi	r19, r0, 1
	swi	r19, r0, KM

#if 0
	// Zero bss area, since we can't rely upon any loader to do so
	addi	r5, r0, CSYM(_sbss)
	add	r6, r0, r0
	addi	r7, r0, CSYM(_ebss)
	rsub	r7, r5, r7
	bralid	r15, CSYM(memset)
	nop

	// Zero sbss area, since we can't rely upon any loader to do so
	addi	r5, r0, CSYM(_ssbss)
	add	r6, r0, r0
	addi	r7, r0, CSYM(_esbss)
	rsub	r7, r5, r7
	bralid	r15, CSYM(memset)
	nop
#endif

	// initialise the current task ptr.  This must happen after
	// we zero BSS, SBSS etc, because "current" is a global C symbol
	GET_CURRENT_TASK(CURRENT_TASK);

	// Start Linux kernel.
	// If this call ever returns (which it shouldn't),
	// set the link pointer to somewhere harmless
	addi	r15, r0, CSYM(machine_halt)
	brai	CSYM(start_kernel)
END(start)
