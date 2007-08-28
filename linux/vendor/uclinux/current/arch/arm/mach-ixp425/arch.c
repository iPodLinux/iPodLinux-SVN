/*
 * arch/arm/mach-ixp425/arch.c 
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

extern void ixp425_map_io(void);
extern void ixp425_init_irq(void);

#ifdef CONFIG_ARCH_IXDP425
MACHINE_START(IXDP425, "Intel IXDP425 Development Platform")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
	BOOT_PARAMS(0x2000)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_IXCDP1100
MACHINE_START(IXCDP1100, "Intel IXCDP1100 Development Platform")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
	BOOT_PARAMS(0x2000)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_PRPMC1100
MACHINE_START(PRPMC1100, "Motorolla PrPMC 1100")
	MAINTAINER("MontaVista Software, Inc.")
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
	BOOT_PARAMS(0x2000)
MACHINE_END
#endif


#ifdef CONFIG_ARCH_ADI_COYOTE
static void __init
fixup_coyote(struct machine_desc *desc, struct param_struct *params,
                char **cmdline, struct meminfo *mi)
{
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = CONFIG_IXP425_SDRAM_SIZE * 1024 * 1024;
	mi->bank[0].node = 0;
	mi->nr_banks = 1;

#ifdef CONFIG_BLK_DEV_INITRD
        setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
        setup_initrd(phys_to_virt(0x00A00000), 4 * 1024 * 1024);
#endif
}

MACHINE_START(ADI_COYOTE, "Intel IXP425 Residential Gateway Demo Platform")
        MAINTAINER("Intel - IABU")
        /*       Memory Base, Phy IO,    Virtual IO */
        BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
                IXP425_PERIPHERAL_BASE_VIRT)
	FIXUP(fixup_coyote)
        MAPIO(ixp425_map_io)
        INITIRQ(ixp425_init_irq)
        BOOT_PARAMS(0x2000)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_SE4000
MACHINE_START(SE4000, "SnapGear SE4000")
	MAINTAINER("SnapGear Inc.")
	BOOT_MEM(PHYS_OFFSET, IXP425_PERIPHERAL_BASE_PHYS,
		IXP425_PERIPHERAL_BASE_VIRT)
	MAPIO(ixp425_map_io)
	INITIRQ(ixp425_init_irq)
	BOOT_PARAMS(0x2000)
MACHINE_END
#endif
