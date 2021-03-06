/*
 *  linux/arch/arm/mach-integrator/pci-integrator.c
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  PCI functions for Integrator
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>

/* 
 * A small note about bridges and interrupts.  The DECchip 21050 (and
 * later) adheres to the PCI-PCI bridge specification.  This says that
 * the interrupts on the other side of a bridge are swizzled in the
 * following manner:
 *
 * Dev    Interrupt   Interrupt 
 *        Pin on      Pin on 
 *        Device      Connector
 *
 *   4    A           A
 *        B           B
 *        C           C
 *        D           D
 * 
 *   5    A           B
 *        B           C
 *        C           D
 *        D           A
 *
 *   6    A           C
 *        B           D
 *        C           A
 *        D           B
 *
 *   7    A           D
 *        B           A
 *        C           B
 *        D           C
 *
 * Where A = pin 1, B = pin 2 and so on and pin=0 = default = A.
 * Thus, each swizzle is ((pin-1) + (device#-4)) % 4
 *
 * The following code swizzles for exactly one bridge.  
 */
static inline int bridge_swizzle(int pin, unsigned int slot) 
{
	return (pin + slot) & 3;
}

/*
 * This routine handles multiple bridges.
 */
static u8 __init integrator_swizzle(struct pci_dev *dev, u8 *pinp)
{
	int pin = *pinp;

	if (pin == 0)
		pin = 1;

	pin -= 1;
	while (dev->bus->self) {
		pin = bridge_swizzle(pin, PCI_SLOT(dev->devfn));
		/*
		 * move up the chain of bridges, swizzling as we go.
		 */
		dev = dev->bus->self;
	}
	*pinp = pin + 1;

	return PCI_SLOT(dev->devfn);
}

static int irq_tab[4] __initdata = {
	IRQ_PCIINT0,	IRQ_PCIINT1,	IRQ_PCIINT2,	IRQ_PCIINT3
};

/*
 * map the specified device/slot/pin to an IRQ.  This works out such
 * that slot 9 pin 1 is INT0, pin 2 is INT1, and slot 10 pin 1 is INT1.
 */
static int __init integrator_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	int intnr = ((slot - 9) + (pin - 1)) & 3;

	return irq_tab[intnr];
}

extern void pci_v3_init(struct arm_pci_sysdata *);

struct hw_pci integrator_pci __initdata = {
	init:		pci_v3_init,
	swizzle:	integrator_swizzle,
	map_irq:	integrator_map_irq,
};
