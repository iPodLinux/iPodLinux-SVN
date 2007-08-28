/*
 * time.c - timer support for iPod
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/config.h>
#include <linux/version.h>
#include <asm/io.h>


#define IPOD_TIMER_STATUS 0xcf001110

/*
 * Set the RTC
 */
extern int ipod_set_rtc(void)
{
	return (0);
}


extern unsigned long ipod_gettimeoffset(void)
{
	return inl(IPOD_TIMER_STATUS);
}

extern void ipod_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        do_timer(regs);
}

