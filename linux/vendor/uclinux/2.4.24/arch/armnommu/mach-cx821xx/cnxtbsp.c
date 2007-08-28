/****************************************************************************
*
*	Name:				CNXTBSP.C
*
*	Description:	Application Manager Application routines implementation.
*
*	Copyright:		(c) 2001 Conexant Systems Inc.
*
*****************************************************************************
  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************
*	$Author: davidm $
*	$Revision: 1.1 $
*	$Modtime: 10/25/02 8:25a $
****************************************************************************/

#define __KERNEL_SYSCALLS__

#include    <linux/kernel.h>
#include    <linux/types.h>
#include    <linux/sched.h>
#include    <linux/unistd.h>
#include    <linux/module.h>
#include    <linux/init.h>

#include    <asm/arch/bsptypes.h>
#include    <asm/arch/bspcfg.h>
#include    <asm/arch/gpio.h>
#include    <asm/arch/cnxtirq.h>
#include    <asm/arch/cnxtbsp.h>
#include    <asm/arch/pwrmanag.h>

#if CONFIG_CNXT_SOFTSAR || CONFIG_CNXT_SOFTSAR_MODULE
#include    <asm/arch/dmasrv.h>
#endif

#include    <asm/arch/syslib.h>
#include    <asm/arch/OsTools.h>
#include    <asm/arch/cnxttimer.h>

#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
#include    <asm/arch/dslisr.h>
#include    <asm/arch/cnxtadsl.h>
#endif

#define LED_DELAY_PERIOD   250 /* 250 ms */

HWSTATESTRUCT HWSTATE;

void CnxtBspLEDTask(void *sem)
{

	// This code does not seem to be necessary. Copied from linux/fs/buffer.c.
	// Does not seem to hurt. Might even work in 2.0.38 but not tested there.
	struct task_struct *tsk = current ;
	tsk->session = 1 ;
	tsk->pgrp = 1 ;
	strcpy ( tsk->comm, "CnxtBspLEDTask" ) ;

	/* sigstop and sigcont will stop and wakeup kupdate */
	spin_lock_irq(&tsk->sigmask_lock);
	sigfillset(&tsk->blocked);
	siginitsetinv(&current->blocked, sigmask(SIGCONT) | sigmask(SIGSTOP));
	recalc_sigpending(tsk);
	spin_unlock_irq(&tsk->sigmask_lock);

	up((struct semaphore *)sem);
	
	while(1)
	{

		/* Update Ready GPIO */
		WriteGPIOData(GPOUT_TASKRDY_LED, LED_ON);
		TaskDelayMsec(LED_DELAY_PERIOD);
		WriteGPIOData(GPOUT_TASKRDY_LED, LED_OFF);

		TaskDelayMsec(LED_DELAY_PERIOD);

	}
}


static STATUS CnxtBspInitTasks( void )
{
	DECLARE_MUTEX_LOCKED( sem );

	kernel_thread( (void *)CnxtBspLEDTask, &sem, CLONE_FS | CLONE_FILES  );
	down(&sem);

#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
	kernel_thread( (void *)CnxtAdslLEDTask, &sem, CLONE_FS | CLONE_FILES  );
	down(&sem);
#endif
	return OK;
}


static int __init CnxtBspInit(void)
{
	sysHwInit2();

	CnxtBspInitTasks();
	
	return 0;
}


/******************************************************************************
|
|  Function:	CnxtBsp_Get_Random_Byte
|
|  Description:	Used to get a somewhat random byte that is used by the
|				CnxtBsp_Get_Mac_Address function for the last byte of 
|				a mac address when it has to create one because the flash
|				device did not have one programmed into it.
|
|  Returns:   unsigned char 
|
*******************************************************************************/

unsigned char CnxtBsp_Get_Random_Byte( void )
{
	/* Read System timer 1, which was turned on at power-on. */
	DWORD uTimerValue = *(UINT16 *)0x350020;

	return uTimerValue & 0xff;
}


/******************************************************************************
|
|  Function:	CnxtBsp_Get_Mac_Address
|
|  Description:	Used to either retrieve the mac addresses programmed into
|				the flash device or to generate a valid Conexant mac address
|				with a somewhat random last byte for the mac address.
|
|  Returns:		NONE 
|
*******************************************************************************/

void CnxtBsp_Get_Mac_Address( int emac_num, char *mac_addr_array )
{
	// Return a hardcoded mac address for now until
	// we have functionality to program/retrieve mac
	// addresses from the flash.
	*mac_addr_array++ = 0x00;
	*mac_addr_array++ = 0x30;
	*mac_addr_array++ = 0xcd;
	*mac_addr_array++ = 0x00;
	*mac_addr_array++ = 0x02;
	*mac_addr_array = CnxtBsp_Get_Random_Byte();
}

#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE

/****************************************************************************
 *
 *  Name:			void DSL_RX_DMA_ISR(void)
 *
 *  Description:	DSL RX DMA interrupt service routine
 *
 *  Inputs:			(int irq, void *dev_id, struct pt_regs * regs)
	*
 *  Outputs:		none
 *
	NOTES:
		Linux specific interrupt handler interface to DSL receive isr.
		
 ***************************************************************************/
 
void DSL_RX_DMA_ISR(int irq, void *dev_id, struct pt_regs * regs)
{
   register UINT32 OldLevel; 

   OldLevel = intLock(); 
   DSL_RX_Handler();
   intUnlock(OldLevel); 
}

void DSL_ERR_ISR(int irq, void *dev_id, struct pt_regs * regs)
{
   register UINT32 OldLevel; 

   OldLevel = intLock(); 
   DSL_ERR_Handler();
   intUnlock(OldLevel); 
}

#endif

module_init(CnxtBspInit)

EXPORT_SYMBOL(CnxtBsp_Get_Mac_Address);
