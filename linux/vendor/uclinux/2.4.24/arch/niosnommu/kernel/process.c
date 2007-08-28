/*
 *  Mar 2, 2001 - Ken Hill
 *     Implement Nios architecture specifics
 *
 *  linux/arch/sparc/kernel/process.c
 *
 *  Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>

#include <asm/uaccess.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/delay.h>
#include <asm/processor.h>
#include <asm/psr.h>

/*
 * Initial task structure. Make this a per-architecture thing,
 * because different architectures tend to have different
 * alignment requirements and potentially different initial
 * setup.
 */
static struct fs_struct init_fs = INIT_FS;
static struct files_struct init_files = INIT_FILES;
static struct signal_struct init_signals = INIT_SIGNALS;
struct mm_struct init_mm = INIT_MM(init_mm);

union task_union init_task_union
__attribute__((section(".data.init_task"), aligned(THREAD_SIZE)))
	= { task: INIT_TASK(init_task_union.task) };


/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	init_idle();
	current->nice = 20;
	current->counter = -100;

	while (1) {
		while (!current->need_resched) {
		}
		schedule();
	}
}

extern char saved_command_line[];

void machine_restart(char * __unused)
{
	//vic FIXME - what should we really do here ?
	asm("trap 0");
}

void machine_halt(void)
{
	asm("trap 0");				/* trap to deamon */
}

void machine_power_off(void)
{
	//vic FIXME - what should we really do here ?
	asm("trap 0");
}

void hard_reset_now(void)
{
#if 0
	panic("Reboot failed!");
#else
	asm("trap 0");				/* this traps back to monitor */
#endif
}

void show_regwindow(struct reg_window *rw)
{
	printk("l0:%08lx l1:%08lx l2:%08lx l3:%08lx l4:%08lx l5:%08lx l6:%08lx l7:%08lx\n",
	       rw->locals[0], rw->locals[1], rw->locals[2], rw->locals[3],
	       rw->locals[4], rw->locals[5], rw->locals[6], rw->locals[7]);
	printk("i0:%08lx i1:%08lx i2:%08lx i3:%08lx i4:%08lx i5:%08lx i6:%08lx i7:%08lx\n",
	       rw->ins[0], rw->ins[1], rw->ins[2], rw->ins[3],
	       rw->ins[4], rw->ins[5], rw->ins[6], rw->ins[7]);
}

void show_regs(struct pt_regs * regs)
{
        printk("PSR: %08lx PC: %08lx\n", regs->psr, regs->pc);
	printk("%%g0: %08lx %%g1: %08lx %%g2: %08lx %%g3: %08lx\n",
	       regs->u_regs[0], regs->u_regs[1], regs->u_regs[2],
	       regs->u_regs[3]);
	printk("%%g4: %08lx %%g5: %08lx %%g6: %08lx %%g7: %08lx\n",
	       regs->u_regs[4], regs->u_regs[5], regs->u_regs[6],
	       regs->u_regs[7]);
	printk("%%o0: %08lx %%o1: %08lx %%o2: %08lx %%o3: %08lx\n",
	       regs->u_regs[8], regs->u_regs[9], regs->u_regs[10],
	       regs->u_regs[11]);
	printk("%%o4: %08lx %%o5: %08lx %%sp: %08lx %%ret_pc: %08lx\n",
	       regs->u_regs[12], regs->u_regs[13], regs->u_regs[14],
	       regs->u_regs[15]);
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
}

/*
 * Free old dead task when we know it can never be on the cpu again.
 */
void release_thread(struct task_struct *dead_task)
{
	/* nothing to do ... */
}

void flush_thread(void)
{
	current->thread.sig_address = 0;
	current->thread.sig_desc = 0;

	/* Now, this task is no longer a kernel thread. */
	current->thread.flags &= ~NIOS_FLAG_KTHREAD;
}

/*
 * Copy a Nios thread.  The fork() return value conventions:
 * 
 * Parent -->  %o0 == childs  pid, %o1 == 0
 * Child  -->  %o0 == 0, 	   %o1 == 1
 *
 * NOTE: We have a separate fork kpsr because
 *       the parent could change this value between
 *       sys_fork invocation and when we reach here
 *       if the parent should sleep while trying to
 *       allocate the task_struct and kernel stack in
 *       do_fork().
 */
extern void ret_sys_call(void);

int copy_thread(int nr, unsigned long clone_flags, unsigned long sp,
		unsigned long unused,
		 struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs *childregs;
	struct reg_window *old_stack, *new_stack;
	unsigned long stack_offset;

	/* Calculate offset to stack_frame & pt_regs */
	stack_offset = (THREAD_SIZE - TRACEREG_SZ);

	if(regs->psr & PSR_SUPERVISOR)
		stack_offset -= REGWIN_SZ;
	childregs = ((struct pt_regs *) ((unsigned long) p + stack_offset));
	*childregs = *regs;
	new_stack = (((struct reg_window *) childregs) - 1);
	old_stack = (((struct reg_window *) regs) - 1);
	*new_stack = *old_stack;
	p->thread.ksp = (unsigned long) new_stack;
	p->thread.kpc = (((unsigned long) ret_sys_call));
	/* Start the new process with a fresh register window */
	p->thread.kpsr = (current->thread.fork_kpsr & ~PSR_CWP) | ((get_hi_limit() - 1) << 4);

	/* If we decide to split the register file up for multiple
	   task usage we will need this. Single register file usage
	   for now so do not use.

	p->thread.kwvalid = current->thread.fork_kwvalid;
	*/

	p->thread.kregs = childregs;
	/* Start the new process with a fresh register window */
	childregs->psr = (childregs->psr & ~PSR_CWP) | (get_hi_limit() << 4);

	if(regs->psr & PSR_SUPERVISOR) {
		stack_offset += TRACEREG_SZ;
		childregs->u_regs[UREG_FP] = (unsigned long) p + stack_offset;
		new_stack->ins[6]=childregs->u_regs[UREG_FP];
		memcpy(childregs->u_regs[UREG_FP], regs->u_regs[UREG_FP], REGWIN_SZ);
		p->thread.flags |= NIOS_FLAG_KTHREAD;
	} else {
		if (sp!=childregs->u_regs[UREG_FP]){
			/* clone */
			childregs->u_regs[UREG_FP] = (unsigned long) sp - REGWIN_SZ;
			new_stack->ins[6]=childregs->u_regs[UREG_FP];
			memcpy(childregs->u_regs[UREG_FP], regs->u_regs[UREG_FP], REGWIN_SZ);
		}
		p->thread.flags &= ~NIOS_FLAG_KTHREAD;
	}

	/* Set the return value for the child. */
	childregs->u_regs[UREG_I0] = 0;
	childregs->u_regs[UREG_I1] = 1;

	/* Set the return value for the parent. */
	regs->u_regs[UREG_I1] = 0;

	return 0;
}

/*
 * This is the mechanism for creating a new kernel thread.
 *
 * NOTE! Only a kernel-only process(ie the swapper or direct descendants
 * who haven't done an "execve()") should use this: it will work within
 * a system call from a "real" process, but the process memory space will
 * not be free'd until both the parent and the child have exited.
 */
pid_t kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	long retval;
	register long clone_arg __asm__ ("%o0") = flags | CLONE_VM;
	__asm__ __volatile(
		"pfx	%%hi(%1)\n\t"
		"movi	%%g1,%%lo(%1)\n\t"	/* syscall # */
		"movi	%%o1, 0\n\t"			/* usp arg == 0 */
		"trap	63\n\t"					/* Linux/nios clone(). */
		"cmpi	%%o1, 0\n\t"			/* 2nd arg returned in %o1 */
		"skps	cc_ne\n\t"
		"br	1f\n\t"					/* The parent, just return. */
		"nop\n\t"						/* Delay slot. */
		"call	%3\n\t"					/* Call the function. */
		"mov	%%o0, %4\n\t"			/* Get the arg (in delay slot) */
		"pfx	%%hi(%2)\n\t"
		"movi	%%g1, %%lo(%2)\n\t"	/* syscall # */
		"trap	63\n\t"					/* Linux/nios exit(). */
		   /* Not reached by child. */
		"1:"
		"mov	%0, %%o0\n\t"			/* arg returned in o0 */
		: "=r" (retval)
		: "i" (__NR_clone), "i" (__NR_exit), "r" (fn), "r" (arg), "r" (clone_arg)
		: "g1","o0","o1");
	return retval;
//vic - may not be able to return argument in %g1 - "ret_trap_entry"
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
	unsigned long first_stack_page;

	dump->magic = CMAGIC;
	dump->start_code = 0;
	dump->start_stack = regs->u_regs[UREG_FP] & ~(PAGE_SIZE - 1);
	dump->regs.psr = regs->psr;
	dump->regs.pc = regs->pc;
	/* fuck me plenty */
	memcpy(&dump->regs, regs, sizeof(regs));
	dump->u_tsize = (((unsigned long) current->mm->end_code) -
		((unsigned long) current->mm->start_code)) & ~(PAGE_SIZE - 1);
	dump->u_dsize = ((unsigned long) (current->mm->brk + (PAGE_SIZE-1)));
	dump->u_dsize -= dump->u_tsize;
	dump->u_dsize &= ~(PAGE_SIZE - 1);
	first_stack_page = (regs->u_regs[UREG_FP] & ~(PAGE_SIZE - 1));
	dump->u_ssize = (TASK_SIZE - first_stack_page) & ~(PAGE_SIZE - 1);
}

/*
 * fill in the fpu structure for a core dump.
 */
int dump_fpu (void *fpu_structure)
{
	/* Currently we report that we couldn't dump the fpu structure */
	return 0;
}

/*
 * nios_execve() executes a new program after the asm stub has set
 * things up for us.  This should basically do what I want it to.
 */
asmlinkage int nios_execve(struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname((char *)regs->u_regs[UREG_I0]);
	error = PTR_ERR(filename);
	if(IS_ERR(filename))
		return error;

	error = do_execve(filename, (char **) regs->u_regs[UREG_I1],
			  (char **) regs->u_regs[UREG_I2], regs);
	putname(filename);
	return error;
}

void show_trace_task(struct task_struct *tsk)
{
	printk("STACK ksp=0x%lx\n", tsk->thread.ksp);
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched	((unsigned long) scheduling_functions_start_here)
#define last_sched	((unsigned long) scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long fp,pc;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	stack_page = (unsigned long)p;
	fp = p->thread.ksp;
	do {
		if (fp < stack_page+sizeof(struct task_struct) ||
		    fp >= THREAD_SIZE-REGWIN_SZ+stack_page)
			return 0;
		pc = ((struct reg_window *)fp)->ins[7]; /* saved return address */
		if (pc < first_sched || pc >= last_sched)
			return pc;
		fp = ((struct reg_window *)fp)->ins[6];
	} while (count++ < 16);
	return 0;
}
