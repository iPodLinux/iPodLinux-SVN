/*
 * linux/include/asm-arm/arch-ixp1200/irqs.h
 *
 * Copyright (C) 2000, Intel Corporation, Inc.
 * Copyright (C) 2001-2002, MontaVista Software, Inc.
 *
 * Changelog:
 *  17-mar-2000 Uday Naik      Created
 *  25-sep-2001 dsaxena
 *  	Moved to 2.4.x kernel & cleanup
 *  01-Mar-2002 dsaxena
 *  	Support for all interrupt sources
 *
 */

#ifndef __IXP1200_IRQ_H_
#define __IXP1200_IRQ_H_

#define NR_IRQS			21

/*
 * Interrupts on the IRQ Register
 */
#define	IXP1200_IRQ_CINT	0
#define	IXP1200_IRQ_UENG	1 
#define	IXP1200_IRQ_SRAM	2
#define	IXP1200_IRQ_UART	3
#define	IXP1200_IRQ_RTC		4
#define	IXP1200_IRQ_SDRAM	5     

/*
 * Interrupts on the PCI Interrupt Register
 */
#define	IXP1200_IRQ_SWI		6
#define	IXP1200_IRQ_TIMER1	7
#define	IXP1200_IRQ_TIMER2	8
#define	IXP1200_IRQ_TIMER3	9
#define	IXP1200_IRQ_TIMER4	10
#define	IXP1200_IRQ_DOORBELL	11
#define	IXP1200_IRQ_DMA1	12
#define	IXP1200_IRQ_DMA2	13
#define	IXP1200_IRQ_PCI		14
#define	IXP1200_IRQ_DMA1NB	15
#define	IXP1200_IRQ_DMA2NB	16
#define	IXP1200_IRQ_BIST	17
#define	IXP1200_IRQ_I2O		18
#define	IXP1200_IRQ_PWRM	19
#define	IXP1200_IRQ_PCI_ERR	20

#endif

