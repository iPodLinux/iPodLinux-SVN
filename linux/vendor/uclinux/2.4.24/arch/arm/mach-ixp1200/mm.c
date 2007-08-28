/*
 * arch/arm/mm/mm-ixp1200.c
 *
 * Low level memory initialization for IXP1200 based systems
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2000 Intel Corporation, Inc.
 * Copyright (C) 2001-2002 MontaVista Software, Inc.
 *
 * Mar-27-2000 - Uday Naik
 * 	Created for IXP1200 Eval board 
 * Nov-11-2001 - Deepak Saxena
 *      Massive cleanup, deleted SDRAM_PACKET mapping as we use all 
 *      of SDRAM for the kernel by default.  If you want to reserve
 *      a large chunk of SDRAM for the packet engines, run make
 *      config and change the CONFIG_IXP1200_SDRAM_BASE and 
 *      CONFIG_IXP1200_SDRAM_SIZE parameters to move the kernel
 *      to upper memory.
 *
 * [Note: This is currently the same as the 2.3.99-pre3 code to allow
 *  compatibility with existing code on the IXP1200, but will probably
 *  drasticlly be rewritten in the near future.  For this reason,
 *  only use the constansts in your code and do not the vaddrs themselves
 *  to make your life easier when it does change]
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/hardware.h>

#include <asm/mach/map.h>

struct map_desc ixp1200_io[] __initdata =
{
	/* Virtual   Physical     Length      Domain     R  W  C  B */

	/* SRAM_BASE */
	{ 0xf4000000, 0x10000000, 0x00800000, DOMAIN_IO, 1, 0, 0, 0 },

	/* PCICFG0_BASE */
	{ 0xf1000000, 0x53000000, 0x01000000, DOMAIN_IO, 1, 0, 0, 0 },

	/* PCICFG1_BASE */
	{ 0xf2000000, 0x52000000, 0x01000000, DOMAIN_IO, 1, 0, 0, 0 },

	/* ARMCSR_BASE */
	{ 0xf0000000, 0x90000000, 0x00100000, DOMAIN_IO, 1, 0, 0, 0 },

	/* PCIIO_BASE */
	{ 0xf0700000, 0x54000000, 0x00100000, DOMAIN_IO, 1, 0, 0, 0 },

	/* PCI_CSR_BASE */
	{ 0xf0100000, 0x42000000, 0x00100000, DOMAIN_IO, 1, 0, 0, 0 },

	/* MICROENGINE_FBI_BASE */
	{ 0xf0600000, 0xb0000000, 0x00100000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_SLOW_PORT_BASE */
	{ 0xf0800000, 0x38400000, 0x00300000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_TEST_SET_BITS */
	{ 0xf8000000, 0x19800000, 0x00800000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_TEST_CLEAR_BITS */
	{ 0xf7800000, 0x19000000, 0x00800000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_SET_BITS */
	{ 0xf7000000, 0x18800000, 0x00800000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_CLEAR_BITS */
	{ 0xf6800000, 0x18000000, 0x00800000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_CAM_UNLOCK */
	{ 0xf6000000, 0x16000000, 0x00800000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_WRITE_UNLOCK */
	{ 0xf5800000, 0x14000000, 0x00800000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_READ_LOCK */
	{ 0xf5000000, 0x12000000, 0x00800000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_PUSH_POPQ_BASE */
	{ 0xd0000000, 0x20000000, 0x08000000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SRAM_CSR_BASE */
	{ 0xf0200000, 0x38000000, 0x00100000, DOMAIN_IO, 1, 0, 0, 0 },

	/* SDRAM_CSR_BASE */
	{ 0xf0300000, 0xff000000, 0x00100000, DOMAIN_IO, 1, 0, 0, 0 },

 	LAST_DESC
};

void __init ixp1200_map_io(void)
{
	iotable_init(ixp1200_io);
}

