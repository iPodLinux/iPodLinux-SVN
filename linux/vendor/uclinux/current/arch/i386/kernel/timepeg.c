/*
 * Implementation of timepeg execution instrumentation.  See:
 *
 * Documentation/timepeg.txt
 * http://www.uow.edu.au/~andrewm/linux/
 *
 * Andrew Morton <andrewm@uow.edu.au>
 *
 * Some code and ideas taken from the net profiling code
 * which was contributed by <insert name here>
 */

#include <linux/module.h>
#include <linux/config.h>

#ifdef CONFIG_TIMEPEG
#include <asm/timepeg.h>

#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/string.h>

#include <linux/init.h>
#include <linux/smp.h>
#include <asm/system.h>
#include <asm/delay.h>

#ifndef CONFIG_PROC_FS
#error Timepegs require the /proc filesystem. Please enable CONFIG_PROC_FS.
#endif

#ifndef CONFIG_X86_TSC
#include <asm/io.h>
#include <linux/timer.h>
#endif

#define DEBUGGING 0

/************************
 * Storage declarations *
 ************************/

#ifndef CONFIG_X86_TSC
volatile unsigned char	*timepeg_mmcrp;	/* mem mapped configuration registers */
#endif

/* Global singly linked list of timepegs */
static struct timepeg_slot *g_timepeg_slots;

/* Lock for the above */
static spinlock_t g_timepeg_slots_l = SPIN_LOCK_UNLOCKED;

/*
 * Per-CPU array of most-previously-visited timepegs
 */
static struct timepeg_slot *g_prev_slots[NR_CPUS];

/*
 * Flag: timepeg system is usable
 */
int timepegs_are_running;

/*
 * Per-CPU array for recursion protection
 */
int timepeg_no_recur[NR_CPUS];

/*
 * Estimate of the time overhead which timepegs cause
 */
timepeg_t timepeg_calib;

/*
 * Records when each CPU entered timepeg_hit().  This used to be
 * a local in timepeg_hit(), but that screws up across calls
 * to schedule().
 */
timepeg_t entered_tp[NR_CPUS];

/*
 * Global /proc locking strategy:
 *
 * When a process is reading the /proc file we want all the users of the
 * timepeg_slot list to simply skip over their instrumentation (leave it alone).
 * But timepeg_read_proc() cannot proceed with using the data structure until
 * all other users have finished.
 *
 * So:
 *
 *
 *	timepeg_read_proc()
 *	{
 *		if (start_of_read())
 *		{
 *			reader_wants_it = 1;
 *			while (usage_count > 0)
 *				;
 *			<I have it>
 *		}
 *		use the data
 *		if (end_of_read)
 *		{
 *			reader_wants_it = 0;
 *		}
 *	}
 *
 *	timepeg_hit()
 *	{
 *		if (reader_wants_it == 0)
 *		{
 *			inc(usage_count);
 *			if (reader_wants_it == 0)
 *				do stuff
 *			dec(usage_count)
 *		}
 *	}
 *
 * There's locking on reader_wants_it
 *
 * There may be a more clever way of doing it...
 */

/* 
 * A (locked) flag which we use to prevent all the data structures
 * from being touched by timepeg_hit() while timepeg_read_proc() is
 * being used (someone is reading the /proc file).
 */

static struct
{
	spinlock_t lock;
	int flag;
	atomic_t usage_count;
} reader_wants_it = {
	SPIN_LOCK_UNLOCKED,
	0,
	ATOMIC_INIT(0),
};

static void run_test_cases(void);

/*
 * Construct a timepeg.
 *
 * Nothing much to do at this time - just link it in to the
 * global list.
 *
 * Local interrupts are disabled and *_ntps is locked
 */

static void
timepeg_construct(struct timepeg_slot * const _ntps)
{
	int i;

	/* We need to lock down the list head.  (pedant) */
	spin_lock(&g_timepeg_slots_l);

	/* All this simply initialises the per-cpu per-arc min_tp/max_tp value */
	for (i = 0; i < NR_CPUS; i++)
	{
		int j;
		struct timepeg_slot_percpu *ntpsp = &(_ntps->percpu[i]);

		for (	j = 0;
			j < sizeof(ntpsp->preds) / sizeof(ntpsp->preds[0]);
			j++)
		{
			struct timepeg_arc *ntpa = &(ntpsp->preds[j]);
			ntpa->max_tp = 0;
			ntpa->min_tp = ~0ULL;
		}
	}

	/*
	 * Slash through the current timepegs to see if bozo-the-coder
	 * has given two timepegs the same name
	 */
	if (_ntps->line == 0)	/* file-n-line timepegs can be duplicated */
	{
		struct timepeg_slot *tps;

		for (tps = g_timepeg_slots; tps; tps = tps->next)
		{
			if ((tps->line == _ntps->line) && !strcmp(tps->name, _ntps->name))
			{
				printk(	"timepeg: detected duplicate timepeg '%s:%d'\n",
					_ntps->name, _ntps->line);
				_ntps->mode_bits |= TIMEPEG_MODE_DEAD;
				break;
			}
		}
	}

	/*
	 * If this is a directed timepeg, it has a non-zero directee_name.
	 * We do a linear search of the timepeg list looking for the
	 * directed predecessor.  If it's not there we complain
	 * and mark the timepeg as dead. We also do not add it to the list
	 * of timpegs.
	 * The linear search is OK - once the list is built it remains
	 * in place until a reboot.  The first timepeg run is a throw-away
	 * anyway.
	 * We ignore the timepeg_slot.line here - assume that directed timepegs
	 * do not point at file-and-line timepegs.
	 */
	if (!(_ntps->mode_bits & TIMEPEG_MODE_DEAD) && _ntps->directee_name)
	{
		struct timepeg_slot *tps = g_timepeg_slots;
		while (tps)
		{
			if (!strcmp(tps->name, _ntps->directee_name))
				break;
			tps = tps->next;
		}

		if (tps == 0)
		{
			printk(	"timepeg: timepeg '%s' is directed to timepeg '%s', "
				"which couldn't be located\n",
				_ntps->name,
				_ntps->directee_name);
			_ntps->mode_bits |= TIMEPEG_MODE_DEAD;
		}
		else
		{
			_ntps->directed_pred = tps;
		}
	}

	if (!(_ntps->mode_bits & TIMEPEG_MODE_DEAD))
	{
		_ntps->next = g_timepeg_slots;
		g_timepeg_slots = _ntps;
	}

	spin_unlock(&g_timepeg_slots_l);
}

/*
 * Somebody has executed TIMEPEG().
 * Called from timepeg_hit().
 *
 * Local interrupts are disabled
 */

timepeg_t *
timepeg_do_hit(	struct timepeg_slot * const _ntps,
		timepeg_t * const _now_tp,
		const int _mode)
{
	struct timepeg_slot * prev_tp;		/* Most-recently-visited */
	const int cpu = smp_processor_id();	/* I _so_ prefer C++ */
	struct timepeg_slot_percpu *cpu_slot;
	timepeg_t *retval = (timepeg_t *)0;

	spin_lock(&reader_wants_it.lock);
	if (reader_wants_it.flag != 0)
	{	/* It's being read.  Skip it */
		spin_unlock(&reader_wants_it.lock);
		goto bye;
	}
	atomic_inc(&reader_wants_it.usage_count);
	spin_unlock(&reader_wants_it.lock);

	/*
	 * We need to lock this timepeg from other CPUs.  We know that
	 * local interrupts are disabled so all we need do is spin on the lock.
	 */
	spin_lock(&_ntps->lock);

	if ((_ntps->mode_bits & TIMEPEG_MODE_INITIALISED) == 0)
	{	/* First time this timepeg has been used */
		timepeg_construct(_ntps);
		if (_ntps->mode_bits & TIMEPEG_MODE_INITIALISED)
			printk("HOLY COW!  REENTERED\n");
		_ntps->mode_bits |= TIMEPEG_MODE_INITIALISED;
	}

	if (_ntps->mode_bits & TIMEPEG_MODE_DEAD)
	{	/*
		 * oops.  This is a directed timepeg and we were
		 * unable to locate its directed predecessor, or it
		 * was previously determined to be a dupe.
		 * Give up.
		 */
		spin_unlock(&_ntps->lock);

		atomic_dec(&reader_wants_it.usage_count);
		goto bye;
	}

	/* Locate the per-cpu info in this timepeg */
	cpu_slot = &(_ntps->percpu[cpu]);

	/*
	 * Find the other end of the arc.  If it's a directed timepeg
	 * then we know.  Otherwise find the most-recently-hit timepeg
	 * for this CPU
	 */
	if (_ntps->directed_pred)
	{	/* Directed */
		prev_tp = _ntps->directed_pred;
	}
	else
	{	/* Most-recently-seen for this CPU */
		prev_tp = g_prev_slots[cpu];
	}

	if (prev_tp == 0)
	{	/* Waah.  This is the first time this CPU has
		 * hit a timepeg.
		 */
		/* NOTHING */
	}
	else if (_mode & TPH_MODE_STOP)
	{
		/*
		 * OK, time to actually do something.
		 * We update this timepeg's info for this CPU.
		 */
		struct timepeg_arc *ntpa = 0;
		timepeg_t elapsed_tp;
		int i;

		/*
		 * First, see if we already have some stats for this arc.
		 * Linear search...
		 */
		ntpa = &cpu_slot->preds[0];
		for (i = 0; i < TIMEPEG_NR_PREDS; i++, ntpa++)
		{
			if (ntpa->timepeg == 0)
			{
				/*
				 * End of the list.  This must be
				 * the first time we've seen this arc
				 */
				ntpa->timepeg = prev_tp;
				break;
			}
			if (ntpa->timepeg == prev_tp)
			{	/* We've used this arc before */
				break;
			}
		}

		if (i == TIMEPEG_NR_PREDS)
		{
			/* oops.  We've run out of space in the preds table */
			if ((_ntps->mode_bits & TIMEPEG_MODE_FULL) == 0)
			{
				int j;
				struct timepeg_arc *ntpa2;

				printk(	"timepeg_do_hit(): out of predecessor slots for %s:%d\n",
					_ntps->name, _ntps->line);
				_ntps->mode_bits |= TIMEPEG_MODE_FULL;

				ntpa2 = &cpu_slot->preds[0];
				printk("Contents:\n");
				for (j = 0; j < TIMEPEG_NR_PREDS; j++, ntpa2++)
				{
					printk(	"  %s:%d\n",
						ntpa2->timepeg->name,
						ntpa2->timepeg->line);
				}
			}
			ntpa = 0;
		}

		/* Accumulate arc time */
		elapsed_tp = *_now_tp - prev_tp->percpu[cpu].departure_tp - timepeg_calib;
#ifndef CONFIG_X86_TSC
		/* Due to the SC520 timer being two parts that cascade together,
		 * it is possible for a negative delta to appear this is caused
		 * by the millisecond counter being updated by the microsecond counter
		 * at about the time we read it.  The problem is that the cascade
		 * doesn't occur naturally between microseconds of 999 and 000, it just
		 * happens around then.  Luckily, the error is always 1 microsecond so
		 * it is correctable once detected.
		 */
		if (*_now_tp < prev_tp->percpu[cpu].departure_tp)
			elapsed_tp += 1000;
#endif

		if (ntpa)
		{
			ntpa->acc_tp += elapsed_tp;

			/* Bump arc usage count */
			ntpa->nr_times++;

			/* Calculate fastest time */
			if (elapsed_tp < ntpa->min_tp)
			{	/* elapsed < min_tp */
				ntpa->min_tp = elapsed_tp;
			}

			/* Calculate slowest time */
			if (elapsed_tp > ntpa->max_tp)
			{	/* elapsed > max_tp */
				ntpa->max_tp = elapsed_tp;
			}
		}
	}

	if (_mode & TPH_MODE_START)
	{
		/*
		 * Note the fact that this CPU's next arc starts on this timepeg
		 */
		g_prev_slots[cpu] = _ntps;

		/*
		 * The outgoing inline code writes the current time into this CPU's
		 * slot in this timepeg
		 */
		retval = &cpu_slot->departure_tp;
	}

	spin_unlock(&_ntps->lock);

	atomic_dec(&reader_wants_it.usage_count);
bye:
	return retval;
}

/*
 * Reinitialise all accumulation data
 * Is this racy?
 */

static void
zap_all_timepegs(void)
{
	struct timepeg_slot *slotp = g_timepeg_slots;
	int i;

	for ( ; slotp; slotp = slotp->next)
	{
		int cpu;

		for (cpu = 0; cpu < NR_CPUS; cpu++)
		{
			int pred;
			struct timepeg_slot_percpu *pcpu;

			pcpu = &(slotp->percpu[cpu]);

			/* Leave departure_tp alone */

			for (pred = 0; pred < TIMEPEG_NR_PREDS; pred++)
			{
				struct timepeg_arc *arc = &(pcpu->preds[pred]);
				arc->timepeg = 0;
				arc->acc_tp = 0;
				arc->min_tp = ~0ULL;
				arc->max_tp = 0;
				arc->nr_times = 0;
			}
		}
	}

	for (i = 0; i < sizeof(g_prev_slots)/sizeof(g_prev_slots[0]); i++)
		g_prev_slots[i] = 0;
}

/*
 * Dump to userland.  Return the number of chars we've generated.
 * Gack.  What a crufty interface.
 *
 *
 * Output format:
 *
 *	n_cpus						Integer
 *	for_all_timepegs
 *	{
 *		timepeg_name
 *		for_all_cpus
 *		{
 *			"cpu%d"				Literal
 *			number_of_preds			Integer
 *			for_all_preds
 *			{
 *				pred_name nt_times acc_tp min_tp max_tp
 *			}
 *		}
 *	}
 *
 */

/*
 * Calculate the time taken for timepeg accounting.  This will be
 * subtracted from all future accounting.  It's a bast-case.  Caches
 * are hot.
 */

static void calibrate_timepegs(void)
{
	timepeg_t in, out;
	int i;
	unsigned long flags;
return;
	local_irq_save(flags);
	for (i = 0; i < 2; i++)
	{
		timepeg_stamp(&in);
		DTIMEPEG("__dummy__", "__dummy__");
		timepeg_stamp(&out);
	}
	timepeg_calib = out - in;
	local_irq_restore(flags);
}

/*
 * The read function gets called once, even after it has returned *eof=1.  That
 * screws up royally.  Kludge it here.
 */

static int g_read_completed = 0;

static int
timepeg_read_proc(
	char *page_buffer,
	char **my_first_byte,
	off_t virtual_start,
	int length,
	int *eof,
	void *data)
{
	int my_buffer_offset = 0, my_virtual_offset = 0;
	char * const my_base = page_buffer;
	struct timepeg_slot *ntps;
	int cpu, retval = 0;

	/*
	 * Turn the timepeg system on now.
	 */
	if (timepegs_are_running == 0) {
		timepegs_are_running = 1;
		calibrate_timepegs();
		return 0;
	}

	if (g_timepeg_slots == 0)
	{
		run_test_cases();
		return 0;
	}

	if (virtual_start == 0)
	{	/* Just been opened */
		reader_wants_it.flag = 1;
		while (atomic_read(&reader_wants_it.usage_count) != 0)
			;
		g_read_completed = 0;
	}
	else if (g_read_completed)
	{
		return 0;
	}

	my_buffer_offset += sprintf(my_base + my_buffer_offset, "%d\n", NR_CPUS);

	for (ntps = g_timepeg_slots; ntps; ntps = ntps->next)
	{
		my_buffer_offset += sprintf(	my_base + my_buffer_offset,
						" \"%s:%d\"\n",
						ntps->name,
						ntps->line);

		for (cpu = 0; cpu < NR_CPUS; cpu++)
		{
			struct timepeg_slot_percpu *per_cpu;
			int j, n = 0;

			per_cpu = &(ntps->percpu[cpu]);
			my_buffer_offset += sprintf(my_base + my_buffer_offset, "  cpu%04d\n", cpu);
			for (j = 0; j < TIMEPEG_NR_PREDS; j++)
			{
				struct timepeg_arc *ntpa;
				ntpa = &(per_cpu->preds[j]);
				if (ntpa->timepeg)
					n++;
				else
					break;
			}
			my_buffer_offset += sprintf(my_base + my_buffer_offset, "  %d\n", n);
				
			for (j = 0; j < TIMEPEG_NR_PREDS; j++)
			{
				struct timepeg_arc *ntpa;
				ntpa = &(per_cpu->preds[j]);
				if (ntpa->timepeg)
				{
					my_buffer_offset += sprintf(
						my_base + my_buffer_offset,
						"   \"%s:%d\" %lu %lu:%lu %lu:%lu %lu:%lu\n",
						ntpa->timepeg->name,
						ntpa->timepeg->line,
						ntpa->nr_times,
						(unsigned long)(ntpa->acc_tp >> 32),
						(unsigned long)ntpa->acc_tp,
						(unsigned long)(ntpa->min_tp >> 32),
						(unsigned long)ntpa->min_tp,
						(unsigned long)(ntpa->max_tp >> 32),
						(unsigned long)ntpa->max_tp);
				}
			}
		}

		my_virtual_offset += my_buffer_offset;
		if (my_virtual_offset <= virtual_start)
		{
			my_buffer_offset = 0;	/* Keep writing until we get into caller's window */
		}
		else
		{	/* We provided some stuff */
			retval = my_virtual_offset - virtual_start;
			*my_first_byte = page_buffer + (my_buffer_offset - retval);
			goto done;
		}
	}

	/*
	 * We've finished the read
	 */
	*eof = 1;
	*my_first_byte = page_buffer;
	g_read_completed = 1;
	zap_all_timepegs();
	reader_wants_it.flag = 0;

	/*
	 * OK, the read has finished and everything is unlocked.
	 * We take this as our cue to run the test cases.
	 */
	run_test_cases();

done:
	if (my_buffer_offset > PAGE_SIZE)
	{
		printk("timepeg: Holy cow! I've just trashed your machine.\n");
	}
	return retval;
}

#ifndef CONFIG_X86_TSC
void
timepeg_stamp(timepeg_t *pstamp)
{
	unsigned long mm;
	int milli, micro, m;
	static int old_milli;
	static unsigned long secs;

	/* Load micro and milli second counts together */
	mm = *(volatile unsigned long *)(timepeg_mmcrp + 0xc60);
	micro = mm >> 16;
	milli = mm & 0xffff;
	
	/* The milli second component is zeroed on read so we've
	 * got some correcting to do.
	 */
	m = milli + old_milli;
	while (m > 1000) {
		m -= 1000;
		secs++;
	}
	old_milli = m;

	
	/* Now bash everything together and return it.
	 * This could be coded more efficiently using some clever
	 * type casts.
	 */
	if (pstamp)
		*pstamp = (((timepeg_t)secs) * 1000 + m) * 1000 + micro;
}

/* We must ensure that the timer_stamp code is polled irregularily.
 * Strictly, we must poll the timer ever 65.5 seconds or it will overflow
 * the milli second component.  We'll poll every 20 seconds to be very
 * conservative.
 */
static void timepeg_timer(unsigned long arg) {
	static struct timer_list timer;
	
	timepeg_stamp(NULL);

	timer.expires = jiffies + HZ * 20;
	timer.function = timepeg_timer;
	timer.data = 0;
	add_timer(&timer);
}

#endif

int __init
timepeg_init(void)
{
	printk("Kernel timepegs enabled. See http://www.uow.edu.au/~andrewm/linux/\n");
#ifdef CONFIG_PROC_FS
	create_proc_read_entry("timepeg", 0, 0, timepeg_read_proc, 0);
#endif
#ifndef CONFIG_X86_TSC
	/* We're doing this for an AMD SC520 at the moment, map the config registers */
	timepeg_mmcrp = (volatile unsigned char *) ioremap(0xfffef000, 4096);
	
	/* Use a 33.000 MHz crystal */
	*(volatile unsigned char *)(timepeg_mmcrp + 0xc64) = 0x01;
	
	/* Begin our ping timer */
	timepeg_timer(0);
#endif
	return 0;
}

__initcall(timepeg_init);

EXPORT_SYMBOL(timepeg_do_hit);
EXPORT_SYMBOL(timepeg_no_recur);
EXPORT_SYMBOL(timepegs_are_running);
EXPORT_SYMBOL(entered_tp);

#if DEBUGGING

static void
tp_delay(const int n)
{
	udelay(n);
}

/*
 * Basic regression test cases
 *
 * Note that the timing measurements here are only meaningful on the
 * second run through.  This is because on the first pass more
 * work has to be done to initialise the timepegs.  This
 * includes work generated by the compiler (static variables with
 * function scope) plus all the fluff in timepeg_construct
 */

static void
test1(void)
{
	/*
	 * Try a directed timepeg, referring to a timepeg which
	 * doesn't exist yet.  We should get a printk message
	 * and the 'misdirected' timepeg is forgotten about.
	 */
	DTIMEPEG("misdirected", "forward");
	tp_delay(20);

	/*
	 * Try a non-existent timepeg.  Same result as above
	 */
	TIMEPEG("dangling_reference");
	tp_delay(30);

	/*
	 * Satisfy the forward reference
	 */
	TIMEPEG("forward");
	tp_delay(40);

	/*
	 * Duplicate timepeg.  The second should be complained about
	 * and ignored
	 */
	TIMEPEG("duplicate");
	TIMEPEG("duplicate");
	DTIMEPEG("duplicate", "dangling_reference");
	tp_delay(50);

	/*
	 * I wonder what this does?
	 */
	TIMEPEG("circular");
	DTIMEPEG("circular", "circular");

	/*
	 * And this?
	 */
	DTIMEPEG("self", "self");

	/*
	 * Measure the inter-timepeg delay.  This
	 * _should_ be zero, but there are a few
	 * instructions which are not accounted for.
	 */
	TIMEPEG("fast_chain_1");
	TIMEPEG("fast_chain_2");
	TIMEPEG("fast_chain_3");
	tp_delay(60);

	/*
	 * Now use a directed timepeg to work out how long a timepeg
	 * _really_ takes.  This works because directed timepegs
	 * do not take into account the time spent in intervening
	 * timepegs.
	 */
	TIMEPEG("measure_1");
	TIMEPEG("dummy1");
	TIMEPEG("dummy2");
	TIMEPEG("dummy3");
	DTIMEPEG("measure_2", "measure_1");
	tp_delay(70);

	/*
	 * Do the above again, but with one less timepeg.  The difference
	 * is the timepeg overhead (modulo cache effects).
	 */
	TIMEPEG("measure_3");
	TIMEPEG("dummy4");
	TIMEPEG("dummy5");
	DTIMEPEG("measure_4", "measure_3");
	tp_delay(80);
}


/*
 * Test complex call graphs
 */

static void
test2_final(void)
{
	TIMEPEG("test2_final");
	tp_delay(5);
}

static void
test2_0(void)
{
	TIMEPEG("test2_0");
	test2_final();
}

static void
test2_1(void)
{
	TIMEPEG("test2_1");
	test2_final();
}

static void
test2_2(void)
{
	TIMEPEG("test2_2");
	test2_final();
}

static void
test2_3(void)
{
	TIMEPEG("test2_3");
	test2_final();
}

static void
test2(void)
{
	int i;

	TIMEPEG("complex_start");

	for (i = 0; i < 20; i++)
	{
		tp_delay(5);
		switch (i % 4)
		{
		case 0:
			test2_0();
			break;
		case 1:
			test2_1();
			break;
		case 2:
			test2_2();
			break;
		case 3:
			test2_3();
			break;
		}
	}
	TIMEPEG("complex_end");
	DTIMEPEG("dcomplex_end", "complex_start");
}

/*
 * Overflow a timpeg's predecessor table
 */

static void test3(void)
{
	int i;
	for (i = 0; i < 17; i++)
	{
		switch (i)
		{
		case  0:	TIMEPEG_MODE("test3 00", TPH_MODE_START); break;
		case  1:	TIMEPEG_MODE("test3 01", TPH_MODE_START); break;
		case  2:	TIMEPEG_MODE("test3 02", TPH_MODE_START); break;
		case  3:	TIMEPEG_MODE("test3 03", TPH_MODE_START); break;
		case  4:	TIMEPEG_MODE("test3 04", TPH_MODE_START); break;
		case  5:	TIMEPEG_MODE("test3 05", TPH_MODE_START); break;
		case  6:	TIMEPEG_MODE("test3 06", TPH_MODE_START); break;
		case  7:	TIMEPEG_MODE("test3 07", TPH_MODE_START); break;
		case  8:	TIMEPEG_MODE("test3 08", TPH_MODE_START); break;
		case  9:	TIMEPEG_MODE("test3 09", TPH_MODE_START); break;
		case 10:	TIMEPEG_MODE("test3 10", TPH_MODE_START); break;
		case 11:	TIMEPEG_MODE("test3 11", TPH_MODE_START); break;
		case 12:	TIMEPEG_MODE("test3 12", TPH_MODE_START); break;
		case 13:	TIMEPEG_MODE("test3 13", TPH_MODE_START); break;
		case 14:	TIMEPEG_MODE("test3 14", TPH_MODE_START); break;
		case 15:	TIMEPEG_MODE("test3 15", TPH_MODE_START); break;
		case 16:	TIMEPEG_MODE("test3 16", TPH_MODE_START); break;
		}
		tp_delay(10);
		TIMEPEG_MODE("test3 sucker", TPH_MODE_STOP);
	}
}

/*
 * Wrap all the other tests
 */

static void
run_test_cases(void)
{
	printk("timepeg: running test cases\n");
	TIMEPEG("run_test_cases_start");
	tp_delay(10);
	test1();
	TIMEPEG("run_test_cases_end");
	DTIMEPEG("run_test_cases_end_d", "run_test_cases_start");
	test2();
	test3();
}

#else	/* DEBUGGING*/

static void
run_test_cases(void)
{}

#endif

#endif	/* CONFIG_TIMEPEG */

