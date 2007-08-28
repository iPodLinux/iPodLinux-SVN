/*
 * arch/arm/mach-ixp1200/arch.c
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2001 MontaVista Software, Inc.
 *
 */

#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <asm/types.h>
#include <asm/setup.h>
#include <asm/system.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

extern void ixp1200_init_irq(void);
extern void ixp1200_map_io(void);

static void __init 
fixup_ixp1200(struct machine_desc *dex, struct param_struct *params,
		char **cmdline, struct meminfo *mi)
{

#ifdef defined(CONFIG_BLK_DEV_INITRD)
	setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE );
	setup_initrd(phys_to_virt(INITRD_LOCATION), INITRD_SIZE);
#endif

	mi->nr_banks = 1;
	mi->bank[0].node = 0;
	mi->bank[0].start = CONFIG_IXP1200_SDRAM_BASE;
	mi->bank[0].size = CONFIG_IXP1200_SDRAM_SIZE * 0x100000;
}

MACHINE_START(IXP1200, "Intel IXP1200-based platform")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(CONFIG_IXP1200_SDRAM_BASE, 0x90000000, 0xf0000000)
	FIXUP(fixup_ixp1200)
	MAPIO(ixp1200_map_io)
	INITIRQ(ixp1200_init_irq)
MACHINE_END

