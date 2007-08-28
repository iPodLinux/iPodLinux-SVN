/*
 * include/asm-arm/arch-iop3xx/pci_iop321.h
 * 
 * IOP321 PCI support.
 *
 * Author: Rory Bolt <rorybolt@pacbell.net>
 * Copyright (C) 2002 Rory Bolt
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef _PCI_IOP321_H
#define _PCI_IOP321_H

extern struct pci_ops iop321_ops;
extern void iop321_init(void);

#endif /* _PCI_IOP321_H */
