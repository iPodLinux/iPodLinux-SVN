/*
 * linux/include/asm-arm/arch-adifcc/irqs.h
 *
 * Author:	Deepak Saxena <dsaxena@mvista.com>
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define IRQ_XS80200_BCU		0	/* Bus Control Unit */
#define IRQ_XS80200_PMU		1	/* Performance Monitoring Unit */
#define IRQ_XS80200_EXTIRQ	2	/* external IRQ signal */
#define IRQ_XS80200_EXTFIQ	3	/* external IRQ signal */

#define XSCALE_PMU_IRQ		IRQ_XS80200_PMU

#define NR_XS80200_IRQS		4
#define NR_IRQS			NR_XS80200_IRQS

#define	IRQ_XSCALE_PMU		IRQ_XS80200_PMU

#ifdef CONFIG_ARCH_BRH

/* Interrupts available on the BRH */
#define	IRQ_BRH(x)		(NR_XS80200_IRQS + (x))

#define IRQ_BRH_SWI		IRQ_BRH(0)
#define	IRQ_BRH_TIMERA		IRQ_BRH(1)
#define IRQ_BRH_TIMERB		IRQ_BRH(2)
#define IRQ_BRH_ERROR		IRQ_BRH(7)
#define IRQ_BRH_DMA_EOT		IRQ_BRH(8)
#define IRQ_BRH_DMA_PARITY	IRQ_BRH(9)
#define IRQ_BRH_DMA_TABORT	IRQ_BRH(10)
#define IRQ_BRH_DMA_MABORT	IRQ_BRH(11)
#define IRQ_BRH_PCI_PERR	IRQ_BRH(16)
#define IRQ_BRH_PCI_SERR	IRQ_BRH(19)
#define IRQ_BRH_ATU_PERR	IRQ_BRH(20)
#define IRQ_BRH_ATU_TABORT	IRQ_BRH(21)
#define IRQ_BRH_ATU_MABORT	IRQ_BRH(22)
#define IRQ_BRH_UART_A		IRQ_BRH(24)
#define IRQ_BRH_UART_B		IRQ_BRH(25)
#define IRQ_BRH_PCI_INT_A	IRQ_BRH(26)
#define IRQ_BRH_PCI_INT_B	IRQ_BRH(27)
#define IRQ_BRH_PCI_INT_C	IRQ_BRH(28)
#define IRQ_BRH_PCI_INT_D	IRQ_BRH(29)
#define IRQ_BRH_SOFT_RESET	IRQ_BRH(30)

#undef NR_IRQS
#define NR_IRQS ((IRQ_BRH_SOFT_RESET)+1)

#endif


