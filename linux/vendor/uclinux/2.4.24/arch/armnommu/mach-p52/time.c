/*
 * time.c  Timer functions for the P52
 * 2001 Mindspeed
 */

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

extern unsigned long p52_gettimeoffset(void)
{
#if 0
	u16 elapsed;

	/* 
	 * Compute the elapsed count. The current count tells us how
	 * many counts remain until the next interrupt. LATCH tells us
	 * how many counts total occur between interrupts. Subtract to
	 * get the counts since the last interrupt.
	 */
	elapsed = LATCH - inb(P52TIMBase_Addr);

	/* 
	 * Convert the elapsed count to usecs. I guess there are 'tick' usecs
	 * between every interrupt. 
	 */
	return (unsigned long)((elapsed * tick) / LATCH);
#endif
	return (unsigned long)(inl(P52TIMBase_Addr));
}

void p52_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	do_timer(regs);
}


