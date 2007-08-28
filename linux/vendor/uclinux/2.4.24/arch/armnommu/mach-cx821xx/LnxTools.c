/****************************************************************************
*
*	Name:			LnxTools.c
*
*	Description:	Provides OS independent kernel functions
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
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

****************************************************************************
*  $Author: davidm $
*  $Revision: 1.1 $
*  $Modtime: 9/06/02 4:20p $
****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <asm/arch/OsTools.h>

void INIT_EVENT( EVENT_HNDL* pEvent )
{
	pEvent->SetFlag = 0;
	pEvent->WaitCnt = 0;
	init_waitqueue_head( &pEvent->WaitQue);
	INIT_SPIN_LOCK( &pEvent->FlagLock );
}

int WAIT_EVENT( EVENT_HNDL* pEvent, ULONG msecDelay )
{
	long int TimeRemain;
	unsigned long flags ;

	// wait on the queue
	// Round up to next jiffie for low Linux 100 Hz resolution and
	// add 1 jiffie for unsynchronized timers
	// and add 1 more jiffie for timer rate differences
	// and add 2 more jiffie because unknown factors are causing timeouts
	TimeRemain = ((msecDelay+49) * HZ) / 1000;

	// if no wait time then just return failure
	if ( ! msecDelay )
	{
		return 0;
	}

	// atomically sleep if event is not set - we must protect the window between
	// finding out if the flag is already set and sleeping as if an interrupt
	// occurs in the window and calls SET_EVENT, SET_EVENT will signal the wait
	// queue before we get placed on it and we will miss this signal.
	save_flags (flags) ;
	cli () ;

	// indicate that I am waiting
	pEvent->WaitCnt++;

	if ( ! pEvent->SetFlag )
	{
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
			while ( !pEvent->SetFlag && TimeRemain )
			{
				// safe to call sleep with interrupts off as kernel will re-enable
				// interrupts after placing us on the queue and before calling schedule
				TimeRemain = interruptible_sleep_on_timeout ( &pEvent->WaitQue,  TimeRemain );
			}
		#else
			while ( !pEvent->SetFlag )
			{
				// safe to call sleep with interrupts off as kernel will re-enable
				// interrupts after placing us on the queue and before calling schedule
				interruptible_sleep_on ( &pEvent->WaitQue );
			}
		#endif
	}
	// remove wait indication
	pEvent->WaitCnt--;

	restore_flags (flags) ;

	// return setflag
	return pEvent->SetFlag;
}

void SET_EVENT( EVENT_HNDL* pEvent )
{
	DWORD LockFlag;

	// if the event is cleared then
	// set the flag, wake up all waiting tasks.
	ACQUIRE_LOCK( &pEvent->FlagLock, LockFlag );
	if ( !pEvent->SetFlag )
	{
		int cnt;
		pEvent->SetFlag = TRUE;
		for  ( cnt=0; cnt<pEvent->WaitCnt; cnt++ )
		{
			wake_up_interruptible ( &pEvent->WaitQue  );
		}
	}
	RELEASE_LOCK( &pEvent->FlagLock, LockFlag );
}

void SET_EVENT_FROM_ISR( EVENT_HNDL* pEvent )
{
	// if the event is cleared then
	// set the flag, wake up all waiting tasks.
	ACQUIRE_LOCK_AT_ISR( &pEvent->FlagLock );
	if ( !pEvent->SetFlag )
	{
		int cnt;
		pEvent->SetFlag = TRUE;
		for  ( cnt=0; cnt<pEvent->WaitCnt; cnt++ )
		{
			wake_up_interruptible ( &pEvent->WaitQue  );
		}
	}
	RELEASE_LOCK_AT_ISR( &pEvent->FlagLock );
}

void RESET_EVENT( EVENT_HNDL* pEvent )
{
	DWORD LockFlag;

	// clear the event flag
	ACQUIRE_LOCK( &pEvent->FlagLock, LockFlag );
	pEvent->SetFlag = FALSE;

	// empty out the queue
	while ( waitqueue_active( &pEvent->WaitQue ) )
		interruptible_sleep_on( &pEvent->WaitQue );

	RELEASE_LOCK( &pEvent->FlagLock, LockFlag );
}


EXPORT_SYMBOL(INIT_EVENT);
EXPORT_SYMBOL(WAIT_EVENT);
EXPORT_SYMBOL(SET_EVENT);
EXPORT_SYMBOL(SET_EVENT_FROM_ISR);
EXPORT_SYMBOL(RESET_EVENT);


