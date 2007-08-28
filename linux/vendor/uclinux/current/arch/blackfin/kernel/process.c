/*
 *  linux/arch/frionommu/kernel/process.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 *
 *
 *  uClinux changes Copyright (C) 2000, Lineo, davidm@lineo.com
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

/*   Changes made for FRIO by Akbar Hussain Lineo Inc., July 2001  */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/reboot.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/setup.h>
#include <asm/pgtable.h>
#include <asm/segment.h>

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
__attribute__((section(".data.init_task"), aligned(KTHREAD_SIZE)))
	= { task: INIT_TASK(init_task_union.task) };

asmlinkage void ret_from_fork(void);


/*
 * The idle loop on FRIO 
 */
static void default_idle(void)
{
	while(1) {
		if (!current->need_resched)
			__asm__("idle;\n\t" : : : "cc"); 
	/* for FRIO idle_req bit of SEQSTAT is set, and also idle is supervisor
	 * mode insn, need "SSYNC" ???  Tony */

		schedule();
	}
}

void (*frio_idle)(void) = default_idle;

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	init_idle(); /* In sched.c (Generic) */
	current->nice = 20;
	current->counter = -100;
	frio_idle();
}

void machine_restart(char * __unused)
{
	if (mach_reset)
		mach_reset();  
/*  Frio 1st system event is for machine setup, see setup.c */
	for (;;);
}

void machine_halt(void)
{
	if (mach_halt)
		mach_halt(); /* NULL see setup.c, ? have hardware support */
	for (;;);
}

void machine_power_off(void)
{
	if (mach_power_off)
		mach_power_off(); /* NULL see setup.c */
	for (;;);
}

void show_regs(struct pt_regs * regs)
{
	printk("\n");
	printk("PC: %08lu  Status: %04lu  SysStatus: %04lu  RETS: %08lu\n",
	       regs->pc, regs->astat, regs->seqstat, regs->rets);
	printk("A0.x: %08lx  A0.w: %08lx  A1.x: %08lx  A1.w: %08lx\n",
	       regs->a0x, regs->a0w, regs->a1x, regs->a1w);
	printk("P0: %08lx  P1: %08lx  P2: %08lx  P3: %08lx\n",
	       regs->p0, regs->p1, regs->p2, regs->p3);
	printk("P4: %08lx  P5: %08lx\n",
	       regs->p4, regs->p5);
	printk("R0: %08lx  R1: %08lx  R2: %08lx  R3: %08lx\n",
	       regs->r0, regs->r1, regs->r2, regs->r3);
	printk("R4: %08lx  R5: %08lx  R6: %08lx  R7: %08lx\n",
	       regs->r4, regs->r5, regs->r6, regs->r7);

	/* PS_S should be 0x0c00, to expect the 0b00 value in SEQSTAT's bit 11:10 */
	if (!(regs->seqstat & PS_S))
		printk("USP: %08lx\n", rdusp());
}

/*
 * Create a kernel thread
 */
int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	long retval;
	long clone_arg = flags | CLONE_VM;
	unsigned long t_hold_sp;

	__asm__ __volatile__ (
			"r1 = sp; \n\t"		/* movel	%%sp, %%d2\n\t"*/
			"r0 = %6; \n\t"		/* movel	%5, %%d1\n\t" */
			"r5 = %2; \n\t"		/* movel	%1, %%d0\n\t" */
			"excpt 0; \n\t"		/* trap		#0\n\t" */
			"%1 = sp; \n\t"		/* cmpl		%%sp, %%d2\n\t" */
			"cc = %1 == r1; \n\t"
			"if cc jump 1f; \n\t"		/* jeq		1f\n\t" */
			"r0 = %4; \n\t"		/* movel	%3, %%sp@-\n\t"*/
			"call (%5); \n\t"	/* jsr		%4@\n\t" */
			"r5 = %3; \n\t"		/* movel	%2, %%d0\n\t" */
			"excpt 0; \n"		/* trap		#0\n" */
			"1:"
		: "=d" (retval), "=d" (t_hold_sp)
		: "i" (__NR_clone),
		  "i" (__NR_exit),
		  "a" (arg),
		  "a" (fn),
		  "a" (clone_arg)
		: "CC", "R0", "R1", "R2", "R5");

	return retval;
}

void flush_thread(void)
{
}

/*
 * "frio_fork()".. By the time we get here, the
 * non-volatile registers have also been saved on the
 * stack. We do some ugly pointer stuff here.. (see
 * also copy_thread)
 */

asmlinkage int frio_fork(struct pt_regs *regs)  
/* Need to change the name in /platform/.../entry.S */
{
#ifdef NO_MM
	/* fork almost works,  enough to trick you into looking elsewhere :-( */
	return(-EINVAL);
#else
	return do_fork(SIGCHLD, rdusp(), regs, 0);
#endif
}

asmlinkage int frio_vfork(struct pt_regs *regs)   
/* Need to change the name in /platform/.../entry.S */
{
	unsigned long newsp;

	newsp = regs->r1;
	if (!newsp)
	   newsp = rdusp();
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, rdusp(), regs, 0);
}

asmlinkage int frio_clone(struct pt_regs *regs) 
/* Need to change the name in /platform/.../entry.S */
{
	unsigned long clone_flags;
	unsigned long newsp;

	/* syscall2 puts clone_flags in r0 and usp in r1 */
	clone_flags = regs->r0;
	newsp = regs->r1;
	if (!newsp)
	   newsp = rdusp();
	return do_fork(clone_flags, newsp, regs, 0);
}

int copy_thread(int nr, unsigned long clone_flags,
		unsigned long usp, unsigned long topstk,
		struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;
	unsigned long stack_offset;

	stack_offset = KTHREAD_SIZE - sizeof(struct pt_regs);
	childregs = (struct pt_regs *) ((unsigned long) p + stack_offset);

	*childregs = *regs;
	childregs->r0 = 0;

	p->thread.usp = usp;

	/* eeks we should be this from copy_thread and not try to construct
	 * ourselves. We'll get in trouble if we get a sys_clone from user
	 * space - amit */

	p->thread.ksp = (unsigned long)childregs;
	p->thread.pc = (unsigned long)ret_from_fork;

	return 0;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{

/* changed the size calculations - should hopefully work better. lbt */
	dump->magic = CMAGIC;
	dump->start_code = 0;
	dump->start_stack = rdusp() & ~(PAGE_SIZE - 1);
	dump->u_tsize = ((unsigned long) current->mm->end_code) >> PAGE_SHIFT;
	dump->u_dsize = ((unsigned long) (current->mm->brk +
					  (PAGE_SIZE-1))) >> PAGE_SHIFT;
	dump->u_dsize -= dump->u_tsize;
	dump->u_ssize = 0;

	if (dump->start_stack < TASK_SIZE)
		dump->u_ssize = ((unsigned long) (TASK_SIZE - dump->start_stack)) >> PAGE_SHIFT;

	dump->u_ar0 = (struct user_regs_struct *)((int)&dump->regs - (int)dump);
	dump->regs.r0 = regs->r0;
	dump->regs.r1 = regs->r1;
	dump->regs.r2 = regs->r2;
	dump->regs.r3 = regs->r3;
	dump->regs.r4 = regs->r4;
	dump->regs.r5 = regs->r5;
	dump->regs.r6 = regs->r6;
	dump->regs.r7 = regs->r7;
	dump->regs.p0 = regs->p0;
	dump->regs.p1 = regs->p1;
	dump->regs.p2 = regs->p2;
	dump->regs.p3 = regs->p3;
	dump->regs.p4 = regs->p4;
	dump->regs.p5 = regs->p5;
	dump->regs.orig_r0 = regs->orig_r0;
	dump->regs.a0w = regs->a0w;
	dump->regs.a1w = regs->a1w;
	dump->regs.a0x = regs->a0x;
	dump->regs.a1x = regs->a1x;
	dump->regs.rets = regs->rets;
	dump->regs.astat = regs->astat;
	dump->regs.pc = regs->pc;
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(struct pt_regs regs)
{
	int error;
	char * filename;

	lock_kernel();
	filename = getname((char *)regs.r0);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, (char **)regs.r1, (char **)regs.r2, &regs);
	putname(filename);
out:
	unlock_kernel();
	return error;
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
	unsigned long fp, pc;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	stack_page = (unsigned long)p;
	fp = p->thread.usp;
	do {
		if (fp < stack_page+sizeof(struct task_struct) ||
		    fp >= 8184+stack_page)
			return 0;
		pc = ((unsigned long *)fp)[1];
		/* FIXME: This depends on the order of these functions. */
		if (pc < first_sched || pc >= last_sched)
			return pc;
		fp = *(unsigned long *) fp;
	} while (count++ < 16);
	return 0;
}
