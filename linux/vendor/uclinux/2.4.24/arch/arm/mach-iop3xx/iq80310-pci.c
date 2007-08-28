/*
 * arch/arm/mach-iop3xx/iq80310-pci.c
 *
 * PCI support for the Intel IQ80310 reference board
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright (C) 2001 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/hardware.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci_auto.h>
#include <asm/arch/pci-bridge.h>
#include <asm/arch/pci_iop310.h>

#undef DEBUG
#ifdef DEBUG
#define  DBG(x...) printk(x)
#else
#define  DBG(x...)
#endif

static struct pci_controller *sec_hose, *pri_hose = NULL;
static struct resource sec_mem, pri_mem, sec_io, pri_io;

void __init
iq80310_init(void *sysdata)
{
	struct pci_bus *bus;
	struct pci_dev *dev;
	int host = *(volatile u32*)IQ80310_BACKPLANE & 0x1;

	system_rev = (*(volatile unsigned int*)0xfe830000) & 0x0f;

	if(system_rev)
		system_rev = 0xF;

	if(host)
		printk("PCI: IQ80310 is system controller\n");
	else
		printk("PCI: IQ80310 is agent\n");

	/* IOP310 default setup */
	iop310_init();

	if(host)
	{
		pri_hose = pcibios_alloc_controller();

		if(!pri_hose)
			panic("Could not allocate PCI hose");

		pri_hose->first_busno = 0;
		pri_hose->last_busno = 0;
		pri_hose->first_busno = 0;
		pri_hose->last_busno = 0xff;
		pri_hose->cfg_addr = (unsigned int *)IOP310_POCCAR;
		pri_hose->cfg_data = (unsigned char *)IOP310_POCCDR;
		pri_hose->io_space.start = IOP310_PCIPRI_LOWER_IO;
		pri_hose->io_space.end = IOP310_PCIPRI_UPPER_IO;
		pri_hose->mem_space.start = IOP310_PCIPRI_LOWER_MEM;
		pri_hose->mem_space.end = IOP310_PCIPRI_UPPER_MEM;

		pri_hose->mem_resources[0].start = IOP310_PCIPRI_LOWER_MEM;
		pri_hose->mem_resources[0].end = IOP310_PCIPRI_UPPER_MEM;
		pri_hose->mem_resources[0].flags = IORESOURCE_MEM;

		pri_hose->io_resource.start = IOP310_PCIPRI_LOWER_IO;
		pri_hose->io_resource.end = IOP310_PCIPRI_UPPER_IO;
		pri_hose->io_resource.flags = IORESOURCE_IO;

		/* Autoconfig the hose */
		pri_hose->last_busno = pciauto_bus_scan(pri_hose, 0);

		/* Scan the hose */
		bus = pci_scan_bus(0, &iop310_ops, sysdata);

		bus->resource[0] = &pri_hose->io_resource;
		bus->resource[1] = &pri_hose->mem_resources[0];

		pri_hose->mem_resources[0].name = "IOP310 PATU PCI Mem";
		pri_hose->io_resource.name =  "IOP310 PATU PCI I/O";
	}
	else
	{
		/*
		 * If we're running in agent mode, we remap the 
		 * SATU window to be at the same PCI location as
		 * the PATU window so that there are no collisions
		 * between the SATU PCI space and the host memory
		 * PCI space. This also allows us to have a single
		 * bus_to_virt/virt_to_bus for both hoses.
		 */

		*IOP310_SIABAR = *IOP310_PIABAR;
	}

	/* Always scan 2ndary hose */

	sec_hose = pcibios_alloc_controller();

	if (!sec_hose)
		panic("Couldn't allocate a PCI hose");

	/*
	 * Note that in agent mode, we can just tell the 2ndary
	 * hose that it is at 0 since it's a private configuration
	 * space from the primary bus.
	 */
	if(host)
		sec_hose->first_busno = pri_hose->last_busno + 1;
	else
		sec_hose->first_busno = 0;

	sec_hose->last_busno = 0xff;
	sec_hose->cfg_addr = (unsigned int *)IOP310_SOCCAR;
	sec_hose->cfg_data = (unsigned char *)IOP310_SOCCDR;
	sec_hose->io_space.start = IOP310_PCISEC_LOWER_IO;
	sec_hose->io_space.end = IOP310_PCISEC_UPPER_IO;
	sec_hose->mem_space.start = IOP310_PCISEC_LOWER_MEM;
	sec_hose->mem_space.end = IOP310_PCISEC_UPPER_MEM;
	
	sec_hose->mem_resources[0].start = IOP310_PCISEC_LOWER_MEM;
	sec_hose->mem_resources[0].end = IOP310_PCISEC_UPPER_MEM;
	sec_hose->mem_resources[0].flags = IORESOURCE_MEM;

	sec_hose->io_resource.start = IOP310_PCISEC_LOWER_IO;
	sec_hose->io_resource.end = IOP310_PCISEC_UPPER_IO;
	sec_hose->io_resource.flags = IORESOURCE_IO;

		/* Autoconfig the hose */
	/* Autoconfig the hose */
	sec_hose->last_busno = 
		pciauto_bus_scan(sec_hose, sec_hose->first_busno);

	/* Scan the hose */
	bus = pci_scan_bus(sec_hose->first_busno, &iop310_ops, sysdata);

	bus->resource[0] = &sec_hose->io_resource;
	bus->resource[1] = &sec_hose->mem_resources[0];

	sec_hose->mem_resources[0].name = "IOP310 SATU PCI Mem";
	sec_hose->io_resource.name = "IOP310 SATU PCI I/O";

	pcibios_allocate_bus_resources(&pci_root_buses);
	pcibios_allocate_resources();
}

#define INTA	IRQ_IQ80310_INTA
#define INTB	IRQ_IQ80310_INTB
#define INTC	IRQ_IQ80310_INTC
#define INTD	IRQ_IQ80310_INTD

#define INTE	IRQ_IQ80310_I82559

static inline int __init
iq80310_pri_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	const long min_idsel = 2, max_idsel = 3, irqs_per_slot = 4;

	/*
	 * On a Rev D.1 and older board, INT A-C are not routed, so we
	 * just fake it as INTA and than we take care of handling it
	 * correctly in the IRQ demux routine.
	 */
	if(!system_rev)
	{
		static char pci_irq_table[ ][4] = 
			{
				/*
	 	 	 	 * PCI IDSEL/INTPIN->INTLINE
	 	 	 	 * A       B       C       D
	 	 	 	 */
				{INTA, INTD, INTA, INTA}, /*  PCI Slot J3 */
				{INTD, INTA, INTA, INTA}, /*  PCI Slot J4 */
			};
		return PCI_IRQ_TABLE_LOOKUP;
	}
	else
	{
		static char pci_irq_table[ ][4] = 
			{
				/*
	 	 	 	 * PCI IDSEL/INTPIN->INTLINE
	 	 	 	 * A       B       C       D
	 	 	 	 */
				{INTC, INTD, INTA, INTB}, /*  PCI Slot J3 */
				{INTD, INTA, INTB, INTC}, /*  PCI Slot J4 */
			};
		return PCI_IRQ_TABLE_LOOKUP;
	}
}


static inline int __init
iq80310_sec_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	const long min_idsel = 0, max_idsel = 2, irqs_per_slot = 4;

	if(!system_rev)
	{
		static char pci_irq_table[ ][4] = 
			{
				/*
	 		 	 * PCI IDSEL/INTPIN->INTLINE
	 		 	 * A       B       C       D
	 		 	 */
				{INTA, INTA, INTA, INTD}, /*  PCI Slot J1 */
				{INTA, INTA, INTD, INTA}, /*  PCI Slot J5 */
				{INTE, INTE, INTE, INTE}, /*  P2P Bridge */
			};
		return PCI_IRQ_TABLE_LOOKUP;
	}
	else
	{
		static char pci_irq_table[ ][4] = 
			{
				/*
	 		 	 * PCI IDSEL/INTPIN->INTLINE
	 		 	 * A       B       C       D
	 		 	 */
				{INTA, INTB, INTC, INTD}, /* PCI Slot J1 */
				{INTB, INTC, INTD, INTA}, /* PCI Slot J5 */
				{INTE, INTE, INTE, INTE}, /* P2P Bridge */
			};
		return PCI_IRQ_TABLE_LOOKUP;
	}

}

static inline int __init
iq80310_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	struct pci_controller *hose;

	hose = pci_bus_to_hose(dev->bus->number);

	if(hose == pri_hose)
		return iq80310_pri_map_irq(dev, idsel, pin);
	else
		return iq80310_sec_map_irq(dev, idsel, pin);
}


struct hw_pci iq80310_pci __initdata = {
	init:		iq80310_init,
	swizzle:	common_swizzle,
	map_irq:	iq80310_map_irq,
};
