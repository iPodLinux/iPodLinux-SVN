/*
 * ipod-ide.c - IDE driver for iPod
 *
 * Copyright (c) 2004-2005 Bernard Leach <leachbj@bouncycastle.org>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <asm/irq.h>

#define IDE_PRIMARY_BASE	0xc00031e0
#define IDE_PRIMARY_CONTROL	0xc00033f8

int __init ipod_ide_register(void)
{
	hw_regs_t hw;
	ide_ioreg_t reg;
	ide_hwif_t *hwifp;
	int i;

	outl(inl(0xc0003024) | (1 << 7), 0xc0003024);
	outl(inl(0xc0003024) & ~(1<<2), 0xc0003024);

	outl(0x10, 0xc0003000);
	outl(0x80002150, 0xc0003004);

	memset(&hw, 0, sizeof(hw));

	reg = IDE_PRIMARY_BASE;
	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw.io_ports[i] = reg;
		reg += 4;	/* our registers are on word boundaries */
	}

	hw.io_ports[IDE_CONTROL_OFFSET] = IDE_PRIMARY_CONTROL;
	hw.irq = IDE_INT0_IRQ;

	ide_register_hw(&hw, &hwifp);

	return 0;
}

#ifdef MODULE
int __init init_module(void)
{
	return ipod_ide_register();
}

void __exit cleanup_module(void)
{
}

module_init(init_module);
module_exit(cleanup_module);

MODULE_AUTHOR("Bernard Leach <leachbj@bouncycastle.org>");
MODULE_DESCRIPTION("IDE driver for IPod");
MODULE_LICENSE("GPL");

#endif
