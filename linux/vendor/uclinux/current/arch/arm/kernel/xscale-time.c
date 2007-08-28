/*
 * linux/arch/arm/kernel/xscale-time.c
 *
 * This file implements a timer based on the XScale's internal
 * performance monitoring clock counter.  This code exists for
 * boards without external timers to be developed on or during 
 * early hardware bring up if the external timer does not
 * work.  To utilize it, CONFIG_XSCALE_PMU_TIMER should be
 * set by the configuration scripts. You also need to have
 * a #define for IRQ_XSCALE_PMU that maps the PMU interrupt
 * on your specific board/processor.
 * 
 * Author:  Nicolas Pitre
 * Copyright:   (C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>
#include <asm/hardware.h>

#include <asm/mach-types.h>

extern int setup_arm_irq(int, struct irqaction *);

/* IRQs are disabled before entering here from do_gettimeofday() */
static unsigned long xscale_pmu_gettimeoffset (void)
{
	unsigned long elapsed, usec;

	/* We need elapsed timer ticks since last interrupt */
	/* 
	 * Read the CCNT value.  The returned value is 
	 * between -LATCH and 0, 0 corresponding to a full jiffy 
	 */
	asm volatile ("mrc p14, 0, %0, c1, c0, 0" : "=r" (elapsed));
	elapsed += LATCH;

	/* Now convert them to usec */
	usec = (unsigned long)(elapsed*tick)/LATCH;

	return usec;
}

static void xscale_pmu_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	long cnt;
	unsigned long PMNC;

	/*
	 * The overflow must be cleared before the CCNT
	 * register is updated to prevent a race condition
	 * that can cause a lost timer tick.
	 */
	asm volatile ("mrc p14, 0, %0, c0, c0, 0" : "=r" (PMNC));
	/* Enable CCNT overflow interrupt in the PMNC */
	asm volatile ("mcr p14, 0, %0, c0, c0, 0" : : "r" (PMNC & 0x0ffff479));

	do {
		do_timer(regs);

		/* 
		 * Bring the count back for one timer tick.
		 * To be precise, we need to consider the cycles 
		 * involved in this read-modify-write sequence, as well as
		 * cache fill operations which can stall the instruction 
		 * pipeline.
		 */
		asm volatile ( "\
		mcr p15, 0, ip, c7, c10, 4 @ Flush the write (& read) buffers\n\
		b	1f\n\
		.align	5		@ Align code on a cache line boundary\n\
1:		mrc p14, 0, %0, c1, c0, 0\n\
		sub	%0, %0, %1\n\
		mcr p14, 0, %0, c1, c0, 0"
		: "=&r" (cnt) : "r" (LATCH-10));
	} while (cnt >= 0);
}

extern unsigned long (*gettimeoffset)(void);

static struct irqaction timer_irq = {
	name: "timer",
	handler: xscale_pmu_timer_interrupt,
	flags: SA_INTERRUPT
};

void __init setup_timer (void)
{
    unsigned long INTSTR;

	gettimeoffset = xscale_pmu_gettimeoffset;
	setup_arm_irq(XSCALE_PMU_IRQ, &timer_irq);

	printk("Using XScale PMU as timer source(IRQ %d, %d MHz Clock Tick)\n",
		XSCALE_PMU_IRQ, CLOCK_TICK_RATE / 1000000 );

	/*
	 * Need to do this b/c going to IDLE mode on XScale causes
	 * the PMU to idle also, which means we loose the timer tick!
	 */
	disable_hlt();


	if(machine_is_iq80310())
	{
		/* Steer PMU int to IRQ */
		asm volatile("mrc p13, 0, %0, c8, c0, 0" : "=r" (INTSTR));
		/* Clear write-as-zero bits 31:2 and PMU steering bit 0 */
		asm volatile ("mcr p13, 0, %0, c8, c0, 0" : : "r" (INTSTR & 2));
	}

	/* set the initial CCNT value */
	asm volatile ("mcr p14, 0, %0, c1, c0, 0" : : "r" (-LATCH));
	
	/* enable interrupt to occur on CCNT wraps in the PMNC */
	asm volatile ("mcr p14, 0, %0, c0, c0, 0" : : "r" (0x0ffff443));
}
