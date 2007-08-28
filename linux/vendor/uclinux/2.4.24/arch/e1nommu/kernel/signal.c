/*
 * linux/arch/e1nommu/kernel/signal.c
 *
 * Copyright (C) 2002-2003     George Thanos <george.thanos@gdt.gr>
 *                             Yannis Mitsos <yannis.mitsos@gdt.gr>
 * Original Copyright (C) 1991, 1992  Linus Torvalds
 */

/*****************************************************************
 * signal handlers should be executed in User Mode. When a signal
 * has arrived that a handler have been set for it, kernel 
 * should return to the handler instead of the main process. 
 * When the handler terminates the kernel will continue the
 * execution of the main process from the point it was
 * interrupted before the signal handler started to execute.
 *
 * Before starting executing the handler the return information 
 * located in the kernel mode aggregate stack should be copied
 * to the user mode aggregate stack. After that, kernel mode
 * aggregate stack should be altered, so that program will 
 * execute the handler. When the handler terminates the sigreturn()
 * system call is invoked in order to restore the return
 * information located in the kernel mode stack for the main/
 * interrupted process. Return info is described by the <kaggrframe
 * struct>.
 ******************************************************************/
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/personality.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>

#define DEBUG_SIG 0

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

void _sigreturn(void)
{
		sigreturn();
}

struct kaggrframe {
	unsigned long UB; /* Lower Address  in kernel aggregate stack */
	unsigned long L5;
	unsigned long L4;
	unsigned long L3;
	unsigned long L2;
	unsigned long L1;
	unsigned long L0;
	unsigned long dummy;
	unsigned long G5;
	unsigned long G4;
	unsigned long G3; /* Upper Address  in kernel aggregate stack */
};

struct aggr_stack_frame {
	struct kaggrframe kaggrframe;
	char sigreturn_code[12]; /* located in lower addresses */
	sigset_t oldset;
};

struct reg_stack_kernel_frame {
	unsigned long ReturnPC; /* L0 */
	unsigned long ReturnSR; /* L1 */
	unsigned long ReturnSP; /* L2 */
};
/* We need a space of 3 registers for the 
 * handler register stack.
 */
struct reg_stack_handler_frame {
	unsigned long signum;   /* L0 */
	unsigned long ReturnPC; /* L1 */
	unsigned long ReturnSR; /* L2 */
};

/* We need a space of 4 registers for the register stack 
 * of the sigreturn system call invocation.
 */
struct reg_stack_sigreturn_frame {
	unsigned long L0;
	unsigned long L1;
	unsigned long L2;
	unsigned long L3;
};

struct reg_stack_frame {
	struct reg_stack_sigreturn_frame reg_sigreturn;
	struct reg_stack_handler_frame   reg_handler;
	struct reg_stack_kernel_frame    reg_kframe;
};


/**********************************************************************
 * How do we setup the frame?
 * OK, we need to maintain: 
 * 1. Information located in the kernel aggregate in order to return nicely.
 * 2. The set of currently blocked signals.
 * 3. We need to copy the _sigreturn() code into the stack. After executing
 *    the handler, we should run sigreturn in order to call sys_sigreturn().
 * All the aformentioned info will be copied in the user aggregate stack.
 * 		|---------------| <-- old user G3
 * 		|blocked signals|
 * 		|---------------|
 * 		|				|
 * 		|   sigreturn   |
 * 		|	code		|
 * 		|---------------|
 * 		|				|
 * 		|				|
 * 		|   kaggrframe	|
 * 		|				|
 * 		|				|
 * 		|---------------| <-- new user G3, G4
 *
 * In the user register stack we only maintain the information in order to
 * return: 
 * 		a. From kernel mode to the signal handler. 
 * 		b. From the signal handler to the sigreturn code
 * 		   located in the user aggregate stack.
 *
 * 		|---------------|    ^  <-- new user SP
 * 		|  Return SP	|    |
 * 		|---------------|    |
 * 		|  Return SR	|  kernel mode
 * 		|---------------|    |
 * 		|  Return PC	|    |
 * 		|---------------|    v	^
 * 		|  Return SR    |   	| 
 * 		|---------------|		|
 * 		|  Return PC    |	 handler
 * 		|---------------|		|
 * 		|   signum      |		|
 * 		|---------------|	^	v
 * 		|     L3        |	|
 * 		|---------------|	|
 * 		|     L2        |	|
 * 		|---------------|	sigreturn frame
 * 		|     L1        |	|
 * 		|---------------|	|
 * 		|     L0        |	|
 * 		|---------------|	v   <-- old user SP
 * 
 **********************************************************************/

//#define DBG_setup_frame
static void setup_frame(int sig, struct k_sigaction *ka,
				            sigset_t *blockedset /*, struct pt_regs * regs*/)
{
	unsigned long *G9;
	unsigned long newG3;
	struct {
		unsigned long FP;
		unsigned long FL;
	} SR;
	struct aggr_stack_frame *user_aggr_frame_ptr;
	struct reg_stack_frame  *user_reg_frame_ptr, reg_frame;

	/* G9 defines the upper bound of the kernel aggregate stack  */
	asm volatile ("mov %0, G9\n\t"
					:"=l"(G9):/* no input*/:"%G9");

#define TOP_OF_KERNEL_AGGREGATE_STACK  G9
#define _G3              *(TOP_OF_KERNEL_AGGREGATE_STACK - 1)
#define _G4              *(TOP_OF_KERNEL_AGGREGATE_STACK - 2)
#define _G5              *(TOP_OF_KERNEL_AGGREGATE_STACK - 3)
#define _dummy			 *(TOP_OF_KERNEL_AGGREGATE_STACK - 4)
#define _L0              *(TOP_OF_KERNEL_AGGREGATE_STACK - 5)
#define _L1              *(TOP_OF_KERNEL_AGGREGATE_STACK - 6)
#define _L2              *(TOP_OF_KERNEL_AGGREGATE_STACK - 7)
#define _L3              *(TOP_OF_KERNEL_AGGREGATE_STACK - 8)
#define _L4              *(TOP_OF_KERNEL_AGGREGATE_STACK - 9)
#define _L5              *(TOP_OF_KERNEL_AGGREGATE_STACK - 10)
#define _UB              *(TOP_OF_KERNEL_AGGREGATE_STACK - 11)

	/***************************************************
	 * Start constructing the aggregate stack
	 * 1. Decrease the user aggregate SP by 
	 *    sizeof(struct aggr_stack_frame).
	 * 2. Copy kernel aggregate return info 
	 *    to <user_aggr_frame_ptr->kaggrframe>.
	 * 3. Copy sigreturn code to 
	 *    <user_aggr_frame_ptr->sigreturn_code>.
	 * 4. Copy *blockedset to <user_aggr_frame_ptr->oldset>
	 ***************************************************/

#ifdef DBG_setup_frame
	printk("Entered setup_frame\n");
	printk("Values located in the kernel aggregate stack\n");
	printk("G3 : 0x%x, G4 : 0x%x, G5 : 0x%x\n", _G3, _G4, _G5 );
	printk("L0 : 0x%x, L1 : 0x%x, L2 : 0x%x\n", _L0, _L1, _L2 );
	printk("L3 : 0x%x, L4 : 0x%x, L5 : 0x%x, UB : 0x%x\n",  _L3, _L4, _L5, _UB ); 
#endif

	/* _G3 is the lower index of the aggregate stack. _G3 is
	 * decreased by sizeof(struct aggr_stack_frame) */
	newG3 = _G3 - sizeof(struct aggr_stack_frame);
	user_aggr_frame_ptr = (struct aggr_stack_frame*)newG3;

	/* fill aggregate stack with the frame info */
	put_user(_G3, &user_aggr_frame_ptr->kaggrframe.G3);
	put_user(_G4, &user_aggr_frame_ptr->kaggrframe.G4);
	put_user(_G5, &user_aggr_frame_ptr->kaggrframe.G5);
	put_user(_dummy, &user_aggr_frame_ptr->kaggrframe.dummy);
	put_user(_L0, &user_aggr_frame_ptr->kaggrframe.L0 );
	put_user(_L1, &user_aggr_frame_ptr->kaggrframe.L1 );
	put_user(_L2, &user_aggr_frame_ptr->kaggrframe.L2 );
	put_user(_L3, &user_aggr_frame_ptr->kaggrframe.L3 );
	put_user(_L4, &user_aggr_frame_ptr->kaggrframe.L4 );
	put_user(_L5, &user_aggr_frame_ptr->kaggrframe.L5 );
	put_user(_UB, &user_aggr_frame_ptr->kaggrframe.UB );

	/* Copy sigreturn code to the user aggregate stack.
	 * 12 is the sizeof _sigreturn function (from objdump...)*/
	copy_to_user((char*)user_aggr_frame_ptr->sigreturn_code, (char*)_sigreturn, 12 );
	put_user(blockedset->sig[0], &user_aggr_frame_ptr->oldset.sig[0]);
	put_user(blockedset->sig[1], &user_aggr_frame_ptr->oldset.sig[1]);

	/* _G3 and _G4 are indexing right after user_aggregate_ptr
	 * Be carefull not to exceed the aggregate stack of the signal
	 * handler. You will corrupt user_aggr_frame_ptr values. */
	_G3 = newG3;
	_G4 = newG3;

#ifdef DBG_setup_frame
	printk("User Aggregate Stack values of the signal handler\n");
	printk("newG3 : 0x%x, newG4 : 0x%x\n", _G3, _G4 );
#endif

	/***************************************************
	 * Start constructing the register stack
	 ***************************************************/
	user_reg_frame_ptr = (struct reg_stack_frame *)_L2;

#define ReturnSP  reg_frame.reg_kframe.ReturnSP
#define ReturnSR  reg_frame.reg_kframe.ReturnSR
#define ReturnPC  reg_frame.reg_kframe.ReturnPC
	ReturnSP = _L2 + sizeof(struct reg_stack_handler_frame) 
				   + sizeof(struct reg_stack_sigreturn_frame);	
	ReturnPC = (unsigned long)ka->sa.sa_handler;

	ReturnSR = _L1 & 0x7F00;
    /* FL = 6 arbitrarily 
	 * FP = SP - 2*4. When entering signal handler a 
	 * frame Lx, L1 will reduce SP by 4 bytes!!!*/
/*1.FL*/SR.FL = 6; SR.FL <<= 21; ReturnSR |= SR.FL;
/*2.FP*/SR.FP = (ReturnSP - 2*4); SR.FP &= 0x000001fc;
	SR.FP <<= 23; ReturnSR |= SR.FP;

	/* Place the return info in the kernel
	 * aggregate stack. */
	_L0 = ReturnPC;
	_L1 = ReturnSR;
	_L2 = ReturnSP;
#undef ReturnSP
#undef ReturnSR
#undef ReturnPC

#define _signum_    reg_frame.reg_handler.signum
#define _ReturnPC_  reg_frame.reg_handler.ReturnPC
#define _ReturnSR_  reg_frame.reg_handler.ReturnSR

	_signum_ = sig;
	put_user( _signum_, &user_reg_frame_ptr->reg_handler.signum );
	_ReturnPC_ = (unsigned long)user_aggr_frame_ptr->sigreturn_code;
	put_user( _ReturnPC_, &user_reg_frame_ptr->reg_handler.ReturnPC );

    /* FL = 4 */
	_ReturnSR_ = 0;
/*1.FL*/SR.FL = 4; SR.FL <<= 21; _ReturnSR_ |= SR.FL;
/*2.FP*/SR.FP = ( reg_frame.reg_kframe.ReturnSP - 7*4); SR.FP &= 0x000001fc;
	SR.FP <<= 23; _ReturnSR_ |= SR.FP;
	put_user( _ReturnSR_, &user_reg_frame_ptr->reg_handler.ReturnSR );

#undef signum
#undef ReturnPC
#undef ReturnSR

#undef _L0
#undef _L1
#undef _L2
#undef _L3
#undef _L4
#undef _L5
#undef _UB
#undef _G3
#undef _G4
#undef _G5
#undef _dummy
#undef TOP_OF_KERNEL_AGGREGATE_STACK
}

static void setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info, sigset_t *set)
{
}

/*
 * OK, we're invoking a handler
 */	
//#define DBG_handle_signal
static void
handle_signal(unsigned long sig, struct k_sigaction *ka,
	      siginfo_t *info, sigset_t *oldset, struct pt_regs regs)
{
#define L    regs.L
#define FL   regs.FL
#define SYSCALL_RETURN_VALUE	L[FL-1]
#define SYSCALL_RETURN_PC		L[FL]

#ifdef  DBG_handle_signal
	printk("f.blocked->sig[0] : 0x%x, blocked->sig[1] : 0x%x\n",
		  (int)current->blocked.sig[0], (int)current->blocked.sig[1] );
	printk("L[FL+3] : 0x%x(%d)\n", (int)L[FL+3], (int)L[FL+3] );
#endif
	/* We have to check for system call restarting both here and
	 * at the end of do_signal.
	 * 1. If a handler is specified do_signal returns immidiately
	 *    according to POSIX. The check code at the end of do_signal
	 *    won't run.
	 * 2. If a handler is not present and action is "ignore" code at
	 *    the end of do_signal will run.
	 *
	 * -> L[FL+3] will contain the value 14 if we came from a 
	 * system-call
	 */

	/* Do we come from a system call? */
	if ( L[FL+3] == 14 ) {
		/* If so, check system call restarting.. */
		switch ( SYSCALL_RETURN_VALUE ) {
			/* Handler is specified here... */
			case -ERESTARTNOHAND:
				SYSCALL_RETURN_VALUE = -EINTR;
				break;
			case -ERESTARTSYS:
				if (!(ka->sa.sa_flags & SA_RESTART)) {
					SYSCALL_RETURN_VALUE = -EINTR;
					break;
				}
			/* fallthrough */
			case -ERESTARTNOINTR:
				SYSCALL_RETURN_PC -= 2;
		}
	}
#undef SYSCALL_RETURN_VALUE
#undef SYSCALL_RETURN_PC
#undef L
#undef FL

	/* Set up the stack frame */
	if (ka->sa.sa_flags & SA_SIGINFO)
		setup_rt_frame(sig, ka, info, oldset);
	else
		setup_frame(sig, ka, oldset);

	if (ka->sa.sa_flags & SA_ONESHOT)
		ka->sa.sa_handler = SIG_DFL;

	if (!(ka->sa.sa_flags & SA_NODEFER)) {
		spin_lock_irq(&current->sigmask_lock);
		sigorsets(&current->blocked,&current->blocked,&ka->sa.sa_mask);
		sigaddset(&current->blocked,sig);
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);
	}
}

//#define DBG_sys_sigreturn
asmlinkage int sys_sigreturn(void)
{
	unsigned long *G9;
	struct aggr_stack_frame *user_aggr_frame_ptr;
	asm volatile ("mov %0, G9"
					:"=l"(G9):/* no input*/);
#define TOP_OF_KERNEL_AGGREGATE_STACK  G9
#define _G3              *(TOP_OF_KERNEL_AGGREGATE_STACK - 1)
#define _G4              *(TOP_OF_KERNEL_AGGREGATE_STACK - 2)
#define _G5              *(TOP_OF_KERNEL_AGGREGATE_STACK - 3)
#define _dummy			 *(TOP_OF_KERNEL_AGGREGATE_STACK - 4)
#define _L0              *(TOP_OF_KERNEL_AGGREGATE_STACK - 5)
#define _L1              *(TOP_OF_KERNEL_AGGREGATE_STACK - 6)
#define _L2              *(TOP_OF_KERNEL_AGGREGATE_STACK - 7)
#define _L3              *(TOP_OF_KERNEL_AGGREGATE_STACK - 8)
#define _L4              *(TOP_OF_KERNEL_AGGREGATE_STACK - 9)
#define _L5              *(TOP_OF_KERNEL_AGGREGATE_STACK - 10)
#define _UB              *(TOP_OF_KERNEL_AGGREGATE_STACK - 11)

	user_aggr_frame_ptr = (struct aggr_stack_frame*)_G3;
	get_user( _G3, &user_aggr_frame_ptr->kaggrframe.G3 );
	get_user( _G4, &user_aggr_frame_ptr->kaggrframe.G4 );
	get_user( _G5, &user_aggr_frame_ptr->kaggrframe.G5 );
	get_user( _dummy, &user_aggr_frame_ptr->kaggrframe.dummy );
	get_user( _L0, &user_aggr_frame_ptr->kaggrframe.L0 );
	get_user( _L1, &user_aggr_frame_ptr->kaggrframe.L1 );
	get_user( _L2, &user_aggr_frame_ptr->kaggrframe.L2 );
	get_user( _L3, &user_aggr_frame_ptr->kaggrframe.L3 );
	get_user( _L4, &user_aggr_frame_ptr->kaggrframe.L4 );
	get_user( _L5, &user_aggr_frame_ptr->kaggrframe.L5 );
	get_user( _UB, &user_aggr_frame_ptr->kaggrframe.UB );

#ifdef DBG_sys_sigreturn
	printk("Entered sys_sigreturn\n");
	printk("aggr_frame : 0x%x\n", (int)aggr_frame );
	printk("G3 : 0x%x, G4 : 0x%x, G5 : 0x%x\n", _G3, _G4, _G5 );
	printk("L0 : 0x%x, L1 : 0x%x, L2 : 0x%x\n", _L0, _L1, _L2 );
	printk("L3 : 0x%x, L4 : 0x%x, L5 : 0x%x, UB : 0x%x\n", _L3, _L4, _L5, _UB );
#endif
	{
		sigset_t oldset;
		/* Get block signal values */
		get_user( oldset.sig[0], &user_aggr_frame_ptr->oldset.sig[0] );
		get_user( oldset.sig[1], &user_aggr_frame_ptr->oldset.sig[1] );

		sigdelsetmask(&oldset, ~_BLOCKABLE);
		spin_lock_irq(&current->sigmask_lock);
		current->blocked = oldset;
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);
	}

#ifdef DBG_sys_sigreturn
	printk("after. blocked->sig[0] : 0x%x, blocked->sig[1] : 0x%x\n",
		(int)current->blocked.sig[0], (int)current->blocked.sig[1] );
#endif

#undef _L0
#undef _L1
#undef _L2
#undef _L3
#undef _L4
#undef _L5
#undef _UB
#undef _G3
#undef _G4
#undef _G5
#undef _dummy
#undef _TOP_OF_KERNEL_AGGREGATE_STACK
	return 0;
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals that
 * the kernel can handle, and then we build all the user-level signal handling
 * stack-frames in one go after that.
 */
//#define DBG_do_signal
int do_signal(struct pt_regs regs)
{
	siginfo_t info;
	struct k_sigaction *ka;
	sigset_t *oldset;

	/*
	 * We want the common case to go fast, which
	 * is why we may in certain cases get here from
	 * kernel mode. Just return without doing anything
	 * if so.
	 */
	if (!user_mode(&regs))
		return 1;

	oldset = &current->blocked;

	for (;;) {
		unsigned long signr;

		spin_lock_irq(&current->sigmask_lock);
		signr = dequeue_signal(&current->blocked, &info);
		spin_unlock_irq(&current->sigmask_lock);

		if (!signr)
			break;

		if ((current->ptrace & PT_PTRACED) && signr != SIGKILL) {
			/* Let the debugger run.  */
			current->exit_code = signr;
			current->state = TASK_STOPPED;
			notify_parent(current, SIGCHLD);
			schedule();

			/* We're back.  Did the debugger cancel the sig?  */
			if (!(signr = current->exit_code))
				continue;
			current->exit_code = 0;

			/* The debugger continued.  Ignore SIGSTOP.  */
			if (signr == SIGSTOP)
				continue;

			/* Update the siginfo structure.  Is this good?  */
			if (signr != info.si_signo) {
				info.si_signo = signr;
				info.si_errno = 0;
				info.si_code = SI_USER;
				info.si_pid = current->p_pptr->pid;
				info.si_uid = current->p_pptr->uid;
			}

			/* If the (new) signal is now blocked, requeue it.  */
			if (sigismember(&current->blocked, signr)) {
				send_sig_info(signr, &info, current);
				continue;
			}
		}

		ka = &current->sig->action[signr-1];
		if (ka->sa.sa_handler == SIG_IGN) {
#ifdef DBG_do_signal
			printk("Signal IGNORED\n");
#endif
			if (signr != SIGCHLD)
				continue;
			/* Check for SIGCHLD: it's special.  */
			while (sys_wait4(-1, NULL, WNOHANG, NULL) > 0)
				/* nothing */;
			continue;
		}

		if (ka->sa.sa_handler == SIG_DFL) {
			int exit_code = signr;
#ifdef DBG_do_signal
			printk("Signal DEFAULT, signum : %d\n", (int)signr);
#endif

			/* Init gets no signals it doesn't want.  */
			if (current->pid == 1)
				continue;

			switch (signr) {
			case SIGCONT: case SIGCHLD: case SIGWINCH:
				continue;

			case SIGTSTP: case SIGTTIN: case SIGTTOU:
				if (is_orphaned_pgrp(current->pgrp))
					continue;
				/* FALLTHRU */

			case SIGSTOP:
				current->state = TASK_STOPPED;
				current->exit_code = signr;
				if (!(current->p_pptr->sig->action[SIGCHLD-1].sa.sa_flags & SA_NOCLDSTOP))
					notify_parent(current, SIGCHLD);
				schedule();
				continue;

			case SIGQUIT: case SIGILL: case SIGTRAP:
			case SIGABRT: case SIGFPE: case SIGSEGV:
			case SIGBUS: case SIGSYS: case SIGXCPU: case SIGXFSZ:
				if (do_coredump(signr, &regs))
					exit_code |= 0x80;
				/* FALLTHRU */

			default:
				sig_exit(signr, exit_code, &info);
				/* NOTREACHED */
			} /* end of switch */
		} /* end of if SIG_DFL */

		/* Whee!  Actually deliver the signal.  */
		handle_signal(signr, ka, &info, oldset, regs);
		return 1;
	}

#define L    regs.L
#define FL   regs.FL
#define SYSCALL_RETURN_VALUE	L[FL-1]
#define SYSCALL_RETURN_PC		L[FL]

#ifdef DBG_do_signal
	printk("L[FL+3] : 0x%x(%d)\n", (int)L[FL+3], (int)L[FL+3] );
#endif
	/* Ok, in our case we don not have negative values for the
	 * interrupts and the exceptions, rather for the sys-calls
	 * the L[FL+3] will contain the value 14 if we came from a 
	 * system-call
	 */

	/* Do we come from a system call? */
	if ( L[FL+3] == 14 ) {
		/* If so, check system call restarting.. */
		switch ( SYSCALL_RETURN_VALUE ) {
			/* No handler is specified here... */
			case -ERESTARTNOHAND:
				SYSCALL_RETURN_PC -= 2;
				break;
			case -ERESTARTSYS:
				if (!(ka->sa.sa_flags & SA_RESTART)) {
					SYSCALL_RETURN_VALUE = -EINTR;
					break;
				}
			/* fallthrough */
			case -ERESTARTNOINTR:
				SYSCALL_RETURN_PC -= 2;
		}
	}
#undef SYSCALL_RETURN_VALUE
#undef SYSCALL_RETURN_PC
#undef L
#undef FL

	return 0;
}

/* sys_rt_sigaction is defined in <kernel/signal.c> */

asmlinkage int
sys_sigaction(int sig, const struct sigaction *act,
						     struct sigaction *oact)
{
    struct k_sigaction new_ka, old_ka;
    int ret;

    if (act) {
        if (verify_area(VERIFY_READ, act, sizeof(*act)) ||
            get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
            get_user(new_ka.sa.sa_restorer, &act->sa_restorer))
            return -EFAULT;
        get_user(new_ka.sa.sa_flags, &act->sa_flags);
        get_user(new_ka.sa.sa_mask.sig[0], &act->sa_mask.sig[0]);
        get_user(new_ka.sa.sa_mask.sig[1], &act->sa_mask.sig[1]);
    }

    ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

    if (!ret && oact) {
        if (verify_area(VERIFY_WRITE, oact, sizeof(*oact)) ||
            put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
            put_user(old_ka.sa.sa_restorer, &oact->sa_restorer))
            return -EFAULT;
        put_user(old_ka.sa.sa_flags, &oact->sa_flags);
        put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask.sig[0]);
        put_user(old_ka.sa.sa_mask.sig[1], &oact->sa_mask.sig[1]);
    }

    return ret;
}

/*
 * Atomically swap in the new signal mask, and wait for a signal.
 *
 * sys_sigsuspend built having in ming the following library implementation
 * from uClibc. sys_sigsuspend is not currently used sys_rt_sigsuspend is
 * used instead.
 *
 *	_syscall3(int, __sigsuspend, int, a, unsigned long int, b, unsigned long int, c);
 * int sigsuspend (const sigset_t *set)
 * {
 *	return __sigsuspend(0, 0, set->__val[0]);
 * }
 */
asmlinkage long 
sys_sigsuspend(struct pt_regs regs)
{
#define L	regs.L
#define FL  regs.FL
#define SYSCALL_RETURN_VALUE	L[FL-1]
/* We don't care about the first 2 args*/
#define _3rd_ARG  				L[FL-5]

    sigset_t saveset;
	old_sigset_t mask = _3rd_ARG;

    mask &= _BLOCKABLE;
    spin_lock_irq(&current->sigmask_lock);
    saveset = current->blocked;
    siginitset(&current->blocked, mask);
    recalc_sigpending(current);
    spin_unlock_irq(&current->sigmask_lock);

    SYSCALL_RETURN_VALUE = -EINTR;
    while (1) {
        current->state = TASK_INTERRUPTIBLE;
        schedule();
    	spin_lock_irq(&current->sigmask_lock);
    	current->blocked = saveset;
    	spin_unlock_irq(&current->sigmask_lock);
        if ( do_signal(regs) )
            return -EINTR;
    }
#undef _3rd_ARG
#undef SYSCALL_RETURN_VALUE
#undef L
#undef FL
}

asmlinkage long
sys_rt_sigsuspend(struct pt_regs regs)
{
#define L	regs.L
#define FL  regs.FL
#define SYSCALL_RETURN_VALUE	L[FL-1]
#define _1st_ARG  				L[FL-3]
#define _2nd_ARG  				L[FL-4]
    sigset_t saveset, newset;
	sigset_t *unewset = (sigset_t *)_1st_ARG;
	size_t sigsetsize = (size_t)_2nd_ARG;

    if (sigsetsize != sizeof(sigset_t))
        return -EINVAL;

    if (copy_from_user(&newset, unewset, sizeof(sigset_t)))
        return -EFAULT;

    sigdelsetmask(&newset, ~_BLOCKABLE);
    spin_lock_irq(&current->sigmask_lock);
    saveset = current->blocked;
    current->blocked = newset;
    recalc_sigpending(current);
    spin_unlock_irq(&current->sigmask_lock);

    SYSCALL_RETURN_VALUE = -EINTR;
    while (1) {
        current->state = TASK_INTERRUPTIBLE;
        schedule();
    	spin_lock_irq(&current->sigmask_lock);
    	current->blocked = saveset;
    	spin_unlock_irq(&current->sigmask_lock);
        if (do_signal(regs))
            return -EINTR;
    }
#undef _1st_ARG
#undef _2nd_ARG
#undef SYSCALL_RETURN_VALUE
#undef FL
#undef L
}

int copy_siginfo_to_user(siginfo_t *to, siginfo_t *from) { return 0;}
