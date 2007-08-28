/*
 *  linux/arch/h8300/platform/h8s/generic/timer.c
 *
 *  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 *  Platform depend Timer Handler
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>

#define TMR8TCOR1 0x00ffffb5
#define TMR8TCSR1 0x00ffffb3
#define TMR8TCR1 0x00ffffb1
#define MSTPCRL 0x00ffff41

int platform_timer_setup(void (*timer_int)(int, void *, struct pt_regs *))
{
	unsigned char mstpcrl;
	mstpcrl = inb(MSTPCRL);
	mstpcrl &= ~0x01;
	outb(mstpcrl,MSTPCRL);
	outb(CONFIG_CLK_FREQ*10/8192,TMR8TCOR1);
	outb(0x00,TMR8TCSR1);
	request_irq(76,timer_int,0,"timer",0);
	outb(0x40|0x08|0x03,TMR8TCR1);
}

void platform_timer_eoi(void)
{
        __asm__("bclr #6,@0xffffb3:8");
}

void platform_gettod(int *year, int *mon, int *day, int *hour,
		 int *min, int *sec)
{
	*year = *mon = *day = *hour = *min = *sec = 0;
}
