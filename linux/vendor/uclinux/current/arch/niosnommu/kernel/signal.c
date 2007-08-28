/*
 *  linux/arch/niosnommu/kernel/signal.c
 *
 * Copied/hacked from:
 */

/*
 *  linux/arch/m68k/kernel/signal.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/*
 * Linux/m68k support by Hamish Macdonald
 *
 * 68060 fixes by Jesper Skov
 *
 * 1997-12-01  Modified for POSIX.1b signals by Andreas Schwab
 *
 * mathemu support by Roman Zippel
 *  (Note: fpstate in the signal context is completly ignored for the emulator
 *         and the internal floating point format is put on stack)
 */

/*
 * ++roman (07/09/96): implemented signal stacks (specially for tosemu on
 * Atari :-) Current limitation: Only one sigstack can be active at one time.
 * If a second signal with SA_ONSTACK set arrives while working on a sigstack,
 * SA_ONSTACK is ignored. This behaviour avoids lots of trouble with nested
 * signal handlers!
 */


#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/highuid.h>
#include <linux/personality.h>

#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/traps.h>
#include <asm/ucontext.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

asmlinkage long sys_wait4(pid_t pid, unsigned int * stat_addr, int options,
			struct rusage * ru);
asmlinkage int do_signal(sigset_t *oldset, struct pt_regs *regs);

/*
 * Atomically swap in the new signal mask, and wait for a signal.
 */
asmlinkage int
do_sigsuspend(old_sigset_t *pmask, struct pt_regs *regs)
{
	old_sigset_t mask;
	sigset_t saveset;
	
	if (copy_from_user(&mask, pmask, sizeof(mask)))
		return -EFAULT;
		
	mask &= _BLOCKABLE;
	spin_lock_irq(&current->sigmask_lock);
	saveset = current->blocked;
	siginitset(&current->blocked, mask);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	regs->u_regs[UREG_I0] = -EINTR;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal(&saveset, regs))
			return -EINTR;
	}
}

asmlinkage int
do_rt_sigsuspend(sigset_t *unewset, size_t sigsetsize, struct pt_regs *regs)
{
	sigset_t saveset, newset;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (copy_from_user(&newset, unewset, sizeof(newset)))
		return -EFAULT;
	sigdelsetmask(&newset, ~_BLOCKABLE);

	spin_lock_irq(&current->sigmask_lock);
	saveset = current->blocked;
	current->blocked = newset;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	regs->u_regs[UREG_I0] = -EINTR;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal(&saveset, regs))
			return -EINTR;
	}
}

asmlinkage int 
sys_sigaction(int sig, const struct old_sigaction *act,
	      struct old_sigaction *oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	if (act) {
		old_sigset_t mask;
		if (verify_area(VERIFY_READ, act, sizeof(*act)) ||
		    __get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
		    __get_user(new_ka.sa.sa_restorer, &act->sa_restorer))
			return -EFAULT;
		__get_user(new_ka.sa.sa_flags, &act->sa_flags);
		__get_user(mask, &act->sa_mask);
		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (verify_area(VERIFY_WRITE, oact, sizeof(*oact)) ||
		    __put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
		    __put_user(old_ka.sa.sa_restorer, &oact->sa_restorer))
			return -EFAULT;
		__put_user(old_ka.sa.sa_flags, &oact->sa_flags);
		__put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask);
	}

	return ret;
}


/*
 * Do a signal return; undo the signal stack.
 *
 */

struct sigframe
{
	struct reg_window	sig_window;
	//char *pretcode;
	int sig;
	int code;
	//struct sigcontext *psc;
	char retcode[8];
	unsigned long extramask[_NSIG_WORDS-1];
	struct sigcontext sc;
};

struct rt_sigframe
{
	struct reg_window	sig_window;
	//char *pretcode;
	int sig;
	//struct siginfo *pinfo;
	//void *puc;
	char retcode[8];
	struct siginfo info;
	struct ucontext uc;
};


int copy_siginfo_to_user(siginfo_t *to, siginfo_t *from)
{
	if (!access_ok (VERIFY_WRITE, to, sizeof(siginfo_t)))
		return -EFAULT;
	if (from->si_code < 0)
		return __copy_to_user(to, from, sizeof(siginfo_t));
	else {
		int err;

		/* If you change siginfo_t structure, please be sure
		   this code is fixed accordingly.
		   It should never copy any pad contained in the structure
		   to avoid security leaks, but must copy the generic
		   3 ints plus the relevant union member.  */
		err = __put_user(from->si_signo, &to->si_signo);
		err |= __put_user(from->si_errno, &to->si_errno);
		err |= __put_user((short)from->si_code, &to->si_code);
		/* First 32bits of unions are always present.  */
		err |= __put_user(from->si_pid, &to->si_pid);
		switch (from->si_code >> 16) {
		case __SI_FAULT >> 16:
			break;
		case __SI_CHLD >> 16:
			err |= __put_user(from->si_utime, &to->si_utime);
			err |= __put_user(from->si_stime, &to->si_stime);
			err |= __put_user(from->si_status, &to->si_status);
		default:
			err |= __put_user(from->si_uid, &to->si_uid);
			break;
		/* case __SI_RT: This is not generated by the kernel as of now.  */
		}
		return err;
	}
}

static inline int
restore_sigcontext(struct pt_regs *regs, struct sigcontext *sc,
					int *rval_p)
{
	int err = 0;

	/* restore the regs from &sc->regs (same as sc, since regs is first)
	 * (sc is already checked for VERIFY_READ since the sigframe was
	 *  checked in sys_sigreturn previously)
	 */
	if (__copy_from_user(regs, &sc->regs, sizeof(struct pt_regs)))
		goto badframe;

	regs->sysc_no = -1;		/* disable syscall checks */

	*rval_p=regs->u_regs[UREG_I0];
	return err;

badframe:
	return 1;
}

static inline int
rt_restore_ucontext(struct pt_regs *regs, /*struct switch_stack *sw,*/
		    struct ucontext *uc, int *rval_p)
{
	int err=0;

	if (__copy_from_user(regs, &uc->uc_mcontext.regs, sizeof(struct pt_regs)))
		goto badframe;

	regs->sysc_no = -1;		/* disable syscall checks */

	//if (do_sigaltstack(&uc->uc_stack, NULL, usp) == -EFAULT)
	//	goto badframe;

	*rval_p=regs->u_regs[UREG_I0];
	return err;

badframe:
	return 1;
}


asmlinkage int do_sigreturn(struct pt_regs *regs)
{
	struct sigframe *frame = (struct sigframe *) regs->u_regs [UREG_FP];
	sigset_t set;
	int rval;

	if (verify_area(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__get_user(set.sig[0], &frame->sc.oldmask) ||
	    (_NSIG_WORDS > 1 &&
	     __copy_from_user(&set.sig[1], &frame->extramask,
			      sizeof(frame->extramask))))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = set;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
	
	if (restore_sigcontext(regs, &frame->sc, &rval))
		goto badframe;
	return rval;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

asmlinkage int do_rt_sigreturn(struct pt_regs *regs)
{
	struct rt_sigframe *frame = (struct rt_sigframe *) regs->u_regs [UREG_FP];
	sigset_t set;
	int rval;

	if (verify_area(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = set;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
	
	if (rt_restore_ucontext(regs, &frame->uc, &rval))
		goto badframe;
	return rval;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

/*
 * Set up a signal frame.
 */

static int
setup_sigcontext(struct sigcontext *sc, struct pt_regs *regs,
			     unsigned long mask)
{
	int err = 0;

	/* copy the regs. they are first in sc  */
	err |= __copy_to_user(&sc->regs, regs, sizeof(struct pt_regs));
	/* then some other stuff */
	err |= __put_user(mask, &sc->oldmask);

	return err;
}

static inline int rt_setup_ucontext(struct ucontext *uc, struct pt_regs *regs)
{
	int err = 0;

	err |= __copy_to_user(&uc->uc_mcontext.regs, regs, sizeof(struct pt_regs));

	return err;
}

static inline void *
get_sigframe(struct k_sigaction *ka, struct pt_regs *regs, size_t frame_size)
{
	unsigned long sp = regs->u_regs[UREG_FP];

	/* This is the X/Open sanctioned signal stack switching.  */
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (!on_sig_stack(sp))
			sp = current->sas_ss_sp + current->sas_ss_size;
	}
	return (void *)((sp - frame_size) & -4UL);
}

static void setup_frame (int sig, struct k_sigaction *ka,
			 sigset_t *set, struct pt_regs *regs)
{
	struct sigframe *frame;
	unsigned long retcode;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof (*frame)))
		goto give_sigsegv;

	err |= __put_user((current->exec_domain
			   && current->exec_domain->signal_invmap
			   && sig < 32
			   ? current->exec_domain->signal_invmap[sig]
			   : sig),
			  &frame->sig);

	//err |= __put_user(&frame->sc, &frame->psc);

	if (_NSIG_WORDS > 1)
		err |= copy_to_user(frame->extramask, &set->sig[1],
				    sizeof(frame->extramask));

	/* for uClinux, we can access directly */
	err |= setup_sigcontext(&frame->sc, regs, set->sig[0]);
//vic	err |= copy_to_user (&frame->sc, &context, sizeof(context));


	/* Set up to return from userspace.  If provided, use a stub
	   already in userspace.  */
	if (ka->sa.sa_flags & SA_RESTORER) {
		regs->u_regs[UREG_I7] = (unsigned long)ka->sa.sa_restorer;
		//err |= __put_user(ka->sa.sa_restorer, &frame->pretcode);
	} else {
		regs->u_regs[UREG_I7] = ((unsigned long)(&frame->retcode[0])) >>1;
		//err |= __put_user(frame->retcode, &frame->pretcode);
		/* pfx __NR_sigreturn (0x9803)*/
		err |= __put_user(0x9800 + (__NR_sigreturn >> 5),
						(short *)(frame->retcode + 0));
		/* movi %g1,__NR_sigreturn (0x36e1)*/
		err |= __put_user(0x3401 + ((__NR_sigreturn & 0x1f) << 5),
						(short *)(frame->retcode + 2));
	 	/* trap 63 ; syscall (0x793f)*/
		err |= __put_user(0x793f, (short *)(frame->retcode + 4));
	 	/* trap 5 ; gdb trap */
		err |= __put_user(0x7905, (short *)(frame->retcode + 6));
	}

	if (err)
		goto give_sigsegv;

	/* Set up registers for signal handler */
	regs->u_regs[UREG_FP] = (unsigned long) frame;
	regs->u_regs[UREG_I0] = sig;
	regs->pc = (unsigned long) ka->sa.sa_handler;

	return;

give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
}

static void setup_rt_frame (int sig, struct k_sigaction *ka, siginfo_t *info,
			    sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe *frame;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	err |= __put_user((current->exec_domain
			   && current->exec_domain->signal_invmap
			   && sig < 32
			   ? current->exec_domain->signal_invmap[sig]
			   : sig),
			  &frame->sig);
	//err |= __put_user(&frame->info, &frame->pinfo);
	//err |= __put_user(&frame->uc, &frame->puc);
	err |= copy_siginfo_to_user(&frame->info, info);

	/* Create the ucontext.  */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	err |= __put_user((void *)current->sas_ss_sp,
			  &frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->u_regs[UREG_FP]),
			  &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= rt_setup_ucontext(&frame->uc, regs);
	err |= copy_to_user (&frame->uc.uc_sigmask, set, sizeof(*set));

	/* Set up to return from userspace.  */
	regs->u_regs[UREG_I7] = ((unsigned long)(&frame->retcode[0])) >>1;
	//err |= __put_user(frame->retcode, &frame->pretcode);
	/* pfx __NR_rt_sigreturn */
	err |= __put_user(0x9800 + (__NR_rt_sigreturn >> 5),
					(short *)(frame->retcode + 0));
	/* movi %g1,__NR_rt_sigreturn */
	err |= __put_user(0x3401 + ((__NR_rt_sigreturn & 0x1f) << 5),
					(short *)(frame->retcode + 2));
 	/* trap 63 ; syscall */
	err |= __put_user(0x793f, (short *)(frame->retcode + 4));
 	/* trap 5 ; gdb trap */
	err |= __put_user(0x7905, (short *)(frame->retcode + 6));

	if (err)
		goto give_sigsegv;

	/* Set up registers for signal handler */
	//vic	wrusp ((unsigned long) frame);
	regs->u_regs[UREG_FP] = (unsigned long) frame;
	regs->u_regs[UREG_I0] = sig;
	regs->u_regs[UREG_I1] = (unsigned long)(&frame->info);
	regs->u_regs[UREG_I2] = (unsigned long)(&frame->uc);
	regs->pc = (unsigned long) ka->sa.sa_handler;

	return;

give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
}

static inline void
handle_restart(struct pt_regs *regs, struct k_sigaction *ka, int has_handler)
{
	switch (regs->u_regs[UREG_I0]) {
	case -ERESTARTNOHAND:
		if (!has_handler)
			goto do_restart;
		regs->u_regs[UREG_I0] = -EINTR;
		break;

	case -ERESTARTSYS:
		if (has_handler && !(ka->sa.sa_flags & SA_RESTART)) {
			regs->u_regs[UREG_I0] = -EINTR;
			break;
		}
	/* fallthrough */
	case -ERESTARTNOINTR:
	do_restart:
		regs->u_regs[UREG_I0] = regs->orig_i0;
		regs->pc -= 2;
		break;
	}
}

/*
 * OK, we're invoking a handler
 */
static void
handle_signal(unsigned long sig, struct k_sigaction *ka,
			siginfo_t *info, sigset_t *oldset, struct pt_regs *regs)
{
	/* are we from a system call? */
	if (regs->sysc_no >= 0)
		/* If so, check system call restarting.. */
		handle_restart(regs, ka, 1);

	/* set up the stack frame */
	if (ka->sa.sa_flags & SA_SIGINFO)
		setup_rt_frame(sig, ka, info, oldset, regs);
	else
		setup_frame(sig, ka, oldset, regs);

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

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals
 * that the kernel can handle, and then we build all the user-level signal
 * handling stack-frames in one go after that.
 */
asmlinkage int do_signal(sigset_t *oldset, struct pt_regs *regs)
{
	siginfo_t info;
	struct k_sigaction *ka;

//vic	current->thread.esp0 = (unsigned long) regs;

	if (!oldset)
		oldset = &current->blocked;

	for (;;) {
		unsigned long signr;

		spin_lock_irq(&current->sigmask_lock);
		signr = dequeue_signal(&current->blocked, &info);
		spin_unlock_irq(&current->sigmask_lock);

		if (!signr)
			break;

		if ((current->ptrace & PT_PTRACED) && signr != SIGKILL) {
			current->exit_code = signr;
			current->state = TASK_STOPPED;

			/* Did we come from a system call? */
			if (regs->sysc_no >= 0) {
				/* Restart the system call the same way as
				   if the process were not traced.  */
				struct k_sigaction *ka =
					&current->sig->action[signr-1];
				int has_handler =
					(ka->sa.sa_handler != SIG_IGN &&
					 ka->sa.sa_handler != SIG_DFL);
				handle_restart(regs, ka, has_handler);
			}

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
			if (signr != SIGCHLD)
				continue;
			/* Check for SIGCHLD: it's special.  */
			while (sys_wait4(-1, NULL, WNOHANG, NULL) > 0)
				/* nothing */;
			continue;
		}

		if (ka->sa.sa_handler == SIG_DFL) {
			int exit_code = signr;

			/* Init gets no signals it doesn't want.  */
			if (current->pid == 1)
				continue;

			switch (signr) {
			case SIGCONT: case SIGCHLD:	
			case SIGWINCH: case SIGURG:
				continue;

			case SIGTSTP: case SIGTTIN: case SIGTTOU:
				if (is_orphaned_pgrp(current->pgrp))
					continue;
				/* FALLTHRU */

			case SIGSTOP:
				current->state = TASK_STOPPED;
				current->exit_code = signr;
				if (!(current->p_pptr->sig->action[SIGCHLD-1].sa.sa_flags &
					  SA_NOCLDSTOP))
					notify_parent(current, SIGCHLD);
				schedule();
				continue;

			case SIGQUIT: case SIGILL: case SIGTRAP:
			case SIGIOT: case SIGFPE: case SIGSEGV:
			case SIGBUS: case SIGSYS: case SIGXCPU: case SIGXFSZ:
				if (do_coredump(signr, regs))
					exit_code |= 0x80;
				/* FALLTHRU */

			default:
				sigaddset(&current->pending.signal, signr);
				recalc_sigpending(current);
				current->flags |= PF_SIGNALED;
				do_exit(exit_code);
				/* NOTREACHED */
			}
		}

		/* Whee!  Actually deliver the signal.  */
		handle_signal(signr, ka, &info, oldset, regs);
		return 1;
	}

	/* Did we come from a system call? */
	if (regs->sysc_no >= 0)
		/* Restart the system call - no handlers present */
		handle_restart(regs, NULL, 0);

	return 0;
}
