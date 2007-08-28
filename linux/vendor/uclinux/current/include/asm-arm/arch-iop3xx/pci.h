/*
 * include/asm-arm/arch-xscale/pci.h
 * 
 * Generic PCI support for Xscale/ARM.  To eventually be moved to a
 * more generic location.
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
#ifndef _ASM_ARCH_PCI_H
#define _ASM_ARCH_PCI_H

extern struct pci_controller* pcibios_alloc_controller(void);
extern void pcibios_allocate_resources(void);
extern void pci_exclude_device(unsigned char, unsigned char);
extern struct pci_dev * fake_pci_dev(struct pci_controller *, int, int);
extern u8 common_swizzle(struct pci_dev *, u8 *);

/*
 * The following macro is used to lookup irqs in a standard table
 * format for those systems that do not already have PCI
 * interrupts properly routed.
 */
#define PCI_IRQ_TABLE_LOOKUP                                            \
({ long _ctl_ = -1;                                                     \
   if (idsel >= min_idsel && idsel <= max_idsel && pin <= irqs_per_slot) \
	 _ctl_ = pci_irq_table[idsel - min_idsel][pin-1]; \
   _ctl_; })

#endif /* _ASM_ARCH_PCI_H */
