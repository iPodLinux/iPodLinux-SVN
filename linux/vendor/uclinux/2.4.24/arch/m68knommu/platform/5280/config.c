/***************************************************************************/

/*
 *	linux/arch/m68knommu/platform/5280/config.c
 *
 * Copied from linux/arch/m68knommu/platform/5282/config.c
 * that is
 *	Copyright (C) 1999-2003, Greg Ungerer (gerg@snapgear.com)
 *	Copyright (C) 2001-2003, SnapGear Inc. (www.snapgear.com)
 */

/*
 * CHANGES
 * 20030916 hede (heiko.degenhardt@sentec-elektronik.de)
 * 	-	I changed the timer stuff according to MCF5282UM.pdf,
 * 		rev. 1, 4/2003, chapter 19 "Programmable Interrupt Timer
 * 		Modules".
 * 	-	I tried to let the timer period be 10ms, and to calculate
 * 		the offset in coldfire_timer_offset for an downcounting
 * 		timer.
 * 	FIXME: Could somebody verify my changes?
 * 
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/ledman.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfpit.h>
#include <asm/mcfsim.h>
#include <asm/mcfdma.h>
#include <asm/delay.h>

/***************************************************************************/

/*
 *	DMA channel base address table.
 */
unsigned int   dma_base_addr[MAX_DMA_CHANNELS] = {
        MCF_MBAR + MCFDMA_BASE0,
};

unsigned int dma_device_address[MAX_DMA_CHANNELS];

/***************************************************************************/

void coldfire_tick(void)
{
	volatile struct mcfpit	*tp;

	/* Reset the ColdFire timer */
	tp = (volatile struct mcfpit *) (MCF_IPSBAR + MCFPIT_BASE1);
	tp->pcsr |= MCFPIT_PCSR_PIF;
}

/***************************************************************************/

void coldfire_timer_init(void (*handler)(int, void *, struct pt_regs *))
{
	volatile unsigned char	*icrp;
	volatile unsigned long	*imrp;
	volatile struct mcfpit	*tp;

	request_irq(64+55, handler, SA_INTERRUPT, "ColdFire Timer", NULL);

	icrp = (volatile unsigned char *) (MCF_IPSBAR + MCFICM_INTC0 +
		MCFINTC_ICR0 + MCFINT_PIT1);
	*icrp = 0x2b; /* PIT1 with level 5, priority 3 */

	imrp = (volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRH);
	*imrp &= ~(1 << (55 - 32));

	/* Set up PIT timer 1 as poll clock */
	tp = (volatile struct mcfpit *) (MCF_IPSBAR + MCFPIT_BASE1);
	tp->pcsr = MCFPIT_PCSR_DISABLE;

	/*
	 * ATTENTION!
	 * We try to get a timeout period of 10ms.
	 * If we set up PMR to have 4999, and on impulse will be
	 * 2us, the timer period would be 5000 * 2us = 10ms.
	 * 
	 * According to mcfpit.h MCFPTI_PCSR_CLK64 is 0x0600, giving
	 * 0x0110 in PRE[3:0]. That means "System Clock Divisor" is
	 * 128 (see p. 19-4), including the "Divide by 2" in figure 19-1.
	 */
	tp->pmr = (((MCF_CLK / 128) / HZ) -1);
	tp->pcsr = MCFPIT_PCSR_EN | MCFPIT_PCSR_PIE | MCFPIT_PCSR_OVW |
		MCFPIT_PCSR_RLD | MCFPIT_PCSR_CLK64;
}

/***************************************************************************/

unsigned long coldfire_timer_offset(void)
{
	volatile struct mcfpit *tp;
	volatile unsigned long *ipr;
	unsigned long pmr, pcntr, offset;

	tp = (volatile struct mcfpit *) (MCF_IPSBAR + MCFPIT_BASE1);
	ipr = (volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IPRH);

	pmr = tp->pmr;
	pcntr = tp->pcntr;

	/*
	 * If we are still in the first half of the downcount and a
	 * timer interupt is pending, then add on a ticks worth of time.
	 *
	 * HINT: 100000 / HZ is 10000, that means 10000 us = 10 ms!
	 *
	 */
	offset = ((pmr - pcntr) * (1000000 / HZ)) / (pmr + 1);
	if (((offset * 2) < (1000000 / HZ)) && (*ipr & (1 << (55 - 32))))
		offset += 1000000 / HZ;
	return offset;	
}

/***************************************************************************/

/*
 *	Program the vector to be an auto-vectored.
 */

void mcf_autovector(unsigned int vec)
{
	/* Everything is auto-vectored on the 5282 */
}

/***************************************************************************/

extern e_vector	*_ramvec;

void set_evector(int vecnum, void (*handler)(void))
{
	if (vecnum >= 0 && vecnum <= 255)
		_ramvec[vecnum] = handler;
}

/***************************************************************************/

/* assembler routines */
asmlinkage void buserr(void);
asmlinkage void trap(void);
asmlinkage void system_call(void);
asmlinkage void inthandler(void);

void coldfire_trap_init(void)
{
	int i;

	*((volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRH)) = 0xffffffff;
	*((volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRL)) = 0xffffffff;

	/*
	 *	There is a common trap handler and common interrupt
	 *	handler that handle almost every vector. We treat
	 *	the system call and bus error special, they get their
	 *	own first level handlers.
	 */
#ifndef ENABLE_dBUG
	for (i = 3; (i <= 23); i++)
		_ramvec[i] = trap;
	for (i = 33; (i <= 63); i++)
		_ramvec[i] = trap;
#endif

	for (i = 24; (i <= 30); i++)
		_ramvec[i] = inthandler;
#ifndef ENABLE_dBUG
	_ramvec[31] = inthandler;  // Disables the IRQ7 button
#endif

	for (i = 64; (i < 255); i++)
		_ramvec[i] = inthandler;
	_ramvec[255] = 0;

	_ramvec[2] = buserr;
	_ramvec[32] = system_call;
}

/***************************************************************************/

/*
 *	Generic dumping code. Used for panic and debug.
 */

void dump(struct pt_regs *fp)
{
	extern unsigned int sw_usp, sw_ksp;
	unsigned long	*sp;
	unsigned char	*tp;
	int		i;

	printk("\nCURRENT PROCESS:\n\n");
	printk("COMM=%s PID=%d\n", current->comm, current->pid);

	if (current->mm) {
		printk("TEXT=%08x-%08x DATA=%08x-%08x BSS=%08x-%08x\n",
			(int) current->mm->start_code,
			(int) current->mm->end_code,
			(int) current->mm->start_data,
			(int) current->mm->end_data,
			(int) current->mm->end_data,
			(int) current->mm->brk);
		printk("USER-STACK=%08x  KERNEL-STACK=%08x\n\n",
			(int) current->mm->start_stack,
			(int) ((unsigned long) current) + KTHREAD_SIZE);
	}

	printk("PC: %08lx\n", fp->pc);
	printk("SR: %08lx    SP: %08lx\n", (long) fp->sr, (long) fp);
	printk("d0: %08lx    d1: %08lx    d2: %08lx    d3: %08lx\n",
		fp->d0, fp->d1, fp->d2, fp->d3);
	printk("d4: %08lx    d5: %08lx    a0: %08lx    a1: %08lx\n",
		fp->d4, fp->d5, fp->a0, fp->a1);
	printk("\nUSP: %08x   KSP: %08x   TRAPFRAME: %08x\n",
		sw_usp, sw_ksp, (unsigned int) fp);

	printk("\nCODE:");
	tp = ((unsigned char *) fp->pc) - 0x20;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x40);  i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nKERNEL STACK:");
	tp = ((unsigned char *) fp) - 0x40;
	for (sp = (unsigned long *) tp, i = 0; (i < 0xc0); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");
	printk("\n");

	printk("\nUSER STACK:");
	tp = (unsigned char *) (sw_usp - 0x10);
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n\n");
}

/***************************************************************************/

void coldfire_reset(void)
{
	HARD_RESET_NOW();
}

/***************************************************************************/

void config_BSP(char *commandp, int size)
{
#ifdef CONFIG_BOOTPARAM
	strncpy(commandp, CONFIG_BOOTPARAM_STRING, size);
	commandp[size-1] = 0;
#else
	memset(commandp, 0, size);
#endif

	mach_sched_init = coldfire_timer_init;
	mach_tick = coldfire_tick;
	mach_trap_init = coldfire_trap_init;
	mach_reset = coldfire_reset;
	mach_gettimeoffset = coldfire_timer_offset;
}

/***************************************************************************/
