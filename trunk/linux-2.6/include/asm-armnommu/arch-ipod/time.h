/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ARCH_ASM_TIME__
#define __ARCH_ASM_TIME__

#include <asm/arch/timex.h>

/*
 * Time(r) routines for kernel/time.c.
 *
 * The entry point from the kerenl is setup_timer() whose main
 * purpose is to start the timer ticking.  It is also used to
 * supply the kernel with out time(r) functions.
 */

/*
 * Set the RTC
 */
int ipod_set_rtc(void)
{
	return (0);
}

#define IPOD_TIMER_STATUS 0xcf001110

unsigned long ipod_gettimeoffset(void)
{
	return inl(IPOD_TIMER_STATUS);
}

static irqreturn_t ipod_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	do_timer(regs);
	do_profile(regs);
										
	return IRQ_HANDLED;
}


/* */
extern struct irqaction timer_irq;

/*
 * Set up timer interrupt, and the current time in seconds.
 *
 * This function is called from kernel/time.c:time_init.  It
 * must start the timer ticking.  The RTC can be set later though.
 *
 */
void __init time_init(void)
{
	/* store in kernel's function pointer */
	gettimeoffset = ipod_gettimeoffset;
	set_rtc = ipod_set_rtc;

	/* set up the timer interrupt */
	timer_irq.handler = ipod_timer_interrupt;

	/* clear timer1 */
	outl(0x0, 0xcf001100);
	inl(0xcf001104);

	/* enable timer, period, trigger value 0x2710 -> 100Hz */
	outl(0xc0002710, 0xcf001100);

	setup_irq(TIMER1_IRQ, &timer_irq);
	// enable_irq(TIMER1_IRQ);
}

#endif

