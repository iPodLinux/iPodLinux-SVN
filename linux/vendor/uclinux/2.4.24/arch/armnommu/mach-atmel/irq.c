/*
 *  linux/arch/arm/mach-integrator/irq.c
 *
 *  Copyright (C) 1999 ARM Limited
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
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

extern struct irqdesc irq_desc[];

 /* Internal Sources */
#define LevelSensitive              (0<<5)
#define EdgeTriggered               (1<<5)

 /* External Sources */
#define LowLevel                    (0<<5)
#define NegativeEdge                (1<<5)
#define HighLevel                   (2<<5)
#define PositiveEdge                (3<<5)

static unsigned char eb01_irq_prtable[32] = {
        7 << 5, /* FIQ */
        0 << 5, /* SWIRQ */
        0 << 5, /* US0IRQ */
        0 << 5, /* US1IRQ */
        2 << 5, /* TC0IRQ */
        2 << 5, /* TC1IRQ */
        2 << 5, /* TC2IRQ */
        0 << 5, /* WDIRQ */
        0 << 5, /* PIOAIRQ */
        0 << 5, /* reserved */
        0 << 5, /* reserved */
        0 << 5, /* reserved */
        0 << 5, /* reserved */
        0 << 5, /* reserved */
        0 << 5, /* reserved */
        0 << 5, /* reserved */
        1 << 5, /* IRQ0 */
	0 << 5, /* IRQ1 */
        0 << 5, /* IRQ2 */
};

static unsigned char eb01_irq_type[32] = {
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,

        EdgeTriggered,	/* IRQ0 = neg. edge */
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
        EdgeTriggered,
};

void at91_mask_irq(unsigned int irq)
{
	unsigned long mask = 1 << (irq);
	__arch_putl(mask, AIC_IDCR);
}	

void at91_unmask_irq(unsigned int irq)
{
	unsigned long mask = 1 << (irq);
	__arch_putl(mask, AIC_IECR);
}

void at91_mask_ack_irq(unsigned int irq)
{
	at91_mask_irq(irq);
}

void at91_init_aic()
{
        int irqno;

        /* Disable all interrupts */
        __arch_putl(0xFFFFFFFF, AIC_IDCR);

        /* Clear all interrupts	*/
        __arch_putl(0xFFFFFFFF, AIC_ICCR);

        for ( irqno = 0 ; irqno < 32 ; irqno++ )
        {
                __arch_putl(irqno, AIC_EOICR);
        }

        for ( irqno = 0 ; irqno < 32 ; irqno++ )
        {
            __arch_putl((eb01_irq_prtable[irqno] >> 5) | eb01_irq_type[irqno],
		 AIC_SMR(irqno));
        }
}

