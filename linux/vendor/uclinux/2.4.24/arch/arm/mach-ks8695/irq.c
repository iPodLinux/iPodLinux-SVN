/*
 *  linux/arch/arm/mach-ks8695/irq.c
 *
 *  Copyright (C) 2002 Micrel Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/arch/irq.h>	/* pick up fixup_irq definition */
#include <asm/mach/irq.h>

extern void do_ks8695_IRQ(int, struct pt_regs *);

/* 
 * All IO addresses are mapped onto VA 0xFFFx.xxxx, where x.xxxx
 * is the (PA >> 12).
 *
 * Setup a VA for the KS8695 interrupt controller (for header #0,
 * just for now).
 */
#define VA_IO_BASE  IO_ADDRESS(KS8695_IO_BASE) 

static void sc_mask_irq(unsigned int irq)
{
    __raw_writel(__raw_readl(VA_IO_BASE + KS8695_INT_ENABLE) & ~(1 << irq), VA_IO_BASE + KS8695_INT_ENABLE);
}

static void sc_unmask_irq(unsigned int irq)
{
   if ( irq == 8 )
        return ;
   __raw_writel(__raw_readl(VA_IO_BASE + KS8695_INT_ENABLE) | (1 << irq), VA_IO_BASE + KS8695_INT_ENABLE);
}

/*
 * to handle UART interrupt in a special way
 */

void __init ks8695_init_irq(void)
{
	unsigned int i;

        for (i = 0; i < NR_IRQS; i++) 
        {
           if (((1 << i) && KS8695_SC_VALID_INT) != 0) 
           {
              irq_desc[i].valid       = 1;
              irq_desc[i].probe_ok    = 1;
              irq_desc[i].noautoenable = 0;
              irq_desc[i].mask_ack    = sc_mask_irq;
              irq_desc[i].mask        = sc_mask_irq;
              irq_desc[i].unmask      = sc_unmask_irq;
           }
        }

	/* Disable all interrupts initially. */
	__raw_writel(0, VA_IO_BASE + KS8695_INT_CONTL);
	__raw_writel(0, VA_IO_BASE + KS8695_INT_ENABLE);
}
