/*
 * linux/arch/arm/mach-ixp1200/irq.c
 *
 * Generic IRQ handling for IXP1200 based systems. 
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 1996-1998 Russell King
 * Copyright (C) 2001 MontaVista Software, Inc
 *
 * Changelog:
 *  16-Mar-2000 Uday N  Modified for IXP 1200 Eval Board
 *  26-Sep-2001 dsaxena Move to 2.4.x kernel & cleanup
 *  01-Mar-2002 dsaxena	More cleanup, added support for all IRQ sources
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/mach/irq.h>
#include <asm/irq.h>
#include <asm/hardware.h>

/*
 * IXP1200 Interrupt registers 
 */
static volatile unsigned long* irq_reg[] = 
	{	 
		CSR_IREG,	/* uEngine	*/
		CSR_IREG,	/* FBI (CINT)	*/
		CSR_SRAM,	/* SRAM		*/
		NULL,		/* UART		*/
		CSR_RTC_DIV,	/* RTC		*/
		CSR_SDRAM,	/* SDRAM	*/
		CSR_IRQ_STATUS, /* SWI		*/
		CSR_IRQ_STATUS,	/* TIMER1	*/
		CSR_IRQ_STATUS,	/* TIMER2	*/
		CSR_IRQ_STATUS,	/* TIMER3	*/
		CSR_IRQ_STATUS,	/* TIMER4	*/
		CSR_IRQ_STATUS, /* DOORBELL	*/
		CSR_IRQ_STATUS,	/* DMA1		*/
		CSR_IRQ_STATUS,	/* DMA2		*/
		CSR_IRQ_STATUS,	/* PCI		*/
		CSR_IRQ_STATUS, /* DMA1NB	*/
		CSR_IRQ_STATUS,	/* DMA2NB	*/
		CSR_IRQ_STATUS,	/* BIST		*/
		CSR_IRQ_STATUS, /* I2O		*/
		CSR_IRQ_STATUS, /* PWRM		*/
		CSR_IRQ_STATUS	/* PCI Error	*/
	};

static unsigned long irq_mask[] = 
	{

		(1 << 30),     /* microengine interrupts */
		(1 << 28),     /* FBI (CINT)  */
		(1 << 3),      /* SRAM        */
		0,             /* UART        */
		(1 << 19),	/* RTC */
		0,             /* Bug in documentation - ???  */
		IRQ_MASK_SWI,
		IRQ_MASK_TIMER1,
		IRQ_MASK_TIMER2,
		IRQ_MASK_TIMER3,
		IRQ_MASK_TIMER4,
		IRQ_MASK_DOORBELL,
		IRQ_MASK_DMA1,
		IRQ_MASK_DMA2,
		IRQ_MASK_PCI,
		IRQ_MASK_DMA1NB,
		IRQ_MASK_DMA2NB,
		IRQ_MASK_BIST,
		IRQ_MASK_I2O,
		IRQ_MASK_PWRM,
		IRQ_MASK_PCI_ERR
	};


static void 
ixp1200_mask_irq(unsigned int irq)
{ 
	volatile unsigned long* irqreg  = irq_reg[irq];
	unsigned long irqmask = irq_mask[irq];

	/* 
	 * For certain devices, we let the drivers handle it as there
	 * is no "mask" bit
	 */
	if (irqreg == 0) return;

	if (irqreg == CSR_IRQ_STATUS)
	{
		/* 
		 * For these interrupts we just set the appropriate bit in the
		 * clear register.  The write has no effect on the bits not set
		 */
		  
		*CSR_IRQ_DISABLE = irqmask;
	}
	else
	{
		/* 
		 * Just set those bits to 0 NOTE:: For UENG and CINT,
		 * the first 26 bits are write 1 to clear. So we dont want to
		 * clear those.  
		 */
		if ( irqreg == CSR_IREG)
			*irqreg &= (~irqmask & 0xf8000000);
		else
			*irqreg &= ~irqmask;
	}

}

static void 
ixp1200_unmask_irq(unsigned int irq)
{
	volatile unsigned long* irqreg  = irq_reg[irq];
	unsigned long irqmask = irq_mask[irq];

	if (irqreg == 0) return;


	if (irqreg == CSR_IRQ_STATUS)
		*CSR_IRQ_ENABLE = irqmask; 
	else
		*irqreg |= irqmask;
}

__init void 
ixp1200_init_irq(void)
{
	int irq;

	*CSR_IRQ_DISABLE = -1;
	*CSR_FIQ_DISABLE = -1;
 
	for (irq = 0; irq < NR_IRQS; irq++) 
	{
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= ixp1200_mask_irq;
		irq_desc[irq].mask	= ixp1200_mask_irq;
		irq_desc[irq].unmask	= ixp1200_unmask_irq;
	}
}

