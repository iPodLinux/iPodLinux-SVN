/*
 * arch/arm/mach-iop310/pci_auto.h
 * 
 * PCI autoconfiguration library definitions
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

#ifndef _ARM_MACH_XSCALE_PCI_AUTO_H
#define _ARM_MACH_XSCALE_PCI_AUTO_H

#include "pci-bridge.h"

extern int pciauto_bus_scan(struct pci_controller *, int);

#define	PCIAUTO_IDE_MODE_MASK		0x05

#endif /* _ARM_MACH_XSCALE_PCI_AUTO_H */
