/*
 * arch/arm/mach-adiff/brh-pci.c
 *
 * PCI routines for ADI BRH board
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2000-2001 MontaVista Software Inc.
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

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/hardware.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci_auto.h>
#include <asm/arch/pci-bridge.h>

extern int (*external_fault)(unsigned long, struct pt_regs *);
static int brh_pci_abort_handler(unsigned long, unsigned long, struct pt_regs *);

//#define DEBUG

#ifdef DEBUG
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif /* DEBUG */

/*
 * Used to check on status of bad config cycles
 */
static int error_ocurred;

static u32* 
brh_pci_config_setup(struct pci_dev *dev, int where)
{
	u32 *paddress;

	error_ocurred = 0;

	/* Must be dword aligned */

	where &= ~3;

	/*
	 * For top bus, generate type 0, else type 1
	 */
	if(!dev->bus->number)
	{
		if(PCI_SLOT(dev->devfn) > 3)
			return NULL;

		*BRH_PCI_CONFIG_CTRL = 0x00000000;

		paddress = ((0xfb000000) | (1 << (PCI_SLOT(dev->devfn)+11)) |
				(PCI_FUNC(dev->devfn) << 8) | where);
	
	}
	else
	{
		*BRH_PCI_CONFIG_CTRL = 0x00000001;

		paddress = ((0xfb000000) | (dev->bus->number << 16) |
				(PCI_SLOT(dev->devfn) << 11) | 
				(PCI_FUNC(dev->devfn) << 8) | where);
	}


	DBG("brh_pci_config_setup(%d:%d:%d:%d) returning %#10x\n", 
			dev->bus->number, PCI_SLOT(dev->devfn), 
			PCI_FUNC(dev->devfn), where, paddress);

	return paddress;
}


static int
brh_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	u32 *address;

	DBG("config_read_byte %d from dev %d:%d:%d\n", where, 
			dev->bus->number, PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn));

	address = brh_pci_config_setup(dev, where);

	if(!address)
		*value = 0xff;
	else
	{
		cli();
		*value = (u8)(((*address) >> ((where % 0x04) * 8)) & 0xff);
		if(error_ocurred) *value = 0xff;
		sti();
	}


	DBG("config_read_byte read %#4x\n", *value);

	return PCIBIOS_SUCCESSFUL;
}

static int
brh_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	u32 *address;

	DBG("config_read_word %d from dev %d:%d:%d\n", where, 
			dev->bus->number, PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn));

	address = brh_pci_config_setup(dev, where);

	if(!address)
		*value = 0xffff;
	else
	{	
		cli();
		*value = (u16)(((*address) >> ((where % 0x04) *8)) & 0xffff);
		if(error_ocurred) *value = 0xffff;
		sti();
	}

	DBG("config_read_word read %#6x\n", *value);

	return PCIBIOS_SUCCESSFUL;
}

static int
brh_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	u32 *address;

	DBG("config_read_dword %d from dev %d:%d:%d\n", where, 
			dev->bus->number, PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn));

	address = brh_pci_config_setup(dev, where);

	if(!address)
		*value = 0xffffffff;
	else
	{
		cli();
		*value = *address;
		if(error_ocurred) *value = 0xffffffff;
		sti();
	}

	DBG("config_read_dword read %#10x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}

/*
 * We don't do error checking on the address for writes.
 * It's assumed that the user checked for the device existing first
 * by doing a read first.
 */
static int 
brh_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	u32 *address;
	u32 mask;
	u32 temp;

	address = brh_pci_config_setup(dev, where);

	mask = ~(0x000000ff << ((where % 0x4) * 8));
	temp = (u32)(((u32)value) << ((where % 0x4) *8));
	*address = (*address & mask) | temp;

	return PCIBIOS_SUCCESSFUL;
}

static int 
brh_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	u32 *address;
	u32 mask;
	u32 temp;

	address = brh_pci_config_setup(dev, where);

	mask = ~(0x0000ffff << ((where % 0x4) * 8));
	temp = (u32)(((u32)value) << ((where % 0x4) * 8));
	*address = (*address & mask) | temp;

	return PCIBIOS_SUCCESSFUL;
}

static int
brh_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	u32 *address;

	address = brh_pci_config_setup(dev, where);

	*address = value;

	return PCIBIOS_SUCCESSFUL;
}


struct pci_ops brh_ops = {
	brh_read_config_byte,
	brh_read_config_word,
	brh_read_config_dword,
	brh_write_config_byte,
	brh_write_config_word,
	brh_write_config_dword,
};

static struct pci_controller *hose;

static int
brh_pci_abort_handler(unsigned long addr, unsigned long fsr, struct pt_regs *regs) 
{
	u16 status;

	DBG("\n");
	DBG("DATA ABORT ON %#010x\n", addr);

	brh_read_config_word(fake_pci_dev(hose, 0, 0), 
				PCI_STATUS,
				&status);

	DBG("BRH_PCI_MAST_INT: %#010x\n", *BRH_PCI_MAST_INT);
	DBG("PCI_CFG_STATUS: %#010x\n", status);
	DBG("INT STATUS: %#010x\n", *BRH_INT_STAT);

	/*
	 * Clear Mast. Abort line
	 */
	*BRH_PCI_MAST_INT |= 0x8;

	status |= PCI_STATUS_REC_MASTER_ABORT;
	brh_write_config_word(fake_pci_dev(hose, 0, 0), 
				PCI_STATUS,
				status);

	error_ocurred = 1;

	/*
	 * If it was an imprecise abort, then we need to correct the
	 * return address to be _after_ the instruction.
	 */
	if (fsr & (1 << 10))
		regs->ARM_pc += 4;
	
	return 0;
}

static struct resource brh_mem = {
	name: "BRH PCI Non-Prefetch Memory Space",
	start: 0x0c000000,
	end:   0x0fffffff,
	flags: IORESOURCE_MEM
};

static struct resource brh_io = {
	name: "BRH PCI I/O Space",
	start: 0x0a000000,
	end:   0x0bffffff,
	flags: IORESOURCE_IO
};

void brh_pci_setup_resources(struct resource **res)
{
	if(request_resource(&ioport_resource, &brh_io))
		panic("COULD NOT REQUEST I/O");

	if(request_resource(&iomem_resource, &brh_mem))
		panic("COULD NOT REQUEST MEM");

	res[0] = &brh_io;
	res[1] = &brh_mem;
	res[2] = NULL;
}

void __init
brh_pci_init(void *sysdata)
{
	struct pci_bus *bus;
	struct pci_dev *dev;
	u8 rev_id;

	DBG("Allocating hose\n");

	hose = pcibios_alloc_controller();
	if(!hose)
		panic("Could not allocate PCI hose");

	brh_read_config_byte(fake_pci_dev(hose, 0, 0), 
			PCI_REVISION_ID, &rev_id);

	printk("PCI: BRH Revision is %#04x\n", rev_id);

	system_rev = rev_id;

	/*
	 * RedBoot enables the PCI unit, but the PCI config can't
	 * be updated while PCI is enabled.
	 */
	brh_write_config_word(fake_pci_dev(hose, 0, 0), PCI_COMMAND, 0);


	/*
	 * Setup two outbound PCI mem ATUs to be 1:1 mapped
	 * along with the I/O ATU.
	 */
#ifndef __ARMEB__
	*BRH_PCI_MEM_ATU1 = (unsigned int)0x0c000000;
	*BRH_PCI_MEM_ATU2 = (unsigned int)0x0e000000;
	*BRH_PCI_IO_ATU   = (unsigned int)0x0a000000;
#else
	*BRH_PCI_MEM_ATU1 = (unsigned int)0x0c000004;
	*BRH_PCI_MEM_ATU2 = (unsigned int)0x0e000004;
	*BRH_PCI_IO_ATU   = (unsigned int)0x0a000004;
#endif

	hose->first_busno = 0;
	hose->last_busno = 0;
	hose->io_space.start = 0x0a000000;
	hose->io_space.end = 0x0bffffff;
	hose->mem_space.start = 0x0c000000;
	hose->mem_space.end = 0x0fffffff;

	/*
	 *
	 * We setup the two inbound windows to be contigous from
	 * 0xc0000000 to 0xc3ffffff.  This allows for 1:1 mapping of
	 * phys to bus addresses. We also need to setup the inbound
	 * translation registers to force the 1:1 mapping.
	 */
	if(!system_rev) {
		brh_write_config_dword(fake_pci_dev(hose, 0, 0), 
			PCI_BASE_ADDRESS_0, 0xc0000000);
#ifndef __ARMEB__
		*BRH_PCI_MEM0_XLATE = 0x00000000;
#else
		*BRH_PCI_MEM0_XLATE = 0x00000001;
#endif
	} else {

		/*
		 * Disable the two 32M windows
		 */
		brh_write_config_dword(fake_pci_dev(hose, 0, 0), 
			PCI_BASE_ADDRESS_0, 0x00000000);
		brh_write_config_dword(fake_pci_dev(hose, 0, 0), 
			PCI_BASE_ADDRESS_1, 0x00000000);

		brh_write_config_dword(fake_pci_dev(hose, 0, 0), 
			PCI_BASE_ADDRESS_2, 0xc0000000);

#ifndef __ARMEB__
		*BRH_PCI_MEM2_XLATE = 0x00000000;
#else
		*BRH_PCI_MEM2_XLATE = 0x00000001;
#endif
	}
	
	brh_write_config_word(fake_pci_dev(hose, 0, 0), PCI_COMMAND,
			PCI_COMMAND_MEMORY|PCI_COMMAND_MASTER);

	DBG("Calling autoconfig\n");

	external_fault = brh_pci_abort_handler;

	/*
	 * Autoconfig the bus 
	 */
	hose->last_busno = pciauto_bus_scan(hose, 0);

	DBG("Scanning the bus\n");
	/* Scan the bus */
	bus = pci_scan_bus(0, &brh_ops, sysdata);
}

#define	INTA	IRQ_BRH_PCI_INT_A
#define	INTB	IRQ_BRH_PCI_INT_B
#define	INTC	IRQ_BRH_PCI_INT_C
#define	INTD	IRQ_BRH_PCI_INT_D

static int __init
brh_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	const long min_idsel = 1, max_idsel = 3, irqs_per_slot = 4;

	static char pci_irq_table[][4] =
		{
			/*
			 * PCI IDSEL/INTPIN -> INTLINE
			 */
			{INTB, INTB, INTB, INTB},	/* Enet 0 */
			{INTC, INTC, INTC, INTC},	/* Enet 1 */
			{INTA, INTB, INTC, INTD},	/* PCI Slot */
		};

	return PCI_IRQ_TABLE_LOOKUP;
}

#define EARLY_PCI_OP(rw, size, type)					\
int early_##rw##_config_##size(struct pci_controller *hose, int bus,	\
		int devfn, int offset, type value)	\
{									\
	return brh_##rw##_config_##size(fake_pci_dev(hose, bus, devfn),	\
			offset, value);			\
}

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)

struct hw_pci brh_pci __initdata = {
	init:		brh_pci_init,
	swizzle:	common_swizzle,
	map_irq:	brh_map_irq,
	setup_resources: brh_pci_setup_resources
};

