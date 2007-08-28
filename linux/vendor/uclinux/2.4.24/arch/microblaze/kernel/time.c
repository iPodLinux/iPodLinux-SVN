/*
 * linux/arch/v850/kernel/time.c -- Arch-dependent timer functions
 *
 *  Copyright (C) 1991, 1992, 1995, 2001, 2002  Linus Torvalds
 *
 * This file contains the v850-specific time handling details.
 * Most of the stuff is located in the machine specific files.
 *
 * 1997-09-10	Updated NTP code according to technical memorandum Jan '96
 *		"A Kernel Model for Precision Timekeeping" by Dave Mills
 */

#include <linux/config.h> /* CONFIG_HEARTBEAT */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>

#include <asm/io.h>

#include <linux/timex.h>

#include "mach.h"
#include <asm/microblaze_timer.h>

static inline void do_profile (unsigned long pc)
{
	if (prof_buffer && current->pid) {
		extern int _stext;
		pc -= (unsigned long) &_stext;
		pc >>= prof_shift;
		if (pc < prof_len)
			++prof_buffer[pc];
		else
		/*
		 * Don't ignore out-of-bounds PC values silently,
		 * put them into the last histogram slot, so if
		 * present, they will show up as a sharp peak.
		 */
			++prof_buffer[prof_len-1];
	}
}

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
static void timer_interrupt (int irq, void *dummy, struct pt_regs *regs)
{
	/* Read the timer control register */
	/* timer 0 is hard coded - bleugh */
	unsigned int csr = timer_get_csr(MICROBLAZE_TIMER_BASE_ADDR,0);

	/* may need to kick the hardware timer */
	if (mach_tick)
	  mach_tick ();

	do_timer (regs);

	if (! user_mode (regs))
		do_profile (regs->pc);

	{
		static int ticks=0, value=0, period=25;
		if(ticks++>=period)
		{
			value=!value;
			mach_heartbeat(value);
			ticks=0;
		}

		
	}

#if 0
	/* use power LED as a heartbeat instead -- much more useful
	   for debugging -- based on the version for PReP by Cort */
	/* acts like an actual heart beat -- ie thump-thump-pause... */
	if (mach_heartbeat) {
	    static unsigned cnt = 0, period = 0, dist = 0;

	    if (cnt == 0 || cnt == dist)
		mach_heartbeat ( 1 );
	    else if (cnt == 7 || cnt == dist+7)
		mach_heartbeat ( 0 );

	    if (++cnt > period) {
		cnt = 0;
		/* The hyperbolic function below modifies the heartbeat period
		 * length in dependency of the current (5min) load. It goes
		 * through the points f(0)=126, f(1)=86, f(5)=51,
		 * f(inf)->30. */
		period = ((672<<FSHIFT)/(5*avenrun[0]+(7<<FSHIFT))) + 30;
		dist = period / 4;
	    }
	}

#endif
	/* write the timer status back to the control register, 
	 * to clear the interrupt.  Once again, timer 0 is hard coded */
	timer_set_csr(MICROBLAZE_TIMER_BASE_ADDR, 0, csr);
	
}

extern rwlock_t xtime_lock;

/*
 * This version of gettimeofday has near microsecond resolution.
 */
void do_gettimeofday (struct timeval *tv)
{
#if 0 /* DAVIDM later if possible */
	extern volatile unsigned long lost_ticks;
	unsigned long lost;
#endif
	unsigned long flags;
	unsigned long usec, sec;

	read_lock_irqsave (&xtime_lock, flags);
#if 0
	usec = mach_gettimeoffset ? mach_gettimeoffset () : 0;
#else
	usec = 0;
#endif
#if 0 /* DAVIDM later if possible */
	lost = lost_ticks;
	if (lost)
		usec += lost * (1000000/HZ);
#endif
	sec = xtime.tv_sec;
	usec += xtime.tv_usec;
	read_unlock_irqrestore (&xtime_lock, flags);

	while (usec >= 1000000) {
		usec -= 1000000;
		sec++;
	}

	tv->tv_sec = sec;
	tv->tv_usec = usec;
}

void do_settimeofday (struct timeval *tv)
{
	write_lock_irq (&xtime_lock);
	/* This is revolting. We need to set the xtime.tv_usec
	 * correctly. However, the value in this location is
	 * is value at the last tick.
	 * Discover what correction gettimeofday
	 * would have done, and then undo it!
	 */
#if 0
	tv->tv_usec -= mach_gettimeoffset ();
#endif

	while (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime () */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq (&xtime_lock);
}

static int timer_dev_id;
static struct irqaction timer_irqaction = {
	timer_interrupt,
	SA_INTERRUPT,
	0,
	"timer",
	&timer_dev_id,
	NULL
};

void time_init (void)
{
	mach_gettimeofday (&xtime);
	mach_sched_init (&timer_irqaction);
}
