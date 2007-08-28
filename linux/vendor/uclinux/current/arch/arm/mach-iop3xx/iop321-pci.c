/*
 * arch/arm/mach-iop3xx/iop321-pci.c
 *
 * PCI support for the Intel IOP321 chipset
 *
 * Author: Rory Bolt <rorybolt@pacbell.net>
 * Copyright (C) 2002 Rory Bolt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/arch/hardware.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci_auto.h>
#include <asm/arch/pci-bridge.h>

// #define DEBUG

#ifdef DEBUG
#define  DBG(x...) printk(x)
#else
#define  DBG(x...)
#endif

extern int (*external_fault)(unsigned long, struct pt_regs *);
int iop321_pci_abort_handler(unsigned long, unsigned long, struct pt_regs *);

void iop321_init(void)
{
	DBG("PCI:  Intel 80321 PCI init code.\n");
	DBG("\tATU: IOP321_ATUCMD=0x%04x\n", *IOP321_ATUCMD);
	DBG("\tATU: IOP321_OMWTVR0=0x%04x, IOP321_OIOWTVR=0x%04x\n", 
			*IOP321_OMWTVR0, 
			*IOP321_OIOWTVR);
	DBG("\tATU: IOP321_ATUCR=0x%08x\n", *IOP321_ATUCR);
	DBG("\tATU: IOP321_IABAR0=0x%08x IOP321_IALR0=0x%08x IOP321_IATVR0=%08x\n", *IOP321_IABAR0, *IOP321_IALR0, *IOP321_IATVR0);
	DBG("\tATU: IOP321_ERBAR=0x%08x IOP321_ERLR=0x%08x IOP321_ERTVR=%08x\n", *IOP321_ERBAR, *IOP321_ERLR, *IOP321_ERTVR);
	DBG("\tATU: IOP321_IABAR2=0x%08x IOP321_IALR2=0x%08x IOP321_IATVR2=%08x\n", *IOP321_IABAR2, *IOP321_IALR2, *IOP321_IATVR2);
	DBG("\tATU: IOP321_IABAR3=0x%08x IOP321_IALR3=0x%08x IOP321_IATVR3=%08x\n", *IOP321_IABAR3, *IOP321_IALR3, *IOP321_IATVR3);

	external_fault = iop321_pci_abort_handler;
}

/*
 * This routine builds either a type0 or type1 configuration command.  If the
 * bus is on the 80321 then a type0 made, else a type1 is created.
 */ 
static inline u32 iop321_pci_config_setup(struct pci_dev *dev, int where)
{
	volatile u32 *paddress;
	volatile u32 *pdata;
	struct pci_controller *hose;

	DBG("\tPCI: config_setup of bus %d\n", dev->bus->number);

	/*
	 * Get appropriate cfga/cfgd values from hose
	 */
	hose = pci_bus_to_hose(dev->bus->number);
	paddress = (u32 *)hose->cfg_addr;
	pdata = (u32 *)hose->cfg_data;

	/* locations must be dword-aligned */
	where &= ~3;

	/*
	 * For top bus generate a type 0 config,
	 * all others use a type 1 config
	 */
	if (dev->bus->number == hose->first_busno)
	{
		*paddress = ( (1 << (PCI_SLOT(dev->devfn) + 16)) |
				(PCI_FUNC(dev->devfn) << 8) | where | 0 );
	}
	else
	{
		*paddress = ( (dev->bus->number << 16) |
				(PCI_SLOT(dev->devfn) << 11) | 
				(PCI_FUNC(dev->devfn) << 8) | where | 1 );
	}

	return (u32)pdata;
}

/*
 * This routine checks the status of the last configuration cycle.  If an error
 * was detected it returns a 1, else it returns a 0.  The errors being checked
 * are parity, master abort, target abort (master and target).  These types of
 * errors occure during a config cycle where there is no device, like during
 * the discovery stage.
 */ 
static int iop321_pci_config_cleanup(u8 bus)
{
	u32 status = 0, err = 0;
	struct pci_controller *hose;

	hose = pci_bus_to_hose(bus);

	/*
	 * Check the status registers.
	 */ 
	if (hose->cfg_addr == (unsigned int *)IOP321_OCCAR) 
	{
		status = *IOP321_ATUSR;
		if (status & 0xF900)
		{
			DBG("\t\t\tPCI: P0 - status = 0x%08x\n", status);
			err = 1;
			*IOP321_ATUSR = status & 0xF900;
		}
		status = *IOP321_ATUISR;
		if (status & 0x679F) 
		{
			DBG("\t\t\tPCI: P1 - status = 0x%08x\n", status);
			err = 1;
			*IOP321_ATUISR = status & 0x679f;
		}
	} 
	return err;
}

/*
 * The read routines must check the error status of the last configuration
 * cycle.  If there was an error, the routine returns all hex f's.
 */ 
static int
iop321_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	volatile u32 *pdata;

	cli();

	pdata = (u32 *)iop321_pci_config_setup(dev, where);
	udelay(50);
	*value = (u8) (((*pdata) >> ((where % 0x04) * 8)) & 0xff);
	udelay(50);

	if( iop321_pci_config_cleanup(dev->bus->number) )
	{
		*value = 0xff;
	} /* IF END */

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop321_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	volatile u32 *pdata;

	cli();

	pdata = (u32 *)iop321_pci_config_setup(dev, where);
	udelay(50);
	*value = (u16) (((*pdata) >> ((where % 0x04) * 8)) & 0xffff);
	udelay(50);

	if( iop321_pci_config_cleanup(dev->bus->number) )
	{
		*value = 0xffff;
	} /* IF END */

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop321_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	volatile u32 *pdata;

	cli();

	pdata = (u32 *)iop321_pci_config_setup(dev, where);
	udelay(50);
	*value =  *pdata;
	udelay(50);

	if( iop321_pci_config_cleanup(dev->bus->number) )
	{
		*value = 0xffffffff;
	} /* IF END */

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop321_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	volatile u32 *pdata;
	u32 mask;
	u32 temp;

	cli();

	pdata = (u32 *)iop321_pci_config_setup(dev, where);
	udelay(50);
	mask = ~(0x000000ff << ((where % 0x4) * 8));
	temp = (u32)(((u32)value) << ((where% 0x4) * 8));
	*pdata = (*pdata & mask) | temp;
	udelay(50);

	iop321_pci_config_cleanup(dev->bus->number);

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop321_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	volatile u32 *pdata;
	u32 mask;
	u32 temp;

	cli();

	pdata = (u32 *)iop321_pci_config_setup(dev, where);
	udelay(50);
	mask = ~(0x0000ffff << ((where % 0x4) * 8));
	temp = (u32)(((u32)value) << ((where % 0x4) * 8));
	*pdata = (*pdata & mask) | temp;
	udelay(50);

	iop321_pci_config_cleanup(dev->bus->number);

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop321_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	volatile u32 *pdata;

	cli();
	
	pdata = (u32 *)iop321_pci_config_setup(dev, where);
	udelay(50);
	*pdata = value;
	udelay(50);

	iop321_pci_config_cleanup(dev->bus->number);

	sti();

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops iop321_ops = {
	iop321_read_config_byte,
	iop321_read_config_word,
	iop321_read_config_dword,
	iop321_write_config_byte,
	iop321_write_config_word,
	iop321_write_config_dword,
};

/*
 * When a PCI device does not exist during config cycles, the 80200 gets a 
 * bus error instead of returning 0xffffffff. This handler simply returns.  
 */ 
int
iop321_pci_abort_handler(unsigned long addr, unsigned long fsr, struct pt_regs *regs) 
{
	/*
	 * If it was an imprecise abort, then we need to correct the
	 * return address to be _after_ the instruction.
	 */
	if (fsr & (1 << 10))
		regs->ARM_pc += 4;
	

	return 0;
}

#define EARLY_PCI_OP(rw, size, type)					\
int early_##rw##_config_##size(struct pci_controller *hose, int bus,	\
		int devfn, int offset, type value)	\
{									\
	return iop321_##rw##_config_##size(fake_pci_dev(hose, bus, devfn),	\
			offset, value);			\
}

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)
