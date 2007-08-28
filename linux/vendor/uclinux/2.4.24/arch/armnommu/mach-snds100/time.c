/*
 * time.c  Timer functions for Samsung 4510B
 */

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

unsigned long samsung_gettimeoffset (void)
{
	return 0;
}

void samsung_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        do_timer(regs);
}
