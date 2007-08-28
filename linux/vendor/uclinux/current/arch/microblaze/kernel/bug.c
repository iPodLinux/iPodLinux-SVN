/*
 * arch/v850/kernel/bug.c -- Bug reporting functions
 *
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/sched.h>

#include <asm/errno.h>
#include <asm/ptrace.h>
#include <asm/current.h>

void __bug (const char *file, int line, void *data)
{
	if (data)
		printk (KERN_CRIT "kernel BUG at %s:%d (data = %p)!\n",
			file, line, data);
	else
		printk (KERN_CRIT "kernel BUG at %s:%d!\n", file, line);

	machine_halt ();
}

int bad_trap (int trap_num, struct pt_regs *regs)
{
	printk (KERN_CRIT
		"unimplemented trap %d called at 0x%08lx, pid %d!\n",
		trap_num, regs->pc, current->pid);
	return -ENOSYS;
}

int debug_trap (struct pt_regs *regs)
{
	int i;
	printk (KERN_CRIT "debug trap\n");
        for(i=0;i<32;i++)
	{
		printk("r%i:%08X\t",i,regs->gpr[i]);
		if((i%4)==3)
			printk("\n");
	}
	printk("pc:%08X\tpsw:%08X\n",regs->pc, regs->psw);
	return -ENOSYS;
}

#ifdef CONFIG_RESET_GUARD
void unexpected_reset (unsigned long ret_addr, unsigned long kmode,
		       struct task_struct *task, unsigned long sp)
{
	printk (KERN_CRIT
		"unexpected reset in %s mode, pid %d"
		" (ret_addr = 0x%lx, sp = 0x%lx)\n",
		kmode ? "kernel" : "user",
		task ? task->pid : -1,
		ret_addr, sp);

	machine_halt ();
}
#endif /* CONFIG_RESET_GUARD */
