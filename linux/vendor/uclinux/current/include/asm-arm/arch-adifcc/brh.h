/*
 * inclue/asm-arm/arch-adifcc/brh.h
 *
 * Register and other defines for BRH board
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2000-2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef _BRH_H_
#define _BRH_H_

/*
 * Registers @ 0x04000000 phys, 0xff400000 virt)
 */
#define BRH_REG_BASE			0xff400000
#define BRH_REG(reg)			(volatile u32 *)(BRH_REG_BASE | reg)

#define BRH_SWITCH			BRH_REG(0x0500)
#define BRH_PCI_SLAVE_INT		BRH_REG(0x0000)
#define BRH_PCI_MEM0_XLATE		BRH_REG(0x0010)
#define BRH_PCI_MEM1_XLATE		BRH_REG(0x0018)
#define BRH_PCI_MEM2_XLATE		BRH_REG(0x0020)
#define BRH_PCI_MAST_INT		BRH_REG(0x0100)
#define BRH_PCI_MEM_ATU1		BRH_REG(0x0110)
#define BRH_PCI_MEM_ATU2		BRH_REG(0x0114)
#define BRH_PCI_IO_ATU			BRH_REG(0x0118)
#define	BRH_PCI_CONFIG_CTRL		BRH_REG(0x0120)
#define BRH_DMA0_CTRL			BRH_REG(0x0200)
#define BRH_DMA1_CTRL			BRH_REG(0x0240)
#define BRH_DMA0_STATUS			BRH_REG(0x0204)
#define BRH_DMA1_STATUS			BRH_REG(0x0244)
#define BRH_DMA0_BYTES			BRH_REG(0x0220)
#define BRH_DMA1_BYTES			BRH_REG(0x0260)
#define BRH_DMA0_PADDR			BRH_REG(0x0214)
#define BRH_DMA1_PADDR			BRH_REG(0x0254)
#define BRH_DMA0_LADDR			BRH_REG(0x021C)
#define BRH_DMA1_LADDR			BRH_REG(0x025C)
#define BRH_DMA0_DCR			BRH_REG(0x0224)
#define BRH_DMA1_DCR			BRH_REG(0x0264)
#define BRH_TIMERA_CTRL			BRH_REG(0x0300)
#define BRH_TIMERB_CTRL			BRH_REG(0x0320)
#define BRH_TIMERA_PRELOAD		BRH_REG(0x0304)
#define BRH_TIMERB_PRELOAD		BRH_REG(0x0324)
#define BRH_TIMERA_VALUE		BRH_REG(0x0308)
#define BRH_TIMERB_VALUE		BRH_REG(0x0328)
#define BRH_INT_STAT			BRH_REG(0x0400)
#define BRH_INT_MASK			BRH_REG(0x0404)
#define BRH_INT_STEERING		BRH_REG(0x0408)


/*
 * Interrupts
 */
#define BRH_INT_SWI			0x00000001
#define	BRH_INT_TIMERA			0x00000002
#define BRH_INT_TIMERB			0x00000004
#define BRH_INT_DIAG			0x00000080
#define	BRH_INT_DMA0_EOT		0x00000100
#define	BRH_INT_DMA1_EOT		0x000001


/*
 * Rotary Switch Status Register
 */
#define	BRH_BIG_ENDIAN_FLASH		0x00000020
#define BRH_SWITCH_POS_MASK		0x00000007

/*
 * PCI Slave Interrupt Status Register
 */
#define BRH_PCI_SLAVE_SERR		0x00000010
#define BRH_PCI_SLAVE_PERR		0x00000100
#define BRH_PCI_SLAVE_IFIFO_UNDER	0x00010000
#define BRH_PCI_SLAVE_IFIFO_OVER	0x00020000
#define BRH_PCI_SLAVE_OFIFO_UNDER	0x00040000
#define BRH_PCI_SLAVE_OFIFO_OVER	0x00080000

/*
 * PCI Slave Translation Register (Inbound ATU)
 */
#define	BRH_PCI_SLAVE_ENDIAN_SWAP	0x00000001

#define BRH_PCI_SLAVE_SDRAM_MASK	0x06000000

/*
 * PCI Master Interrupt Status Register
 */
#define BRH_PCI_MASTER_PERR		0x00000001
#define BRH_PCI_MASTER_TABORT		0x00000004
#define BRH_PCI_MASTER_MABORT		0x00000008
#define BRH_PCI_MASTER_IFIFO_UNDER	0x00010000
#define BRH_PCI_MASTER_IFIFO_OVER	0x00020000
#define BRH_PCI_MASTER_OFIFO_UNDER	0x00040000
#define BRH_PCI_MASTER_OFIFO_OVER	0x00080000

/*
 * PCI Outbound Memory ATU Register
 */
#define BRH_PCI_ATU_BURST_LINEAR	0x00000000
#define BRH_PCI_ATU_ENDIAN_SWAP		0x00000004
#define BRH_PCI_ATU_FORCE_32BIT		0x00000008

#define BRH_PCI_ATU_ADDR_MASK		0x87000000

/*
 * PCI Outbound I/O ATU Register
 */
#define BRH_PCI_IO_ATU_ADDR_MASK	0x87000000

/*
 * DMA Control Register
 */
#define BRH_DMA_ENABLE			0x00000001

/*
 * DMA Status Register
 */
#define BRH_DMA_PERR			0x00000001
#define BRH_DMA_TABORT			0x00000004
#define BRH_DMA_MABORT			0x00000008
#define BRH_DMA_EOT			0x00000100
#define BRH_DMA_ACTIVE			0x00000400
#define BRH_DMA_IFIFO_UNDER		0x00010000
#define BRH_DMA_IFIFO_OVER		0x00020000
#define BRH_DMA_OFIFO_UNDER		0x00040000
#define BRH_DMA_OFIFO_OVER		0x00080000

/*
 * DMA Byte Count Register
 */
#define BRH_DMA_MAX_BYTES		(1 << 24)

/*
 * DMA Local Address Register
 */
#define BRH_MAX_DMA_ADDR		(1 << 26)

/*
 * Timer Control/Status Register
 */
#define BRH_TIMER_ENABLE		0x00000001
#define BRH_TIMER_CONTINOUS		0x00000002
#define BRH_TIMER_INTERRUPT		0x00000200

/*
 * On board devices
 */

#define	BRH_UART0_BASE			0xff000000
#define BRH_UART1_BASE			0xff100000
#define BRH_LED_BASE			0xff200000

/*
 * PCI I/O bits
 */
#define	BRH_PCI_IO_VIRT			0xfd000000
#define BRH_PCI_IO_PHYS			0x0a000000
#define	PCIO_BASE			((BRH_PCI_IO_VIRT - BRH_PCI_IO_PHYS))

#define PCIBIOS_MIN_IO			0x0a000000
#define	PCIBIOS_MIN_MEM			0x0c000000

#define pcibios_assign_all_busses()	1

#endif // _BRH_H_
