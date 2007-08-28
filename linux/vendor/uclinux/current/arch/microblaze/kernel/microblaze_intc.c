/*
 * arch/microblaze/kernel/microblaze_intc.c -- Microblaze cpu core interrupt controller (INTC)
 *
 *  Copyright (C) 2003       John Williams <jwilliams@itee.uq.edu.au>
 *                           based upon v850 version 
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by John Williams <jwilliams@itee.uq.edu.au>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/microblaze_intc.h>


/* For now, don't ack interrupt until end of interrupt processing */
/* Same uncertainty exists in PPC xintc driver */
#define ACKNOWLEDGE_AFTER

static void irq_nop (unsigned irq) { }

/* Master enable to start the intc */
void microblaze_intc_master_enable(void)
{
  __asm__ __volatile__ (
		"	sw	%0, r0, %1;"
		:: "r" (MICROBLAZE_INTC_MASTER_ENABLE_MASK | MICROBLAZE_INTC_HARDWARE_ENABLE_MASK), "r" (MICROBLAZE_INTC_MER_ADDR));
}

static unsigned microblaze_intc_irq_startup (unsigned irq)
{
	microblaze_intc_clear_pending_irq (irq);
	microblaze_intc_enable_irq (irq);
	return 0;
}

/* Initialize HW_IRQ_TYPES for INTC-controlled irqs described in array
   INITS (which is terminated by an entry with the name field == 0).  */
void __init microblaze_intc_init_irq_types (struct microblaze_intc_irq_init *inits,
				       struct hw_interrupt_type *hw_irq_types)
{
	struct microblaze_intc_irq_init *init;
	for (init = inits; init->name; init++) {
		int i;
		struct hw_interrupt_type *hwit = hw_irq_types++;

		hwit->typename = init->name;

		hwit->startup  = microblaze_intc_irq_startup;
		hwit->shutdown = microblaze_intc_disable_irq;
		hwit->enable   = microblaze_intc_enable_irq;
		hwit->disable  = microblaze_intc_disable_irq;
		hwit->ack      = microblaze_intc_disable_and_ack_irq;
		hwit->end      = microblaze_intc_end;
/* 
		hwit->ack      = irq_nop;
		hwit->end      = microblaze_intc_ack_irq;
*/
		
		/* Initialize kernel IRQ infrastructure for this interrupt.  */
		init_irq_handlers (init->base, init->num, hwit);

		/* 
		 * Clear and disable all interrupts.
		 * In the v850 code this also sets the priorities, but
		 * priorites are fixed in Microblaze (int0 -> highest)
		 * so we don't do that here 
		 */
	
		for (i = 0; i < init->num; i++) {
			unsigned irq = init->base + i;

			/* If the interrupt is currently enabled (all
			   interrupts are initially disabled), then
			   assume whoever enabled it has set things up
			   properly, and avoid messing with it.  */
			if (!microblaze_intc_irq_enabled (irq)) {
				microblaze_intc_disable_irq(irq);
				microblaze_intc_clear_pending_irq(irq);
			}
		}
	}
}

/* Now the actual IRQ controller functions */
/* 
 * Get active IRQ from controller 
 * Returns -1 if no IRQ is active.  This shouldn't happen because 
 * this is only called from handle_irq()
 */
int microblaze_intc_get_active_irq(void)
{

#ifdef CONFIG_MICROBLAZE_INTC_HAS_IVR
	int irq;
	/* Query the IVR register of the INTC */
	__asm__ __volatile__ ("	lw	%0, r0, %1"
				: "=r" (irq)
				: "r" (MICROBLAZE_INTC_IVR_ADDR)
				: "memory");
	return irq;
#else
	unsigned int isr, ier, active, irq=0;
	__asm__ __volatile__ ("	lw	%0, r0, %2;
				lw	%1, r0, %3"
				: "=r" (isr),
				  "=r" (ier)
				: "r" (MICROBLAZE_INTC_ISR_ADDR),
				  "r" (MICROBLAZE_INTC_IER_ADDR)
				: "memory");
	
	/* Find active and enabled IRQs */
	active = isr & ier;
	/* Search higest priority one */
	while(!(active & 1)) {
		active >>= 1;
		irq++;
	}

	/* Was one found? */
	if(irq>31)
		return -1;	/* No?  Return -1 */
	else
		return irq;	

#endif /* CONFIG_MICROBLAZE_INTC_HAS_IVR */
}

/* Acknowledge irq */
void microblaze_intc_ack_irq(unsigned irq)
{
	unsigned irq_mask = (1 << irq);

	__asm__ __volatile__ ("	sw	%0, r0, %1"
				:
				: "r" (irq_mask),
				  "r" (MICROBLAZE_INTC_IAR_ADDR)
				: "memory");
}

/* Enable interrupt handling for interrupt IRQ.  */
void microblaze_intc_enable_irq (unsigned irq)
{
	/* Convert irq to bit mask */
	unsigned int irq_mask = 1 << irq;
	
#ifdef CONFIG_MICROBLAZE_INTC_HAS_SIE
	/* Use Set Interrupt Enables register (SIE) to avoid test and set */
	__asm__ __volatile__ ("sw	%0, r0, %1"
				: 
				: "r" (irq_mask),
				  "r" (MICROBLAZE_INTC_SIE_ADDR)
				: "memory");
#else
	unsigned int old_mask;
	/* get old IER, set bit, reset IER */
	__asm__ __volatile__ ("	lw	%0, r0, %3;
				ori	%0, %1, %2;
				sw	%1, r0, %3"
				:"=r" (old_mask)
				:"0" (old_mask),
				 "r" (irq_mask),
				 "r" (MICROBLAZE_INTC_IER_ADDR)
				: "memory");
#endif /* CONFIG_MICROBLAZE_INTC_HASE_SIE */
}

/* Disable interrupt handling for interrupt IRQ.  Note that any
   interrupts received while disabled will be delivered once the
   interrupt is enabled again, unless they are explicitly cleared using
   `microblaze_intc_clear_pending_irq'.  */
void microblaze_intc_disable_irq (unsigned irq)
{

#ifdef CONFIG_MICROBLAZE_INTC_HAS_CIE
	unsigned int irq_mask = 1 << irq;
	__asm__ __volatile__ ("	sw	%0, r0, %1"
				:
				: "r" (irq_mask),
				  "r" (MICROBLAZE_INTC_CIE_ADDR)
				: "memory");
#else
	unsigned int old_mask;
	unsigned int irq_mask = ~(1 << irq);
	/* get old IER, clear bit, reset IER */
	__asm__ __volatile__ ("	lw	%0, r0, %3;
				andi	%0, %1, %2;
				sw	%1, r0, %3"
				: "=r" (old_mask)
				: "0" (old_mask),
				  "r" (irq_mask),
				  "r" (MICROBLAZE_INTC_IER_ADDR)
				: "memory");
#endif /* CONFIG_MICROBLAZE_INTC_HAS_CIE */
}

/* Return true if interrupt handling for interrupt IRQ is enabled.  */
int microblaze_intc_irq_enabled (unsigned irq)
{
	unsigned int irq_mask;
	__asm__ __volatile__ ("	lw	%0, r0, %1"
				: "=r" (irq_mask)
				: "r" (MICROBLAZE_INTC_IER_ADDR)
				: "memory");

	return irq_mask & (1 << irq);
}

/* Disable irqs from 0 until LIMIT.  */
void _microblaze_intc_disable_irqs (unsigned limit)
{

#ifdef CONFIG_MICROBLAZE_INTC_HAS_CIE
	unsigned int irq_mask = ~((1 << (limit+1)) -1);
	__asm__ __volatile__ ("	sw	%0, r0, %1"
				:
				: "r" (irq_mask),
				  "r" (MICROBLAZE_INTC_CIE_ADDR)
				: "memory");
#else
	/* Create a bit mask with all ones except zeros for lower IRQs */
	unsigned int irq_mask = (1 << (limit+1))-1;
	unsigned int old_mask;
	/* get old IER, clear bits, reset IER */
	__asm__ __volatile__ ("	lw	%0, r0, %3;
				andi	%0, %1, %2;
				sw	%1, r0, %3"
				: "=r" (old_mask)
				: "0" (old_mask),
				  "r" (irq_mask),
				  "r" (MICROBLAZE_INTC_IER_ADDR)
				: "memory");
#endif /* CONFIG_MICROBLAZE_INTC_HAS_CIE */

}

/* Clear any pending interrupts for IRQ.  */
void microblaze_intc_clear_pending_irq (unsigned irq)
{
	unsigned int irq_mask = 1 << irq;
	__asm__ __volatile__ ("	sw	%0, r0, %1"
				:
				: "r" (irq_mask),
				  "r" (MICROBLAZE_INTC_IAR_ADDR)
				: "memory");
}

/* Return true if interrupt IRQ is pending (but disabled).  */
int microblaze_intc_irq_pending (unsigned irq)
{
	int irq_mask = 1 << irq;
	unsigned int ier_mask, isr_mask;
	
	__asm__ __volatile__ ("	lw	%0, r0, %2;
				lw	%1, r0, %3"
				: "=r" (ier_mask),
				  "=r" (isr_mask)
				: "r" (MICROBLAZE_INTC_IER_ADDR),
				  "r" (MICROBLAZE_INTC_ISR_ADDR)
				: "memory");

	/* Return 1 if irq in question is pending but disabled */
	return irq_mask & (isr_mask & ~ier_mask);
}

  
void microblaze_intc_disable_and_ack_irq(unsigned irq)
{
	microblaze_intc_disable_irq(irq);
	#if !defined(ACKNOWLEDGE_AFTER)
	microblaze_intc_ack_irq(irq);
	#endif
}

void microblaze_intc_end(unsigned irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS))) {
		microblaze_intc_enable_irq(irq);
		#if defined(ACKNOWLEDGE_AFTER)
		microblaze_intc_ack_irq(irq);
		#endif
	}
}

