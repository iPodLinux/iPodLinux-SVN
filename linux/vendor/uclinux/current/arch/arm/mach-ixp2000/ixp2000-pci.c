/*
 * arch/arm/mach-ixp2000/ixp2000-pci.c
 *
 * PCI routines for IXDP2400/IXDP2800 boards
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
#include <asm/mach-types.h>
#include <asm/hardware.h>

#include <asm/arch/pci.h>
//#include <asm/arch/pci_auto.h>
#include <asm/arch/pci-bridge.h>

extern int (*external_fault)(unsigned long, struct pt_regs *);

static int pci_master_aborts = 0;

// #define DEBUG 1

#ifdef DEBUG 
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif /* DEBUG */


int clear_master_aborts(void);

static u32* 
ixp2000_pci_config_setup(struct pci_dev *dev, int where)
{
	u32 *paddress;

	if ( PCI_SLOT(dev->devfn) > 7 ) 
		return 0;

	/* Must be dword aligned */

	where &= ~3;

	/*
	 * For top bus, generate type 0, else type 1
	 */
	if(!dev->bus->number) {
		/* only bits[23:16] are used for IDSEL */
		paddress = (u32 *)(PCI_CFG0_VIRT_BASE 
				| (1 << (PCI_SLOT(dev->devfn)+16)) 
				| (PCI_FUNC(dev->devfn) << 8) | where);
	} else {
		paddress = (u32 *)(PCI_CFG1_VIRT_BASE 
				| (dev->bus->number << 16) 
				| (PCI_SLOT(dev->devfn) << 11) 
				| (PCI_FUNC(dev->devfn) << 8) | where | 0);
	}

	return paddress;
}

static int
ixp2000_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	u32 *address;

	DBG("In config_read_byte %d from dev %d:%d:%d\n", where, 
			dev->bus->number, PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn));

	address = ixp2000_pci_config_setup(dev, where);

	if ( !address ) { 
		*value = 0xff;
		return PCIBIOS_DEVICE_NOT_FOUND;

	}

	*value = (u8)(((*address) >> ((where % 0x04) * 8)) & 0xff);
	if (pci_master_aborts) {
		pci_master_aborts = 0;
		*value = 0xff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp2000_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	u32 *address;

	DBG("config_read_word %d from dev %d:%d:%d\n", where, 
			dev->bus->number, PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn));

	address = ixp2000_pci_config_setup(dev, where);

	if ( !address ) { 
		*value = 0xffff;
		return PCIBIOS_DEVICE_NOT_FOUND;

	}
	*value = (u16)(((*address) >> ((where % 0x04) *8)) & 0xffff);
	
	if (pci_master_aborts) {
		pci_master_aborts = 0;
		*value = 0xffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp2000_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	u32 *address;

	DBG("config_read_dword %d from dev %d:%d:%d\n", where, 
			dev->bus->number, PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn));

	address = ixp2000_pci_config_setup(dev, where);

	if ( !address ) { 
		*value = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;

	}
	*value = *address;
	
	if (pci_master_aborts) {
		pci_master_aborts = 0;
		*value = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}


	return PCIBIOS_SUCCESSFUL;
}

/*
 * We don't do error checking on the address for writes.
 * It's assumed that the user checked for the device existing first
 * by doing a read first.
 */
static int 
ixp2000_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	u32 *address;
	u32 mask;
	u32 temp;

	address = ixp2000_pci_config_setup(dev, where);

	mask = ~(0x000000ff << ((where % 0x4) * 8));
	temp = (u32)(((u32)value) << ((where % 0x4) *8));
	*address = (*address & mask) | temp;

	clear_master_aborts(); 

	return PCIBIOS_SUCCESSFUL;
}

static int 
ixp2000_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	u32 *address;
	u32 mask;
	u32 temp;

	address = ixp2000_pci_config_setup(dev, where);

	mask = ~(0x0000ffff << ((where % 0x4) * 8));
	temp = (u32)(((u32)value) << ((where % 0x4) * 8));
	*address = (*address & mask) | temp;

	clear_master_aborts(); 

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp2000_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	u32 *address;

	address = ixp2000_pci_config_setup(dev, where);

	*address = value;
	clear_master_aborts(); 

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops ixp2000_ops = {
	ixp2000_read_config_byte,
	ixp2000_read_config_word,
	ixp2000_read_config_dword,
	ixp2000_write_config_byte,
	ixp2000_write_config_word,
	ixp2000_write_config_dword,
};

int
ixp2000_pci_abort_handler(unsigned long addr, struct pt_regs *regs) 
{
	
      volatile u32 temp;

      pci_master_aborts = 1;

      cli();
      temp = *(IXP2000_PCI_CONTROL);
      if (temp & ((1<<8) | (1<<5))) {  /* master abort and cmd tgt err */
              *(IXP2000_PCI_CONTROL) = temp;
      }

      temp = *(IXP2000_PCI_CMDSTAT);
     if (temp & (1<<29) ) {
      		while (temp & (1<<29) ) {/* Rx_MA recv abort with master*/
              		*(IXP2000_PCI_CMDSTAT) = temp;
      			temp = *(IXP2000_PCI_CMDSTAT);
		}
      }
     sti();

	return 0;
}


int clear_master_aborts(void)
{
	volatile u32 temp;

	cli();
	temp = *(IXP2000_PCI_CONTROL);
	if (temp & ((1<<8) | (1<<5))) {  /* master abort and cmd tgt err */
		*(IXP2000_PCI_CONTROL) = temp;
	}

	temp = *(IXP2000_PCI_CMDSTAT);
	if (temp & (1<<29) ) {
		while (temp & (1<<29) ) {/* Rx_MA recv abort with master*/
			*(IXP2000_PCI_CMDSTAT) = temp;
			temp = *(IXP2000_PCI_CMDSTAT);
		}
	}
	sti();

	return 0;
}

void __init ixp2000_pci_init(void *sysdata)
{
	external_fault = ixp2000_pci_abort_handler;
	
	DBG("PCI: set upper mem io bits\n");
	*IXP2000_PCI_ADDR_EXT = 0x0;  

	if (npu_is_master()) {/* master NPU */
		pci_scan_bus(0, &ixp2000_ops, sysdata);
	} else { /* NPU is slave */

		/*
		 * NOTE TO HW DESIGNERS:
		 *
		 * Do NOT make boards like this which have slave devices that scan
		 * the bus and own devices. It is completely against the PCI spec
		 * as it is _NOT_ a peer to peer bus.
		 */
		if(machine_is_ixdp2400())
			pci_scan_bus(0, &ixp2000_ops, sysdata);
	}

	/* enable PCI INTB */
 	*(IXP2000_IRQ_ENABLE_SET) = (1<<15);
	*IXP2000_PCI_XSCALE_INT_ENABLE |= (1<<27) | (1<<26);

	DBG("ixp200_pci_init Done\n");
}

#define EARLY_PCI_OP(rw, size, type)					\
int early_##rw##_config_##size(struct pci_controller *hose, int bus,	\
		int devfn, int offset, type value)	\
{									\
	return ixp2000_##rw##_config_##size(fake_pci_dev(hose, bus, devfn),	\
			offset, value);			\
}

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)

