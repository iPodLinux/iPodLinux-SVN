/*
 * include/asm-arm/arch-iop310/pci-bridge.h
 * 
 * Generic PCI hose support for Xscale/ARM.  To eventually be
 * moved to a more generic location.
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifdef __KERNEL__
#ifndef _ASM_PCI_BRIDGE_H
#define _ASM_PCI_BRIDGE_H

struct device_node;
struct pci_controller;

/* Get the PCI host controller for a bus */
extern struct pci_controller* pci_bus_to_hose(int bus);

/*
 * Structure of a PCI controller (host bridge)
 */
struct pci_controller {
	int index;
	struct pci_controller *next;
        struct pci_bus *bus;
	void *arch_data;

	int first_busno;
	int last_busno;
        
	struct pci_ops *ops;
	volatile unsigned int *cfg_addr;
	volatile unsigned char *cfg_data;

	/* Currently, we limit ourselves to 1 IO range and 3 mem
	 * ranges since the common pci_bus structure can't handle more
	 */
	struct resource	io_resource;
	struct resource mem_resources[3];
	int mem_resource_count;

	/* Host bridge I/O and Memory space
	 * Used for BAR placement algorithms
	 */
	struct resource io_space;
	struct resource mem_space;
};

/* These are used for config access before all the PCI probing
   has been done. */
int early_read_config_byte(struct pci_controller *hose, int bus, int dev_fn, int where, u8 *val);
int early_read_config_word(struct pci_controller *hose, int bus, int dev_fn, int where, u16 *val);
int early_read_config_dword(struct pci_controller *hose, int bus, int dev_fn, int where, u32 *val);
int early_write_config_byte(struct pci_controller *hose, int bus, int dev_fn, int where, u8 val);
int early_write_config_word(struct pci_controller *hose, int bus, int dev_fn, int where, u16 val);
int early_write_config_dword(struct pci_controller *hose, int bus, int dev_fn, int where, u32 val);
#endif
#endif /* __KERNEL__ */
