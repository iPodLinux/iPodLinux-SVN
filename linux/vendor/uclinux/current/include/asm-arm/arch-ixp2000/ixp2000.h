/*
 * include/asm-arm/arch-ixp2000/ixp2000.h
 *
 * Generic register definitions for IXP2000 based systems.
 *
 * Original Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#ifndef _IXP2000_H_
#define _IXP2000_H_

#ifndef __ASM_ARCH_HARDWARE_H__
#error "Do not include this file directly. Instead include <asm/hardware.h>"
#endif

#define PHYS_SDRAM_BASE		0x1c000000 /* start for upper 64M */
#define INITRD_LOCATION		0x1c600000 
#define INITRD_SIZE		0x00400000 
#define PHYS_SDRAM_SIZE		(64*1024*1024)

#define FLASH_BASE	0xc4000000
#define MAX_FLASH_SIZE	0x04000000

/* 
 * Static I/O regions. The manual defines each region as being several
 * MB in size, but all the registers are within the first 4K, so there's
 * no purpose in mapping the whole region in.
 */
#define	GLOBAL_REG_PHYS_BASE	0xc0004000
#define	GLOBAL_REG_VIRT_BASE	0xfaff8000
#define	GLOBAL_REG_SIZE		0x1000

#define	GPIO_PHYS_BASE		0xc0010000
#define	GPIO_VIRT_BASE		0xfaff9000
#define	GPIO_SIZE		0x1000

#define	TIMER_PHYS_BASE		0xc0020000
#define	TIMER_VIRT_BASE		0xfaffa000
#define	TIMER_SIZE		0x1000

#define	UART_PHYS_BASE		0xc0030000
#define	UART_VIRT_BASE		0xfaffb000
#define UART_SIZE		0x1000

#define	SLOWPORT_CSR_PHYS_BASE	0xc0080000
#define	SLOWPORT_CSR_VIRT_BASE	0xfaffc000
#define	SLOWPORT_CSR_SIZE	0x1000

#define INTCTL_PHYS_BASE	0xd6000000
#define	INTCTL_VIRT_BASE	0xfaffd000
#define	INTCTL_SIZE		0x01000

#define PCI_CREG_PHYS_BASE	0xde000000
#define	PCI_CREG_VIRT_BASE	0xfaffe000
#define	PCI_CREG_SIZE		0x1000

#define PCI_CSR_PHYS_BASE	0xdf000000
#define	PCI_CSR_VIRT_BASE	0xfafff000
#define	PCI_CSR_SIZE		0x1000

#define PCI_CFG0_PHYS_BASE	0xda000000
#define PCI_CFG0_VIRT_BASE	0xfb000000
#define PCI_CFG0_SIZE   	0x01000000

#define PCI_CFG1_PHYS_BASE	0xdb000000
#define PCI_CFG1_VIRT_BASE	0xfc000000
#define PCI_CFG1_SIZE		0x01000000

#define PCI_IO_PHYS_BASE	0xd8000000
#define	PCI_IO_VIRT_BASE	0xfd000000
#define PCI_IO_SIZE     	0x02000000

/* 
 * Timers
 */
#define	IXP2000_TIMER_REG(x)   ((volatile unsigned long*)(TIMER_VIRT_BASE+(x)))
/* Timer control */
#define   IXP2000_T1_CTL    	IXP2000_TIMER_REG(0x00)    
/* Store initial value */
#define   IXP2000_T1_CLD    	IXP2000_TIMER_REG(0x10)    
/* Store current value */
#define   IXP2000_T1_CSR    	IXP2000_TIMER_REG(0x20)    
/* Clear associated timer interrupt */
#define   IXP2000_T1_CLR 	IXP2000_TIMER_REG(0x30)    
/* Timer watchdog enable for T4 */
#define  IXP2000_TWDE         IXP2000_TIMER_REG(0x40)   


/*
 * Interrupt controller registers
 */
#define IXP2000_INTCTL_REG(reg) (volatile unsigned long*)(INTCTL_VIRT_BASE | reg)
#define IXP2000_IRQ_STATUS   	IXP2000_INTCTL_REG(0x08)
#define IXP2000_IRQ_ENABLE     	IXP2000_INTCTL_REG(0x10)
#define IXP2000_IRQ_ENABLE_SET 	IXP2000_INTCTL_REG(0x10)
#define IXP2000_IRQ_ENABLE_CLR 	IXP2000_INTCTL_REG(0x18)
#define IXP2000_FIQ_ENABLE_CLR 	IXP2000_INTCTL_REG(0x14)

/* 
 * IRQ mask/unmask bits. These map 1:1 to IRQ number
 */
#define IRQ_MASK_SOFT           (1 << 0)
#define IRQ_MASK_ERRSUM         (1 << 1)
#define IRQ_MASK_UART           (1 << 2)
#define IRQ_MASK_GPIO           (1 << 3)
#define IRQ_MASK_TIMER1         (1 << 4)
#define IRQ_MASK_TIMER2         (1 << 5)
#define IRQ_MASK_TIMER3         (1 << 6)
#define IRQ_MASK_TIMER4         (1 << 7)
#define IRQ_MASK_PMU            (1 << 8)
#define IRQ_MASK_SPF            (1 << 9) /* Slowport Framer */
#define IRQ_MASK_DMA1_DONE      (1 << 10)
#define IRQ_MASK_DMA2_DONE      (1 << 11)
#define IRQ_MASK_DMA3_DONE      (1 << 12)
#define IRQ_MASK_DOORBELL	(1 << 13) /* PCI Doorbell */
#define IRQ_MASK_UENG           (1 << 14) /* ME attention */
#define IRQ_MASK_PCI            (1 << 15) /* OR of INTA or INTB */
#define IRQ_MASK_THDA0          (1 << 16) /* ThreadA 0-31 */
#define IRQ_MASK_THDA1          (1 << 17) /* ThreadA 32-63 */
#define IRQ_MASK_THDA2          (1 << 18) /* ThreadA 64-95 */
#define IRQ_MASK_THDA3          (1 << 19) /* ThreadA 96-127 */
#define IRQ_MASK_THDB0          (1 << 25) /* ThreadB 0-31 */
#define IRQ_MASK_THDB1          (1 << 26) /* ThreadB 32-63 */
#define IRQ_MASK_THDB2          (1 << 27) /* ThreadB 64-95 */
#define IRQ_MASK_THDB3          (1 << 28) /* ThreadB 96-127 */

/*
 * 16550 UART
 */
#define IXP2000_UART_BASE 	UART_VIRT_BASE

/*
 * PCI config register access from core
 */
#define IXP2000_PCI_CREG(reg)	(volatile unsigned long*)(PCI_CREG_VIRT_BASE | reg)
#define IXP2000_PCI_CMDSTAT 	IXP2000_PCI_CREG(0x04)
#define IXP2000_PCI_SDRAM_BAR	IXP2000_PCI_CREG(0x18)

/*
 * PCI CSRs
 */
#define IXP2000_PCI_CSR(reg)	(volatile unsigned long*)(PCI_CSR_VIRT_BASE | reg)

/*
 * PCI outbound interrupts
 */
#define IXP2000_PCI_OUT_INT_STATUS   	IXP2000_PCI_CSR(0x30)
#define IXP2000_PCI_OUT_INT_MASK        IXP2000_PCI_CSR(0x34)
/*
 * PCI communications
 */
#define IXP2000_PCI_MAILBOX0            IXP2000_PCI_CSR(0x50)
#define IXP2000_PCI_MAILBOX1            IXP2000_PCI_CSR(0x54)
#define IXP2000_PCI_MAILBOX2            IXP2000_PCI_CSR(0x58)
#define IXP2000_PCI_MAILBOX3            IXP2000_PCI_CSR(0x5C)
#define IXP2000_XSCALE_DOORBELL         IXP2000_PCI_CSR(0x60)
#define IXP2000_XSCALE_DOORBELL_SETUP   IXP2000_PCI_CSR(0x64)
#define IXP2000_PCI_DOORBELL            IXP2000_PCI_CSR(0x70)
#define IXP2000_PCI_DOORBELL_SETUP  	IXP2000_PCI_CSR(0x74)

/*
 * DMA engines
 */
#define IXP2000_PCI_CH1_BYTE_CNT        IXP2000_PCI_CSR(0x80)
#define IXP2000_PCI_CH1_ADDR            IXP2000_PCI_CSR(0x84)
#define IXP2000_PCI_CH1_DRAM_ADDR    	IXP2000_PCI_CSR(0x88)
#define IXP2000_PCI_CH1_DESC_PTR        IXP2000_PCI_CSR(0x8C)
#define IXP2000_PCI_CH1_CNTRL           IXP2000_PCI_CSR(0x90)
#define IXP2000_PCI_CH1_ME_PARAM        IXP2000_PCI_CSR(0x94)
#define IXP2000_PCI_CH2_BYTE_CNT        IXP2000_PCI_CSR(0xA0)
#define IXP2000_PCI_CH2_ADDR            IXP2000_PCI_CSR(0xA4)
#define IXP2000_PCI_CH2_DRAM_ADDR    	IXP2000_PCI_CSR(0xA8)
#define IXP2000_PCI_CH2_DESC_PTR        IXP2000_PCI_CSR(0xAC)
#define IXP2000_PCI_CH2_CNTRL           IXP2000_PCI_CSR(0xB0)
#define IXP2000_PCI_CH2_ME_PARAM        IXP2000_PCI_CSR(0xB4)
#define IXP2000_PCI_CH3_BYTE_CNT        IXP2000_PCI_CSR(0xC0)
#define IXP2000_PCI_CH3_ADDR            IXP2000_PCI_CSR(0xC4)
#define IXP2000_PCI_CH3_DRAM_ADDR    	IXP2000_PCI_CSR(0xC8)
#define IXP2000_PCI_CH3_DESC_PTR        IXP2000_PCI_CSR(0xCC)
#define IXP2000_PCI_CH3_CNTRL           IXP2000_PCI_CSR(0xD0)
#define IXP2000_PCI_CH3_ME_PARAM        IXP2000_PCI_CSR(0xD4)
#define IXP2000_DMA_INF_MODE         	IXP2000_PCI_CSR(0xE0)
/*
 * Size masks for BARs
 */
#define IXP2000_PCI_SRAM_BASE_ADDR_MASK IXP2000_PCI_CSR(0xFC)
#define IXP2000_PCI_DRAM_BASE_ADDR_MASK IXP2000_PCI_CSR(0x100)
/*
 * Control and uEngine related
 */
#define IXP2000_PCI_CONTROL             IXP2000_PCI_CSR(0x13C)
#define IXP2000_PCI_ADDR_EXT            IXP2000_PCI_CSR(0x140)
#define IXP2000_PCI_ME_PUSH_STATUS   	IXP2000_PCI_CSR(0x148)
#define IXP2000_PCI_ME_PUSH_EN          IXP2000_PCI_CSR(0x14C)
#define IXP2000_PCI_ERR_STATUS          IXP2000_PCI_CSR(0x150)
#define IXP2000_PCI_ERR_ENABLE          IXP2000_PCI_CSR(0x154)
/*
 * Inbound PCI interrupt control
 */
#define IXP2000_PCI_XSCALE_INT_STATUS   IXP2000_PCI_CSR(0x158)
#define IXP2000_PCI_XSCALE_INT_ENABLE   IXP2000_PCI_CSR(0x15C)

#define IXP2000_PCICNTL_PNR	(1<<17)	/* PCI not Reset bit of PCI_CONTROL */
#define IXP2000_PCICNTL_PCF	(1<<28)	/* PCI Centrolfunction bit */
#define IXP2000_XSCALE_INT	(1<<1)	/* Interrupt from  XScale to PCI */

/* These are from the IRQ register in the PCI ISR register */
#define PCI_CONTROL_BE_DEO      (1 << 22) /* Big Endian Data Enable Out */
#define PCI_CONTROL_BE_DEI      (1 << 21) /* Big Endian Data Enable In  */
#define PCI_CONTROL_BE_BEO      (1 << 20) /* Big Endian Byte Enable Out */
#define PCI_CONTROL_BE_BEI      (1 << 19) /* Big Endian Byte Enable In  */
#define PCI_CONTROL_PNR         (1 << 17) /* PCI Not Reset bit of SA_CONTROL */


#define IXP2000_PCI_RST_REL  	(1 << 2) 
#define CFG_RST_DIR  		(*IXP2000_PCI_CONTROL & IXP2000_PCICNTL_PCF) 
#define CFG_PCI_BOOT_HOST       (1 << 2)
#define CFG_BOOT_PROM           (1 << 1)


/*
 * SlowPort CSRs
 *
 * The slowport is used to access things like flash, SONET framer control
 * ports, slave microprocessors, CPLDs, and others of chip memory mapped
 * peripherals.
 */
#define	SLOWPORT_CSR(reg)	(volatile unsigned long*)(SLOWPORT_CSR_VIRT_BASE | reg)

#define	IXP2000_SLOWPORT_CCR		SLOWPORT_CSR(0x00)
#define	IXP2000_SLOWPORT_WTC1		SLOWPORT_CSR(0x04)
#define	IXP2000_SLOWPORT_WTC2		SLOWPORT_CSR(0x08)
#define	IXP2000_SLOWPORT_RTC1		SLOWPORT_CSR(0x0c)
#define	IXP2000_SLOWPORT_RTC2		SLOWPORT_CSR(0x10)
#define	IXP2000_SLOWPORT_FSR		SLOWPORT_CSR(0x14)
#define	IXP2000_SLOWPORT_PCR		SLOWPORT_CSR(0x18)
#define	IXP2000_SLOWPORT_ADC		SLOWPORT_CSR(0x1C)
#define	IXP2000_SLOWPORT_FAC		SLOWPORT_CSR(0x20)
#define	IXP2000_SLOWPORT_FRM		SLOWPORT_CSR(0x24)
#define	IXP2000_SLOWPORT_FIN		SLOWPORT_CSR(0x28)

/*
 * CCR values.  
 * The CCR configures the clock division for the slowport interface.
 */
#define	SLOWPORT_CCR_DIV_1	0x00
#define	SLOWPORT_CCR_DIV_2	0x01
#define	SLOWPORT_CCR_DIV_4	0x02
#define	SLOWPORT_CCR_DIV_6	0x03
#define	SLOWPORT_CCR_DIV_8	0x04
#define	SLOWPORT_CCR_DIV_10	0x05
#define	SLOWPORT_CCR_DIV_12	0x06
#define	SLOWPORT_CCR_DIV_14	0x07
#define	SLOWPORT_CCR_DIV_16	0x08
#define	SLOWPORT_CCR_DIV_18	0x09
#define	SLOWPORT_CCR_DIV_20	0x0a
#define	SLOWPORT_CCR_DIV_22	0x0b
#define	SLOWPORT_CCR_DIV_24	0x0c
#define	SLOWPORT_CCR_DIV_26	0x0d
#define	SLOWPORT_CCR_DIV_28	0x0e
#define	SLOWPORT_CCR_DIV_30	0x0f

/*
 * PCR values.  PCR configure the mode of the interfac3
 */
#define	SLOWPORT_MODE_FLASH		0x00	
#define	SLOWPORT_MODE_LUCENT		0x01
#define	SLOWPORT_MODE_PMC_SIERRA	0x02
#define	SLOWPORT_MODE_INTEL_UP		0x03
#define	SLOWPORT_MODE_MOTOROLA_UP	0x04

/*
 * ADC values.  Defines data and address bus widths
 */
#define	SLOWPORT_ADDR_WIDTH_8		0x0000
#define	SLOWPORT_ADDR_WIDTH_16		0x0001
#define	SLOWPORT_ADDR_WIDTH_24		0x0002
#define	SLOWPORT_ADDR_WIDTH_32		0x0003
#define	SLOWPORT_DATA_WIDTH_8		0x0000
#define	SLOWPORT_DATA_WIDTH_16		0x0100
#define	SLOWPORT_DATA_WIDTH_24		0x0200
#define	SLOWPORT_DATA_WIDTH_32		0x0300

/*
 * Masks and shifts for various fields in the WTC and RTC registers
 */
#define	SLOWPORT_WRTC_MASK_HD		0x0003
#define	SLOWPORT_WRTC_MASK_SU		0x003c
#define	SLOWPORT_WRTC_MASK_PW		0x03c0

#define	SLOWPORT_WRTC_SHIFT_HD		0x00
#define	SLOWPORT_WRTC_SHIFT_SU		0x02
#define	SLOWPORT_WRTC_SHFIT_PW		0x06


/*
 * Boards may multiplex different devices on the 2nd channel of 
 * the slowport interface that each need different configuration 
 * settings.  For example, the IXDP2400 uses channel 2 on the interface 
 * to access the CPLD, the switch fabric card, and te media card.  Each 
 * one needs a different mode so drivers must save/restore the mode 
 * before and after each operation.  
 *
 * acquire_slowport(&your_config);
 * ...
 * do slowport operations
 * ...
 * release_slowport();
 *
 * Note that while you have the slowport, you are holding a spinlock,
 * so do not do anything you would not normally do if you had explicitly
 * acquired any lock.  
 *
 * The configuration only affects device 2 on the slowport, so the
 * MTD map driver does not acquire/release the slowport.  
 */
#ifndef __ASSEMBLY__
struct slowport_cfg {
	unsigned long	CCR;		/* Clock divide */
	unsigned long 	WTC;		/* Write Timing Control */
	unsigned long	RTC;		/* Read Timing Control */
	unsigned long	PCR;		/* Protocol Control Register */	
	unsigned long	ADC;		/* Address/Data Width Control */
};

void acquire_slowport(struct slowport_cfg *);
void release_slowport(void);
#endif

/*
 * TODO: GPIO registers & GPIO interface
 */


/*
 * "Global" registers...whatever that's supposed to mean.
 */
#define GLOBAL_REG_BASE		(GLOBAL_REG_VIRT_BASE + 0x0a00)
#define GLOBAL_REG(reg)		(volatile unsigned long*)(GLOBAL_REG_BASE | reg)
#define IXP2000_RESET0      	GLOBAL_REG(0x0c)
#define IXP2000_RESET1      	GLOBAL_REG(0x10)
#define IXP2000_CCR            	GLOBAL_REG(0x14)
#define	IXP2000_STRAP_OPTIONS  	GLOBAL_REG(0x18)

#define RSTALL           	(1 << 16)

#ifndef __ASSEMBLY__
static inline unsigned int npu_has_flash(void)
{
        return  ((*IXP2000_STRAP_OPTIONS) & (CFG_BOOT_PROM));
}

static inline unsigned int npu_is_pcimaster(void)
{
        return ((*IXP2000_STRAP_OPTIONS) & (CFG_PCI_BOOT_HOST));
}

static inline unsigned int npu_is_master(void)
{
        return ((npu_has_flash()) && (npu_is_pcimaster()) );
}
#endif

#endif /* _IXP2000_H_ */
