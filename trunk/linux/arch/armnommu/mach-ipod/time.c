/*
 * time.c - timer support for iPod
 *
 * Copyright (c) 2003-2005, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/config.h>
#include <linux/version.h>
#include <asm/timex.h>
#include <asm/io.h>


/*
 * Set the RTC to the current kernel time
 */
extern int ipod_set_rtc(void)
{
	/* TODO: not yet implemented */
	return 0;
}

/*
 * Get the number of useconds since the last interrupt
 */
extern unsigned long pp5002_gettimeoffset(void)
{
	return USECS_PER_INT - (inl(PP5002_TIMER1) & 0xffff);
}

extern unsigned long pp5020_gettimeoffset(void)
{
	return USECS_PER_INT - (inl(PP5020_TIMER1) & 0xffff);
}

extern void ipod_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        do_timer(regs);
}

