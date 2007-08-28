/*
 * arch/arm/mach-iop3xx/iop310-pci.c
 *
 * PCI support for the Intel IOP310 chipset
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

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/arch/hardware.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci_auto.h>
#include <asm/arch/pci-bridge.h>

#undef DEBUG
#ifdef DEBUG
#define  DBG(x...) printk(x)
#else
#define  DBG(x...)
#endif

extern int (*external_fault)(unsigned long, struct pt_regs *);
int iop310_pci_abort_handler(unsigned long, unsigned long, struct pt_regs *);

int iop310_host;

void iop310_init(void)
{
	DBG("PCI:  Intel 80312 PCI-to-PCI init code.\n");
	DBG("\tATU secondary: IOP310_SOMWVR=0x%04x, IOP310_SOIOWVR=0x%04x\n", 
			*IOP310_SOMWVR, 
			*IOP310_SOIOWVR);
	DBG("\tATU secondary: IOP310_ATUCR=0x%08x\n", *IOP310_ATUCR);
	DBG("\tATU secondary: IOP310_SIABAR=0x%08x IOP310_SIALR=0x%08x IOP310_SIATVR=%08x\n", *IOP310_SIABAR, *IOP310_SIALR, *IOP310_SIATVR);

	DBG("\tATU primary: IOP310_POMWVR=0x%04x, IOP310_POIOWVR=0x%04x\n", 
			*IOP310_POMWVR, 
			*IOP310_POIOWVR);
	DBG("\tATU secondary: IOP310_PIABAR=0x%08x IOP310_PIALR=0x%08x IOP310_PIATVR=%08x\n", *IOP310_PIABAR, *IOP310_PIALR, *IOP310_PIATVR);

	DBG("\tP2P: IOP310_PCR=0x%04x IOP310_BCR=0x%04x IOP310_EBCR=0x%04x\n", *IOP310_PCR, *IOP310_BCR, *IOP310_EBCR);

	/*
	 * Windows have to be carefully opened via a nice set of calls
	 * here or just some direct register fiddling in the board
	 * specific init when we want transactions to occur between the
	 * two PCI hoses.
	 *
	 * To do this, we will have manage RETRY assertion between the
	 * firmware and the kernel.  This will ensure that the host
	 * system's enumeration code is held off until we have tweaked
	 * the interrupt routing and public/private IDSELs.
	 * 
	 * For now we will simply default to disabling the integrated type
	 * 81 P2P bridge.
	 */
	*IOP310_PCR &= 0xfff8;
	
	external_fault = iop310_pci_abort_handler;
	
}

/*
 * This routine builds either a type0 or type1 configuration command.  If the
 * bus is on the 80312 then a type0 made, else a type1 is created.
 */ 
static inline u32 iop310_pci_config_setup(struct pci_dev *dev, int where)
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
static int iop310_pci_config_cleanup(u8 bus)
{
	u32 status = 0, err = 0;
	struct pci_controller *hose;

	hose = pci_bus_to_hose(bus);

	/*
	 * If our hose is using the primary CFGA then check the
	 * primary status registers, otherwise check the relevant
	 * secondary status registers.
	 */ 
	if (hose->cfg_addr == (unsigned int *)IOP310_POCCAR) 
	{
		status = *IOP310_PATUSR;
		if (status & 0xF900)
		{
			DBG("\t\t\tPCI: P0 - status = 0x%08x\n", status);
			err = 1;
			*IOP310_PATUSR = status & 0xF980;
		}
		status = *IOP310_PATUISR;
		if (status & 0x79F) 
		{
			DBG("\t\t\tPCI: P1 - status = 0x%08x\n", status);
			err = 1;
			*IOP310_PATUISR = status & 0x79f;
		}
		status = *IOP310_PSR;
		if (status & 0xF900) 
		{
			DBG("\t\t\tPCI: P2 - status = 0x%08x\n", status);
			err = 1;
			*IOP310_PSR = status & 0xF980;
		}
		status = *IOP310_PBISR;
		if (status & 0x3F) 
		{
			DBG("\t\t\tPCI: P3 - status = 0x%08x\n", status);
			err = 1;
			*IOP310_PBISR = status & 0x3F;
		}
	} 
	else 
	{
		status = *IOP310_SATUSR;
		if (status & 0xF900) 
		{
			DBG("\t\t\tPCI: S0 - status = 0x%08x\n", status);
			err = 1;
			*IOP310_SATUSR = status & 0xF900;
		}
		status = *IOP310_SATUISR;
		if (status & 0x69F) 
		{
			DBG("\t\t\tPCI: S1 - status = 0x%08x\n", status);
			err = 1;
			*IOP310_SATUISR= status & 0x69F;
		}
	}

	return err;
}

/*
 * The read routines must check the error status of the last configuration
 * cycle.  If there was an error, the routine returns all hex f's.
 */ 
static int
iop310_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	u32 *pdata;

	cli();

	pdata = (u32 *)iop310_pci_config_setup(dev, where);

	*value = (u8) (((*pdata) >> ((where % 0x04) * 8)) & 0xff);

	if( iop310_pci_config_cleanup(dev->bus->number) )
	{
		*value = 0xff;
	} /* IF END */

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop310_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	u32 *pdata;

	cli();

	pdata = (u32 *)iop310_pci_config_setup(dev, where);
	*value = (u16) (((*pdata) >> ((where % 0x04) * 8)) & 0xffff);
	if( iop310_pci_config_cleanup(dev->bus->number) )
	{
		*value = 0xffff;
	} /* IF END */

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop310_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	u32 *pdata;
	u8 scratch_pad[5];  /* used to fix dword PCI problem */

	cli();

	pdata = (u32 *)iop310_pci_config_setup(dev, where);
	*value =  *pdata;

	/* Workaround for timing problem errata on 80312 */ 
	sprintf(scratch_pad, "bugs");

	if( iop310_pci_config_cleanup(dev->bus->number) )
	{
		*value = 0xffffffff;
	} /* IF END */

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop310_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	u32 *pdata;
	u32 mask;
	u32 temp;

	cli();

	pdata = (u32 *)iop310_pci_config_setup(dev, where);
	mask = ~(0x000000ff << ((where % 0x4) * 8));
	temp = (u32)(((u32)value) << ((where% 0x4) * 8));
	*pdata = (*pdata & mask) | temp;

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop310_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	u32 *pdata;
	u32 mask;
	u32 temp;

	cli();

	pdata = (u32 *)iop310_pci_config_setup(dev, where);
	mask = ~(0x0000ffff << ((where % 0x4) * 8));
	temp = (u32)(((u32)value) << ((where % 0x4) * 8));
	*pdata = (*pdata & mask) | temp;

	sti();

	return PCIBIOS_SUCCESSFUL;
}

static int
iop310_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	u32 *pdata;

	cli();
	
	pdata = (u32 *)iop310_pci_config_setup(dev, where);
	*pdata = value;

	sti();

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops iop310_ops = {
	iop310_read_config_byte,
	iop310_read_config_word,
	iop310_read_config_dword,
	iop310_write_config_byte,
	iop310_write_config_word,
	iop310_write_config_dword,
};

/*
 * When a PCI device does not exist during config cycles, the 80200 gets a 
 * bus error instead of returning 0xffffffff. This handler simply returns.  
 */ 
int
iop310_pci_abort_handler(unsigned long addr, unsigned long fsr, struct pt_regs *regs) 
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
	return iop310_##rw##_config_##size(fake_pci_dev(hose, bus, devfn),	\
			offset, value);			\
}

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)
