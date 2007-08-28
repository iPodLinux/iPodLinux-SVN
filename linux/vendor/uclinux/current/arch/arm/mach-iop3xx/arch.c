/*
 * linux/arch/arm/mach-iop3xx/arch.c
 *
 * Author: Nicolas Pitre <nico@cam.org>
 * Copyright (C) 2001 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#ifdef CONFIG_ARCH_IQ80310
extern void iq80310_map_io(void);
extern void iq80310_init_irq(void);
#endif

#ifdef CONFIG_ARCH_IQ80321
extern void iq80321_map_io(void);
extern void iop321_init_irq(void);
#endif

static void __init
fixup_iop3xx(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
	/*
	 * DIE DIE DIE
	 *
	 * This needs to go in the commandline or in the bootparams
	 * from the bootloader
	 */
	if(machine_is_iq80310()) {
		mi->bank[0].start = PHYS_OFFSET;
		mi->bank[0].size  = (32*1024*1024);
		mi->bank[0].node  = 0;
		mi->nr_banks      = 1;
	}
	if(machine_is_iq80321()) {
		mi->bank[0].start = PHYS_OFFSET;
		mi->bank[0].size  = (128*1024*1024);
		mi->bank[0].node  = 0;
		mi->nr_banks      = 1;
	}

#ifdef CONFIG_BLK_DEV_INITRD
	setup_ramdisk( 1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE );
	setup_initrd( phys_to_virt(0xa0800000), 4*1024*1024 );
#endif
}

#ifdef CONFIG_ARCH_IQ80310
MACHINE_START(IQ80310, "Cyclone IQ80310")
	MAINTAINER("MontaVista Software Inc.")
	BOOT_MEM(0xa0000000, 0xfe000000, 0xfe000000)
	FIXUP(fixup_iop3xx)
	MAPIO(iq80310_map_io)
	INITIRQ(iq80310_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IQ80321
MACHINE_START(IQ80321, "Intel IQ80321")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(0xa0000000, 0xfe800000, 0xfe800000)
	FIXUP(fixup_iop3xx)
	MAPIO(iq80321_map_io)
	INITIRQ(iop321_init_irq)
MACHINE_END
#endif
