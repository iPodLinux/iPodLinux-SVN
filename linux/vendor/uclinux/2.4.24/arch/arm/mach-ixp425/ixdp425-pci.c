/*
 * arch/arm/mach-ixdp425/ixdp425-pci.c 
 *
 * IXDP425 PCI initialization
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/mach/pci.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/arch/pci.h>


/* PCI controller pin mappings */
#define INTA_PIN	IXP425_GPIO_PIN_11
#define INTB_PIN	IXP425_GPIO_PIN_10
#define	INTC_PIN	IXP425_GPIO_PIN_9
#define	INTD_PIN	IXP425_GPIO_PIN_8

#define IXP425_PCI_RESET_GPIO   IXP425_GPIO_PIN_13
#define IXP425_PCI_CLK_PIN      IXP425_GPIO_CLK_0
#define IXP425_PCI_CLK_ENABLE   IXP425_GPIO_CLK0_ENABLE
#define IXP425_PCI_CLK_TC_LSH   IXP425_GPIO_CLK0TC_LSH
#define IXP425_PCI_CLK_DC_LSH   IXP425_GPIO_CLK0DC_LSH

void __init ixdp425_pci_init(void *sysdata)
{
	gpio_line_config(INTA_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTB_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTC_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTD_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

	gpio_line_isr_clear(INTA_PIN);
	gpio_line_isr_clear(INTB_PIN);
	gpio_line_isr_clear(INTC_PIN);
	gpio_line_isr_clear(INTD_PIN);

	ixp425_pci_init(sysdata);
}


/*
 * Interrupt mapping
 */
#define INTA			IRQ_IXDP425_PCI_INTA
#define INTB			IRQ_IXDP425_PCI_INTB
#define INTC			IRQ_IXDP425_PCI_INTC
#define INTD			IRQ_IXDP425_PCI_INTD

#define IXP425_PCI_MAX_DEV      4
#define IXP425_PCI_IRQ_LINES    4

static int __init ixdp425_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	static int pci_irq_table[IXP425_PCI_MAX_DEV][IXP425_PCI_IRQ_LINES] = 
	{
		{INTA, INTB, INTC, INTD},
		{INTB, INTC, INTD, INTA},
		{INTC, INTD, INTA, INTB},
		{INTD, INTA, INTB, INTC}
	};

	int irq = -1;

	if (slot >= 1 && slot <= IXP425_PCI_MAX_DEV && 
		pin >= 1 && pin <= IXP425_PCI_IRQ_LINES)
	{
		irq = pci_irq_table[slot-1][pin-1];
	}

	return irq;
}

struct hw_pci ixdp425_pci __initdata = {
	init:		ixdp425_pci_init,
	swizzle:	common_swizzle,
	map_irq:	ixdp425_map_irq,
};

