/****************************************************************************
*
*	Name:			cnxttimer.c
*
*	Description:	CNXT timer driver
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
*  $Modtime: 10/16/02 10:25a $
****************************************************************************/

/*
DESCRIPTION
This library contains routines to manipulate the timer functions on a 
CNXT timer. This driver provides 3 main functions, system clock 
support, auxiliary clock support, and timestamp timer support.  The 
timestamp function is always conditional upon the INCLUDE_TIMESTAMP macro.


There are three programmable timers available with a range up to 65ms.
The timers are based on 16-bit counters that increment at a 1.0MHz rate.
This gives a resolution of 1 microsecond.  The third timer is also used
as a system watchdog.  If the interrupt for this timer is not cleared
before the next interrupt event a reset occurs.


The timers increment from 0 up to a limit value.  The limit value is
is programmed into the TM_LMT1 register for timer 1, TM_LMT2 for timer 2
and TM_LMT3 for timer 3.  When the timer reaches the limit value, it resets
back to zero and sets a corresponding interrupt.  If TM_LMT is set to zero,
then TM_CNT stays reset and never causes an interrupt.  TM_CNT is reset to
zero whenever the TM_LMT register is written to.

The timer is loaded by writing to TM_LMT register, and then, if
enabled, the timer register TM_CNT will count up to the limit value set 
in TM_LMT. 


At any time, the current timer value may be read from the TM_CNT register.


REGISTERS:

Note: x used in for example, TM_CNT(x) represents timers 1,2,3.  There
are six timer registers for the three timers.

TM_CNT(x): (read only)  Current count value of the timer.  The timer
                        increments every 1 uS from 0 to the limit
                        value, then resets to 0 and counts again. TM_CNT
                        is reset whenever TM_LMT is written.

TM_LMT(x): (read/write) When the current count value of the timer reaches
                        the limit value, an interrupt INT_TM is set. 
                        the periodic timer interrupt event rate is = 
                        1 MHz / (TM_LMT + 1).  If TM_LMT is set to 0, 
                        TM_CNT remains reset.  TM_CNT is reset whenever
                        TM_LMT is written.

The macros SYS_CLK_RATE_MIN, SYS_CLK_RATE_MAX, AUX_CLK_RATE_MIN, and
AUX_CLK_RATE_MAX must be defined to provide parameter checking for the
sys[Aux]ClkRateSet() routines.  The following macros must also be defined:

SYS_TIMER_CLK			/@ frequency of clock feeding SYS_CLK timer @/
AUX_TIMER_CLK			/@ frequency of clock feeding AUX_CLK @/
CNXT_RELOAD_TICKS		/@ any overhead in ticks when timer reloads @/
SYS_TIMER_CLEAR			/@ addresses of timer registers @/
SYS_TIMER_CTRL			/@ "" @/
SYS_TIMER_LOAD			/@ "" @/
SYS_TIMER_VALUE			/@ "" @/
AUX_TIMER_CLEAR			/@ "" @/
AUX_TIMER_CTRL			/@ "" @/
AUX_TIMER_LOAD			/@ "" @/
AUX_TIMER_VALUE			/@ "" @/
SYS_TIMER_INT_LVL		/@ interrupt level for sys Clk @/
AUX_TIMER_INT_LVL		/@ interrupt level for aux Clk @/
CNXT_TIMER_SYS_TC_DISABLE	/@ Control register values used when @/
CNXT_TIMER_SYS_TC_ENABLE	/@ enabling and disabling the two timers @/
CNXT_TIMER_AUX_TC_DISABLE	/@ "" @/
CNXT_TIMER_AUX_TC_ENABLE	/@ "" @/

The following may also be defined, if required:
CNXT_TIMER_READ (reg, result)	/@ read an CNXT timer register @/
CNXT_TIMER_WRITE (reg, data)	/@ write ... @/
CNXT_TIMER_INT_ENABLE (level)	/@ enable an interrupt @/
CNXT_TIMER_INT_DISABLE (level)	/@ disable an interrpt @/

BSP
Apart from defining such macros described above as are needed, the BSP
will need to connect the interrupt handlers (typically in sysHwInit2()).
e.g.

.CS
    /@ connect sys clock interrupt and auxiliary clock interrupt @/

    intConnect (INUM_TO_IVEC (INT_VEC...), sysClkInt, 0);
    intConnect (INUM_TO_IVEC (INT_VEC...), sysAuxClkInt, 0);
.CE

INCLUDES:
CNXTTimer.h
timestampDev.h

SEE ALSO:
.I "CX82100 Silicon System Design Engineering Specification"
*/


/* includes */

#include <linux/config.h>

#include <linux/module.h>
#include <linux/init.h>

#include <linux/param.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/arch/irq.h>

#include <asm/arch/bsptypes.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/OsTools.h>
#include <asm/arch/cnxttimer.h>
#include <asm/arch/cnxtirq.h>
#include <asm/arch/gpio.h>

/* locals */

#ifdef	INCLUDE_TIMESTAMP
static BOOL	sysTimestampRunning  	= FALSE;    /* timestamp running flag */
#endif /* INCLUDE_TIMESTAMP */

#define ONE_MILLISECOND	1000
#define STOP 			0
#define NUMBER_OF_TIMERS	4
#define TIMER_OFFSET		4 
#define NO_TIMER_INT		-9

#if INCLUDE_AUX_CLK
	// Number of callbacks per timer
	#define NUMCBS 16

	// Callback control structure
	#define VCB_CB_ENABLED 1
	#define VCB_ONE_SHOT 2

	#define isCbEnabled(x) (x.Options & VCB_CB_ENABLED)
	#define SetCbEnabled(x) (x.Options|= VCB_CB_ENABLED)
	#define ClearCbEnabled(x) (x.Options&= (~VCB_CB_ENABLED))

	#define isOsEnabled(x) (x.Options & VCB_ONE_SHOT)
	#define SetOsEnabled(x) (x.Options|= VCB_ONE_SHOT)
	#define ClearOsEnabled(x) (x.Options&= (~VCB_ONE_SHOT))

	typedef struct tagVirCallback
	{
		UINT32		Options;
		int			TimeUntilCallback;
		int			TimeTally;
		void		(*pTimerRoutine)(int);
		int         TimerArg;
		UINT32		TimerTicksPerSecond;
	} VIRCALLBACK,*PVIRCALLBACK;

	// Zero is reserved for standard sysAux* routines
	VIRCALLBACK vcbAux[NUMCBS];
#endif

// Hardware structure for the timers
HW_TIMER HW_Timers[MAX_NUM_HW_TIMERS];

// Interrupt table
void (*TimerHandler_Tbl[MAX_NUM_HW_TIMERS])(int irq, void *dev_id, struct pt_regs * regs) =
{
	NULL,				// Timer 1
	NULL,				// Timer 2
	Timer3IntHandler,	// Timer 3
#if INCLUDE_AUX_CLK
	sysAuxClkInt		// Timer 4
#else
	NULL
#endif
};

#ifdef INCLUDE_AUX_CLK
// Local functions for virtual callback support
static UINT32 vcbMaxRate(void);
static void vcbClkEnable (int cbh);
static STATUS vcbClkConnect(FUNCPTR routine,int arg,int j);
static int vcbAnyCbEnabled(void);
static void vcbClkDisable (int cbh);
#endif

// !!! test code !!!
DWORD _10ms_or_less;

// GENERAL PURPOSE TIMER STUFF

typedef struct _GP_TIMER_T_
{
	EVENT_HNDL	*pEvent;		// Event to wait on/signal for connected task
	DWORD		pid;			// ID of the task that installed function
	BOOLEAN		periodic;		// TRUE = periodic, FALSE = one_shot
	
} GP_TIMER_T;

GP_TIMER_T	gp_timer[NUMBER_OF_TIMERS];
int		gp_timer_task[NUMBER_OF_TIMERS];
DWORD	millisecond_ticks;


/******************************************************************************

FUNCTION NAME:
		gp_timer_start

ABSTRACT:
		Starts the general purpose timer
DETAILS:
		NOTE: The maximum time interval for this timer is 65535 usec.

*******************************************************************************/

BOOLEAN gp_timer_start
(
	WORD 	usec,
	BOOLEAN periodic,
	WORD	timerInt
)
{
	GP_TIMER_T	*gp_timer_t = &gp_timer[timerInt];
	
	// Is this the same task that connected a function?
	if ( gp_timer_t->pid == current->pid )
	{
		// Set the mode of operation
		gp_timer_t->periodic = periodic;
	 
		// NOTE: Setting the TM_LMTx register to a non-zero
		// value will both reset and start the timer.
		switch( timerInt )
		{
			case INT_LVL_TIMER_4:
				*TM_LMT4 = usec;
				break;

			case INT_LVL_TIMER_2:
				*TM_LMT2 = usec;
				break;

			default:
				return FALSE;
		}	
		
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
	

/******************************************************************************

FUNCTION NAME:
		gp_timer_stop

ABSTRACT:
		Stops the general purpose timer
DETAILS:

*******************************************************************************/

BOOLEAN gp_timer_stop( WORD timerInt )
{
	GP_TIMER_T	*gp_timer_t = &gp_timer[timerInt];
	
	// Is this the same task that connected the function?
	if ( gp_timer_t->pid == current->pid )
	{
		// NOTE: Setting the TM_LMTx register to a zero
		// value will both reset and stop the timer.
		*(TM_LMT1 + timerInt * TIMER_OFFSET) = 0;
		
		// Reset the mode of operation to default one-shot operation
		gp_timer_t->periodic = FALSE;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


/******************************************************************************

FUNCTION NAME:
		gp_timer2_isr

ABSTRACT:
		General purpose timer interrupt handler routine.
DETAILS:
		Uses CNXT timer 2 as it's hardware timer.

*******************************************************************************/

void gp_timer2_isr(int irq, void *dev_id, struct pt_regs * regs)
{
	
	GP_TIMER_T	*gp_timer_t = &gp_timer[INT_LVL_TIMER_2];
	
	// Clear out the timer2 interrupt
    *((volatile UINT32*)INT_STAT) = INT_TM2;
	
	// Check the mode of operation
	if ( gp_timer_t->periodic == FALSE )
	{
		// We are operating in a one-shot mode 
		// so stop the timer from running
		*TM_LMT2 = 0;
	}

	// If there is an event connected to this timer? 
	if(	gp_timer_t->pEvent != NULL )
	{
		SET_EVENT_FROM_ISR( gp_timer_t->pEvent );
	}
}	

/******************************************************************************

FUNCTION NAME:
		gp_timer1_isr

ABSTRACT:
		General purpose timer interrupt handler routine.
DETAILS:
		Uses CNXT timer 1 as it's hardware timer.

*******************************************************************************/

void gp_timer4_isr(int irq, void *dev_id, struct pt_regs * regs)
{
	
	GP_TIMER_T	*gp_timer_t = &gp_timer[INT_LVL_TIMER_4];
	
	// Clear out the timer1 interrupt
    *((UINT32*)INT_STAT) = INT_TM4;
	
	// Check the mode of operation
	if ( gp_timer_t->periodic == FALSE )
	{
		// We are operating in a one-shot mode 
		// so stop the timer from running
		*TM_LMT4 = 0;
	}

	// If there is an event connected to this timer? 
	if(	gp_timer_t->pEvent != NULL )
	{
		SET_EVENT_FROM_ISR( gp_timer_t->pEvent );
	}
}	

/******************************************************************************

FUNCTION NAME:
		gp_timer_init

ABSTRACT:
		Starts the general purpose timer
DETAILS:
		NOTE: The maximum time interval for this timer is 65535 usec.

*******************************************************************************/

BOOLEAN gp_timer_init( WORD timerInt )
{
	GP_TIMER_T	*gp_timer_t = &gp_timer[timerInt];
	
	
	
	gp_timer_t->pEvent = 0;
	gp_timer_t->periodic = FALSE;
	gp_timer_t->pid = 0;	
	
	switch( timerInt )
	{
		case INT_LVL_TIMER_2:

			*TM_LMT2 = 0;	
			if( request_irq( INT_LVL_TIMER_2, &gp_timer2_isr, 0, "dmt hi-res timer", 0 ))
			{
				// What do we do if the interrupt does not connect
				return FALSE;
			} 
			cnxt_unmask_irq ( timerInt );
			break;

		case INT_LVL_TIMER_4:

			*TM_LMT4 = 0;
			if( request_irq( INT_LVL_TIMER_4, &gp_timer4_isr, 0, "SAR traffic shaping", 0 ))
			{
				// What do we do if the interrupt does not connect
				return FALSE;
			} 
			cnxt_unmask_irq ( timerInt );	
			break;

		default:
			return FALSE;
	}
	
	return TRUE;
}	


/******************************************************************************

FUNCTION NAME:
		gp_timer_connect

ABSTRACT:
		Connects a function and function argument that
		will be called when the gp timer interrupt handler
		executes.

DETAILS:
		Uses CNXT timer 2 as it's hardware timer.
		TRUE = function connected, FALSE = function not connected

*******************************************************************************/

BOOLEAN gp_timer_connect
(
	EVENT_HNDL	*pEvent,
	WORD		timerInt
)
{
	GP_TIMER_T	*gp_timer_t = &gp_timer[timerInt];
	
	// Is there already a connected event?
	if ( gp_timer_t->pEvent == NULL )
	{
		gp_timer_t->pEvent = pEvent;
		gp_timer_t->pid = current->pid;
		gp_timer_task[timerInt] = current->pid;
		return TRUE;
	}
	else
	{
	  return FALSE;
	}
}	 


/******************************************************************************

FUNCTION NAME:
		gp_timer_disconnect

ABSTRACT:
		Disconnects a function and function argument that
		were connected to the interrupt handler.

DETAILS:
		Only the task that connected the function can disconnect it.
		TRUE = function disconnected, FALSE = function not disconnected

*******************************************************************************/

BOOLEAN gp_timer_disconnect( WORD timerInt )
{
	GP_TIMER_T	*gp_timer_t = &gp_timer[timerInt];
	
	// Is this the same task that connected the function?
	if ( gp_timer_t->pid == current->pid )
	{
		gp_timer_t->pEvent = 0;
		gp_timer_t->pid = 0;
		gp_timer_task[timerInt] = 0;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


/* If duration passed in is smaller than current system */
/* timer resolution than by default it will be forced   */
/* to current resolution. */

void TaskDelayMsec(UINT32 TimeDuration)
{
   register int NumTickDelay;
   DWORD  timerInt = NO_TIMER_INT;	

   // if task registered itself as a user of the hardware timer and its request
   // does not exceed the capabilities of the timer
   if( ( current->pid == gp_timer_task[INT_LVL_TIMER_4] ) && ( TimeDuration <= 65 ) ) timerInt = INT_LVL_TIMER_4; 
   if( ( current->pid == gp_timer_task[INT_LVL_TIMER_2] ) && ( TimeDuration <= 65 ) ) timerInt = INT_LVL_TIMER_2;

   if ( timerInt != NO_TIMER_INT )
	{
		GP_TIMER_T	*gp_timer_t = &gp_timer[timerInt];
	
		// Reset the event before we start the timer that will set it.
		RESET_EVENT ( gp_timer_t->pEvent ) ;

		// If the delay is zero then just give any other
		// waiting tasks a chance to run.
		if( TimeDuration < 1 )
		{
			schedule();
		}
		else
		{
			// - start gp timer
			if ( gp_timer_start( (TimeDuration * 1000), FALSE, timerInt ) == FALSE )
			{
				printk("cnxttimer.c: gp timer did not start.\n");
				return;
			}
			
			// - wait on event to be set or timeout to expire
			//	 event structure located in task context
			// - Round up to next jiffie for low Linux 100 Hz resolution and
			//   add 1 jiffie for unsynchronized timers
			//   and add 1 more jiffie for timer rate differences
			if ( ! WAIT_EVENT ( gp_timer_t->pEvent, TimeDuration + 29 ) )
			{
				printk("cnxttimer.c: Timer expired without event being signaled.\n");
			}
		}
	}
	else
	{
	  // !!! test code !!!	
	  if( TimeDuration < 10 )
	  {
	        _10ms_or_less++;
	  }	  
	  // !!! test code !!!	

	  if( TimeDuration < 1 )
	  {
	        schedule();
	  }
	  else
	  {
	        /* add 999 to round up to the closest ticks */
		NumTickDelay = ( ((TimeDuration * HZ) + 999) / 1000) ;

		/* Add one to guarantee at least 1 tick delay. */   
		if( NumTickDelay < 1 )
		{
		   NumTickDelay++;
		}

		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout ( NumTickDelay ) ;
	  }

	}
}


/******************************************************************************

FUNCTION NAME:
		_1MS_timebase_isr

ABSTRACT:
		Provides a one millisecond time base for falcon code
DETAILS:
		Uses CNXT timer 3 as it's hardware timer.

*******************************************************************************/

void _1MS_timebase_isr(int irq, void *dev_id, struct pt_regs * regs)
{
	
	// Clear out the timer3 interrupt
    *((UINT32*)INT_STAT) = INT_TM3;
	
	// Increment the one millisecond time base
	millisecond_ticks++;	
}	


/******************************************************************************

FUNCTION NAME:
		_1MS_timebase_start

ABSTRACT:
		Starts the 1 ms timebase timer
DETAILS:

*******************************************************************************/

BOOLEAN _1MS_timebase_start( void )
{

	*TM_LMT3 = STOP;

	if( request_irq( INT_LVL_TIMER_3, &_1MS_timebase_isr, 0, "one millisecond timer isr", 0 ))
	{
		// What do we do if the interrupt does not connect
		return FALSE;
	} 

	// NOTE: Setting the TM_LMTx register to a non-zero
	// value will both reset and start the timer.
	*TM_LMT3 = ONE_MILLISECOND;
	
	return TRUE;
}	


/****************************************************************************
 *
 *  Name:			void SysTimerDelay ( UINT32 uMicroSec )
 *
 *  Description:	
 *
 *  Inputs:		UINT32 uMicroSec = number of microseconds to delay minus 1
 *
 *  Outputs:		TRUE = delay succeeded,  FALSE = delay failed.
 *
 ***************************************************************************/

/* Do not use in the task or interrupt context, this is for HW */
/* delay loop. */
BOOL sysTimerDelay( UINT32 uMicroSec )
{

   /* Don't allow a delay any longer than 65535 useconds */
   if ( (uMicroSec > 0xFFFF) || (uMicroSec == 0) )
   {
      return FALSE;
   }

   udelay( uMicroSec );
   return TRUE;
}


/******************************************************************************

FUNCTION NAME:
		sysClkRateGet

ABSTRACT:
		
DETAILS:

*******************************************************************************/

int sysClkRateGet (void)
{
	return HZ;
}	


/******************************************************************************

FUNCTION NAME:
		tickGet

ABSTRACT:
		
DETAILS:

*******************************************************************************/

ULONG tickGet(void)
{
	return jiffies;
}

	
#if INCLUDE_AUX_CLK

/******************************************************************************

FUNCTION NAME:
		vcbUpdateCounter

ABSTRACT:
		
DETAILS:

*******************************************************************************/

static BOOL vcbUpdateCounter(UINT32 Ticks)
{
	register int cbh;
	register BOOL TimerShiftRate= FALSE;

	// Do not use the Timer API callback variables in the HW_Timer structure
	for (cbh = 0; cbh < NUMCBS; cbh++)
	{
		if (isCbEnabled(vcbAux[cbh]))
		{
			// Add total time from H/W interrupt
			vcbAux[cbh].TimeTally+= Ticks;

			// Check if greater than selected callback time 
			if (vcbAux[cbh].TimeTally >= vcbAux[cbh].TimeUntilCallback)
			{
				// Do a subtraction to maintain long-term average (with jitter)
				vcbAux[cbh].TimeTally-= vcbAux[cbh].TimeUntilCallback;
				if (vcbAux[cbh].pTimerRoutine)
				{
					(*vcbAux[cbh].pTimerRoutine)(vcbAux[cbh].TimerArg);

					// Check if this the first and last time this callback is used
					if (isOsEnabled(vcbAux[cbh]))
					{
						// Clear the callback and info
						sysAuxClkDisconnectExt(cbh);

						// Flag a possible change to the interrupt rate
						TimerShiftRate= TRUE;
					}
				}
			}
		}
	}

	return TimerShiftRate;
}


/******************************************************************************

FUNCTION NAME:
		vcbMaxRate

ABSTRACT:
		Find the fastest rate for all callbacks (including the legacy
		sysAux callback at index 0)
DETAILS:

*******************************************************************************/

static UINT32 vcbMaxRate(void)
{
	register UINT32 maxrate;
	register UINT32 j;

	maxrate=0;
	for (j = 0; j < NUMCBS; j++)
	{
		if (isCbEnabled(vcbAux[j]))
		{
			maxrate= max(maxrate,vcbAux[j].TimerTicksPerSecond);
		}
	}

	return maxrate;
}


/******************************************************************************

FUNCTION NAME:
		vcbFind
ABSTRACT:

DETAILS:
		Find the index of the callback structure for a given callback address.
		Ignore legacy at index 0.

*******************************************************************************/

static int vcbFind(FUNCPTR routine,int arg)
{
	register int j;

	// Zero index is reserved for legacy sysAux* routines
	for (j = 1; j < NUMCBS; j++)
		if (vcbAux[j].pTimerRoutine == routine && vcbAux[j].TimerArg == arg)
			return j;

	return -1;
}


/******************************************************************************

FUNCTION NAME:
		vcbClkConnect

ABSTRACT:

DETAILS:
		Record the function pointer and the argument for the interrupt routine.

*******************************************************************************/

static STATUS vcbClkConnect(FUNCPTR routine,int arg,int j)
{
   #if ((CPU == ARM7TDMI_T) || (CPU == ARMARCH4_T))
   /* set b0 so that sysClkConnect() can be used from shell */
   vcbAux[j].pTimerRoutine = (FUNCPTR)((UINT32)routine | 1);
   #else
   vcbAux[j].pTimerRoutine = routine;
   #endif (CPU == ARM7TDMI_T)

   vcbAux[j].TimerArg      = arg;

   // The only thing we need from the Timer API
   HW_Timers[AUX_TIMER].TimerConnected = TRUE;

   return OK;
}


/******************************************************************************

FUNCTION NAME:
		vcbClkDisable
ABSTRACT:
		Disable a callback
DETAILS:

*******************************************************************************/

static void vcbClkDisable (int cbh)
{
    register int oldLevel;

    oldLevel = intLock();

	// Disable callback
	ClearCbEnabled(vcbAux[cbh]);

	// Disable the H/W if there are no more callbacks, otherwise 
	// flag to change the resolution in the interrupt.
	if (vcbAnyCbEnabled() == -1)
	{
		TimerDisable(&HW_Timers[AUX_TIMER], FALSE);
	}
	else
	{
		HW_Timers[AUX_TIMER].TimerShiftRate= TRUE;
	}

    intUnlock(oldLevel);
}


/******************************************************************************

FUNCTION NAME:
		vcbAnyCbEnabled
ABSTRACT:
		Find index for the first enabled callback.
DETAILS:

*******************************************************************************/

static int vcbAnyCbEnabled(void)
{
	register int cbh;

	// Zero index is reserved for legacy sysAux* routines
	for (cbh = 0; cbh < NUMCBS; cbh++)
		if (isCbEnabled(vcbAux[cbh]))
			return cbh;

	return -1;
}


/******************************************************************************

FUNCTION NAME:
		vcbClkEnable
ABSTRACT:
		Enable a callback
DETAILS:

*******************************************************************************/

static void vcbClkEnable (int cbh)
{
	// Enable callback
	SetCbEnabled(vcbAux[cbh]);

	// Recompute number of microsenconds between interrupts for this callback
	vcbAux[cbh].TimeUntilCallback= HW_Timers[AUX_TIMER].TimerClkSpeed / vcbAux[cbh].TimerTicksPerSecond;
	// Clear the number of microseconds duration counter
	vcbAux[cbh].TimeTally= 0;

	// If the H/W timer is running, then shift the resolution in the interrupt
	if (HW_Timers[AUX_TIMER].TimerRunning)
	{
		HW_Timers[AUX_TIMER].TimerShiftRate= TRUE;
	}
	else
	{
		// Otherwise, compute the new resolution and go
		HW_Timers[AUX_TIMER].TimerShiftRate= FALSE;
		HW_Timers[AUX_TIMER].TimerTicksPerSecond= vcbMaxRate();
		// TimerEnable updates TimerHwTicks
		TimerEnable(&HW_Timers[AUX_TIMER], TRUE);
	}
}


/******************************************************************************

FUNCTION NAME:

ABSTRACT:
DETAILS:

*******************************************************************************/

// This one-shot function adjusts the H/W timer.  It risks that the cyclic timers get more
// jitter than usual.
static UINT32 AuxTimerGetValue (void)
{
    register UINT32 count;

    HW_REG_READ (HW_Timers[AUX_TIMER].pTimerCount, count);
	return count;
}


/******************************************************************************

FUNCTION NAME:
		sysAuxClkInt

ABSTRACT:
		Handles an auxiliary clock interrupt
DETAILS:
		This routine handles an auxiliary clock interrupt.  It acknowledges the
		interrupt and calls the routine installed by sysAuxClkConnect().

*******************************************************************************/

void sysAuxClkInt (int irq, void *dev_id, struct pt_regs * regs)
{
    register UINT32 temp;

    TimerClearIntStatus(&HW_Timers[AUX_TIMER]);

	// Update counters and do callbacks.
	// Only OR the TimerShiftRate to do the update.
	if (vcbUpdateCounter(HW_Timers[AUX_TIMER].TimerHwTicks))
	{
		HW_Timers[AUX_TIMER].TimerShiftRate= TRUE;
	}

	// Shift Interrupt Rate only after the interrupt has occured.
	if (HW_Timers[AUX_TIMER].TimerShiftRate)
	{
		HW_Timers[AUX_TIMER].TimerShiftRate= FALSE;

		// Use j as a temp variable
		temp= vcbMaxRate();

		// Check if there is any enabled callbacks
		if (temp == 0)
		{
			TimerDisable(&HW_Timers[AUX_TIMER], TRUE);
		}
		else
		{
			// Set the H/W timer to the highest rate if different
			if (temp != HW_Timers[AUX_TIMER].TimerTicksPerSecond)
			{
				HW_Timers[AUX_TIMER].TimerTicksPerSecond= temp;
				TimerDisable(&HW_Timers[AUX_TIMER], TRUE);
				// TimerEnable updates TimerHwTicks
				TimerEnable(&HW_Timers[AUX_TIMER], TRUE);
			}
		}
	}
}


/******************************************************************************

FUNCTION NAME:
		sysAuxClkSetOneShotExt

ABSTRACT:

DETAILS:

*******************************************************************************/

STATUS sysAuxClkSetOneShotExt(FUNCPTR routine,int arg,UINT32 us)
{
	register int cbh;
	register int ticksPerSecond;
	register int temp;

	// Divide by zero is not good
	if (us == 0)
	{
		return ERROR;
	}

	// The rest of the code expects ticks per second, do the conversion
	ticksPerSecond= HW_Timers[AUX_TIMER].TimerClkSpeed / us;

	// Check for a valid resolution
	if (ticksPerSecond < AUX_CLK_RATE_MIN || ticksPerSecond > AUX_CLK_RATE_MAX)
	{
	   return ERROR;
	}

	// Return if no space left in the callback table
    cbh= sysAuxClkConnectExt(routine,arg);
	if (cbh == -1)
	{
		return ERROR;
	}

	// If the timer is running, stop the timer and update the counters.
	// Do call backs if necessary.
	if (HW_Timers[AUX_TIMER].TimerRunning)
	{
		// Get the time already past since the last interrupt.
		// TimerDisable may wipe out current timer count.
		temp= HW_Timers[AUX_TIMER].TimerHwTicks - AuxTimerGetValue();

		TimerDisable(&HW_Timers[AUX_TIMER], TRUE);

		// Do not need the updated timershift state info.
		vcbUpdateCounter(temp);
	}

	// Set the one shot flag
	SetOsEnabled(vcbAux[cbh]);

	// Note only the virtual resolution is update.  The H/W resolution is derived.
	vcbAux[cbh].TimerTicksPerSecond = ticksPerSecond;
	vcbClkEnable(cbh);

	return OK;
}


/*******************************************************************************
*
* sysAuxClkConnect - connect a routine to the auxiliary clock interrupt
*
* This routine specifies the interrupt service routine to be called at each
* auxiliary clock interrupt.  It also connects the clock error interrupt
* service routine.
*
* RETURNS: OK, or ERROR if the routine cannot be connected to the interrupt.
*
* SEE ALSO: intConnect(), sysAuxClkEnable()
*/

STATUS sysAuxClkConnect
(
	FUNCPTR routine,    /* routine called at each aux clock interrupt */
	int		arg			/* argument with which to call routine        */
)
{
    register STATUS status;
    register int oldLevel;

    oldLevel = intLock();
	status= vcbClkConnect(routine,arg,0);
    intUnlock(oldLevel);

	return status;
}


/******************************************************************************

FUNCTION NAME:
		sysAuxClkConnectExt

ABSTRACT:
		Extended sysAuxClkConnect function
DETAILS:

*******************************************************************************/

int sysAuxClkConnectExt
(
	FUNCPTR routine,    /* routine called at each aux clock interrupt */
	int		arg			/* argument with which to call routine        */
)
{
    register int oldLevel;
    register int j;

    oldLevel = intLock();
	// Get the index number associated with the callback address
    j= vcbFind(routine,arg);
	if (j == -1)
	{
		// Find an empty
		j= vcbFind(0,0);
		if (j != -1)
		{
			if (vcbClkConnect(routine,arg,j) != OK)
			{
				j= -1;
			}
		}
	}
    intUnlock(oldLevel);

	return j;
}


/******************************************************************************

FUNCTION NAME:
		sysAuxClkDisconnectExt

ABSTRACT:
		Extended sysAuxClkDisconnect function
DETAILS:
		Disconnect a callback function (except the legacy callback)

*******************************************************************************/

STATUS sysAuxClkDisconnectExt (int cbh)
{
    register int oldLevel;

    oldLevel = intLock();

	// Clear the callback bit and the one shot bit
	ClearCbEnabled(vcbAux[cbh]);
	ClearOsEnabled(vcbAux[cbh]);

	// Clear the callback.  There is now room for another callback. Must be both vars.
    vcbAux[cbh].pTimerRoutine = 0;
	vcbAux[cbh].TimerArg= 0;

    intUnlock(oldLevel);

	return OK;
}


/******************************************************************************

FUNCTION NAME:
		sysAuxClkDisable and sysAuxClkDisableExt

ABSTRACT:
		Turn off auxiliary clock interrupts
DETAILS:
		sysAuxClkDisable is for legacy calls.  sysAuxClkDisableExt is the
		preferred embodiment today.

*******************************************************************************/

void sysAuxClkDisable (void)
{
    register int oldLevel;

    oldLevel = intLock();
	vcbClkDisable(0);
    intUnlock(oldLevel);
}

void sysAuxClkDisableExt (int cbh)
{
    register int oldLevel;

    oldLevel = intLock();
	vcbClkDisable(cbh);
    intUnlock(oldLevel);
}


/******************************************************************************

FUNCTION NAME:
		sysAuxClkEnable and sysAuxClkEnableExt

ABSTRACT:
		Turn on auxiliary clock interrupts
DETAILS:
		sysAuxClkEnable is for legacy calls.  sysAuxClkEnableExt is the
		preferred embodiment today.

*******************************************************************************/

void sysAuxClkEnable (void)
{
    register int oldLevel;

    oldLevel = intLock();
	vcbClkEnable(0);
    intUnlock(oldLevel);
}

void sysAuxClkEnableExt (int cbh)
{
    register int oldLevel;

    oldLevel = intLock();
	vcbClkEnable(cbh);
    intUnlock(oldLevel);
}


/******************************************************************************

FUNCTION NAME:
		sysAuxClkRateGet and sysAuxClkRateGetExt

ABSTRACT:
		Get the auxiliary clock rate
DETAILS:
		This routine returns the interrupt rate of the auxiliary clock or
		the number of ticks per second of the auxiliary clock.

		sysAuxClkRateGet is for legacy calls.  sysAuxClkRateGetExt is the
		preferred embodiment today.

*******************************************************************************/

int sysAuxClkRateGet (void)
{
	// Note this function is not using the actual H/W time since another callback may have the timer running faster
    return (vcbAux[0].TimerTicksPerSecond);
}

int sysAuxClkRateGetExt (int cbh)
{
    return (vcbAux[cbh].TimerTicksPerSecond);
}


/******************************************************************************

FUNCTION NAME:
		sysAuxClkRateGet and sysAuxClkRateGetExt

ABSTRACT:
		Set the auxiliary clock rate
DETAILS:
		This routine sets the interrupt rate of the auxiliary clock.  If the
		auxiliary clock is currently enabled, the clock is disabled and then
		re-enabled with the new rate.

		sysAuxClkRateSet is for legacy calls.  sysAuxClkRateSetExt is the
		preferred embodiment today.

*******************************************************************************/

STATUS sysAuxClkRateSet
(
	int ticksPerSecond	    /* number of clock interrupts per second */
)
{
    register int oldLevel;
	
	if (ticksPerSecond < AUX_CLK_RATE_MIN || ticksPerSecond > AUX_CLK_RATE_MAX)
	{
	   return ERROR;
	}

	oldLevel = intLock();
	// Note only the virtual resolution is update.  The H/W resolution is derived.
	vcbAux[0].TimerTicksPerSecond = ticksPerSecond;
#if 0
	vcbClkEnable(0);
#endif
    intUnlock(oldLevel);

	return OK;
}

STATUS sysAuxClkRateSetExt
(
	int	cbh,
	int ticksPerSecond	    /* number of clock interrupts per second */
)
{
    register int oldLevel;

	if (ticksPerSecond < AUX_CLK_RATE_MIN || ticksPerSecond > AUX_CLK_RATE_MAX)
	{
	   return ERROR;
	}

	oldLevel = intLock();
	// Note only the virtual resolution is update.  The H/W resolution is derived.
	vcbAux[cbh].TimerTicksPerSecond = ticksPerSecond;
	vcbClkEnable(cbh);
    intUnlock(oldLevel);

	return OK;
}

#endif // INCLUDE_AUX_CLK


/******************************************************************************

FUNCTION NAME:
		CnxtTimerInit
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void CnxtTimerInit(void)
{
	register UINT32 i;

	/* For now assume that all timers run at the same clock speed */
	/* HW_CLK_SPEED. */
	for (i = 0; i < MAX_NUM_HW_TIMERS; i++)
	{
	#ifdef DEVICE_YELLOWSTONE
		HW_Timers[i].pTimerIntEnable     = (volatile UINT32 *) (HW_TIMER_INT_ENABLE);
		HW_Timers[i].pTimerIntAssignPolled = (volatile UINT32 *) (HW_TIMER_INT_ASSIGN_POLLED);
		HW_Timers[i].pTimerIntStatPolled = (volatile UINT32 *) (HW_TIMER_INT_STAT_POLLED);
		HW_Timers[i].pTimerIntStat       = (volatile UINT32 *) (HW_TIMER_INT_STAT);
		HW_Timers[i].pTimerIntMask       = (volatile UINT32 *) (HW_TIMER_INT_MASK);
		HW_Timers[i].pTimerIntStatMask   = (volatile UINT32 *) (HW_TIMER_INT_STAT_MASKED);
		HW_Timers[i].pTimerIntAssignA    = (volatile UINT32 *) (HW_TIMER_INT_ASSIGN_A);
		HW_Timers[i].pTimerIntAssignB    = (volatile UINT32 *) (HW_TIMER_INT_ASSIGN_B);
		HW_Timers[i].pTimerIntStatMaskA  = (volatile UINT32 *) (HW_TIMER_INT_STAT_MASKED_A);
		HW_Timers[i].pTimerIntStatMaskB  = (volatile UINT32 *) (HW_TIMER_INT_STAT_MASKED_A); 
		HW_Timers[i].pTimerCSR           = (volatile UINT32 *) (HW_TIMER_CSR);
		HW_Timers[i].pTimerPrescale      = (volatile UINT32 *) (HW_TIMER_PRESCALE_BASE + (i * HW_TIMER_REG_OFFSET));

		// Reset and disable timer
		HW_REG_WRITE (HW_Timers[i].pTimerCSR, 0x0);
		// Disable timer interrupt
		HW_REG_WRITE (HW_Timers[i].pTimerIntEnable, 0x0);
		HW_REG_WRITE (HW_Timers[i].pTimerIntMask, 0xFFFFFFFF);
		// Clear timer status
		HW_REG_WRITE (HW_Timers[i].pTimerIntStat, 0xFFFFFFFF);
	#endif

		HW_Timers[i].pTimerCount	= (volatile UINT32 *) (HW_TIMER_COUNT_BASE + (i * HW_TIMER_REG_OFFSET));
		HW_Timers[i].pTimerLimit	= (volatile UINT32 *) (HW_TIMER_LIMIT_BASE + (i * HW_TIMER_REG_OFFSET));
		HW_Timers[i].TimerNum		= i;

	#ifdef DEVICE_YELLOWSTONE
		HW_Timers[i].TimerIntSource      = INT_SOURCE_TIMER_INTA;
		HW_Timers[i].TimerIntLevel       = INT_LVL_TIMER_INTA;
	#else
		HW_Timers[i].TimerIntSource      = HW_Timers[i].TimerNum;
		HW_Timers[i].TimerIntLevel       = i;
	#endif

		HW_Timers[i].TimerOpMode	      = TIMER_PERIODIC;
		HW_Timers[i].TimerResolution     = 0;

		HW_Timers[i].pTimerRoutine       = NULL;
		HW_Timers[i].TimerArg            = 0;

		HW_Timers[i].TimerClkSpeed       = HW_CLK_SPEED;
		HW_Timers[i].TimerRunning        = FALSE;
		HW_Timers[i].TimerConnected      = FALSE;
		HW_Timers[i].TimerTicksPerSecond = 0;
		HW_Timers[i].TimerHwTicks        = 0;
		HW_Timers[i].TimerShiftRate      = FALSE;
		HW_Timers[i].TimerOsIntConnected = FALSE;
		
	}

	#if INCLUDE_AUX_CLK
	// Do this here because this function is not part of the timer API
	memzero( (void *)vcbAux, sizeof(vcbAux) );
	#endif
}


/******************************************************************************

FUNCTION NAME:
		TimerConfigure
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void TimerConfigure(eTimerType TimerNum, HW_TIMER TimerConfig)
{
      HW_Timers[TimerNum] = TimerConfig;
}


/******************************************************************************

FUNCTION NAME:
		TimerSetCallBack
ABSTRACT:
		
DETAILS:

*******************************************************************************/

STATUS TimerSetCallBack
(
	FUNCPTR 	routine,	/* Routine to be called at each clock interrupt */
	int 		arg,		/* Argument with which to call routine */
	eTimerType	TimerNum	/* Which timer */
)
{
   HW_Timers[TimerNum].pTimerRoutine = NULL;
   HW_Timers[TimerNum].TimerArg      = arg;

   #if ((CPU == ARM7TDMI_T) || (CPU == ARMARCH4_T))
   /* set b0 so that sysClkConnect() can be used from shell */
   HW_Timers[TimerNum].pTimerRoutine = (FUNCPTR)((UINT32)routine | 1);
   #else
   HW_Timers[TimerNum].pTimerRoutine = routine;
   #endif (CPU == ARM7TDMI_T)

   HW_Timers[TimerNum].TimerConnected = TRUE;
   return OK;
}


/******************************************************************************

FUNCTION NAME:
		TimerDisable
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void TimerDisable (PHW_TIMER Timer, UINT32 bIntEnable)
{
	#ifdef DEVICE_YELLOWSTONE
	UINT32 Value = 0;
	#endif

	if (Timer->TimerRunning == TRUE)
	{
		if (bIntEnable == FALSE)
		{
			TimerIntCtrl(Timer, bIntEnable);
		}

		// Halt Timer 
		#ifdef DEVICE_YELLOWSTONE
		HW_REG_READ (Timer->pTimerCSR, Value);
		Value &= ( ~(1 << Timer->TimerNum + GPT_ENA_BIT_OFFSET) );
		HW_REG_WRITE (Timer->pTimerCSR, Value);
		#endif

		/* Disable timer by writing to 0 to timer Limit and Count registers */
		HW_REG_WRITE(Timer->pTimerLimit, 0);
		HW_REG_WRITE(Timer->pTimerCount, 0);

		TimerClearIntStatus(&HW_Timers[Timer->TimerNum]);
		Timer->TimerRunning = FALSE;
	}
}


/******************************************************************************

FUNCTION NAME:
		TimerEnable
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void TimerEnable (PHW_TIMER Timer, UINT32 bIntEnable)
{
	/* Set up in periodic mode, divide clock input by 16, enable timer
	 * Calculate the timer value
	 * Note that it may take some ticks to reload the counter
	 * so counter value = (clock rate / sysClkTicksPerSecond) - num_ticks
	 */

   // Take timer out of reset
#ifdef DEVICE_YELLOWSTONE
   UINT32 Value = 0;

   HW_REG_READ (Timer->pTimerCSR, Value);
   Value |= 0x01;
   HW_REG_WRITE (Timer->pTimerCSR, Value);
#endif

   TimerSetResolution(Timer);
   TimerSetPeriod(Timer);

   Timer->TimerRunning = TRUE;

   // Set up timer operation mode and enable timer
#ifdef DEVICE_YELLOWSTONE
   Value |= ( (1 << Timer->TimerNum + GPT_ENA_BIT_OFFSET) | (Timer->TimerOpMode << (Timer->TimerNum + GPT_MODE_BIT_OFFSET)) );
   HW_REG_WRITE (Timer->pTimerCSR, Value);
#endif

   if (bIntEnable == TRUE)
      TimerIntCtrl(Timer, bIntEnable);

}


/******************************************************************************

FUNCTION NAME:
		TimerSetResolution
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void TimerSetResolution(PHW_TIMER Timer)
{
#ifdef DEVICE_YELLOWSTONE
   UINT32 Value = 0;
#endif
	// Equation for YellowStone GPT Prescale register is
	// Timer Resolution  = period (clk_HCLK) * GPTn_PreScale
	// (1/CLK_xSEC_RATE) = (1/HW_CLK_SPEED)  * GPTn_PreScale

	if (Timer->TimerTicksPerSecond > CLK_uSEC_RATE)
	{
		Timer->TimerResolution = CLK_uSEC_RATE;
	}
	else if (Timer->TimerTicksPerSecond > CLK_SEC_RATE)
	{
		Timer->TimerResolution = CLK_mSEC_RATE;
	}
    else
	{
		Timer->TimerResolution = CLK_SEC_RATE;
	}

   #ifdef DEVICE_YELLOWSTONE
   //*(Timer->pTimerPrescale) = (Timer->TimerClkSpeed / Timer->TimerResolution);
   Value = (Timer->TimerClkSpeed / Timer->TimerResolution);
   while (Value > GPT_PRESCALE_MAX_VALUE)
   {
      Timer->TimerResolution = (Timer->TimerResolution * 10);
      Value = (Timer->TimerClkSpeed / Timer->TimerResolution);
   }

   HW_REG_WRITE(Timer->pTimerPrescale, Value);
   
   //*(Timer->pTimerPrescale) = 0x63;
   #endif
}


/******************************************************************************

FUNCTION NAME:
		TimerSetPeriod
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void TimerSetPeriod(PHW_TIMER Timer)
{
   #ifdef DEVICE_YELLOWSTONE
   Timer->TimerHwTicks = Timer->TimerResolution / Timer->TimerTicksPerSecond;
   HW_REG_WRITE(Timer->pTimerLimit, Timer->TimerHwTicks);
   //HW_REG_WRITE(Timer->pTimerLimit, 0x62A4 );
   
   #else
   // Compute number of microseconds per interrupt
   Timer->TimerHwTicks = Timer->TimerClkSpeed / Timer->TimerTicksPerSecond;
   HW_REG_WRITE(Timer->pTimerLimit, Timer->TimerHwTicks);
   #endif
}


/******************************************************************************

FUNCTION NAME:
		TimerIntCtrl
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void TimerIntCtrl(PHW_TIMER Timer, UINT32 bIntEnable)
{
#ifdef DEVICE_YELLOWSTONE
	UINT32 TempValue  = 0;
#endif
	if (bIntEnable == TRUE)
	{
	#ifdef DEVICE_YELLOWSTONE
		// Disable corresponding polling status mode
		HW_REG_READ (Timer->pTimerIntAssignPolled, TempValue);
		TempValue &= (~(1<<Timer->TimerNum));
		HW_REG_WRITE (Timer->pTimerIntAssignPolled, TempValue);
		// Clear polling status bit
		TempValue = (1<<Timer->TimerNum);
		HW_REG_WRITE (Timer->pTimerIntStatPolled, TempValue);

		// Clear interrupt status bit
		TempValue = (1<<Timer->TimerNum);
		HW_REG_WRITE (Timer->pTimerIntStat, TempValue);
		
		// Assigns timer interrupt to corresponding group A or B
		if(Timer->TimerIntSource == INT_SOURCE_TIMER_INTA)
		{
			// Disable corresponding bit in group B
			HW_REG_READ (Timer->pTimerIntAssignB, TempValue);
			TempValue &= (~(1<<Timer->TimerNum));
			HW_REG_WRITE (Timer->pTimerIntAssignB, TempValue);

			// Enable corresponding bit in group A
			HW_REG_READ (Timer->pTimerIntAssignA, TempValue);
			TempValue |= (1<<Timer->TimerNum);
			HW_REG_WRITE (Timer->pTimerIntAssignA, TempValue);
		}
		else if(Timer->TimerIntSource == INT_SOURCE_TIMER_INTB)
		{
			// Disable corresponding bit in group A
			HW_REG_READ (Timer->pTimerIntAssignA, TempValue);
			TempValue &= (~(1<<Timer->TimerNum));
			HW_REG_WRITE (Timer->pTimerIntAssignA, TempValue);

			// Enable corresponding bit in group B
			HW_REG_READ (Timer->pTimerIntAssignB, TempValue);
			TempValue |= (1<<Timer->TimerNum);
			HW_REG_WRITE (Timer->pTimerIntAssignB, TempValue);
		}

		// Enble Timer Int Enable bit by writing 1
		HW_REG_READ (Timer->pTimerIntEnable, TempValue);
		TempValue |= (1<<Timer->TimerNum);
		HW_REG_WRITE (Timer->pTimerIntEnable, TempValue);

		// Enable Timer Int Mask by writing 0
		HW_REG_READ (Timer->pTimerIntMask, TempValue);
		TempValue &= (~(1<<Timer->TimerNum));
		HW_REG_WRITE (Timer->pTimerIntMask, TempValue);

		// Update INT source to INT level mapping table
		if (FALSE == PICSetIntLevel(Timer->TimerIntSource, Timer->TimerIntLevel))
		{
			return;
		}
	#endif

		if( Timer->TimerOsIntConnected == FALSE )
		{
			if( request_irq( Timer->TimerIntLevel, TimerHandler_Tbl[Timer->TimerNum], 0, "timer", 0 ))
			{
				// What do we do if the interrupt does not connect
				return;
			}
			Timer->TimerOsIntConnected = TRUE;
		} 
		cnxt_unmask_irq ( Timer->TimerIntLevel );

	}
	else
	{
	#ifdef DEVICE_YELLOWSTONE
		// Disable Timer Int Enable bit by writing 0
		HW_REG_READ (Timer->pTimerIntEnable, TempValue);
		TempValue &= (~(1<<Timer->TimerNum));
		HW_REG_WRITE (Timer->pTimerIntEnable, TempValue);

		// Disable Timer Int Mask by writing 1
		HW_REG_READ (Timer->pTimerIntMask, TempValue);
		TempValue |= (1<<Timer->TimerNum);
		HW_REG_WRITE (Timer->pTimerIntMask, TempValue);
	#endif

		/* Disable System timer interrupt in the Interrupt Controller */
		cnxt_mask_irq (Timer->TimerIntLevel);
		TimerClearIntStatus(&HW_Timers[Timer->TimerNum]);
	}
}


/******************************************************************************

FUNCTION NAME:
		Timer3IntHandler
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void Timer3IntHandler (int irq, void *dev_id, struct pt_regs * regs)
{
    register int oldLevel;

	/* Acknowledge interrupt: any write to Clear Register clears interrupt */
	oldLevel = intLock();

	TimerClearIntStatus( &HW_Timers[TIMER3] );

	/* If any routine attached via sysClkConnect(), call it */
	if (HW_Timers[TIMER3].pTimerRoutine != NULL)
	{
		(*HW_Timers[TIMER3].pTimerRoutine)(HW_Timers[TIMER3].TimerArg);
	}
	intUnlock(oldLevel);
}


/******************************************************************************

FUNCTION NAME:
		TimerClearIntStatus
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void TimerClearIntStatus(PHW_TIMER Timer)
{
    #ifdef DEVICE_YELLOWSTONE
    HW_REG_WRITE (Timer->pTimerIntStat, (1 << Timer->TimerNum));

    #else
    PICClearIntStatus(Timer->TimerIntSource);
    #endif
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////


#ifdef	INCLUDE_TIMESTAMP

/*******************************************************************************
*
* sysTimestampConnect - connect a user routine to a timestamp timer interrupt
*
* This routine specifies the user interrupt routine to be called at each
* timestamp timer interrupt.
*
* RETURNS: ERROR, always.
*/

STATUS sysTimestampConnect
    (
    FUNCPTR routine,    /* routine called at each timestamp timer interrupt */
    int arg             /* argument with which to call routine */
    )
    {
    return ERROR;
    }

/*******************************************************************************
*
* sysTimestampEnable - enable a timestamp timer interrupt
*
* This routine enables timestamp timer interrupts and resets the counter.
*
* RETURNS: OK, always.
*
* SEE ALSO: sysTimestampDisable()
*/

STATUS sysTimestampEnable (void)
   {
   if (sysTimestampRunning)
      {
      return OK;
      }

   if (!sysClkRunning)          /* timestamp timer is derived from the sysClk */
      return ERROR;

   sysTimestampRunning = TRUE;

   return OK;
   }

/*******************************************************************************
*
* sysTimestampDisable - disable a timestamp timer interrupt
*
* This routine disables timestamp timer interrupts.
*
* RETURNS: OK, always.
*
* SEE ALSO: sysTimestampEnable()
*/

STATUS sysTimestampDisable (void)
    {
    if (sysTimestampRunning)
        sysTimestampRunning = FALSE;

    return OK;
    }

/*******************************************************************************
*
* sysTimestampPeriod - get the period of a timestamp timer
*
* This routine gets the period of the timestamp timer, in ticks.  The
* period, or terminal count, is the number of ticks to which the timestamp
* timer counts before rolling over and restarting the counting process.
*
* RETURNS: The period of the timestamp timer in counter ticks.
*/

UINT32 sysTimestampPeriod (void)
    {
    /*
     * The period of the timestamp depends on the clock rate of the system
     * clock.
     */

    return ((UINT32)(SYS_TIMER_CLK / sysClkTicksPerSecond));
    }

/*******************************************************************************
*
* sysTimestampFreq - get a timestamp timer clock frequency
*
* This routine gets the frequency of the timer clock, in ticks per
* second.  The rate of the timestamp timer is set explicitly by the
* hardware and typically cannot be altered.
*
* NOTE: Because the system clock serves as the timestamp timer,
* the system clock frequency is also the timestamp timer frequency.
*
* RETURNS: The timestamp timer clock frequency, in ticks per second.
*/

UINT32 sysTimestampFreq (void)
    {
    return ((UINT32)SYS_TIMER_CLK);
    }

/*******************************************************************************
*
* sysTimestamp - get a timestamp timer tick count
*
* This routine returns the current value of the timestamp timer tick counter.
* The tick count can be converted to seconds by dividing it by the return of
* sysTimestampFreq().
*
* This routine should be called with interrupts locked.  If interrupts are
* not locked, sysTimestampLock() should be used instead.
*
* RETURNS: The current timestamp timer tick count.
*
* SEE ALSO: sysTimestampFreq(), sysTimestampLock()
*/

UINT32 sysTimestamp (void)
    {
    register UINT32 t;

    CNXT_TIMER_READ (SYS_TIMER_VALUE (CNXT_TIMER_BASE), t);

#if defined (CNXT_TIMER_VALUE_MASK)
    t &= CNXT_TIMER_VALUE_MASK;
#endif

    return (sysTimestampPeriod() - t);
    }

/*******************************************************************************
*
* sysTimestampLock - lock interrupts and get the timestamp timer tick count
*
* This routine locks interrupts when the tick counter must be stopped
* in order to read it or when two independent counters must be read.
* It then returns the current value of the timestamp timer tick
* counter.
*
* The tick count can be converted to seconds by dividing it by the return of
* sysTimestampFreq().
*
* If interrupts are already locked, sysTimestamp() should be
* used instead.
*
* RETURNS: The current timestamp timer tick count.
*
* SEE ALSO: sysTimestampFreq(), sysTimestamp()
*/

UINT32 sysTimestampLock (void)
    {
    register UINT32 t;

    CNXT_TIMER_READ (SYS_TIMER_VALUE (CNXT_TIMER_BASE), t);

#if defined (CNXT_TIMER_VALUE_MASK)
    t &= CNXT_TIMER_VALUE_MASK;
#endif

    return (sysTimestampPeriod() - t);
    }

#endif  /* INCLUDE_TIMESTAMP */

EXPORT_SYMBOL(sysTimerDelay);
EXPORT_SYMBOL(gp_timer_connect);
EXPORT_SYMBOL(gp_timer_init);
EXPORT_SYMBOL(TaskDelayMsec);
EXPORT_SYMBOL(_1MS_timebase_start);
EXPORT_SYMBOL(millisecond_ticks);
EXPORT_SYMBOL(sysAuxClkDisable);
EXPORT_SYMBOL(sysAuxClkEnable);
EXPORT_SYMBOL(sysAuxClkConnect);
EXPORT_SYMBOL(sysAuxClkRateSet);
EXPORT_SYMBOL(sysClkRateGet);
EXPORT_SYMBOL(TimerSetCallBack);
EXPORT_SYMBOL(tickGet);
