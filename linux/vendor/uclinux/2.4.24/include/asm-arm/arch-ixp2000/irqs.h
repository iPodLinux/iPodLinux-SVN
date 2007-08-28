/*
 * linux/include/asm-arm/arch-ixp2000/irqs.h
 *
 * Original Author:	Naeem Afzal <naeem.m.afzal@intel.com>
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright:	(C) 2002 Intel Corp.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IRQS_H
#define _IRQS_H

#define	IRQ_IXP2000_SWI			0 /* soft interrupt */
#define	IRQ_IXP2000_ERRSUM		1 /* OR of all bits in ErrorStatus reg*/
#define	IRQ_IXP2000_UART		2
#define	IRQ_IXP2000_GPIO		3
#define	IRQ_IXP2000_TIMER1     		4
#define	IRQ_IXP2000_TIMER2     		5
#define	IRQ_IXP2000_TIMER3     		6
#define	IRQ_IXP2000_TIMER4     		7
#define	IRQ_IXP2000_PMU        		8               
#define	IRQ_IXP2000_SPF        		9  /* Slow port framer IRQ */
#define	IRQ_IXP2000_DMA1      		10
#define	IRQ_IXP2000_DMA2      		11
#define	IRQ_IXP2000_DMA3      		12
#define	IRQ_IXP2000_PCI_DOORBELL	13
#define	IRQ_IXP2000_ME_ATTN       	14 
#define	IRQ_IXP2000_THDA0   		15 /* thread 0-31A */
#define	IRQ_IXP2000_THDA1  		16 /* thread 32-63A */
#define	IRQ_IXP2000_THDA2		17 /* thread 64-95A */
#define	IRQ_IXP2000_THDA3 		18 /* thread 96-127A */
#define	IRQ_IXP2000_THDB0  		19 /* thread 0-31 B */
#define	IRQ_IXP2000_THDB1  		20 /* thread 32-63B */
/* only 64 threads supported for IXP2400, rest or for IXP2800*/
#define	IRQ_IXP2000_THDB2  		21 /* thread 64-95B */
#define	IRQ_IXP2000_THDB3 		22 /* thread 96-127B */
#define	IRQ_IXP2000_PCI   		23 /* PCI INTA or INTB */

#define	NR_IXP2000_IRQS         	24

/* 
 * IXDP2400 specific IRQs
 */
#define	IRQ_IXDP2400(x)			(NR_IXP2000_IRQS + (x))

#define	IRQ_IXDP2400_SETHERNET		IRQ_IXDP2400(0) /* Slave NPU NIC irq */
#define	IRQ_IXDP2400_INGRESS_NPU	IRQ_IXDP2400(1) /* Slave NPU irq */
#define	IRQ_IXDP2400_METHERNET		IRQ_IXDP2400(2) /* Master NPU NIC irq */
#define	IRQ_IXDP2400_MEDIA_PCI		IRQ_IXDP2400(3) /* Media on PCI irq */
#define	IRQ_IXDP2400_MEDIA_SP		IRQ_IXDP2400(4) /* Media on SlowPort */
#define	IRQ_IXDP2400_SF_PCI		IRQ_IXDP2400(5) /* Sw Fab. on PCI */
#define	IRQ_IXDP2400_SF_SP		IRQ_IXDP2400(6) /* Switch Fab on SP */
#define	IRQ_IXDP2400_PMC		IRQ_IXDP2400(7) /* PMC slot ineterrupt*/
#define	IRQ_IXDP2400_TVM		IRQ_IXDP2400(8) /* Temp & Voltage */

#define	IRQ_IXP2000_INTERRUPT   ((IRQ_IXDP2400_TVM)+1)  

#undef NR_IRQS
#define NR_IRQS ((IRQ_IXP2000_INTERRUPT)+1)

#endif /*_IRQS_H*/
