/*
 * Copyright (c) 2003, Bernard Leach <leachbj@bouncycastle.org>
 */

#ifndef __ASM_ARCH_IRQS_H__
#define __ASM_ARCH_IRQS_H__

#define IDE_INT0_IRQ	1
#define SER0_IRQ	4
#define I2S_IRQ		5
#define SER1_IRQ	7
#define TIMER1_IRQ	11
#define GPIO_IRQ	14
#define DMA_OUT_IRQ	30
#define DMA_IN_IRQ	31

#define NR_IRQS	32

#define VALID_IRQ(x)	(x==IDE_INT0_IRQ||x==SER0_IRQ||x==I2S_IRQ||x==SER1_IRQ||x==TIMER1_IRQ||x==GPIO_IRQ||x==DMA_OUT_IRQ||x==DMA_IN_IRQ)

#endif

