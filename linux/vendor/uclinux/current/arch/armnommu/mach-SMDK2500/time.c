/*
 * time.c Timer functions for SMDK2500-AN
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
    static int sec, on;
    
    if  (mS++ > 49) {
	if  ( on ) {
	     Vol32(0xf0030000) &= 0xffffff00;	
	     Vol32(0xf003001c) = (255-sec);
	     if  (sec++ > 255) sec = 0;
	     on = 0;
	     }
	else {
	     Vol32(0xf0030000) |= 0x000000ff;
	     on = 1;
	     }
	mS = 0;
	}
    	
    REG_WRITE(IC, (1<<(irq-31)));
    
    do_timer(regs);
}
