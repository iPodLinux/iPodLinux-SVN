/*
 * Header for timepeg instrumentation.  See:
 *
 * Documentation/timepeg.txt
 * http://www.uow.edu.au/~andrewm/linux/
 *
 * Andrew Morton <andrewm@uow.edu.au>
 *
 * Some code and ideas taken from the net profiling code
 * which was contributed by <insert name here>
 */

#ifndef _TIMEPEG_H_
#define _TIMEPEG_H_ 1

#include <linux/config.h> 		/* for CONFIG_TIMEPEG */

#ifdef CONFIG_TIMEPEG

#include <linux/types.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <linux/module.h>		/* for EXPORT_SYMBOL */
#include <linux/cache.h>		/* for __cacheline_aligned */
#include <asm/current.h>		/* smp_processor_id() needs this */
#include <linux/sched.h>		/* for struct task_struct */
#include <linux/smp.h>			/* for smp_proessor_id() */
#include <asm/atomic.h>
#ifdef CONFIG_X86_TSC
#include <asm/msr.h>			/* for rdtsc() */
#endif

#include <asm/timepeg_slot.h>

#ifdef CONFIG_X86_TSC

extern __inline__ void
timepeg_stamp(timepeg_t *pstamp)
{
	rdtscll(*pstamp);
}

#else

/* This one is a bit ugly to include inline */
extern void timepeg_stamp(timepeg_t *pstamp);

/*
 * The profile stuff uses gettimeofday() here.
 * Skip it for now.
 */

//#error Timepegs not supported on this architecture.  Did you configure for a Pentium?

#endif	/* architecture */

#endif	/* CONFIG_TIMEPEG */


#ifdef CONFIG_TIMEPEG

/*
 * This does the real work for timepeg_hit.  It returns a pointer to
 * the most-recently-departed timepeg_slot_percpu's departure_tp
 * so that we can update it within the inline code, thus providing
 * minimal drift.
 *
 * Performs once-off on-demand initialisation of the timepeg.
 */

extern timepeg_t *
timepeg_do_hit(	struct timepeg_slot * const _ntps,
		timepeg_t * const _now_tp,
		const int _mode);
/*
 * The main worker function:  execution enters a new timepeg.
 *
 * 'mode' is a combination of:
 *
 * TPH_MODE_START: this is a 'start' timepeg - mark it in g_prev_slots
 * TPH_MODE_STOP:  this is a 'stop' timepeh - accumulate predecessor arc.
 */

#define TPH_MODE_START	1
#define TPH_MODE_STOP	2
#define TPH_MODE_BOTH	(TPH_MODE_START | TPH_MODE_STOP)

extern int timepeg_no_recur[NR_CPUS];
extern int timepegs_are_running;
extern timepeg_t entered_tp[NR_CPUS];

extern __inline__ void
timepeg_hit(struct timepeg_slot * const _ntps, const int _mode)
{
	if (timepegs_are_running) {
		const int cpu = smp_processor_id();

		if (timepeg_no_recur[cpu] == 0)
		{
			unsigned long flags;
			timepeg_t *prev_tp;

			timepeg_no_recur[cpu] = 1;

			local_irq_save(flags);

			timepeg_stamp(&entered_tp[cpu]);

			{
				/*
				 * Within this block timepeg accounting is 'suspended'.  We
				 * can spend as long as we like in here and it doesn't
				 * affect the instrumentation
				 */
				prev_tp = timepeg_do_hit(_ntps, &entered_tp[cpu], _mode);
			}

			if (prev_tp)
				timepeg_stamp(prev_tp);

			local_irq_restore(flags);
			timepeg_no_recur[cpu] = 0;
		}
	}
}

/*
 * timepeg interface macros.  NB: None of macros have
 * semicolons.
 */
#define TIMEPEG_DEFINE(_id_, _M_name)				\
		static struct timepeg_slot _id_ =		\
		{						\
			name: _M_name,				\
			lock: SPIN_LOCK_UNLOCKED,		\
		}

/*
 * Define and hit a timepeg
 */
#define TIMEPEG_MODE(_M_name, _M_mode)				\
	do {							\
		static struct timepeg_slot _x_Timepeg =		\
		{						\
			name: _M_name,				\
			lock: SPIN_LOCK_UNLOCKED,		\
		};						\
		(void)timepeg_hit(&_x_Timepeg, _M_mode);	\
	} while (0)

/*
 * With file-and-line
 */

#define TIMEPEG_MODE_FL(_M_mode)				\
	do {							\
		static struct timepeg_slot _x_Timepeg =		\
		{						\
			name: __FILE__,				\
			line: __LINE__,				\
			lock: SPIN_LOCK_UNLOCKED,		\
		};						\
		(void)timepeg_hit(&_x_Timepeg, _M_mode);	\
	} while (0)

#define TIMEPEG(_M_name)					\
	TIMEPEG_MODE(_M_name, TPH_MODE_BOTH)

#define TIMEPEG_FL() TIMEPEG_MODE_FL(TPH_MODE_BOTH)

/*
 * Define and hit a directed timepeg
 */
#define DTIMEPEG(_M_name, _M_directed_pred)			\
	do {							\
		static struct timepeg_slot _x_Timepeg =		\
		{						\
			name: _M_name,				\
			lock: SPIN_LOCK_UNLOCKED,		\
			directee_name: _M_directed_pred,	\
		};						\
		(void)timepeg_hit(&_x_Timepeg, TPH_MODE_BOTH);	\
	} while (0)

#else	/* CONFIG_TIMEPEG */

#define TIMEPEG_DEFINE(_id_, _M_name)

#define TIMEPEG_MODE(_M_name, _M_mode)				\
	do {							\
	} while (0)

#define TIMEPEG(_M_name)					\
	do { } while (0)

#define DTIMEPEG(_M_name, _M_directed_pred)			\
	do { } while (0)

#endif	/* CONFIG_TIMEPEG */

#endif	/* _TIMEPEG_H_ */

