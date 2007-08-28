/*
 *  linux/arch/niosnommu/kernel/time.c
 *
 *  Copyright (C) 1998-2000  D. Jeff Dionne <jeff@uClinux.org>,
 *                           Kenneth Albanowski <kjahds@kjahds.com>,
 *
 *  Copied/hacked from:
 *
 *  linux/arch/m68knommu/kernel/time.c
 *
 *  Copied/hacked from:
 *
 *  linux/arch/m68k/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *
 * This file contains the m68k-specific time handling details.
 * Most of the stuff is located in the machine specific files.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/nios.h>

#include <linux/timex.h>


static inline int set_rtc_mmss(unsigned long nowtime)
{
  return -1;
}

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
void timer_interrupt(int irq, void *dummy, struct pt_regs * regs)
{
	/* last time the cmos clock got updated */
	static long last_rtc_update=0;
	

	do_timer(regs);

	/*
	 * If we have an externally synchronized Linux clock, then update
	 * CMOS clock accordingly every ~11 minutes. Set_rtc_mmss() has to be
	 * called as close as possible to 500 ms before the new second starts.
	 */
	if (time_state != TIME_BAD && xtime.tv_sec > last_rtc_update + 660 &&
	    xtime.tv_usec > 500000 - (tick >> 1) &&
	    xtime.tv_usec < 500000 + (tick >> 1)) {
	  if (set_rtc_mmss(xtime.tv_sec) == 0)
	    last_rtc_update = xtime.tv_sec;
	  else
	    last_rtc_update = xtime.tv_sec - 600; /* do it again in 60 s */
	}

}

void time_init(void)
{
	unsigned int year, mon, day, hour, min, sec;

	extern void arch_gettod(int *year, int *mon, int *day, int *hour,
				int *min, int *sec);

	arch_gettod(&year, &mon, &day, &hour, &min, &sec);

	if ((year += 1900) < 1970)
		year += 100;
	xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
	xtime.tv_usec = 0;

	request_irq(na_timer0_irq, timer_interrupt, IRQ_FLG_LOCK, "timer", NULL);

	na_timer0->np_timerperiodl = (nasys_clock_freq/HZ)-1;
	na_timer0->np_timerperiodh = ((nasys_clock_freq/HZ)-1) >> 16;

	/* interrupt enable + continuous + start */
	na_timer0->np_timercontrol = np_timercontrol_start_mask
										+ np_timercontrol_cont_mask
										+ np_timercontrol_ito_mask;
}

/*
 * This version of gettimeofday has near microsecond resolution.
 */
void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;
	
	save_flags(flags);
	cli();
	*tv = xtime;
	restore_flags(flags);
}

void do_settimeofday(struct timeval *tv)
{
	cli();
	/* This is revolting. We need to set the xtime.tv_usec
	 * correctly. However, the value in this location is
	 * is value at the last tick.
	 * Discover what correction gettimeofday
	 * would have done, and then undo it!
	 */

	if (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_state = TIME_BAD;
	time_maxerror = MAXPHASE;
	time_esterror = MAXPHASE;
	sti();
}

