/*
 * keyboard.c - keyboard driver for iPod
 *
 * Copyright (c) 2003-2005 Bernard Leach (leachbj@bouncycastle.org)
 * 
 * 2005-04-08 4g/photo patched by Niccolo' Contessa <sonictooth@gmail.com>
 *
 * 2005-12-21 added /dev/wheel  Joshua Oreman <oremanj@gmail.com>
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
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <asm/io.h>
#include <asm/arch/irqs.h>
#include <asm/keyboard.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>

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

#define EV_SCROLL        0x1000
#define EV_TAP           0x2000
#define EV_PRESS         0x4000
#define EV_RELEASE       0x8000
#define EV_TOUCH         0x0100
#define EV_LIFT          0x0200
#define EV_MASK          0xff00

#define BTN_ACTION       0x0001
#define BTN_NEXT         0x0002
#define BTN_PREVIOUS     0x0004
#define BTN_PLAY         0x0008
#define BTN_MENU         0x0010
#define BTN_HOLD         0x0020
#define BTN_MASK         0x00ff

#define SCROLL_LEFT      0x0080
#define SCROLL_RIGHT     0x0000
#define SCROLL(dist) ((dist) < 0? (-(dist) & 0x7f) | SCROLL_LEFT : (dist) & 0x7f)
#define SCROLL_MASK      0x007f

/*
 * We really want to restrict it to less-than-0x60,
 * but that's not a power of 2 so 0x5f doesn't work as mask.
 * We'll settle for 0x7f, since the val should never get
 * above 0x5f in hardware anyway.
 */
#define TAP(loc)         ((loc) & 0x7f)
#define TOUCH(loc)       ((loc) & 0x7f)
#define LIFT(loc)        ((loc) & 0x7f)
#define TAP_MASK         0x007f
#define TOUCH_MASK       0x007f
#define LIFT_MASK        0x007f

static volatile int ikb_reading;
static volatile int ikb_opened;
static volatile unsigned short ikb_events[32];
static volatile unsigned ikb_ev_head, ikb_ev_tail;

static volatile unsigned ikb_pressed_at; /* jiffy value when user touched wheel, 0 if not touching. */
static volatile unsigned ikb_first_loc; /* location where user first touched wheel */
static volatile int ikb_current_scroll; /* current scroll distance */
static volatile unsigned ikb_buttons_pressed, ikb_buttons_pressed_new; /* mask of BTN_* values */

static DECLARE_WAIT_QUEUE_HEAD (ikb_read_wait);

static void ikb_push_event (unsigned ev) 
{
	if (!ikb_opened) return;
	
	if ((ikb_ev_head+1 == ikb_ev_tail) || (ikb_ev_head == 31 && ikb_ev_tail == 0))
		printk (KERN_ERR "dropping event %08x\n", ev);
	
	ikb_events[ikb_ev_head++] = ev;
	ikb_ev_head &= 31;
	wake_up_interruptible (&ikb_read_wait);
}

/* Turn the counter into a scroll event. */
static void ikb_make_scroll_event (void)
{
	if (ikb_current_scroll) {
		ikb_push_event (EV_SCROLL | SCROLL(ikb_current_scroll));
		ikb_current_scroll = 0;
	}
}

static void ikb_scroll (int dir) 
{
	ikb_current_scroll += dir;

	while (dir > 0) {
		handle_scancode (R_SC, 1);
		handle_scancode (R_SC, 0);
		dir--;
	}

	while (dir < 0) {
		handle_scancode (L_SC, 1);
		handle_scancode (L_SC, 0);
		dir++;
	}

	if (ikb_reading)
		ikb_make_scroll_event();
}

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
				ikb_scroll (1);
			}
			else {
				/* 'l' keypress */
				ikb_scroll (-1);
			}
			break;
		case -1:
			if (reverse) {
				/* 'l' keypress */
				ikb_scroll (-1);
			}
			else {
				/* 'r' keypress */
				ikb_scroll (1);
			}
			break;
		default:
			/* only happens if we get out of sync */
			break;
		}
	}

	prev_scroll = new_scroll;
}

static void ikb_handle_button (int button, int press) 
{
	int sc;

	if (press)
		ikb_buttons_pressed_new |= button;
	else
		ikb_buttons_pressed_new &= ~button;
	
	/* Send the code to the TTY driver too */
	switch (button) {
	case BTN_ACTION:   sc = ACTION_SC; break;
	case BTN_PREVIOUS: sc = LEFT_SC;   break;
	case BTN_NEXT:     sc = RIGHT_SC;  break;
	case BTN_MENU:     sc = UP_SC;     break;
	case BTN_PLAY:     sc = DOWN_SC;   break;
	case BTN_HOLD:     sc = HOLD_SC;   break;
	default:           sc = 0;         break;
	}

	if (sc)
		handle_scancode (sc, press);
}

static void ikb_start_buttons (void)
{
	ikb_buttons_pressed_new = ikb_buttons_pressed;
}

static void ikb_finish_buttons (void)
{
	/* Pressed: */
	if (ikb_buttons_pressed_new & ~ikb_buttons_pressed) {
		ikb_push_event (EV_PRESS | (ikb_buttons_pressed_new & ~ikb_buttons_pressed));
	}
	/* Released: */
	if (ikb_buttons_pressed & ~ikb_buttons_pressed_new) {
		ikb_push_event (EV_RELEASE | (ikb_buttons_pressed & ~ikb_buttons_pressed_new));
	}
	
	ikb_buttons_pressed = ikb_buttons_pressed_new;
}

static void keyboard_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char source, state;
	static int was_hold = 0;

	ikb_start_buttons();

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
				ikb_handle_button (BTN_HOLD, 0);
				was_hold = 1;
			}
			else {
				ikb_handle_button (BTN_HOLD, 1);
			}

			/* hold switch on 3g causes all outputs to go low */
			/* we shouldn't interpret these as key presses */
			goto done;
		}
		else {
			if (state & 0x20) {
				ikb_handle_button (BTN_HOLD, 1);
				was_hold = 1;
			}
			else {
				ikb_handle_button (BTN_HOLD, 0);
				was_hold = 0;
			}
		}
	}

	if (source & 0x1) {
		if (state & 0x1) {
#if defined(DO_SCROLLBACK)
			scrollfront(0);
#else
			ikb_handle_button (BTN_NEXT, 0);
#endif
		}
#if !defined(DO_SCROLLBACK)
		else {
			ikb_handle_button (BTN_NEXT, 1);
		}
#endif
	}
	if (source & 0x2) {
		if (state & 0x2) {
			ikb_handle_button (BTN_ACTION, 0);
		}
		else {
			ikb_handle_button (BTN_ACTION, 1);
		}
	}
	if (source & 0x4) {
		if (state & 0x4) {
#if defined(DO_CONTRAST)
			contrast_down();
#else
			ikb_handle_button (BTN_PLAY, 0);
#endif
		}
#if !defined(DO_CONTRAST)
		else {
			ikb_handle_button (BTN_PLAY, 1);
		}
#endif
	}
	if (source & 0x8) {
		if (state & 0x8) {
#if defined(DO_SCROLLBACK)
			scrollback(0);
#else
			ikb_handle_button (BTN_PREVIOUS, 0);
#endif
		}
#if !defined(DO_SCROLLBACK)
		else {
			ikb_handle_button (BTN_PREVIOUS, 1);
		}
#endif
	}
	if (source & 0x10) {
		if (state & 0x10) {
#if defined(DO_CONTRAST)
			contrast_up();
#else
			ikb_handle_button (BTN_MENU, 0);
#endif
		}
#if !defined(DO_CONTRAST)
		else {
			ikb_handle_button (BTN_MENU, 1);
		}
#endif
	}

	if (source & 0xc0) {
		handle_scroll_wheel((state & 0xc0) >> 6, was_hold, 0);
	}
done:

	tasklet_schedule(&keyboard_tasklet);
#endif /* CONFIG_VT */

	ikb_finish_buttons();

	/* ack any active interrupts */
	outb(source, 0xcf000070);
}

static void key_mini_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char source, wheel_source, state, wheel_state = 0;

	ikb_start_buttons();

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
			ikb_handle_button (BTN_HOLD, 0);
		}
		else {
			ikb_handle_button (BTN_HOLD, 1);
		}

		/* hold switch on mini causes all outputs to go low */
		/* we shouldn't interpret these as key presses */
		goto done;
	}

	if (source & 0x1) {
		if (state & 0x1) {
			ikb_handle_button (BTN_ACTION, 0);
		}
		else {
			ikb_handle_button (BTN_ACTION, 1);
		}
	}
	if (source & 0x2) {
		if (state & 0x2) {
			ikb_handle_button (BTN_MENU, 0);
		}
		else {
			ikb_handle_button (BTN_MENU, 1);
		}
	}
	if (source & 0x4) {
		if (state & 0x4) {
			ikb_handle_button (BTN_PLAY, 0);
		}
		else {
			ikb_handle_button (BTN_PLAY, 1);
		}
	}
	if (source & 0x8) {
		if (state & 0x8) {
			ikb_handle_button (BTN_NEXT, 0);
		}
		else {
			ikb_handle_button (BTN_NEXT, 1);
		}
	}
	if (source & 0x10) {
		if (state & 0x10) {
			ikb_handle_button (BTN_PREVIOUS, 0);
		}
		else {
			ikb_handle_button (BTN_PREVIOUS, 1);
		}
	}

	if (wheel_source & 0x30) {
		handle_scroll_wheel((wheel_state & 0x30) >> 4, 0, 1);
	}
done:

	tasklet_schedule(&keyboard_tasklet);
#endif /* CONFIG_VT */

	ikb_finish_buttons();

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

	ikb_start_buttons();

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
					ikb_handle_button (BTN_ACTION, 1);
				}
			}
			else if (button_mask & 0x1) {
				ikb_handle_button (BTN_ACTION, 0);
			}

			if ((status & 0x1000) != 0) {
				new_button_mask |= 0x10;	/* Menu */
				if (!(button_mask & 0x10)) {
					ikb_handle_button (BTN_MENU, 1);
				}
			}
			else if (button_mask & 0x10) {
				ikb_handle_button (BTN_MENU, 0);
			}

			if ((status & 0x800) != 0) {
				new_button_mask |= 0x8;	/* Play/Pause */
				if (!(button_mask & 0x8)) {
					ikb_handle_button (BTN_PLAY, 1);
				}
			}
			else if (button_mask & 0x8) {
				ikb_handle_button (BTN_PLAY, 0);
			}

			if ((status & 0x200) != 0) {
				new_button_mask |= 0x2;	/* Next */
				if (!(button_mask & 0x2)) {
					ikb_handle_button (BTN_NEXT, 1);
				}
			}
			else if (button_mask & 0x2) {
				ikb_handle_button (BTN_NEXT, 0);
			}

			if ((status & 0x400) != 0) {
				new_button_mask |= 0x4;	/* Prev */
				if (!(button_mask & 0x4)) {
					ikb_handle_button (BTN_PREVIOUS, 1);
				}
			}
			else if (button_mask & 0x4) {
				ikb_handle_button (BTN_PREVIOUS, 0);
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

					ikb_scroll (wheel_delta);
				} else {
					ikb_pressed_at = jiffies;
					ikb_first_loc = new_wheel_value;
					ikb_push_event (EV_TOUCH | TOUCH(new_wheel_value));
				}
				wheel_bits16_22 = new_wheel_value;
			}
			else if (button_mask & 0x20) {
				/* scroll wheel up */
				ikb_push_event (EV_LIFT | LIFT(wheel_bits16_22));
				if ((ikb_current_scroll > -4) && (ikb_current_scroll < 4) &&
				    (jiffies - ikb_pressed_at < HZ/5)) {
					ikb_push_event (EV_TAP | TAP(ikb_first_loc));
				}
				ikb_make_scroll_event();

				wheel_bits16_22 = -1;
				wheel_events = 0;
				ikb_pressed_at = 0;
				ikb_first_loc = 0;
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

	ikb_finish_buttons();

	if ((inl(reg) & 0x8000000) != 0) {
		outl(0xffffffff, 0x7000c120);
		outl(0xffffffff, 0x7000c124);
	}

	outl(inl(0x7000c104) | 0xC000000, 0x7000c104);
	outl(0x400a1f00, 0x7000c100);

	outl(inl(0x6000d024) | 0x10, 0x6000d024);	/* port B bit 4 = 1 */
}

static ssize_t ikb_read (struct file *file, char *buf, size_t nbytes, loff_t *ppos) 
{
	DECLARE_WAITQUEUE (wait, current);
	ssize_t retval = 0, count = 0;
	
	if (nbytes == 0) return 0;

	ikb_make_scroll_event();
	ikb_reading = 1;

	add_wait_queue (&ikb_read_wait, &wait);
	while (nbytes > 0) {
		int ev;

		set_current_state (TASK_INTERRUPTIBLE);

		if (ikb_ev_head == ikb_ev_tail) {
			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				break;
			}
			if (signal_pending (current)) {
				retval = -ERESTARTSYS;
				break;
			}
			schedule();
			continue;
		}
		
		for (ev = ikb_ev_tail; ev != ikb_ev_head && nbytes >= 2; ikb_ev_tail++, ikb_ev_tail &= 31, ev = ikb_ev_tail) {
			put_user (ikb_events[ev], (unsigned short *)buf);
			count += 2;
			buf += 2;
			nbytes -= 2;
		}

		break; /* only read as much as we have */
	}

	ikb_reading = 0;
	current->state = TASK_RUNNING;
	remove_wait_queue (&ikb_read_wait, &wait);

	return (count? count : retval);
}

static unsigned int ikb_poll (struct file *file, poll_table *wait) 
{
	unsigned int mask = 0;

	poll_wait (file, &ikb_read_wait, wait);

	if (ikb_ev_head != ikb_ev_tail)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

/*
 * We allow multiple opens, even though multiple readers will compete for events,
 * since usually one reader is in wait() for the other to complete.
 * (It's no worse than a bog-standard TTY device.)
 */
static int ikb_open (struct inode *inode, struct file *file) 
{
	MOD_INC_USE_COUNT;
	ikb_opened++;
	return 0;
}

static int ikb_release (struct inode *inode, struct file *file) 
{
	ikb_opened--;
	MOD_DEC_USE_COUNT;
	return 0;
}

static struct file_operations ikb_fops = {
	read:		ikb_read,
	poll:		ikb_poll,
	open:		ikb_open,
	release:	ikb_release,
};
static struct miscdevice ikb_misc = { MISC_DYNAMIC_MINOR, "wheel", &ikb_fops };

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

	misc_register (&ikb_misc);
}

/*
 * Local Variables:
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
