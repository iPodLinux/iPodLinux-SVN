/**************************************************************************
 * arch/arm/mach-iop310/aau.c
 *
 * Support functions for the Intel 80310 AAU.
 * (see also Documentation/arm/XScale/IOP310/aau.txt)
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Todos:  Thorough Error handling
 *          Do zero-size AAU transfer/channel at init
 *              so all we have to do is chainining  
 *
 *
 *  History:    (07/18/2001, DJ) Initial Creation
 *              (08/22/2001, DJ) Changed spinlock calls to no save flags
 *              (08/27/2001, DJ) Added irq threshold handling
 *		(09/11/2001, DJ) Changed AAU to list data structure,
 *			modified the user interface with embedded descriptors.
 *
 *************************************************************************/

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <asm/system.h>
#include <asm/types.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/iq80310.h>
#include <asm/arch/irqs.h>
#include <asm/memory.h>

#include <asm/arch/aau.h>

#include "aau.h"

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#undef DEBUG
#ifdef DEBUG
#define DPRINTK(s, args...) printk("80310AAU: " s, ## args)
#else
#define DPRINTK(s, args...)
#endif

/* globals */
static iop310_aau_t aau_dev;	/* AAU device */
static struct list_head free_stack;	/* free AAU desc stack */
static spinlock_t free_lock;	/* free AAU stack lock */

/* static prototypes */
static int __init aau_init(void);
static int aau_start(iop310_aau_t *, sw_aau_t *);
static int aau_flush_all(u32);
static void aau_process(iop310_aau_t *);
static void aau_task(void *);
static void aau_irq_handler(int, void *, struct pt_regs *);

/*=======================================================================*/
/*  Procedure:  aau_start()                                              */
/*                                                                       */
/*  Description:    This function starts the AAU. If the AAU             */
/*                  has already started then chain resume will be done   */
/*                                                                       */
/*  Parameters:  aau: AAU device                                         */
/*               aau_chain: AAU data chain to pass to the AAU            */
/*                                                                       */
/*  Returns:    int -- success: OK                                       */
/*                     failure: -EBUSY                                   */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 07/18/01  Initial Creation                    */
/*=======================================================================*/
static int aau_start(iop310_aau_t * aau, sw_aau_t * aau_chain)
{
	u32 status;

	/* get accelerator status */
	status = *(IOP310_AAUASR);

	/* check accelerator status error */
	if(status & AAU_ASR_ERROR_MASK)
	{
		DPRINTK("start: Accelerator Error %x\n", status);
		/* should clean the accelerator up then, or let int handle it? */
		return -EBUSY;
	}

	/* if first time */
	if(!(status & AAU_ASR_ACTIVE))
	{
		/* set the next descriptor address register */

		*(IOP310_AAUANDAR) = aau_chain->aau_phys;

		DPRINTK("Enabling accelerator now\n");
		/* enable the accelerator */
		*(IOP310_AAUACR) |= AAU_ACR_ENABLE;
	}
	else
	{
		DPRINTK("Resuming chain\n");
		/* if active, chain up to last AAU chain */

		aau->last_aau->aau_desc.NDA = aau_chain->aau_phys;

		/* flush cache since we changed the field */
		/* 32bit word long */
		cpu_dcache_clean_range((u32)&aau->last_aau->aau_desc.NDA,
			   (u32)(&aau->last_aau->aau_desc.NDA));

		/* resume the chain */
		*(IOP310_AAUACR) |= AAU_ACR_CHAIN_RESUME;
	}

	/* set the last accelerator descriptor to last descriptor in chain */
	aau->last_aau = aau_chain->tail;

	return 0;
}


/*=======================================================================*/
/*  Procedure:  aau_request()                                            */
/*                                                                       */
/*  Description:    This function requests the AAU                       */
/*                                                                       */
/*  Parameters: aau_context: aau context				 */
/*			     device_id -- unique device name             */
/*                                                                       */
/*  Returns:     0 - ok                                                  */
/*               NULL -- failed                                          */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 07/18/01  Initial Creation                    */
/*=======================================================================*/
int aau_request(u32 * aau_context, const char *device_id)
{
	iop310_aau_t *aau = &aau_dev;

	DPRINTK("Entering AAU request\n");
	/* increment reference count */
	atomic_inc(&aau->ref_count);

	/* get interrupt if ref count is less than or equal to 1 */
	if(atomic_read(&aau->ref_count) <= 1)
	{
		/* set device ID */
		aau->dev_id = device_id;
	}

	DPRINTK("Assigning AAU\n");
	*aau_context = (u32) aau;

	return 0;
}

/*=======================================================================*/
/*  Procedure:  aau_suspend()                                            */
/*                                                                       */
/*  Description:    This function suspends the AAU at the earliest       */
/*                  instant it is capable of.                            */
/*                                                                       */
/*  Parameters:  aau: AAU device context                                */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 07/18/01  Initial Creation                    */
/*=======================================================================*/
void aau_suspend(u32 aau_context)
{
	iop310_aau_t *aau = (iop310_aau_t *) aau_context;
	*(IOP310_AAUACR) &= ~AAU_ACR_ENABLE;
}

/*=======================================================================*/
/*  Procedure:  aau_resume()                                             */
/*                                                                       */
/*  Description:    This function resumes the AAU operations             */
/*                                                                       */
/*  Parameters:  aau: AAU device context                                */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 07/18/01  Initial Creation                    */
/*=======================================================================*/
void aau_resume(u32 aau_context)
{
	iop310_aau_t *aau = (iop310_aau_t *) aau_context;
	u32 status;

	status = *(IOP310_AAUASR);

	/* if it's already active */
	if(status & AAU_ASR_ACTIVE)
	{
		DPRINTK("Accelerator already active\n");
		return;
	}
	else if(status & AAU_ASR_ERROR_MASK)
	{
		printk("80310 AAU in error state! Cannot resume\n");
		return;
	}
	else
	{
		*(IOP310_AAUACR) |= AAU_ACR_ENABLE;
	}
}

/*=======================================================================*/
/*  Procedure:  aau_queue_buffer()                                       */
/*                                                                       */
/*  Description:    This function creates an AAU buffer chain from the   */
/*                  user supplied SGL chain. It also puts the AAU chain  */
/*                  onto the processing queue. This then starts the AAU  */
/*                                                                       */
/*  Parameters:  aau: AAU device context                                */
/*               listhead: User SGL                                      */
/*                                                                       */
/*  Returns:     int: success -- OK                                      */
/*                    failed: -ENOMEM                                    */
/*                                                                       */
/*  Notes/Assumptions:  User SGL must point to kernel memory, not user   */
/*                                                                       */
/*  History:    Dave Jiang 07/18/01  Initial Creation                    */
/*              Dave Jiang 07/20/01  Removed some junk code not suppose  */
/*                  to be there that will cause infinite loop            */
/*=======================================================================*/
int aau_queue_buffer(u32 aau_context, aau_head_t * listhead)
{
	sw_aau_t *sw_desc = (sw_aau_t *) listhead->list;
	sw_aau_t *prev_desc = NULL;
	sw_aau_t *head = NULL;
	aau_head_t *sgl_head = listhead;
	int err = 0;
	int i;
	iop310_aau_t *aau = (iop310_aau_t *) aau_context;
	DECLARE_WAIT_QUEUE_HEAD(wait_q);

	DPRINTK("Entering aau_queue_buffer()\n");

	/* scan through entire user SGL */
	while(sw_desc)
	{
		sw_desc->sgl_head = (u32) listhead;

		/* we clean the cache for previous descriptor in chain */
		if(prev_desc)
		{
			prev_desc->aau_desc.NDA = sw_desc->aau_phys;
			cpu_dcache_clean_range((u32)&prev_desc->aau_desc,
			   (u32)&prev_desc->aau_desc + AAU_DESC_SIZE);
		}
		else
		{
			/* no previous descriptor, so we set this to be head */
			head = sw_desc;
		}

		sw_desc->head = head;
		/* set previous to current */
		prev_desc = sw_desc;

		/* put descriptor on process */
		spin_lock_irq(&aau->process_lock);
		list_add_tail(&sw_desc->link, &aau->process_q);
		spin_unlock_irq(&aau->process_lock);

		sw_desc = (sw_aau_t *)sw_desc->next;
	}
	DPRINTK("Done converting SGL to AAU Chain List\n");

	/* if our tail exists */
	if(prev_desc)
	{
		/* set the head pointer on tail */
		prev_desc->head = head;
		/* set the header pointer's tail to tail */
		head->tail = prev_desc;
		prev_desc->tail = prev_desc;

		/* clean cache for tail */
		cpu_dcache_clean_range((u32)&prev_desc->aau_desc, 
			(u32)&prev_desc->aau_desc + AAU_DESC_SIZE);

		DPRINTK("Starting AAU accelerator\n");
		/* start the AAU */
		DPRINTK("Starting at chain: 0x%x\n", (u32)head);
		if((err = aau_start(aau, head)) >= 0)
		{
			DPRINTK("ASR: %#x\n", *IOP310_AAUASR);
			if(!sgl_head->callback)
			{
				wait_event_interruptible(aau->wait_q, 
					(sgl_head->status & AAU_COMPLETE));
			}
			return 0;
		}
		else
		{
			DPRINTK("AAU start failed!\n");
			return err;
		}
	}

	return -EINVAL;
}

/*=======================================================================*/
/*  Procedure:  aau_flush_all()                                          */
/*                                                                       */
/*  Description:    This function flushes the entire process queue for   */
/*                  the AAU. It also clears the AAU.                     */
/*                                                                       */
/*  Parameters:  aau: AAU device context                                 */
/*                                                                       */
/*  Returns:     int: success -- OK                                      */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 07/19/01  Initial Creation                    */
/*=======================================================================*/
static int aau_flush_all(u32 aau_context)
{
	iop310_aau_t *aau = (iop310_aau_t *) aau_context;
	int flags;
	sw_aau_t *sw_desc;

	DPRINTK("Flushall is being called\n");

	/* clear ACR */
	/* read clear ASR */
	*(IOP310_AAUACR) = AAU_ACR_CLEAR;
	*(IOP310_AAUASR) |= AAU_ASR_CLEAR;

	/* clean up processing Q */
	while(!list_empty(&aau->hold_q))
	{
		spin_lock_irqsave(&aau->process_lock, flags);
		sw_desc = SW_ENTRY(aau->process_q.next);
		list_del(aau->process_q.next);
		spin_unlock_irqrestore(&aau->process_lock, flags);

		/* set status to be incomplete */
		sw_desc->status |= AAU_INCOMPLETE;
		/* put descriptor on holding queue */
		spin_lock_irqsave(&aau->hold_lock, flags);
		list_add_tail(&sw_desc->link, &aau->hold_q);
		spin_unlock_irqrestore(&aau->hold_lock, flags);
	}

	return 0;
}

/*=======================================================================*/
/*  Procedure:  aau_free()                                               */
/*                                                                       */
/*  Description:    This function frees the AAU from usage.              */
/*                                                                       */
/*  Parameters:  aau -- AAU device context                               */
/*                                                                       */
/*  Returns:     int: success -- OK                                      */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 07/19/01  Initial Creation                    */
/*=======================================================================*/
void aau_free(u32 aau_context)
{
	iop310_aau_t *aau = (iop310_aau_t *) aau_context;

	atomic_dec(&aau->ref_count);

	/* if ref count is 1 or less, you are the last owner */
	if(atomic_read(&aau->ref_count) <= 1)
	{
		/* flush AAU channel */
		aau_flush_all(aau_context);
		/* flush holding queue */
		aau_task(aau);

		if(aau->last_aau)
		{
			aau->last_aau = NULL;
		}

		DPRINTK("Freeing IRQ %d\n", aau->irq);
		/* free the IRQ */
		free_irq(aau->irq, (void *)aau);
	}

	DPRINTK("freed\n");
}

/*=======================================================================*/
/*  Procedure:  aau_irq_handler()                                        */
/*                                                                       */
/*  Description:    This function is the int handler for the AAU         */
/*                  driver. It removes the done AAU descriptors from the */
/*                  process queue and put them on the holding Q. it will */
/*                  continue to process until process queue empty or the */
/*                  current AAU desc on the accelerator is the one we    */
/*                  are inspecting                                       */
/*                                                                       */
/*  Parameters:  irq:  IRQ activated                                     */
/*               dev_id: device                                          */
/*               regs:  registers                                        */
/*                                                                       */
/*  Returns:    NONE                                                     */
/*                                                                       */
/*  Notes/Assumptions: Interupt is masked                                */
/*                                                                       */
/*  History:    Dave Jiang 07/19/01  Initial Creation                    */
/*              Dave Jiang 07/20/01  Check FIQ1 instead of ASR for INTs  */
/*=======================================================================*/
static void aau_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	iop310_aau_t *aau = (iop310_aau_t *) dev_id;
	u32 int_status = 0;
	u32 status = 0;
	u32 thresh;

	/* get FIQ1 status */
	int_status = *(IOP310_FIQ1ISR);

	DPRINTK("IRQ: irq=%d status=%#x\n", irq, status);

	/* this is not our interrupt */
	if(!(int_status & AAU_INT_MASK))
	{
		return;
	}

	/* get accelerator status */
	status = *(IOP310_AAUASR);

	/* get threshold */
	thresh = atomic_read(&aau->irq_thresh);

	/* process while we have INT */
	while((int_status & AAU_INT_MASK) && thresh--)
	{
		/* clear ASR */
		*(IOP310_AAUASR) &= AAU_ASR_MASK;

		/* flush all with err condition */
		if(status & AAU_ASR_DONE_MASK)
		{
			aau_process(aau);
		}

		/* read accelerator status */
		status = *(IOP310_AAUASR);

		/* get interrupt status */
		int_status = *(IOP310_FIQ1ISR);
	}

	/* schedule bottom half */
	aau->aau_task.data = (void *)aau;
	/* task goes to the immediate task queue */
	queue_task(&aau->aau_task, &tq_immediate);
	/* mark IMMEDIATE BH for execute */
	mark_bh(IMMEDIATE_BH);
}


/*=======================================================================*/
/*  Procedure:  aau_process()                                            */
/*                                                                       */
/*  Description:    This function processes moves all the AAU desc in    */
/*                  the processing queue that are considered done to the */
/*                  holding queue. It is called by the int when the      */
/*                  done INTs are asserted. It will continue until       */
/*                  either the process Q is empty or current AAU desc    */
/*                  equals to the one in the ADAR                        */
/*                                                                       */
/*  Parameters:  aau: AAU device as parameter                            */
/*                                                                       */
/*  Returns:    NONE                                                     */
/*                                                                       */
/*  Notes/Assumptions: Interrupt is masked                               */
/*                                                                       */
/*  History:    Dave Jiang 07/19/01  Initial Creation                    */
/*=======================================================================*/
static void aau_process(iop310_aau_t * aau)
{
	sw_aau_t *sw_desc;
	u8 same_addr = 0;

	DPRINTK("Entering aau_process()\n");

	while(!same_addr && !list_empty(&aau->process_q))
	{
		spin_lock(&aau->process_lock);
		sw_desc = SW_ENTRY(aau->process_q.next);
		list_del(aau->process_q.next);
		spin_unlock(&aau->process_lock);

		if(sw_desc->head->tail->status & AAU_NEW_HEAD)
		{
			DPRINTK("Found new head\n");
			sw_desc->tail->head = sw_desc;
			sw_desc->head = sw_desc;
			sw_desc->tail->status &= ~AAU_NEW_HEAD;
		}

		sw_desc->status |= AAU_DESC_DONE;

		/* if we see end of chain, we set head status to DONE */
		if(sw_desc->aau_desc.DC & AAU_DCR_IE)
		{
			if(sw_desc->status & AAU_END_CHAIN)
			{
				sw_desc->tail->status |= AAU_COMPLETE;
			}
			else
			{
				sw_desc->head->tail = sw_desc;
				sw_desc->tail = sw_desc;
				sw_desc->tail->status |= AAU_NEW_HEAD;
			}
			sw_desc->tail->status |= AAU_NOTIFY;
		}

		/* if descriptor equal same being proccessed, put it back */
		if(((u32) sw_desc == *(IOP310_AAUADAR)
		   ) && ( *(IOP310_AAUASR) & AAU_ASR_ACTIVE))
		{
			spin_lock(&aau->process_lock);
			list_add(&sw_desc->link, &aau->process_q);
			spin_unlock(&aau->process_lock);
			same_addr = 1;
		}
		else
		{
			spin_lock(&aau->hold_lock);
			list_add_tail(&sw_desc->link, &aau->hold_q);
			spin_unlock(&aau->hold_lock);
		}
	}			
	DPRINTK("Exit aau_process()\n");
}

/*=======================================================================*/
/*  Procedure:  aau_task()                                               */
/*                                                                       */
/*  Description:    This func is the bottom half handler of the AAU INT  */
/*                  handler. It is queued as an imm task on the imm      */
/*                  task Q. It process all the complete AAU chain in the */
/*                  holding Q and wakes up the user and frees the        */
/*                  resource.                                            */
/*                                                                       */
/*  Parameters:  aau_dev: AAU device as parameter                        */
/*                                                                       */
/*  Returns:    NONE                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 07/19/01  Initial Creation                    */
/*=======================================================================*/
static void aau_task(void *aau_dev)
{
	iop310_aau_t *aau = (iop310_aau_t *) aau_dev;
	u8 end_chain = 0;
	sw_aau_t *sw_desc = NULL;
	aau_head_t *listhead = NULL;	/* user list */

	DPRINTK("Entering bottom half\n");

	if(!list_empty(&aau->hold_q))
	{
		sw_desc = SW_ENTRY(aau->hold_q.next);
		listhead = (aau_head_t *) sw_desc->sgl_head;
	}
	else
		return;

	/* process while AAU chain is complete */
	while(sw_desc && (sw_desc->tail->status & (AAU_NOTIFY | AAU_INCOMPLETE)))
	{
		/* clean up until end of AAU chain */
		while(!end_chain)
		{
			/* IE flag indicate end of chain */
			if(sw_desc->aau_desc.DC & AAU_DCR_IE)
			{
				end_chain = 1;
				listhead->status |= 
					sw_desc->tail->status & AAU_USER_MASK;

				sw_desc->status |= AAU_NOTIFY;

				if(sw_desc->status & AAU_END_CHAIN)
					listhead->status |= AAU_COMPLETE;
			}

			spin_lock_irq(&aau->hold_lock);
			/* remove from holding queue */
			list_del(&sw_desc->link);
			spin_unlock_irq(&aau->hold_lock);

			cpu_dcache_invalidate_range((u32)&sw_desc->aau_desc,
				(u32)&sw_desc->aau_desc + AAU_DESC_SIZE);

			if(!list_empty(&aau->hold_q))
			{
				sw_desc = SW_ENTRY(aau->hold_q.next);
				listhead = (aau_head_t *) sw_desc->sgl_head;
			}
			else
				sw_desc = NULL;
		}

		/* reset end of chain flag */
		end_chain = 0;

		/* wake up user function waiting for return */
		/* or use callback if exist */
		if(listhead->callback)
		{
			DPRINTK("Calling callback\n");
			listhead->callback((void *)listhead);
		}
		else if(listhead->status & AAU_COMPLETE)
			/* if(waitqueue_active(&aau->wait_q)) */
		{
			DPRINTK("Waking up waiting process\n");
			wake_up_interruptible(&aau->wait_q);
		}
	}							/* end while */
	DPRINTK("Exiting bottom task\n");
}

/*=======================================================================*/
/*  Procedure:  aau_init()                                               */
/*                                                                       */
/*  Description:    This function initializes the AAU.                   */
/*                                                                       */
/*  Parameters:  NONE                                                    */
/*                                                                       */
/*  Returns:     int: success -- OK                                      */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 07/18/01  Initial Creation                    */
/*=======================================================================*/
static int __init aau_init(void)
{
	int i;
	sw_aau_t *sw_desc;
	int err;
	void *desc = NULL;

	printk("Intel 80310 AAU Copyright(c) 2001 Intel Corporation\n");
	DPRINTK("Initializing...");

	/* set the IRQ */
	aau_dev.irq = IRQ_IOP310_AAU;

	err = request_irq(aau_dev.irq, aau_irq_handler, SA_INTERRUPT,
					  NULL, (void *)&aau_dev);
	if(err < 0)
	{
		printk(KERN_ERR "unable to request IRQ %d for AAU: %d\n",
			   aau_dev.irq, err);
		return err;
	}

	/* init free stack */
	INIT_LIST_HEAD(&free_stack);
	/* init free stack spinlock */
	spin_lock_init(&free_lock);


	/* pre-alloc AAU descriptors */
	for(i = 0; i < MAX_AAU_DESC; i++)
	{
		desc = kmalloc((sizeof(sw_aau_t) + 0x20), GFP_KERNEL);
		memset(desc, 0, sizeof(sw_aau_t));
		sw_desc = (sw_aau_t *) (((u32) desc & 0xffffffe0) + 0x20);
		sw_desc->aau_phys = virt_to_phys((void *)sw_desc);
		/* we keep track of original address before alignment adjust */
		/* so we can free it later */
		sw_desc->desc_addr = (u32) desc;

		spin_lock_irq(&free_lock);
		/* put the descriptors on the free stack */
		list_add_tail(&sw_desc->link, &free_stack);
		spin_unlock_irq(&free_lock);
	}

	/* set the register data structure to the mapped memory regs AAU */
	aau_dev.regs = (aau_regs_t *) IOP310_AAUACR;

	atomic_set(&aau_dev.ref_count, 0);

	/* init process Q */
	INIT_LIST_HEAD(&aau_dev.process_q);
	/* init holding Q */
	INIT_LIST_HEAD(&aau_dev.hold_q);
	/* init locks for Qs */
	spin_lock_init(&aau_dev.hold_lock);
	spin_lock_init(&aau_dev.process_lock);

	aau_dev.last_aau = NULL;

	/* initialize BH task */
	aau_dev.aau_task.sync = 0;
	aau_dev.aau_task.routine = (void *)aau_task;

	/* initialize wait Q */
	init_waitqueue_head(&aau_dev.wait_q);

	/* clear AAU channel control register */
	*(IOP310_AAUACR) = AAU_ACR_CLEAR;
	*(IOP310_AAUASR) = AAU_ASR_CLEAR;
	*(IOP310_AAUANDAR) = 0;

	/* set default irq threshold */
	atomic_set(&aau_dev.irq_thresh, DEFAULT_AAU_IRQ_THRESH);
	DPRINTK("Done!\n");

	return 0;
}	

/*=======================================================================*/
/*  Procedure:  aau_set_irq_threshold()                                  */
/*                                                                       */
/*  Description:  This function readjust the threshold for the irq.      */
/*                                                                       */
/*  Parameters:  aau: pointer to aau device descriptor                   */
/*               value: value of new irq threshold                       */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:  default is set at 10                             */
/*                                                                       */
/*  History:    Dave Jiang 08/27/01  Initial Creation                    */
/*=======================================================================*/
void aau_set_irq_threshold(u32 aau_context, int value)
{
	iop310_aau_t *aau = (iop310_aau_t *) aau_context;
	atomic_set(&aau->irq_thresh, value);
}								/* End of aau_set_irq_threshold() */


/*=======================================================================*/
/*  Procedure:  aau_get_buffer()                                         */
/*                                                                       */
/*  Description:    This function acquires an SGL element for the user   */
/*                  and returns that. It will retry multiple times if no */
/*                  descriptor is available.                             */
/*                                                                       */
/*  Parameters:  aau_context: AAU context                                */
/*               num_buf:  number of descriptors                         */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 9/11/01  Initial Creation                     */
/*		 Dave Jiang 10/04/01	Fixed list linking problem	  */
/*=======================================================================*/
aau_sgl_t *aau_get_buffer(u32 aau_context, int num_buf)
{
	sw_aau_t *sw_desc = NULL;
	sw_aau_t *sw_head = NULL;
	sw_aau_t *sw_prev = NULL;

	int retry = 10;
	int i;
	DECLARE_WAIT_QUEUE_HEAD(wait_q);

	if((num_buf > MAX_AAU_DESC) || (num_buf <= 0))
	{
		return NULL;
	}

	DPRINTK("Getting %d descriptors\n", num_buf);
	for(i = num_buf; i > 0; i--)
	{
		spin_lock_irq(&free_lock);
		if(!list_empty(&free_stack))
		{
			sw_desc = SW_ENTRY(free_stack.next);
			list_del(free_stack.next);
			spin_unlock_irq(&free_lock);
		}
		else
		{
			while(retry-- && !sw_desc)
			{
				spin_unlock_irq(&free_lock);
				interruptible_sleep_on_timeout(&wait_q, SLEEP_TIME);
				spin_lock_irq(&free_lock);
				if(!list_empty(&free_stack))
				{
					sw_desc = SW_ENTRY(free_stack.next);
					list_del(free_stack.next);
				}
				spin_unlock_irq(&free_lock);
			}

			sw_desc = sw_head;
			spin_lock_irq(&free_lock);
			while(sw_desc)
			{
				sw_desc->status = 0;
				sw_desc->head = NULL;
				sw_desc->tail = NULL;
				list_add(&sw_desc->link, &free_stack);
				sw_desc = (sw_aau_t *) sw_desc->next;
			}					/* end while */
			spin_unlock_irq(&dma_free_lock);
			return NULL;
		}						/* end else */

		if(sw_prev)
		{
			sw_prev->next = (aau_sgl_t *) sw_desc;
			sw_prev->aau_desc.NDA = sw_desc->aau_phys;
		}
		else
		{
			sw_head = sw_desc;
		}

		sw_prev = sw_desc;
	}							/* end for */

	sw_desc->aau_desc.NDA = 0;
	sw_desc->next = NULL;
	sw_desc->status = 0;
	return (aau_sgl_t *) sw_head;
}	


/*=======================================================================*/
/*  Procedure:  aau_return_buffer()                                      */
/*                                                                       */
/*  Description:    This function takes a list of SGL and return it to   */
/*                  the free stack.                                      */
/*                                                                       */
/*  Parameters:  aau_context: AAU context                                */
/*               list: SGL list to return to free stack                  */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 9/11/01  Initial Creation                     */
/*=======================================================================*/
void aau_return_buffer(u32 aau_context, aau_sgl_t * list)
{
	sw_aau_t *sw_desc = (sw_aau_t *) list;

	spin_lock_irq(&free_lock);
	while(sw_desc)
		if(sw_desc)
		{
			list_add(&sw_desc->link, &free_stack);
			sw_desc = (sw_aau_t *) sw_desc->next;
		}
	spin_unlock_irq(&free_lock);
}

/* no cache coherency done on data. user need to take care of that */
int aau_memcpy(void *dest, void *src, u32 size)
{
	iop310_aau_t *aau = &aau_dev;
	aau_head_t head;
	aau_sgl_t *list;
	int err;

	head.total = size;
	head.status = 0;
	head.callback = NULL;

	list = aau_get_buffer((u32) aau, 1);
	if(list)
	{
		head.list = list;
	}
	else
	{
		return -ENOMEM;
	}

	while(list)
	{
		list->status = 0;
		list->src[0] = src;
		list->aau_desc.SAR[0] = (u32) virt_to_phys(src);
		list->dest = dest;
		list->aau_desc.DAR = (u32) virt_to_phys(dest);
		list->aau_desc.BC = size;
		list->aau_desc.DC = AAU_DCR_WRITE | AAU_DCR_BLKCTRL_1_DF;
		if(!list->next)
		{
			list->aau_desc.DC |= AAU_DCR_IE;
			list->status |= AAU_END_CHAIN;
			break;
		}
		list = list->next;
	}
	err = aau_queue_buffer((u32) aau, &head);
	aau_return_buffer((u32) aau, head.list);
	return err;
}	

EXPORT_SYMBOL_NOVERS(aau_request);
EXPORT_SYMBOL_NOVERS(aau_queue_buffer);
EXPORT_SYMBOL_NOVERS(aau_suspend);
EXPORT_SYMBOL_NOVERS(aau_resume);
EXPORT_SYMBOL_NOVERS(aau_free);
EXPORT_SYMBOL_NOVERS(aau_set_irq_threshold);
EXPORT_SYMBOL_NOVERS(aau_get_buffer);
EXPORT_SYMBOL_NOVERS(aau_return_buffer);
EXPORT_SYMBOL_NOVERS(aau_memcpy);

module_init(aau_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dave Jiang <dave.jiang@intel.com>");
MODULE_DESCRIPTION("Driver for IOP3xx Application Accelerator Unit");

