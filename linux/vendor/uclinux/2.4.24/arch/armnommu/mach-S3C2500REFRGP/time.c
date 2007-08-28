/* 
 * time.c Timer functions for S3C2500
 *
 * Copyright (C) 2002 Arcturus Networks Inc.
 *                    by Oleksandr Zhadan <www.ArcturusNetworks.com>
 *
 */

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

#define HZMSEC	10000	/* (HZ*100) */

unsigned long s3c2500_gettimeoffset (void)
{
    struct s3c2500_timer *tt = (struct s3c2500_timer *) (TIMER_BASE);
    return ((tt->td[2*KERNEL_TIMER] - tt->td[2*KERNEL_TIMER+1]) / (tt->td[2*KERNEL_TIMER]/HZMSEC));
}

void s3c2500_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    static int mS;
    static int led;
    if  (mS++ > 49) {
	led = Vol32(0xf003001c);
	if   ( led & 0x08000000 )
	     Vol32(0xf003001c) = led & 0xf7ffffff;
	else
	     Vol32(0xf003001c) = led | 0x08000000;
	mS = 0;
	}

    REG_WRITE(IC, (1<<(irq-31)));
    do_timer(regs);
}
