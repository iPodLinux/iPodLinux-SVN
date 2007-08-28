//vic - change use of current_set to just current - needs verification
/*
 * Feb 26, 2001 Ken Hill
 *	Port to Nios architecture
 *
 * switch.S: Sparc task switch code.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#include <asm/cprefix.h>
#include <asm/psr.h>
#include <asm/asmmacro.h>
#include <asm/niosasm.h>
//vic#include <asm/ptrace.h>
#include <asm/winmacro.h>

#include "nios_defs.h"

#define sw_ntask     g0
#define sw_tmp       g1
#define sw_psr       g2
#define sw_epsr      g3

/* Context switch code.  The new process's task_struct
 * ptr is passed as the first parameter.
 *
 * First successful task switch 05/13/95 21:52:37
 */
	.align 4
	.globl	C_LABEL(nios_switch_to)
C_LABEL(nios_switch_to):
	mov	%sw_ntask, %o1

	/* Save kernel state. */
	
	pfx	8
	wrctl	%g5				; disable interrupts
	 nop
	 nop

	FLUSH_WINDOWS				/* flush reg file to stack */

	MOVIA	%sw_epsr, nios_status		/* get STATUS extension ptr */
	rdctl	%sw_psr				/* read STATUS (IE=0) */
	ANDIP	%sw_psr, 0xffff			/* clear upper */
	ld	%sw_tmp, [%sw_epsr]		/* get STATUS extension */
	ANDNIP	%sw_tmp, 0xffff			/* clear lower */
	or	%sw_psr, %sw_tmp
	mov	%sw_tmp, %o0			/* get 'prev' task ptr */
	STBO	%sw_tmp, (TASK_THREAD+THREAD_KPSR), %sw_psr	/* save STATUS & ESTATUS  */
	STBO	%sw_tmp, (TASK_THREAD+THREAD_KSP), %sp	/* save current stack ptr */
	STBO	%sw_tmp, (TASK_THREAD+THREAD_KPC), %o7	/* save current return pc */

	/* Load new kernel state. */

	MOVIA	%sw_tmp,C_LABEL(_current_task)	/* set new task as current */
	st	[%sw_tmp], %sw_ntask
	LDBO	%sw_psr, %sw_ntask, (TASK_THREAD+THREAD_KPSR)	/* get saved STATUS & ESTATUS */
	ANDNIP	%sw_psr, PSR_IE			/* Clear IE bit */
	wrctl	%sw_psr				/* set new STATUS (IE=0) */
	 nop
	 nop
	ANDNIP	%sw_psr, 0xffff			/* clear lower */
	st	[%sw_epsr], %sw_psr		/* set new STATUS extension */
	LDBO	%sp, %sw_ntask, (TASK_THREAD+THREAD_KSP)	/* set new stack ptr */
	LDBO	%o7, %sw_ntask, (TASK_THREAD+THREAD_KPC)	/* set new return pc */

	FILL_WINDOWS				/* fill reg file from stack */

	pfx	9				/* Ok to interrupt now */
	wrctl	%g0

	lret
	 nop





