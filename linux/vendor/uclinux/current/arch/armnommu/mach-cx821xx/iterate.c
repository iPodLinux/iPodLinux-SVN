/****************************************************************************
*
*	Name:			iterate.c
*
*	Description:	Runs iterations through a timeing loop for CPU utilization
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
*  $Modtime: 8/13/02 7:58a $
****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <timex.h>		// CLOCK_TICK_RATE
#include <asm/param.h>	// HZ
#include <asm/system.h>	// save_flags
#include <sched.h>		// jiffies
#include "Common.h"		// BOOLEAN

volatile unsigned long cnxt_gettimeoffset(void);

/****************************************************************************
	Function		: Iterate
	Description		: Performs as many iterations as possible in the given time.
	Abstract:		: This routine is used to measure how busy the CPU is.
					: It runs for a fixed period (measured by looking at the
					: timer register and/or jiffies count) and sees how many
					: iterrations of looping, incrementing a counter and watching
					: for clock edges it can get done in the fixed period of time.
					: Thus the returned count is a function of how much it is
					: preempted and the number of CPU cycles to perform one
					: iteration. This latter factor is a constant and basically
					: arbitrary so only use the returned numbers as ratios since
					: the number is not a MIP or any other standard measure and
					: could change in the future.
					: Use this routine by calling once with DisableInts set to get a
					: maximum. Then call with with DisableInts cleared to see
					: how many iterations are possible when preemption occurs.
					: Then divide the latter number by the former to get a ratio
					: of CPU loading.
					: Call both times using the same setting for DisableCache
					: for the ratio to be meaningful.
					: The ARM940T has 4KB Data and 4KB Instruction cache. Depending
					: on the task size and behavior under consideration for 
					: insertion into the spare cycles, you may wish to take your
					: two measurements with or without cacheing.
	Details			: This routine is required to detect clock timer rollover
					: (edges) with interrupts off or on. Furthermore the paths
					: for each case must be the same since this detection is a
					: considerable part of the busyness of the measurement.
					: With interrupts enabled, we may be preempted and so we
					: cannot guarantee that we will see every clock increment.
					: Without seeing every clock increment, we are forced to
					: alternate between looking for the rollover and the halfway
					: point. Of course we cannot be guaranteed of seeing either of
					: these with interrupts enabled, so we watch the jiffies count
					: which is incremented by the interrupt. With interrupts off,
					: it is not incremented, but we will see every clock tick. 
					: This does result in a slightly different path when the interrupt
					: enabled version misses an edge, it will instead see a jiffie
					: change which the disabled version never takes. But this slightly
					: longer path can will not occur often and can be attributed to
					: the overhead of preemption.
	Notes:			: The time period should not exceed approximately 2 seconds
					: or the busyness counter may overflow. This time period can
					: be extended by artificially extending the cycles consumed
					: in each iteration.
					: The time period will be converted to the nearest jiffie.
	Notes:			: We assume cache is normally on
****************************************************************************/
void cache_off ( void ) ;
void cache_on (void ) ;
unsigned long int Iterate ( unsigned short int MSecs, BOOLEAN DisableInts, BOOLEAN DisableCache )
{
	int State  ;						// Tracks which edge (0 or 1/2) we are looking for
	unsigned long int elapsedjiffies ;	// Count of jiffies that have elapsed since we started looping
	unsigned long int lastjiffie ;		// Value of jiffies when we last saw a 0 edige
	unsigned long int jiffieinc ;		// Indicates whether we should increment lastjiffied upon detecting a 0 edige
    unsigned long int busycounter = 0 ;	// Counts number of iterations we manage in the given period
    unsigned long int jiffielimit ;		// Amount of time in jiffies to loop
	unsigned long flags = 0 ;			// Status flags saved when we disable interrupts
    
	//*************************************************************************
	// If we are supposed to disable interrupts or cache?
	//*************************************************************************
	if ( DisableInts )
	{
		// disable interrupts
		save_flags (flags) ;
		cli () ;
	}

	if ( DisableCache )
	{
		cache_off () ;
	}
	
	
	//*************************************************************************
	//* initialize
	//*************************************************************************
	
	// disable interrupts
	{
		unsigned long flags = 0 ;
		
		save_flags (flags) ;
		cli () ;
		
		// wait on an edge
		while ( cnxt_gettimeoffset() != 0 )
		{
			;
		}
		
		// initialize elapsed jiffies
		elapsedjiffies = 0 ;
		
		// initialize lastjiffie
		lastjiffie = jiffies ;
		
		// if interrupts are disabled, don't increment lastjiffie when measuring
		if ( DisableInts )
		{
			jiffieinc = 0 ;
		}
		else
		{
			jiffieinc = 1 ;
		}
		
		// state is initialize to look for half point of cycle since we just saw 0 edge
		State = 1;

		// enable interrupts
		restore_flags (flags) ;
	}
    
    
    
    //*************************************************************************
    //* Measure
    //*************************************************************************
    
    // loop for given period
    jiffielimit = ( MSecs * HZ ) / 1000L ;
    while ( elapsedjiffies <  jiffielimit )
    {
	    // looking for CLOCK_TICK_RATE to 0 edge
	    if ( State == 0 )
	    {
	    	// check for edge before checking for miss since when interrupts are
	    	// enabled, it will always look like we missed 1 when it rolls over.
	    	
	    	// if we passed CLOCK_TICK_RATE to 0 edge
	    	if ( cnxt_gettimeoffset() < (CLOCK_TICK_RATE/2) )
	    	{
	    		// update elapsed counter
	    		elapsedjiffies ++ ;

	    		// update lastjiffie (if interrupts are not disabled) so that we catch next edge
	    		lastjiffie += jiffieinc ;
	    		
	    		// change to state of looking for CLOCK_TICK_RATE/2 edge
	    		State = 1 ;
	    	}

	    	// if we missed this edge (and the following CLOCK_TICK_RATE/2 edge)
	    	else if ( lastjiffie != jiffies )
	    	{
	    		//  elapsed counter
	    		elapsedjiffies ++ ;

	    		// update lastjiffie (if interrupts are not disabled) so that we catch next edge
	    		lastjiffie += jiffieinc ;
	    		
	    		// change to state of looking for CLOCK_TICK_RATE/2 edge
	    		State = 1 ;
	    	}
	    }
	    else
	    // looking for CLOCK_TICK_RATE/2 edge
	    {
	    	// if we passed CLOCK_TICK_RATE/2 edge
	    	if ( cnxt_gettimeoffset() > (CLOCK_TICK_RATE/2) )
	    	{
	    		// change to state of looking for CLOCK_TICK_RATE/0 edge
	    		State = 0 ;
	    	}

	    	// if we missed this edge (and the following CLOCK_TICK_RATE to 0 edge)
	    	else if ( lastjiffie != jiffies )
	    	{
	    		// change to state of looking for CLOCK_TICK_RATE/0 edge
	    		State = 0 ;
	    	}
	    }
    	
    	// bump busyness counter for each pass through loop
    	busycounter++ ;
    }
    
    //*************************************************************************
    // enable interrupts or cache if disabled
	//*************************************************************************
	if ( DisableCache )
	{
		cache_on () ;
	}
	
	
	if ( DisableInts )
	{
		restore_flags (flags) ;
	}
	
	return busycounter ;
}

EXPORT_SYMBOL(Iterate);

/***************************************************************************
 $Log: iterate.c,v $
 Revision 1.1  2003/08/28 11:41:23  davidm

 Add in the Conexant platform files

 Revision 1.1  2003/06/29 14:28:18  gerg
 Import of the Conexant 82100 code (from Conexant).
 Compiles, needs to be tested on real hardware.

 * 
 * 4     8/13/02 9:58a Palazzjd
 * Exported symbol Iterate for when building ADSL driver stack as modules.
 * 
 * 3     6/10/02 1:55p Lewisrc
 * Enable Cache support
 * 
 * 2     6/06/02 10:46a Lewisrc
 * Add include of Common.h so BOOLEAN is defined.
 * 
 * 1     6/05/02 3:05p Lewisrc
 * Added iterate.o
****************************************************************************/
/***** end of file $Workfile: iterate.c $ *****/
