/*
 * arch.c - architecture definition for iPod
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/arch/irq.h>
#include <asm/mach/arch.h>

static void __init
ipod_fixup(struct machine_desc *desc, struct param_struct *params,
	char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_BLK_DEV_RAM
	extern void __ramdisk_data, __ramdisk_data_end, _text;

	setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
	setup_initrd((int)&__ramdisk_data,
		(int)&__ramdisk_data_end - (int)&__ramdisk_data);
#else
	strcpy(*cmdline, "root=/dev/hda2 rw");
#endif
}

MACHINE_START(IPOD, "iPod")
	MAINTAINER("Bernard Leach")
	BOOT_MEM(0x28000000, 0xc0000000, 0x00000000)
	INITIRQ(ipod_init_irq)
	FIXUP(ipod_fixup)
MACHINE_END

