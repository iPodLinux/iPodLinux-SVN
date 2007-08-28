/*
 * linux/arch/$(ARCH)/platform/$(PLATFORM)/ints.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Apr18 2003, Changed by HuTao to support interrupt cascading for Blackfin 
 *             drivers
 * Sep 2003, Changed to support BlackFin BF533.
 *
 * Copyright 1996 Roman Zippel
 * Copyright 1999 D. Jeff Dionne <jeff@uclinux.org>
 * Copyright 2000-2001 Lineo, Inc. D. Jefff Dionne <jeff@lineo.ca>
 * Copyright 2002 Arcturus Networks Inc. MaTed <mated@sympatico.ca>
 * Copyright 2003 Metrowerks/Motorola
 * Copyright 2003 Bas Vermeulen <bas@buyways.nl>,
 *                BuyWays B.V. (www.buyways.nl)
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/kernel_stat.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/setup.h>

#include <asm/blackfin.h>

/********************************************************************
 * NOTES:
 * - we have separated the physical Hardware interrupt from the
 * levels that the LINUX kernel sees (see the description in irq.h)
 * - 
 ********************************************************************/

#define INTERNAL_IRQS (32)

int irq_flags = 0;

/*********************
 * Prototypes
 ********************/
/* Changed by HuTao, May21, 2003 2:38PM */
asmlinkage void frio_irq_panic( int reason, struct pt_regs * reg);
extern void dump(struct pt_regs * regs);

/* BASE LEVEL interrupt handler routines */
//asmlinkage void evt_reset(void);
asmlinkage void evt_nmi(void);
asmlinkage void evt_exception(void);
asmlinkage void trap(void);
asmlinkage void evt_ivhw(void);
asmlinkage void evt_timer(void);
asmlinkage void evt_evt7(void);
asmlinkage void evt_evt8(void);
asmlinkage void evt_evt9(void);
asmlinkage void evt_evt10(void);
asmlinkage void evt_evt11(void);
asmlinkage void evt_evt12(void);
asmlinkage void evt_evt13(void);
asmlinkage void evt_soft_int1(void);
asmlinkage void evt_system_call(void);

/* irq node variables for the 32 (potential) on chip sources */
static irq_node_t *int_irq_list[INTERNAL_IRQS];

static int int_irq_count[INTERNAL_IRQS];
static short int_irq_ablecount[INTERNAL_IRQS];

/* Modified by HuTao, May21, 2003 */
/*********
 * frio_irq_panic
 * - calls panic with string setup
 *********/
asmlinkage void frio_irq_panic( int reason, struct pt_regs * regs)
{
	
  	printk("\n\nException: IRQ 0x%x entered\n", reason);
	printk("code=[0x%08x], ", regs->seqstat & 0x3f);
	printk("stack frame=0x%04x, ",(unsigned long) regs);
	printk("bad PC=0x%04x\n", regs->pc);
	dump(regs);
	panic("Unhandled IRQ or exceptions!\n");
}

static void int_badint(int irq, void *dev_id, struct pt_regs *fp)
{
	num_spurious += 1;
}

 
/*
 * This function should be called during kernel startup to initialize
 * the Frio IRQ handling routines.
 */

void FRIO_init_IRQ(void)
{
   /*  Disable all the peripheral intrs  - page 4-29 HW Ref manual */	
   *(unsigned volatile long *)(SIC_MASK_ADDR) = SIC_MASK_ALL;
   
   cli();
#ifndef CONFIG_KGDB	// nisa-stub does this if enabled
   *(unsigned volatile long *)(EVT_EMULATION_ADDR) = 
#endif
// RESET is a R/O register ...MaTed---
// *(unsigned volatile long *)(EVT_RESET_ADDR) = 
//                            (unsigned volatile long)evt_reset;
   *(unsigned volatile long *)(EVT_NMI_ADDR) = 
                              (unsigned volatile long)evt_nmi;
   *(unsigned volatile long *)(EVT_EXCEPTION_ADDR) = 
                              (unsigned volatile long)trap;
   *(unsigned volatile long *)(EVT_HARDWARE_ERROR_ADDR) = 
                              (unsigned volatile long)evt_ivhw;
   *(unsigned volatile long *)(EVT_TIMER_ADDR) = 
                              (unsigned volatile long)evt_timer;
   *(unsigned volatile long *)(EVT_IVG7_ADDR) = 
                              (unsigned volatile long)evt_evt7;
   *(unsigned volatile long *)(EVT_IVG8_ADDR) = 
                              (unsigned volatile long)evt_evt8;
   *(unsigned volatile long *)(EVT_IVG9_ADDR) = 
                              (unsigned volatile long)evt_evt9;
   *(unsigned volatile long *)(EVT_IVG10_ADDR) = 
                              (unsigned volatile long)evt_evt10;
   *(unsigned volatile long *)(EVT_IVG11_ADDR) = 
                              (unsigned volatile long)evt_evt11;
   *(unsigned volatile long *)(EVT_IVG12_ADDR) = 
                              (unsigned volatile long)evt_evt12;
   *(unsigned volatile long *)(EVT_IVG13_ADDR) = 
                              (unsigned volatile long)evt_evt13;
   *(unsigned volatile long *)(EVT_IVG14_ADDR) = 
                              (unsigned volatile long)evt_system_call;
   *(unsigned volatile long *)(EVT_IVG15_ADDR) = 
                              (unsigned volatile long)evt_soft_int1;

   /* Enable interrupts IVG7-15 */
   IMASK = irq_flags |= 0xffa0;
   ILAT=0;
   sti();
}

void FRIO_enable_irq(unsigned int irq);
void FRIO_disable_irq(unsigned int irq);

int FRIO_request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *), unsigned long flags, const char *devname, void *dev_id)
{
	if (irq >= INTERNAL_IRQS) {
		printk("%s: Unknown IRQ %d from %s\n", 
			      __FUNCTION__, irq, devname);
		return -ENXIO;
	}

	if (!int_irq_list[irq]) {
		int_irq_list[irq] = new_irq_node();
		int_irq_list[irq]->flags   = IRQ_FLG_STD;
	}

	if (!(int_irq_list[irq]->flags & IRQ_FLG_STD)) {
		if (int_irq_list[irq]->flags & IRQ_FLG_LOCK) {
			printk("IRQ %d is already assigned.\n", irq);
			return -EBUSY;
		}
		if (flags & IRQ_FLG_REPLACE) {
			printk("Can't replace IRQ %d\n", irq);
			return -EBUSY;
		}
	}
	int_irq_list[irq]->handler = handler;
	int_irq_list[irq]->flags   = flags;
	int_irq_list[irq]->dev_id  = dev_id;
	int_irq_list[irq]->devname = devname;

	return 0;
}

void FRIO_free_irq(unsigned int irq, void *dev_id)
{
	if (irq >= INTERNAL_IRQS) {
		printk ("%s: Unknown IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (int_irq_list[irq]->dev_id != dev_id)
		printk("%s: removing probably wrong IRQ %d from %s\n",
		       __FUNCTION__, irq, int_irq_list[irq]->devname);
	int_irq_list[irq]->handler = int_badint;
	int_irq_list[irq]->flags   = IRQ_FLG_STD;
	int_irq_list[irq]->dev_id  = NULL;
	int_irq_list[irq]->devname = NULL;

	FRIO_disable_irq(irq);
}

/*
 * Enable/disable a particular machine specific interrupt source.
 * Note that this may affect other interrupts in case of a shared interrupt.
 * This function should only be called for a _very_ short time to change some
 * internal data, that may not be changed by the interrupt at the same time.
 * int_(enable|disable)_irq calls may also be nested.
 */

void FRIO_enable_irq(unsigned int irq)
{
	unsigned long irq_val;

	cli();
	if (irq >= INTERNAL_IRQS) {
		printk("%s: Unknown IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (irq <= IRQ_CORETMR)
	{
		/* enable the interrupt */
		irq_flags |= 1<<irq;
		sti();
		return;
	}

	if (irq == IRQ_SPORT0)
		irq_val = 0x600;
	else if (irq == IRQ_SPORT1)
		irq_val = 0x1800;
	else if (irq == IRQ_UART)
		irq_val = 0xC000;
	else
		irq_val = (1<<(irq - IRQ_CORETMR));

   	*(unsigned volatile long *)(SIC_MASK_ADDR) |= irq_val;

	sti();
	
}

void FRIO_disable_irq(unsigned int irq)
{
	unsigned long irq_val;

	if (irq >= INTERNAL_IRQS) {
		printk("%s: Unknown IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (irq < IRQ_CORETMR)
	{
		cli();
		irq_flags &= ~(1<<irq);
		sti();
		return;
	}

	/*
 	 * If it is the interrupt for peripheral,
	 * we only disable it in SIC_IMASK register.
	 * No need to change IMASK register of CORE,
	 * since all of the IVG for peripherals was 
 	 * enabled in FRIO_init_IRQ()
	 *
 	 * HuTao, Apr18 2003
	 */

	if (irq == IRQ_SPORT0)
		irq_val = 0x600;
	else if (irq == IRQ_SPORT1)
		irq_val = 0x1800;
	else if (irq == IRQ_UART)
		irq_val = 0xC000;
	else
		irq_val = (1<<(irq - IRQ_CORETMR));

	cli();

   	*(unsigned volatile long *)(SIC_MASK_ADDR) &= ~(irq_val); 
	
	sti();

}

void  call_isr(int irq, struct pt_regs * fp)
{
	if(int_irq_list[irq]->handler)
	    int_irq_list[irq]->handler(irq,int_irq_list[irq]->dev_id, fp);
	else 
	    printk("unregistered interrupt %d\n", irq); 
}

void FRIO_do_irq(int vec, struct pt_regs *fp)
{
    /*
     * We will check the SIC_IPEND to determine if it is
     * the peripherals' interrupt.
     * 
     * HuTao, Apr18 2003
     */
    if (vec <= IRQ_CORETMR)
    {	
	 call_isr(vec, fp);
	 return;
    }

    switch (vec)
    {
    case IVG7:
	    if ((SIC_ISR & 0x1) && (SIC_MASK & 0x1))
		    call_isr(IRQ_PLL_WAKEUP, fp);
	    if ((SIC_ISR & 0x2) && (SIC_MASK & 0x2))
		    call_isr(IRQ_DMA_ERROR, fp);
	    if ((SIC_ISR & 0x4) && (SIC_MASK & 0x4))
		    call_isr(IRQ_PPI_ERROR, fp);
	    if ((SIC_ISR & 0x8) && (SIC_MASK & 0x8))
		    call_isr(IRQ_SPORT0_ERROR, fp);
	    if ((SIC_ISR & 0x10) && (SIC_MASK & 0x10))
		    call_isr(IRQ_SPORT1_ERROR, fp);
	    if ((SIC_ISR & 0x20) && (SIC_MASK & 0x20))
		    call_isr(IRQ_SPI_ERROR, fp);
	    if ((SIC_ISR & 0x40) && (SIC_MASK & 0x40))
		    call_isr(IRQ_UART_ERROR, fp);
	    break;
    case IVG8:
	    if ((SIC_ISR & 0x80) && (SIC_MASK & 0x80))
		    call_isr(IRQ_RTC, fp);
	    if ((SIC_ISR & 0x100) && (SIC_MASK & 0x100))
		    call_isr(IRQ_PPI, fp);
	    break;
    case IVG9:
	    if ((SIC_ISR & 0x600) && (SIC_MASK & 0x600))
		    call_isr(IRQ_SPORT0, fp);
	    if ((SIC_ISR & 0x1800) && (SIC_MASK & 0x1800))
		    call_isr(IRQ_SPORT1, fp);
	    break;
    case IVG10:
	    if ((SIC_ISR & 0x2000) && (SIC_MASK & 0x2000))
		    call_isr(IRQ_SPI, fp);
	    if ((SIC_ISR & 0xC000) && (SIC_MASK & 0xC000))
		    call_isr(IRQ_UART, fp);
	    break;
    case IVG11:
	    if ((SIC_ISR & 0x10000) && (SIC_MASK & 0x10000))
		    call_isr(IRQ_TMR0, fp);
	    if ((SIC_ISR & 0x20000) && (SIC_MASK & 0x20000))
		    call_isr(IRQ_TMR1, fp);
	    if ((SIC_ISR & 0x40000) && (SIC_MASK & 0x40000))
		    call_isr(IRQ_TMR2, fp);
	    break;
    case IVG12:
	    if ((SIC_ISR & 0x80000) && (SIC_MASK & 0x80000))
		    call_isr(IRQ_PROG_INTA, fp);
	    if ((SIC_ISR & 0x100000) && (SIC_MASK & 0x100000))
		    call_isr(IRQ_PROG_INTB, fp);
	    break;
    case IVG13:
	    if ((SIC_ISR & 0x200000) && (SIC_MASK & 0x200000))
		    call_isr(IRQ_MEM_DMA0, fp);
	    if ((SIC_ISR & 0x400000) && (SIC_MASK & 0x400000))
		    call_isr(IRQ_MEM_DMA1, fp);
	    if ((SIC_ISR & 0x800000) && (SIC_MASK & 0x800000))
		    call_isr(IRQ_WATCH, fp);
	    break;
    }
}

int FRIO_get_irq_list(char *buf)
{
	int i, len = 0;
	irq_node_t *node;

	len += sprintf(buf+len, "Internal Blackfin interrupts\n");

	for (i = 0; i < INTERNAL_IRQS; i++) {
		if (!(node = int_irq_list[i]))
			continue;
		if (!(node->handler))
			continue;

		len += sprintf(buf+len, " %2d: %10u    %s\n", i,
		               int_irq_count[i], int_irq_list[i]->devname);
	}
	return len;
}

void config_FRIO_irq(void)
{
	mach_default_handler = NULL;
	mach_init_IRQ        = FRIO_init_IRQ;
	mach_request_irq     = FRIO_request_irq;
	mach_free_irq        = FRIO_free_irq;
	mach_enable_irq      = FRIO_enable_irq;
	mach_disable_irq     = FRIO_disable_irq;
	mach_get_irq_list    = FRIO_get_irq_list;
	mach_process_int     = FRIO_do_irq;
}
