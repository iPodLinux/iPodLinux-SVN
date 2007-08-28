/*
 * time.c  Timer functions for evS3C4530HEI
 * OZH, 2001 Oleksandr Zhadan
 */

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

#define CLOCKS_PER_USEC	50	/* (ARM_CLK/1000000) */

unsigned long s3c4530_gettimeoffset (void)
{
    struct s3c4530_timer* tt = (struct s3c4530_timer*) (TIMER_BASE);
#if 	( KERNEL_TIMER==0 ) 
    return ((tt->tdr0 - tt->tcr0) / CLOCKS_PER_USEC);
#elif	( KERNEL_TIMER==1 ) 
    return ((tt->tdr1 - tt->tcr1) / CLOCKS_PER_USEC);
#else
#error Weird -- KERNEL_TIMER does not seem to be defined...
#endif	
}

void s3c4530_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    do_timer(regs);
}
