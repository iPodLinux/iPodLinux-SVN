/*
 * keyboard.c - keyboard driver for iPod
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/mm.h>
#include <linux/kbd_kern.h>
#include <asm/io.h>
#include <asm/arch/irqs.h>
#include <asm/keyboard.h>

static void keyboard_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
#if 1
	static int left = 0, scrolling = 0;
#endif
	unsigned char source, state;

	/* get source of interupts */
	source = inb(0xcf000040);

	/* get current keypad status */
	state = inb(0xcf000030);
	outb(~state, 0xcf000060);

/* we use the keycodes and translation is 1 to 1 */
#define UP_SC		103
#define LEFT_SC		105
#define RIGHT_SC	106
#define DOWN_SC		108

#define ACTION_SC	28

#ifdef CONFIG_VT
	kbd_pt_regs = regs;

	if ( source & 0x1 ) {
		if ( state & 0x1 ) {
#if 0
			//printk("ff up\n");
			handle_scancode(RIGHT_SC, 0);
#else
scrollfront(0);
#endif
		}
		else {
#if 0
			//printk("ff down\n");
			handle_scancode(RIGHT_SC, 1);
#endif
		}
	}
	if ( source & 0x2 ) {
		if ( state & 0x2 ) {
			//printk("action up\n");
			handle_scancode(ACTION_SC, 0);
		}
		else {
			//printk("action down\n");
			handle_scancode(ACTION_SC, 1);
		}
	}

#define LEFT_CTRL_SC	29
#define Q_SC		16
#define S_SC		31

	if ( source & 0x4 ) {
		if ( state & 0x4 ) {
#if 0
			//printk("pause up\n");
			handle_scancode(DOWN_SC, 0);
#else
			// simulate CTRL-Q up
			handle_scancode(Q_SC, 0);
			handle_scancode(LEFT_CTRL_SC, 0);
#endif
		}
		else {
#if 0
			//printk("pause down\n");
			handle_scancode(DOWN_SC, 1);
#else
			// simulate CTRL-Q down
			handle_scancode(LEFT_CTRL_SC, 1);
			handle_scancode(Q_SC, 1);
#endif
		}
	}
	if ( source & 0x8 ) {
		if ( state & 0x8 ) {
#if 0
			//printk("rr up\n");
			handle_scancode(LEFT_SC, 0);
#else
scrollback(0);
#endif
		}
		else {
#if 0
			//printk("rr down\n");
			handle_scancode(LEFT_SC, 1);
#endif
		}
	}
	if ( source & 0x10 ) {
		if ( state & 0x10 ) {
#if 0
			//printk("menu up\n");
			handle_scancode(LEFT_SC, 0);
#else
			/* CTRL-S up */
			handle_scancode(S_SC, 0);
			handle_scancode(LEFT_CTRL_SC, 0);
#endif
		}
		else {
#if 0
			//printk("menu down\n");
			handle_scancode(LEFT_SC, 1);
#else
			/* CTRL-S down */
			handle_scancode(LEFT_CTRL_SC, 1);
			handle_scancode(S_SC, 1);
#endif
		}
	}
	if ( source & 0x20 ) {
		if ( state & 0x20 ) {
			//printk("menu up\n");
			handle_scancode(UP_SC, 0);
		}
		else {
			//printk("menu down\n");
			handle_scancode(UP_SC, 1);
		}
	}
#endif

#if 0
	if ( (source & 0xc0) == 0xc0 ) {
		scrolling = 1;
		if ( left ) {
			printk("left\n");
		}
		else {
			printk("right\n");
		}
	}
	else if ( source & 0x40 ) {
		left = 1;
		printk("left\n");
	}
	else if ( source & 0x80 ) {
		left = 0;
		printk("right\n");
	}
#endif

#ifdef CONFIG_VT
	tasklet_schedule(&keyboard_tasklet);
#endif

	/* ack any active interrupts */
	outb(source, 0xcf000070);
}

void __init ipodkb_init_hw(void)
{
	outb(~inb(0xcf000030), 0xcf000060);
	outb(inb(0xcf000040), 0xcf000070);

	outb(inb(0xcf000004) | 0x1, 0xcf000004);
	outb(inb(0xcf000014) | 0x1, 0xcf000014);
	outb(inb(0xcf000024) | 0x1, 0xcf000024);

	if ( request_irq(GPIO_IRQ, keyboard_interrupt, 0, "keyboard", NULL) ) {
		printk("ipodkb: IRQ %d failed\n", GPIO_IRQ);
	}

	outb(0xff, 0xcf000050);
}

