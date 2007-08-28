/***************************************************************************/

/*
 *	linux/arch/m68knommu/platform/5206e/config.c
 *
 *	Copyright (C) 1999-2002, Greg Ungerer (gerg@snapgear.com)
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/ledman.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcftimer.h>
#include <asm/mcfsim.h>
#include <asm/mcfdma.h>

#ifdef CONFIG_NETtel
#include <asm/irq.h>
#include <asm/delay.h>
#endif

/***************************************************************************/

#ifdef CONFIG_NETtel
void	reset_setupbutton(void);
#endif

/***************************************************************************/

/*
 *	DMA channel base address table.
 */
unsigned int   dma_base_addr[MAX_DMA_CHANNELS] = {
        MCF_MBAR + MCFDMA_BASE0,
        MCF_MBAR + MCFDMA_BASE1,
};

unsigned int dma_device_address[MAX_DMA_CHANNELS];

/***************************************************************************/

void coldfire_tick(void)
{
	volatile unsigned char	*timerp;

	/* Reset the ColdFire timer */
	timerp = (volatile unsigned char *) (MCF_MBAR + MCFTIMER_BASE1);
	timerp[MCFTIMER_TER] = MCFTIMER_TER_CAP | MCFTIMER_TER_REF;
}

/***************************************************************************/

void coldfire_timer_init(void (*handler)(int, void *, struct pt_regs *))
{
	volatile unsigned short	*timerp;
	volatile unsigned char	*icrp;

	/* Set up TIMER 1 as poll clock */
	timerp = (volatile unsigned short *) (MCF_MBAR + MCFTIMER_BASE1);
	timerp[MCFTIMER_TMR] = MCFTIMER_TMR_DISABLE;

	timerp[MCFTIMER_TRR] = (unsigned short) ((MCF_CLK / 16) / HZ);
	timerp[MCFTIMER_TMR] = MCFTIMER_TMR_ENORI | MCFTIMER_TMR_CLK16 |
		MCFTIMER_TMR_RESTART | MCFTIMER_TMR_ENABLE;

	icrp = (volatile unsigned char *) (MCF_MBAR + MCFSIM_TIMER1ICR);

#ifdef CONFIG_NETtel
	*icrp = MCFSIM_ICR_AUTOVEC | MCFSIM_ICR_LEVEL6 | MCFSIM_ICR_PRI3;
	request_irq(30, handler, SA_INTERRUPT, "ColdFire Timer", NULL);
	reset_setupbutton();
#else
	*icrp = MCFSIM_ICR_AUTOVEC | MCFSIM_ICR_LEVEL5 | MCFSIM_ICR_PRI3;
	request_irq(29, handler, SA_INTERRUPT, "ColdFire Timer", NULL);
#endif
	mcf_setimr(mcf_getimr() & ~MCFSIM_IMR_TIMER1);
}

/***************************************************************************/

unsigned long coldfire_timer_offset(void)
{
	volatile unsigned short *timerp;
	unsigned long		trr, tcn, offset;

	timerp = (volatile unsigned short *) (MCF_MBAR + MCFTIMER_BASE1);
	tcn = timerp[MCFTIMER_TCN];
	trr = timerp[MCFTIMER_TRR];

	/*
	 * If we are still in the first half of the upcount and a
	 * timer interupt is pending, then add on a ticks worth of time.
	 */
	offset = ((tcn * (1000000 / HZ)) / trr);
	if (((offset * 2) < (1000000 / HZ)) && (mcf_getipr() & MCFSIM_IMR_TIMER1))
		offset += 1000000 / HZ;
	return offset;	
}

/***************************************************************************/

/*
 *	Program the vector to be an auto-vectored.
 */

void mcf_autovector(unsigned int vec)
{
	volatile unsigned char  *mbar;
	unsigned char		icr;

	if ((vec >= 25) && (vec <= 31)) {
		vec -= 25;
		mbar = (volatile unsigned char *) MCF_MBAR;
		icr = MCFSIM_ICR_AUTOVEC | (vec << 3);
		*(mbar + MCFSIM_ICR1 + vec) = icr;
		vec = 0x1 << (vec + 1);
		mcf_setimr(mcf_getimr() & ~vec);
	}
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
#ifdef MCF_MEMORY_PROTECT
	extern unsigned long _end;
	extern unsigned long memory_end;
#endif

#ifndef ENABLE_dBUG
	mcf_setimr(MCFSIM_IMR_MASKALL);
#endif

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
			(int) (((unsigned long) current) + 2 * PAGE_SIZE));
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

#if 1
	printk("\nUSER STACK:");
	tp = (unsigned char *) sw_usp;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n\n");
#endif
}

/***************************************************************************/

#ifdef CONFIG_NETtel

/*
 *	Routines to support the NETtel software reset button.
 */
void reset_button(int irq, void *dev_id, struct pt_regs *regs)
{
	static int	inbutton = 0;

	/*
	 *	IRQ7 is not maskable by the CPU core. It is possible
	 *	that switch bounce may get us back here before we have
	 *	really serviced the interrupt.
	 */
	if (inbutton)
		return;
	inbutton = 1;

	/* Disable interrupt at SIM - best we can do... */
	mcf_setimr(mcf_getimr() | MCFSIM_IMR_EINT7);

#ifdef CONFIG_LEDMAN
	ledman_signalreset();
#endif

	/* Don't leave here 'till button is no longer pushed! */
	for (;;) {
		if ((mcf_getipr() & MCFSIM_IMR_EINT7) == 0)
			break;
	}

#ifndef CONFIG_LEDMAN
	HARD_RESET_NOW();
	/* Should never get here... */
#endif

	inbutton = 0;
	/* Interrupt service done, enable it again */
	mcf_setimr(mcf_getimr() & ~MCFSIM_IMR_EINT7);
}

/***************************************************************************/

void reset_setupbutton(void)
{
	mcf_autovector(31);
	request_irq(31, reset_button, (SA_INTERRUPT | IRQ_FLG_FAST),
		"Reset Button", NULL);
}

#endif /* CONFIG_NETtel */

/***************************************************************************/

void config_BSP(char *commandp, int size)
{
#ifdef CONFIG_BOOTPARAM
	strncpy(commandp, CONFIG_BOOTPARAM_STRING, size);
	commandp[size-1] = 0;
#elif defined(CONFIG_NETtel)
	/* Copy command line from FLASH to local buffer... */
	memcpy(commandp, (char *) 0xf0004000, size);
	commandp[size-1] = 0;
#else
	memset(commandp, 0, size);
#endif

	mach_sched_init = coldfire_timer_init;
	mach_tick = coldfire_tick;
	mach_trap_init = coldfire_trap_init;
	mach_gettimeoffset = coldfire_timer_offset;
}

/***************************************************************************/
