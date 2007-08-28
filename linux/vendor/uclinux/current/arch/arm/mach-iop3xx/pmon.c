/*
 * linux/arch/arm/mach-iop310/pmon.c
 *
 * Support functions for the Intel 80312 Performance Monitor Unit
 * See Documentation/arm/XScale/IOP310/pmon.txt for API
 *
 * Author: Chen Chen (chen.chen@intel.com)
 * Copyright (C) 2001 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * History:	07/23/2001 Chen Chen 
 * 			Initial Creation   
 *              07/27/2001 Chen Chen 
 *              	Add interrupt routine 
 *              09/28/2001 dsaxena
 *             		API and code cleanup, merge into iop310 tree 
 *
 *************************************************************************/

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <asm/atomic.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pmon.h>

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#include <linux/module.h>
#endif

#undef DEBUG
#ifdef DEBUG
#define DPRINTK(s, args...) printk("80310PMON: " s, ## args)
#else
#define DPRINTK(s, args...)
#endif

#define ENABLE_INTERRUPT 0x00000001
#define ENABLE_COUNTERS  0xFFFFFFFB
#define OVERFLOW         0xFFFFFFFF
#define NUM_OF_PECR      14
#define EMISR_SHIFT      1
#define EMISR_MASK       0x00000001
#define PMON_INT_MASK    0x00000010

static atomic_t usage;
static int claim_id;
iop310_pmon_res_t pmon_results;


/* static prototypes */
static void pmon_irq_handler(int, void *, struct pt_regs *);

/*=======================================================================*/
/*  Procedure:  pmon_irq_handler()                                       */
/*                                                                       */
/*  Description:   This function is the interrupt handler for the intel  */
/*                 80312 PMON.  When overflow occurs in PECR and GTSR     */
/*                 registers, an interrupt is generated to the intel     */
/*                 80200 chip.  This function handles the interrupt.     */
/*                 It increments the overflow counts and records it into */
/*                 the device data structure.                            */
/*                                                                       */
/*  Parameters:    int nIrq -- not used                                  */
/*                 void * pDevId -- not used                             */
/*                 pt_regs * pRegs -- not used                           */
/*                                                                       */
/*  Returns:       void                                                  */
/*                                                                       */
/*  Notes/Assumptions: none                                              */
/*                                                                       */
/*  History:	Chen Chen 07/27/2001 					 */
/*  			Initial Creation                 		 */
/*		dsaxena   09/28/2001					 */
/*			Removed looping and checking on FIQISR1		 */
/*=======================================================================*/
static void pmon_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	u32 em_isr_status = 0;
	u32 i = 0;

	DPRINTK("enter pmon_irq_handler()\n");

	/* get EMISR status */
	em_isr_status = *(IOP310_PMONEMISR);

	/* clear interrupts by writing back to it */
	/* since this is a read-clear register */
	*(IOP310_PMONEMISR) |= em_isr_status;

	/* if global timer overflows, increment the counter */
	if(em_isr_status & EMISR_MASK)
	{
		pmon_results.timestamp_overflow++;
	}

	/* shift register value to look at the next bit */
	em_isr_status = em_isr_status >> 1;

	/*loop to look at each PECR to see if overflow occurred */
	for(i = 0; i < NUM_OF_PECR; i++)
	{
		/* if overflow occurred, increment the counter */
		if(em_isr_status & EMISR_MASK)
		{
			pmon_results.event_overflow[i]++;
		}
		/* shift register value to look at the next bit */
		em_isr_status = em_isr_status >> 1;
	}

	DPRINTK("exit pmon_irq_handler()\n");
}

/*=======================================================================*/
/*  Procedure:  iop310_pmon_claim()                                      */
/*                                                                       */
/*  Description:   This function allows the user to claim the usage of   */
/*                 PMON.  The function returns an identifier.  When the  */
/*                 user is done using the PMON, the user has to          */
/*                 call iop310_pmon_release with this identifier to      */
/*                 release the PMON.                                     */
/*                                                                       */
/*  Parameters:    None                                                  */
/*                                                                       */
/*  Returns:       success: Int -- identifier being returned to the user */
/*                 failure: -EBUSY                                       */
/*                                                                       */
/*  History:       Chen Chen 07/23/2001 Initial Creation                 */
/*=======================================================================*/
int iop310_pmon_claim(void)
{
	int err;

	DPRINTK("enter pmon_claim()\n");

	/* if unable to read the usage variable, return with busy */
	if(atomic_read(&usage))
	{
		return -EBUSY;
	}

	/* increment the usage variable */
	atomic_inc(&usage);

	/* register the interrupt handler */
	err = request_irq(IRQ_IOP310_PMON, pmon_irq_handler,
				  SA_INTERRUPT, "IOP310 PMON", &pmon_results);

	/* return if unable to register the interrupt handler */
	if(err < 0)
	{
		DPRINTK(KERN_ERR "unable to request IRQ for PMON: %d\n", err);
		return err;
	}

	memzero(&pmon_results, sizeof(pmon_results));

	DPRINTK("exit pmon_claim()\n");

	return ++claim_id;
}

/*=======================================================================*/
/*  Procedure:  pmon_release()                                           */
/*                                                                       */
/*  Description:   When this function is called, it will release the PMON.*/
/*                 When the user is done with the PMON, the user need to  */
/*                 call this function with the identifier they got from  */
/*                 calling the i80312PmuClaim() to release the PMON.      */
/*                                                                       */
/*  Parameters:    u32 nClaimId                                          */
/*                 The identifier user got when calling i80312PmuClaim() */
/*                                                                       */
/*  Returns:       success:  0                                          */
/*                 failure: -EPERM                                       */
/*                                                                       */
/*  Notes/Assumptions: The user will release the PMON when done monitoring*/
/*                                                                       */
/*  History:       Chen Chen 07/23/2001 Initial Creation                 */
/*=======================================================================*/
int iop310_pmon_release(int claim)
{
	DPRINTK("enter pmon_release()\n");

	/* if the passed in id number doesn't match local id number, */
	/* return with wrong parameter */
	if(claim != claim_id)
	{
		return -EPERM;
	}

	atomic_dec(&usage);

	/* un-register the interrupt handler */
	free_irq(IRQ_IOP310_PMON, NULL);

	DPRINTK("exit pmon_release()\n");

	return 0;
}

/*=======================================================================*/
/*  Procedure:  iop310_pmon_start()                                      */
/*                                                                       */
/*  Description:   Each mode allows the user to monitor a set of events. */
/*                 The user need to call this function with a mode to    */
/*                 start the PMON monitoring process.  There are eight   */
/*                 monitored mode specified in the Event Select Register.*/
/*                                                                       */
/*  Parameters:    u32 nClaimID                                          */
/*                 The identifier user got when calling i80312PmuClaim() */
/*                 u32 nMode			                         */
/*                 The mode user would like to monitor in                */
/*                                                                       */
/*  Returns:       success:  0                                           */
/*                 failure:  -EPERM                                      */
/*                                                                       */
/*  Notes/Assumptions: None                                              */
/*                                                                       */
/*  History:       Chen Chen 07/23/2001 Initial Creation                 */
/*                 Dave Jiang 08/27/01  Combied Int enable & counter     */
/*=======================================================================*/
int iop310_pmon_start(int claim, int mode)
{
	DPRINTK("enter pmon_start()\n");

	if(claim != claim_id)
	{
		return -EPERM;
	}

	/* enable interrupt bit in Global Timer Mode Register */
	/* enable all counters in Global Timer Mode Register */
	*IOP310_PMONGTMR |= (ENABLE_INTERRUPT | ENABLE_COUNTERS);

	/* set the Event Select Register to the appropriate mode */
	/* (writing to ESR, also cause GTSR to reset to 0) */
	*IOP310_PMONESR |= mode;

	DPRINTK("exit pmon_start()\n");

	return 0;
}

/*=======================================================================*/
/*  Procedure:  pmon_stop()                                              */
/*                                                                       */
/*  Description:   When this function is called, the PMON will stop       */
/*                 monitoring the events.  It will read all registers,   */
/*                 and record them into pPmuResult.                      */
/*                                                                       */
/*  Parameters:    struct i80312PmuResults pPmuResult                    */
/*                 the place to write the result back to                 */
/*                                                                       */
/*  Returns:       success:  0                                          */
/*                                                                       */
/*  Notes/Assumptions: None                                              */
/*                                                                       */
/*  History:       Chen Chen 07/23/2001 Initial Creation                 */
/*=======================================================================*/
int iop310_pmon_stop(iop310_pmon_res_t *pmon_result)
{
	DPRINTK("enter pmon_stop()\n");

	memcpy(pmon_result, &pmon_results, sizeof(iop310_pmon_res_t));

	/* reset the monitoring mode back to 0 -- disable monitor */
	*IOP310_PMONESR = IOP310_PMON_MODE0;

	DPRINTK("exit pmon_stop()\n");

	return 0;
}

EXPORT_SYMBOL_NOVERS(iop310_pmon_claim);
EXPORT_SYMBOL_NOVERS(iop310_pmon_release);
EXPORT_SYMBOL_NOVERS(iop310_pmon_start);
EXPORT_SYMBOL_NOVERS(iop310_pmon_stop);

