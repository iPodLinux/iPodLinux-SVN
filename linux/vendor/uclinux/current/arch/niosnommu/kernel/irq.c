/*
 * linux/arch/$(ARCH)/irq.c -- general exception handling code
 *
 * Cloned from Linux/m68k.
 *
 * No original Copyright holder listed,
 * Probabily original (C) Roman Zippel (assigned DJD, 1999)
 *
 * Copyright 1999-2000 D. Jeff Dionne, <jeff@uClinux.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/nios.h>
#include <asm/hardirq.h>

/* assembler routines */
asmlinkage void system_call(void);
asmlinkage void real_irq_entry(void);
asmlinkage void bad_trap_handler(void);
asmlinkage void timer_handler(void);
asmlinkage void binfmt_flat_debug_handler(void);
asmlinkage void ocidec_handler(void);

/* table for system interrupt handlers */
static irq_handler_t irq_list[SYS_IRQS];

static const char *default_names[SYS_IRQS] = {
	"huh", "EDAC", "UART 1", "UART 2",
	"pio0 handler", "pio1 handler", "pio2 handler", "pio3 handler",
	"Timer 1", "Timer 2",
};

/* The number of spurious interrupts */
volatile unsigned int num_spurious;

#define NUM_IRQ_NODES 16
static irq_node_t nodes[NUM_IRQ_NODES];

void __init init_irq_proc(void)
{
	/* Insert /proc/irq driver here */
}

/*
 * void init_IRQ(void)
 *
 * Parameters:	None
 *
 * Returns:	Nothing
 *
 * This function should be called during kernel startup to initialize
 * the IRQ handling routines.
 */

void __init init_IRQ(void)
{
	int i;
	unsigned int *vectorTable;

	vectorTable = (int *)nasys_vector_table;

/* Vic - RAM vector table inits */
	/* set up the vectors */
//	_ramvec[0] = 0;					/* hard reset  */
//	_ramvec[1] = 0;					/* underflow   */
//	_ramvec[2] = 0;					/* overflow    */
//	_ramvec[3] = 0;					/* breakpoint  */
//	_ramvec[4] = 0;					/* single step */
//	_ramvec[5] = 0;					/* GDB start   */
//      _ramvec[6] = 0;                                 /* binfmt_flat_debug_flag */
	vectorTable[7] = bad_trap_handler;
//	_ramvec[8] = 0;					/* debug/trace */

	for (i = 9; i <= 15; i++)
		vectorTable[i] = bad_trap_handler;
	for (i = 16; i <= 62 ; i++)
		vectorTable[i] = real_irq_entry;

	vectorTable [6] = binfmt_flat_debug_handler;
	vectorTable[63] = system_call;

	vectorTable[na_timer0_irq] = timer_handler;
#ifdef na_ide_interface_irq
	vectorTable[na_ide_interface_irq] = ocidec_handler;
#endif

/* Vic - end of RAM vector table inits */

	for (i = 0; i < SYS_IRQS; i++) {
		irq_list[i].handler = NULL;
		irq_list[i].flags   = IRQ_FLG_STD;
		irq_list[i].dev_id  = NULL;
		irq_list[i].devname = default_names[i];
	}

	for (i = 0; i < NUM_IRQ_NODES; i++)
		nodes[i].handler = NULL;
#ifdef DEBUG
	printk("init_IRQ done\n");
#endif
}

irq_node_t *new_irq_node(void)
{
	irq_node_t *node;
	short i;

	for (node = nodes, i = NUM_IRQ_NODES-1; i >= 0; node++, i--)
		if (!node->handler)
			return node;

	printk ("new_irq_node: out of nodes\n");
	return NULL;
}

int request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *),
                unsigned long flags, const char *devname, void *dev_id)
{
	if (irq < IRQMIN || irq > IRQMAX) {
		printk("%s: Incorrect IRQ %d from %s\n", __FUNCTION__, irq, devname);
		return -ENXIO;
	}

	if (!(irq_list[irq].flags & IRQ_FLG_STD)) {
		if (irq_list[irq].flags & IRQ_FLG_LOCK) {
			printk("%s: IRQ %d from %s is not replaceable\n",
			       __FUNCTION__, irq, irq_list[irq].devname);
			return -EBUSY;
		}
		if (flags & IRQ_FLG_REPLACE) {
			printk("%s: %s can't replace IRQ %d from %s\n",
			       __FUNCTION__, devname, irq, irq_list[irq].devname);
			return -EBUSY;
		}
	}
	irq_list[irq].handler = handler;
	irq_list[irq].flags   = flags;
	irq_list[irq].dev_id  = dev_id;
	irq_list[irq].devname = devname;
	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
	if (irq < IRQMIN || irq > IRQMAX) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	if (irq_list[irq].dev_id != dev_id)
		printk("%s: Removing probably wrong IRQ %d from %s\n",
		       __FUNCTION__, irq, irq_list[irq].devname);

	irq_list[irq].handler = NULL;
	irq_list[irq].flags   = IRQ_FLG_STD;
	irq_list[irq].dev_id  = NULL;
	irq_list[irq].devname = default_names[irq];
}

/* usually not useful in embedded systems */
unsigned long probe_irq_on (void)
{
	return 0;
}

int probe_irq_off (unsigned long irqs)
{
	return 0;
}

void enable_irq(unsigned int irq)
{
}

void disable_irq(unsigned int irq)
{
}

asmlinkage void process_int(unsigned long vec, struct pt_regs *fp)
{
	/* give the machine specific code a crack at it first */
	irq_enter(0, irq);
	kstat.irqs[0][vec]++;
	if (irq_list[vec].handler) {
		irq_list[vec].handler(vec, irq_list[vec].dev_id, fp);
	} else
#ifdef DEBUG
		{
		printk("No interrupt handler for level %ld\n", vec);
////		asm("trap 5");
		}
#else
  #if 1
		printk("Ignoring interrupt %ld: no handler\n", vec);
  #else
		panic("No interrupt handler for level %ld\n", vec);
  #endif
#endif

	irq_exit(0, irq);
	if (softirq_pending(0))
		do_softirq();

}

int get_irq_list(char *buf)
{
	int i, len = 0;

	/* autovector interrupts */
	for (i = 0; i < SYS_IRQS; i++) {
		if (irq_list[i].handler) {
			len += sprintf(buf+len, "auto %2d: %10u ", i,
			               i ? kstat.irqs[0][i] : num_spurious);
			if (irq_list[i].flags & IRQ_FLG_LOCK)
				len += sprintf(buf+len, "L ");
			else
				len += sprintf(buf+len, "  ");
			len += sprintf(buf+len, "%s\n", irq_list[i].devname);
		}
	}
	return len;
}
