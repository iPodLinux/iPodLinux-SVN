/*
 * ipodremote.c - remote control line dicipline for iPod
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/init.h>

#define IPOD_REMOTE_MAX_LEN	4
#define N_BUTTONS	5

static int ipod_remote_buttons[] = {
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5
};

struct ipod_remote {
	struct input_dev dev;
	struct serio *serio;

	int idx;
	unsigned char data[IPOD_REMOTE_MAX_LEN];
};

static void ipod_remote_process_input(struct ipod_remote *ipod)
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
	if ( ipod->idx == IPOD_REMOTE_MAX_LEN ) {
		ipod->idx = 0;
		ipod_remote_process_input(ipod);
	}
}


static struct serio_dev ipod_remote_dev = {
	connect:	ipod_remote_connect,
	disconnect:	ipod_remote_disconnect,
	interrupt:	ipod_remote_interrupt,
};

static int __init ipod_remote_init(void)
{
	printk("ipod_remote: $Id$\n");

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

