/**************************************************************************
 * arch/arm/mach-iop310/message.c
 *
 * Support functions for the Intel 80310 MU.
 * (see also Documentation/arm/XScale/IOP310/message.txt)
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Todos:  Thorough Error handling
 *
 *  History:    (07/18/2001, DJ) Initial Creation
 *              (08/22/2001, DJ) changed spinlock calls to no save flags
 *              (08/27/2001, DJ) Added irq threshold handling
 *		(10/11/2001, DJ) Tested with host side driver. Circular
 *				   Queues working.
 *		(10/12/2001, DJ) Rest of MU works now
 *************************************************************************/
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <asm/system.h>
#include <asm/types.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/iq80310.h>
#include <asm/arch/irqs.h>
#include <asm/memory.h>
#include <asm/arch/message.h>
#include <asm/arch/memory.h>

#include "message.h"

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#undef DEBUG
#ifdef DEBUG
#define DPRINTK(s, args...) printk("80310MU: " s, ## args)
#else
#define DPRINTK(s, args...)
#endif

void *mu_mem = NULL;

/* globals */
static iop310_mu_t mu_dev;

/* static prototypes */
static int __init mu_init(void);
static void mu_irq_handler(int, void *, struct pt_regs *);
static void mu_msg_irq_task(void *);
static void mu_db_irq_task(void *);
static void mu_cq_irq_task(void *);
static void mu_ir_irq_task(void *);

/*===========================MU Message Registers========================*/

/*=======================================================================*/
/*  Procedure:  mu_msg_request()                                         */
/*                                                                       */
/*  Description:    This function is used to request the ownership of    */
/*                  MU Message Registers component ownership             */
/*                                                                       */
/*  Parameters:  *mu_context -- pass by reference int, written by API    */
/*                                                                       */
/*  Returns:     mu_context -- pointer to MU descriptor, ownership       */
/*                                granted.                               */
/*               NULL -- ownership denied                                */
/*                                                                       */
/*  Notes/Assumptions: The requester if successful will own the entire   */
/*                     MU message registers mechanism.                   */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
int mu_msg_request(u32 * mu_context)
{
	iop310_mu_t *mu = &mu_dev;
	mu_msg_t *msg = &mu->msg_desc;
	int i;
	mu_msg_desc_t *msg_desc = NULL;

	/* denied if already owned */
	if(atomic_read(&msg->in_use))
	{
		return -EBUSY;
	}

	/* set ownership flag */
	atomic_inc(&msg->in_use);

	/* if we haven't allocated resources */
	if(!atomic_read(&msg->res_init))
	{
		/* allocate message descriptors */
		for(i = 0; i < MAX_MSG_DESC; i++)
		{
			/* allocate memory for message descriptor */
			msg_desc = (mu_msg_desc_t *) kmalloc(sizeof(mu_msg_desc_t),
												 GFP_KERNEL);
			if(!msg_desc)
			{
				DPRINTK("Unable to allocate message descriptor for MU\n");
				return -ENOMEM;
			}

			/* Init message descriptor */
			msg_desc->message = 0;
			msg_desc->reg_num = INBOUND_REG_UNDEF;

			/* put descriptor on free stack */
			spin_lock_irq(&msg->free_lock);
			list_add(&msg_desc->link, &msg->free_stack);
			spin_unlock_irq(&msg->free_lock);
		}
		/* set resource init flag */
		atomic_inc(&msg->res_init);
	}

	/* Unmask outbound interrupts */
	*(IOP310_MUOIMR) &= ~(MUOIMR_OBMSG_0_MASK | MUOIMR_OBMSG_1_MASK);

	*mu_context = (u32)mu;
	return 0;
}								/* end of mu_msg_request() */

/*=======================================================================*/
/*  Procedure:  mu_msg_set_callback()                                    */
/*                                                                       */
/*  Description:  This function sets up the callback functions for the   */
/*                Inbound message passing of the MR mechanism. The app   */
/*                calls this can setup callback for either of the        */
/*                inbound registers or have the same callback for both.  */
/*                                                                       */
/*                                                                       */
/*  Parameters:  mu_context - context to MU descriptor                   */
/*               reg - INBOUND_REG_BOTH, INBOUND_REG_0, INBOUND_REG_1    */
/*               func - pointer to callback                              */
/*                                                                       */
/*  Returns:     0: SUCCESS                                              */
/*               -EINVAL - invalid parameters passed in                  */
/*                                                                       */
/*  Notes/Assumptions: The app owns the message registers mechanism.     */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_msg_set_callback(u32 mu_context, u8 reg, mu_msg_cb_t func)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	mu_msg_t *msg = &mu->msg_desc;

	/* make sure callback valid and the correct register number passed in */
	if(!func || (!(reg & INBOUND_REG_0) && !(reg & INBOUND_REG_1)))
	{
		return -EINVAL;
	}

	switch (reg)
	{
	case INBOUND_REG_BOTH:		/* both inbound registers */
		msg->callback_0 = func;
		msg->callback_1 = func;
		/* unmask inbound INTs */
		*(IOP310_MUIIMR) &= ~(MUIIMR_IBMSG_0_MASK | MUIIMR_IBMSG_1_MASK);
		break;
	case INBOUND_REG_0:		/* inbound register 0 */
		msg->callback_0 = func;
		/* unmask inbound INT 0 */
		*(IOP310_MUIIMR) &= ~MUIIMR_IBMSG_0_MASK;
		break;
	case INBOUND_REG_1:		/* inbound register 1 */
		msg->callback_1 = func;
		/* unmask inbound INT 1 */
		*(IOP310_MUIIMR) &= ~MUIIMR_IBMSG_1_MASK;
		break;
	default:					/* this should never be executed */
		printk("We should never get here\n");
		BUG();
		return -EINVAL;
	}

	return 0;
}								/* End of mu_msg_set_callback() */

/*=======================================================================*/
/*  Procedure:  mu_msg_post()                                            */
/*                                                                       */
/*  Description: This function allows the app to post outbound messages  */
/*               by the desired register.                                */
/*                                                                       */
/*                                                                       */
/*  Parameters:  mu_context - context to MU device descriptor            */
/*               val - message value to post                             */
/*               cReg - OUTBOUND_REG_0, OUTBOUND_REG_1                   */
/*                                                                       */
/*  Returns:    0 - success                                              */
/*              -EINVAL - invalid parameter                              */
/*              -EBUSY - register busy                                   */
/*                                                                       */
/*  Notes/Assumptions: The app will figure out which reg to use. The app */
/*                     must already own the message registers.           */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_msg_post(u32 mu_context, u32 val, u8 reg)
{
	iop310_mu_t *mu = (iop310_mu_t *)mu_context;
	/* check the parameters */
	if((reg != OUTBOUND_REG_0) && (reg != OUTBOUND_REG_1))
	{
		DPRINTK("posting to wrong register\n");
		return -EINVAL;
	}

	/* if we are posting on register 0 */
	if(reg == OUTBOUND_REG_0)
	{
		/* if outbound INT bit still asserted, previous message has not */
		/* been read. cannot write a new message */
		if(*(IOP310_MUOISR) & MUOISR_OBMSG_0_INT)
		{
			DPRINTK("INT 0 for register still asserted\n");
			return -EBUSY;
		}

		/* write message to register */
		*(IOP310_MUOMR0) = val;
	}
	else						/* we are posting register 1 */
	{
		/* register busy */
		if(*(IOP310_MUOISR) & MUOISR_OBMSG_1_INT)
		{
			DPRINTK("INT 1 for register still asserted\n");
			return -EBUSY;
		}

		/* post message */
		*(IOP310_MUOMR1) = val;
	}
	return 0;
}								/* End of mu_msg_post() */

/*=======================================================================*/
/*  Procedure:  mu_msg_irq_task()                                        */
/*                                                                       */
/*  Description:  This function is the "bottom half" task of the irq     */
/*                handler that handles the message registers inbound     */
/*                messages. It will attempt to pass the message to the   */
/*                app via the provided callback function                 */
/*                                                                       */
/*                                                                       */
/*  Parameters:  dev - pointer to MU device descriptor                   */
/*                                                                       */
/*  Returns:     NONE                                                    */
/*                                                                       */
/*  Notes/Assumptions:  The app is responsible for providing a mechanism */
/*                      to store the messages via the callback function  */
/*                      when the task hands them off. This task is       */
/*                      scheduled by the MU irq handler.                 */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
static void mu_msg_irq_task(void *dev)
{
	iop310_mu_t *mu = (iop310_mu_t *) dev;
	mu_msg_t *msg = &mu->msg_desc;
	mu_msg_desc_t *msg_desc = NULL;

	/* while we have message descriptors to process */
	while(!list_empty(&msg->process_q))
	{
		/* get a message descriptor from process queue */
		spin_lock_irq(&msg->process_lock);
		msg_desc = MSG_ENTRY(msg->process_q.next);
		list_del(&msg_desc->link);
		spin_unlock_irq(&msg->process_lock);

		/* if register 0 message */
		if((msg_desc->reg_num == INBOUND_REG_0) && msg->callback_0)
		{
			msg->callback_0(msg_desc);
		}
		/* or register 1 message */
		else if((msg_desc->reg_num == INBOUND_REG_1) && msg->callback_1)
		{
			msg->callback_1(msg_desc);
		}

		/* reinit message descriptors */
		msg_desc->reg_num = INBOUND_REG_UNDEF;
		msg_desc->message = 0;

		/* put descriptor back on stack */
		spin_lock_irq(&msg->free_lock);
		list_add(&msg_desc->link, &msg->free_stack);
		spin_unlock_irq(&msg->free_lock);
	}
}								/* End of msg_msg_irq_task() */

/*=======================================================================*/
/*  Procedure:  mu_msg_free()                                            */
/*                                                                       */
/*  Description:  This function is called to release the ownership of    */
/*                the message registers mechanism. Resource is dealloc   */
/*                if requested via parameter                             */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor                    */
/*               mode - MSG_FREE_SOFTMODE, MSG_FREE_HARDMODE             */
/*                                                                       */
/*  Returns:    0 - success                                             */
/*              -EPERM - permissione denied                              */
/*                                                                       */
/*  Notes/Assumptions: Only the owner of the message mechanism shall     */
/*                     call this function                                */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
int mu_msg_free(u32 mu_context, u8 mode)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	mu_msg_t *msg = &mu->msg_desc;
	mu_msg_desc_t *msg_desc = NULL;

	/* check ownership */
	if(!atomic_read(&msg->in_use))
	{
		return -EPERM;
	}

	/* return all the descriptors to free stack */
	while(!list_empty(&msg->process_q))
	{
		spin_lock_irq(&msg->process_lock);
		msg_desc = MSG_ENTRY(msg->process_q.next);
		list_del(&msg_desc->link);
		spin_unlock_irq(&msg->process_lock);

		msg_desc->reg_num = INBOUND_REG_UNDEF;
		msg_desc->message = 0;

		spin_lock_irq(&msg->free_lock);
		list_add(&msg_desc->link, &msg->free_stack);
		spin_unlock_irq(&msg->free_lock);
	}

	/* if hardmode we free all the descriptor memories */
	if(mode == MSG_FREE_HARDMODE)
	{
		while(!list_empty(&msg->free_stack))
		{
			spin_lock_irq(&msg->free_lock);
			msg_desc = MSG_ENTRY(msg->free_stack.next);
			list_del(&msg_desc->link);
			spin_unlock_irq(&msg->free_lock);
			kfree(msg_desc);
		}
		/* unset resource init flag */
		atomic_dec(&msg->res_init);
	}

	msg->callback_0 = NULL;
	msg->callback_1 = NULL;

	/* mask inbound/outbound INTs */
	*(IOP310_MUIIMR) |= (MUIIMR_IBMSG_0_MASK | MUIIMR_IBMSG_1_MASK);
	*(IOP310_MUOIMR) |= (MUOIMR_OBMSG_0_MASK | MUOIMR_OBMSG_1_MASK);

	/* reset ownership flag */
	atomic_dec(&msg->in_use);
	return 0;
}								/* End of mu_msg_free() */

/*==========================MU Doorbell Registers========================*/

/*=======================================================================*/
/*  Procedure:  mu_db_request()                                          */
/*                                                                       */
/*  Description: This function is used to request the ownership of the   */
/*               MU doorbell registers mechanism.                        */
/*                                                                       */
/*  Parameters:  NONE                                                    */
/*                                                                       */
/*  Returns:     iop310_mu_t * - success                                 */
/*               NULL - failed                                           */
/*                                                                       */
/*  Notes/Assumptions: The requestor will own the entire MU db regs if   */
/*                     successful.                                       */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_db_request(u32 * mu_context)
{
	iop310_mu_t *mu = &mu_dev;
	mu_db_t *db = &mu->db_desc;

	/* check ownership flag */
	if(atomic_read(&db->in_use))
	{
		/* already owned */
		return -EBUSY;
	}

	/* set usage flag */
	atomic_inc(&db->in_use);

	/* unmask outbound int */
	*(IOP310_MUOIMR) &= ~(MUOIMR_OBDB_MASK | MUOIMR_PCI_MASKA |
						  MUOIMR_PCI_MASKB | MUOIMR_PCI_MASKC |
						  MUOIMR_PCI_MASKD);

	*mu_context = (u32) mu;
	return 0;
}	/* End of mu_db_request() */

/*=======================================================================*/
/*  Procedure:  mu_db_set_callback()                                     */
/*                                                                       */
/*  Description: This function sets up the callback func that will rcv   */
/*               the inbound doorbell messages.                          */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor                    */
/*               func - pointer to callback function                     */
/*                                                                       */
/*  Returns:     -EINVAL - invalid parameter                             */
/*                0 - success                                            */
/*                                                                       */
/*  Notes/Assumptions: The app owns the db mechanism.                    */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_db_set_callback(u32 mu_context, mu_db_cb_t func)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	mu_db_t *db = &mu->db_desc;

	/* check parameter */
	if(!func)
	{
		return -EINVAL;
	}

	/* set callback */
	db->callback = func;

	/* unmask inbound INTs */
	*(IOP310_MUIIMR) &= ~(MUIIMR_IBDB_FIQ2_MASK | MUIIMR_IBDB_IRQ_MASK);

	return 0;
}	/* End of mu_db_set_callback() */

/*=======================================================================*/
/*  Procedure:  mu_db_ring()                                             */
/*                                                                       */
/*  Description: This funciton sets the outbound db reg bits requested   */
/*               by the app                                              */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor.                   */
/*               mask - doorbell value to set                            */
/*                                                                       */
/*  Returns:     NONE                                                    */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
inline void mu_db_ring(u32 mu_context, u32 mask)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	/* set outbound db register */
	*(IOP310_MUODR) |= mask;
}	/* End of mu_db_ring() */

/*=======================================================================*/
/*  Procedure:  mu_db_irq_task()                                         */
/*                                                                       */
/*  Description: This function is the "bottom half" task for the irq hdl */
/*               that will pass the inbound doorbell register value to   */
/*               the app via callback.                                   */
/*                                                                       */
/*  Parameters:  dev - pointer to MU device descriptor                   */
/*                                                                       */
/*  Returns:     NONE                                                    */
/*                                                                       */
/*  Notes/Assumptions: The app is responsible for providing a mechanism  */
/*                     to parse the doorbell register value after recv   */
/*                     via callback.                                     */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
static void mu_db_irq_task(void *dev)
{
	iop310_mu_t *mu = (iop310_mu_t *) dev;
	mu_db_t *db = &mu->db_desc;
	u32 value;

	/* get latest inbound db value and reset hold var */
	spin_lock_irq(&db->reg_lock);
	value = db->reg_val;
	db->reg_val = 0;
	spin_unlock_irq(&db->reg_lock);

	/* callback if exists */
	if(db->callback)
	{
		db->callback(value);
	}
}	/* End of mu_db_irq_task() */

/*=======================================================================*/
/*  Procedure:  mu_db_free()                                             */
/*                                                                       */
/*  Description: This function releases ownership of the db registers.   */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor.                   */
/*                                                                       */
/*  Returns:     0 - success                                            */
/*               -EPERM - permission denied.                             */
/*                                                                       */
/*  Notes/Assumptions: Only the owner of db shall call this function.    */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_db_free(u32 mu_context)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	mu_db_t *db = &mu->db_desc;

	/* check ownership */
	if(!atomic_read(&db->in_use))
	{
		return -EPERM;
	}

	/* reset usage flag */
	atomic_dec(&db->in_use);

	db->callback = NULL;

	/* reset store value */
	spin_lock_irq(&db->reg_lock);
	db->reg_val = 0;
	spin_unlock_irq(&db->reg_lock);

	/* mask inbound & outbound INTs */
	*(IOP310_MUIIMR) |= (MUIIMR_IBDB_FIQ2_MASK | MUIIMR_IBDB_IRQ_MASK);
	*(IOP310_MUOIMR) |= (MUOIMR_OBDB_MASK | MUOIMR_PCI_MASKA |
						 MUOIMR_PCI_MASKB | MUOIMR_PCI_MASKC |
						 MUOIMR_PCI_MASKD);
	return 0;
}								/* End of mu_db_free() */

/*===========================MU Circular Queues==========================*/

/*=======================================================================*/
/*  Procedure:  mu_cq_request()                                          */
/*                                                                       */
/*  Description: This function is used to request the ownership of the   */
/*               MU circular queues mechanism.                           */
/*                                                                       */
/*  Parameters:  q_size - MUCR_QUEUE_4K, MUCR_QUEUE_8K, MUCR_QUEUE_16K,  */
/*                        MUCR_QUEUE_32K, MUCR_QUEUE_64K                 */
/*                                                                       */
/*  Returns:     iop310_mu_t * - success                                 */
/*               NULL - permission denied                                */
/*                                                                       */
/*  Notes/Assumptions: The request shall own entire cq if successful.    */
/*                                                                       */
/*  History:    Dave Jiang 08/17/01  Initial Creation                    */
/*=======================================================================*/
int mu_cq_request(u32 * mu_context, u32 q_size)
{
	iop310_mu_t *mu = &mu_dev;
	mu_cq_t *cq = &mu->cq_desc;
	u32 entry_size = 0;
	u32 queue_cmd = 0;
	
	/* check ownership */
	if(atomic_read(&cq->in_use))
	{
		DPRINTK("Already owned\n");
		return -EBUSY;
	}
	
	/* set ownership flag */
	atomic_inc(&cq->in_use);

	/* set top of inbound free queue */	
	cq->inb_free_q_top = virt_to_phys(mu_mem);
	
	/* set queue size */
	cq->q_size = q_size;

	/* set rest of queue pointers */
	switch (q_size)
	{
	case MUCR_QUEUE_4K:
		entry_size = QUEUE_16K;
		queue_cmd = MUCR_QUEUE_4K;
		cq->num_entries = ENTRY_4K;
		break;
	case MUCR_QUEUE_8K:
		entry_size = QUEUE_32K;
		queue_cmd = MUCR_QUEUE_8K;
		cq->num_entries = ENTRY_8K;
		break;
	case MUCR_QUEUE_16K:
		entry_size = QUEUE_64K;
		queue_cmd = MUCR_QUEUE_16K;
		cq->num_entries = ENTRY_16K;
		break;
	case MUCR_QUEUE_32K:
		entry_size = QUEUE_128K;
		queue_cmd = MUCR_QUEUE_32K;
		cq->num_entries = ENTRY_32K;
		break;
	case MUCR_QUEUE_64K:
		entry_size = QUEUE_256K;
		queue_cmd = MUCR_QUEUE_64K;
		cq->num_entries = ENTRY_64K;
		break;
	default:
		/* wrong queue size from parameter */
		return -EINVAL;
	}
	
	cq->inb_post_q_top = entry_size;
	cq->outb_post_q_top = entry_size * 2;
	cq->outb_free_q_top = entry_size * 3;
	cq->inb_free_q_end = cq->inb_post_q_top - 4;
	cq->inb_post_q_end = cq->outb_post_q_top - 4;
	cq->outb_post_q_end = cq->outb_free_q_top;
	cq->outb_free_q_end = (entry_size * 4) - 4;
	*(IOP310_MUMUCR) = queue_cmd;

	cpu_dcache_clean_range((u32)mu_mem, ((u32)mu_mem + entry_size));

	/* Set head pointers and tail pointers */
	/* equal since they are empty */
	*(IOP310_MUIFHPR) = cq->inb_free_q_top;
	*(IOP310_MUIFTPR) = cq->inb_free_q_top;
	*(IOP310_MUIPHPR) = cq->inb_post_q_top;
	*(IOP310_MUIPTPR) = cq->inb_post_q_top;
	*(IOP310_MUOPHPR) = cq->outb_post_q_top;
	*(IOP310_MUOPTPR) = cq->outb_post_q_top;
	*(IOP310_MUOFHPR) = cq->outb_free_q_top;
	*(IOP310_MUOFTPR) = cq->outb_free_q_top;

	*mu_context = (u32) mu;
	return 0;
}								/* End of mu_cq_request() */

/*=======================================================================*/
/*  Procedure:  mu_cq_inbound_init()                                     */
/*                                                                       */
/*  Description: This function sets up the inbound queues. The caller of */
/*               the function will pass down a linked list of MFA addrs. */
/*               The function will put all the mfa in the free ib queue. */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor                    */
/*               list - pointer to list of message frame address         */
/*               size - number of entries in list                        */
/*               func - pointer to callback function                     */
/*                                                                       */
/*  Returns:     0 - success                                             */
/*               -EINVAL - invalid parameter                             */
/*                                                                       */
/*  Notes/Assumptions: Only to be called by owner of CQ.                 */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
int mu_cq_inbound_init(u32 mu_context, mfa_list_t * list, u32 size,
				   mu_cq_cb_t func)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	mu_cq_t *cq = &mu->cq_desc;
	mfa_list_t *tmp_list = list;
	u32 mem_offset = 0;
	u32 *mem = NULL;
	u32 limit;
	
	/* check parameters */
	if(!list || !func)
	{
		return -EINVAL;
	}

	/* if list size larger than queue size */
	if(size > cq->num_entries)
	{
		DPRINTK("More MFA entries than inbound free queue can hold!\n");
		return -EINVAL;
	}

	/* Get Address Limit */
	limit = *(IOP310_PIALR);

	/* put all MFA in list on free queue */
	while(tmp_list)
	{
		mem_offset = *IOP310_MUIFHPR & 0xfffff;
		mem = (u32 *)((u32)mu_mem + mem_offset);

		/* The MFA value is the offset of the Primary Inbound ATU Base Address */
		*mem = (u32)virt_to_phys(tmp_list->mfa) & ~limit;

		/* move 32 bits to next pointer */
		*(IOP310_MUIFHPR) += sizeof(u32);

		/* if we are at top of queue wrap around */
		if(*(IOP310_MUIFHPR) == cq->inb_free_q_end)
		{
			DPRINTK("wrap around\n");
			*(IOP310_MUIFHPR) = cq->inb_free_q_top;
		}

		/* if the head and tail meet, we have no more space */
		if(*(IOP310_MUIFHPR) == *(IOP310_MUIFTPR))
		{
			break;
		}

		tmp_list = tmp_list->next_list;
	}							/* end of while */

	/* set resource init flag */
	atomic_inc(&cq->init_ref);
	/* set callback */
	cq->callback = func;

	return 0;
}								/* End of mu_cq_inbound_init() */

/*=======================================================================*/
/*  Procedure:  mu_cq_enable()                                           */
/*                                                                       */
/*  Description: This function enables the circular queues and unmask    */
/*               the INTs.                                               */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor.                   */
/*                                                                       */
/*  Returns:     -EPERM - permission denied                              */
/*               0 - success                                            */
/*                                                                       */
/*  Notes/Assumptions: Only called after iq80310MuCqIbInit().            */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_cq_enable(u32 mu_context)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	mu_cq_t *cq = &mu->cq_desc;

	/* if no resource init fail */
	if(!atomic_read(&cq->init_ref) || !atomic_read(&cq->in_use))
	{
		DPRINTK("Resources not initialized\n");
		return -EPERM;
	}

	/* enable circular queues */
	*(IOP310_MUMUCR) |= MUCR_QUEUE_ENABLE;
	/* unmask inbound ints */
	*(IOP310_MUIIMR) &= ~(MUIIMR_IBPQ_MASK | MUIIMR_IBFQF_MASK);
	/* unmask outbound ints */
	/* the host should unmask this when it sends down */
	/* message frames */
	/* *(IOP310_MUOIMR) &= ~MUOIMR_OBPQ_MASK; */

	/* enable PATU memory access response */	
	*(IOP310_PATUCMD) |= 0x2;	

	return 0;
}

/*=======================================================================*/
/*  Procedure:  mu_cq_get_frame()                                        */
/*                                                                       */
/*  Description: This function will attempt to retrieve a free message   */
/*               frame from the outbound free queue.                     */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor                    */
/*                                                                       */
/*  Returns:     free frame address                                      */
/*               0xFFFFFFFF - no frame available                         */
/*                                                                       */
/*  Notes/Assumptions: N/A                                               */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
inline u32 mu_cq_get_frame(u32 mu_context)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	mu_cq_t *cq = &mu->cq_desc;
	u32 mem_offset = 0;
	u32 *mfa_ptr = NULL;

	spin_lock_irq(&cq->ob_free_lock);
	if(*(IOP310_MUOFTPR) == *(IOP310_MUOFHPR))
	{
		DPRINTK("No outbound frames!\n");
		spin_unlock_irq(&&cq->ob_free_lock);
		return FULL_MASK;
	}
	/* get address from outbound free tail pointer register */
	mem_offset = *(IOP310_MUOFTPR) & 0xfffff;
	/* wrap around if need to */
	*(IOP310_MUOFTPR) += sizeof(u32);
	if(*(IOP310_MUOFTPR) == cq->outb_free_q_end)
	{
		*(IOP310_MUOFTPR) = cq->outb_free_q_top;
	}
	spin_unlock_irq(&cq->ob_free_lock);
	
	mfa_ptr = (u32 *)((u8 *)mu_mem + mem_offset);
	cpu_dcache_invalidate_range((u32)mfa_ptr, (u32)mfa_ptr+4);

	return *mfa_ptr;
}

/*=======================================================================*/
/*  Procedure:  mu_cq_post_frame()                                       */
/*                                                                       */
/*  Description: This function attempt to post the message frame prov by */
/*               the user onto the outbound post qeuue.                  */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor                    */
/*               nMfa - message frame address                            */
/*                                                                       */
/*  Returns:     -EBUSY - outbound queue full                            */
/*               0 - success                                             */
/*                                                                       */
/*  Notes/Assumptions: Only the owner of cq shall call this function.    */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_cq_post_frame(u32 mu_context, u32 mfa)
{
	iop310_mu_t *mu = (iop310_mu_t *)mu_context;
	mu_cq_t *cq = &mu->cq_desc;
	u32 irq_assert = 0;
	u8 ptr_equal = 0;
	u32 mem_offset = 0;
	u32 *mem = NULL;
	
	spin_lock_irq(&cq->ob_post_lock);

	/* see if queue full */
	irq_assert = *(IOP310_MUOISR) & MUOISR_OBPQ_INT;
	ptr_equal = (*(IOP310_MUOPHPR) == *(IOP310_MUOPTPR)) ? 1 : 0;

	/* if queue full condition */
	if(irq_assert && ptr_equal)
	{
		spin_unlock_irq(&cq->ob_post_lock);
		return -EBUSY;
	}

	mem_offset = *IOP310_MUOPHPR & 0xfffff;
	mem = (u32 *)((u8 *)mu_mem + mem_offset);
	*mem = (u32)virt_to_bus(mfa);
	cpu_dcache_clean_range((u32)mem, (u32)mem+4);
	/* move 32 bits to next pointer */
	*(IOP310_MUOPHPR) += sizeof(u32);

	/* if we are at top of queue wrap around */
	if(*(IOP310_MUOPHPR) == cq->outb_post_q_end)
	{
		DPRINTK("wrap around\n");
		*(IOP310_MUOPHPR) = cq->outb_post_q_top;
	}

	spin_unlock_irq(&cq->ob_post_lock);
	return 0;
}

/*=======================================================================*/
/*  Procedure:  mu_cq_irq_task()                                         */
/*                                                                       */
/*  Description: This funciton is scheduled by the MU irq hdlr. It       */
/*               removes all inbound message frames from the ib post q.  */
/*               It then passes them to the app. Then it put the mfa     */
/*               back on the free queue                                  */
/*                                                                       */
/*  Parameters:  dev - pointer to MU device descriptor                   */
/*                                                                       */
/*  Returns:     NONE                                                    */
/*                                                                       */
/*  Notes/Assumptions:  The content of message frame must be perserved   */
/*                      by app when passed back. Once callback return    */
/*                      the message frame is considered free.            */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
static void mu_cq_irq_task(void *dev)
{
	iop310_mu_t *mu = (iop310_mu_t *)dev;
	mu_cq_t *cq = &mu->cq_desc;
	u32 *mfa_ptr = NULL;
	u32 mem_offset = 0;
	u32 *mem = NULL;
	
	DPRINTK("Entering cq irq task\n");

	while(*(IOP310_MUIPTPR) != *(IOP310_MUIPHPR))
	{
		spin_lock_irq(&cq->ib_post_lock);
		/* move tail pointer, wrap if neccesary */
		/* remove MFA from post queue */
		mem_offset = *IOP310_MUIPTPR & 0xfffff;
		*(IOP310_MUIPTPR) += sizeof(u32);
		if(*(IOP310_MUIPTPR) == mu->cq_desc.inb_post_q_end)
		{
			*(IOP310_MUIPTPR) = mu->cq_desc.inb_post_q_top;
		}
		spin_unlock_irq(&cq->ib_post_lock);

		mfa_ptr = (u32 *)((u8 *)mu_mem + mem_offset);
		cpu_dcache_invalidate_range((u32)mfa_ptr, (u32)mfa_ptr+4);
		
		if(cq->callback)
		{
			DPRINTK("Calling callback\n");
			/* pass frame to app */
			cq->callback(*mfa_ptr | *(IOP310_PIATVR));
		}
		
		/* put frame back to free queue */
		spin_lock_irq(&cq->ib_free_lock);
		mem_offset = *IOP310_MUIFHPR & 0xfffff;
		mem = (u32 *)((u32)mu_mem + mem_offset);
		*mem = (u32)virt_to_bus(*mfa_ptr);
		*(IOP310_MUIFHPR) += sizeof(u32);
		if(*(IOP310_MUIFHPR) == cq->inb_free_q_end)
		{
			*(IOP310_MUIFHPR) = cq->inb_free_q_top;
		}
		spin_unlock_irq(&cq->ib_free_lock);
	}

	*(IOP310_MUIIMR) &= ~MUIIMR_IBPQ_MASK; 
}

/*=======================================================================*/
/*  Procedure:  mu_cq_free()                                             */
/*                                                                       */
/*  Description: This function is called to release ownership of CQ.     */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor.                   */
/*                                                                       */
/*  Returns:     0 - success                                            */
/*               -EPERM - permission denied.                             */
/*                                                                       */
/*  Notes/Assumptions: Only the owner of CQ shall call this.             */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_cq_free(u32 mu_context)
{
	iop310_mu_t *mu = (iop310_mu_t *)mu_context;
	mu_cq_t *cq = &mu->cq_desc;
	
	/* check ownership */
	if(!atomic_read(&cq->in_use))
	{
		return -EPERM;
	}

	/* reset variables */
	atomic_dec(&cq->in_use);
	atomic_dec(&cq->init_ref);
	cq->callback = NULL;

	/* disable CQ and mask inbound/outbound CQ INTs */
	*(IOP310_MUMUCR) = NULL_MASK;
	*(IOP310_MUIIMR) |= MUIIMR_IBPQ_MASK | MUIIMR_IBFQF_MASK;
	*(IOP310_MUOIMR) |= MUOIMR_OBPQ_MASK;

	return 0;
}	


/*===========================MU Index Registers==========================*/

/*=======================================================================*/
/*  Procedure:  mu_ir_request()                                          */
/*                                                                       */
/*  Description: This function is used to request the ownership of the   */
/*               Index Registers mechanism.                              */
/*                                                                       */
/*  Parameters:  NONE                                                    */
/*                                                                       */
/*  Returns:     iop310_mu_t * - success                                 */
/*               NULL - failed                                           */
/*                                                                       */
/*  Notes/Assumptions: The requestor will own IR mechanism if successful */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_ir_request(u32 * mu_context)
{
	iop310_mu_t *mu = &mu_dev;
	mu_ir_t *ir = &mu->ir_desc;

	/* if onwed already */
	if(atomic_read(&ir->in_use))
	{
		return -EBUSY;
	}

	/* set ownership */
	atomic_inc(&ir->in_use);
	*mu_context = (u32)mu;
	return 0;
}

/*=======================================================================*/
/*  Procedure:  mu_ir_set_callback()                                     */
/*                                                                       */
/*  Description: This function sets the callback function used to recv   */
/*               incoming messages.                                      */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor                    */
/*               func - pointer to callback                              */
/*                                                                       */
/*  Returns:     0 - success                                            */
/*               -EINVAL - invalid parameter                             */
/*                                                                       */
/*  Notes/Assumptions: called after claiming ownership of IR             */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_ir_set_callback(u32 mu_context, mu_ir_cb_t func)
{
	iop310_mu_t *mu = (iop310_mu_t *) mu_context;
	mu_ir_t *ir = &mu->ir_desc;

	/* check parameter */
	if(!func)
	{
		return -EINVAL;
	}

	/* set parameter */
	ir->callback = func;

	/* unmask INTs */
	*(IOP310_MUIIMR) &= ~MUIIMR_IR_MASK;

	return 0;
}

/*=======================================================================*/
/*  Procedure:  mu_ir_irq_task()                                         */
/*                                                                       */
/*  Description: This function retrieves the offset from temp store in   */
/*               the IR descriptor and computes the message address.     */
/*               After passing the address it unmasks the INT bit.       */
/*                                                                       */
/*  Parameters:  dev - pointer to MU device descriptor                  */
/*                                                                       */
/*  Returns:     NONE                                                    */
/*                                                                       */
/*  Notes/Assumptions: NONE                                              */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
inline static void mu_ir_irq_task(void *dev)
{
	iop310_mu_t *mu = (iop310_mu_t *)dev;
	mu_ir_t *ir = &mu->ir_desc;
	u32 addr_val = 0;
	u32 *mem = NULL;

	/* calculate message address */
	/* start from Primary Inbound Address Translation Value Register */
	addr_val = ir->offset + *(IOP310_PIATVR);

	mem = phys_to_virt(addr_val);
	/* pass value to callback */
	if(ir->callback)
	{
		ir->callback(*mem);
	}

	/* Unmask the inbound INT */
	*(IOP310_MUIIMR) &= ~MUIIMR_IR_MASK;
}	

/*=======================================================================*/
/*  Procedure:  mu_ir_free()                                             */
/*                                                                       */
/*  Description: This function releases ownership of IR.                 */
/*                                                                       */
/*  Parameters:  mu - pointer to MU device descriptor                    */
/*                                                                       */
/*  Returns:     -EPERM - permission denied                              */
/*               0 - success                                            */
/*                                                                       */
/*  Notes/Assumptions: Only called by the owner of IR.                   */
/*                                                                       */
/*  History:    Dave Jiang 08/20/01  Initial Creation                    */
/*=======================================================================*/
inline int mu_ir_free(u32 mu_context)
{
	iop310_mu_t *mu = (iop310_mu_t *)mu_context;
	mu_ir_t *ir = &mu->ir_desc;

	/* check ownership */
	if(!atomic_read(&ir->in_use))
	{
		return -EPERM;
	}

	/* unset owner */
	atomic_dec(&ir->in_use);


	ir->callback = NULL;

	/* mask INT */
	*(IOP310_MUIIMR) |= MUIIMR_IR_MASK;

	return 0;
}


/*===========================MU Common Functions=========================*/

/*=======================================================================*/
/*  Procedure:  mu_irq_handler()                                         */
/*                                                                       */
/*  Description: This function is the interrupt handler for MU. It       */
/*               checks for MU int bit asserted and then schedules the   */
/*               proper bottom half task to take care of the INTs.       */
/*                                                                       */
/*  Parameters:  irq - IRQ number (not used)                             */
/*               dev_id - pointer to MU device descriptor                */
/*               regs - CPU registers (not used)                         */
/*                                                                       */
/*  Returns:     NONE                                                    */
/*                                                                       */
/*  Notes/Assumptions: This function is only called by XScale IRQ hdlr.  */
/*                                                                       */
/*  History:    Dave Jiang 08/16/01  Initial Creation                    */
/*=======================================================================*/
static void mu_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	iop310_mu_t *mu = (iop310_mu_t *)dev_id;
	u32 irq_status;
	u32 iisr;
	volatile u32 *reg_val_ptr;
	u8 msg_reg = 0;
	u32 mask = 0;
	mu_msg_desc_t *msg_desc = NULL;
	int thresh;

	/* get inbound irq status */
	iisr = *(IOP310_MUIISR);

	thresh = atomic_read(&mu->irq_thresh);

	do
	{
		/* if message registers INT */
		if(iisr & (MUIISR_IBMSG_0_INT | MUIISR_IBMSG_1_INT))
		{
			if(!list_empty(&mu->msg_desc.free_stack))
			{
				/* get message descriptor from free stack */
				spin_lock(&mu->msg_desc.free_lock);
				msg_desc = MSG_ENTRY(mu->msg_desc.free_stack.next);
				list_del(&msg_desc->link);
				spin_unlock(&mu->msg_desc.free_lock);
			}
			else
			{
				DPRINTK("Out of message descriptors!\n");
				return;
			}

			/* if inbound register 0 */
			if(iisr & MUIISR_IBMSG_0_INT)
			{
				msg_reg = INBOUND_REG_0;
				reg_val_ptr = IOP310_MUIMR0;
				mask = MUIISR_IBMSG_0_INT;
			}
			else				/* or inbound register 1 */
			{
				msg_reg = INBOUND_REG_1;
				reg_val_ptr = IOP310_MUIMR1;
				mask = MUIISR_IBMSG_1_INT;
			}

			/* set register number */
			msg_desc->reg_num = msg_reg;

			/* record register value */
			msg_desc->message = *(reg_val_ptr);

			/* put message descriptor on process queue */
			spin_lock(&mu->msg_desc.process_lock);
			list_add_tail(&msg_desc->link, &mu->msg_desc.process_q);
			spin_unlock(&mu->msg_desc.process_lock);

			/* read/clear INT */
			*(IOP310_MUIISR) |= ~mask;
			/* schedule bottom half tasklet */
			mu->msg_desc.msg_task.data = (void *)mu;
			/* task goes to the immediate task queue */
			queue_task(&mu->msg_desc.msg_task, &tq_immediate);
		}

		/* if doorbell INT */
		if(iisr & (MUIISR_IBDB_FIQ2_INT | MUIISR_IBDB_IRQ_INT))
		{
			spin_lock(&mu->db_desc.reg_lock);
			/* get register value */
			mu->db_desc.reg_val |= *(IOP310_MUIDR);
			/* read clear register, this clear INTs */
			*(IOP310_MUIDR) |= mu->db_desc.reg_val;
			spin_unlock(&mu->db_desc.reg_lock);

			/* schedule bottom half tasklet */
			mu->db_desc.db_task.data = (void *)mu;
			/* task goes to the immediate task queue */
			queue_task(&mu->db_desc.db_task, &tq_immediate);
		}

		/* if inbound post queue */
		if(iisr & MUIISR_IBPQ_INT)
		{
			/* ack INT bit , read/clear */
			*(IOP310_MUIISR) |= MUIISR_IBPQ_INT;
			*(IOP310_MUIIMR) |= MUIIMR_IBPQ_MASK; 
			/* schedule task */
			mu->cq_desc.cq_task.data = (void *)mu;
			queue_task(&mu->cq_desc.cq_task, &tq_immediate);
		}

		/* if inbound free queue full */
		if(iisr & MUIISR_IBFQF_INT)
		{
			/* ack INT bit */
			*(IOP310_MUIISR) &= ~(MUIISR_IBFQF_INT);
		}

		/* if index register */
		if(iisr & MUIISR_IR_INT)
		{
			/* mask interrupt bit */
			/* store offset */
			/* read/clear interrupt status */
			*(IOP310_MUIIMR) |= MUIIMR_IR_MASK;
			mu->ir_desc.offset = *(IOP310_MUIAR);
			/* read clear INT */
			*(IOP310_MUIISR) |= MUIISR_IR_INT;
			/* queue task */
			mu->ir_desc.ir_task.data = (void *)mu;
			queue_task(&mu->ir_desc.ir_task, &tq_immediate);
		}

		/* get int status */
		iisr = *(IOP310_MUIISR);

		/* get FIQ status */
		irq_status = *(IOP310_FIQ2ISR);
	}
	while(irq_status && --thresh);

	/* mark IMMEDIATE BH for execute */
	mark_bh(IMMEDIATE_BH);
}

/*=======================================================================*/
/*  Procedure:  mu_init()                                                */
/*                                                                       */
/*  Description:    This function initializes the MU to pre-own state.   */
/*                                                                       */
/*  Parameters:  NONE                                                    */
/*                                                                       */
/*  Returns:     0 - success                                             */
/*               -ENOMEM - no memory allocated for circular queues       */
/*               -EBUSY - cannot alloc irq                               */
/*                                                                       */
/*  Notes/Assumptions: This function is only called by kernel at init.   */
/*                                                                       */
/*  History:    Dave Jiang 08/16/01  Initial Creation                    */
/*=======================================================================*/
static int __init mu_init(void)
{
	iop310_mu_t *mu = &mu_dev;
	int err = 0;

	printk("Intel 80310 MU Copyright(c) 2001 Intel Corporation\n");

	/* check to see if we allocated QBAR at 0x100000 align, 0x100000 size */
	if(!mu_mem)
	{
		DPRINTK("No memory allocated for circular queue\n");
		return -ENOMEM;
	}

#ifdef REGMAP
	mu->regs = (mu_regs_t *)IOP310_MUIMR0;
#endif

	/* mask all INTs */
	*(IOP310_MUIIMR) = FULL_MASK;
	*(IOP310_MUOIMR) = FULL_MASK;

	/* set irq */
	mu->irq = IRQ_IOP310_MU;

	/* setup message registers */
	mu->msg_desc.callback_0 = NULL;
	mu->msg_desc.callback_1 = NULL;
	spin_lock_init(&mu->msg_desc.free_lock);
	spin_lock_init(&mu->msg_desc.process_lock);
	INIT_LIST_HEAD(&mu->msg_desc.free_stack);
	INIT_LIST_HEAD(&mu->msg_desc.process_q);
	atomic_set(&mu->msg_desc.res_init, 0);
	atomic_set(&mu->msg_desc.in_use, 0);
	mu->msg_desc.msg_task.sync = 0;
	mu->msg_desc.msg_task.routine = mu_msg_irq_task;


	/* setup doorbell registers */
	mu->db_desc.reg_val = 0;
	mu->db_desc.callback = NULL;
	atomic_set(&mu->db_desc.in_use, 0);
	spin_lock_init(&mu->db_desc.reg_lock);
	mu->db_desc.db_task.sync = 0;
	mu->db_desc.db_task.routine = mu_db_irq_task;

	/* setup circular queues */
	mu->cq_desc.q_size = 0;
	mu->cq_desc.callback = NULL;
	spin_lock_init(&mu->cq_desc.ib_free_lock);
	spin_lock_init(&mu->cq_desc.ib_post_lock);
	spin_lock_init(&mu->cq_desc.tObFreeQLock);
	spin_lock_init(&mu->cq_desc.ob_post_lock);
	atomic_set(&mu->cq_desc.init_ref, 0);
	atomic_set(&mu->cq_desc.in_use, 0);
	mu->cq_desc.cq_task.sync = 0;
	mu->cq_desc.cq_task.routine = mu_cq_irq_task;

	/* set QBAR */
	*(IOP310_MUQBAR) = (u32)virt_to_phys(mu_mem);

	/* setup index registers */
	mu->ir_desc.offset = 0;
	mu->ir_desc.callback = NULL;
	atomic_set(&mu->ir_desc.in_use, 0);
	mu->ir_desc.ir_task.sync = 0;
	mu->ir_desc.ir_task.routine = mu_ir_irq_task;


	/* request IRQ for the MU */
	err = request_irq(mu->irq, mu_irq_handler, SA_INTERRUPT,
					  "i80310 MU", (void *)mu);
	if(err < 0)
	{
		printk(KERN_ERR "%s: unable to request IRQ %d for MU: %d\n",
			   "i80310 MU", mu->irq, err);
		/* reset ref count */
		return -EBUSY;
	}

	/* set irq threshold */
	atomic_set(&mu->irq_thresh, DEFAULT_MU_IRQ_THRESH);

	return 0;
}

/*=======================================================================*/
/*  Procedure:  mu_set_irq_threshold()                                   */
/*                                                                       */
/*  Description:  This function readjust the threshold for the irq.      */
/*                                                                       */
/*  Parameters:  mu: pointer to mu device descriptor                     */
/*               value: value of new irq threshold                       */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:  default is set at 10                             */
/*                                                                       */
/*  History:    Dave Jiang 08/27/01  Initial Creation                    */
/*=======================================================================*/
void mu_set_irq_threshold(u32 mu_context, int thresh)
{
	iop310_mu_t *mu = (iop310_mu_t *)mu_context;

	atomic_set(&mu->irq_thresh, thresh);
}/* End of mu_set_irq_threshold() */


EXPORT_SYMBOL_NOVERS(mu_msg_request);
EXPORT_SYMBOL_NOVERS(mu_msg_set_callback);
EXPORT_SYMBOL_NOVERS(mu_msg_post);
EXPORT_SYMBOL_NOVERS(mu_msg_free);
EXPORT_SYMBOL_NOVERS(mu_db_request);
EXPORT_SYMBOL_NOVERS(mu_db_set_callback);
EXPORT_SYMBOL_NOVERS(mu_db_ring);
EXPORT_SYMBOL_NOVERS(mu_db_free);
EXPORT_SYMBOL_NOVERS(mu_cq_request);
EXPORT_SYMBOL_NOVERS(mu_cq_inbound_init);
EXPORT_SYMBOL_NOVERS(mu_cq_enable);
EXPORT_SYMBOL_NOVERS(mu_cq_get_frame);
EXPORT_SYMBOL_NOVERS(mu_cq_post_frame);
EXPORT_SYMBOL_NOVERS(mu_cq_free);
EXPORT_SYMBOL_NOVERS(mu_ir_request);
EXPORT_SYMBOL_NOVERS(mu_ir_set_callback);
EXPORT_SYMBOL_NOVERS(mu_ir_free);
EXPORT_SYMBOL_NOVERS(mu_set_irq_threshold);

module_init(mu_init);

