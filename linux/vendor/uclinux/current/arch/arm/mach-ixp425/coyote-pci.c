/*
 *  coyote-pci.c
 *
 *  Copyright (C) 2002 Jungo Software Technologies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/mach/pci.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pci.h>
#include <asm/arch/gpio.h>
#include <asm/arch/pci.h>

#define INTA    IRQ_IXP425_GPIO11
#define INTB    IRQ_IXP425_GPIO6
#undef INTC
#undef INTD

#define PCI_FIRST_SLOT          14
#define IXP425_PCI_MAX_DEV      2
#define IXP425_PCI_IRQ_LINES    2

/* PCI controller pin mappings */
#define IXP425_PCI_RESET_GPIO   IXP425_GPIO_PIN_12
#define IXP425_PCI_CLK_PIN      IXP425_GPIO_CLK_0
#define IXP425_PCI_CLK_ENABLE   IXP425_GPIO_CLK0_ENABLE
#define IXP425_PCI_CLK_TC_LSH   IXP425_GPIO_CLK0TC_LSH
#define IXP425_PCI_CLK_DC_LSH   IXP425_GPIO_CLK0DC_LSH

#ifdef CONFIG_PCI_RESET

void __init coyote_pci_hw_init(void)
{
 /* Disable PCI clock */
 *IXP425_PERIPHERAL_REG(IXP425_GPIO_GPCLKR_VIRT) &=
   ~IXP425_PCI_CLK_ENABLE;

 /* configure PCI-related GPIO */
 gpio_line_config(IXP425_PCI_CLK_PIN, IXP425_GPIO_OUT);
 gpio_line_config(IXP425_PCI_RESET_GPIO, IXP425_GPIO_OUT);

 gpio_line_config(IXP425_GPIO_PIN_11, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
 gpio_line_config(IXP425_GPIO_PIN_6, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

 gpio_line_isr_clear(IXP425_GPIO_PIN_11);
 gpio_line_isr_clear(IXP425_GPIO_PIN_6);

 /* Assert reset for PCI controller */
 gpio_line_set(IXP425_PCI_RESET_GPIO, IXP425_GPIO_LOW);
 /* wait 1ms to satisfy "minimum reset assertion time" of the PCI spec. */
 udelay(1000);
 /* Config PCI clock */
 *IXP425_PERIPHERAL_REG(IXP425_GPIO_GPCLKR_VIRT) |=
   (0xf << IXP425_PCI_CLK_TC_LSH) | (0xf << IXP425_PCI_CLK_DC_LSH);
 /* Enable PCI clock */
 *IXP425_PERIPHERAL_REG(IXP425_GPIO_GPCLKR_VIRT) |= IXP425_PCI_CLK_ENABLE;
 /* wait 100us to satisfy "minimum reset assertion time from clock stable"
  * requirement of the PCI spec. */
 udelay(100);
 /* Deassert reset for PCI controller */
 gpio_line_set(IXP425_PCI_RESET_GPIO, IXP425_GPIO_HIGH);

 /* wait a while to let other devices get ready after PCI reset */
 udelay(1000);
}

#endif

void __init coyote_pci_init(void *sysdata)
{
#ifdef CONFIG_PCI_RESET
 if (ixp425_pci_is_host())
   coyote_pci_hw_init();
#endif

 gpio_line_config(IXP425_GPIO_PIN_11, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
 gpio_line_config(IXP425_GPIO_PIN_6, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

 gpio_line_isr_clear(IXP425_GPIO_PIN_11);
 gpio_line_isr_clear(IXP425_GPIO_PIN_6);

 ixp425_pci_init(sysdata);
}

static int __init coyote_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	static int pci_irq_table[IXP425_PCI_MAX_DEV][IXP425_PCI_IRQ_LINES] =
	{
		{INTB, INTB},
		{INTA, INTA}
	};

	int irq = -1;

	if (slot >= PCI_FIRST_SLOT) slot = (slot - PCI_FIRST_SLOT) + 1;

	if (slot >= 1 && slot <= IXP425_PCI_MAX_DEV &&
		pin >= 1 && pin <= IXP425_PCI_IRQ_LINES)
	{
		irq = pci_irq_table[slot-1][pin-1];
	}

	return irq;
}

struct hw_pci coyote_pci __initdata = {
 init:   coyote_pci_init,
 swizzle:  common_swizzle,
 map_irq:  coyote_map_irq,
};


