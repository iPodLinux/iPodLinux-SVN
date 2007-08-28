/*
 *  linux/arch/e1nommu/kernel/time.c
 *
 *  Copyright (C) 2002-2003,    George Thanos <george.thanos@gdt.gr>
 *				Yannis Mitsos <yannis.mitsos@gdt.gr>
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 * 
 *  This file contains the e1-specific time handling details.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>

#include <asm/machdep.h>
#include <asm/io.h>

#include <linux/timex.h>


static inline int set_rtc_mmss(unsigned long nowtime)
{
  if (mach_set_clock_mmss)
    return mach_set_clock_mmss (nowtime);
  return -1;
}

static inline void do_profile (unsigned long pc)
{
#ifdef CONFIG_PROFILE /* FIXME */
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
#endif
}

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
#define DEBUG_TIMER 0 
#if DEBUG_TIMER
unsigned long in_timer_interrupt =0;
#endif
void timer_interrupt(int irq, void *dummy, struct pt_regs * regs)
{
	/* last time the cmos clock got updated */
	static long last_rtc_update=0;

#if DEBUG_TIMER
	in_timer_interrupt++;

	if( !(in_timer_interrupt % 500) ) {
		printk("timer_interrupt: %d\n", in_timer_interrupt);
#if 0
#define  L   regs->L
#define  FL  regs->FL
	{ int i;
	  printk("Interrupted Task:");
	  for( i=0 ; i<FL; i++) {
		  printk("L[%d] : 0x%08x,%s",i,L[i], (i%3)?" ":"\n");
	  }
	  printk("\nSaved PC: 0x%08x\n",L[FL] );
	  printk("Exception Entry: %d\n",L[FL+3] );
	}

#undef   FL
#undef   L
#endif
	}
#endif
	
	/* may need to kick the hardware timer */
	/* We have already called BSP tick
	if (mach_tick)
	  mach_tick();
	*/

	do_timer(regs);

#ifdef CONFIG_PROFILE /* FIXME */
	if (!user_mode(regs))
		do_profile(regs->pc);
#endif

	/*
	 * If we have an externally synchronized Linux clock, then update
	 * CMOS clock accordingly every ~11 minutes. Set_rtc_mmss() has to be
	 * called as close as possible to 500 ms before the new second starts.
	 */
	if ((time_status & STA_UNSYNC) == 0 &&
	    xtime.tv_sec > last_rtc_update + 660 &&
	    xtime.tv_usec >= 500000 - ((unsigned) tick) / 2 &&
	    xtime.tv_usec <= 500000 + ((unsigned) tick) / 2) {
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

	/* FIX by dqg : Set to zero for platforms that don't have tod */
	/* without this time is undefined and can overflow time_t, causing  */
	/* very stange errors */
	year = 1980;
	mon = day = 1;
	hour = min = sec = 0;
	arch_gettod (&year, &mon, &day, &hour, &min, &sec);

	if ((year += 1900) < 1970)
		year += 100;
	xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
	xtime.tv_usec = 0;

	mach_sched_init(timer_interrupt);
}

extern rwlock_t xtime_lock;

/*
 * This version of gettimeofday has near microsecond resolution.
 */
void do_gettimeofday(struct timeval *tv)
{
/* FIXME
	extern volatile unsigned long wall_jiffies;
*/
	unsigned long flags;
	unsigned long usec, sec, lost;

	read_lock_irqsave(&xtime_lock, flags);
	usec = mach_gettimeoffset ? mach_gettimeoffset() : 0;
/* FIXME
	lost = jiffies - wall_jiffies;
	if (lost)
		usec += lost * (1000000/HZ);
*/
	sec = xtime.tv_sec;
	usec += xtime.tv_usec;
	read_unlock_irqrestore(&xtime_lock, flags);

	while (usec >= 1000000) {
		usec -= 1000000;
		sec++;
	}

	tv->tv_sec = sec;
	tv->tv_usec = usec;
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq(&xtime_lock);
	/* This is revolting. We need to set the xtime.tv_usec
	 * correctly. However, the value in this location is
	 * is value at the last tick.
	 * Discover what correction gettimeofday
	 * would have done, and then undo it!
	 */
	if (mach_gettimeoffset)
		tv->tv_usec -= mach_gettimeoffset();

	while (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq(&xtime_lock);
}
