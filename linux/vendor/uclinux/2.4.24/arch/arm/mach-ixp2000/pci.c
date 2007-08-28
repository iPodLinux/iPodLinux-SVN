/*
 * arch/arm/mach-ixdp2400/pci.c
 *
 * Generic PCI support functionality that should eventually move to
 * arch/arm/kernel/bios32.c.  Derived from support in ppc and alpha.
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright (C) 2001 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/arch/hardware.h>
#include <asm/arch/pci-bridge.h>

#define DEBUG 1 

#ifdef DEBUG
#define  DBG(x...) printk(x)
#else
#define  DBG(x...)
#endif

struct pci_controller* hose_head;
struct pci_controller** hose_tail = &hose_head;

/*
 * The following code swizzles for exactly one bridge.  The routine
 * common_swizzle below handles multiple bridges.  But there are a
 * some boards that don't follow the PCI spec's suggestion so we
 * break this piece out separately.
 */
static inline u8
bridge_swizzle(u8 pin, u8 slot)
{
	return (((pin-1) + slot) % 4) + 1;
}

u8 __init
common_swizzle(struct pci_dev *dev, u8 *pinp)
{
	struct pci_controller *hose;
	
	hose = pci_bus_to_hose(dev->bus->number);

	if (dev->bus->number != hose->first_busno) {
		u8 pin = *pinp;
		do {
			pin = bridge_swizzle(pin, PCI_SLOT(dev->devfn));
			/* Move up the chain of bridges. */
			dev = dev->bus->self;
		} while (dev->bus->self);
		*pinp = pin;

		/* The slot is the slot of the last bridge. */
	}

	return PCI_SLOT(dev->devfn);
}

/*
 * Null PCI config access functions, for the case when we can't
 * find a hose.
 */
#define NULL_PCI_OP(rw, size, type)					\
static int								\
null_##rw##_config_##size(struct pci_dev *dev, int offset, type val)	\
{									\
	return PCIBIOS_DEVICE_NOT_FOUND;    				\
}

	NULL_PCI_OP(read, byte, u8 *)
	NULL_PCI_OP(read, word, u16 *)
	NULL_PCI_OP(read, dword, u32 *)
	NULL_PCI_OP(write, byte, u8)
	NULL_PCI_OP(write, word, u16)
NULL_PCI_OP(write, dword, u32)

static struct pci_ops null_pci_ops =
{
	null_read_config_byte,
	null_read_config_word,
	null_read_config_dword,
	null_write_config_byte,
	null_write_config_word,
	null_write_config_dword
};

/*
 * These functions are used early on before PCI scanning is done
 * and all of the pci_dev and pci_bus structures have been created.
 */
struct pci_dev *
fake_pci_dev(struct pci_controller *hose, int busnr, int devfn)
{
	static struct pci_dev dev;
	static struct pci_bus bus;

	if (hose == 0) {
		hose = pci_bus_to_hose(busnr);
		if (hose == 0)
			printk(KERN_ERR "Can't find hose for PCI bus %d!\n", busnr);
	}
	dev.bus = &bus;
	dev.sysdata = hose;
	dev.devfn = devfn;
	bus.number = busnr;
	bus.ops = hose? hose->ops: &null_pci_ops;
	return &dev;
}

static int next_controller_index;

struct pci_controller * __init
pcibios_alloc_controller(void)
{
	struct pci_controller *hose;

	hose = (struct pci_controller *)kmalloc(sizeof(*hose), GFP_KERNEL);
	memset(hose, 0, sizeof(struct pci_controller));
	
	*hose_tail = hose;
	hose_tail = &hose->next;

	hose->index = next_controller_index++;

	return hose;
}
	
struct pci_controller *
pci_bus_to_hose(int bus)
{
	struct pci_controller *hose = hose_head;

	for (; hose; hose = hose->next)
		if (bus >= hose->first_busno && bus <= hose->last_busno)
			return hose;
	return NULL;
}

void __init
pci_exclude_device(unsigned char bus, unsigned char devfn)
{
	struct pci_dev *dev, *pdev = NULL;

	/* Walk the pci device list */
	pci_for_each_dev(dev)
	{
		if ((dev->bus->number == bus) && (dev->devfn == devfn))
		{
			/*
			 * Remove the device from the global and
			 * bus lists.
			 */
			list_del(&(dev->global_list));
			list_del(&(dev->bus_list));

			/* Free the pci_dev structure */
			kfree(dev);

			break;
		}
		pdev = dev;
	}
}

static inline void __init alloc_resource(struct pci_dev *dev, int idx)
{
	struct resource *pr, *r = &dev->resource[idx];

	pr = pci_find_parent_resource(dev, r);
	if(!pr || request_resource(pr, r) < 0)
	{
		printk(KERN_ERR "PCI: Can not allocate resource region %d" \
				" of device %s\n", idx, dev->slot_name);
		r->end -= r->start;
		r->start = 0;
	}
}

void __init
pcibios_allocate_bus_resources(struct list_head *bus_list)
{
	struct list_head *ln;
	struct pci_bus *bus;
	int i;
	struct resource *res, *pr;

	if(!bus_list) return; 

	/* Depth-First Search on bus tree */
	for (ln = bus_list->next; ln != bus_list; ln=ln->next) {
		bus = pci_bus_b(ln);
		pci_read_bridge_bases(bus);
		for (i = 0; i < 4; ++i) {
			if ((res = bus->resource[i]) == NULL || !res->flags)
				continue;
			if (bus->parent == NULL)
				pr = (res->flags & IORESOURCE_IO)?
					&ioport_resource: &iomem_resource;
			else
				pr = pci_find_parent_resource(bus->self, res);

			if (pr && request_resource(pr, res) == 0)
				continue;
			printk(KERN_ERR "PCI: Can not allocate resource region "
			       "%d of PCI bridge %d\n", i, bus->number);
			DBG("PCI: resource is %lx..%lx (%lx)\n",
					res->start, res->end, res->flags);
		}
		pcibios_allocate_bus_resources(&bus->children);
	}
}

void __init pcibios_allocate_resources(void)
{
	struct pci_dev *dev;
	int idx, disabled;
	u16 command;
	struct resource *r;

	pci_for_each_dev(dev)
	{
		pci_read_config_word(dev, PCI_COMMAND, &command);
		for(idx = 0; idx < 6; idx++)
		{
			r = &dev->resource[idx];

			if(r->parent)
				continue;
			if(!r->start)
				continue;

			alloc_resource(dev, idx);
		}
	}
}

