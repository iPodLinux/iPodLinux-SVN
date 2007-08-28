/*
 * Interrupt latency instrumentation
 *
 * Andrew Morton <andrewm@uow.edu.au>
 * http://www.uow.edu.au/~andrewm/linux/
 */

#include <linux/config.h>

#ifdef CONFIG_INTLAT

#ifndef CONFIG_TIMEPEG
#error INTLAT requires CONFIG_TIMEPEG
#endif

#include <linux/module.h>
#include <asm/system.h>
#include <asm/timepeg.h>

#define read_flags(x)	\
	__asm__ __volatile__("pushfl ; popl %0":"=g" (x): /* no input */ :"memory")

#define INT_ON			0x0200
#define INT_OFF			0x0000
#define INTS_ON(flags)		((flags) & INT_ON)
#define INTS_OFF(flags)		(!((flags) & INT_ON))

/*
 * We're about to set the interrupt flags.
 * If we're going from enabled -> disabled, do a TPH_MODE_START.
 * If we're going from disabled -> enabled, do a TPH_MODE_STOP.
 */
static void
about_to_set(struct timepeg_slot *_tps, unsigned long _flags_now, unsigned long _new_flags)
{
	if (INTS_ON(_flags_now) && !INTS_ON(_new_flags))
	{	/* enabled -> disabled */
		timepeg_hit(_tps, TPH_MODE_START);
	}
	else if (!INTS_ON(_flags_now) && INTS_ON(_new_flags))
	{	/* disabled -> enabled */
		timepeg_hit(_tps, TPH_MODE_STOP);
	}
}

/*
 * Entering an ISR, we assume that interrupts were on,  so
 * make it look like they're now turned off.
 */

void
intlat_enter_isr(struct timepeg_slot *_tps)
{
	timepeg_hit(_tps, TPH_MODE_START);
}

/*
 * Exitting from the ISR.  If interrupts have been reenabled within the
 * ISR we do nothing.  If they haven't, make it look like the ISR exit
 * is an enabling of interrupts.
 */
void
intlat_exit_isr(struct timepeg_slot *_tps)
{
	unsigned long flags_now;

	read_flags(flags_now);

	if (INTS_OFF(flags_now))
	{
		timepeg_hit(_tps, TPH_MODE_STOP);
	}
}

/*
 * We must always call about_to_set with interrupts off; otherwise there
 * are nasty windows
 */
void
intlat_restore_flags(struct timepeg_slot *_tps, unsigned long _flags)
{
	unsigned long flags_now;

	read_flags(flags_now);

	if (INTS_ON(_flags))
	{	/* Enabling */
		about_to_set(_tps, flags_now, _flags);
		__asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"g" (_flags):"memory");
	}
	else
	{	/* Disabling */
		__asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"g" (_flags):"memory");
		about_to_set(_tps, flags_now, _flags);
	}
}

void
intlat_cli(struct timepeg_slot *_tps)
{
	unsigned long flags_now;

	read_flags(flags_now);

	__asm__ __volatile__("cli": : :"memory");
	about_to_set(_tps, flags_now, INT_OFF);
}

void
intlat_sti(struct timepeg_slot *_tps)
{
	unsigned long flags_now;

	read_flags(flags_now);

	about_to_set(_tps, flags_now, INT_ON);
	__asm__ __volatile__("sti": : :"memory");
}

unsigned long
intlat_local_irq_save(struct timepeg_slot *_tps)
{
	unsigned long x;
	unsigned long flags_now;

	read_flags(flags_now);

	__asm__ __volatile__("pushfl ; popl %0 ; cli":"=g" (x): /* no input */ :"memory");
	about_to_set(_tps, flags_now, INT_OFF);
	return x;
}

void
intlat_local_irq_restore(struct timepeg_slot *_tps, unsigned long _flags)
{
	unsigned long flags_now;

	read_flags(flags_now);

	if (INTS_ON(_flags))
	{	/* Enabling */
		about_to_set(_tps, flags_now, _flags);
		__asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"g" (_flags):"memory");
	}
	else
	{	/* Disabling */
		__asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"g" (_flags):"memory");
		about_to_set(_tps, flags_now, _flags);
	}
}

void
intlat_local_irq_disable(struct timepeg_slot *_tps)
{
	unsigned long flags_now;

	read_flags(flags_now);

	__asm__ __volatile__("cli": : :"memory");
	about_to_set(_tps, flags_now, INT_OFF);
}

void
intlat_local_irq_enable(struct timepeg_slot *_tps)
{
	unsigned long flags_now;

	read_flags(flags_now);

	about_to_set(_tps, flags_now, INT_ON);
	__asm__ __volatile__("sti": : :"memory");
}

#ifdef CONFIG_MODULES
EXPORT_SYMBOL(intlat_restore_flags);
EXPORT_SYMBOL(intlat_local_irq_enable);
EXPORT_SYMBOL(intlat_cli);
EXPORT_SYMBOL(intlat_sti);
EXPORT_SYMBOL(intlat_local_irq_save);
EXPORT_SYMBOL(intlat_local_irq_restore);
EXPORT_SYMBOL(intlat_local_irq_disable);
#endif	/* CONFIG_MODULES */

#endif	/* CONFIG_INTLAT */

