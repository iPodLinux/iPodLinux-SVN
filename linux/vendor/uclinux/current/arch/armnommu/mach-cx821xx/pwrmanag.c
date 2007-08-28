/****************************************************************************
*
*	Name:			pwrmanag.c
*
*	Description:	
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
*  $Modtime: 8/06/02 7:19a $
****************************************************************************/

#include <linux/config.h>
#include <linux/ptrace.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/arch/bsptypes.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/syslib.h>
#include <asm/arch/pwrmanag.h>
#include "plldef.h"

#include <asm/arch/gpio.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/OsTools.h>
#include <asm/arch/cnxttimer.h>
#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
#include <asm/arch/hwio.h>
#include <asm/arch/cardinfo.h>
#endif

void sysDevicePowerCtl(eHWDevice eDeviceType, eHWPowerState ePowerState)
{
   switch (eDeviceType)
   {
      case DEVICE_ARMCORE:
         sysARMCOREPwrCtl(ePowerState);
         break;

      case DEVICE_SDRAM:
         sysSDRAMPwrCtl(ePowerState);
         break;

      #if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
         case DEVICE_FALCON:
            sysFALCONPwrCtl(ePowerState);
            break;

         case DEVICE_AFE:
            sysAFEPwrCtl(ePowerState);
            break;
      #endif //CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE


      default:
         break;
   }
}


void sysARMCOREPwrCtl(eHWPowerState ePowerState)
{
   UINT32 volatile *pCtlRegF = 0;
   UINT32 volatile *pCtlRegB = 0;

   pCtlRegF = (UINT32 volatile *) PLL_F;
   pCtlRegB = (UINT32 volatile *) PLL_B;

   switch (ePowerState)
   {
      case DEVICE_SLOW_SPEED:
         *pCtlRegF |= PLL_CR_SLOW;
         sysTimerDelay(10000);
         *pCtlRegB |= PLL_CR_SLOW;
         sysTimerDelay(10000);
         break;

      case DEVICE_NORMAL_SPEED:
         *pCtlRegF &= (~PLL_CR_SLOW);
         sysTimerDelay(10000);
         *pCtlRegB &= (~PLL_CR_SLOW);
         sysTimerDelay(10000);
         break;

      case DEVICE_OFF:
      case DEVICE_SLEEP:
      case DEVICE_RESET:
      default:
         break;
   }
}


void sysSDRAMPwrCtl(eHWPowerState ePowerState)
{
   UINT32 volatile *pCtlReg = 0;

   pCtlReg = (UINT32 volatile *) EXT_MEM_CTL;

   switch (ePowerState)
   {
      case DEVICE_ON:
         *pCtlReg |= EXT_SDRAM_ENA;
         HWSTATE.bSDRAMState = TRUE;
         sysTimerDelay(10000);
         break;

      case DEVICE_OFF:
         *pCtlReg &= (~EXT_SDRAM_ENA);
         HWSTATE.bSDRAMState = FALSE;
         sysTimerDelay(10000);
         break;

      case DEVICE_SLEEP:
      case DEVICE_RESET:
      default:
         break;
   }
}


#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
void sysFALCONPwrCtl(eHWPowerState ePowerState)
{
	switch (ePowerState)
	{
		case DEVICE_ON:
			/* Enable Falcon Power */
			WriteGPIOData( GPOUT_F2PWRDWN, 0 );
			sysTimerDelay(10000);

			/* Remove Falcon Reset */
			WriteGPIOData( GPOUT_F2RESET, 1 );
			sysTimerDelay(10000);
			break;

		case DEVICE_OFF:
			/* Disable Falcon Power */
			WriteGPIOData( GPOUT_F2PWRDWN, 1 );

			/* Put Falcon in Reset */
			WriteGPIOData( GPOUT_F2RESET, 0 );
			WriteGPIOData( GPOUT_F2_SCANEN, 1 );

			WriteGPIOData( GPOUT_OUTER_PAIR, 0 );
			// Inner pair
			WriteGPIOData( GPOUT_INNER_PAIR, 0 );

			HWSTATE.eADSLLineState = ADSL_DISABLE;
			HWSTATE.eADSLRequestLineState= ADSL_DISABLE; 
			HWSTATE.dwLineStatus= HW_IO_MODEM_STATUS_DOWN;
			break;

		case DEVICE_RESET:
			/* Put Falcon in Reset */
			WriteGPIOData( GPOUT_F2RESET, 0);
			sysTimerDelay(100000);
			/* Take Falcon out of Reset */
			WriteGPIOData( GPOUT_F2RESET, 1 );
			sysTimerDelay(100000);
			break;

		case DEVICE_SLEEP:
			default:
			break;
	}
}

#endif // CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE

#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE

void sysAFEPwrCtl(eHWPowerState ePowerState)
{
   switch (ePowerState)
   {
      case DEVICE_ON:
          /* Power up Line Driver */
         #ifdef DL10_D325_003
            WriteGPIOData( GPOUT_LDRVPWR, 1 );
         #else
            WriteGPIOData( GPOUT_LDRVPWR, 0 );
         #endif
         sysTimerDelay(10000);

         /* Take AFE out of Reset*/
         WriteGPIOData( GPOUT_AFERESET, 1 );
         sysTimerDelay(10000);
         break;

      case DEVICE_OFF:
         /* Power down Line Driver */
         #ifdef DL10_D325_003 
            WriteGPIOData( GPOUT_LDRVPWR, 0 );
         #else
            WriteGPIOData( GPOUT_LDRVPWR, 1 );
         #endif

         /* Place AFE in Reset*/
         WriteGPIOData( GPOUT_AFERESET, 0 );
         break;

      case DEVICE_RESET:
         /* Place AFE in Reset */
         WriteGPIOData( GPOUT_AFERESET, 0 );
         /* Hold reset for some time */
         sysTimerDelay(100000);
         /* Take AFE out of Reset*/
         WriteGPIOData( GPOUT_AFERESET, 1 );
         sysTimerDelay(100000);
         break;

      case DEVICE_SLEEP:
      default:
         break;
   }
}

#endif // CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE


EXPORT_SYMBOL(sysDevicePowerCtl);

