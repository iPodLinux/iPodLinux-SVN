/*
 * ipodremote.c - remote control line dicipline for iPod
 *
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <asm/hardware.h>

/*
 * This driver allows input from the remote control to be directed
 * to the console as key input.
 *
 * Once this driver is loaded use the inputattach program to redirect
 * the serial input to this driver.
 *
 * For the third generation iPods:
 * 	inputattach -ipod3 /dev/ttyS0
 * For original "scroll-wheel" or second generation "touch-wheel" iPods:
 * 	inputattach -ipod /dev/ttyS0
 */

/*
 * 1g and 2g iPod remotes communicate at 9600 Baud 8N1.  When a
 * button is pressed on the remote the 4 bytes are generated:
 *
 * 0xff 0xfd n 0x00
 *
 * The n value varies depending on which button is pressed.  This sequence
 * will continue until the button is released at which point a single
 * sequence of:
 *
 * 0xff 0xfd 0xf0 0x00
 *
 * is sent.
 *
 * The possible n values are: 0xf1 -> play/pause, 0xf2 -> volume up,
 * 0xf3 -> volume down, 0xf4 -> fast forward, 0xf5 -> rewind and
 * 0xf0 -> no key down.
 */

/*
 * 3g iPod remotes comminicate at 19,200 Baud 8N1.  When a button
 * is pressed or released a stream of bytes is send.  Whilst the button
 * is held down 7 bytes will be repeated sent in the form:
 *
 * 0xff 0x55 0x03 0x02 0x00 n 0xfa
 *
 * Where n is either 0x01, 0x02, 0x04, 0x08 or 0x10.  These represent
 * play/pause, volume up, volume down, fast forward and rewind respectively.
 *
 * When the button is released the following bytes will be written one
 * or more times.
 *
 * 0Xff 0X55 0x03 0x02 0x00 0x00 0xfb
 */

#define IPOD_REMOTE_12G_MAX_LEN	4
#define IPOD_REMOTE_3G_MAX_LEN	7
#define N_BUTTONS	5

static int ipod_remote_buttons[] = {
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5
};

struct ipod_remote {
	struct input_dev dev;
	struct serio *serio;

	unsigned hw_ver;
	int idx;
	unsigned char data[IPOD_REMOTE_3G_MAX_LEN];
};

static void ipod_remote_process_input_12g(struct ipod_remote *ipod)
{
	static int last_button = 0;

	if ( ipod->data[2] == 0xf0 ) {
		input_report_key(&ipod->dev, last_button, 0);
	}
	else {
		int pos = ipod->data[2] - 0xf1;
		if ( pos >= 0  && pos < 5 ) {
			last_button = ipod_remote_buttons[pos];
			input_report_key(&ipod->dev, last_button, 1);
		}
	}
}

static void ipod_remote_process_input_3g(struct ipod_remote *ipod)
{
	static int last_button = -1;

	if ( ipod->data[6] == 0xfb ) {
		if (last_button >= 0) {
			input_report_key(&ipod->dev, last_button, 0);
			last_button = -1;
		}
	}
	else {
		int pos = -1;

		switch (ipod->data[5]) {
		case 0x1:
			pos = 0;
			break;
		case 0x2:
			pos = 1;
			break;
		case 0x4:
			pos = 2;
			break;
		case 0x8:
			pos = 3;
			break;
		case 0x10:
			pos = 4;
			break;
		}

		if ( pos >= 0  && pos < 5 ) {
			last_button = ipod_remote_buttons[pos];
			input_report_key(&ipod->dev, last_button, 1);
		}
	}
}

static void ipod_remote_connect(struct serio *serio, struct serio_dev *dev)
{
	struct ipod_remote *ipod;
	int i;

	if (serio->type != (SERIO_RS232 | SERIO_IPOD_REM)) {
		printk(KERN_ERR "ipod_remote: invalid serio->type 0x%lx\n", serio->type);
		return;
	}
                                                                                
	ipod = kmalloc(sizeof(struct ipod_remote), GFP_KERNEL);
	if ( !ipod ) {
		printk(KERN_ERR "ipod_remote: out of memory\n");
		return;
	}
	memset(ipod, 0x0, sizeof(struct ipod_remote));

	ipod->serio = serio;
	ipod->serio->private = ipod;
	ipod->dev.private = ipod;

	ipod->hw_ver = ipod_get_hw_version() >> 16;
	ipod->dev.name = "ipod-remote";
	ipod->dev.idbus = BUS_RS232;
	ipod->dev.idvendor = SERIO_IPOD_REM;
	ipod->dev.idproduct = 0x0001;
	ipod->dev.idversion = 0x0001;

	ipod->dev.evbit[0] = BIT(EV_KEY);
	for (i = 0; i < N_BUTTONS; i++) {
		set_bit(ipod_remote_buttons[i], &ipod->dev.keybit);
	}

	if ( serio_open(serio, dev) ) {
		printk(KERN_ERR "ipod_remote: serio_open failed\n");
		kfree(ipod);
		return;
	}

	input_register_device(&ipod->dev);
	printk("ipod_remote: input%d: registered\n", ipod->dev.number);
}

static void ipod_remote_disconnect(struct serio *serio)
{
	struct ipod_remote *ipod;

	ipod = serio->private;

	input_unregister_device(&ipod->dev);
	serio_close(serio);
	kfree(ipod);
}

static void ipod_remote_interrupt(struct serio *serio, unsigned char data, unsigned int flags)
{
	struct ipod_remote *ipod;

	ipod = serio->private;

	ipod->data[ipod->idx++] = data;
	if (ipod->hw_ver == 0x1 || ipod->hw_ver == 0x2) {
		if ( ipod->idx == IPOD_REMOTE_12G_MAX_LEN ) {
			ipod->idx = 0;
			ipod_remote_process_input_12g(ipod);
		}
	}
	else {
		if ( ipod->idx == IPOD_REMOTE_3G_MAX_LEN ) {
			ipod->idx = 0;
			ipod_remote_process_input_3g(ipod);
		}
	}
}


static struct serio_dev ipod_remote_dev = {
	connect:	ipod_remote_connect,
	disconnect:	ipod_remote_disconnect,
	interrupt:	ipod_remote_interrupt,
};

static int __init ipod_remote_init(void)
{
	printk("ipod_remote: $Id: ipodremote.c,v 1.1 2003/03/21 10:37:18 leachbj Exp $\n");

	serio_register_device(&ipod_remote_dev);

	return 0;
}

static void __exit ipod_remote_exit(void)
{
	serio_unregister_device(&ipod_remote_dev);
}

module_init(ipod_remote_init);
module_exit(ipod_remote_exit);

MODULE_AUTHOR("Bernard Leach <leachbj@bouncycastle.org>");
MODULE_DESCRIPTION("line driver for remote control for the iPod");
MODULE_LICENSE("GPL");

