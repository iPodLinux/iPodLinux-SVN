/*
 * keyboard.c - keyboard driver for iPod
 *
 * Copyright (c) 2003-2005 Bernard Leach (leachbj@bouncycastle.org)
 * 
 * 2005-04-08 4g/photo patched by Niccolo' Contessa <sonictooth@gmail.com>
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
	unsigned char source, wheel_source, state, wheel_state = 0;

	/*
	 * we need some delay for mini, cause hold generates several interrupts,
	 * some of them delayed
	 */
	udelay(250);

	/* get source(s) of interupt */
	source = inb(0x6000d040) & 0x3f;
	if (ipod_hw_ver == 0x4) {
		wheel_source = inb(0x6000d044) & 0x30;
	}
	else {
		source &= 0x20;
		wheel_source = 0x0;
	}

	if (source == 0 && wheel_source == 0) {
		return; 	/* not for us */
	}

	/* get current keypad & wheel status */
	state = inb(0x6000d030) & 0x3f;
	if (ipod_hw_ver == 0x4) {
		wheel_state = inb(0x6000d034) & 0x30;
	}
	else {
		state &= 0x20;
	}

	/* toggle interrupt level */
	outb(~state, 0x6000d060);
	if (ipod_hw_ver == 0x4) {
		outb(~wheel_state, 0x6000d064);
	}

#ifdef CONFIG_VT
	kbd_pt_regs = regs;

	if (source & 0x20) {
		/* mini hold switch is active low */
		if (state & 0x20) {
			handle_scancode(HOLD_SC, 0);
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
		handle_scroll_wheel((wheel_state & 0x30) >> 4, 0, 1);
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

static void opto_i2c_init(void)
{
	int i, curr_value;

	/* wait for value to settle */
	i = 1000;
	curr_value = (inl(0x7000c104) << 16) >> 24;
	while (i > 0)
	{
		int new_value = (inl(0x7000c104) << 16) >> 24;

		if (new_value != curr_value) {
			i = 10000;
			curr_value = new_value;
		}
		else {
			i--;
		}
	}

	outl(inl(0x6000d024) | 0x10, 0x6000d024);	/* port B bit 4 = 1 */

	outl(inl(0x6000600c) | 0x10000, 0x6000600c);	/* dev enable */
	outl(inl(0x60006004) | 0x10000, 0x60006004);	/* dev reset */
	udelay(5);
	outl(inl(0x60006004) & ~0x10000, 0x60006004);	/* dev reset finish */

	outl(0xffffffff, 0x7000c120);
	outl(0xffffffff, 0x7000c124);
	outl(0xc00a1f00, 0x7000c100);
	outl(0x1000000, 0x7000c104);
}

static void key_i2c_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned reg, status;
	static int button_mask = 0;
	static int wheel_bits16_22 = -1;
	static int wheel_events = 0;
	int wheel_delta = 0;
	int wheel_delta_abs = 0;
	int wheel_value = 0;
	int wheel_keycode = 0;
	int wheel_keypresses = 0;

	udelay(250);

	reg = 0x7000c104;

	if ((inl(0x7000c104) & 0x4000000) != 0) {
		reg = reg + 0x3C;	/* 0x7000c140 */

		status = inl(0x7000c140);
		outl(0x0, 0x7000c140);	/* clear interrupt status? */

		if ((status & 0x800000ff) == 0x8000001a) {
			int new_button_mask = 0;
			int new_wheel_value = 0;

			/* NB: highest wheel = 0x5F, clockwise increases */
			new_wheel_value = ((status << 9) >> 25) & 0xff;
			
			if ((status & 0x100) != 0) {
				new_button_mask |= 0x1;	/* Action */
				if (!(button_mask & 0x1)) {
					handle_scancode(ACTION_SC, 1);
				}
			}
			else if (button_mask & 0x1) {
				handle_scancode(ACTION_SC, 0);
			}

			if ((status & 0x1000) != 0) {
				new_button_mask |= 0x10;	/* Menu */
				if (!(button_mask & 0x10)) {
					handle_scancode(UP_SC, 1);
				}
			}
			else if (button_mask & 0x10) {
				handle_scancode(UP_SC, 0);
			}

			if ((status & 0x800) != 0) {
				new_button_mask |= 0x8;	/* Play/Pause */
				if (!(button_mask & 0x8)) {
					handle_scancode(DOWN_SC, 1);
				}
			}
			else if (button_mask & 0x8) {
				handle_scancode(DOWN_SC, 0);
			}

			if ((status & 0x200) != 0) {
				new_button_mask |= 0x2;	/* Next */
				if (!(button_mask & 0x2)) {
					handle_scancode(RIGHT_SC, 1);
				}
			}
			else if (button_mask & 0x2) {
				handle_scancode(RIGHT_SC, 0);
			}

			if ((status & 0x400) != 0) {
				new_button_mask |= 0x4;	/* Prev */
				if (!(button_mask & 0x4)) {
					handle_scancode(LEFT_SC, 1);
				}
			}
			else if (button_mask & 0x4) {
				handle_scancode(LEFT_SC, 0);
			}

			if ((status & 0x40000000) != 0) {
				/* scroll wheel down */
				new_button_mask |= 0x20;

				if (wheel_bits16_22 != -1) {
					wheel_delta = new_wheel_value - wheel_bits16_22;
					wheel_delta_abs = wheel_delta < 0 ? -wheel_delta : wheel_delta;

					if (wheel_delta_abs > 48) {
						if (wheel_bits16_22 > new_wheel_value) { 
							wheel_bits16_22 -= 96;
						}
						else { 
							wheel_bits16_22 += 96;
						}
					}

					wheel_delta = new_wheel_value - wheel_bits16_22;

					if (wheel_delta > 0 && wheel_delta < 4) {
						wheel_keycode = R_SC;
						wheel_keypresses = wheel_delta;
					}
					else if (wheel_delta < 0 && wheel_delta > -4) {
						wheel_keycode = L_SC;
						wheel_keypresses = -wheel_delta;
					}

					wheel_events += wheel_keypresses;

					if (wheel_events > 2) {
						while (wheel_keypresses--) {
							handle_scancode(wheel_keycode, 1);
							handle_scancode(wheel_keycode, 0);
						}

						wheel_bits16_22 = new_wheel_value;
					}
				}
				else {
					wheel_bits16_22 = new_wheel_value;
				}
			}
			else if (button_mask & 0x20) {
				/* scroll wheel up */
				wheel_bits16_22 = -1;
				wheel_events = 0;
			}

			button_mask = new_button_mask;

#ifdef CONFIG_VT
			tasklet_schedule(&keyboard_tasklet);
#endif /* CONFIG_VT */
		}
		else if ((status & 0x800000FF) == 0x8000003A) {
			wheel_value = status & 0x800000FF;
		}
		else if (status == 0xffffffff) {
			opto_i2c_init();
		}
	}

	if ((inl(reg) & 0x8000000) != 0) {
		outl(0xffffffff, 0x7000c120);
		outl(0xffffffff, 0x7000c124);
	}

	outl(inl(0x7000c104) | 0xC000000, 0x7000c104);
	outl(0x400a1f00, 0x7000c100);

	outl(inl(0x6000d024) | 0x10, 0x6000d024);	/* port B bit 4 = 1 */
}

void __init ipodkb_init_hw(void)
{
	/* get our hardware type */
	ipod_hw_ver = ipod_get_hw_version() >> 16;

	if (ipod_hw_ver == 0x4) {
		/* mini */

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
		ipod_i2c_init();

		opto_i2c_init();

		if (request_irq(PP5020_I2C_IRQ, key_i2c_interrupt, SA_SHIRQ, "keyboard", KEYBOARD_DEV_ID)) {
			printk("ipodkb: IRQ %d failed\n", PP5020_I2C_IRQ);
		}

		/* hold button - enable as input */
		outl(inl(0x6000d000) | 0x20, 0x6000d000);
		outl(inl(0x6000d010) & ~0x20, 0x6000d010);

		/* hold button - set interrupt levels */
		outl(~(inl(0x6000d030) & 0x20), 0x6000d060);
		outl((inl(0x6000d040) & 0x20), 0x6000d070);

		if (request_irq(PP5020_GPIO_IRQ, key_mini_interrupt, SA_SHIRQ, "keyboard", KEYBOARD_DEV_ID)) {
			printk("ipodkb: IRQ %d failed\n", PP5020_GPIO_IRQ);
		}

		/* enable interrupts */
		outl(0x20, 0x6000d050);
	}
	else {
		/* 1g, 2g, 3g */
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

