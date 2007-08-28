/*
 * Definitions for IOP310 AAU 
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _IOP310_AAU_H_
#define _IOP310_AAU_H_


#define DEFAULT_AAU_IRQ_THRESH  10

#define MAX_AAU_DESC    1024	/*64 */
#define AAU_SAR_GROUP   	4


#define AAU_DESC_DONE   	 0x0010
#define AAU_INCOMPLETE  	 0x0020
#define AAU_HOLD			 0x0040
#define AAU_END_CHAIN		 0x0080
#define AAU_COMPLETE		 0x0100
#define AAU_NOTIFY			 0x0200
#define AAU_NEW_HEAD		 0x0400

#define AAU_USER_MASK	(AAU_NOTIFY | AAU_INCOMPLETE | \
						 AAU_HOLD | AAU_COMPLETE)

#define DESC_HEAD           0x0010
#define DESC_TAIL           0x0020

/* result writeback */
#define AAU_DCR_WRITE           	0x80000000
/* source block extension */
#define AAU_DCR_BLK_EXT         	0x02000000
#define AAU_DCR_BLKCTRL_8_XOR       0x00400000
#define AAU_DCR_BLKCTRL_7_XOR       0x00080000
#define AAU_DCR_BLKCTRL_6_XOR       0x00010000
#define AAU_DCR_BLKCTRL_5_XOR       0x00002000
#define AAU_DCR_BLKCTRL_4_XOR       0x00000400
#define AAU_DCR_BLKCTRL_3_XOR       0x00000080
#define AAU_DCR_BLKCTRL_2_XOR       0x00000010
#define AAU_DCR_BLKCTRL_1_XOR       0x00000002
/* first block direct fill instead of XOR to buffer */
#define AAU_DCR_BLKCTRL_1_DF		0x0000000E
/* interrupt enable */
#define AAU_DCR_IE              	0x00000001

#define DCR_BLKCTRL_OFFSET      3


/* AAU callback */
typedef void (*aau_callback_t) (void *buf_id);

/* hardware descriptor */
typedef struct _aau_desc
{
	u32 NDA;					/* next descriptor address */
	u32 SAR[AAU_SAR_GROUP];		/* src addrs */
	u32 DAR;					/* destination addr */
	u32 BC;						/* byte count */
	u32 DC;						/* descriptor control */
	u32 SARE[AAU_SAR_GROUP];	/* extended src addrs */
} aau_desc_t;

/* user SGL format */
typedef struct _aau_sgl
{
	aau_desc_t aau_desc;		/* AAU HW Desc */
	u32 status;
	struct _aau_sgl *next;		/* pointer to next SG */
	void *dest;					/* destination addr */
	void *src[AAU_SAR_GROUP];	/* source addr[4] */
	void *ext_src[AAU_SAR_GROUP];	/* ext src addr[4] */
	u32 total_src;				/* total number of source */
} aau_sgl_t;

/* header for user SGL */
typedef struct _aau_head
{
	u32 total;
	u32 status;					/* SGL status */
	aau_sgl_t *list;			/* ptr to head of list */
	aau_callback_t callback;	/* callback func ptr */
} aau_head_t;

/* prototypes */
int aau_request(u32 *, const char *);
int aau_queue_buffer(u32, aau_head_t *);
void aau_suspend(u32);
void aau_resume(u32);
void aau_free(u32);
void aau_set_irq_threshold(u32, int);
void aau_return_buffer(u32, aau_sgl_t *);
aau_sgl_t *aau_get_buffer(u32, int);
int aau_memcpy(void *, void *, u32);

#endif
/* EOF */
