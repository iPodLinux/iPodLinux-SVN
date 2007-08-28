/*
 * arch/arm/mach-ixp2000/ixmb2400-pci.c
 *
 * PCI routines for IXDP2400 board
 *
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/hardware.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci-bridge.h>

//#define DEBUG 1

#ifdef DEBUG 
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif /* DEBUG */

extern void ixp2000_pci_init(void *);

/*
 * This board does not do normal PCI IRQ routing, or any
 * sort of swizzling, so we just need to check where on the
 * bus the device is and figure out what CPLD pin it's 
 * being routed to.
 */
static int __init
ixdp2400_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{

	if (npu_is_master()) {

		/*
		 * Root bus devices.  Slave NPU is only one with interrupt.
		 * Everything else, we just return -1 which is invalid.
		 */
		if(!dev->bus->self) {
			if(dev->devfn == 0x0028 )
					return IRQ_IXDP2400_INGRESS_NPU;

			return -1;
		}

		/*
		 * We have a bridge behind the PMC slot
		 * NOTE: Only INTA from the PMC slot is routed.
		 */
		if(PCI_SLOT(dev->bus->self->devfn) == 0x7 &&
				  PCI_SLOT(dev->bus->parent->self->devfn) == 0x4 &&
				  !dev->bus->parent->self->bus->parent)
				  return	IRQ_IXDP2400_PMC;

		/*
		 * Device behind the first bridge
		 */
		if(PCI_SLOT(dev->bus->self->devfn) == 0x4) {
			switch(PCI_SLOT(dev->devfn)) {
				case 0x4:	// Master ethernet
					return IRQ_IXDP2400_METHERNET;	
			
				case 0x5:	// Media interface
					return IRQ_IXDP2400_MEDIA_PCI;

				case 0x6:	// Switch fabric
					return IRQ_IXDP2400_SF_PCI;

				case 0x7:	// PMC slot
					return IRQ_IXDP2400_PMC;
			}
		}

		return 0;

	} else return IRQ_IXDP2400_SETHERNET; /* Slave NIC interrupt */
}

extern void ixp2000_pci_init(void *);

void __init ixdp2400_pci_init(void *sysdata)
{
	ixp2000_pci_init(sysdata);

	/*
	 * RedBoot configures the PCI bus with all I/O > 1MB,
	 * PCI starting at 0.
	 */
	*IXP2000_PCI_ADDR_EXT = 0x00100000;

	/*
	 * Exclude devices depending on whether we are the master
	 * or slave NPU. Sigh...
	 */
	if (npu_is_master()) {
		pci_exclude_device(0x01, 0x18); /* remove slave's NIC*/
	} else {
		int i;
		printk("NPU IS SLAVE..\n");

		pci_exclude_device(0x01, 0x38); /* remove PMC site */
		pci_exclude_device(0x01, 0x20); /* remove master NIC*/
		pci_exclude_device(0x0, 0x20); /* remove remove 21154*/
		pci_exclude_device(0x0, 0x30); /* remove 21555*/
		pci_exclude_device(0x0, 0x38); /* remove Master IXP*/

		/*
		 * In case there's a bridge on the PMC, we remove everything
		 * on bus 2.
		 */
		for(i = 0; i < 0xff; i++)
				  pci_exclude_device(0x2, i);
	}
}

struct hw_pci ixdp2400_pci __initdata = {
        init:           ixdp2400_pci_init,
        swizzle:        no_swizzle,
        map_irq:        ixdp2400_map_irq,
	mem_offset:	0xe0000000
};


