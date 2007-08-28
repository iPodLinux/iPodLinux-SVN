/*
 * arch/arm/mach-ixp425/ixp425-pci.c 
 *
 * IXP425 PCI routines
 *
 * Maintainer: Deepak Saxena
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
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
#include <asm/sizes.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/hardware.h>
#include <asm/arch/pci.h>
#include <asm/arch/pci_auto.h>
#include <asm/arch/pci-bridge.h>

#ifdef DEBUG
#  define DBG(x...) printk(__FILE__": "x)
#else
#  define DBG(x...)
#endif

int (*ixp425_pci_read)(u32 addr, u32 cmd, u32* data);

/*
 * PCI cfg an I/O routines are done by programming a 
 * command/byte enable register, and then read/writing
 * the data from a data regsiter. We need to ensure
 * these transactions are atomic or we will end up
 * with corrupt data on the bus or in a driver.
 */
static spinlock_t ixp425_pci_lock = SPIN_LOCK_UNLOCKED;

/*
 * Read from PCI config space
 */
static void crp_read(u32 ad_cbe, u32 *data)
{
	unsigned long flags;
	spin_lock_irqsave(&ixp425_pci_lock, flags);
	*PCI_CRP_AD_CBE = ad_cbe;
	*data = *PCI_CRP_RDATA;
	spin_unlock_irqrestore(&ixp425_pci_lock, flags);
}

/*
 * Write to PCI config space
 */
static void crp_write(u32 ad_cbe, u32 data)
{ 
	unsigned long flags;
	spin_lock_irqsave(&ixp425_pci_lock, flags);
	*PCI_CRP_AD_CBE = CRP_AD_CBE_WRITE | ad_cbe;
	*PCI_CRP_WDATA = data;
	spin_unlock_irqrestore(&ixp425_pci_lock, flags);
}

static int local_read_config_byte(int where, u8 *value)
{ 
	u32 n, data;
	DBG("local_read_config_byte from %d\n", where);
	n = where % 4;
	crp_read(where & ~3, &data);
	*value = data >> (8*n);
	DBG("local_read_config_byte read %#x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}

static int local_read_config_word(int where, u16 *value)
{
	u32 n, data;
	DBG("local_read_config_word from %d\n", where);
	n = where % 4;
	crp_read(where & ~3, &data);
	*value = data >> (8*n);
	DBG("local_read_config_word read %#x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}

static int local_read_config_dword(int where, u32 *value)
{
	DBG("local_read_config_dword from %d\n", where);
	crp_read(where & ~3, value);
	DBG("local_read_config_dword read %#x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}

static int local_write_config_byte(int where, u8 value)
{
	u32 n, byte_enables, data;
	DBG("local_write_config_byte %#x to %d\n", value, where);
	n = where % 4;
	byte_enables = (0xf & ~BIT(n)) << CRP_AD_CBE_BESL;
	data = value << (8*n);
	crp_write((where & ~3) | byte_enables, data);
	return PCIBIOS_SUCCESSFUL;
}

static int local_write_config_word(int where, u16 value)
{ 
	u32 n, byte_enables, data;
	DBG("local_write_config_word %#x to %d\n", value, where);
	n = where % 4;
	byte_enables = (0xf & ~(BIT(n) | BIT(n+1))) << CRP_AD_CBE_BESL;
	data = value << (8*n);
	crp_write((where & ~3) | byte_enables, data);
	return PCIBIOS_SUCCESSFUL;
}

static int local_write_config_dword(int where, u32 value)
{
	DBG("local_write_config_dword %#x to %d\n", value, where);
	crp_write(where & ~3, value);
	return PCIBIOS_SUCCESSFUL;
}

static inline int check_master_abort(void)
{
	/* check Master Abort bit after access */
	unsigned long isr = *PCI_ISR;

	if (isr & PCI_ISR_PFE)
	{
		/* make sure the Master Abort bit is reset */    
		*PCI_ISR = PCI_ISR_PFE;
		DBG(__FUNCTION__" failed\n");
		return 1;
	}

	return 0;
}

int ixp425_pci_read_errata(u32 addr, u32 cmd, u32* data)
{
	unsigned long flags;
	int retval = 0;
	int i;

	spin_lock_irqsave(&ixp425_pci_lock, flags);

	*PCI_NP_AD = addr;

	/* 
	 * PCI workaround  - only works if NP PCI space reads have 
	 * no side effects!!! Read 8 times. last one will be good.
	 */
	for (i = 0; i < 8; i++)
	{
		*PCI_NP_CBE = cmd;
		*data = *PCI_NP_RDATA;
		*data = *PCI_NP_RDATA;
	}

	if(check_master_abort())
		retval = 1;

	spin_unlock_irqrestore(&ixp425_pci_lock, flags);
	return retval;
}

int ixp425_pci_read_no_errata(u32 addr, u32 cmd, u32* data)
{
	unsigned long flags;
	int retval = 0;

	spin_lock_irqsave(&ixp425_pci_lock, flags);

	*PCI_NP_AD = addr;

	/* set up and execute the read */    
	*PCI_NP_CBE = cmd;

	/* the result of the read is now in NP_RDATA */
	*data = *PCI_NP_RDATA; 

	if(check_master_abort())
		retval = 1;

	spin_unlock_irqrestore(&ixp425_pci_lock, flags);
	return retval;
}

int ixp425_pci_write(u32 addr, u32 cmd, u32 data)
{    
	unsigned long flags;
	int retval = 0;

	spin_lock_irqsave(&ixp425_pci_lock, flags);

	*PCI_NP_AD = addr;

	/* set up the write */
	*PCI_NP_CBE = cmd;

	/* execute the write by writing to NP_WDATA */
	*PCI_NP_WDATA = data;

	if(check_master_abort())
		retval = 1;

	spin_unlock_irqrestore(&ixp425_pci_lock, flags);
	return retval;
}

static u32 ixp425_config_addr(u8 bus_num, u16 devfn, int where)
{
	u32 addr;
	if (!bus_num)
	{
		/* type 0 */
		addr = BIT(32-PCI_SLOT(devfn)) | ((PCI_FUNC(devfn)) << 8) | 
		    (where & ~3);	
	}
	else
	{
		/* type 1 */
		addr = (bus_num << 16) | ((PCI_SLOT(devfn)) << 11) | 
			((PCI_FUNC(devfn)) << 8) | (where & ~3) | 1;
	}
	return addr;
}

static int read_config_byte(u8 bus_num, u16 devfn, int where, u8 *value)
{
	u32 n, byte_enables, addr, data;
	DBG("read_config_byte from %d dev %d:%d:%d\n", where, bus_num, 
		PCI_SLOT(devfn), PCI_FUNC(devfn));
	n = where % 4;
	byte_enables = (0xf & ~BIT(n)) << 4;
	addr = ixp425_config_addr(bus_num, devfn, where);
	if (ixp425_pci_read(addr, byte_enables | NP_CMD_CONFIGREAD, &data))
	{
		*value = 0xff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	*value = data >> (8*n);
	DBG("read_config_byte read %#x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}

static int read_config_word(u8 bus_num, u16 devfn, int where, u16 *value)
{
	u32 n, byte_enables, addr, data;
	DBG("read_config_word from %d dev %d:%d:%d\n", where, bus_num, 
		PCI_SLOT(devfn), PCI_FUNC(devfn));
	n = where % 4;
	byte_enables = (0xf & ~(BIT(n) | BIT(n+1))) << 4;
	addr = ixp425_config_addr(bus_num, devfn, where);
	if (ixp425_pci_read(addr, byte_enables | NP_CMD_CONFIGREAD, &data))
	{
		*value = 0xffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	*value = data >> (8*n);
	DBG("read_config_word read %#x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}


static int read_config_dword(u8 bus_num, u16 devfn, int where, u32 *value)
{
	u32 addr;
	DBG("read_config_dword from %d dev %d:%d:%d\n", where, bus_num, 
		PCI_SLOT(devfn), PCI_FUNC(devfn));
	addr = ixp425_config_addr(bus_num, devfn, where);
	if (ixp425_pci_read(addr, NP_CMD_CONFIGREAD, value))
	{
		*value = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	DBG("read_config_dword read %#x\n", *value);
	return PCIBIOS_SUCCESSFUL;
}

static int write_config_byte(u8 bus_num, u16 devfn, int where, u8 value)
{
	u32 n, byte_enables, addr, data;
	DBG("write_config_byte %#x to %d dev %d:%d:%d\n", value, where, bus_num, 
		PCI_SLOT(devfn), PCI_FUNC(devfn));
	n = where % 4;
	byte_enables = (0xf & ~BIT(n)) << 4;
	addr = ixp425_config_addr(bus_num, devfn, where);
	data = value << (8*n);
	if (ixp425_pci_write(addr, byte_enables | NP_CMD_CONFIGWRITE, data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

static int write_config_word(u8 bus_num, u16 devfn, int where, u16 value)
{
	u32 n, byte_enables, addr, data;
	DBG("write_config_word %#x to %d dev %d:%d:%d\n", value, where, bus_num, 
		PCI_SLOT(devfn), PCI_FUNC(devfn));
	n = where % 4;
	byte_enables = (0xf & ~(BIT(n) | BIT(n+1))) << 4;
	addr = ixp425_config_addr(bus_num, devfn, where);
	data = value << (8*n);
	if (ixp425_pci_write(addr, byte_enables | NP_CMD_CONFIGWRITE, data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

static int write_config_dword(u8 bus_num, u16 devfn, int where, u32 value)
{
	u32 addr;
	DBG("write_config_dword %#x to %d dev %d:%d:%d\n", value, where, bus_num, 
		PCI_SLOT(devfn), PCI_FUNC(devfn));
	addr = ixp425_config_addr(bus_num, devfn, where);
	if (ixp425_pci_write(addr, NP_CMD_CONFIGWRITE, value))
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}


/*
 * We have different access functions for local (IXP425) and 
 * bus devices.  This macro nicely hides that fact from the 
 * layers above.
 */
#define IXP425_PCI_OP(rw, size, type)					\
static int ixp425_##rw##_config_##size(struct pci_dev *dev, int where,	\
	type value)							\
{									\
	if (!dev->bus->number && !PCI_SLOT(dev->devfn))			\
		return local_##rw##_config_##size(where, value);	\
	else								\
		return rw##_config_##size(dev->bus->number, dev->devfn, where, value); \
}

IXP425_PCI_OP(read, byte, u8 *)
IXP425_PCI_OP(read, word, u16 *)
IXP425_PCI_OP(read, dword, u32 *)
IXP425_PCI_OP(write, byte, u8)
IXP425_PCI_OP(write, word, u16)
IXP425_PCI_OP(write, dword, u32)

struct pci_ops ixp425_ops = 
{
	ixp425_read_config_byte,
	ixp425_read_config_word,
	ixp425_read_config_dword,
	ixp425_write_config_byte,
	ixp425_write_config_word,
	ixp425_write_config_dword,
};

/*
 * PCI abort handler
 */
static int abort_handler(unsigned long addr, unsigned long fsr, struct pt_regs *regs)
{
	u32 isr;
	u16 status;

	isr = *PCI_ISR;
	local_read_config_word(PCI_STATUS, &status);
	printk("!!!abort_handler addr = %#lx, isr = %#x, status = %#x\n", addr, 
		isr, status);

	/* make sure the Master Abort bit is reset */    
	*PCI_ISR = PCI_ISR_PFE;
	status |= PCI_STATUS_REC_MASTER_ABORT;
	local_write_config_word(PCI_STATUS,status);

	/*
	 * If it was an imprecise abort, then we need to correct the
	 * return address to be _after_ the instruction.
	 */
	if (fsr & (1 << 10))
		regs->ARM_pc += 4;

	return 0;
}

extern int (*external_fault)(unsigned long addr, struct pt_regs *regs);

static struct pci_controller *hose;

int __init ixp425_pci_is_host(void)
{
	u32 csr;
	
	csr = *PCI_CSR;
	return csr & PCI_CSR_HOST;
}

void __init ixp425_pci_init(void *sysdata)
{  
	unsigned long processor_id;

	asm("mrc p15, 0, %0, cr0, cr0, 0;" : "=r"(processor_id) :);

#if CONFIG_IXP425_PCI_ERRATA
	if(!(processor_id & 0xf)) {
		printk("IXP425 PCI: A0 silicon detected - PCI Non-Prefetch Workaround Enabled\n");
		ixp425_pci_read = ixp425_pci_read_errata;
	} else
		ixp425_pci_read = ixp425_pci_read_no_errata;
#else
	ixp425_pci_read = ixp425_pci_read_no_errata;
#endif

	/* hook in our fault handler for PCI errors */
	external_fault = abort_handler;

	/* 
	 * We use identity AHB->PCI address translation
	 * in the 0x48000000 address space
	 */
	DBG("setup PCI-AHB(inbound) and AHB-PCI(outbound) address mappings\n");
	*PCI_PCIMEMBASE = 0x48494A4B;

	/* 
	 * We also use identity PCI->AHB address translation
	 * in 4 16MB BARs that begin at the physical memory start
	 */
	*PCI_AHBMEMBASE = (PHYS_OFFSET & 0xFF000000) + 
		((PHYS_OFFSET & 0xFF000000) >> 8) +
		((PHYS_OFFSET & 0xFF000000) >> 16) +
		((PHYS_OFFSET & 0xFF000000) >> 24) +
		0x00010203;

	if (ixp425_pci_is_host())
	{
		DBG("setup BARs in controller\n");

		/*
		 * We configure the PCI inbound memory windows to be 1:1 mapped to SDRAM
		 */
		local_write_config_dword(PCI_BASE_ADDRESS_0, PHYS_OFFSET + 0x00000000);
		local_write_config_dword(PCI_BASE_ADDRESS_1, PHYS_OFFSET + 0x01000000);
		local_write_config_dword(PCI_BASE_ADDRESS_2, PHYS_OFFSET + 0x02000000);
		local_write_config_dword(PCI_BASE_ADDRESS_3, PHYS_OFFSET + 0x03000000);

		/*
		 * Disable our CSR and I/O windows from responding
		 * to anything. Since we're the master, no one should
		 * be fiddling with our internals.
		 */
		local_write_config_dword(PCI_BASE_ADDRESS_4, 0xffffffff);
		local_write_config_dword(PCI_BASE_ADDRESS_5, 0xffffffff);
	}

	DBG("clear error bits in ISR\n");
	*PCI_ISR = PCI_ISR_PSE | PCI_ISR_PFE | PCI_ISR_PPE | PCI_ISR_AHBE;

	/*
	 * Set Initialize Complete in PCI Control Register: allow IXP425 to
	 * respond to PCI configuration cycles. Specify that the AHB bus is
	 * operating in big endian mode. Set up byte lane swapping between 
	 * little-endian PCI and the big-endian AHB bus 
	 */
#ifdef __ARMEB__
	*PCI_CSR = PCI_CSR_IC | PCI_CSR_ABE | PCI_CSR_PDS | PCI_CSR_ADS;
#else
	*PCI_CSR = PCI_CSR_IC;
#endif

	if (ixp425_pci_is_host())
	{
		local_write_config_word(PCI_COMMAND, PCI_COMMAND_MASTER | 
			PCI_COMMAND_MEMORY);

		DBG("allocating hose\n");
		hose = pcibios_alloc_controller();
		if (!hose)
		panic("Could not allocate PCI hose");

		hose->first_busno = 0;
		hose->last_busno = 0;
		hose->io_space.start = 0;
		hose->io_space.end = 0xffffffff;
		hose->mem_space.start = 0x48000000;
		hose->mem_space.end = 0x4bffffff;

		/* autoconfig the bus */ DBG("AUTOCONFIG\n");
		hose->last_busno = pciauto_bus_scan(hose, 0);
	    
		/* scan the bus */
		DBG("SCANNING THE BUS\n");
		pci_scan_bus(0, &ixp425_ops, sysdata);
	}

	DBG("DONE\n");
}

#define EARLY_PCI_OP(rw, size, type)                                    \
int early_##rw##_config_##size(struct pci_controller *hose, int bus,    \
	int devfn, int offset, type value)                                  \
{                                                                       \
	return ixp425_##rw##_config_##size(fake_pci_dev(hose, bus, devfn),  \
		offset, value);                                                 \
}


EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)

EXPORT_SYMBOL(ixp425_pci_read);
EXPORT_SYMBOL(ixp425_pci_write);

