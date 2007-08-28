//vic FIXME - bad struct member: signal
/* ptrace.c: Sparc process tracing support.
 *
 * Copyright (C) 1996 David S. Miller (davem@caipfs.rutgers.edu)
 *
 * Based upon code written by Ross Biro, Linus Torvalds, Bob Manson,
 * and David Mosberger.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>

#include <asm/pgtable.h>
#include <asm/system.h>

#if 0
/* Returning from ptrace is a bit tricky because the syscall return
 * low level code assumes any value returned which is negative and
 * is a valid errno will mean setting the condition codes to indicate
 * an error return.  This doesn't work, so we have this hook.
 */
static inline void pt_error_return(struct pt_regs *regs, unsigned long error)
{
	regs->u_regs[UREG_I0] = error;
	regs->psr |= PSR_C;
	regs->pc = regs->npc;
	regs->npc += 4;
}

static inline void pt_succ_return(struct pt_regs *regs, unsigned long value)
{
	regs->u_regs[UREG_I0] = value;
	regs->psr &= ~PSR_C;
	regs->pc = regs->npc;
	regs->npc += 4;
}

/* Fuck me gently with a chainsaw... */
static inline void read_sunos_user(struct pt_regs *regs, unsigned long offset,
				   struct task_struct *tsk)
{
	struct pt_regs *cregs = tsk->thread.kregs;
	struct thread_struct *t = &tsk->thread;

	if(offset >= 1024)
		offset -= 1024; /* whee... */
	if(offset & ((sizeof(unsigned long) - 1))) {
		pt_error_return(regs, EIO);
		return;
	}
	if(offset >= 16 && offset < 784) {
		offset -= 16; offset >>= 2;
		pt_succ_return(regs, *(((unsigned long *)(&t->reg_window[0]))+offset));	
		return;
	}
	if(offset >= 784 && offset < 832) {
		offset -= 784; offset >>= 2;
		pt_succ_return(regs, *(((unsigned long *)(&t->rwbuf_stkptrs[0]))+offset));
		return;
	}
	switch(offset) {
	case 0:
		regs->u_regs[UREG_I0] = t->ksp;
		break;
	case 4:
		regs->u_regs[UREG_I0] = t->kpc;
		break;
	case 8:
		regs->u_regs[UREG_I0] = t->kpsr;
		break;
	case 12:
		regs->u_regs[UREG_I0] = t->uwinmask;
		break;
	case 832:
		regs->u_regs[UREG_I0] = t->w_saved;
		break;
	case 896:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I0];
		break;
	case 900:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I1];
		break;
	case 904:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I2];
		break;
	case 908:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I3];
		break;
	case 912:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I4];
		break;
	case 916:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I5];
		break;
	case 920:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I6];
		break;
	case 924:
		if(tsk->thread.flags & 0x80000000)
			regs->u_regs[UREG_I0] = cregs->u_regs[UREG_G1];
		else
			regs->u_regs[UREG_I0] = 0;
		break;
	case 940:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I0];
		break;
	case 944:
		regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I1];
		break;

	case 948:
		/* Isn't binary compatibility _fun_??? */
		if(cregs->psr & PSR_C)
			regs->u_regs[UREG_I0] = cregs->u_regs[UREG_I0] << 24;
		else
			regs->u_regs[UREG_I0] = 0;
		break;

		/* Rest of them are completely unsupported. */
	default:
		printk("%s [%d]: Wants to read user offset %d\n",
		       current->comm, current->pid, offset);
		pt_error_return(regs, EIO);
		return;
	}
	regs->psr &= ~PSR_C;
	regs->pc = regs->npc;
	regs->npc += 4;
	return;
}

static inline void write_sunos_user(struct pt_regs *regs, unsigned long offset,
				    struct task_struct *tsk)
{
	struct pt_regs *cregs = tsk->thread.kregs;
	struct thread_struct *t = &tsk->thread;
	unsigned long value = regs->u_regs[UREG_I3];

	if(offset >= 1024)
		offset -= 1024; /* whee... */
	if(offset & ((sizeof(unsigned long) - 1)))
		goto failure;
	if(offset >= 16 && offset < 784) {
		offset -= 16; offset >>= 2;
		*(((unsigned long *)(&t->reg_window[0]))+offset) = value;
		goto success;
	}
	if(offset >= 784 && offset < 832) {
		offset -= 784; offset >>= 2;
		*(((unsigned long *)(&t->rwbuf_stkptrs[0]))+offset) = value;
		goto success;
	}
	switch(offset) {
	case 896:
		cregs->u_regs[UREG_I0] = value;
		break;
	case 900:
		cregs->u_regs[UREG_I1] = value;
		break;
	case 904:
		cregs->u_regs[UREG_I2] = value;
		break;
	case 908:
		cregs->u_regs[UREG_I3] = value;
		break;
	case 912:
		cregs->u_regs[UREG_I4] = value;
		break;
	case 916:
		cregs->u_regs[UREG_I5] = value;
		break;
	case 920:
		cregs->u_regs[UREG_I6] = value;
		break;
	case 924:
		cregs->u_regs[UREG_I7] = value;
		break;
	case 940:
		cregs->u_regs[UREG_I0] = value;
		break;
	case 944:
		cregs->u_regs[UREG_I1] = value;
		break;

		/* Rest of them are completely unsupported or "no-touch". */
	default:
		printk("%s [%d]: Wants to write user offset %d\n",
		       current->comm, current->pid, offset);
		goto failure;
	}
success:
	pt_succ_return(regs, 0);
	return;
failure:
	pt_error_return(regs, EIO);
	return;
}

/* #define ALLOW_INIT_TRACING */
/* #define DEBUG_PTRACE */

#endif /* 0 */

asmlinkage void do_ptrace(struct pt_regs *regs)
{
#if 1
	return -ENOSYS;
#else /* 0 */
	unsigned long request = regs->u_regs[UREG_I0];
	unsigned long pid = regs->u_regs[UREG_I1];
	unsigned long addr = regs->u_regs[UREG_I2];
	unsigned long data = regs->u_regs[UREG_I3];
	unsigned long addr2 = regs->u_regs[UREG_I4];
	struct task_struct *child;

#ifdef DEBUG_PTRACE
	printk("do_ptrace: rq=%d pid=%d addr=%08lx data=%08lx addr2=%08lx\n",
	       (int) request, (int) pid, addr, data, addr2);
#endif
	if(request == PTRACE_TRACEME) {
		/* are we already being traced? */
		if (current->flags & PT_PTRACED) {
			pt_error_return(regs, EPERM);
			return;
		}
		/* set the ptrace bit in the process flags. */
		current->flags |= PT_PTRACED;
		pt_succ_return(regs, 0);
		return;
	}
#ifndef ALLOW_INIT_TRACING
	if(pid == 1) {
		/* Can't dork with init. */
		pt_error_return(regs, EPERM);
		return;
	}
#endif
	if(!(child = get_task(pid))) {
		pt_error_return(regs, ESRCH);
		return;
	}

	if(request == PTRACE_SUNATTACH) {
		if(child == current) {
			/* Try this under SunOS/Solaris, bwa haha
			 * You'll never be able to kill the process. ;-)
			 */
			pt_error_return(regs, EPERM);
			return;
		}
		if((!child->dumpable ||
		    (current->uid != child->euid) ||
		    (current->uid != child->uid) ||
		    (current->gid != child->egid) ||
		    (current->gid != child->gid)) && !suser()) {
			pt_error_return(regs, EPERM);
			return;
		}
		/* the same process cannot be attached many times */
		if (child->flags & PT_PTRACED) {
			pt_error_return(regs, EPERM);
			return;
		}
		child->flags |= PT_PTRACED;
		if(child->p_pptr != current) {
			REMOVE_LINKS(child);
			child->p_pptr = current;
			SET_LINKS(child);
		}
		send_sig(SIGSTOP, child, 1);
		pt_succ_return(regs, 0);
		return;
	}
	if(!(child->flags & PT_PTRACED)) {
		pt_error_return(regs, ESRCH);
		return;
	}
	if(child->state != TASK_STOPPED) {
		if(request != PTRACE_KILL) {
			pt_error_return(regs, ESRCH);
			return;
		}
	}
	if(child->p_pptr != current) {
		pt_error_return(regs, ESRCH);
		return;
	}
	switch(request) {
	case PTRACE_PEEKTEXT: /* read word at location addr. */ 
	case PTRACE_PEEKDATA: {
		unsigned long tmp;
		int res;

		/* Non-word alignment _not_ allowed on Sparc. */
		if(addr & (sizeof(unsigned long) - 1)) {
			pt_error_return(regs, EINVAL);
			return;
		}
		res = read_long(child, addr, &tmp);
		if (res < 0) {
			pt_error_return(regs, -res);
			return;
		}
		pt_succ_return(regs, tmp);
		return;
	}

	case PTRACE_PEEKUSR:
		read_sunos_user(regs, addr, child);
		return;

	case PTRACE_POKEUSR:
		write_sunos_user(regs, addr, child);
		return;

	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA: {
		struct vm_area_struct *vma;
		int res;

		/* Non-word alignment _not_ allowed on Sparc. */
		if(addr & (sizeof(unsigned long) - 1)) {
			pt_error_return(regs, EINVAL);
			return;
		}
		vma = find_extend_vma(child, addr);
		if(vma && request == PTRACE_POKEDATA && (vma->vm_flags & VM_EXEC)) {
			pt_error_return(regs, EIO);
			return;
		}
		res = write_long(child, addr, data);
		if(res < 0)
			pt_error_return(regs, -res);
		else
			pt_succ_return(regs, res);
		return;
	}

	case PTRACE_GETREGS: {
		struct pt_regs *pregs = (struct pt_regs *) addr;
		struct pt_regs *cregs = child->thread.kregs;
		int rval;

		rval = verify_area(VERIFY_WRITE, pregs, sizeof(struct pt_regs) - 4);
		if(rval) {
			pt_error_return(regs, rval);
			return;
		}
		pregs->psr = cregs->psr;
		pregs->pc = cregs->pc;
		pregs->npc = cregs->npc;
		pregs->y = cregs->y;
		for(rval = 1; rval < 16; rval++)
			pregs->u_regs[rval - 1] = cregs->u_regs[rval];
		pt_succ_return(regs, 0);
		return;
	}

	case PTRACE_SETREGS: {
		struct pt_regs *pregs = (struct pt_regs *) addr;
		struct pt_regs *cregs = child->thread.kregs;
		unsigned long psr;
		int rval, i;

		rval = verify_area(VERIFY_READ, pregs, sizeof(struct pt_regs) - 4);
		if(rval) {
			pt_error_return(regs, rval);
			return;
		}
		/* Must be careful, tracing process can only set certain
		 * bits in the psr.
		 */
		psr = (pregs->psr) & PSR_ICC;
		cregs->psr &= ~PSR_ICC;
		cregs->psr |= psr;
		if(!((pregs->pc | pregs->npc) & 3)) {
			cregs->pc = pregs->pc;
			cregs->npc = pregs->npc;
		}
		cregs->y = pregs->y;
		for(i = 1; i < 16; i++)
			cregs->u_regs[i] = pregs->u_regs[i-1];
		pt_succ_return(regs, 0);
		return;
	}

	case PTRACE_GETFPREGS: {
		struct fps {
			unsigned long regs[32];
			unsigned long fsr;
			unsigned long flags;
			unsigned long extra;
			unsigned long fpqd;
			struct fq {
				unsigned long *insnaddr;
				unsigned long insn;
			} fpq[16];
		} *fps = (struct fps *) addr;
		int rval, i;

		rval = verify_area(VERIFY_WRITE, fps, sizeof(struct fps));
		if(rval) { pt_error_return(regs, rval); return; }
		for(i = 0; i < 32; i++)
			fps->regs[i] = child->thread.float_regs[i];
		fps->fsr = child->thread.fsr;
		fps->fpqd = child->thread.fpqdepth;
		fps->flags = fps->extra = 0;
		for(i = 0; i < 16; i++) {
			fps->fpq[i].insnaddr = child->thread.fpqueue[i].insn_addr;
			fps->fpq[i].insn = child->thread.fpqueue[i].insn;
		}
		pt_succ_return(regs, 0);
		return;
	}

	case PTRACE_SETFPREGS: {
		struct fps {
			unsigned long regs[32];
			unsigned long fsr;
			unsigned long flags;
			unsigned long extra;
			unsigned long fpqd;
			struct fq {
				unsigned long *insnaddr;
				unsigned long insn;
			} fpq[16];
		} *fps = (struct fps *) addr;
		int rval, i;

		rval = verify_area(VERIFY_READ, fps, sizeof(struct fps));
		if(rval) { pt_error_return(regs, rval); return; }
		for(i = 0; i < 32; i++)
			child->thread.float_regs[i] = fps->regs[i];
		child->thread.fsr = fps->fsr;
		child->thread.fpqdepth = fps->fpqd;
		for(i = 0; i < 16; i++) {
			child->thread.fpqueue[i].insn_addr = fps->fpq[i].insnaddr;
			child->thread.fpqueue[i].insn = fps->fpq[i].insn;
		}
		pt_succ_return(regs, 0);
		return;
	}

	case PTRACE_READTEXT:
	case PTRACE_READDATA: {
		unsigned char *dest = (unsigned char *) addr2;
		unsigned long src = addr;
		unsigned char tmp;
		int res, len = data;

		res = verify_area(VERIFY_WRITE, (void *) dest, len);
		if(res) {
			pt_error_return(regs, -res);
			return;
		}
		while(len) {
			res = read_byte(child, src, &tmp);
			if(res < 0) {
				pt_error_return(regs, -res);
				return;
			}
			*dest = tmp;
			src++; dest++; len--;
		}
		pt_succ_return(regs, 0);
		return;
	}

	case PTRACE_WRITETEXT:
	case PTRACE_WRITEDATA: {
		unsigned char *src = (unsigned char *) addr2;
		unsigned long dest = addr;
		int res, len = data;

		res = verify_area(VERIFY_READ, (void *) src, len);
		if(res) {
			pt_error_return(regs, -res);
			return;
		}
		while(len) {
			res = write_byte(child, dest, *src);
			if(res < 0) {
				pt_error_return(regs, -res);
				return;
			}
			src++; dest++; len--;
		}
		pt_succ_return(regs, 0);
		return;
	}

	case PTRACE_SYSCALL: /* continue and stop at (return from) syscall */
		data = 0;
		addr = 1;

	case PTRACE_CONT: { /* restart after signal. */
		if ((unsigned long) data > NSIG) {
			pt_error_return(regs, EIO);
			return;
		}
		if (request == PTRACE_SYSCALL)
			child->flags |= PT_TRACESYS;
		else
			child->flags &= ~PT_TRACESYS;
		child->exit_code = data;
		if((addr != 1) & !(addr & 3)) {
			child->thread.kregs->pc = addr;
			child->thread.kregs->npc = addr + 4;
		}
		wake_up_process(child);
		pt_succ_return(regs, 0);
		return;
	}

/*
 * make the child exit.  Best I can do is send it a sigkill. 
 * perhaps it should be put in the status that it wants to 
 * exit.
 */
	case PTRACE_KILL: {
		if (child->state == TASK_ZOMBIE) {	/* already dead */
			pt_succ_return(regs, 0);
			return;
		}
		wake_up_process(child);
		child->exit_code = SIGKILL;
		pt_succ_return(regs, 0);
		return;
	}

	case PTRACE_SUNDETACH: { /* detach a process that was attached. */
		if ((unsigned long) data > NSIG) {
			pt_error_return(regs, EIO);
			return;
		}
		child->flags &= ~(PT_PTRACED|PT_TRACESYS);
		wake_up_process(child);
		child->exit_code = data;
		REMOVE_LINKS(child);
		child->p_pptr = child->p_opptr;
		SET_LINKS(child);
		pt_succ_return(regs, 0);
		return;
	}

	/* PTRACE_DUMPCORE unsupported... */

	default:
		pt_error_return(regs, EIO);
		return;
	}
#endif /* 0 */
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure the single step bit is not set.
 */
void ptrace_disable(struct task_struct *child)
{
	//vic - FIXME - ptrace not really implemented anyway
}

asmlinkage void syscall_trace(void)
{
#ifdef DEBUG_PTRACE
	printk("%s [%d]: syscall_trace\n", current->comm, current->pid);
#endif
	if ((current->flags & (PT_PTRACED|PT_TRACESYS))
			!= (PT_PTRACED|PT_TRACESYS))
		return;
	current->exit_code = SIGTRAP;
	current->state = TASK_STOPPED;
	current->thread.flags ^= 0x80000000;
	notify_parent(current, SIGTRAP);
	schedule();
	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
//vic FIXME - is this ever used ? - needs work to update signals
//vic	if (current->exit_code)
//vic		current->signal |= (1 << (current->exit_code - 1));
	current->exit_code = 0;
}
