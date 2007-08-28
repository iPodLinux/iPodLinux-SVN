/*
 * Definitions for IQ80310 messageing unit
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _IOP310_MU_H_
#define _IOP310_MU_H_

#define MUDB_PCI_INTD           0x80000000
#define MUDB_PCI_INTC           0x40000000
#define MUDB_PCI_INTB           0x20000000
#define MUDB_PCI_INTA           0x10000000
#define MUDB_PCI_SOFT27         0x08000000
#define MUDB_PCI_SOFT26         0x04000000
#define MUDB_PCI_SOFT25         0x02000000
#define MUDB_PCI_SOFT24         0x01000000
#define MUDB_PCI_SOFT23         0x00800000
#define MUDB_PCI_SOFT22         0x00400000
#define MUDB_PCI_SOFT21         0x00200000
#define MUDB_PCI_SOFT20         0x00100000
#define MUDB_PCI_SOFT19         0x00080000
#define MUDB_PCI_SOFT18         0x00040000
#define MUDB_PCI_SOFT17         0x00020000
#define MUDB_PCI_SOFT16         0x00010000
#define MUDB_PCI_SOFT15         0x00008000
#define MUDB_PCI_SOFT14         0x00004000
#define MUDB_PCI_SOFT13         0x00002000
#define MUDB_PCI_SOFT12         0x00001000
#define MUDB_PCI_SOFT11         0x00000800
#define MUDB_PCI_SOFT10         0x00000400
#define MUDB_PCI_SOFT9          0x00000200
#define MUDB_PCI_SOFT8          0x00000100
#define MUDB_PCI_SOFT7          0x00000080
#define MUDB_PCI_SOFT6          0x00000040
#define MUDB_PCI_SOFT5          0x00000020
#define MUDB_PCI_SOFT4          0x00000010
#define MUDB_PCI_SOFT3          0x00000008
#define MUDB_PCI_SOFT2          0x00000004
#define MUDB_PCI_SOFT1          0x00000002
#define MUDB_PCI_SOFT0          0x00000001

#define MUCR_QUEUE_ENABLE       0x0001
#define MUCR_QUEUE_4K           0x0002
#define MUCR_QUEUE_8K           0x0004
#define MUCR_QUEUE_16K          0x0008
#define MUCR_QUEUE_32K          0x0010
#define MUCR_QUEUE_64K          0x0020

#define QUEUE_16K               0x04000
#define QUEUE_32K               0x08000
#define QUEUE_64K               0x10000
#define QUEUE_128K              0x20000
#define QUEUE_256K              0x40000

#define ENTRY_4K                4000
#define ENTRY_8K                8000
#define ENTRY_16K              16000
#define ENTRY_32K              32000
#define ENTRY_64K              64000

/* MU Message Descriptors */
typedef struct _mu_msg_desc
{
	struct list_head link;		/* queue entry */
	u8 reg_num;					/* register number */
	u32 message;				/* Message content */
} mu_msg_desc_t;

/* MU msg callback type */
typedef void (*mu_msg_cb_t) (mu_msg_desc_t *);

/* MU Doorbell callback type*/
typedef void (*mu_db_cb_t) (u32);


/* message frame list descriptor */
typedef struct _mfa_list
{
	u32 mfa;					/* message frame address */
	struct _mfa_list *next_list;	/* pointer to next list element */
} mfa_list_t;

/* MU CQ callback type */
typedef void (*mu_cq_cb_t) (u32);

/* MU IR callback type */
typedef void (*mu_ir_cb_t) (u32);


/* prototypes */
int mu_msg_request(u32 *);
int mu_msg_set_callback(u32, u8, mu_msg_cb_t);
int mu_msg_post(u32, u32, u8);
int mu_msg_free(u32, u8);
int mu_db_request(u32 *);
int mu_db_set_callback(u32, mu_db_cb_t);
void mu_db_ring(u32, u32);
int mu_db_free(u32);
int mu_cq_request(u32 *, u32);
int mu_cq_inbound_init(u32, mfa_list_t *, u32, mu_cq_cb_t);
int mu_cq_enable(u32);
u32 mu_cq_get_frame(u32);
int mu_cq_post_frame(u32, u32);
int mu_cq_free(u32);
int mu_ir_request(u32 *);
int mu_ir_set_callback(u32, mu_ir_cb_t);
int mu_ir_free(u32);
void mu_set_irq_threshold(u32, int);

#endif
/* EOF */
