/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ARCH_ASM_TIME__
#define __ARCH_ASM_TIME__

/*
 * Time(r) routines for kernel/time.c.
 *
 * The entry point from the kerenl is setup_timer() whose main
 * purpose is to start the timer ticking.  It is also used to
 * supply the kernel with out time(r) functions.
 */

extern unsigned long ipod_gettimeoffset(void);
extern int ipod_set_rtc(void);
extern void ipod_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);

/* */
extern struct irqaction timer_irq;

/*
 * Set up timer interrupt, and the current time in seconds.
 *
 * This function is called from kernel/time.c:time_init.  It
 * must start the timer ticking.  The RTC can be set later though.
 *
 */
extern __inline__ void setup_timer(void)
{
	/* store in kernel's function pointer */
	gettimeoffset = ipod_gettimeoffset;
	set_rtc = ipod_set_rtc;

	/* set up the timer interrupt */
	timer_irq.handler = ipod_timer_interrupt;

	if ((ipod_get_hw_version() >> 16) > 0x3) {
		/* clear timer1 */
		outl(0x0, PP5020_TIMER1);
		inl(PP5020_TIMER1_ACK);

		/* enable timer, period, trigger value 0x2710 -> 100Hz */
		outl(0xc0000000 | USECS_PER_INT, PP5020_TIMER1);
	}
	else {
		/* clear timer1 */
		outl(0x0, PP5002_TIMER1);
		inl(PP5002_TIMER1_ACK);

		/* enable timer, period, trigger value 0x2710 -> 100Hz */
		outl(0xc0000000 | USECS_PER_INT, PP5002_TIMER1);
	}

	setup_arm_irq(TIMER1_IRQ, &timer_irq);
	enable_irq(TIMER1_IRQ);
}

#endif

