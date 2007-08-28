/*
 * Private Definitions for IOP310 MU 
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _MU_PRIVATE_H_
#define _MU_PRIVATE_H_

#include <asm/arch/message.h>

#define DEFAULT_MU_IRQ_THRESH  10

#define FULL_MASK       0xffffffff
#define NULL_MASK       0x00000000

#define MAX_MSG_DESC    32
#define MAX_MFA_DESC	128

#define FIQ2ISR_MU      0x00000004
#define IRQISR_MU       0x00000100

#define INBOUND_REG_UNDEF   0x00
#define INBOUND_REG_0       0x01
#define INBOUND_REG_1       0x02
#define INBOUND_REG_BOTH    (INBOUND_REG_0 | INBOUND_REG_1)

#define OUTBOUND_REG_0      0x01
#define OUTBOUND_REG_1      0x02

#define MSG_FREE_HARDMODE   0x01
#define MSG_FREE_SOFTMODE   0x02

#define MUIISR_IBMSG_0_INT      0x0001
#define MUIISR_IBMSG_1_INT      0x0002
#define MUIISR_IBDB_FIQ2_INT    0x0004
#define MUIISR_IBDB_IRQ_INT     0x0008
#define MUIISR_IBPQ_INT         0x0010
#define MUIISR_IBFQF_INT        0x0020
#define MUIISR_IR_INT           0x0040

#define MUIIMR_IBMSG_0_MASK     0x0001
#define MUIIMR_IBMSG_1_MASK     0x0002
#define MUIIMR_IBDB_FIQ2_MASK   0x0004
#define MUIIMR_IBDB_IRQ_MASK    0x0008
#define MUIIMR_IBPQ_MASK        0x0010
#define MUIIMR_IBFQF_MASK       0x0020
#define MUIIMR_IR_MASK          0x0040

#define MUOISR_OBMSG_0_INT      0x0001
#define MUOISR_OBMSG_1_INT      0x0002
#define MUOISR_OBDB_INT         0x0004
#define MUOISR_OBPQ_INT         0x0008
#define MUOISR_PCI_INTA         0x0010
#define MUOISR_PCI_INTB         0x0020
#define MUOISR_PCI_INTC         0x0040
#define MUOISR_PCI_INTD         0x0080

#define MUOIMR_OBMSG_0_MASK     0x0001
#define MUOIMR_OBMSG_1_MASK     0x0002
#define MUOIMR_OBDB_MASK        0x0004
#define MUOIMR_OBPQ_MASK        0x0008
#define MUOIMR_PCI_MASKA        0x0010
#define MUOIMR_PCI_MASKB        0x0020
#define MUOIMR_PCI_MASKC        0x0040
#define MUOIMR_PCI_MASKD        0x0080

/* MU hardware registers */
typedef struct _mu_regs
{
	volatile u32 IMR0;
	volatile u32 IMR1;
	volatile u32 OMR0;
	volatile u32 OMR1;
	volatile u32 IDR;
	volatile u32 IISR;
	volatile u32 IIMR;
	volatile u32 ODR;
	volatile u32 OISR;
	volatile u32 OIMR;
	u32 reserved_0[7];
	volatile u32 MUCR;
	volatile u32 QBAR;
	u32 reserved_1[3];
	volatile u32 IFHPR;
	volatile u32 IFTPR;
	volatile u32 IPHPR;
	volatile u32 IPTPR;
	volatile u32 OFHPR;
	volatile u32 OFTPR;
	volatile u32 OPHPR;
	volatile u32 OPTPR;
	volatile u32 IAR;
} mu_regs_t;

/** MU Message Registers **/


/* MU Message Registers Descriptor */
typedef struct _mu_msgreg
{
	atomic_t in_use;			/* usage flag */
	atomic_t res_init;			/* resource usage flag */
	struct list_head free_stack;	/* free descriptor stack */
	struct list_head process_q;	/* process queue */
	spinlock_t free_lock;		/* free stack spinlock */
	spinlock_t process_lock;	/* process queue spinlock */
	mu_msg_cb_t callback_0;		/* callback for inbound 0 */
	mu_msg_cb_t callback_1;		/* callback for inbound 1 */
	struct tq_struct msg_task;	/* message task */
} mu_msg_t;

/** MU Doorbell Registers **/

/* MU Doorbell Registers descriptor */
typedef struct _mu_dbreg
{
	atomic_t in_use;			/* usage flag */
	mu_db_cb_t callback;		/* callback */
	u32 reg_val;				/* register value */
	spinlock_t reg_lock;		/* register value spinlock */
	struct tq_struct db_task;	/* doorbell task */
} mu_db_t;

/** Circular Queues registers **/

/* MU circular queues registers descriptor */
typedef struct _mu_cqreg
{
	atomic_t in_use;			/* usage flag */
	atomic_t init_ref;			/* Initialization reference count */
	u32 q_size;					/* Size of the queues */
	u32 num_entries;			/* Number of entries in Q */
	spinlock_t ib_free_lock;	/* spinlock for Inbound free queue */
	spinlock_t ib_post_lock;	/* spinlock for Inbound free queue */
	spinlock_t ob_free_lock;	/* spinlock for Outbound free queue */
	spinlock_t ob_post_lock;	/* spinlock for Outbound post queue */
	mu_cq_cb_t callback;		/* Inbound callback function */
	struct tq_struct cq_task;	/* circular queues task */
	u32 inb_free_q_top;			/* Inbound free queue top */
	u32 inb_free_q_end;			/* Inbound free queue bottom */
	u32 inb_post_q_top;			/* Inbound post queue top */
	u32 inb_post_q_end;			/* Inbound post queue bottom */
	u32 outb_free_q_top;		/* Outbound free queue top */
	u32 outb_free_q_end;		/* Outbound free queue bottom */
	u32 outb_post_q_top;		/* Outbound post queue top */
	u32 outb_post_q_end;		/* Outbound post queue bottom */
} mu_cq_t;

/* MU Index Registers */

/* MU Index Registers Descriptor */
typedef struct _mu_ir_reg
{
	atomic_t in_use;			/* Usage flag */
	mu_ir_cb_t callback;		/* callback function */
	u32 offset;					/* offset of address */
	struct tq_struct ir_task;	/* Index Registers task */
} mu_ir_t;


/* MU device descriptor */
typedef struct _mu_desc
{
	u32 irq;					/* interrupt number */
	mu_regs_t *regs;			/* MU local mapped registers */
	atomic_t irq_thresh;		/* irq threshold */
	mu_msg_t msg_desc;			/* MU Msg Register */
	mu_db_t db_desc;			/* MU Doorbell registers */
	mu_cq_t cq_desc;			/* MU Circular Queues Registers */
	mu_ir_t ir_desc;			/* MU INDEX Registers */
} iop310_mu_t;


#define MSG_ENTRY(list) list_entry((list), mu_msg_desc_t, link)

extern void *mu_mem;

#endif
