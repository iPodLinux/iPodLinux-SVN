/*
 * linux/arch/armnommu/mach-S3C44B0X/time.c
 *
 * Copyright (C) 2003 Thomas Eschenbacher <eschenbacher@sympat.de>
 *
 * S3C44B0X timer interrupt handler and gettimeoffset
 *
 */

#include <linux/time.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

#define CLOCKS_PER_USEC	(CONFIG_ARM_CLK/1000000)

#if ((CLOCKS_PER_USEC*1000000) != CONFIG_ARM_CLK)
#warning "clock rate is not in whole MHz, gettimeoffset will give wrong results"
#endif

unsigned long s3c44b0x_gettimeoffset (void)
{
	return (inw(S3C44B0X_TCNTB5) / CLOCKS_PER_USEC);
}

void s3c44b0x_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	do_timer(regs);
}

