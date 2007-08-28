/*
 * arch/microblaze/kernel/entry.S -- Low-level system-call handling, trap handlers,
 *	and context-switching
 *
 *  Copyright (C) 2003	     John Williams <jwilliams@itee.uq.edu.au>
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 * Heavily modified by John Williams for Microblaze
 */

#include <linux/sys.h>

#include <asm/entry.h>
#include <asm/clinkage.h>
#include <asm/current.h>
#include <asm/processor.h>
#include <asm/irq.h>

#include "microblaze_defs.h"


/* Make a slightly more convenient alias for C_SYMBOL_NAME.  */
#define CSYM	C_SYMBOL_NAME

/* The offset of the struct pt_regs in a `state save frame' on the stack.  */
#define PTO	STATE_SAVE_PT_OFFSET

/* Standard macros to save and restore regs in the pt_regs struct pointed
   to by the stack pointer (r1) */
#define SAVE_REG(reg_num) \
	swi	macrology_paste(r,reg_num), r1, PTO+PT_GPR(reg_num);

#define RESTORE_REG(reg_num) \
	lwi	macrology_paste(r,reg_num), r1, PTO+PT_GPR(reg_num);

/* Save argument registers to the struct pt_regs pointed to by EP.  */
#define SAVE_ARG_REGS							      \
	SAVE_REG(5);							      \
	SAVE_REG(6);							      \
	SAVE_REG(7);							      \
	SAVE_REG(8);							      \
	SAVE_REG(9);							      \
	SAVE_REG(10);	

/* Restore argument registers from the struct pt_regs pointed to by EP.  */
#define RESTORE_ARG_REGS						      \
	RESTORE_REG(5);							      \
	RESTORE_REG(6);							      \
	RESTORE_REG(7);							      \
	RESTORE_REG(8);							      \
	RESTORE_REG(9);							      \
	RESTORE_REG(10);

/* Save value return registers to the struct pt_regs pointed to by EP.  */
#define SAVE_RVAL_REGS							      \
	SAVE_REG(3);							      \
	SAVE_REG(4);							      

/* Restore value return registers from the struct pt_regs pointed to by EP.  */
#define RESTORE_RVAL_REGS						      \
	RESTORE_REG(3);							      \
	RESTORE_REG(4);	

#define SAVE_CALL_CLOBBERED_REGS_BEFORE_ARGS

#define SAVE_CALL_CLOBBERED_REGS_AFTER_RVAL				      \
	SAVE_REG(11);							      \
	SAVE_REG(12);							      

#define RESTORE_CALL_CLOBBERED_REGS_BEFORE_ARGS	

#define RESTORE_CALL_CLOBBERED_REGS_AFTER_RVAL				      \
	RESTORE_REG(11);						      \
	RESTORE_REG(12);						      

/* Save `call clobbered' registers to the struct pt_regs pointed to by EP.  */
#define SAVE_CALL_CLOBBERED_REGS					      \
	SAVE_CALL_CLOBBERED_REGS_BEFORE_ARGS;				      \
	SAVE_RVAL_REGS;							      \
	SAVE_ARG_REGS;							      \
	SAVE_CALL_CLOBBERED_REGS_AFTER_RVAL;
/* Restore `call clobbered' registers from the struct pt_regs pointed to
   by EP.  */
#define RESTORE_CALL_CLOBBERED_REGS					      \
	RESTORE_CALL_CLOBBERED_REGS_BEFORE_ARGS;			      \
	RESTORE_RVAL_REGS;						      \
	RESTORE_ARG_REGS;						      \
	RESTORE_CALL_CLOBBERED_REGS_AFTER_RVAL;

/* Save `call clobbered' registers except for the return-value registers
   to the struct pt_regs pointed to by EP.  */
#define SAVE_CALL_CLOBBERED_REGS_NO_RVAL				      \
	SAVE_CALL_CLOBBERED_REGS_BEFORE_ARGS;				      \
	SAVE_ARG_REGS;							      \
	SAVE_CALL_CLOBBERED_REGS_AFTER_RVAL;
/* Restore `call clobbered' registers except for the return-value registers
   from the struct pt_regs pointed to by EP.  */
#define RESTORE_CALL_CLOBBERED_REGS_NO_RVAL				      \
	RESTORE_CALL_CLOBBERED_REGS_BEFORE_ARGS;			      \
	RESTORE_ARG_REGS;						      \
	RESTORE_CALL_CLOBBERED_REGS_AFTER_RVAL;

#define ZERO_REG(reg_num)						      \
	add	r ## reg_num, r0, r0;

/* Zero `call clobbered' registers except for the return-value registers.  */
#define ZERO_CALL_CLOBBERED_REGS_NO_RVAL				      \
	ZERO_REG(11); ZERO_REG(12);			      

/* Save `call saved' registers to the struct pt_regs pointed to by EP.  */
#define SAVE_CALL_SAVED_REGS						      \
	SAVE_REG(19);							      \
	SAVE_REG(20);							      \
	SAVE_REG(21);							      \
	SAVE_REG(22);							      \
	SAVE_REG(23);							      \
	SAVE_REG(24);							      \
	SAVE_REG(25);							      \
	SAVE_REG(26);							      \
	SAVE_REG(27);							      \
	SAVE_REG(28);							      \
	SAVE_REG(29);							      \
	SAVE_REG(30);							      \
 	SAVE_REG(31);

/* Restore `call saved' registers from the struct pt_regs pointed to by EP.  */
#define RESTORE_CALL_SAVED_REGS						      \
	RESTORE_REG(19);						      \
	RESTORE_REG(20);						      \
	RESTORE_REG(21);						      \
	RESTORE_REG(22);						      \
	RESTORE_REG(23);						      \
	RESTORE_REG(24);						      \
	RESTORE_REG(25);						      \
	RESTORE_REG(26);						      \
	RESTORE_REG(27);						      \
	RESTORE_REG(28);						      \
	RESTORE_REG(29);						      \
	RESTORE_REG(30);						      \
	RESTORE_REG(31);

/* Save system registers to the struct pt_regs pointed to by REG.  
   r11 is clobbered.  */
#define SAVE_SYS_REGS_FOR_IRQ						      \
	swi	r14, r1, PTO+PT_PC;	/* user's PC, before interrupt */     \
	mfs	r11, rmsr;	/* & PSW (XXX save this?) */		      \
	swi	r11, r1, PTO+PT_PSW;	

/* Restore system registers from the struct pt_regs pointed to by EP.         
   clobber r14 to restore status register, it gets fixed immediately */    
#define RESTORE_SYS_REGS_FOR_IRQ					      \
	lwi	r11, r1, PTO+PT_PSW;	/* Processor status word */	      \
	mts	rmsr, r11;						      \
	lwi	r14, r1, PTO+PT_PC;	/* User's PC before interrupt */      

/* Save system registers to the struct pt_regs pointed to by REG.  
   r11 is clobbered.  */
#define SAVE_SYS_REGS_FOR_TRAP						      \
	swi	r17, r1, PTO+PT_PC;	/* user's PC, before trap*/	      \
	mfs	r11, rmsr;	/* & PSW (XXX save this?) */		      \
	swi	r11, r1, PTO+PT_PSW;	

/* Restore system registers from the struct pt_regs pointed to by EP.         
   clobber r11 to restore status register */
#define RESTORE_SYS_REGS_FOR_TRAP					      \
	lwi	r11, r1, PTO+PT_PSW;	/* Processor status word */	      \
	mts	rmsr, r11;						      \
	lwi	r17, r1, PTO+PT_PC;	/* User's PC before trap */      

/* Save system registers to the struct pt_regs pointed to by REG.  
   r11 is clobbered.  */
#define SAVE_SYS_REGS_FOR_DBTRAP					      \
	swi	r17, r1, PTO+PT_PC;	/* user's PC, before trap*/	      \
	mfs	r11, rmsr;	/* & PSW (XXX save this?) */		      \
	swi	r11, r1, PTO+PT_PSW;	

/* Restore system registers from the struct pt_regs pointed to by EP.         
   clobber r11 to restore status register */
#define RESTORE_SYS_REGS_FOR_DBTRAP					      \
	lwi	r11, r1, PTO+PT_PSW;	/* Processor status word */	      \
	mts	rmsr, r11;						      \
	lwi	r17, r1, PTO+PT_PC;	/* User's PC before trap */      

/* Save system registers to the struct pt_regs pointed to by REG.  This is a
   NMI-specific version, because NMIs save the PC/PSW in a different place
   than other interrupt requests.  r11 is clobbered.  */
#define SAVE_SYS_REGS_FOR_NMI						      \
	swi	r14, r1, PTO+PT_PC;	/* user's PC, before NMI */	      \
	mfs	r11, rmsr;						      \
	swi	r11, r1, PTO+PT_PSW;

/* Restore system registers from the struct pt_regs pointed to by EP.  This is
   a NMI-specific version, because NMIs save the PC/PSW in a different place
   than other interrupt requests.  r11 is clobbered (it is used as a scratch
   register because the POP_STATE macro restores it, and this macro is usually
   used inside POP_STATE).  */
#define RESTORE_SYS_REGS_FOR_NMI					      \
	lwi	r11, r1, PTO+PT_PSW;					      \
	mts	rmsr, r11;						      \
	lwi	r14, r1, PTO+PT_PC;	/* User's PC before NMI */

/* Push register state, except for the stack pointer, on the stack in the form
   of a struct pt_regs, in preparation for a system call.  This macro makes
   sure that `special' registers, system registers; TYPE identifies the set of
   extra registers to be saved as well.  EP is clobbered.  
   Uses add(i)k to prevent corrupting carry flags prior to them being saved */
#ifdef CONFIG_REGISTER_TASK_PTR						      
#define PUSH_STATE(type)						      \
	addik	r1, r1, -STATE_SAVE_SIZE; /* Make room on the stack.  */      \
	SAVE_REG(2);				/* Save SDA */		      \
	SAVE_REG(13);				/* Save SDA2 */		      \
	SAVE_REG(15);				/* Save LP */		      \
	SAVE_REG(18);				/* Save asm scratch reg */    \
	SAVE_REG(CURRENT_TASK_REGNUM);		/* Save current task reg */   \
	type ## _STATE_SAVER;
#else
#define PUSH_STATE(type)						      \
	addik	r1, r1, -STATE_SAVE_SIZE; /* Make room on the stack.  */      \
	SAVE_REG(2);				/* Save SDA */		      \
	SAVE_REG(13);				/* Save SDA2 */		      \
	SAVE_REG(15);				/* Save LP */		      \
	SAVE_REG(18);				/* Save asm scratch reg */    \
	type ## _STATE_SAVER;
#endif									      

/* Pop a register state, except for the stack pointer, from the struct pt_regs
   on the stack.  */
#ifdef CONFIG_REGISTER_TASK_PTR						      
#define POP_STATE(type)							      \
	type ## _STATE_RESTORER;					      \
	RESTORE_REG(2);				/* Restore SDA */	      \
	RESTORE_REG(13);			/* Restore SDA2 */	      \
	RESTORE_REG(15);			/* Restore LP */	      \
	RESTORE_REG(18);			/* Restore asm scratch reg */ \
	RESTORE_REG(CURRENT_TASK_REGNUM);	/* Restore cur task reg */    \
	addik	r1, r1, STATE_SAVE_SIZE		/* Clean up stack space.  */
#else
#define POP_STATE(type)							      \
	type ## _STATE_RESTORER;					      \
	RESTORE_REG(2);				/* Restore SDA */	      \
	RESTORE_REG(13);			/* Restore SDA2 */	      \
	RESTORE_REG(15);			/* Restore LP */	      \
	RESTORE_REG(18);			/* Restore asm scratch reg */ \
	addik	r1, r1, STATE_SAVE_SIZE		/* Clean up stack space.  */
#endif


/* Switch to the kernel stack if necessary, and push register state on
   the stack in the form of a struct pt_regs.  Also load the current
   task pointer if switching from user mode.  The stack-pointer (r3)
   should have already been saved to the memory location SP_SAVE_LOC
   (the reason for this is that the interrupt vectors may be beyond a
   22-bit signed offset jump from the actual interrupt handler, and this
   allows them to save the stack-pointer and use that register to do an
   indirect jump).  This macro makes sure that `special' registers,
   system registers, and the stack pointer are saved; TYPE identifies
   the set of extra registers to be saved as well.  SYSCALL_NUM is the
   register in which the system-call number this state is for is stored
   (r0 if this isn't a system call).  Interrupts should already be
   disabled when calling this.  */
#define SAVE_STATE(type, syscall_num, sp_save_loc)			      \
	swi	r11, r0, R11_SAVE;	/* Save r11 */			      \
        lwi	r11, r0, KM;		/* See if already in kernel mode.  */ \
	beqi	r11, 1f;		/* Jump ahead if coming from user */  \
	lwi	r11, r0, R11_SAVE;	/* restore r11 */		      \
        /* Kernel-mode state save.  */					      \
	lwi	r1, r0, sp_save_loc;	/* Reload kernel stack-pointer.  */   \
	swi	r1, r1, (PT_GPR(GPR_SP)-PT_SIZE); /* Save original SP. */     \
        PUSH_STATE(type);						      \
	addi	r11, r0, 1; 		/* Was in kernel-mode.  */	      \
        swi	r11, r1, PTO+PT_KERNEL_MODE; 	 			      \
        brid	2f;							      \
	nop;				/* Fill delay slot */		      \
1:      /* User-mode state save.  */					      \
	lwi	r11, r0, R11_SAVE;	/* restore r11 */		      \
        lwi	r1, r0, KSP;		/* Switch to kernel stack.  */	      \
        PUSH_STATE(type);						      \
	swi	r0, r1, PTO+PT_KERNEL_MODE; /* Was in user-mode.  */	      \
	lwi	r11, r0, sp_save_loc;					      \
	swi	r11, r1, PTO+PT_GPR(GPR_SP); /* Store user SP.  */	      \
	addi	r11, r0, 1;						      \
	swi	r11, r0, KM;		/* Now we're in kernel-mode.  */      \
	GET_CURRENT_TASK(CURRENT_TASK);	/* Update the current task pointer.*/ \
2:      /* Save away the syscall number.  */				      \
        swi	syscall_num, r1, PTO+PT_SYSCALL


/* Save register state not normally saved by PUSH_STATE for TYPE.  */
#define SAVE_EXTRA_STATE(type)						      \
	type ## _EXTRA_STATE_SAVER;

/* Restore register state not normally restored by POP_STATE for TYPE.  */
#define RESTORE_EXTRA_STATE(type)					      \
	type ## _EXTRA_STATE_RESTORER;

/* Save any call-clobbered registers not normally saved by PUSH_STATE
   for TYPE.  */
#define SAVE_EXTRA_STATE_FOR_FUNCALL(type)				      \
	type ## _FUNCALL_EXTRA_STATE_SAVER;

/* Restore any call-clobbered registers not normally restored by POP_STATE for
   TYPE.  */
#define RESTORE_EXTRA_STATE_FOR_FUNCALL(type)				      \
	type ## _FUNCALL_EXTRA_STATE_RESTORER;


/* These are extra_state_saver/restorer values for a user trap.  Note that we
   save the argument registers so that restarted syscalls will function
   properly (otherwise it wouldn't be necessary), and we must _not_ restore
   the return-value registers (so that traps can return a value!), but there
   are various options for what happens to other call-clobbered registers,
   selected by preprocessor conditionals.  */

#if TRAPS_PRESERVE_CALL_CLOBBERED_REGS
   
/* Traps save/restore all call-clobbered registers (except for rval regs).  */
#define TRAP_STATE_SAVER						      \
     SAVE_CALL_CLOBBERED_REGS_NO_RVAL;					      \
     SAVE_SYS_REGS_FOR_TRAP

#define TRAP_STATE_RESTORER						      \
     RESTORE_CALL_CLOBBERED_REGS_NO_RVAL;				      \
     RESTORE_SYS_REGS_FOR_TRAP

#else /* !TRAPS_PRESERVE_CALL_CLOBBERED_REGS */

/* Traps don't save call-clobbered registers (but do still save arg regs).  */
#define TRAP_STATE_SAVER						      \
     SAVE_ARG_REGS;							      \
     SAVE_SYS_REGS_FOR_TRAP

#if TRAPS_ZERO_CALL_CLOBBERED_REGS
   
/* Traps zero call-clobbered registers (except for arg/rval regs) before
   returning from a system call, to avoid any internal values from leaking out
   of the kernel.  */
#define TRAP_STATE_RESTORER						      \
     ZERO_CALL_CLOBBERED_REGS_NO_ARGS_NO_RVAL;				      \
     RESTORE_ARG_REGS;							      \
     RESTORE_SYS_REGS_FOR_TRAP

#else /* !TRAPS_ZERO_CALL_CLOBBERED_REGS */
   
/* When traps return, they just leave call-clobbered registers (except for arg
   regs) with whatever value they have from the kernel.  */
#define TRAP_STATE_RESTORER						      \
     RESTORE_ARG_REGS;							      \
     RESTORE_SYS_REGS_FOR_TRAP

#endif /* TRAPS_ZERO_CALL_CLOBBERED_REGS */
#endif /* TRAPS_PRESERVE_CALL_CLOBBERED_REGS */

/* Save registers not normally saved by traps.  */
#define TRAP_EXTRA_STATE_SAVER						      \
   SAVE_RVAL_REGS;							      \
   SAVE_CALL_SAVED_REGS
#define TRAP_EXTRA_STATE_RESTORER					      \
   RESTORE_RVAL_REGS;							      \
   RESTORE_CALL_SAVED_REGS
#define TRAP_FUNCALL_EXTRA_STATE_SAVER					      \
   SAVE_RVAL_REGS
#define TRAP_FUNCALL_EXTRA_STATE_RESTORER				      \
   RESTORE_RVAL_REGS

/* Save registers not normally saved by traps.  */
#define DBTRAP_EXTRA_STATE_SAVER					      \
   SAVE_RVAL_REGS;							      \
   SAVE_CALL_SAVED_REGS
#define DBTRAP_EXTRA_STATE_RESTORER					      \
   RESTORE_RVAL_REGS;							      \
   RESTORE_CALL_SAVED_REGS
#define DBTRAP_FUNCALL_EXTRA_STATE_SAVER				      \
   SAVE_RVAL_REGS
#define DBTRAP_FUNCALL_EXTRA_STATE_RESTORER				      \
   RESTORE_RVAL_REGS

#define DBTRAP_STATE_SAVER						      \
     SAVE_CALL_CLOBBERED_REGS;						      \
     SAVE_SYS_REGS_FOR_DBTRAP

#define DBTRAP_STATE_RESTORER						      \
     RESTORE_CALL_CLOBBERED_REGS;					      \
     RESTORE_SYS_REGS_FOR_DBTRAP


/* Register saving/restoring for maskable interrupts.  */
#define IRQ_STATE_SAVER							      \
   SAVE_CALL_CLOBBERED_REGS;						      \
   SAVE_SYS_REGS_FOR_IRQ

#define IRQ_STATE_RESTORER						      \
   RESTORE_SYS_REGS_FOR_IRQ						      \
   RESTORE_CALL_CLOBBERED_REGS;	

#define IRQ_EXTRA_STATE_SAVER						      \
   SAVE_CALL_SAVED_REGS

#define IRQ_EXTRA_STATE_RESTORER					      \
   RESTORE_CALL_SAVED_REGS

#define IRQ_FUNCALL_EXTRA_STATE_SAVER       /* nothing */

#define IRQ_FUNCALL_EXTRA_STATE_RESTORER    /* nothing */

/* Register saving/restoring for non-maskable interrupts.  */
#define NMI_STATE_SAVER							      \
   SAVE_CALL_CLOBBERED_REGS;						      \
   SAVE_SYS_REGS_FOR_NMI

#define NMI_STATE_RESTORER						      \
   RESTORE_CALL_CLOBBERED_REGS;						      \
   RESTORE_SYS_REGS_FOR_NMI

#define NMI_EXTRA_STATE_SAVER						      \
   SAVE_CALL_SAVED_REGS

#define NMI_EXTRA_STATE_RESTORER					      \
   RESTORE_CALL_SAVED_REGS

#define NMI_FUNCALL_EXTRA_STATE_SAVER       /* nothing */

#define NMI_FUNCALL_EXTRA_STATE_RESTORER    /* nothing */
   
/* Register saving/restoring for a context switch.  We don't need to save too
   many registers, because context-switching looks like a function call (via
   the function `switch_thread'), so callers will save any call-clobbered
   registers themselves.  The stack pointer and return value are handled by
   switch_thread itself.  */
#define SWITCH_STATE_SAVER						      \
   SAVE_CALL_SAVED_REGS

#define SWITCH_STATE_RESTORER						      \
   RESTORE_CALL_SAVED_REGS

/* Instructions to return from an IRQ */
#define IRQ_RETURN_INST							      \
	rtid	r14, 0;		/* Follow interrupt link pointer back to */   \
				/* next insn after interrupt occured */       \
	nop;			/* Delay slot */		      

/* Instructions to return from a trap */
#define TRAP_RETURN_INST						      \
	rtsd	r17, 8;		/* r17 used as trap link register */	      \
	nop;

/* Instructions to return from a debug trap */
#define DBTRAP_RETURN_INST						      \
	rtsd	r17, 8;		/* r17 used as trap link register */	      \
	nop;

/* Code fragment to return from NMI */
#define NMI_RETURN_INST							      \
	rtbd	r16, 8;		/* r16 is NMI (brk) link register */	      \
	nop;

/* Restore register state from the struct pt_regs on the stack, switch back
   to the user stack if necessary, and return from the trap/interrupt.
   EXTRA_STATE_RESTORER is a sequence of assembly language statements to
   restore anything not restored by this macro.  Only registers not saved by
   the C compiler are restored (that is, R1(sp), R2(gp), R15(lp), and
   anything restored by EXTRA_STATE_RESTORER).  */
#define RETURN(type)							      \
	mfs	r11, rmsr;		/* Disable interrupts */	      \
	andi	r11, r11, ~2;						      \
	mts	rmsr, r11;						      \
        lwi	r11, r1, PTO+PT_KERNEL_MODE;				      \
	bnei	r11, 2f;		/* See if returning to kernel mode, */\
					/* ... if so, skip resched &c.  */    \
									      \
	/* We're returning to user mode, so check for various conditions that \
	   trigger rescheduling. */					      \
	/* Get current task ptr into r11 */				      \
	RETRIEVE_CURRENT_TASK(r11);					      \
	lwi	r11, r11, TASK_NEED_RESCHED;				      \
	bnei	r11, 3f;		/* Call the scheduler */	      \
									      \
	/* XXX Is PT_DTRACE handling needed here? */			      \
	/* XXX m68knommu also checks TASK_STATE & TASK_COUNTER here.  */      \
									      \
	/* Maybe handle a signal */					      \
	RETRIEVE_CURRENT_TASK(r11);					      \
	lwi	r11, r11, TASK_SIGPENDING;				      \
	bnei	r11, 4f;		/* Signals to handle, handle them */  \
									      \
/* Finally, return to user state.  */					      \
1:	swi	r0, r0, KM;		/* Now officially in user state. */   \
	POP_STATE(type);						      \
	swi	r1, r0, KSP;		/* Save the kernel stack pointer. */  \
	lwi	r1, r1, PT_GPR(GPR_SP)-PT_SIZE;				      \
					/* Restore user stack pointer. */     \
	type ## _RETURN_INST  		/* Perform appropriate return */      \
									      \
/* Return to kernel state.  */						      \
2:	POP_STATE(type);						      \
	type ## _RETURN_INST						      \
									      \
/* Call the scheduler before returning from a syscall/trap. */		      \
3:	SAVE_EXTRA_STATE_FOR_FUNCALL(type); /* Prepare for funcall. */	      \
	bralid	r15, CSYM(schedule);	/* Call scheduler */		      \
	nop;				/* delay slot */		      \
	mfs	r11, rmsr;	/* The scheduler enables interrupts */	      \
	andi	r11, r11, ~2;						      \
	mts	rmsr, r11;						      \
	RESTORE_EXTRA_STATE_FOR_FUNCALL(type);				      \
	brid	1b;							      \
	nop;								      \
									      \
/* Handle a signal return; Pending signals should be in r18.  */	      \
4:      /* Not all registers are saved by the normal trap/interrupt entry     \
	   points (for instance, call-saved registers (because the normal     \
	   C-compiler calling sequence in the kernel makes sure they're	      \
	   preserved), and call-clobbered registers in the case of	      \
	   traps), but signal handlers may want to examine or change the      \
	   complete register state.  Here we save anything not saved by	      \
	   the normal entry sequence, so that it may be safely restored	      \
	   (in a possibly modified form) after do_signal returns.  */	      \
        SAVE_EXTRA_STATE(type)		/* Save state not saved by entry. */  \
	la	r5, r1, PTO;		/* Arg 1: struct pt_regs *regs */     \
	add	r6, r0, r0;		/* Arg 2: sigset_t *oldset */	      \
	bralid	r15, CSYM(do_signal);	/* Handle any signals */	      \
	nop;								      \
	mfs	r11, rmsr;	/* Sig handling enables interrupts */	      \
	andi	r11, r11, ~2;						      \
	mts	rmsr, r11;						      \
        RESTORE_EXTRA_STATE(type);	/* Restore extra regs.  */	      \
	brid	1b;							      \
	nop;	


/* Jump to the appropriate function for the system call number in r12
   (r12 is not preserved), or return an error if r12 is not valid.  The
   LP register should point to the location where the called function
   should return.  [note that MAKE_SYS_CALL uses label 1]  */
#define MAKE_SYS_CALL							      \
	/* See if the system call number is valid.  */			      \
	addi	r11, r12, -NR_syscalls;					      \
	bgei	r11,1f;							      \
	/* Figure out which function to use for this system call.  */	      \
	/* Note Microblaze barrel shift is optional, so don't rely on it */   \
	add	r12, r12, r12;			/* convert num -> ptr */      \
	add	r12, r12, r12;						      \
	lwi	r12, r12, CSYM(sys_call_table);	/* Get function pointer */		      \
	/* Make the system call.  */					      \
	bra	r12;							      \
	/* The syscall number is invalid, return an error.  */		      \
1:	addi	r3, r0, -ENOSYS;					      \
	rtsd	r15,8;		/* looks like a normal subroutine return */


	// Ugly kludge to get around some Xilinx mb-gcc wierdness
	.data
	.globl CSYM(_shift_temp_loc)
CSYM(_shift_temp_loc):
	.long	0

	.text

/*
 * User trap.
 *
 * System calls are handled here.
 *
 * The stack-pointer (r1) should have already been saved to the memory
 * location ENTRY_SP (the reason for this is that the interrupt vectors may be
 * beyond a 22-bit signed offset jump from the actual interrupt handler, and
 * this allows them to save the stack-pointer and use that register to do an
 * indirect jump).
 *	
 * Syscall protocol:
 *   Syscall number in r12, args in r5-r10
 *   Return value in r3
 */
G_ENTRY(trap):
	mfs	r11, rmsr;		// disable interrupts 
	andi	r11, r11, ~2;
	mts	rmsr, r11;		
	swi	r1, r0, ENTRY_SP;	// save stack (emulate v850)
	SAVE_STATE (TRAP, r12, ENTRY_SP) // Save registers. 
	// No need to enable intrs here because microblaze
	// traps just implemented as a branch, not a hardware trap
	mfs	r11, rmsr;		// Enable interrupts
	ori	r11, r11, 2;
	mts	rmsr, r11;
	la	r15, r0, ret_from_trap-8// where the trap should return
					// need -8 to adjust for rtsd r15, 8

	MAKE_SYS_CALL			// Jump to the syscall function. 

END(trap)

/* This is just like ret_from_trap, but first restores extra registers
   saved by some wrappers.  */
L_ENTRY(restore_extra_regs_and_ret_from_trap):
	RESTORE_EXTRA_STATE(TRAP)
	// fall through
END(restore_extra_regs_and_ret_from_trap)

/* Entry point used to return from a syscall/trap.  */
L_ENTRY(ret_from_trap):
	RETURN(TRAP)
END(ret_from_trap)

/* This the initial entry point for a new child thread, with an appropriate
   stack in place that makes it look the the child is in the middle of an
   syscall.  This function is actually `returned to' from switch_thread
   (copy_thread makes ret_from_fork the return address in each new thread's
   saved context).  */
C_ENTRY(ret_from_fork):
	add	r3, r5, r0;		// switch_thread returns the prev task.
	bralid	r15, CSYM(schedule_tail); // ...which is schedule_tail's arg
	nop;				// delay slot
	add	r3, r0, r0;		// Child's fork call should return 0.
	brid	ret_from_trap		// Do normal trap return.
	nop;
C_END(ret_from_fork)


#if !TRAPS_PRESERVE_CALL_CLOBBERED_REGS
	// Make sure r13 and r14 are preserved, in case we have to restart a
	// system call because of a signal (ep has already been set by caller).
	//st.w	r13, PTO+PT_GPR(13)[sp]
	//st.w	r14, PTO+PT_GPR(13)[sp]
	la	r15, r0, ret_from_long_syscall-8
#endif /* !TRAPS_PRESERVE_CALL_CLOBBERED_REGS */

	MAKE_SYS_CALL			// Jump to the syscall function.
END(syscall_long)	

#if !TRAPS_PRESERVE_CALL_CLOBBERED_REGS
/* Entry point used to return from a long syscall.  Only needed to restore
   r13/r14 if the general trap mechanism doesnt' do so.  */
L_ENTRY(ret_from_long_syscall):
	//ld.w	PTO+PT_GPR(13)[sp], r13 // Restore the extra registers
	//ld.w	PTO+PT_GPR(13)[sp], r14
	brid	ret_from_trap;		// The rest is the same as other traps
	nop;
END(ret_from_long_syscall)
#endif /* !TRAPS_PRESERVE_CALL_CLOBBERED_REGS */


/* These syscalls need access to the struct pt_regs on the stack, so we
   implement them in assembly (they're basically all wrappers anyway).  */

L_ENTRY(sys_fork_wrapper):
#ifdef NO_MM
	// fork almost works, enough to trick you into looking elsewhere :-(
	addi	r3, r0, -EINVAL
	rtsd	r15, 8;
#else
	// Save state not saved by entry.  This is actually slight overkill;
	// it's actually only necessary to save any state restored by
	// switch_thread that's not saved by the trap entry.
	SAVE_EXTRA_STATE(TRAP)		// Save state not saved by entry.
	addi	r5, r0, SIGCHLD		// Arg 0: flags
	lwi	r6, r1, PTO+PT_GPR(GPR_SP) // Arg 1: child SP (use parent's)
	la	r7, r1, PTO;		// Arg 2: parent context
	add	r9. r0, r0;		// Arg 3: (unused)
	brid	CSYM(do_fork)		// Do real work (tail-call).
	nop;
#endif
END(sys_fork_wrapper)

L_ENTRY(sys_vfork_wrapper):
	// Save state not saved by entry.  This is actually slight overkill;
	// it's actually only necessary to save any state restored by
	// switch_thread that's not saved by the trap entry.
	SAVE_EXTRA_STATE(TRAP)		// Save state not saved by entry.
	addi	r5, r0, CLONE_VFORK | CLONE_VM | SIGCHLD // Arg 0: flags
	lwi	r6, r1, PTO+PT_GPR(GPR_SP) // Arg 1: child SP (use parent's)
	la	r7, r1, PTO;		// Arg 2: parent context
	add	r8, r0, r0;			// Arg 3: (unused)
	brid	CSYM(do_fork)		// Do real work (tail-call).
	nop
END(sys_vfork_wrapper)

L_ENTRY(sys_clone_wrapper):
	// Save state not saved by entry.  This is actually slight overkill;
	// it's actually only necessary to save any state restored by
	// switch_thread that's not saved by the trap entry.
	SAVE_EXTRA_STATE(TRAP)		// Save state not saved by entry.
	bnei	r6, 1f; 		// See if child SP arg (arg 1) is 0.
	lwi	r6, r1, PTO+PT_GPR(GPR_SP);	// If so, use paret's stack ptr
1:	la	r7, r1, PTO;		// Arg 2: parent context
	add	r8, r0, r0;		// Arg 3: (unused)
	brid	CSYM(do_fork)		// Do real work (tail-call).
	nop;
END(sys_clone_wrapper)

L_ENTRY(sys_execve_wrapper):
	la	r8, r1, PTO;		// add user context as 4th arg
	brid	CSYM(sys_execve);	// Do real work (tail-call).
	nop;
END(sys_execve_wrapper)

L_ENTRY(sys_sigsuspend_wrapper):
        SAVE_EXTRA_STATE(TRAP)		// Save state not saved by entry.
	la	r6, r1, PTO;		// add user context as 2nd arg
	bralid	r15, CSYM(sys_sigsuspend); // Do real work.
	nop;
	brid	restore_extra_regs_and_ret_from_trap;
	nop;
END(sys_sigsuspend_wrapper)

L_ENTRY(sys_rt_sigsuspend_wrapper):
        SAVE_EXTRA_STATE(TRAP)		// Save state not saved by entry.
	la	r7, r1, PTO;		// add user context as 3rd arg
	brlid	r15, CSYM(sys_rt_sigsuspend);	// Do real work.
	nop;
	brid	restore_extra_regs_and_ret_from_trap;
	nop;
END(sys_rt_sigsuspend_wrapper)

L_ENTRY(sys_sigreturn_wrapper):
        SAVE_EXTRA_STATE(TRAP)		// Save state not saved by entry.
	la	r5, r1, PTO;		// add user context as 1st arg
	brlid	r15, CSYM(sys_sigreturn);	// Do real work.
	nop;
	brid	restore_extra_regs_and_ret_from_trap;
	nop;
END(sys_sigreturn_wrapper)

L_ENTRY(sys_rt_sigreturn_wrapper):
        SAVE_EXTRA_STATE(TRAP)		// Save state not saved by entry.
	la	r5, r1, PTO;		// add user context as 1st arg
	brlid	r15, CSYM(sys_rt_sigreturn)	// Do real work.
	nop;
	brid	restore_extra_regs_and_ret_from_trap;
	nop;
END(sys_rt_sigreturn_wrapper)


/*
 * Hardware maskable interrupts.
 *
 * The stack-pointer (r1) should have already been saved to the memory
 * location ENTRY_SP.
 * Currently this is just because the v850 code does it.  This should
 * be removed at some point to streamline things
 */
G_ENTRY(irq):
	swi	r1, r0, ENTRY_SP;	// save stack (emulate v850)
	SAVE_STATE (IRQ, r0, ENTRY_SP)	// Save registers. 

	add	r5, r0, r0;		// Clear first param
	la	r6, r1, PTO;		// User regs are arg2

	// It's up to the high level handler to query the
	// interrupt controller and get the IRQ number etc

	// Call the high-level interrupt handling code.
	bralid r15, CSYM(handle_irq);
	nop;

	// fall through

/* Entry point used to return from an interrupt (also used by exception
   handle
s, below).  */
ret_from_irq:
	RETURN(IRQ)
END(irq)


/*
 * Hardware non-maskable interrupts.
 *
 * The stack-pointer (r1) should have already been saved to the memory
 * location ENTRY_SP (the reason for this is that the interrupt vectors may be
 * beyond a 22-bit signed offset jump from the actual interrupt handler, and
 * this allows them to save the stack-pointer and use that register to do an
 * indirect jump).
 */
G_ENTRY(nmi):
	swi	r1, r0, NMI_ENTRY_SP;		/* Save state (emulate v850) */
	SAVE_STATE (NMI, r0, NMI_ENTRY_SP); /* Save registers.  */

	add	r5, r0, r0;
	la	r6, r1, PTO;	/* User regs are arg2.  */

	/* Non-maskable interrupts always lie right after maskable interrupts.
	   Call the generic IRQ handler, with two arguments, the IRQ number,
	   and a pointer to the user registers, to handle the specifics.
	   (we subtract one because the first NMI has code 1).  */
	bralid 	r15, CSYM(handle_irq);
	nop;

	RETURN(NMI)
END(nmi0)


/*
 * Illegal instruction trap.
 *
 * The stack-pointer (r3) should have already been saved to the memory
 * location ENTRY_SP (the reason for this is that the interrupt vectors may be
 * beyond a 22-bit signed offset jump from the actual interrupt handler, and
 * this allows them to save the stack-pointer and use that register to do an
 * indirect jump).
 */
G_ENTRY(illegal_instruction):
	swi	r1, r0, ENTRY_SP;	// Save stack (emulate v850)
	SAVE_STATE (IRQ, r0, ENTRY_SP)	// Save registers. 
	mfs	r5, rmsr;		// Enable interrupts
	ori	r5, r5, 2;
	mts	rmsr, r5;

	addi	r5, r0, SIGILL;		// Arg 0: signal number
	RETRIEVE_CURRENT_TASK(r6);	// Arg 1: task
	la	r15, r0, ret_from_irq-8	// where the handler should return
	brid	CSYM(force_sig);
	nop;

END(illegal_instruction)


/*
 * `Debug' trap
 */
G_ENTRY(dbtrap):
	mfs	r11, rmsr;		// disable interrupts 
	andi	r11, r11, ~2;
	mts	rmsr, r11;		
	swi	r1, r0, ENTRY_SP;	// Save stack (emulate v850)
	SAVE_STATE (DBTRAP, r0, ENTRY_SP)// Save registers. 
	mfs	r11, rmsr;		// Enable interrupts
	ori	r11, r11, 2;
	mts	rmsr, r11;

	// Must insert code to detect illegal traps etc

	// For now, call into kernel for debugging
	brlid	r15, CSYM(debug_trap);
	la	r5, r1, PTO

	addi	r5, r0, SIGTRAP;	// send the trap signal
	RETRIEVE_CURRENT_TASK(r6);	// to the current task
	brlid	r15, CSYM(send_sig);
	nop;
	RETURN(DBTRAP)

END(dbtrap)

/*
 * Trap with no handler
 */
L_ENTRY(bad_trap_wrapper):
	add	r5, r0, r19; 		// Arg 0: trap number
	la	r6, r1, PTO;		// Arg 1: user regs
	brid	CSYM(bad_trap);		// tail call handler
	nop;

END(bad_trap_wrapper)

format_string:
.string "%08X\n";

/*
 * This is where we switch between two threads.  The arguments are:
 *   r5 -- pointer to the struct thread for the `current' process
 *   r6 -- pointer to the struct thread for the `new' process.  
 * when this function returns, it will return to the new thread.
 */
C_ENTRY(switch_thread):

	// Return the previous task
	// Do this before push_state, so that return value is on stack
	// Update the current task pointer
	//GET_CURRENT_TASK(CURRENT_TASK)
	RETRIEVE_CURRENT_TASK(r3);

	// First, push the current processor state on the stack
	PUSH_STATE(SWITCH)

	// Now save the location of the kernel stack pointer for this thread;
	// since we've pushed all other state on the stack, this is enough to
	// restore it all later.
	swi	r1, r5, THREAD_KSP;
	// Now restore the stack pointer from the new process
	lwi	r1, r6, THREAD_KSP;
	// ... and restore all state from that
	POP_STATE(SWITCH)
	// Update the current task pointer
	GET_CURRENT_TASK(CURRENT_TASK)
	// Now return into the new thread
	rtsd	r15,8;
	nop;
C_END(switch_thread)

	.section .rodata
	
	.align 4
	.globl CSYM(sys_call_table)
CSYM(sys_call_table):
	.long CSYM(sys_ni_syscall)	// 0  -  old "setup()" system call
	.long CSYM(sys_exit)
	.long sys_fork_wrapper
	.long CSYM(sys_read)
	.long CSYM(sys_write)
	.long CSYM(sys_open)		// 5
	.long CSYM(sys_close)
	.long CSYM(sys_waitpid)
	.long CSYM(sys_creat)
	.long CSYM(sys_link)
	.long CSYM(sys_unlink)		// 10
	.long sys_execve_wrapper
	.long CSYM(sys_chdir)
	.long CSYM(sys_time)
	.long CSYM(sys_mknod)
	.long CSYM(sys_chmod)		// 15
	.long CSYM(sys_chown16)
	.long CSYM(sys_ni_syscall)	// was: break
	.long CSYM(sys_ni_syscall)	// was: oldstat (aka stat)
	.long CSYM(sys_lseek)
	.long CSYM(sys_getpid)		// 20
	.long CSYM(sys_mount)
	.long CSYM(sys_oldumount)
	.long CSYM(sys_setuid16)
	.long CSYM(sys_getuid16)
	.long CSYM(sys_stime)		// 25
	.long CSYM(sys_ptrace)
	.long CSYM(sys_alarm)
	.long CSYM(sys_ni_syscall)	// was: oldfstat (aka fstat)
	.long CSYM(sys_pause)
	.long CSYM(sys_utime)		// 30
	.long CSYM(sys_ni_syscall)	// was: stty
	.long CSYM(sys_ni_syscall)	// was: gtty
	.long CSYM(sys_access)
	.long CSYM(sys_nice)
	.long CSYM(sys_ni_syscall)	// 35, was: ftime
	.long CSYM(sys_sync)
	.long CSYM(sys_kill)
	.long CSYM(sys_rename)
	.long CSYM(sys_mkdir)
	.long CSYM(sys_rmdir)		// 40
	.long CSYM(sys_dup)
	.long CSYM(sys_pipe)
	.long CSYM(sys_times)
	.long CSYM(sys_ni_syscall)	// was: prof
	.long CSYM(sys_brk)		// 45
	.long CSYM(sys_setgid16)
	.long CSYM(sys_getgid16)
	.long CSYM(sys_signal)
	.long CSYM(sys_geteuid16)
	.long CSYM(sys_getegid16)	// 50
	.long CSYM(sys_acct)
	.long CSYM(sys_umount)		// recycled never used phys()
	.long CSYM(sys_ni_syscall)	// was: lock
	.long CSYM(sys_ioctl)
	.long CSYM(sys_fcntl)		// 55
	.long CSYM(sys_ni_syscall)	// was: mpx
	.long CSYM(sys_setpgid)
	.long CSYM(sys_ni_syscall) 	// was: ulimit
	.long CSYM(sys_ni_syscall)
	.long CSYM(sys_umask)		// 60
	.long CSYM(sys_chroot)
	.long CSYM(sys_ustat)
	.long CSYM(sys_dup2)
	.long CSYM(sys_getppid)
	.long CSYM(sys_getpgrp)		// 65
	.long CSYM(sys_setsid)
	.long CSYM(sys_sigaction)
	.long CSYM(sys_sgetmask)
	.long CSYM(sys_ssetmask)
	.long CSYM(sys_setreuid16)	// 70
	.long CSYM(sys_setregid16)
	.long sys_sigsuspend_wrapper
	.long CSYM(sys_sigpending)
	.long CSYM(sys_sethostname)
	.long CSYM(sys_setrlimit)	// 75
	.long CSYM(sys_old_getrlimit)
	.long CSYM(sys_getrusage)
	.long CSYM(sys_gettimeofday)
	.long CSYM(sys_settimeofday)
	.long CSYM(sys_getgroups16)	// 80
	.long CSYM(sys_setgroups16)
	.long CSYM(sys_ni_syscall)	// was: old_select
	.long CSYM(sys_symlink)
	.long CSYM(sys_ni_syscall)	// was: oldlstat (aka lstat)
	.long CSYM(sys_readlink)	// 85
	.long CSYM(sys_uselib)
	.long CSYM(sys_swapon)
	.long CSYM(sys_reboot)
	.long CSYM(old_readdir)
	.long CSYM(sys_mmap)		// 90
	.long CSYM(sys_munmap)
	.long CSYM(sys_truncate)
	.long CSYM(sys_ftruncate)
	.long CSYM(sys_fchmod)
	.long CSYM(sys_fchown16)	// 95
	.long CSYM(sys_getpriority)
	.long CSYM(sys_setpriority)
	.long CSYM(sys_ni_syscall) 	// was: profil
	.long CSYM(sys_statfs)
	.long CSYM(sys_fstatfs)		// 100
	.long CSYM(sys_ni_syscall)	// i386: ioperm
	.long CSYM(sys_socketcall)
	.long CSYM(sys_syslog)
	.long CSYM(sys_setitimer)
	.long CSYM(sys_getitimer)	// 105
	.long CSYM(sys_newstat)
	.long CSYM(sys_newlstat)
	.long CSYM(sys_newfstat)
	.long CSYM(sys_ni_syscall)	// was: olduname (aka uname)
	.long CSYM(sys_ni_syscall)	// 110, i386: iopl
	.long CSYM(sys_vhangup)
	.long CSYM(sys_ni_syscall)	// was: idle
	.long CSYM(sys_ni_syscall)	// i386: vm86old
	.long CSYM(sys_wait4)
	.long CSYM(sys_swapoff)		// 115
	.long CSYM(sys_sysinfo)
	.long CSYM(sys_ipc)
	.long CSYM(sys_fsync)
	.long sys_sigreturn_wrapper
	.long sys_clone_wrapper		// 120
	.long CSYM(sys_setdomainname)
	.long CSYM(sys_newuname)
	.long CSYM(sys_ni_syscall)	// i386: modify_ldt, m68k: cacheflush 
	.long CSYM(sys_adjtimex)
	.long CSYM(sys_mprotect)	// 125
	.long CSYM(sys_sigprocmask)
	.long CSYM(sys_create_module)
	.long CSYM(sys_init_module)
	.long CSYM(sys_delete_module)
	.long CSYM(sys_get_kernel_syms) // 130
	.long CSYM(sys_quotactl)
	.long CSYM(sys_getpgid)
	.long CSYM(sys_fchdir)
	.long CSYM(sys_bdflush)
	.long CSYM(sys_sysfs)		// 135
	.long CSYM(sys_personality)
	.long CSYM(sys_ni_syscall)	// for afs_syscall
	.long CSYM(sys_setfsuid16)
	.long CSYM(sys_setfsgid16)
	.long CSYM(sys_llseek)		// 140
	.long CSYM(sys_getdents)
	.long CSYM(sys_select)
	.long CSYM(sys_flock)
	.long CSYM(sys_msync)
	.long CSYM(sys_readv)		// 145
	.long CSYM(sys_writev)
	.long CSYM(sys_getsid)
	.long CSYM(sys_fdatasync)
	.long CSYM(sys_sysctl)
	.long CSYM(sys_mlock)		// 150
	.long CSYM(sys_munlock)
	.long CSYM(sys_mlockall)
	.long CSYM(sys_munlockall)
	.long CSYM(sys_sched_setparam)
	.long CSYM(sys_sched_getparam)	// 155
	.long CSYM(sys_sched_setscheduler)
	.long CSYM(sys_sched_getscheduler)
	.long CSYM(sys_sched_yield)
	.long CSYM(sys_sched_get_priority_max)
	.long CSYM(sys_sched_get_priority_min)  // 160
	.long CSYM(sys_sched_rr_get_interval)
	.long CSYM(sys_nanosleep)
	.long CSYM(sys_mremap)
	.long CSYM(sys_setresuid16)
	.long CSYM(sys_getresuid16)	// 165
	.long CSYM(sys_ni_syscall)	// for vm86
	.long CSYM(sys_query_module)
	.long CSYM(sys_poll)
	.long CSYM(sys_nfsservctl)
	.long CSYM(sys_setresgid16)	// 170
	.long CSYM(sys_getresgid16)
	.long CSYM(sys_prctl)
	.long sys_rt_sigreturn_wrapper
	.long CSYM(sys_rt_sigaction)
	.long CSYM(sys_rt_sigprocmask)	// 175
	.long CSYM(sys_rt_sigpending)
	.long CSYM(sys_rt_sigtimedwait)
	.long CSYM(sys_rt_sigqueueinfo)
	.long sys_rt_sigsuspend_wrapper
	.long CSYM(sys_pread)		// 180
	.long CSYM(sys_pwrite)
	.long CSYM(sys_lchown16);
	.long CSYM(sys_getcwd)
	.long CSYM(sys_capget)
	.long CSYM(sys_capset)		// 185
	.long CSYM(sys_sigaltstack)
	.long CSYM(sys_sendfile)
	.long CSYM(sys_ni_syscall)	// streams1
	.long CSYM(sys_ni_syscall)	// streams2
	.long sys_vfork_wrapper		// 190
	.long CSYM(sys_getrlimit)
	.long CSYM(sys_mmap2)
	.long CSYM(sys_truncate64)
	.long CSYM(sys_ftruncate64)
	.long CSYM(sys_stat64)		// 195
	.long CSYM(sys_lstat64)
	.long CSYM(sys_fstat64)
	.long CSYM(sys_chown)
	.long CSYM(sys_getuid)
	.long CSYM(sys_getgid)		// 200
	.long CSYM(sys_geteuid)
	.long CSYM(sys_getegid)
	.long CSYM(sys_setreuid)
	.long CSYM(sys_setregid)
	.long CSYM(sys_getgroups)	// 205
	.long CSYM(sys_setgroups)
	.long CSYM(sys_fchown)
	.long CSYM(sys_setresuid)
	.long CSYM(sys_getresuid)
	.long CSYM(sys_setresgid)	// 210
	.long CSYM(sys_getresgid)
	.long CSYM(sys_lchown)
	.long CSYM(sys_setuid)
	.long CSYM(sys_setgid)
	.long CSYM(sys_setfsuid)	// 215
	.long CSYM(sys_setfsgid)
	.long CSYM(sys_pivot_root)
	.long CSYM(sys_mincore)		// just returns ENOSYS without MMU
	.long CSYM(sys_madvise)		// just returns ENOSYS without MMU
	.long CSYM(sys_getdents64)	// 220
	.long CSYM(sys_fcntl64)
	.long CSYM(sys_ni_syscall)	// Reserved for TUX
	.long CSYM(sys_ni_syscall)	// Reserved for `security'
	.long CSYM(sys_gettid)
	.long CSYM(sys_ni_syscall)	// 225, sys_readahead on i386

	.space (NR_syscalls-225)*4
