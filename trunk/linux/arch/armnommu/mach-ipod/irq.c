/*
 * irq.c - irq processing for iPod
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/irq.h>
#include <asm/timex.h>

/* PP5002 functions */
static void pp5002_unmask_irq(unsigned int irq)
{
	/* unmask the interrupt */
	outl((1 << irq), 0xcf001024);
	/* route it to IRQ not FIQ */
	outl(inl(0xcf00102c) & ~(1 << irq), 0xcf00102c);
}

static void pp5002_mask_irq(unsigned int irq)
{
	/* mask the interrupt */
	outl((1 << irq), 0xcf001028);
}

static void pp5002_mask_ack_irq(unsigned int irq)
{

	/* there is no general IRQ ack, we have to do it at the source */
	switch (irq) {
	case IDE_INT0_IRQ:
		/* clear FIFO interrupt status */
		outl(0xff, 0xc0003020);
		outl(inl(0xc0003024) | (1<<4) | (1<<5), 0xc0003024);
		break;

	case TIMER1_IRQ:
		inl(PP5002_TIMER1_ACK);
		break;
	}

	pp5002_mask_irq(irq);
}

/* PP5020 functions */

#define PP5020_TIMER1_MASK (1<<0)

static void pp5020_unmask_irq(unsigned int irq)
{
	switch (irq) {
	case TIMER1_IRQ:
		/* route it to IRQ not FIQ */
		outl(inl(0x6000402c) & ~PP5020_TIMER1_MASK, 0x6000402c);
		/* unmask the interrupt */
		outl(PP5020_TIMER1_MASK, 0x60004024);
		break;

	case IDE_INT0_IRQ:
		outl(inl(0xc3000028) | (1<<5), 0xc3000028);
		/* fall through */

	default:
		/* hi interrupt enable */
		outl(0x40000000, 0x60004024);
		/* route it to IRQ not FIQ */
		outl(inl(0x6000412c) & ~(1<<irq), 0x6000412c);
		/* unmask the interrupt */
		outl((1<<irq), 0x60004124);
		break;
	}
}

static void pp5020_mask_irq(unsigned int irq)
{
	/* mask the interrupt */
	switch (irq) {
	case TIMER1_IRQ:
		outl(PP5020_TIMER1_MASK, 0x60004028);
		break;
	default:
		outl((1<<irq), 0x60004128);
	}
}

static void pp5020_mask_ack_irq(unsigned int irq)
{
	/* there is no general IRQ ack, we have to do it at the source */
	switch (irq) {
	case TIMER1_IRQ:
		inl(PP5020_TIMER1_ACK);
		break;

	case IDE_INT0_IRQ:
		outl(inl(0xc3000028) & ~((1<<4) | (1<<5)), 0xc3000028);
		break;
	}

	pp5020_mask_irq(irq);
}

int ipod_init_irq(void)
{
	int irq, ipod_hw_ver;

	ipod_hw_ver = ipod_get_hw_version() >> 16;

	/* disable all interrupts */
	if (ipod_hw_ver > 0x3) {
		outl(-1, 0x60001138);
		outl(-1, 0x60001128);
		outl(-1, 0x6000111c);

		outl(-1, 0x60001038);
		outl(-1, 0x60001028);
		outl(-1, 0x6000101c);
	}
	else {
		outl(-1, 0xcf00101c);
		outl(-1, 0xcf001028);
		outl(-1, 0xcf001038);
	}

	/* clear all interrupts */
	for ( irq = 0; irq < NR_IRQS; irq++ ) {

		if (!VALID_IRQ(irq)) continue;

		irq_desc[irq].valid     = 1;
		irq_desc[irq].probe_ok  = 1;
		if (ipod_hw_ver > 0x3) {
			irq_desc[irq].mask_ack  = pp5020_mask_ack_irq;
			irq_desc[irq].mask      = pp5020_mask_irq;
			irq_desc[irq].unmask    = pp5020_unmask_irq;
		}
		else {
			irq_desc[irq].mask_ack  = pp5002_mask_ack_irq;
			irq_desc[irq].mask      = pp5002_mask_irq;
			irq_desc[irq].unmask    = pp5002_unmask_irq;
		}
	}

	/*
	 * since the interrupt vectors at 0x0 are now installed
	 * we can wake up the COP safely
	 */
	if (ipod_hw_ver > 0x3) {
		outl(0x0, 0x60007004);
	}
	else {
		outl(0xce, 0xcf004058);
	}

        return 0;
}

