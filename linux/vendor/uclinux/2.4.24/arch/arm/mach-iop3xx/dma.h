/*
 * Private Definitions for XScale DMA 
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _DMA_PRIVATE_H_
#define _DMA_PRIVATE_H_

#define DEFAULT_DMA_IRQ_THRESH  10

#define DMA_DESC_SIZE 24

#define SLEEP_TIME          50

#define DESC_DONE           DMA_DONE
#define DESC_INCOMPLETE     DMA_INCOMPLETE
#define DESC_HEAD           0x0010
#define DESC_TAIL           0x0020

#define DMA_INT_0       0x01
#define DMA_INT_1       0x02
#define DMA_INT_2       0x04
#define DMA_INT_MASK    (DMA_INT_0 | DMA_INT_1 | DMA_INT_2)

#define DMA_CCR_CLEAR               0x00000000
#define DMA_CCR_CHANNEL_ENABLE      0x00000001
#define DMA_CCR_CHAIN_RESUME        0x00000002

#define DMA_CSR_PCI_PAR_ERROR       0x00000001
#define DMA_CSR_PCI_TGT_ABORT       0x00000004
#define DMA_CSR_PCI_MST_ABORT       0x00000008
#define DMA_CSR_INT_MST_ABORT       0x00000020
#define DMA_CSR_EOC_INT             0x00000100
#define DMA_CSR_EOT_INT             0x00000200
#define DMA_CSR_CH_ACTIVE           0x00000400
#define DMA_CSR_DONE_MASK           (DMA_CSR_EOC_INT | DMA_CSR_EOT_INT)

#define DMA_CSR_ERR_MASK            (DMA_CSR_PCI_PAR_ERROR | \
									 DMA_CSR_PCI_TGT_ABORT | \
                                     DMA_CSR_PCI_MST_ABORT | \
									 DMA_CSR_INT_MST_ABORT)

#define DMA_CSR_DONE_MASK           (DMA_CSR_EOC_INT | DMA_CSR_EOT_INT)

/*
 * DMA Software Descriptor
 */
typedef struct _sw_dma
{
	dma_desc_t dma_desc;		/* DMA descriptor pointer */
	u32 status;
	void *data;					/* local virt */
	struct _dma_sgl *next;		/* next descriptor */
	struct list_head link;
	u32 sw_desc;				/* parent address */
	u32 dma_phys;				/* Physical address of DMA */
	u32 desc_addr;				/* desc unaligned alloc addr */
	u32 sgl_head;				/* user SG head */
	struct _sw_dma *tail;		/* chain tail */
	struct _sw_dma *head;		/* chain head */
} sw_dma_t;

/*
 * DMA control register structure
 */
typedef struct _dma_regs
{
	volatile u32 CCR;			/* channel control register */
	volatile u32 CSR;			/* channel status register */
	volatile u32 DAR;			/* descriptor address register */
	volatile u32 NDAR;			/* next descriptor address register */
	volatile u32 PADR;			/* PCI address register */
	volatile u32 PUADR;			/* upper PCI address register */
	volatile u32 LADR;			/* local address register */
	volatile u32 BCR;			/* byte count register */
	volatile u32 DCR;			/* descriptor control register */
} dma_regs_t;


/*
 * DMA channel structure.
 */

typedef struct _dmac_ctrl_t
{
	dmach_t channel;			/* channel */
	u32 lock;					/* Device is allocated */
	struct list_head process_q;
	struct list_head hold_q;
	spinlock_t process_lock;	/* process queue lock */
	spinlock_t hold_lock;		/* hold queue lock */
	int ready;					/* 1 if DMA can occur */
	int active;					/* 1 if DMA is processing data */
	int irq;					/* IRQ used by the channel */
	atomic_t ref_count;			/* resource count for DMA */
#ifndef REGMAP
	struct						/* register offset */
	{
		volatile u32 *CCR;		/* channel control register */
		volatile u32 *CSR;		/* channel status register */
		volatile u32 *DAR;		/* descriptor address register */
		volatile u32 *NDAR;		/* next descriptor address register */
		volatile u32 *PADR;		/* PCI address register */
		volatile u32 *PUADR;	/* upper PCI address register */
		volatile u32 *LADR;		/* local address register */
		volatile u32 *BCR;		/* byte count register */
		volatile u32 *DCR;		/* descriptor control register */
	}
	reg_addr;
#endif
	atomic_t last_dma_lock;		/* last DMA descriptor lock */
	atomic_t irq_thresh;
	wait_queue_head_t wait_q;	/* wait queue for user to sleep on */
	struct tq_struct dma_task;	/* tasklet */
	sw_dma_t *last_desc;		/* last descriptor for DMAC */
	dma_regs_t *regs;			/* points to DMA registers */
	const char *device_id;		/* Device name */
} iop310_dma_t;

#define SW_ENTRY(list) list_entry((list), sw_dma_t, link)

#endif
 /*EOF*/
