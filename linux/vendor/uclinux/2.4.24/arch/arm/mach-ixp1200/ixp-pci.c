/*
 * arch/arm/mach-ixp1200/ixp1200-pci.c: 
 *
 * Generic PCI support for IXP1200 based systems
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2001 MontaVista Software, Inc.
 * Copyright (C) 1998-2000 Russell King, Phil Blundell
 *
 * May-27-2000: Uday Naik
 * 	Initial port to IXP1200 based on dec21285 code.
 *
 * Sep-25-2001: dsaxena
 * 	Port to 2.4.x kernel/massive rewrite
 *
 * TODO: Get rid of pci-auto
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>

#include <asm/byteorder.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/mach-types.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci-auto.h>
#include <asm/arch/pci-bridge.h>

#define MAX_SLOTS		14

static unsigned long
ixp1200_base_address(struct pci_dev *dev, int where)
{
	unsigned long addr = 0;
	unsigned int devfn = dev->devfn;

	if(dev->bus->number == 0)
	{
		if(PCI_SLOT(devfn) == 0) {
			addr = PCI_CSR_BASE +  (where & ~0x3);
		}
		else if (devfn < PCI_DEVFN(MAX_SLOTS, 0))

			addr = PCICFG0_BASE | (1 << PCI_SLOT(devfn) + 10) |
				(PCI_FUNC(devfn) << 8) | (where & 0xFF);
	}
	else
	{
		addr = PCICFG1_BASE | (dev->bus->number << 16) | (devfn <<8) | where;
	}

	return addr;
}

static int
ixp1200_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if(!addr)
		*value = 0xff;
	else if((addr & PCI_CSR_BASE) != PCI_CSR_BASE)
        	*value = *(u8 *)addr;
	else {
		u32 *local_addr = (u32*)addr;
		*value = (u8)(((*local_addr) >> ((where % 0x04) * 8)) & 0xff);
	}

	return PCIBIOS_SUCCESSFUL;

}

static int
ixp1200_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if(!addr)
		*value = 0xffff;
	else if((addr & PCI_CSR_BASE) != PCI_CSR_BASE)
		*value = __le16_to_cpu((*(u16 *)addr));
	else 
	{
		u32 *local_addr = (u32*)addr;
		*value = (u16)(((*local_addr) >> ((where % 0x04) * 8)) & 0xffff);
	}

	return PCIBIOS_SUCCESSFUL;

}

static int
ixp1200_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if(!addr)
		*value = 0xffffffff;
	else if((addr & PCI_CSR_BASE) != PCI_CSR_BASE)
		*value = __le32_to_cpu((*(u32 *)addr));
	else
		*value = *((u32 *)addr);

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp1200_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if((addr & PCI_CSR_BASE) != PCI_CSR_BASE) {
		*(u8*)addr = value;
	} else {
		u32 mask = ~(0xff << ((where % 0x4) * 8));
		u32 temp = value << ((where % 0x4) * 8);
		u32 *local_addr = (u32*)addr;

		*local_addr = (*(local_addr) & mask) | temp;
	}	

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp1200_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if((addr & PCI_CSR_BASE) != PCI_CSR_BASE) {
        	*(u16 *)addr= __cpu_to_le16(value);
	} else {
		u32 mask = ~(0xffff << ((where % 0x4) * 8));
		u32 temp = (u32)(((u32)value) << ((where % 0x4) * 8));
		u32 *local_addr = (u32*)addr;

		*local_addr = (*local_addr & mask) | temp;
	}	

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp1200_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if((addr & PCI_CSR_BASE) != PCI_CSR_BASE)
        	*(u32 *)addr = __cpu_to_le32(value);
	else
        	*(u32 *)addr = value;

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops ixp1200_ops = {
	ixp1200_read_config_byte,
	ixp1200_read_config_word,
	ixp1200_read_config_dword,
	ixp1200_write_config_byte,
	ixp1200_write_config_word,
	ixp1200_write_config_dword,
};

void __init ixp1200_setup_resources(struct resource **resource)
{
	struct resource *busmem, *busmempf;

	busmem = kmalloc(sizeof(*busmem), GFP_KERNEL);
	busmempf = kmalloc(sizeof(*busmempf), GFP_KERNEL);
	memset(busmem, 0, sizeof(*busmem));
	memset(busmempf, 0, sizeof(*busmempf));

	busmem->flags = IORESOURCE_MEM;
	busmem->name  = "PCI non-prefetch";

	allocate_resource(&iomem_resource, busmem, 0x20000000,
			  0x60000000, 0xffffffff, 0x20000000, NULL, NULL);

	resource[0] = &ioport_resource;
	resource[1] = busmem;
}

static struct pci_controller *hose = NULL;

static int ixp_host = 0;

static void report_pci_dev_error(void)
{
	struct pci_dev *dev;

	pci_for_each_dev(dev) 
	{
		unsigned short status;

		pci_read_config_word(dev, PCI_STATUS, &status);

		if(status & 0xf900)
		{
			printk(KERN_ERR "    Bus %d:DevFn %04x Status = %x\n",
					dev->bus->number, dev->devfn,
					status & 0xf900);
		}
	}
}

static void pci_err_irq(int irqno, void *notused, struct pt_regs *regs)
{
	static unsigned long next_warn = 0;

	unsigned long cmd = *CSR_PCICMD & 0x0000ffff;
	unsigned long ctrl = *CSR_SA_CONTROL & 0xffffdeff;
	unsigned long irqstatus = *CSR_IRQ_RAWSTATUS;

	int warn = time_after_eq(jiffies, next_warn);
	if(warn)
	{
		next_warn = jiffies + 3 * HZ;
	}

	if(irqstatus & IRQ_MASK_DPE)
	{
		if(warn)
			printk(KERN_ERR "IXP1200 PCI: Detected Parity Error\n");
		cmd |= PCI_STATUS_DETECTED_PARITY << 16;
	}

	if(irqstatus & IRQ_MASK_RTA)
	{
		if(warn)
			printk(KERN_ERR "IXP1200 PCI: Received Target Abort\n");
		cmd |= PCI_STATUS_REC_TARGET_ABORT << 16;
	}

	if(irqstatus & IRQ_MASK_RMA)
	{
		if(warn)
			printk(KERN_ERR "IXP1200 PCI: Received Master Abort\n");
		cmd |= PCI_STATUS_REC_MASTER_ABORT << 16;
	}

	if(irqstatus & IRQ_MASK_DPED)
	{	
		if(warn)
			printk(KERN_ERR "IXP1200 PCI: Data Parity Error\n");
		cmd |= PCI_STATUS_PARITY << 16;
	}

	if(irqstatus & IRQ_MASK_DTE)
	{
		if(warn)
			printk(KERN_ERR "IXP1200 PCI: Discard timer Expired\n");
		ctrl &= ~SA_CNTL_DISCARDTIMER;
	}

	if(irqstatus & IRQ_MASK_RSERR)
	{
		printk(KERN_ERR "IXP1200 PCI: Received System Error\n");
		warn = 1;
	}

	/*
	 * Only dump PCI error status if we are the host
	 */
	if(warn && ixp_host) report_pci_dev_error(); 

	*CSR_PCICMD = cmd;
	cmd = *CSR_PCICMD;

	*CSR_SA_CONTROL = ctrl;
	ctrl = *CSR_SA_CONTROL;
}

void __init ixp1200_pci_init(struct arm_pci_sysdata *sysdata)
{
	u32 dummy_read;
	unsigned int pci_cmd = 	PCI_COMMAND_IO |
				PCI_COMMAND_MEMORY |
				PCI_COMMAND_MASTER |
				PCI_COMMAND_INVALIDATE;
		
        unsigned int big_endian_cntl   = SA_CNTL_BE_BEO |
					 SA_CNTL_BE_DEO |
					 SA_CNTL_BE_BEI |
                                         SA_CNTL_BE_DEI;
	*CSR_PCIOUTBINTMASK = 0xc;
	dummy_read = *CSR_SA_CONTROL;

	/*
	 * Disable doorbell int to PCI in doorbell mask register
	 */
	*CSR_DOORBELL_PCI = 0; 
	dummy_read = *CSR_SA_CONTROL;

	/*
	 * Disable doorbell int to SA-110 in Doorbell SA-110 mask register
	 */
        *CSR_DOORBELL_SA = 0x0;
	dummy_read = *CSR_SA_CONTROL;

	/* 
	 * Set PCI Address externsion reg to known state
	 * 
	 * We setup a 1:1 map of bus<->physical addresses
	 */
        *CSR_PCIADDR_EXTN = 0x54006000; 
	dummy_read = *CSR_SA_CONTROL;

#ifdef __ARMEB__
	*CSR_SA_CONTROL |= big_endian_cntl; /* set swap bits for big endian*/
	dummy_read = *CSR_SA_CONTROL;
#endif

	/*
	 * We just map in the maximum SDRAM (256MB) into PCI space at 
	 * location 0x00000000.  Even if the board has less RAM or if
	 * Linux is only using part of it, this shouldn't be an issue
	 * as long as drivers are well behaved.
	 */
	*CSR_SDRAMBASEMASK = (256 << 20) - 0x40000;
	dummy_read = *CSR_SA_CONTROL;

 	*CSR_CSRBASEMASK = 0x000c0000; /*1 Mbyte*/
	dummy_read = *CSR_SA_CONTROL;

	/* Clear PCI command register*/
        *CSR_PCICMD &= 0xffff0000; 
	dummy_read = *CSR_SA_CONTROL;

        *CSR_SA_CONTROL |= IXP1200_SA_CONTROL_PNR; /*Negate PCI reset*/
	dummy_read = *CSR_SA_CONTROL;

	udelay(1000);

	/*
	 * Host means whether or not the IXP should go out and
	 * scan the bus. On the IXP12EB, this is determined through
	 * looking at the PCF register.  On certain boards, the PCF
	 * pin is not properly wired, so we can't determine whether
	 * we are the controller or not.  In that case, we can use
	 * the "ixphost" parameter to force the bus scan.
	 */
	if(!ixp_host)
        	ixp_host = IXP1200_SA_CONTROL_PCF & *CSR_SA_CONTROL;

        if(ixp_host)
        {
		printk("PCI: IXP1200 is system controller\n");
		*CSR_PCICSRBASE = 0x40000000;
		dummy_read = *CSR_SA_CONTROL;

		*CSR_PCICSRIOBASE = 0x0000f000;
		dummy_read = *CSR_SA_CONTROL; 

		/*
		 * SDRAM mapped at PCI addr 0
		 */
		*CSR_PCISDRAMBASE = 0; 
		dummy_read = *CSR_SA_CONTROL;

		*CSR_PCICMD = (*CSR_PCICMD & 0xFFFF0000)| pci_cmd;
		dummy_read = *CSR_SA_CONTROL;

        	*CSR_SA_CONTROL |= 0x00000001;
		dummy_read = *CSR_SA_CONTROL;

		*CSR_PCICACHELINESIZE = 0x00002008;
		dummy_read = *CSR_SA_CONTROL;

		hose = pcibios_alloc_controller();
		if(!hose) panic("Could not allocate PCI hose!");

		hose->first_busno = 0;
		hose->last_busno = 0;
		hose->io_space.start = 0x54000000;
		hose->io_space.end = 0x5400ffff;
		hose->mem_space.start = 0x60000000;
		hose->mem_space.end = 0x7fffffff;

		/* Re-Enumarate the bus since we don't trust cygmon */
		hose->last_busno = pciauto_bus_scan(hose, 0);

		/* Scan the bus */
		pci_scan_bus(0, &ixp1200_ops, sysdata);
	}
	else 
	{
		/*
		 * In agent mode we don't have to do anything.
		 * We setup the window sizes above and it's up to
		 * the host to properly configure us.
		 */
		printk("PCI: IXP1200 is agent\n");
	}

	/*
	 * Request error IRQs so that we can report them to user
	 */
	request_irq(IXP1200_IRQ_PCI_ERR, pci_err_irq, 0, "PCI Error", NULL);
}


#define EARLY_PCI_OP(rw, size, type)					\
int early_##rw##_config_##size(struct pci_controller *hose, int bus,	\
		int devfn, int offset, type value)	\
{									\
	return ixp1200_##rw##_config_##size(fake_pci_dev(hose, bus, devfn),	\
			offset, value);			\
}

static u8 __init ixp1200_swizzle(struct pci_dev *dev, u8 *pin)
{
	return PCI_SLOT(dev->devfn);
}

static int __init ixp1200_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	if (dev->vendor == PCI_VENDOR_ID_INTEL &&
		dev->device == PCI_DEVICE_ID_INTEL_IXP1200)
		return 0;

	return IXP1200_IRQ_PCI;
}

struct hw_pci ixp1200_pci __initdata = {
	init: ixp1200_pci_init,
	setup_resources: ixp1200_setup_resources,
	swizzle: ixp1200_swizzle,
	map_irq: ixp1200_map_irq,
};

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)

static int __init ixphost_setup(char *__unused)
{
	ixp_host = 1;
	return 0;
}

static int __init ixpagent_setup(char *__unused)
{
	ixp_host = 0;
	return 0;
}

__setup("ixphost", ixphost_setup);
__setup("ixpagent", ixpagent_setup);

