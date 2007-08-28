/*
 * inclue/asm-arm/arch-ixp2000/slave.h
 *
 * Register and other defines for IXMB2400 for slave NPU board
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
#ifndef _SLAVE_H
#define _SLAVE_H

#define MISC_CONTROL_OFF    	0x04
#define IXP_RESET0_OFF      	0x0C
#define CCR_OFF          	0x14
#define STRAP_OPTIONS_OFF    	0x18

/* slave's csr as seen from master PCI*/
#define GLOBAL_CONTROL_BASE_FRM_PCI       0x4A00
#define IXP_RESET0_FRM_PCI     (GLOBAL_CONTROL_BASE_FRM_PCI + IXP_RESET0_OFF)
#define CCR_FRM_PCI            (GLOBAL_CONTROL_BASE_FRM_PCI + CCR_OFF)
#define STRAP_OPTIONS_FRM_PCI  (GLOBAL_CONTROL_BASE_FRM_PCI + STRAP_OPTIONS_OFF)
#define MISC_CONTROL_FRM_PCI   (GLOBAL_CONTROL_BASE_FRM_PCI + MISC_CONTROL_OFF)

#define SLOW_PORT_CSR_BASE_FRM_PCI      0x80000
#define SCRATCH_BASE_FRM_PCI            0xF0000

#define DDR_RX_DLL_VAL                  0x11
#define DDR_RX_DESKEW_VAL               0x11
#define DDR_RDDLYSEL_RECEN_VAL          0x11

#define SRAM_CH1_BASE_FRM_PCI   	0xF9800
#define SRAM_CH0_BASE_FRM_PCI   	0xF9C00
#define SRAM_CH_BASE_FRM_PCI   		0xF9000

#define DU_CONTROL_OFF        		0x0
#define DU_ECC_TEST_OFF       		0x18
#define DU_INIT_OFF           		0x20
#define DU_CONTROL2_OFF       		0x28
#define DDR_RCOMP_IO_CONFIG_OFF      	0x3c0
#define DDR_RDDLYSEL_RECEN_OFF       	0x3c8
#define DDR_RX_DLL_OFF       		0x650
#define DDR_RX_DESKEW_OFF      		0x688

#define CR0_FRCSMRCOMP_OFF              0x100
#define CR0_DSTRENGTHSEL_OFF            0x130
#define CR0_DDQRCOMP_OFF                0x148
#define CR0_DCTLRCOMP_OFF               0x190
#define CR0_DRCVRCOMP_OFF               0x1D8
#define CR0_DCKERCOMP_OFF               0x228
#define CR0_DCSRCOMP_OFF                0x2B0
#define CR0_DCKRCOMP_OFF                0x338
#define CR0_DX8X16CKECSCKSEL_OFF        0x220
#define CR0_RCOMPPRD_OFF                0x108
#define CR0_DIGFIL_OFF                  0x118

#define CR0_SLEWPROGRAMMED_OFF          0x128
#define CR0_OVRRIDEH_OFF                0x138
#define CR0_OVRRIDEV_OFF                0x140
#define CR0_JT_CONFIG_OFF               0x3C0

#define CR0_FRCSMRCOMP_FRM_PCI          (DRAM_CH0_BASE_FRM_PCI + CR0_FRCSMRCOMP_OFF)
#define CR0_DSTRENGTHSEL_FRM_PCI        (DRAM_CH0_BASE_FRM_PCI + CR0_DSTRENGTHSEL_OFF)
#define CR0_DDQRCOMP_FRM_PCI            (DRAM_CH0_BASE_FRM_PCI + CR0_DDQRCOMP_OFF)
#define CR0_DCTLRCOMP_FRM_PCI           (DRAM_CH0_BASE_FRM_PCI + CR0_DCTLRCOMP_OFF)
#define CR0_DRCVRCOMP_FRM_PCI           (DRAM_CH0_BASE_FRM_PCI + CR0_DRCVRCOMP_OFF)
#define CR0_DCKERCOMP_FRM_PCI           (DRAM_CH0_BASE_FRM_PCI + CR0_DCKERCOMP_OFF)
#define CR0_DCSRCOMP_FRM_PCI            (DRAM_CH0_BASE_FRM_PCI + CR0_DCSRCOMP_OFF)
#define CR0_DCKRCOMP_FRM_PCI            (DRAM_CH0_BASE_FRM_PCI + CR0_DCKRCOMP_OFF)
#define CR0_DX8X16CKECSCKSEL_FRM_PCI    (DRAM_CH0_BASE_FRM_PCI + CR0_DX8X16CKECSCKSEL_OFF)
#define CR0_RCOMPPRD_FRM_PCI            (DRAM_CH0_BASE_FRM_PCI + CR0_RCOMPPRD_OFF)
#define CR0_DIGFIL_FRM_PCI              (DRAM_CH0_BASE_FRM_PCI + CR0_DIGFIL_OFF)

#define CR0_SLEWPROGRAMMED_FRM_PCI      (DRAM_CH0_BASE_FRM_PCI + CR0_SLEWPROGRAMMED_OFF)
#define CR0_OVRRIDEH_FRM_PCI            (DRAM_CH0_BASE_FRM_PCI + CR0_OVRRIDEH_OFF)
#define CR0_OVRRIDEV_FRM_PCI            (DRAM_CH0_BASE_FRM_PCI + CR0_OVRRIDEV_OFF)
#define CR0_JT_CONFIG_FRM_PCI           (DRAM_CH0_BASE_FRM_PCI + CR0_JT_CONFIG_OFF)

#define DRAM_CH0_BASE_FRM_PCI   0xFD800

#define DU_CONTROL_FRM_PCI    	(DRAM_CH0_BASE_FRM_PCI + DU_CONTROL_OFF)
#define DU_ECC_TEST_FRM_PCI   	(DRAM_CH0_BASE_FRM_PCI + DU_ECC_TEST_OFF)
#define DU_INIT_FRM_PCI       	(DRAM_CH0_BASE_FRM_PCI + DU_INIT_OFF)
#define DU_CONTROL2_FRM_PCI    	(DRAM_CH0_BASE_FRM_PCI + DU_CONTROL2_OFF)
#define DDR_RCOMP_IO_CONFIG_FRM_PCI  (DRAM_CH0_BASE_FRM_PCI + DDR_RCOMP_IO_CONFIG_OFF)
#define DDR_RDDLYSEL_RECEN_FRM_PCI  (DRAM_CH0_BASE_FRM_PCI + DDR_RDDLYSEL_RECEN_OFF)
#define DDR_RX_DLL_FRM_PCI     (DRAM_CH0_BASE_FRM_PCI + DDR_RX_DLL_OFF)
#define DDR_RX_DESKEW_FRM_PCI  (DRAM_CH0_BASE_FRM_PCI + DDR_RX_DESKEW_OFF)

#define PCI_OUT_INT_MASK_OFF    0x34
#define MAILBOX_1_OFF       	0x54
#define PCI_CONTROL_OFF       	0x13C
#define PCI_ADDR_EXT_OFF      	0x140
#define XSCALE_INT_STATUS_OFF   0x158
#define XSCALE_INT_ENABLE_OFF   0x15C

#define PCI_CSR_BASE_FRM_PCI   0xFE000

#define PCI_OUT_INT_MASK_FRM_PCI (PCI_CSR_BASE_FRM_PCI + PCI_OUT_INT_MASK_OFF)
#define PCI_CONTROL_FRM_PCI      (PCI_CSR_BASE_FRM_PCI + PCI_CONTROL_OFF)
#define PCI_ADDR_EXT_FRM_PCI     (PCI_CSR_BASE_FRM_PCI + PCI_ADDR_EXT_OFF)

#define SLAVE_PCI_CMD_STAT_VAL (PCI_COMMAND_INVALIDATE \
				| PCI_COMMAND_MEMORY  \
				| PCI_COMMAND_IO)

#define CFG_PCI_BOOT_HOST   	(1 << 2)
#define PCI_OUT_INT_MASK_VAL   	(1<<1)
#define XSCALE_RESET_BIT 	0x00000001

#define FLASH_WRITE_ENABLE      (1 << 9)
#define FLASH_ALIAS_DISABLE     (1 << 8)

#define IXP_TYPE_2400   	0
#define IXP_TYPE_2800   	1

#define IXP_IOC_QUERY_INFO      0x1
#define IXP_IOC_RELEASE_RST     0x2

#define IXP_FLAG_ROM_BOOT       (1<<0x0)
#define IXP_FLAG_PCI_HOST       (1<<0x1)
#define IXP_FLAG_PCI_ARB        (1<<0x2)
#define IXP_FLAG_NPU_UP         (1<<0x3) /* Slave NPU is running after reset */
#define IXP_FLAG_NO_FLASH       (1<<0x4) /* Slave NPU does not have flash*/


struct ixp_info {
        char name[32];
        unsigned short ixp_type;
        unsigned short ixp_rev;

        unsigned long sdram_size;
        unsigned long sram_size;

        unsigned long flags;

        unsigned long csr_bar;
        unsigned long sram_bar;
        unsigned long sdram_bar;

        unsigned long csr_ioaddr;
        unsigned long sram_ioaddr;
        unsigned long sdram_ioaddr;
	struct ixp_info *next;
};

#endif
