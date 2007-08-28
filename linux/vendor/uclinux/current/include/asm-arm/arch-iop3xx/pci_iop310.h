/*
 * include/asm-arm/arch-iop310/pci_iop310.h
 * 
 * IOP310 PCI support.
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
#ifndef _PCI_IOP310_H
#define _PCI_IOP310_H

extern struct pci_ops iop310_ops;
extern void iop310_init(void);

#endif /* _PCI_IOP310_H */
