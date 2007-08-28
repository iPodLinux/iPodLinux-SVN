/*
 * inclue/asm-arm/arch-ixp2000/ixmb2400.h
 *
 * Register and other defines for IXDP2400
 *
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#ifndef _IXDP2400_H_
#define _IXDP2400_H_


/*
 * On board CPLD memory map
 */
#define IXDP2400_PHY_CPLD_BASE	0xc7000000
#define IXDP2400_VIRT_CPLD_BASE	0xd4000000
#define IXDP2400_CPLD_SIZE	0x01000000


#define IXDP2400_CPLD_REG(reg)  	(volatile unsigned long *)(IXDP2400_VIRT_CPLD_BASE | reg)

#define IXDP2400_CPLD_SYSLED   		IXDP2400_CPLD_REG(0x0)  
#define IXDP2400_CPLD_DISP_DATA        	IXDP2400_CPLD_REG(0x4)
#define IXDP2400_CPLD_CLOCK_SPEED      	IXDP2400_CPLD_REG(0x8)
#define IXDP2400_CPLD_INT              	IXDP2400_CPLD_REG(0xc)
#define IXDP2400_CPLD_REV              	IXDP2400_CPLD_REG(0x10)
#define IXDP2400_CPLD_SYS_CLK_M        	IXDP2400_CPLD_REG(0x14)
#define IXDP2400_CPLD_SYS_CLK_N        	IXDP2400_CPLD_REG(0x18)
#define IXDP2400_CPLD_INT_MASK         	IXDP2400_CPLD_REG(0x48)

/* 
 * Interrupt register mask bits on CPLD register 
 */
#define IXDP2400_MASK_INGRESS		(1<<0)
#define IXDP2400_MASK_EGRESS_NIC	(1<<1)   /*Master Ethernet */
#define IXDP2400_MASK_MEDIA_PCI		(1<<2) 	 /* media PCI interrupt */
#define IXDP2400_MASK_MEDIA_SP		(1<<3)   /* media slow port interrupt*/
#define IXDP2400_MASK_SF_PCI		(1<<4)   /* Sw Fabric PCI intrpt */
#define IXDP2400_MASK_SF_SP		(1<<5)   /* SW Fabric SlowPort intrpt */
#define IXDP2400_MASK_PMC		(1<<6)
#define IXDP2400_MASK_TVM		(1<<7)

/* defines for access from slowport */
#define CFG_PCI_BOOT_HOST	(1 << 2)
#define CFG_PROM_BOOT		(1 << 1)

#define SYSTEM_LED_REG_OFF      0x0
#define ALPHANUM_DIS_DATA_OFF   0x4
#define CPLD_INT_OFF            0xC
#define CPLD_REV_OFF            0x10
#define SYS_CLK_M_OFF           0x14
#define SYS_CLK_N_OFF           0x18

#define CPLD_CSR_BASE           PHY_CPLD_CSR
#define SYSTEM_LED              (CPLD_CSR_BASE + SYSTEM_LED_REG_OFF)
#define ALPHANUM_DIS_DATA       (CPLD_CSR_BASE + ALPHANUM_DIS_DATA_OFF)
#define CPLD_INT                (CPLD_CSR_BASE + CPLD_INT_OFF)
#define CPLD_REV                (CPLD_CSR_BASE + CPLD_REV_OFF)
#define SYS_CLK_M               (CPLD_CSR_BASE + SYS_CLK_M_OFF)
#define SYS_CLK_N               (CPLD_CSR_BASE + SYS_CLK_N_OFF)

#define PCI_RCOMP_OVER  0xde000060
#define RESET_DRAM0	(1 << 11)
#define RESET_SRAM0	(1 << 3)
#define RESET_SRAM1	(1 << 4)
#define PCIRST		(1 << 2)
#define RESET_PCI	(1 << 1)
#define RESET_XSCALE	1
#define RESET_SDRAM_SRAM	(RESET_DRAM0 | RESET_SRAM0 | RESET_SRAM1)
#define NOT_RESET_PCI		~(PCIRST | RESET_PCI)

#define PARITY_ENABLE			(1 << 3)

#define FLASH_WRITE_ENABLE		(1 << 9)
#define FLASH_ALIAS_DISABLE		(1 << 8)

/* SDRAM controller values */
#define SRAM_DIVISOR			0x3
#define SDRAM_DIVISOR			0x4
#define SRAM_200MHZ_DIVISOR             0x3
#define SRAM_150MHZ_DIVISOR             0x4


#define PARITY_ENABLE			(1 << 3)

#define FLASH_WRITE_ENABLE		(1 << 9)
#define FLASH_ALIAS_DISABLE		(1 << 8)

#define DDR_RX_DLL_VAL			0x11
#define DDR_RX_DESKEW_VAL		0x11
#define DDR_RDDLYSEL_RECEN_VAL		0x11
#define DDR_RCOMP_IO_CONFIG_VAL		0x0

#define TCL		(1 << 2)
#define TRC		(1 << 6)
#define TWTR		(1 << 10)
#define REF_EN		(1 << 14)
#define REF_COUNT	(10 << 15)
#define NUM_ROW_COL	(3 << 23)
#define NUM_SIDES	(1 << 26)
#define TRRD		(1 << 28)
#define DU_CONTROL_VAL	(TRRD | NUM_SIDES | NUM_ROW_COL | REF_COUNT | REF_EN | TWTR | TRC | TCL)

#define RESET_DLL	 0x100
#define LOAD_MODE_NORMAL 0x62
#define PRECHARGE_ALL	(1 << 10)
#define EXT_LD_MODE	(1 << 12)
#define SIDE0		(1 << 14)
#define SIDE1		(1 << 15)
#define CKE		(1 << 16)
#define PRECHARGE	(1 << 29)
#define REFRESH		(1 << 30)
#define LD_MODE_REG	(1 << 31)

#define DISABLE_CHK	(1 << 8)

#define DU_CONTROL		PHY_DRAM_CSR + 0x0000
#define DU_ECC_TEST		PHY_DRAM_CSR + 0x0018
#define DU_INIT			PHY_DRAM_CSR + 0x0020
#define DU_CONTROL2		PHY_DRAM_CSR + 0x0028

#define DDR_RDDLYSEL_RECEN      PHY_DRAM_CSR + 0x03C8    
#define DDR_RX_DLL              PHY_DRAM_CSR + 0x0650
#define DDR_RX_DESKEW           PHY_DRAM_CSR + 0x0688
#define DDR_RCOMP_IO_CONFIG     PHY_DRAM_CSR + 0x03C0

#define CR0_FRCSMRCOMP_OFF	0x100
#define CR0_DSTRENGTHSEL_OFF	0x130
#define CR0_DDQRCOMP_OFF	0x148
#define CR0_DCTLRCOMP_OFF	0x190
#define CR0_DRCVRCOMP_OFF	0x1D8
#define CR0_DCKERCOMP_OFF	0x228
#define CR0_DCSRCOMP_OFF	0x2B0
#define CR0_DCKRCOMP_OFF	0x338
#define CR0_DX8X16CKECSCKSEL_OFF 0x220
#define CR0_RCOMPPRD_OFF	0x108
#define CR0_DIGFIL_OFF		0x118

#define CR0_SLEWPROGRAMMED_OFF	0x128
#define CR0_OVRRIDEH_OFF	0x138
#define CR0_OVRRIDEV_OFF	0x140
#define CR0_JT_CONFIG_OFF	0x3C0

#define DRAM_CSR_BASE		PHY_DRAM_CSR

#define CR0_FRCSMRCOMP		(DRAM_CSR_BASE + CR0_FRCSMRCOMP_OFF)
#define CR0_DSTRENGTHSEL	(DRAM_CSR_BASE + CR0_DSTRENGTHSEL_OFF)
#define CR0_DDQRCOMP		(DRAM_CSR_BASE + CR0_DDQRCOMP_OFF)
#define CR0_DCTLRCOMP		(DRAM_CSR_BASE + CR0_DCTLRCOMP_OFF)
#define CR0_DRCVRCOMP		(DRAM_CSR_BASE + CR0_DRCVRCOMP_OFF)
#define CR0_DCKERCOMP		(DRAM_CSR_BASE + CR0_DCKERCOMP_OFF)
#define CR0_DCSRCOMP		(DRAM_CSR_BASE + CR0_DCSRCOMP_OFF)
#define CR0_DCKRCOMP		(DRAM_CSR_BASE + CR0_DCKRCOMP_OFF)
#define CR0_DX8X16CKECSCKSEL	(DRAM_CSR_BASE + CR0_DX8X16CKECSCKSEL_OFF)
#define CR0_RCOMPPRD		(DRAM_CSR_BASE + CR0_RCOMPPRD_OFF)
#define CR0_DIGFIL		(DRAM_CSR_BASE + CR0_DIGFIL_OFF)

#define CR0_SLEWPROGRAMMED	(DRAM_CSR_BASE + CR0_SLEWPROGRAMMED_OFF)
#define CR0_OVRRIDEH		(DRAM_CSR_BASE + CR0_OVRRIDEH_OFF)
#define CR0_OVRRIDEV		(DRAM_CSR_BASE + CR0_OVRRIDEV_OFF)
#define CR0_JT_CONFIG		(DRAM_CSR_BASE + CR0_JT_CONFIG_OFF)

/* QDR Read/Write Pointers info */
#define CHAN0_SRAM_CONTROL	PHY_SRAM_CSR
#define CHAN1_SRAM_CONTROL 	CHAN0_SRAM_CONTROL + 0x400000

#define QDR_RD_WR_PTR_BASE0	CHAN0_SRAM_CONTROL + 0x0240
#define QDR_RX_DESKEW_BASE0	CHAN0_SRAM_CONTROL + 0x0244
#define QDR_RX_DLL_BASE0	CHAN0_SRAM_CONTROL + 0x0248

#define QDR_RX_DESKEW_BASE1	CHAN1_SRAM_CONTROL + 0x0244
#define QDR_RD_WR_PTR_BASE1	CHAN1_SRAM_CONTROL + 0x0240
#define QDR_RX_DLL_BASE1	CHAN1_SRAM_CONTROL + 0x0248
#define QDR2			(1<<2)

#endif /*_IXDP2400_H_ */
