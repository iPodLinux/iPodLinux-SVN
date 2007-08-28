/*
 *  linux/arch/arm/mach-adifcc/arch.c
 *
 *  Copyright (C) 2001 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>


static void __init
fixup_adi(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
	if(machine_is_adi_evb())
	{
		mi->bank[0].start = 0xc0000000;
		mi->bank[0].size  = (32*1024*1024);
		mi->bank[0].node  = 0;
		mi->nr_banks      = 1;
	} 
}

extern void adi_map_io(void);

#ifdef CONFIG_ARCH_ADI_EVB
extern void xs80200_init_irq(void);

MACHINE_START(ADI_EVB, "ADI 80200EVB Evaluation Board")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(0xc0000000, 0x00400000, 0xff400000)
	FIXUP(fixup_adi)
	MAPIO(adi_map_io)
	INITIRQ(xs80200_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_BRH
extern void brh_init_irq(void);

MACHINE_START(BRH, "ADI BRH")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(0xc0000000, 0x03000000, 0xff000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_adi)
	MAPIO(adi_map_io)
	INITIRQ(brh_init_irq)
MACHINE_END
#endif

