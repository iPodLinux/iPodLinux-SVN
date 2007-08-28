/****************************************************************************
*
*	Name:			   syslib.c
*
*	Description:	ARM system-dependent routines
*
*	Copyright:		(c) 2001 Conexant Systems Inc.
*
*****************************************************************************
*  $Author: davidm $
*  $Revision: 1.1 $
*  $Modtime: 10/07/02 11:35a $
****************************************************************************/


/*
modification history
--------------------
01a,21jul98,gar  created based on ARM PID 01a
*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/io.h>

#include <asm/arch/bspcfg.h>
#include <asm/arch/bsptypes.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/gpio.h>
#include <asm/arch/cnxtirq.h>
#include <asm/arch/hstif.h>
#include <asm/arch/pwrmanag.h>
#include "eeprom.h"
#include <asm/arch/OsTools.h>
#include <asm/arch/cnxttimer.h>

#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
#include <asm/arch/dsl.h>
#include <asm/arch/cardinfo.h>
#include <asm/arch/irq.h>
#include <asm/arch/dslisr.h>
#endif
#include "plldef.h"

extern void DSL_RX_DMA_ISR(int irq, void *dev_id, struct pt_regs * regs);
extern void DSL_ERR_ISR(int irq, void *dev_id, struct pt_regs * regs);

void sysSerialCheck ( void ) ;

/* imports */

int	sysProcNum;    /* processor number of this cpu */


/* locals */

/* PCLK = BCLK/2 */
/* Since timer value is 16-bit we will not attempt to support  */
/* BCLK values of 150Mhz and higher since these values is more */
/* than 16-bit long. */

UINT32 RUSHMORE_PCLKNormalRate_Table[RUSHMORE_MAX_NUM_BLCK] =
   { 12500,   /* BCLK = 25Mhz */
     25000,   /* BCLK = 50Mhz */
     37500,   /* BCLK = 75Mhz */
     50000,   /* BCLK = 100Mhz */
     62500    /* BCLK = 125Mhz */
   };


UINT32 P52_PCLKNormalRate_Table[P52_MAX_NUM_BLCK] =
   { 37500,   /* BCLK = 75Mhz */
     50000,   /* BCLK = 100Mhz */
     62500    /* BCLK = 125Mhz */
   };


UINT32 P52_PCLKSlowRate_Table[P52_MAX_NUM_BLCK] =
   { 12500,
     25000,
     31250
   };



/* globals */


/* forward LOCAL functions declarations */




/*******************************************************************************
*
* sysHwInit - initialize the CPU board hardware
*
* This routine initializes various features of the hardware.
* Normally, it is called from usrInit() in usrConfig.c.
*
* NOTE: This routine should not be called directly by the user.
*
* RETURNS: N/A
*/

void syshwinit (void)
{
	UINT32 *pHostSig = 0;

	// Initialize PLL regs, I and D caches
	sysInit();
	
	/* Get Device Type.  For now return either P52 or Rushmore. */
	HWSTATE.eDeviceType = _sysGetDeviceType();

	/* Initialize the GPIO device */
	GPIO_Init();

	HSTInit();

#if INCLUDE_AUX_CLK
	CnxtTimerInit();
#endif

#ifndef DEVICE_YELLOWSTONE
	#ifdef CONFIG_BD_HASBANI
		#ifdef DL10_D325_003
			// Toggles the reset line for the PHY device
			// NOTE: The DL10-D325-003 version of the Hasbani
			// hardware requires that the reset line of the EMAC
			// device be toggled because there is no longer an RC
			// network on the line to do it for us.
			sysDevicePowerCtl(DEVICE_EPHY, DEVICE_RESET);
		#endif		

		#ifdef POWER_CTRL
			#ifndef DL10_D325_003
				/* Shut off Flash once code is executing from SDRAM to save power */
				sysDevicePowerCtl(DEVICE_FLASH, DEVICE_OFF);
			#endif
			
			#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
				/* Shut off ADSL interfaces */
				sysDevicePowerCtl(DEVICE_AFE, DEVICE_OFF);
				sysDevicePowerCtl(DEVICE_FALCON, DEVICE_OFF);
			#endif
		#endif
	#else
		#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE && defined ( POWER_CTRL )
			/* Shut off ADSL interfaces */
			sysDevicePowerCtl(DEVICE_AFE, DEVICE_OFF);
			sysDevicePowerCtl(DEVICE_FALCON, DEVICE_OFF);
		#endif
	#endif
#endif

	#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
		HWSTATE.eADSLLineState = ADSL_DISABLE;
		HWSTATE.eADSLRequestLineState= ADSL_DISABLE; 
		HWSTATE.dwADSLDownRate = 0;
		HWSTATE.dwADSLUpRate = 0;
		HWSTATE.eADSLLineState = ADSL_DISABLE;
		HWSTATE.bSRAMState = TRUE;
		HWSTATE.bSDRAMState = TRUE;
		HWSTATE.bEEPROMState = FALSE;
		HWSTATE.eUARTState = NO_UART;
		HWSTATE.g_pUARTChan = 0;
		/* Assume self-test passed */
		HWSTATE.bSelfTestState = TRUE;
		HWSTATE.dwMACAddressLow  = 0x0;
		HWSTATE.dwMACAddressHigh = 0x0;
		
		/* initialise serial data structure */
		HWSTATE.eUARTState = OursysSerialHwInit ();

	#else
		HWSTATE.bSRAMState = TRUE;
		HWSTATE.bSDRAMState = TRUE;
		HWSTATE.bEEPROMState = FALSE;
		HWSTATE.eUARTState = NO_UART;
		HWSTATE.g_pUARTChan = 0;
		/* Assume self-test passed */
		HWSTATE.bSelfTestState = TRUE;
		HWSTATE.dwMACAddressLow  = 0x0;
		HWSTATE.dwMACAddressHigh = 0x0;
		
	#endif


	/* Init the device descritor and configuration tables independent of */
	/* whether we will configure USB core or not. */
	InitEEPROMImage();

	pHostSig = (UINT32 *) HOST_SIGNATURE;


   /* Do Serial number check after System clock is running since */
   /* we rely on the timer to randomly generate a serial number  */
   /* as needed. */
	sysSerialCheck();
}


/*******************************************************************************
*
* sysHwInit2 - additional system configuration and initialization
*
* This routine connects system interrupts and does any additional
* configuration necessary.
*
* RETURNS: N/A
*
* NOMANUAL
*
* Note: this is called from sysClkConnect() in the timer driver.
*/

void sysHwInit2 (void)
{

#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
	DmaInit();

	DSL_init();

	/* Connect DSL error interrupt */
	if( request_irq( INT_LVL_ADSL_ERR, &DSL_ERR_ISR, 0, "dsl error isr", 0 ))
	{
	 // What do we do if the interrupt does not connect
	} 
	cnxt_unmask_irq (INT_LVL_ADSL_ERR);

#endif

	// setup overall GPIO interrupt handler
	request_irq( INT_LVL_GPIO, GPIOHandler, 0, "gpio isr", 0 ) ;
	
#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
	/* Setup serial port GPIO interrupt */
	sysSerialHwInit2();
#endif
}

/******************************************************************************

FUNCTION NAME:
		GetRxTimeOutValue

ABSTRACT:
		
DETAILS:

*******************************************************************************/

UINT32 GetRxTimeOutValue(UINT32 usTime)
{
   UINT32 volatile *pPLLreg = 0;
   UINT32 TimerValue = 0;
   UINT32 value = 0;

   pPLLreg = (UINT32 volatile *) PLL_B;
   value = *pPLLreg;

   if (HWSTATE.eDeviceType >= DEVICE_RUSHMORE)
   {
      /* If in slow mode return 0 to turn off both USB and DLS RX */
      /* time out features. */
      if (value & RUSHMORE_PLL_CR_SLOW)
         TimerValue = 0;

      else
      {
         value = (value & RUSHMORE_PLL_CLK_RATE);
         /* If value is too large for timer 16-bit value, we will also  */
         /* return 0 to turn off both USB and DSL RX time out features. */
         if (value >= RUSHMORE_MAX_NUM_BLCK)
            TimerValue = 0;

         else
         {
            TimerValue = (usTime * RUSHMORE_PCLKNormalRate_Table[value])/1000;
            TimerValue = (TimerValue >> 8);
         }
      }
   }

   else
   {
      if (value & P52_PLL_CR_SLOW)
      {
         value = ((value & P52_PLL_CLK_RATE) >> 24);
         TimerValue = (usTime * P52_PCLKSlowRate_Table[value])/1000;
         /* Shift down by 8 to accommodate the mapping of the    */
         /* 16-bit Timer Register into the 24-bit Timer Counter. */
         TimerValue = (TimerValue >> 8);
      }

      else
      {
         /* TimerValue = 0x124 for 75Mhz */
         value = ((value & P52_PLL_CLK_RATE) >> 24);
         TimerValue = (usTime * P52_PCLKNormalRate_Table[value])/1000;
         TimerValue = (TimerValue >> 8);
      }
   }

   return (TimerValue);
}


BOOL SYS_bDUALEMAC_Support( void )
{
	// Basically a stub for now
	return FALSE;
}

