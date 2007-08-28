/*
 * Private Definitions for XScale AAU 
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _AAU_PRIVATE_H_
#define _AAU_PRIVATE_H_

#define SLEEP_TIME          50
#define AAU_DESC_SIZE		48
#define AAU_INT_MASK        0x0020

#define AAU_ACR_CLEAR           0x00000000
#define AAU_ACR_ENABLE          0x00000001
#define AAU_ACR_CHAIN_RESUME    0x00000002
#define AAU_ACR_512_BUFFER      0x00000004

#define AAU_ASR_CLEAR           0x00000320
#define AAU_ASR_MA_ABORT        0x00000020
#define AAU_ASR_ERROR_MASK      AAU_ASR_MA_ABORT
#define AAU_ASR_DONE_EOT        0x00000200
#define AAU_ASR_DONE_EOC        0x00000100
#define AAU_ASR_DONE_MASK       (AAU_ASR_DONE_EOT | AAU_ASR_DONE_EOC)
#define AAU_ASR_ACTIVE          0x00000400
#define AAU_ASR_MASK            (AAU_ASR_ERROR_MASK | AAU_ASR_DONE_MASK)

/* software descriptor */
typedef struct _sw_aau
{
	aau_desc_t aau_desc;		/* AAU HW Desc */
	u32 status;
	struct _aau_sgl *next;		/* pointer to next SG */
	void *dest;					/* destination addr */
	void *src[AAU_SAR_GROUP];	/* source addr[4] */
	void *ext_src[AAU_SAR_GROUP];	/* ext src addr[4] */
	u32 total_src;				/* total number of source */
	struct list_head link;		/* Link to queue */
	u32 aau_phys;				/* AAU Phys Addr (aligned) */
	u32 desc_addr;				/* unaligned HWDESC virtual addr */
	u32 sgl_head;
	struct _sw_aau *head;		/* head of list */
	struct _sw_aau *tail;		/* tail of list */
} sw_aau_t;

/* AAU registers */
typedef struct _aau_regs_t
{
	volatile u32 ACR;			/* Accelerator Control Register */
	volatile u32 ASR;			/* Accelerator Status Register */
	volatile u32 ADAR;			/* Descriptor Address Register */
	volatile u32 ANDAR;			/* Next Desc Address Register */
	volatile u32 LSAR[AAU_SAR_GROUP];	/* source addrs */
	volatile u32 LDAR;			/* local destination address register */
	volatile u32 ABCR;			/* byte count */
	volatile u32 ADCR;			/* Descriptor Control */
	volatile u32 LSARE[AAU_SAR_GROUP];	/* extended src addrs */
} aau_regs_t;


/* device descriptor */
typedef struct _iop310_aau_t
{
	const char *dev_id;			/* Device ID */
	struct list_head process_q;	/* Process Q */
	struct list_head hold_q;	/* Holding Q */
	spinlock_t process_lock;	/* PQ spinlock */
	spinlock_t hold_lock;		/* HQ spinlock */
	aau_regs_t *regs;			/* AAU registers */
	int irq;					/* IRQ number */
	sw_aau_t *last_aau;			/* ptr to last AAU desc */
	struct tq_struct aau_task;	/* AAU task entry */
	wait_queue_head_t wait_q;	/* AAU wait queue */
	atomic_t ref_count;			/* AAU ref count */
	atomic_t irq_thresh;		/* IRQ threshold */
} iop310_aau_t;

#define SW_ENTRY(list) list_entry((list), sw_aau_t, link)

#endif
