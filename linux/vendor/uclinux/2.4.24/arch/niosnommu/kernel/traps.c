/*FIXME - some unimplemented stuff to do */

/*
 * arch/niosnommu/kernel/traps.c
 *
 * Copyright 2001 Vic Phillips
 *
 * hacked from:
 *
 * arch/sparcnommu/kernel/traps.c
 *
 * Copyright 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/sched.h>  /* for jiffies */
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/signal.h>

#include <asm/delay.h>
#include <asm/system.h>
#include <asm/ptrace.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/unistd.h>
#include <asm/traps.h>

#include <asm/nios.h>
#include "hex.h"

/* #define TRAP_DEBUG */

static void puts(unsigned char *s) {
	while(*s) {
		while (!(nasys_printf_uart->np_uartstatus & np_uartstatus_trdy_mask));
		nasys_printf_uart->np_uarttxdata = *s++;
	}
	while (!(nasys_printf_uart->np_uartstatus & np_uartstatus_trdy_mask));
	nasys_printf_uart->np_uarttxdata = '\r';
	while (!(nasys_printf_uart->np_uartstatus & np_uartstatus_trdy_mask));
	nasys_printf_uart->np_uarttxdata = '\n';
}

#if 0
void dumpit(unsigned long l1, unsigned long l2)
{
	outhex32(l1);
	puts("l1");
	outhex32(l2);
	puts("l2");
	while(1);
}

struct trap_trace_entry {
	unsigned long pc;
	unsigned long type;
};

int trap_curbuf = 0;
struct trap_trace_entry trapbuf[1024];

void syscall_trace_entry(struct pt_regs *regs)
{
	printk("%s[%d]: ", current->comm, current->pid);
	printk("scall<%d> (could be %d)\n", (int) regs->u_regs[UREG_G1],
	       (int) regs->u_regs[UREG_I0]);
}

void syscall_trace_exit(struct pt_regs *regs)
{
}
#endif

void die_if_kernel(char *str, struct pt_regs *pregs)
{
	unsigned long pc;

	pc = pregs->pc;
	puts("");
	outhex32(pc);
	puts(" trapped to die_if_kernel");
	show_regs(pregs);
#if 0
	if(pregs->psr & PSR_SUPERVISOR)
		do_exit(SIGKILL);
	do_exit(SIGSEGV);
#endif
}

void do_hw_interrupt(unsigned long type, unsigned long psr, unsigned long pc)
{
	if(type < 0x10) {
		printk("Unimplemented Nios TRAP, type = %02lx\n", type);
		die_if_kernel("Whee... Hello Mr. Penguin", current->thread.kregs);
	}	
#if 0
	if(type == SP_TRAP_SBPT) {
		send_sig(SIGTRAP, current, 1);
		return;
	}
	current->thread.sig_desc = SUBSIG_BADTRAP(type - 0x80);
	current->thread.sig_address = pc;
	send_sig(SIGILL, current, 1);
#endif
}

#if 0
void handle_watchpoint(struct pt_regs *regs, unsigned long pc, unsigned long psr)
{
#ifdef TRAP_DEBUG
	printk("Watchpoint detected at PC %08lx PSR %08lx\n", pc, psr);
#endif
	if(psr & PSR_SUPERVISOR)
		panic("Tell me what a watchpoint trap is, and I'll then deal "
		      "with such a beast...");
}
#endif

void trap_init(void)
{
#ifdef DEBUG
	printk("trap_init reached\n");
#endif
}
