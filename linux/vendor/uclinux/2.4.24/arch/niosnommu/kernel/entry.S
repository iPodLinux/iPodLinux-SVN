/*
 * arch/niosnommu/kernel/entry.S:  NIOS trap low-level entry points.
 *
 * Copyright (C) 2001 Vic Phillips (vic@microtronix.com)
 *
 * based on sparcnommu
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/sys.h>
#include <linux/config.h>
#include <linux/errno.h>

#include "nios_defs.h"

#include <asm/niosasm.h>
#include <asm/ptrace.h>
#include <asm/cprefix.h>
#include <asm/winmacro.h>
#include <asm/asmmacro.h>



	.macro	SAVE_LAST_WINDOW

	pfx	8
	wrctl	%g5				; disable interrupts
	 nop
	 nop

;	pfx	0
	rdctl	%g5				; read STATUS
	ANDIP	%g5, PSR_CWP	; isolate CWP

	pfx	2
	rdctl	%g4				; read WVALID
	lsri	%g4, 1			; get HI_LIMIT into CWP position
	ANDIP	%g4, PSR_CWP	; isolate it

	GET_CURRENT	g6
	ADDIP %g6, TASK_THREAD+THREAD_SPARE_W_SAVED
	st [%g6], %fp			; save sp for parent

	SUBIP %g6, THREAD_SPARE_W_SAVED-THREAD_SPARE_REG_WINDOW

	cmp	%g5, %g4			; reached HI_LIMIT yet ?
	BNE	6f					; No
	nop

	; copy l, i regs from [fp] to [g6]


	mov %g4, %fp
	movi %g7, 16
5:	
	; for (i=0;i<g7;i++) [g4++]->[g6++]
	ld %g5, [%g4]		
	st [%g6], %g5
	addi %g4, 4
	subi %g7, 1
	BNE 5b
	addi %g6, 4				; delay slot

	br 7f	
	nop

6:
	; copy 1, i regs from last reg windows to [g6]
	mov %g5, %sp
	restore
	mov %g4, %sp			; save sp
	mov %sp, %g6			; 
	STORE_WINDOW
	mov %sp, %g4
	save %sp, 0
	mov %sp, %g5
7:
	pfx	9
	wrctl	%g5				; enable interrupts

	.endm



	.macro	LOAD_LAST_WINDOW
	pfx	8
	wrctl	%g5				; disable interrupts
	 nop
	 nop

	; copy back the registers from thread_struct to user stack


	GET_CURRENT	g6
	ADDIP %g6, TASK_THREAD+THREAD_SPARE_W_SAVED
	ld %fp, [%g6]			; get the saved sp
	SUBIP %g6, THREAD_SPARE_W_SAVED-THREAD_SPARE_REG_WINDOW


	mov %g4, %fp
	movi %g7, 16
3:	
	; for (i=0;i<g7;i++) [g6++]->[g4++]
	ld %g5, [%g6]		
	st [%g4], %g5
	addi %g4, 4
	subi %g7, 1
	BNE 3b
	addi %g6, 4				; delay slot

;	pfx	0
	rdctl	%g5				; read STATUS
	ANDIP	%g5, PSR_CWP	; isolate CWP

	pfx	2
	rdctl	%g4				; read WVALID
	lsri	%g4, 1			; get HI_LIMIT into CWP position
	ANDIP	%g4, PSR_CWP	; isolate it

	cmp	%g5, %g4			; reached HI_LIMIT yet ?
	BEQ	4f					; No
	nop

	; refresh the last register window
	mov %g5, %sp
	restore
	LOAD_WINDOW
	save %sp,0
	mov %sp, %g5

4:
	pfx	9
	wrctl	%g5				; enable interrupts

	.endm
	

/* define a macro for calling syscall_trace */
	.macro	SYS_CALL_TRACE
#if 0
	GET_CURRENT	l3
	ldp	%l3, [%l3, TASK_FLAGS/4]
	skp1	%l3, 5				; 0x20 = bit 5
	br	ret\@
	 nop
	MOVIA	%l3,C_LABEL(syscall_trace)@h
	call	%l3
	 nop
ret\@:
#endif
	.endm

/* All trap entry points _must_ begin with this macro or else you
 * lose.  It makes sure the kernel has a proper window so that
 * c-code can be called.
 */
.macro	SAVE_ALL
	MOVIA	%l7, ret\@@h
	br	trap_setup
	 nop
ret\@:
.endm

/* All traps low-level code here must end with this macro.
 */
.macro	RESTORE_ALL
	br	ret_trap_entry
	 nop
.endm


	.text
	/* fs/binfmt_flat.c loader debug trap handler.
	 * Set debug in the current task's thread flags.
	 * This flags binfmt_flat loader to cause a gdb trap when
	 * the application starts.
	 */
	.align	2
	.globl	binfmt_flat_debug_handler
binfmt_flat_debug_handler:

	/* WARNING! Never, ever, put a GDB breakpoint (or single step thru)
	 *          here!  The bpt trap will stomp the ISTATUS.
	 */
	LOAD_CURRENT	l3
	ADDIP	%l3, TASK_THREAD+THREAD_FLAGS
	ld	%l2, [%l3]
	ORIP	%l2, NIOS_FLAG_DEBUG
	st	[%l3], %l2
	tret	%o7

	
.ifdef na_ide_interface
	.align	2
	.globl	ocidec_handler
ocidec_handler:

	/* WARNING! Never, ever, put a GDB breakpoint (or single step thru)
	 *          here!  The bpt trap will stomp the ISTATUS.
	 */

	MOVIP	%l0,na_ide_interface+4	; get ocidec address - status word
	movi	%l1,0			; write zero to clear interrupt
	st	[%l0],%l1		; 
	br	real_irq_entry		; continue with common interrupt handling
	 nop				; 
.endif

	.align	2
	.globl	timer_handler
timer_handler:

	/* WARNING! Never, ever, put a GDB breakpoint (or single step thru)
	 *          here!  The bpt trap will stomp the ISTATUS.
	 */

	MOVIP	%o1,na_timer0		; get timer address
	movi	%o0,0			; 
	pfx	np_timerstatus		;
	st	[%o1],%o0		; clear interrupt condition
;;;	br	real_irq_entry		; continue with common interrupt handling
;;;	 nop				; 


/* For now all IRQ's not registered get sent here. handler_irq() will
 * see if a routine is registered to handle this interrupt and if not
 * it will say so on the console.
 */

	.align	2
	.globl	real_irq_entry
real_irq_entry:

	/* WARNING! Never, ever, put a GDB breakpoint (or single step thru)
	 *          here!  The bpt trap will stomp the ISTATUS.
	 */
	SAVE_ALL			; common interrupt setup

	/* Enable under/over traps w/IRQs off, so we can call c-code properly. */
;;;	pfx	0			;
	rdctl	%g0			; get status
	mov	%o0, %g0		; copy to preserve IPRI
	ANDNIP	%g0, PSR_IPRI		; remove old IPRI
	ORIP	%g0, PSR_IE | PSR_IPRI_3
;;;	pfx	0			;
	wrctl	%g0			; reset to allow only o/u/flow traps
	 nop
	 nop

	ANDIP	%o0, PSR_IPRI		; isolate the IPRI
	mov	%o1, %sp
	ADDIP	%o1, REGWIN_SZ		; arg1 = pt_regs ptr
	MOVIA	%l5, C_LABEL(process_int)@h
	call	%l5
	 lsri	%o0, 9			; arg0 = vector/irq #

	RESTORE_ALL


	/* This routine handles invalid traps/interrupts
	 */
	.align	2
	.globl	bad_trap_handler
bad_trap_handler:

	/* WARNING! Never, ever, put a GDB breakpoint (or single step thru)
	 *          here!  The bpt trap will stomp the ISTATUS.
	 */
	SAVE_ALL
;;;	pfx	0			;
	rdctl	%g0			; get status
	mov	%o0, %g0		; copy to preserve IPRI
	ANDNIP	%g0, PSR_IPRI		; remove old IPRI
	ORIP	%g0, PSR_IE | PSR_IPRI_3
;;;	pfx	0			;
	wrctl	%g0			; to allow only o/u/f traps
	 nop
	 nop
	ANDIP	%o0, PSR_IPRI		; isolate the IPRI
	lsri	%o0, 9					; arg0 = vector/irq #
	lds	%o1, [%sp, (REGWIN_SZ + PT_PSR)/4]	; arg1 = psr
	MOVIA	%l5, C_LABEL(do_hw_interrupt)@h
	call	%l5
	 lds	%o2, [%sp, (REGWIN_SZ + PT_PC)/4]	; arg2 = pc
	RESTORE_ALL


	/* Linux native system calls enter here... */

	/* inputs are:
	 *  %g1 = syscall #
	 *  %i0 = first argument (if any)
	 *  %i1 = second argument (if any)
	 *  %i2 = third argument (if any)
	 *  %i3 = fourth argument (if any)
	 *  %i4 = fifth argument (if any)
	 *  %i5 = sixth argument (if any)
	 *
	 *  %i6 = %fp
	 *
	 *  %o7 = return address
	 *
	 */

	.align	2
	.globl	C_LABEL(system_call)
C_LABEL(system_call):
	/* TODO: When we have ptrace working test for PF_TRACESYS in task flags. */

	SAVE_ALL				; common interrupt setup (%g1 preserved)

/* FIXME - What if IPRI has IRQ's blocked when the syscall trap was done ?
 *         Doesn't this mean we're enabling interrupts prematurely?
 *         Maybe we should restore IPRI from ISTATUS.
 */
	pfx	9				; 
	wrctl	%g0				; enable interrupts

	CMPIP	%g1, NR_syscalls		; syscall # in range ?
	ifs	cc_lt
	br	1f
	 lsli	%g1,2				;  (delay slot) index * 4
	
	MOVIA	%l7,C_LABEL(sys_ni_syscall)@h	; No, use "not implemented"
	br	2f
	 nop
1:
	MOVIA	%l7,sys_call_table		; Yes, get ptr to syscall table
	add	%l7, %g1			; get table entry
	ld	%l7, [%l7] 
	sts	[%sp, (REGWIN_SZ + PT_SYSC_NO)/4], %g1	; save syscall # (checked by do_signal)
2:
	SYS_CALL_TRACE

	lds	%o0, [%sp, (REGWIN_SZ + PT_I0)/4]	; get args into %o0-5
	lds	%o1, [%sp, (REGWIN_SZ + PT_I1)/4]
	lds	%o2, [%sp, (REGWIN_SZ + PT_I2)/4]
	lds	%o3, [%sp, (REGWIN_SZ + PT_I3)/4]
	lds	%o4, [%sp, (REGWIN_SZ + PT_I4)/4]
	lds	%o5, [%sp, (REGWIN_SZ + PT_I5)/4]

	sts	[%sp, (REGWIN_SZ + PT_ORIG_I0)/4], %o0	; save first arg for restarting syscalls

	call	%l7					; call syscall handler
	 nop

	sts	[%sp, (REGWIN_SZ + PT_I0)/4], %o0	; save the returned arg


	.globl	C_LABEL(ret_sys_call)
C_LABEL(ret_sys_call):
	lds	%o0, [%sp, (REGWIN_SZ + PT_I0)/4]	; retrieve returned arg
	SYS_CALL_TRACE
	RESTORE_ALL



/* At trap time, interrupts and all generic traps do the following:
 *
 * SAVE_ALL macro which does:
 *
 * 	MOVIA	%l7,return_address@h
 * 	br	trap_setup
 * 	 nop
 * return_address:
 *
 * The entering values are:
 *
 *  %g1 = possible syscall #
 *  %i0 - %i5 - possible syscall arguments
 *  %fp = interruptee's stack ptr
 *  %l7 = return address of caller
 *
 * The returning values are:
 *
 *  %g1 = possible syscall # (preserved)
 *  %sp = kernel stack ptr with PT_REGS structure stored
 *
 */

	.align 2
	.globl	C_LABEL(trap_setup)
C_LABEL(trap_setup):

	pfx	1
	rdctl	%l1				; read ISTATUS (interruptee's psr)
	ext16s	%l1, 0				; clear upper part

	MOVIA	%l0, nios_status		; get STATUS extension
	ld	%l2, [%l0]			; 
	ANDNIP	%l2, 0xffff			; clear lower stuff
	or	%l1, %l2			; save as extended ISTATUS
	bgen	%l3, PSR_SUPERVISOR_BIT		; set supervisor mode
	st	[%l0], %l3			; into current extended STATUS

	/* See if from user or kernel */
	skp0	%l2, PSR_SUPERVISOR_BIT		; interrupted mode = supervisor ?
	br	1f				; yes, from kernel
	 mov	%sp, %fp			; assume from kernel: get kernel stack ptr
	LOAD_CURRENT	l0			; from user: get kernel stack pointer
	ADDIP	%l0,THREAD_SIZE			;vic; start from the top of stack again ?
	mov	%sp, %l0			;vic;

1:
	SUBIP	%sp, REGWIN_SZ+TRACEREG_SZ	; allocate more stack
	STORE_PT_ALL	l1, o7			; save interruptee's %g's, %o's, STATUS, return address

	bgen	%g0, 0
	neg	%g0				; -1 -> g0
	sts	[%sp, (REGWIN_SZ + PT_SYSC_NO)/4], %g0	; init the place of saved syscall no by -1
	
	/* See if we are in the last window (LO_LIMIT-1). */
;;;	pfx	0
	rdctl	%g0				; read STATUS (interrupt psr)
	ANDIP	%g0, PSR_CWP			; isolate CWP

	pfx	2
	rdctl	%g4				; read WVALID
	mov	%g3, %g4			; get LO_LIMIT into CWP position
	lsli	%g3, 4
	ANDIP	%g3, PSR_CWP			; isolate it
	cmp	%g0, %g3			; CWP < LO_LIMIT ?
	BLT	2f				; Yes - big overhead coming
	 nop
	jmp	%l7				; return to caller
	 nop

	/* In last window (LO_LIMIT-1) - need to flush and reopen register file.
	 * The current window will be switched to HI_LIMIT and left pointing
	 * at the same stack frame.  The only other register that needs to be
	 * preserved and restored into the new window is the frame ptr (%fp).
	 * No other %o or %l registers need to be preserved as these are ours
	 * only and we really haven't used them yet.  The other %i registers
	 * don't need to be preserved here since they are saved and restored
	 * from the pt_regs frame.  So we start flushing register windows into
	 * the stack frames starting with LO_LIMIT and ending at HI_LIMIT.
	 */
2:
	mov	%g6, %sp			; save stack ptr
	mov	%g7, %l7			; and caller's return address

	lsri	%g4, 1				; get HI_LIMIT into CWP position
	ANDIP	%g4, PSR_CWP			; isolate it
	STORE_WINDOW				; save %i's & %l's in register window
3:
	restore					; CWP ++
	STORE_WINDOW				; save %i's & %l's from this register window
	addi	%g0, 0x10			; step our value for CWP
	cmp	%g0, %g4			; reached HI_LIMIT yet ?
	BNE	3b				; No
	 nop
						; Yes, CWP = HI_LIMIT
	mov	%sp, %g6			; restore stack pointer into it
	LOAD_WINDOW				; restore %i's & %l's into this window
	jmp	%g7				; return to caller (saved %l7)
	 nop


/* All interrupt routines return thru here via RESTORE_ALL
 *
 * We need to check for a few things which are:
 * 1) The need to call schedule() because this process's quantum is up.
 * 2) Pending signals for this process.  If any exist we need to call
 *     do_signal() to do the needy.
 *
 * Else we just check if the tret would land us in an invalid window,
 * if so we need to refill the register file from the user/kernel stack first.
 */

	.align 2
	.globl	ret_trap_entry
ret_trap_entry:

#if 0
	/* disable other interrupts */
;;;	pfx	0			;
	rdctl	%g0			; get status
	ANDNIP	%g0, PSR_IPRI		; remove old IPRI
	ORIP	%g0, PSR_IE | PSR_IPRI_3
;;;	pfx	0			;
	wrctl	%g0			; reset to allow only o/u/flow traps
	 nop
	 nop
#endif

	lds	%l5, [%sp, (REGWIN_SZ + PT_PSR)/4]	; get ISTATUS
	skp0	%l5, PSR_SUPERVISOR_BIT		; from supervisor ?
	br	ret_trap_kernel			; Yes
	 nop
	
	GET_CURRENT	l3			; from user - check scheduling
	ldp	%l4, [%l3, TASK_NEED_RESCHED/4]
	skprnz	%l4
	br	signal_p
	 nop
	MOVIA	%l6, C_LABEL(schedule)@h
	call	%l6
	 nop

	/* Try to return again... We are a different process,
	 * most likely so load and then check if going back
	 * to user or kernel this time.
	 */
	br	ret_trap_entry
	 nop

signal_p:
	/* No signals for swapper. */
	GET_CURRENT	l3

	MOVIA	%l5, C_LABEL(init_task_union)
	cmp	%l5, %l3		; task 0 ?
	BEQ	ret_trap_continue	; Yes
	 nop

	ldp	%l4, [%l3, TASK_SIGPENDING/4]
	skprnz	%l4
	 br	ret_trap_continue
	 xor  %o0, %o0			; oldset shall be NULL when called from here
	mov	%o1, %sp
	ADDIP	%o1, REGWIN_SZ		; pt_regs ptr
	MOVIA	%l6, C_LABEL(do_signal)@h
	call	%l6
	 nop
	/* Fall through... */

ret_trap_continue:
ret_trap_kernel:
	pfx	8
	wrctl	%g0			; disable interrupts
	 nop
	 nop

	lds	%l1, [%sp, (REGWIN_SZ + PT_PSR)/4]  ; get saved ISTATUS
	ANDIP	%l1, PSR_CWP		; isolate saved CWP
;;;	pfx	0
	rdctl	%g0			; read STATUS (note IE = 0)
	mov	%g1, %g0		
	ANDIP	%g1, PSR_CWP		; isolate CWP

	subi	%l1, 0x0010		; decrement saved CWP to what it
					;   would have been after the trap
	cmp	%g1, %l1		; this should equal current CWP
	BEQ	4f			; it does ... whew!
	 nop

	/* The CWP being returned to is not one-off the current
	 * find out if the original trap did a flush.
	 */

	pfx	2
	rdctl	%g3			; read WVALID
	mov	%g4, %g3		; get HI_LIMIT into CWP position
	lsri	%g4, 1			; 
	ANDIP	%g4, PSR_CWP		; isolate it
	lsli	%g3, 4			; Get LO_LIMIT into CWP position
	ANDIP	%g3, PSR_CWP		; isolate it
	subi	%g3, 0x0010		; set to LO_LIMIT-1

	cmp	%l1, %g3		; saved CWP-1 = LO_LIMIT-1 ?
	BNE	1f			; No
	 nop
	cmp	%g1, %g4		; CWP = HI_LIMIT ?
	BEQ	2f			; Yes
	 nop
1:
	trap	5			;vic; yikes ! panic time ! what's wrong here ?
2:

	/* Refill register file from corresponding stacked register
	 * windows.  At this point, saved ISTATUS has CWP = LO_LIMIT
	 * which means the trap that got us here had to flush the
	 * register file to the stack and reopen the file to the top
	 * and CWP went from LO_LIMIT-1 to HI_LIMIT where we are now.
	 *
	 * ie, STATUS.CWP = HI_LIMIT and must end up at LO_LIMIT-1
	 *            to which the PT_REGS.i are restored
	 * so that the tret restoring ISTATUS.CWP = LO_LIMIT has the
	 *            %o's in the right place.
	 */

	; %g0 = current STATUS
	; %g1 = current CWP
	; %sp -> pt_regs


	; transfer window from HI_LIMIT to LO_LIMIT-1
	; then climb the reg file

#ifdef DEBUG
	/* What if current %fp is not the same as the pt_fp ? */
	lds	%l6, [%sp, (REGWIN_SZ + PT_I6)/4]
	cmp	%fp, %l6		; the same?
	skps	cc_z			; yes
	trap	5			; no, call me
#endif
	mov	%g6, %sp		; save stack ptr

	ANDNIP	%g0, PSR_CWP		; remove current CWP from STATUS value
	mov	%g1, %g3		; replace with LO_LIMIT - 1 
	or	%g0, %g1		;
;;;	pfx	0			;
	wrctl	%g0			; set new CWP
	 nop
	 nop
	mov	%sp, %g6		; transfer stack ptr into it
3:
	LOAD_WINDOW			; refill window from corresponding stack	
	restore				; goto next window
	cmp	%g1, %g4		; CWP = HI_LIMIT ?
	BNE	3b			; nope
	 addi	%g1, 0x0010		; bump to next window

	ANDNIP	%g0, PSR_CWP		; remove current CWP from STATUS value
	or	%g0, %g3		; replace with LO_LIMIT - 1 
;;;	pfx	0			;
	wrctl	%g0			; set CWP to LO_LIMIT-1 again
	 nop
	 nop
	mov	%sp, %g6		; retrieve stack ptr
4:
	LOAD_PT_ALL	l1, o7		; reload g's & i's, get ISTATUS & ret addr

	/* WARNING! Never, ever, put a GDB breakpoint (or single step thru)
	 *          here!  The bpt trap will stomp the ISTATUS.
	 */
	pfx	1
	wrctl	%l1			; write ISTATUS (for tret to make current)
	ANDNIP	%l1, 0xffff		; clear lower part
	MOVIA	%l0, nios_status	; set STATUS extension
	st	[%l0], %l1		;
	tret	%o7			; return to whoever (could be task switch)
	nop


	/* Note how we really return to ret_sys_call because we share the
	 * register window with our caller.
	 */

	.align	2
	.globl	C_LABEL(sys_ptrace)
C_LABEL(sys_ptrace):
	mov	%o0, %sp
	ADDIP	%o0, REGWIN_SZ			; pt_regs *regs arg
	MOVIA	%l7, C_LABEL(do_ptrace)@h
	call	%l7
	 nop
	SYS_CALL_TRACE
	RESTORE_ALL


	.align	2
	.globl	C_LABEL(sys_execve)
C_LABEL(sys_execve):
	;; xwt: here we move the current window to the high limit
	pfx	8
	wrctl	%g5				; disable interrupts
	 nop
	 nop
	FLUSH_WINDOWS
	
	mov		%g6, %sp;			; keep %sp
	
	;; change CWP
	pfx	2
	rdctl	%g4				; read WVALID
	lsri	%g4, 1			; get HI_LIMIT into CWP position
	MOVIP  %g7, PSR_CWP
	AND 	%g4, %g7	; isolate it
	;pfx	0
	rdctl	%g5				; read STATUS
	ANDN   %g5, %g7			; clear CWP
	or	   %g5, %g4
	;pfx	0
	wrctl	%g5				; update STATUS
	 nop
	 nop

	mov		%sp, %g6		; read back %sp, output regs
	lds	%i6, [%sp, RW_I6/4]	; LOAD_WINDOW is OK, but actually only %fp is enough


	pfx	9
	wrctl	%g5				; enable interrupts
	;; xwt: ok, we can go nios_execve.


	mov	%o0, %sp
	ADDIP	%o0, REGWIN_SZ			; pt_regs *regs arg
	MOVIA	%l7, C_LABEL(nios_execve)@h
	call	%l7
	 nop
	sts	[%sp,(REGWIN_SZ + PT_I0)/4],%o0	; save the returned arg

	;; xwt: if nios_execve succeeds, we move the current window
	;;      to HI_LIMIT-1, otherwise we restore the saved CWP
	pfx	8
	wrctl	%g5				; disable interrupts
	 nop
	 nop

	MOVIP  %g7, PSR_CWP
	lds    %g4, [%sp, (REGWIN_SZ + PT_PSR)/4]		; get saved ISTATUS
	and    %g4, %g7			; get ICWP

	mov		%g6, %sp;			; keep %sp

	pfx	2
	rdctl	%g5				; read WVALID
	mov		%g3, %g5
	lsli	%g3, 4			; get LO_LIMIT into CWP position
	and		%g3, %g7		; isolate it
	cmp		%g4, %g3		; ICWP == LO_LIMIT ?
	BNE	2f
	nop

	lsri	%g5, 1			; get HI_LIMIT into CWP position
	and		%g5, %g7		; isolate it
	br		3f
	mov		%g4, %g5		; delay slot

2:
	SUBIP  %g4, 0x0010		; get CWP we shall change to
3:
	;pfx	0
	rdctl	%g5				; read STATUS
;;	MOVIP  %g7, PSR_CWP
	ANDN   %g5, %g7			; clear CWP
	or	   %g5, %g4
	;pfx	0
	wrctl	%g5				; update STATUS
	 nop
	 nop

	mov		%sp, %g6		; read back %sp, output regs

	FILL_WINDOWS
	pfx	9
	wrctl	%g5				; enable interrupts
	;; xwt: ok, we can continue

	br	ret_sys_call			;
	 nop


	.align	2
	.globl	C_LABEL(sys_sigaltstack)
C_LABEL(sys_sigaltstack):
	/* Note: %o0, %o1 (arg0, arg1) already have correct value... */
//vic FIXME  is this right ?
	lds	%o2, [%sp, (REGWIN_SZ + PT_I6)/4] ; arg2 = user's stack (fp)
	MOVIA	%l7, C_LABEL(do_sigaltstack)@h
	call	%l7
	 nop
	SYS_CALL_TRACE
	/* We are returning to a signal handler. */
	RESTORE_ALL


	.align	2
	.globl	C_LABEL(sys_sigsuspend)
C_LABEL(sys_sigsuspend):
	/* Note: %o0 (arg0) already has correct value... */
	mov	%o1, %sp
	ADDIP	%o1, REGWIN_SZ			; arg1 = pt_regs *regs arg
	MOVIA	%l7, C_LABEL(do_sigsuspend)@h
	call	%l7
	 nop
	SYS_CALL_TRACE
	/* We are returning to a signal handler. */
	RESTORE_ALL


	.align	2
	.globl	C_LABEL(sys_rt_sigsuspend)
C_LABEL(sys_rt_sigsuspend):
	/* Note: %o0, %o1 (arg0, arg1) already have correct value... */
	mov	%o2, %sp
	ADDIP	%o2, REGWIN_SZ			; arg 2 = pt_regs *regs arg
	MOVIA	%l7, C_LABEL(do_rt_sigsuspend)@h
	call	%l7
	 nop
	SYS_CALL_TRACE
	/* We are returning to a signal handler. */
	RESTORE_ALL


	.align	2
	.globl	C_LABEL(sys_sigreturn)
C_LABEL(sys_sigreturn):
	mov	%o0, %sp
	ADDIP	%o0, REGWIN_SZ			; pt_regs *regs arg
	MOVIA	%l7, C_LABEL(do_sigreturn)@h
	call	%l7
	 nop
	SYS_CALL_TRACE
	/* We don't want to muck with user registers like a
	 * normal syscall, just return.
	 */
	RESTORE_ALL


	.align	2
	.globl	C_LABEL(sys_rt_sigreturn)
C_LABEL(sys_rt_sigreturn):
	mov	%o0, %sp
	ADDIP	%o0, REGWIN_SZ			; pt_regs *regs arg
	MOVIA	%l7, C_LABEL(do_rt_sigreturn)@h
	call	%l7
	 nop
	SYS_CALL_TRACE
	/* We don't want to muck with user registers like a
	 * normal syscall, just return.
	 */
	RESTORE_ALL


	.align	2
	.globl	C_LABEL(sys_fork)
C_LABEL(sys_fork):
	.globl	C_LABEL(sys_vfork)
C_LABEL(sys_vfork):
	
	SAVE_LAST_WINDOW
;;;	pfx	0
	rdctl	%g4				; Get STATUS
	ANDIP	%g4, 0xffff			; clear upper stuff
#if 1
	pfx	8
	wrctl	%g0				; disable interrupts
	 nop
	 nop
#endif
	STORE_WINDOW

	MOVIA	%l2, nios_status		; add STATUS extension
	ld	%l2, [%l2]			; 
	ANDNIP	%l2, 0xffff			; clear lower stuff
	or	%g4, %l2			;

	GET_CURRENT	g6
	ADDIP %g6, TASK_THREAD+THREAD_FORK_KPSR
	st	[%g6], %g4
#if 1
	pfx	9
	wrctl	%g0				; enable interrupts
#endif
	MOVIA	%o0, SIGCHLD | CLONE_VM | CLONE_VFORK	; arg0:	clone flags
	lds	%o1, [%sp, (REGWIN_SZ + PT_FP)/4]	; arg1:	usp
	mov	%o2, %sp
	ADDIP	%o2, REGWIN_SZ				; arg2:	pt_regs ptr
	movi	%o3, 0					; arg3: stack size (not used)
	MOVIA	%g5, C_LABEL(do_fork)@h
	call	%g5
	 nop
	sts	[%sp, (REGWIN_SZ + PT_I0)/4], %o0	; save the returned arg
	LOAD_LAST_WINDOW
	br	ret_sys_call				;
	 nop
	

	.align	2
	.globl	C_LABEL(sys_clone)
C_LABEL(sys_clone):

;;;	pfx	0
	rdctl	%g4				; get STATUS
	ANDIP	%g4, 0xffff			; clear upper stuff
#if 1
	pfx	8
	wrctl	%g0				; disable interrupts
	 nop
	 nop
	FLUSH_WINDOWS
#endif
	MOVIA	%l2, nios_status		; add STATUS extension
	ld	%l2, [%l2]			; 
	ANDNIP	%l2, 0xffff			; clear lower stuff
	or	%g4, %l2			; 

	GET_CURRENT	g6
	ADDIP %g6, TASK_THREAD+THREAD_FORK_KPSR
	st	[%g6], %g4
#if 1
	pfx	9
	wrctl	%g0				; enable interrupts
#endif
	lds	%o0, [%sp, (REGWIN_SZ + PT_I0)/4]	; arg0: flags
	lds	%o1, [%sp, (REGWIN_SZ + PT_I1)/4]	; arg1: usp
	cmpi	%o1, 0x0				; Is new_usp NULL?
	skps	cc_ne
	lds	%o1, [%sp, (REGWIN_SZ + PT_FP)/4]	; yes, use current usp
	mov	%o2, %sp
	ADDIP	%o2, REGWIN_SZ				; arg2:	pt_regs ptr
	movi	%o3, 0					; arg3: stack size (not used)

	MOVIA	%g5, C_LABEL(do_fork)@h
	call	%g5
	 nop
	sts	[%sp, (REGWIN_SZ + PT_I0)/4], %o0	; save the returned arg
	br	ret_sys_call				;
	 nop


	.data
	.align	2
	.globl C_LABEL(sys_call_table)
C_LABEL(sys_call_table):
	.long C_LABEL(sys_ni_syscall)@h		/* 0  -  old "setup()" system call*/
	.long C_LABEL(sys_exit)@h
	.long C_LABEL(sys_fork)@h
	.long C_LABEL(sys_read)@h
	.long C_LABEL(sys_write)@h
	.long C_LABEL(sys_open)@h		/* 5 */
	.long C_LABEL(sys_close)@h
	.long C_LABEL(sys_waitpid)@h
	.long C_LABEL(sys_creat)@h
	.long C_LABEL(sys_link)@h
	.long C_LABEL(sys_unlink)@h		/* 10 */
	.long C_LABEL(sys_execve)@h
	.long C_LABEL(sys_chdir)@h
	.long C_LABEL(sys_time)@h
	.long C_LABEL(sys_mknod)@h
	.long C_LABEL(sys_chmod)@h		/* 15 */
	.long C_LABEL(sys_chown)@h
	.long C_LABEL(sys_ni_syscall)@h				/* old break syscall holder */
	.long C_LABEL(sys_stat)@h
	.long C_LABEL(sys_lseek)@h
	.long C_LABEL(sys_getpid)@h		/* 20 */
	.long C_LABEL(sys_mount)@h
	.long C_LABEL(sys_umount)@h
	.long C_LABEL(sys_setuid)@h
	.long C_LABEL(sys_getuid)@h
	.long C_LABEL(sys_stime)@h		/* 25 */
	.long C_LABEL(sys_ptrace)@h
	.long C_LABEL(sys_alarm)@h
	.long C_LABEL(sys_fstat)@h
	.long C_LABEL(sys_pause)@h
	.long C_LABEL(sys_utime)@h		/* 30 */
	.long C_LABEL(sys_ni_syscall)@h				/* old stty syscall holder */
	.long C_LABEL(sys_ni_syscall)@h				/* old gtty syscall holder */
	.long C_LABEL(sys_access)@h
	.long C_LABEL(sys_nice)@h
	.long C_LABEL(sys_ni_syscall)@h		/* 35 */	/* old ftime syscall holder */
	.long C_LABEL(sys_sync)@h
	.long C_LABEL(sys_kill)@h
	.long C_LABEL(sys_rename)@h
	.long C_LABEL(sys_mkdir)@h
	.long C_LABEL(sys_rmdir)@h		/* 40 */
	.long C_LABEL(sys_dup)@h
	.long C_LABEL(sys_pipe)@h
	.long C_LABEL(sys_times)@h
	.long C_LABEL(sys_ni_syscall)@h				/* old prof syscall holder */
	.long C_LABEL(sys_brk)@h		/* 45 */
	.long C_LABEL(sys_setgid)@h
	.long C_LABEL(sys_getgid)@h
	.long C_LABEL(sys_signal)@h
	.long C_LABEL(sys_geteuid)@h
	.long C_LABEL(sys_getegid)@h		/* 50 */
	.long C_LABEL(sys_acct)@h
	.long C_LABEL(sys_umount)@h				/* recycled never used phys() */
	.long C_LABEL(sys_ni_syscall)@h				/* old lock syscall holder */
	.long C_LABEL(sys_ioctl)@h
	.long C_LABEL(sys_fcntl)@h		/* 55 */
	.long C_LABEL(sys_ni_syscall)@h				/* old mpx syscall holder */
	.long C_LABEL(sys_setpgid)@h
	.long C_LABEL(sys_ni_syscall)@h				/* old ulimit syscall holder */
	.long C_LABEL(sys_ni_syscall)@h
	.long C_LABEL(sys_umask)@h		/* 60 */
	.long C_LABEL(sys_chroot)@h
	.long C_LABEL(sys_ustat)@h
	.long C_LABEL(sys_dup2)@h
	.long C_LABEL(sys_getppid)@h
	.long C_LABEL(sys_getpgrp)@h		/* 65 */
	.long C_LABEL(sys_setsid)@h
	.long C_LABEL(sys_sigaction)@h
	.long C_LABEL(sys_sgetmask)@h
	.long C_LABEL(sys_ssetmask)@h
	.long C_LABEL(sys_setreuid)@h		/* 70 */
	.long C_LABEL(sys_setregid)@h
	.long C_LABEL(sys_sigsuspend)@h
	.long C_LABEL(sys_sigpending)@h
	.long C_LABEL(sys_sethostname)@h
	.long C_LABEL(sys_setrlimit)@h		/* 75 */
	.long C_LABEL(sys_old_getrlimit)@h
	.long C_LABEL(sys_getrusage)@h
	.long C_LABEL(sys_gettimeofday)@h
	.long C_LABEL(sys_settimeofday)@h
	.long C_LABEL(sys_getgroups)@h		/* 80 */
	.long C_LABEL(sys_setgroups)@h
	.long C_LABEL(old_select)@h
	.long C_LABEL(sys_symlink)@h
	.long C_LABEL(sys_lstat)@h
	.long C_LABEL(sys_readlink)@h		/* 85 */
	.long C_LABEL(sys_uselib)@h
	.long C_LABEL(sys_swapon)@h
	.long C_LABEL(sys_reboot)@h
	.long C_LABEL(old_readdir)@h
	.long C_LABEL(old_mmap)@h		/* 90 */
	.long C_LABEL(sys_munmap)@h
	.long C_LABEL(sys_truncate)@h
	.long C_LABEL(sys_ftruncate)@h
	.long C_LABEL(sys_fchmod)@h
	.long C_LABEL(sys_fchown)@h		/* 95 */
	.long C_LABEL(sys_getpriority)@h
	.long C_LABEL(sys_setpriority)@h
	.long C_LABEL(sys_ni_syscall)@h				/* old profil syscall holder */
	.long C_LABEL(sys_statfs)@h
	.long C_LABEL(sys_fstatfs)@h		/* 100 */
	.long C_LABEL(sys_ioperm)@h
	.long C_LABEL(sys_socketcall)@h
	.long C_LABEL(sys_syslog)@h
	.long C_LABEL(sys_setitimer)@h
	.long C_LABEL(sys_getitimer)@h		/* 105 */
	.long C_LABEL(sys_newstat)@h
	.long C_LABEL(sys_newlstat)@h
	.long C_LABEL(sys_newfstat)@h
	.long C_LABEL(sys_ni_syscall)@h		//vic old sys_uname
	.long C_LABEL(sys_ni_syscall)@h		/* iopl for i386 */ /* 110 */
	.long C_LABEL(sys_vhangup)@h
	.long C_LABEL(sys_ni_syscall)@h		/* obsolete idle() syscall */
	.long C_LABEL(sys_ni_syscall)@h		/* vm86 for i386 */
	.long C_LABEL(sys_wait4)@h
	.long C_LABEL(sys_swapoff)@h		/* 115 */
	.long C_LABEL(sys_sysinfo)@h
	.long C_LABEL(sys_ipc)@h
	.long C_LABEL(sys_fsync)@h
	.long C_LABEL(sys_sigreturn)@h
	.long C_LABEL(sys_clone)@h		/* 120 */
	.long C_LABEL(sys_setdomainname)@h
	.long C_LABEL(sys_newuname)@h
	.long C_LABEL(sys_cacheflush)@h		/* modify_ldt for i386 */
	.long C_LABEL(sys_adjtimex)@h
	.long C_LABEL(sys_mprotect)@h		/* 125 */
	.long C_LABEL(sys_sigprocmask)@h
	.long C_LABEL(sys_create_module)@h
	.long C_LABEL(sys_init_module)@h
	.long C_LABEL(sys_delete_module)@h
	.long C_LABEL(sys_get_kernel_syms)@h	/* 130 */
	.long C_LABEL(sys_quotactl)@h
	.long C_LABEL(sys_getpgid)@h
	.long C_LABEL(sys_fchdir)@h
	.long C_LABEL(sys_bdflush)@h
	.long C_LABEL(sys_sysfs)@h		/* 135 */
	.long C_LABEL(sys_personality)@h
	.long C_LABEL(sys_ni_syscall)@h		/* for afs_syscall */
	.long C_LABEL(sys_setfsuid)@h
	.long C_LABEL(sys_setfsgid)@h
	.long C_LABEL(sys_llseek)@h		/* 140 */
	.long C_LABEL(sys_getdents)@h
	.long C_LABEL(sys_select)@h
	.long C_LABEL(sys_flock)@h
	.long C_LABEL(sys_msync)@h
	.long C_LABEL(sys_readv)@h		/* 145 */
	.long C_LABEL(sys_writev)@h
	.long C_LABEL(sys_getsid)@h
	.long C_LABEL(sys_fdatasync)@h
	.long C_LABEL(sys_sysctl)@h
	.long C_LABEL(sys_mlock)@h		/* 150 */
	.long C_LABEL(sys_munlock)@h
	.long C_LABEL(sys_mlockall)@h
	.long C_LABEL(sys_munlockall)@h
	.long C_LABEL(sys_sched_setparam)@h
	.long C_LABEL(sys_sched_getparam)@h   	/* 155 */
	.long C_LABEL(sys_sched_setscheduler)@h
	.long C_LABEL(sys_sched_getscheduler)@h
	.long C_LABEL(sys_sched_yield)@h
	.long C_LABEL(sys_sched_get_priority_max)@h
	.long C_LABEL(sys_sched_get_priority_min)@h  /* 160 */
	.long C_LABEL(sys_sched_rr_get_interval)@h
	.long C_LABEL(sys_nanosleep)@h
	.long C_LABEL(sys_mremap)@h
//vic - add new syscalls
	.long C_LABEL(sys_setresuid16)@h
	.long C_LABEL(sys_getresuid16)@h	/* 165 */
	.long C_LABEL(sys_ni_syscall)@h		/* for vm86 */
	.long C_LABEL(sys_query_module)@h
	.long C_LABEL(sys_poll)@h
	.long C_LABEL(sys_nfsservctl)@h
	.long C_LABEL(sys_setresgid16)@h	/* 170 */
	.long C_LABEL(sys_getresgid16)@h
	.long C_LABEL(sys_prctl)@h
	.long C_LABEL(sys_rt_sigreturn)@h
	.long C_LABEL(sys_rt_sigaction)@h
	.long C_LABEL(sys_rt_sigprocmask)@h	/* 175 */
	.long C_LABEL(sys_rt_sigpending)@h
	.long C_LABEL(sys_rt_sigtimedwait)@h
	.long C_LABEL(sys_rt_sigqueueinfo)@h
	.long C_LABEL(sys_rt_sigsuspend)@h
	.long C_LABEL(sys_pread)@h		/* 180 */
	.long C_LABEL(sys_pwrite)@h
	.long C_LABEL(sys_lchown16)@h
	.long C_LABEL(sys_getcwd)@h
	.long C_LABEL(sys_capget)@h
	.long C_LABEL(sys_capset)@h           	/* 185 */
	.long C_LABEL(sys_sigaltstack)@h
	.long C_LABEL(sys_sendfile)@h
	.long C_LABEL(sys_ni_syscall)@h		/* streams1 */
	.long C_LABEL(sys_ni_syscall)@h		/* streams2 */
	.long C_LABEL(sys_vfork)@h            	/* 190 */
	.long C_LABEL(sys_getrlimit)@h
	.long C_LABEL(sys_mmap2)@h
	.long C_LABEL(sys_truncate64)@h
	.long C_LABEL(sys_ftruncate64)@h
	.long C_LABEL(sys_stat64)@h		/* 195 */
	.long C_LABEL(sys_lstat64)@h
	.long C_LABEL(sys_fstat64)@h
	.long C_LABEL(sys_chown)@h
	.long C_LABEL(sys_getuid)@h
	.long C_LABEL(sys_getgid)@h		/* 200 */
	.long C_LABEL(sys_geteuid)@h
	.long C_LABEL(sys_getegid)@h
	.long C_LABEL(sys_setreuid)@h
	.long C_LABEL(sys_setregid)@h
	.long C_LABEL(sys_getgroups)@h		/* 205 */
	.long C_LABEL(sys_setgroups)@h
	.long C_LABEL(sys_fchown)@h
	.long C_LABEL(sys_setresuid)@h
	.long C_LABEL(sys_getresuid)@h
	.long C_LABEL(sys_setresgid)@h		/* 210 */
	.long C_LABEL(sys_getresgid)@h
	.long C_LABEL(sys_lchown)@h
	.long C_LABEL(sys_setuid)@h
	.long C_LABEL(sys_setgid)@h
	.long C_LABEL(sys_setfsuid)@h		/* 215 */
	.long C_LABEL(sys_setfsgid)@h

//vic	.rept	NR_syscalls-216			/* 216 - 255 */
	.rept	256-216			/* 216 - 255 */
	.long C_LABEL(sys_ni_syscall)@h
	.endr

/* End of entry.S */

