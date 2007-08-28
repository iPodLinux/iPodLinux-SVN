/*
 * linux/arch/h8300/platform/h8300h/ints.c
 *
 * Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * Based on linux/arch/$(ARCH)/platform/$(PLATFORM)/ints.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Copyright 1996 Roman Zippel
 * Copyright 1999 D. Jeff Dionne <jeff@uClinux.org>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/bootmem.h>
#include <linux/random.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/gpio.h>
#include <asm/hardirq.h>
#include <asm/regs306x.h>

/* table for system interrupt handlers */
static irq_handler_t *irq_list[NR_IRQS];
static int use_kmalloc;

extern unsigned long *interrupt_redirect_table;

#define CPU_VECTOR ((unsigned long *)0x000000)
#define ADDR_MASK (0xffffff)

static inline unsigned long *get_vector_address(void)
{
	unsigned long *rom_vector = CPU_VECTOR;
	unsigned long base,tmp;
	int vec_no;

	base = rom_vector[EXT_IRQ0] & ADDR_MASK;
	
	/* check romvector format */
	for (vec_no = EXT_IRQ1; vec_no <= EXT_IRQ5; vec_no++) {
		if ((base+(vec_no - EXT_IRQ0)*4) != (rom_vector[vec_no] & ADDR_MASK))
			return NULL;
	}

	/* ramvector base address */
	base -= EXT_IRQ0*4;

	/* writerble check */
	tmp = ~(*(volatile unsigned long *)base);
	(*(volatile unsigned long *)base) = tmp;
	if ((*(volatile unsigned long *)base) != tmp)
		return NULL;
	return (unsigned long *)base;
}

void __init init_IRQ(void)
{
#if defined(CONFIG_RAMKERNEL)
	int i;
	unsigned long *ramvec,*ramvec_p;
	unsigned long break_vec;

	ramvec = get_vector_address();
	if (ramvec == NULL)
		panic("interrupt vector serup failed.");
	else
		printk("virtual vector at 0x%08lx\n",(unsigned long)ramvec);

#if defined(CONFIG_GDB_DEBUG)
	/* save orignal break vector */
	break_vec = ramvec[TRAP3_VEC];
#else
	break_vec = VECTOR(trace_break);
#endif

	/* create redirect table */
	for (ramvec_p = ramvec, i = 0; i < NR_IRQS; i++)
		*ramvec_p++ = REDIRECT(interrupt_entry);

	/* set special vector */
	ramvec[TRAP0_VEC] = VECTOR(system_call);
	ramvec[TRAP3_VEC] = break_vec;
	interrupt_redirect_table = ramvec;
#ifdef DUMP_VECTOR
	ramvec_p = ramvec;
	for (i = 0; i < NR_IRQS; i++) {
		if ((i % 8) == 0)
			printk("\n%p: ",ramvec_p);
		printk("%p ",*ramvec_p);
		ramvec_p++;
	}
	printk("\n");
#endif
#endif
}

int request_irq(unsigned int irq, 
		void (*handler)(int, void *, struct pt_regs *),
                unsigned long flags, const char *devname, void *dev_id)
{
	irq_handler_t *irq_handle;
	int bitmask;
	if (irq < 0 || irq >= NR_IRQS) {
		printk("Incorrect IRQ %d from %s\n", irq, devname);
		return -EINVAL;
	}
	if (irq_list[irq])
		return -EBUSY;  /* already used */
	/* initialize IRQ pin */
	bitmask = 1 << (irq - EXT_IRQ0);
	switch(irq) {
	case EXT_IRQ0:
	case EXT_IRQ1:
	case EXT_IRQ2:
	case EXT_IRQ3:
		if (H8300_GPIO_RESERVE(H8300_GPIO_P8, bitmask) == 0)
			return -EBUSY;
		H8300_GPIO_DDR(H8300_GPIO_P8, bitmask, H8300_GPIO_INPUT);
		break;
	case EXT_IRQ4:
	case EXT_IRQ5:
		if (H8300_GPIO_RESERVE(H8300_GPIO_P9, bitmask) == 0)
			return -EBUSY;
		H8300_GPIO_DDR(H8300_GPIO_P9, bitmask, H8300_GPIO_INPUT);
		break;
	}

	if(use_kmalloc)
		irq_handle = (irq_handler_t *)kmalloc(sizeof(irq_handler_t), GFP_ATOMIC);
	else {
		irq_handle = (irq_handler_t *)alloc_bootmem(sizeof(irq_handler_t));
		(unsigned long)irq_handle |= 0x80000000UL;
	}

	if (irq_handle == NULL)
		return -ENOMEM;

	irq_handle->handler = handler;
	irq_handle->flags   = flags;
	irq_handle->count   = 0;
	irq_handle->dev_id  = dev_id;
	irq_handle->devname = devname;
	irq_list[irq] = irq_handle;
	if (irq_handle->flags & SA_SAMPLE_RANDOM)
		rand_initialize_irq(irq);

	/* enable interrupt */
	/* compatible i386  */
	if (irq >= EXT_IRQ0 && irq <= EXT_IRQ5)
		*(volatile unsigned char *)IER |= bitmask;
	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
	int bitmask;
	if (irq >= NR_IRQS) {
		return;
	}
	if (!irq_list[irq] || irq_list[irq]->dev_id != dev_id)
		printk("Removing probably wrong IRQ %d from %s\n",
		       irq, irq_list[irq]->devname);
	/* disable interrupt & release IRQ pin */
	bitmask = 1 << (irq - EXT_IRQ0);
	switch(irq) {
	case EXT_IRQ0:
	case EXT_IRQ1:
	case EXT_IRQ2:
	case EXT_IRQ3:
		*(volatile unsigned char *)IER &= ~bitmask;
		H8300_GPIO_FREE(H8300_GPIO_P8, bitmask);
		break ;
	case EXT_IRQ4:
	case EXT_IRQ5:
		*(volatile unsigned char *)IER &= ~bitmask;
		H8300_GPIO_FREE(H8300_GPIO_P9, bitmask);
		break;
	}
	if (((unsigned long)irq_list[irq] & 0x80000000UL) == 0) {
		kfree(irq_list[irq]);
		irq_list[irq] = NULL;
	}
}

/*
 * Do we need these probe functions on the m68k?
 */
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
	if (irq >= EXT_IRQ0 && irq <= EXT_IRQ5) {
		*(volatile unsigned char *)IER |= (1 << (irq - EXT_IRQ0));
		*(volatile unsigned char *)ISR &= ~(1 << (irq - EXT_IRQ0));
	}
}

void disable_irq(unsigned int irq)
{
	if (irq >= EXT_IRQ0 && irq <= EXT_IRQ5) {
		*(volatile unsigned char *)IER &= ~(1 << (irq - EXT_IRQ0));
	}
}

asmlinkage void process_int(int vec, struct pt_regs *fp)
{
	irq_enter(0,0);
	/* ISR clear       */
	/* compatible i386 */
	if (vec >= EXT_IRQ0 && vec <= EXT_IRQ5)
		*(volatile unsigned char *)ISR &= ~(1 << (vec - EXT_IRQ0));
	if (vec < NR_IRQS) {
		if (irq_list[vec]) {
			irq_list[vec]->handler(vec, irq_list[vec]->dev_id, fp);
			irq_list[vec]->count++;
			if (irq_list[vec]->flags & SA_SAMPLE_RANDOM)
				add_interrupt_randomness(vec);
		}
	} else {
		BUG();
	}
	irq_exit(0,0);
}

int get_irq_list(char *buf)
{
	int i, len = 0;

	for (i = 0; i < NR_IRQS; i++) {
		if (irq_list[i]) {
			len += sprintf(buf+len, "irq %2d: %10u ", i,
			               irq_list[i]->count);
			len += sprintf(buf+len, "%s\n", irq_list[i]->devname);
		}
	}
	return len;
}

void init_irq_proc(void)
{
}

static int __init enable_kmalloc(void)
{
	use_kmalloc = 1;
	return 0;
}
__initcall(enable_kmalloc);
