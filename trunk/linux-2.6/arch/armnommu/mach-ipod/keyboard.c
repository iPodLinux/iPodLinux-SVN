/*
 * keyboard.c - keyboard driver for iPod
 *
 * Copyright (c) 2003,2004 Bernard Leach <leachbj@bouncycastle.org>
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/kbd_kern.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/arch/irqs.h>
#include <asm/arch/keyboard.h>
#include <asm/hardware.h>
#include <linux/input.h>
#include <linux/vt_kern.h>

/* undefine these to produce keycodes from left/right/up/down */
#undef DO_SCROLLBACK
#undef DO_CONTRAST
#undef USE_ARROW_KEYS

/* we use the keycodes and translation is 1 to 1 */
#define R_SC		19	/* 'r' */
#define L_SC		38	/* 'l' */

#if defined(USE_ARROW_KEYS)
#define UP_SC		103
#define LEFT_SC		105
#define RIGHT_SC	106
#define DOWN_SC		108
#else
#define UP_SC		50	/* 'm' */
#define LEFT_SC		17	/* 'w' */
#define RIGHT_SC	33	/* 'f' */
#define DOWN_SC		32	/* 'd' */
#endif

#define ACTION_SC	KEY_ENTER	/* '\n' */

/* send ^S and ^Q for the hold switch */
#define LEFT_CTRL_SC	29
#define Q_SC		16
#define S_SC		31

static unsigned char ipodkbd_keycode[10] = {
	R_SC, L_SC, UP_SC, LEFT_SC, RIGHT_SC, DOWN_SC,
	ACTION_SC, LEFT_CTRL_SC, Q_SC, S_SC
};

static struct input_dev ipodkbd_dev;

static char *ipodkbd_name = "iPod Keypad";
static char *ipodkbd_phys = "ipodkbd/input0";

static unsigned ipod_hw_ver;

static int ipodkbd_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	static int prev_scroll = -1;
	unsigned char source, state;
	static int was_hold = 0;

	/* we need some delay for g3, cause hold generates several interrupts,
	 * some of them delayed
	 */
	if (ipod_hw_ver == 0x3) {
		udelay(250);
	}

	/* get source of interupts */
	source = inb(0xcf000040);
	if ( source == 0 ) {
		return IRQ_NONE; 	/* not for us */
	}

	/* get current keypad status */
	state = inb(0xcf000030);
	outb(~state, 0xcf000060);

	if (ipod_hw_ver == 0x3) {
		if (was_hold && source == 0x40 && state == 0xbf) {
			/* ack any active interrupts */
			outb(source, 0xcf000070);
			return IRQ_HANDLED;
		}
		was_hold = 0;
	}

	input_regs(&ipodkbd_dev, regs);

	if ( source & 0x20 ) {
		if ((ipod_hw_ver == 0x3 && (state & 0x20) == 0 ) || 
				(state & 0x20)) {
			/* CTRL-S down */
			input_report_key(&ipodkbd_dev, LEFT_CTRL_SC, 1);
			input_report_key(&ipodkbd_dev, S_SC, 1);

			/* CTRL-S up */
			input_report_key(&ipodkbd_dev, S_SC, 0);
			input_report_key(&ipodkbd_dev, LEFT_CTRL_SC, 0);

			input_sync(&ipodkbd_dev);
		}
		else {
			/* CTRL-Q down */
			input_report_key(&ipodkbd_dev, LEFT_CTRL_SC, 1);
			input_report_key(&ipodkbd_dev, Q_SC, 1);

			/* CTRL-Q up */
			input_report_key(&ipodkbd_dev, Q_SC, 0);
			input_report_key(&ipodkbd_dev, LEFT_CTRL_SC, 0);

			input_sync(&ipodkbd_dev);

			if (ipod_hw_ver == 0x3) {
				was_hold = 1;
			}
		}
		/* hold switch on 3g causes all outputs to go low */
		/* we shouldn't interpret these as key presses */
		if (ipod_hw_ver == 0x3) {
			goto done;
		}
	}

	if ( source & 0x1 ) {
		if ( state & 0x1 ) {
#if defined(DO_SCROLLBACK)
			scrollfront(0);
#else
			input_report_key(&ipodkbd_dev, RIGHT_SC, 0);
#endif
		}
#if !defined(DO_SCROLLBACK)
		else {
			input_report_key(&ipodkbd_dev, RIGHT_SC, 1);
		}
#endif

		input_sync(&ipodkbd_dev);
	}
	if ( source & 0x2 ) {
		if ( state & 0x2 ) {
			input_report_key(&ipodkbd_dev, ACTION_SC, 0);
		}
		else {
			input_report_key(&ipodkbd_dev, ACTION_SC, 1);
		}

		input_sync(&ipodkbd_dev);
	}

	if ( source & 0x4 ) {
		if ( state & 0x4 ) {
#if defined(DO_CONTRAST)
			contrast_down();
#else
			input_report_key(&ipodkbd_dev, DOWN_SC, 0);
#endif
		}
#if !defined(DO_CONTRAST)
		else {
			input_report_key(&ipodkbd_dev, DOWN_SC, 1);
		}
#endif

		input_sync(&ipodkbd_dev);
	}
	if ( source & 0x8 ) {
		if ( state & 0x8 ) {
#if defined(DO_SCROLLBACK)
			scrollback(0);
#else
			input_report_key(&ipodkbd_dev, LEFT_SC, 0);
#endif
		}
#if !defined(DO_SCROLLBACK)
		else {
			input_report_key(&ipodkbd_dev, LEFT_SC, 1);
		}
#endif

		input_sync(&ipodkbd_dev);
	}
	if ( source & 0x10 ) {
		if ( state & 0x10 ) {
#if defined(DO_CONTRAST)
			contrast_up();
#else
			input_report_key(&ipodkbd_dev, UP_SC, 0);
#endif
		}
#if !defined(DO_CONTRAST)
		else {
			input_report_key(&ipodkbd_dev, UP_SC, 1);
		}
#endif

		input_sync(&ipodkbd_dev);
	}

	if ( source & 0xc0 ) {
		static int scroll_state[4][4] = {
			{0, 1, -1, 0},
			{-1, 0, 0, 1},
			{1, 0, 0, -1},
			{0, -1, 1, 0}
		};
		unsigned now_scroll = (state & 0xc0) >> 6;
						
		if ( prev_scroll == -1 ) {
			prev_scroll = now_scroll;
		}
		else {
			switch (scroll_state[prev_scroll][now_scroll]) {
			case 1:
				/* 'l' keypress */
				input_report_key(&ipodkbd_dev, L_SC, 1);
				input_report_key(&ipodkbd_dev, L_SC, 0);
				break;
			case -1:
				/* 'r' keypress */
				input_report_key(&ipodkbd_dev, R_SC, 1);
				input_report_key(&ipodkbd_dev, R_SC, 0);
				break;
			default:
				/* only happens if we get out of sync */
			}

			input_sync(&ipodkbd_dev);
		}

		prev_scroll = now_scroll;
	}
done:

	/* ack any active interrupts */
	outb(source, 0xcf000070);

	return IRQ_HANDLED;
}

static int ipodkbd_open(struct input_dev *dev)
{
	printk("ipod open\n");
	return 0;
}

static void ipodkbd_close(struct input_dev *dev)
{
	printk("ipod close\n");
}

static int __init ipodkbd_init(void)
{
	int i;

	outb(~inb(0xcf000030), 0xcf000060);
	outb(inb(0xcf000040), 0xcf000070);

	outb(inb(0xcf000004) | 0x1, 0xcf000004);
	outb(inb(0xcf000014) | 0x1, 0xcf000014);
	outb(inb(0xcf000024) | 0x1, 0xcf000024);

	if ( request_irq(GPIO_IRQ, ipodkbd_interrupt, SA_SHIRQ, "keyboard", ipodkbd_interrupt) ) {
		printk("ipodkb: IRQ %d failed\n", GPIO_IRQ);
	}

	outb(0xff, 0xcf000050);

	/* get our hardware type */
	ipod_hw_ver = ipod_get_hw_version() >> 16;

	init_input_dev(&ipodkbd_dev);

	ipodkbd_dev.evbit[0] = BIT(EV_KEY) | BIT(EV_REP);
	ipodkbd_dev.keycode = ipodkbd_keycode;
	ipodkbd_dev.keycodesize = sizeof(unsigned char);
	ipodkbd_dev.keycodemax = ARRAY_SIZE(ipodkbd_keycode);

	for (i = 0; i < 10; i++) {
		if (ipodkbd_keycode[i])
			set_bit(ipodkbd_keycode[i], ipodkbd_dev.keybit);
	}

	ipodkbd_dev.name = ipodkbd_name;
	ipodkbd_dev.phys = ipodkbd_phys;
	ipodkbd_dev.id.bustype = BUS_HOST;
	ipodkbd_dev.id.vendor = 0x0001;
	ipodkbd_dev.id.product = 0x0001;
	ipodkbd_dev.id.version = ipod_get_hw_version();

ipodkbd_dev.open = ipodkbd_open;
ipodkbd_dev.close = ipodkbd_close;

	input_register_device(&ipodkbd_dev);

	printk(KERN_ERR "input: %s\n", ipodkbd_name);

	return 0;
}

static void __exit ipodkbd_exit(void)
{
	input_unregister_device(&ipodkbd_dev);
	free_irq(GPIO_IRQ , ipodkbd_interrupt);
}

module_init(ipodkbd_init);
module_exit(ipodkbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bernard Leach <leachbj@bouncycastle.org>");
MODULE_DESCRIPTION("Framebuffer driver for iPod LCD");

