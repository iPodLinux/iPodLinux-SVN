/*
 * keyboard.c - keyboard driver for iPod
 *
 * Copyright (c) 2003-2005 Bernard Leach (leachbj@bouncycastle.org)
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
#include <linux/delay.h>
#include <linux/input.h>
#include <asm/io.h>
#include <asm/arch/irqs.h>
#include <asm/keyboard.h>
#include <asm/hardware.h>

/* undefine these to produce keycodes from left/right/up/down */
#undef DO_SCROLLBACK
#undef DO_CONTRAST

/* we use the keycodes and translation is 1 to 1 */
#define R_SC		KEY_R
#define L_SC		KEY_L

#define UP_SC		KEY_M
#define LEFT_SC		KEY_W
#define RIGHT_SC	KEY_F
#define DOWN_SC		KEY_D

#define HOLD_SC		KEY_H
#define ACTION_SC	KEY_ENTER

/* need to pass something becuase we use a shared irq */
#define KEYBOARD_DEV_ID	(void *)0x4b455942

static unsigned ipod_hw_ver;

static void handle_scroll_wheel(int new_scroll, int was_hold, int reverse)
{
	static int prev_scroll = -1;
	static int scroll_state[4][4] = {
		{0, 1, -1, 0},
		{-1, 0, 0, 1},
		{1, 0, 0, -1},
		{0, -1, 1, 0}
	};

	if ( prev_scroll == -1 ) {
		prev_scroll = new_scroll;
	}
	else if (!was_hold) {
		switch (scroll_state[prev_scroll][new_scroll]) {
		case 1:
			if (reverse) {
				/* 'r' keypress */
				handle_scancode(R_SC, 1);
				handle_scancode(R_SC, 0);
			}
			else {
				/* 'l' keypress */
				handle_scancode(L_SC, 1);
				handle_scancode(L_SC, 0);
			}
			break;
		case -1:
			if (reverse) {
				/* 'l' keypress */
				handle_scancode(L_SC, 1);
				handle_scancode(L_SC, 0);
			}
			else {
				/* 'r' keypress */
				handle_scancode(R_SC, 1);
				handle_scancode(R_SC, 0);
			}
			break;
		default:
			/* only happens if we get out of sync */
			break;
		}
	}

	prev_scroll = new_scroll;
}

static void keyboard_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char source, state;
	static int was_hold = 0;

	/*
	 * we need some delay for g3, cause hold generates several interrupts,
	 * some of them delayed
	 */
	if (ipod_hw_ver == 0x3) {
		udelay(250);
	}

	/* get source of interupts */
	source = inb(0xcf000040);
	if (source == 0) {
		return; 	/* not for us */
	}

	/* get current keypad status */
	state = inb(0xcf000030);
	outb(~state, 0xcf000060);

	if (ipod_hw_ver == 0x3) {
		if (was_hold && source == 0x40 && state == 0xbf) {
			/* ack any active interrupts */
			outb(source, 0xcf000070);
			return;
		}
		was_hold = 0;
	}

#ifdef CONFIG_VT
	kbd_pt_regs = regs;

	if ( source & 0x20 ) {
		if (ipod_hw_ver == 0x3) {
			/* 3g hold switch is active low */
			if (state & 0x20) {
				handle_scancode(HOLD_SC, 0);
				was_hold = 1;
			}
			else {
				handle_scancode(HOLD_SC, 1);
			}

			/* hold switch on 3g causes all outputs to go low */
			/* we shouldn't interpret these as key presses */
			goto done;
		}
		else {
			if (state & 0x20) {
				handle_scancode(HOLD_SC, 1);
				was_hold = 1;
			}
			else {
				handle_scancode(HOLD_SC, 0);
				was_hold = 0;
			}
		}
	}

	if (source & 0x1) {
		if (state & 0x1) {
#if defined(DO_SCROLLBACK)
			scrollfront(0);
#else
			handle_scancode(RIGHT_SC, 0);
#endif
		}
#if !defined(DO_SCROLLBACK)
		else {
			handle_scancode(RIGHT_SC, 1);
		}
#endif
	}
	if (source & 0x2) {
		if (state & 0x2) {
			handle_scancode(ACTION_SC, 0);
		}
		else {
			handle_scancode(ACTION_SC, 1);
		}
	}
	if (source & 0x4) {
		if (state & 0x4) {
#if defined(DO_CONTRAST)
			contrast_down();
#else
			handle_scancode(DOWN_SC, 0);
#endif
		}
#if !defined(DO_CONTRAST)
		else {
			handle_scancode(DOWN_SC, 1);
		}
#endif
	}
	if (source & 0x8) {
		if (state & 0x8) {
#if defined(DO_SCROLLBACK)
			scrollback(0);
#else
			handle_scancode(LEFT_SC, 0);
#endif
		}
#if !defined(DO_SCROLLBACK)
		else {
			handle_scancode(LEFT_SC, 1);
		}
#endif
	}
	if (source & 0x10) {
		if (state & 0x10) {
#if defined(DO_CONTRAST)
			contrast_up();
#else
			handle_scancode(UP_SC, 0);
#endif
		}
#if !defined(DO_CONTRAST)
		else {
			handle_scancode(UP_SC, 1);
		}
#endif
	}

	if (source & 0xc0) {
		handle_scroll_wheel((state & 0xc0) >> 6, was_hold, 0);
	}
done:

	tasklet_schedule(&keyboard_tasklet);
#endif /* CONFIG_VT */

	/* ack any active interrupts */
	outb(source, 0xcf000070);
}

static void key_mini_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char source, wheel_source, state, wheel_state;
	static int was_hold = 0;

	/*
	 * we need some delay for mini, cause hold generates several interrupts,
	 * some of them delayed
	 */
	udelay(250);

	/* get source(s) of interupt */
	source = inb(0x6000d040) & 0x3f;
	wheel_source = inb(0x6000d044) & 0x30;

	if (source == 0 && wheel_source == 0) {
		return; 	/* not for us */
	}

	/* get current keypad & wheel status */
	state = inb(0x6000d030) & 0x3f;
	wheel_state = inb(0x6000d034) & 0x30;

	/* toggle interrupt level */
	outb(~state, 0x6000d060);
	outb(~wheel_state, 0x6000d064);

	/* TODO need to review */
	if (was_hold && source == 0x40 && state == 0xbf) {
		/* ack any active interrupts */
		outb(source, 0x6000d070);
		return;
	}
	was_hold = 0;

#ifdef CONFIG_VT
	kbd_pt_regs = regs;

	if ( source & 0x20 ) {
		/* mini hold switch is active low */
		if (state & 0x20) {
			handle_scancode(HOLD_SC, 0);
			was_hold = 1;
		}
		else {
			handle_scancode(HOLD_SC, 1);
		}

		/* hold switch on mini causes all outputs to go low */
		/* we shouldn't interpret these as key presses */
		goto done;
	}

	if (source & 0x1) {
		if (state & 0x1) {
			handle_scancode(ACTION_SC, 0);
		}
		else {
			handle_scancode(ACTION_SC, 1);
		}
	}
	if (source & 0x2) {
		if (state & 0x2) {
			handle_scancode(UP_SC, 0);
		}
		else {
			handle_scancode(UP_SC, 1);
		}
	}
	if (source & 0x4) {
		if (state & 0x4) {
			handle_scancode(DOWN_SC, 0);
		}
		else {
			handle_scancode(DOWN_SC, 1);
		}
	}
	if (source & 0x8) {
		if (state & 0x8) {
			handle_scancode(RIGHT_SC, 0);
		}
		else {
			handle_scancode(RIGHT_SC, 1);
		}
	}
	if (source & 0x10) {
		if (state & 0x10) {
			handle_scancode(LEFT_SC, 0);
		}
		else {
			handle_scancode(LEFT_SC, 1);
		}
	}

	if (wheel_source & 0x30) {
		handle_scroll_wheel((wheel_state & 0x30) >> 4, was_hold, 1);
	}
done:

	tasklet_schedule(&keyboard_tasklet);
#endif /* CONFIG_VT */

	/* ack any active interrupts */
	if (source) {
		outb(source, 0x6000d070);
	}
	if (wheel_source) {
		outb(wheel_source, 0x6000d074);
	}
}

void __init ipodkb_init_hw(void)
{
	/* get our hardware type */
	ipod_hw_ver = ipod_get_hw_version() >> 16;

	if (ipod_hw_ver == 0x4) {
		/* buttons - enable as input */
		outl(inl(0x6000d000) | 0x3f, 0x6000d000);
		outl(inl(0x6000d010) & ~0x3f, 0x6000d010);

		/* scroll wheel- enable as input */
		outl(inl(0x6000d004) | 0x30, 0x6000d004); /* port b 4,5 */
		outl(inl(0x6000d014) & ~0x30, 0x6000d014); /* port b 4,5 */

		/* buttons - set interrupt levels */
		outl(~(inl(0x6000d030) & 0x3f), 0x6000d060);
		outl((inl(0x6000d040) & 0x3f), 0x6000d070);

		/* scroll wheel - set interrupt levels */
		outl(~(inl(0x6000d034) & 0x30), 0x6000d064);
		outl((inl(0x6000d044) & 0x30), 0x6000d074);

		if (request_irq(PP5020_GPIO_IRQ, key_mini_interrupt, SA_SHIRQ, "keyboard", KEYBOARD_DEV_ID)) {
			printk("ipodkb: IRQ %d failed\n", PP5020_GPIO_IRQ);
		}

		/* enable interrupts */
		outl(0x3f, 0x6000d050);
		outl(0x30, 0x6000d054);
	}
	else if (ipod_hw_ver > 0x4) {
		/* 4g and photo */
	}
	else {
		outb(~inb(0xcf000030), 0xcf000060);
		outb(inb(0xcf000040), 0xcf000070);

		if (ipod_hw_ver == 0x1) {
			outb(inb(0xcf000004) | 0x1, 0xcf000004);
			outb(inb(0xcf000014) | 0x1, 0xcf000014);
			outb(inb(0xcf000024) | 0x1, 0xcf000024);
		}

		if (request_irq(PP5002_GPIO_IRQ, keyboard_interrupt, SA_SHIRQ, "keyboard", KEYBOARD_DEV_ID)) {
			printk("ipodkb: IRQ %d failed\n", PP5002_GPIO_IRQ);
		}

		outb(0xff, 0xcf000050);
	}
}

