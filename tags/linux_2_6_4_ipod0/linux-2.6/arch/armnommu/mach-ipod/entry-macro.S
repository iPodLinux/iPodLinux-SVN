/*
 * arch/armnommu/mach-ipod/entry-macro.S
 *
 * Copyright (c) 2004 Bernard Leach (leachbj@bouncycastle.org)
 *
 * defines machine dependent entry macros.
 * included in the arch/armnommu/kernel/entry-armv.S
 */

#if defined(CONFIG_ARCH_IPOD)

#include <asm/arch/irqs.h>

#define IDE_INT0_MASK	(1 << IDE_INT0_IRQ)
#define SER0_MASK	(1 << SER0_IRQ)
#define I2S_MASK	(1 << I2S_IRQ)
#define SER1_MASK	(1 << SER1_IRQ)
#define TIMER1_MASK	(1 << TIMER1_IRQ)
#define GPIO_MASK	(1 << GPIO_IRQ)
#define DMA_OUT_MASK	(1 << DMA_OUT_IRQ)

		.macro	disable_fiq
.ifdef NOTDEF
		mrs	r13, spsr
		orr	r13, r13, #F_BIT
		msr	spsr_c, r13
.endif
		ldr	pc, cop_fiq_loc

cop_fiq_loc:	.word	cop_fiq

		.endm

		.macro  get_irqnr_and_base, irqnr, irqstat, base, tmp

		ldr 	\base, =(0xcf001000)
		ldr	\irqstat, [\base]

#if 1
		tst	\irqstat, #DMA_OUT_MASK
		movne	\irqnr, #DMA_OUT_IRQ
		bne	1001f

		tst	\irqstat, #GPIO_MASK
		movne	\irqnr, #GPIO_IRQ
		bne	1001f

		tst	\irqstat, #IDE_INT0_MASK
		movne	\irqnr, #IDE_INT0_IRQ
		bne	1001f

		tst	\irqstat, #SER1_MASK
		movne	\irqnr, #SER1_IRQ
		bne	1001f

		tst	\irqstat, #I2S_MASK
		movne	\irqnr, #I2S_IRQ
		bne	1001f

		tst	\irqstat, #SER0_MASK
		movne	\irqnr, #SER0_IRQ
		bne	1001f

		tst	\irqstat, #TIMER1_MASK
		movne	\irqnr, #TIMER1_IRQ
		bne	1001f

1001:

#else
		mov     \irqnr, #0
1001:
		tst     \irqstat, #1
		bne     1002f
		add     \irqnr, \irqnr, #1
		mov     \irqstat, \irqstat, lsr #1
		cmp     \irqnr, #32
		bcc     1001b
1002:		/* EQ will be set if we reach 32 */

#endif
		.endm

		.macro irq_prio_table
		.endm

#endif

