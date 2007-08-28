/**************************************************************************
 *
 * arch/arm/mach-iop310/dma.c
 *
 * Support functions for the Intel 80312 DMA channels.
 * (see also Documentation/arm/XScale/iop310/dma.txt)
 *
 * Design inspired by the SA1100 DMA code by Nicolas Pitre
 * Generic DMA ATU setup sample code provided by Larry Stewart
 *
 * Author: Dave Jiang (dave.jiang@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Todos:  Thorough Error handling (no FIQ support yet)
 *          Do zero-size DMA transfer/channel at init
 *          so all we have to do is chainining  
 *
 *
 *  History:    (06/26/2001, DJ) Initial Creation
 *              (07/06/2001, DJ) Use __raw_(read/write)l() instead of 
 *                 memory mapped register because they aren't working yet
 *              (07/12/2001, DJ) Hooked DMA to INTD. Need their own INTs.
 *                               (not anymore since 2.4.7-rmk3-iop310.1-dj1)
 *              (07/19/2001, DJ) Fixed some logic stuff after doing AAU. 
 *              (08/22/2001, DJ) Changed spinlock calls to no save flags
 *              (08/27/2001, DJ) Added irq threshold handling
 *		(09/07/2001, DJ) Added cached/non-cached support, changed
 *		   data structure to use list.h from Linux kernel, the API
 *		   sleep now instead of requiring user to. slight API
 *		   interface changes. Added mid-chain int notification
 *		   support. User can also select the appropriate PCI
 *		   commands.
 *		(09/10/2001, DJ) Made DMA descriptor embed in user SGL. 
 *		   Removed significant amount of data copying. User
 *		   procedures a bit more exposed to hardware operation in
 *		   trade off of performance.
 * 		(10/11/2001, DJ) Added cache invalidate before hardware touches
 * 			it to prevent data corruption.
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
#include <asm/types.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/memory.h>
#include <linux/list.h>
#include <asm/arch/dma.h>
#include <asm/mach/dma.h>

/*
 * pick up local definitions
 */
#include "dma.h"

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#undef DEBUG
#ifdef DEBUG
#define DPRINTK(s, args...) printk("80310DMA: " s, ## args)
#else
#define DPRINTK(s, args...)
#endif

/* globals */
static iop310_dma_t dma_chan[MAX_IOP310_DMA_CHANNEL];	/* DMA channels */
static struct list_head dma_free_stack;	/* free DMA descriptor stack */
static spinlock_t dma_free_lock;	/* free DMA stack lock */

/* static prototypes */
static inline int dma_start(iop310_dma_t *, sw_dma_t *);
static void dma_task(void *);
static void dma_irq_handler(int, void *, struct pt_regs *);
static void dma_process(iop310_dma_t *);

/*=======================================================================*/
/*  Procedure:  dma_start()                                              */
/*                                                                       */
/*  Description:    This function starts the DMA engine. If the DMA      */
/*                  has already started then chain resume will be done   */
/*                                                                       */
/*  Parameters:  dma: DMA channel device                                 */
/*               dma_chain: DMA data chain to pass to DMAC               */
/*                                                                       */
/*  Returns:    int -- success: 0                                       */
/*                     failure: -EBUSY                                   */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*=======================================================================*/
static int dma_start(iop310_dma_t * dma, sw_dma_t * dma_chain)
{
	u32 status;

	status = *(dma->reg_addr.CSR);

	/* check channel status error */
	if(status & DMA_CSR_ERR_MASK)
	{
		DPRINTK("start: Channel Error %x\n", status);
		/* should clean the channel up then, or let int handle it? */
		return -EBUSY;
	}

	/* if channel not active */
	if(!(status & DMA_CSR_CH_ACTIVE))
	{
		/* set the next descriptor address register */
		DPRINTK("Start at phys: %#x\n", dma_chain->dma_phys);
		*(dma->reg_addr.NDAR) = dma_chain->dma_phys;

		DPRINTK("Enabling channel now\n");

		*(dma->reg_addr.CCR) = DMA_CCR_CHANNEL_ENABLE;
	}
	else
	{
		DPRINTK("Resuming chain\n");
		/* if active, chain up to last DMA chain */

		dma->last_desc->dma_desc.NDAR = (u32) dma_chain->dma_phys;

		/* flush cache since we changed field */
		cpu_dcache_clean_range((u32)&dma->last_desc->dma_desc.NDAR,
			   (u32)(&dma->last_desc->dma_desc.NDAR + 1));

		/* resume the chain */
		*(dma->reg_addr.CCR) |= DMA_CCR_CHAIN_RESUME;
	}

	/* set the last channel descriptor to last descriptor in chain */
	dma->last_desc = dma_chain->tail;

	return 0;
}

/*=======================================================================*/
/*  Procedure:  dma_task()                                               */
/*                                                                       */
/*  Description:    This function is the bottom half hdlr of the DMA INT */
/*                  handler. It is queued as an imm task on the imm      */
/*                  task Q. It process all the complete DMA chain in the */
/*                  holding Q and wakes up the user and free the resource*/
/*                                                                       */
/*  Parameters:   data: DMA device as parameter                          */
/*                                                                       */
/*  Returns:    NONE                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*=======================================================================*/
static void dma_task(void *data)
{
	iop310_dma_t *dma = (iop310_dma_t *) data;
	u8 end_chain = 0;
	sw_dma_t *sw_desc = NULL;
	dma_head_t *listhead = NULL;

	DPRINTK("Entering dma_task()\n");
	if(!list_empty(&dma->hold_q))
	{
		sw_desc = SW_ENTRY(dma->hold_q.next);
		listhead = (dma_head_t *) sw_desc->sgl_head;
	}
	else
	{
		DPRINTK("List empty!\n");
		return;
	}

	DPRINTK("SW Desc: %#x\n", (u32) sw_desc);
	/* process while DMA chain is complete */
	while(sw_desc && 
		(sw_desc->tail->status & (DMA_NOTIFY | DMA_INCOMPLETE)))
	{
		/* clean up until end of DMA chain */
		while(!end_chain)
		{
			/* IE indicates end of chain or int per user request */
			/* DMA_COMPLETE denotes end of real chain */
			if(sw_desc->dma_desc.DC & DMA_DCR_IE)
			{
				end_chain = 1;
				/* set the user list status as done */
				listhead->status |= 
					sw_desc->tail->status & DMA_USER_MASK;
				sw_desc->status |= DMA_NOTIFY;

				if(sw_desc->status & DMA_END_CHAIN)
				{
					listhead->status |= DMA_COMPLETE;
				}
			}

			DPRINTK("Remove from holding Q\n");
			spin_lock_irq(&dma->hold_lock);
			/* remove from holding queue */
			list_del(&sw_desc->link);
			spin_unlock_irq(&dma->hold_lock);

			cpu_dcache_invalidate_range((u32)&sw_desc->dma_desc,
				(u32)&sw_desc->dma_desc + DMA_DESC_SIZE);


			DPRINTK("Cleaning up\n");
			sw_desc->head = NULL;
			sw_desc->tail = NULL;

			DPRINTK("Look at another descriptor\n");
			if(!list_empty(&dma->hold_q))
			{
				/* peek at next descriptor */
				sw_desc = SW_ENTRY(dma->hold_q.next);
				listhead = (dma_head_t *) sw_desc->sgl_head;
			}
			else
			{
				sw_desc = NULL;
			}
		}

		/* reset chain */
		end_chain = 0;

		DPRINTK("----List status: %#x\n", listhead->status);
		/* wake up user function waiting for return */
		if(listhead->callback)
		{
			listhead->callback(listhead);
		}
		else if(listhead->status & DMA_COMPLETE)
			/*if(waitqueue_active(&dma->wait_q)) */
		{
			DPRINTK("Waking up waiting process\n");
			wake_up_interruptible(&dma->wait_q);
		}
	}							/* end while */

	DPRINTK("Exiting DMA task\n");
}

/*=======================================================================*/
/*  Procedure:  dma_irq_handler()                                        */
/*                                                                       */
/*  Description:    This function is the interrupt handler for the DMA   */
/*                  driver. It removes the done DMA descriptors from the */
/*                  process Q and put them on the holding Q. it will     */
/*                  continue to process until process queue empty or the */
/*                  current DMA desc on the DMAC is the one we are       */
/*                  inspecting                                           */
/*                                                                       */
/*  Parameters:   irq: IRQ activated                                     */
/*                dev_id: device                                         */
/*                regs: registers                                        */
/*                                                                       */
/*  Returns:    NONE                                                     */
/*                                                                       */
/*  Notes/Assumptions: Interupt is masked. No error handling is done.    */
/*		       Errors should be done with the FIQ handler, which */
/*		       is not supported currently.			 */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*              Dave Jiang 07/20/01 Check INT instead of CSR for more INT*/
/*=======================================================================*/
static void dma_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	iop310_dma_t *dma = (iop310_dma_t *) dev_id;
	u32 irq_status = 0;
	u32 status = 0;
	u32 thresh;

	irq_status = *(IOP310_FIQ1ISR);

	if(!(irq_status & DMA_INT_MASK))
	{
		return;
	}
	DPRINTK("IRQ: irq=%d status=%#x\n", irq, irq_status);

	status = *(dma->reg_addr.CSR);

	thresh = atomic_read(&dma->irq_thresh);


	DPRINTK("CSR: %#x\n", status);
	DPRINTK("Thresh: %d\n", thresh);
	/* while we continue to get DMA INTs */
	while((irq_status & DMA_INT_MASK) && thresh--)
	{
		/* clear CSR */
		*(dma->reg_addr.CSR) |= DMA_CSR_DONE_MASK;

		dma_process(dma);

		status = *(dma->reg_addr.CSR);
		irq_status = *(IOP310_FIQ1ISR);
	}

	/* schedule bottom half */
	dma->dma_task.data = (void *)dma;
	/* task goes to the immediate task queue */
	queue_task(&dma->dma_task, &tq_immediate);
	/* mark IMMEDIATE BH for execute */
	mark_bh(IMMEDIATE_BH);
}

/*=======================================================================*/
/*  Procedure:  dma_process()                                            */
/*                                                                       */
/*  Description:    This function processes moves all the dma desc in    */
/*                  the processing queue that are considered done to the */
/*                  holding Q. It is called by the interrupt when the    */
/*                  done INTs are asserted. It will continue to process  */
/*                  until either the process queue is empty or current   */
/*                  DMA desc equals to the one in the DMAC               */
/*                                                                       */
/*  Parameters:   dma: DMA device as parameter                           */
/*                                                                       */
/*  Returns:    NONE                                                     */
/*                                                                       */
/*  Notes/Assumptions: Interrupt is masked                               */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*=======================================================================*/
static void dma_process(iop310_dma_t * dma)
{
	sw_dma_t *sw_desc;
	u8 same_addr = 0;

	DPRINTK("Entering dma_process()\n");

	while(!same_addr && !list_empty(&dma->process_q))
	{
		spin_lock(&dma->process_lock);
		/* peek the head of the queue */
		sw_desc = SW_ENTRY(dma->process_q.next);
		list_del(dma->process_q.next);
		spin_unlock(&dma->process_lock);

		if(sw_desc->head->tail->status & DMA_NEW_HEAD)
		{
			DPRINTK("Process new head\n");
			sw_desc->tail->head = sw_desc;
			sw_desc->head = sw_desc;
			sw_desc->tail->status &= ~DMA_NEW_HEAD;
		}

		sw_desc->status |= DMA_DESC_DONE;
		/* if we see end of chain, we set head status to DONE */
		if(sw_desc->dma_desc.DC & DMA_DCR_IE)
		{
			DPRINTK("INT flag found!\n");
			if(sw_desc->status & DMA_END_CHAIN)
			{
				DPRINTK("chain complete\n");
				sw_desc->tail->status |= DMA_COMPLETE;
			}
			else
			{
				DPRINTK("mid chain flag\n");
				sw_desc->head->tail = sw_desc;
				sw_desc->tail = sw_desc;
				sw_desc->tail->status |= DMA_NEW_HEAD;
			}
			sw_desc->tail->status |= DMA_NOTIFY;
		}

		DPRINTK("Check if current processing\n");
		/* if descriptor equal same being proccessed, put it back */
		if(((u32) sw_desc == *(dma->reg_addr.DAR)) &&
		   (*(dma->reg_addr.CSR) & DMA_CSR_CH_ACTIVE))
		{
			spin_lock(&dma->process_lock);
			list_add(&sw_desc->link, &dma->process_q);
			spin_unlock(&dma->process_lock);
			same_addr = 1;
		}
		else
		{
			DPRINTK("Add descriptor: %#x\n", (u32) sw_desc);
			spin_lock(&dma->hold_lock);
			/* push descriptor in hold Q */
			list_add_tail(&sw_desc->link, &dma->hold_q);
			spin_unlock(&dma->hold_lock);
		}
	}

	DPRINTK("Exiting dma_process()\n");
}					

/*=======================================================================*/
/*  Procedure:  dma_request()                                            */
/*                                                                       */
/*  Description:    This function requests a DMA chan depend on the bus. */
/*                  The bus could be primary PCI or secondary. Currently */
/*                  this parameter acts as a DMA channel selector        */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*               device_id: device ID                                    */
/*                                                                       */
/*  Returns:     int - channel #		                                 */
/*               -EINVAL: no such DMA channel exist                      */
/*				 -EBUSY: can't allocate irq								 */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*				Dave Jiang 9/02/01	Interface changed					 */
/*=======================================================================*/
int dma_request(dmach_t channel, const char *device_id)
{
	iop310_dma_t *dma = NULL;
	int ch = channel;
	int err;

	if(channel >= MAX_IOP310_DMA_CHANNEL)
	{
		return -EINVAL;
	}

	dma = &dma_chan[ch];
	atomic_inc(&dma->ref_count);

	/* get interrupt if ref count is less than or equal to 1 */
	if(atomic_read(&dma->ref_count) <= 1)
	{
		/* request IRQ for the channel */
		err = request_irq(dma->irq, dma_irq_handler, SA_INTERRUPT,
						  device_id, (void *)dma);
		if(err < 0)
		{
			printk(KERN_ERR "%s: unable to request IRQ %d for \
	  			   DMA channel %d: %d\n", device_id, dma->irq, ch, err);
			dma->lock = 0;
			atomic_set(&dma->ref_count, 0);
			return -EBUSY;
		}
		dma->device_id = device_id;
	}

	return ch;
}

/*=======================================================================*/
/*  Procedure:  dma_queue_buffer()                                       */
/*                                                                       */
/*  Description:    This function creates a DMA buffer chain from the    */
/*                  user supplied SGL chain. It also puts the DMA chain  */
/*                  onto the processing queue. DMA enable will be called */
/*                  by this func                                         */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*               listhead: User SGL (kernel memory)		                 */
/*                                                                       */
/*  Returns:     int: success -- 0                                      */
/*               dma_start() condition                                   */
/*                                                                       */
/*  Notes/Assumptions:  User SGL must point to kernel memory, not user   */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*=======================================================================*/
int dma_queue_buffer(dmach_t channel, dma_head_t * listhead)
{
	sw_dma_t *sw_desc = (sw_dma_t *) listhead->list;
	sw_dma_t *prev_desc = NULL;
	sw_dma_t *head = NULL;
	dma_head_t *sgl_head = listhead;
	int err = 0;
	iop310_dma_t *dma = &dma_chan[channel];
	DECLARE_WAIT_QUEUE_HEAD(wait_q);

	/* scan through entire user SGL */
	DPRINTK("Enter dma_queue_buffer()\n");
	while(sw_desc)
	{
		DPRINTK("Putting descriptor in process Q\n");
		/* get DMA descriptor */

		DPRINTK("sw_desc: %#x\n", (u32) sw_desc);
		sw_desc->sgl_head = (u32) listhead;

		/* we clean the cache for previous descriptor in chain */
		if(prev_desc)
		{
			/* prev_desc->dma_desc.NDAR = (u32)sw_desc->dma_phys; */
			cpu_dcache_clean_range((u32)&prev_desc->dma_desc, 
				(u32)&prev_desc->dma_desc + DMA_DESC_SIZE);
			
		}
		else
		{
			/* no previous descriptor, so we set this to be head */
			head = sw_desc;
		}

		sw_desc->head = head;
		/* set previous to current */
		prev_desc = sw_desc;

		/* put descriptor on process Q */
		DPRINTK("Adding swdesc: %#x\n", (u32) sw_desc);
		spin_lock_irq(&dma->process_lock);
		list_add_tail(&sw_desc->link, &dma->process_q);
		spin_unlock_irq(&dma->process_lock);

		/* go to next SGL */
		sw_desc = (sw_dma_t *) sw_desc->next;
	}
	DPRINTK("Done setting up descriptor list\n");

	/* if our tail exists */
	if(prev_desc)
	{
		/* set the head pointer on tail */
		prev_desc->head = head;
		/* set the header pointer's tail to tail */
		head->tail = prev_desc;
		prev_desc->tail = prev_desc;

		/* clean cache for tail */
		cpu_dcache_clean_range((u32)&prev_desc->dma_desc,
			   (u32)&prev_desc->dma_desc + DMA_DESC_SIZE);
		
		DPRINTK("Starting DMA engine\n");
		/* start the DMA */
		DPRINTK("Starting at chain: 0x%x\n", (u32) head);
		if((err = dma_start(dma, head)) < 0)
		{
			DPRINTK("dma_start() failed\n");
			return err;
		}
		else
		{
			if(!sgl_head->callback)
			{
				wait_event_interruptible(dma->wait_q,
					 (sgl_head->status & DMA_COMPLETE));
				DPRINTK("----W0EN: %#x\n", sgl_head->status);
			}
			return 0;
		}
	}
	else
	{
		DPRINTK("What happened to the chain?\n");
	}

	return -EINVAL;
}		


/*=======================================================================*/
/*  Procedure:  dma_suspend()                                            */
/*                                                                       */
/*  Description:    This function stops the DMA at the earliest instant  */
/*                  it is capable of. It actually suspends operation.    */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*                                                                       */
/*  Returns:     int: success -- 0                                      */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*=======================================================================*/
int dma_suspend(dmach_t channel)
{
	iop310_dma_t *dma = &dma_chan[channel];

	/* remove channel enable bit */
	*(dma->reg_addr.CCR) &= ~DMA_CCR_CHANNEL_ENABLE;

	return 0;
}

/*=======================================================================*/
/*  Procedure:  dma_resume()                                             */
/*                                                                       */
/*  Description:    This function resumes the DMA operations             */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*                                                                       */
/*  Returns:     int: success -- 0                                      */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*=======================================================================*/
int dma_resume(dmach_t channel)
{
	iop310_dma_t *dma = &dma_chan[channel];

	/* set channel enable bit */
	*(dma->reg_addr.CCR) |= DMA_CCR_CHANNEL_ENABLE;
	return 0;
}

/*=======================================================================*/
/*  Procedure:  dma_flush_all()                                          */
/*                                                                       */
/*  Description:    This function flushes the entire process Q for the   */
/*                  DMA channel in question. It also clears the DMAC.    */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*                                                                       */
/*  Returns:     int: success -- 0                                      */
/*                                                                       */
/*  Notes/Assumptions: Interrupt is masked unless called by app          */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*=======================================================================*/
int dma_flush_all(dmach_t channel)
{
	iop310_dma_t *dma = &dma_chan[channel];
	int flags;
	sw_dma_t *sw_desc;

	DPRINTK("Flushalll is being called\n");
	*(dma->reg_addr.CCR) = DMA_CCR_CLEAR;
	*(dma->reg_addr.CSR) = (DMA_CSR_DONE_MASK | DMA_CSR_ERR_MASK);

	while(!list_empty(&dma->hold_q))
	{
		spin_lock_irqsave(&dma->process_lock, flags);
		sw_desc = SW_ENTRY(dma->process_q.next);
		list_del(dma->process_q.next);
		spin_unlock_irqrestore(&dma->process_lock, flags);

		/* set status to be incomplete */
		sw_desc->status |= DMA_INCOMPLETE;
		/* put descriptor on holding queue */
		spin_lock_irqsave(&dma->hold_lock, flags);
		list_add_tail(&sw_desc->link, &dma->hold_q);
		spin_unlock_irqrestore(&dma->hold_lock, flags);
	}

	return 0;
}

/*=======================================================================*/
/*  Procedure:  dma_free()                                               */
/*                                                                       */
/*  Description:    This function frees the DMA channel from usage.      */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*                                                                       */
/*  Returns:     int: success -- 0                                      */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*              Dave Jiang 7/20/01 Added more clean up code              */
/*=======================================================================*/
void dma_free(dmach_t channel)
{
	iop310_dma_t *dma = &dma_chan[channel];

	atomic_dec(&dma->ref_count);

	if(atomic_read(&dma->ref_count) <= 1)
	{
		/* flush DMA channel */
		dma_flush_all(dma->channel);
		dma_task((void *)dma);
		dma->ready = 0;
		dma->last_desc = NULL;

		/* free the IRQ */
		free_irq(dma->irq, (void *)dma);
		dma->lock = 0;
	}

	DPRINTK("freed\n");
}	

/*=======================================================================*/
/*  Procedure:  dma_init()                                               */
/*                                                                       */
/*  Description:    This function initializes all the DMA channels.      */
/*                                                                       */
/*  Parameters:  NONE                                                    */
/*                                                                       */
/*  Returns:     int: success -- 0                                      */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 6/26/01  Initial Creation                     */
/*=======================================================================*/
int __init dma_init(void)
{
	int channel;
	int i;
	sw_dma_t *sw_desc = NULL;
	void *desc = NULL;

	printk("Intel 80310 DMA Copyright(c) 2001 Intel Corporation\n");
	DPRINTK("Initializing...\n");

	/* init free stack */
	INIT_LIST_HEAD(&dma_free_stack);
	/* init free stack spinlock */
	spin_lock_init(&dma_free_lock);

	/* pre-alloc DMA descriptors */
	for(i = 0; i < MAX_DMA_DESC; i++)
	{
		/* 
		 * we keep track of original address before alignment 
		 * adjust so we can free it later 
		 */
		desc = kmalloc((sizeof(sw_dma_t) + 0x20), GFP_KERNEL);
		memset(desc, 0, sizeof(sw_dma_t) + 0x20);

		/* 
		 * hardware descriptors must be aligned on an 
		 * 8-word boundary 
		 */
		sw_desc = (sw_dma_t *) (((u32) desc & 0xffffffe0) + 0x20);
		sw_desc->desc_addr = (u32) desc;
		sw_desc->dma_phys = (u32) virt_to_phys(sw_desc);


		/* put the descriptors on the free stack */
		spin_lock_irq(&dma_free_lock);
		list_add(&sw_desc->link, &dma_free_stack);
		spin_unlock_irq(&dma_free_lock);
	}

	dma_chan[0].reg_addr.CCR = IOP310_DMA0CCR;
	dma_chan[0].reg_addr.CSR = IOP310_DMA0CSR;
	dma_chan[0].reg_addr.DAR = IOP310_DMA0DAR;
	dma_chan[0].reg_addr.NDAR = IOP310_DMA0NDAR;
	dma_chan[0].reg_addr.PADR = IOP310_DMA0PADR;
	dma_chan[0].reg_addr.PUADR = IOP310_DMA0PUADR;
	dma_chan[0].reg_addr.LADR = IOP310_DMA0LADR;
	dma_chan[0].reg_addr.BCR = IOP310_DMA0BCR;
	dma_chan[0].reg_addr.DCR = IOP310_DMA0DCR;
	dma_chan[1].reg_addr.CCR = IOP310_DMA1CCR;
	dma_chan[1].reg_addr.CSR = IOP310_DMA1CSR;
	dma_chan[1].reg_addr.DAR = IOP310_DMA1DAR;
	dma_chan[1].reg_addr.NDAR = IOP310_DMA1NDAR;
	dma_chan[1].reg_addr.PADR = IOP310_DMA1PADR;
	dma_chan[1].reg_addr.PUADR = IOP310_DMA1PUADR;
	dma_chan[1].reg_addr.LADR = IOP310_DMA1LADR;
	dma_chan[1].reg_addr.BCR = IOP310_DMA1BCR;
	dma_chan[1].reg_addr.DCR = IOP310_DMA1DCR;
	dma_chan[2].reg_addr.CCR = IOP310_DMA2CCR;
	dma_chan[2].reg_addr.CSR = IOP310_DMA2CSR;
	dma_chan[2].reg_addr.DAR = IOP310_DMA2DAR;
	dma_chan[2].reg_addr.NDAR = IOP310_DMA2NDAR;
	dma_chan[2].reg_addr.PADR = IOP310_DMA2PADR;
	dma_chan[2].reg_addr.PUADR = IOP310_DMA2PUADR;
	dma_chan[2].reg_addr.LADR = IOP310_DMA2LADR;
	dma_chan[2].reg_addr.BCR = IOP310_DMA2BCR;
	dma_chan[2].reg_addr.DCR = IOP310_DMA2DCR;

	/* set the IRQ */
	dma_chan[0].irq = IRQ_IOP310_DMA0;
	dma_chan[1].irq = IRQ_IOP310_DMA1;
	dma_chan[2].irq = IRQ_IOP310_DMA2;

	/* initialize all the channels */
	for(channel = 0; channel < MAX_IOP310_DMA_CHANNEL; channel++)
	{
		/* init channel lock to 0 */
		atomic_set(&dma_chan[channel].ref_count, 0);
		dma_chan[channel].lock = 0;

		atomic_set(&dma_chan[channel].irq_thresh, 
				DEFAULT_DMA_IRQ_THRESH);

		/* init process Q */
		INIT_LIST_HEAD(&dma_chan[channel].process_q);
		/* init holding Q */
		INIT_LIST_HEAD(&dma_chan[channel].hold_q);
		/* init locks for Qs */
		spin_lock_init(&dma_chan[channel].hold_lock);
		spin_lock_init(&dma_chan[channel].process_lock);

		dma_chan[channel].last_desc = NULL;
		/* set the channel for DMA struct */
		dma_chan[channel].channel = channel;

		/* initialize BH task */
		dma_chan[channel].dma_task.sync = 0;
		dma_chan[channel].dma_task.routine = (void *)dma_task;

		/* initialize wait Q */
		init_waitqueue_head(&dma_chan[channel].wait_q);
		/* clear DMA channel control register */

		*(dma_chan[channel].reg_addr.CCR) = DMA_CCR_CLEAR;
		*(dma_chan[channel].reg_addr.CSR) |= 
			(DMA_CSR_DONE_MASK | DMA_CSR_ERR_MASK);
		*(dma_chan[channel].reg_addr.NDAR) = 0;
	}

	DPRINTK("DMA init Done!\n");
	return 0;
}				

/*=======================================================================*/
/*  Procedure:  dma_set_irq_threshold()                                  */
/*                                                                       */
/*  Description:    This function sets the threshold for irq service.    */
/*                  this is how many times irq gets serviced before bail */
/*                  so it wouldn't hog resources                         */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*               value: threshold value                                  */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 8/27/01  Initial Creation                     */
/*=======================================================================*/
void dma_set_irq_threshold(dmach_t channel, int value)
{
	iop310_dma_t *dma = &dma_chan[channel];

	atomic_set(&dma->irq_thresh, value);
}								/* End oi dma_set_irq_threshold() */

/*=======================================================================*/
/*  Procedure:  dma_get_buffer()                                         */
/*                                                                       */
/*  Description:    This function acquires an SGL element for the user   */
/*                  and returns that. It will retry multiple times if no */
/*                  descriptor is available.                             */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*               num_desc: number of descriptors                         */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 9/10/01  Initial Creation                     */
/*				Dave Jiang 10/04/01 Fixed linking up of sgl list		 */
/*=======================================================================*/
dma_sgl_t *dma_get_buffer(dmach_t channel, int num_desc)
{
	sw_dma_t *sw_desc = NULL;
	sw_dma_t *sw_head = NULL;
	sw_dma_t *sw_prev = NULL;

	int retry = 10;
	DECLARE_WAIT_QUEUE_HEAD(wait_q);
	int i;

	if(num_desc > MAX_DMA_DESC)
	{
		DPRINTK("More descriptor requested than allocated\n");
		return NULL;
	}

	for(i = num_desc; i > 0; i--)
	{
		spin_lock_irq(&dma_free_lock);
		if(!list_empty(&dma_free_stack))
		{
			sw_desc = SW_ENTRY(dma_free_stack.next);
			list_del(dma_free_stack.next);
			spin_unlock_irq(&dma_free_lock);
		}
		else
		{
			while(retry-- && !sw_desc)
			{
				spin_unlock_irq(&dma_free_lock);
				interruptible_sleep_on_timeout(&wait_q, SLEEP_TIME);
				spin_lock_irq(&dma_free_lock);
				if(!list_empty(&dma_free_stack))
				{
					sw_desc = SW_ENTRY(dma_free_stack.next);
					list_del(dma_free_stack.next);
				}
				spin_unlock_irq(&dma_free_lock);
			}

			if(!sw_desc)
			{
				printk(KERN_WARNING "80310 DMA: No more DMA descriptors");
				spin_lock_irq(&dma_free_lock);
				if(sw_head)
				{
					sw_desc = sw_head;
					while(sw_desc)
					{
						list_add(&sw_desc->link, &dma_free_stack);
						sw_desc = (sw_dma_t *) sw_desc->next;
					}
				}
				spin_unlock_irq(&dma_free_lock);
				return NULL;
			}
		}

		if(sw_prev)
		{
			sw_prev->next = (dma_sgl_t *) sw_desc;
			sw_prev->dma_desc.NDAR = sw_desc->dma_phys;
		}
		else
		{
			sw_head = sw_desc;
		}

		sw_prev = sw_desc;
	}

	sw_desc->dma_desc.NDAR = 0;
	sw_desc->next = NULL;
	sw_desc->status = 0;

	return (dma_sgl_t *) sw_head;
}

/*=======================================================================*/
/*  Procedure:  dma_return_buffer()                                      */
/*                                                                       */
/*  Description:    This function takes a list of SGL and return it to   */
/*                  the free stack.                                      */
/*                                                                       */
/*  Parameters:  channel: DMA channel                                    */
/*               list: SGL list to return to free stack                  */
/*                                                                       */
/*  Returns:     N/A                                                     */
/*                                                                       */
/*  Notes/Assumptions:                                                   */
/*                                                                       */
/*  History:    Dave Jiang 9/10/01  Initial Creation                     */
/*=======================================================================*/
void dma_return_buffer(dmach_t channel, dma_sgl_t * list)
{
	sw_dma_t *sw_desc = (sw_dma_t *) list;

	spin_lock_irq(&dma_free_lock);
	while(sw_desc)
	{
		list_add(&sw_desc->link, &dma_free_stack);
		sw_desc = (sw_dma_t *) sw_desc->next;
	}
	spin_unlock_irq(&dma_free_lock);

}

EXPORT_SYMBOL_NOVERS(dma_request);
EXPORT_SYMBOL_NOVERS(dma_queue_buffer);
EXPORT_SYMBOL_NOVERS(dma_suspend);
EXPORT_SYMBOL_NOVERS(dma_resume);
EXPORT_SYMBOL_NOVERS(dma_free);
EXPORT_SYMBOL_NOVERS(dma_set_irq_threshold);
EXPORT_SYMBOL_NOVERS(dma_get_buffer);
EXPORT_SYMBOL_NOVERS(dma_return_buffer);

module_init(dma_init);
