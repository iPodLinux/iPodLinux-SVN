/****************************************************************************
*
*	Name:			gpioisr.c
*
*	Description:	This module contains the GPIO interrupt handling functions.
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
*  $Modtime: 9/20/02 7:18p $
****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <asm/arch/bsptypes.h>
#include <asm/arch/bspcfg.h>
#include <asm/arch/syslib.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/cnxtirq.h>
#include <asm/arch/OsTools.h>
#include <asm/arch/gpio.h>


#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
    void (*pF2_IRQ1_ISR)(void *)= 0;
    void (*pF2_IRQ2_ISR)(void *)= 0;
	void *pFalconContext ;
#endif

/*
** GPIOISRxHandler_Tbl contains hooks for applications that want to
** to do the entire interrupt for the given GPIO.  These routines
** must execute ClearGPIOIntStatus(GpioNumber) at the end.
*/
static void (*GPIOISR1Handler_Tbl[16])(int) = {0};
static void (*GPIOISR2Handler_Tbl[16])(int) = {0};
static void (*GPIOISR3Handler_Tbl[8])(int) = {0};

/*
** GPIO_SetGPIOIRQCallback is the usual callback. Applications
** need not know about GPIOs.
*/
static void ((*(pGPIO_ISR)[NUM_OF_GPIOS])(int)) = {0};

/*
** Set the usual callback.  DMT datapump uses this function.
*/
void GPIO_SetGPIOIRQCallback(int gpio,void (*(pISR))(int))
{
	if (gpio >= 0 && gpio < NUM_OF_GPIOS)
		pGPIO_ISR[gpio]= pISR;
}

/*
** Get the usual callback.  DMT datapump uses this function for
** critical sections.
*/
void *GPIO_GetGPIOIRQCallback(int gpio)
{
	if (gpio >= 0 && gpio < NUM_OF_GPIOS)
		return pGPIO_ISR[gpio];

	return 0;
}

/*
** Set the irq handler.  USB plug/unplug uses this.
*/
void GPIO_SetGPIOIRQRoutine(int gpio,void (*(pISR))(int))
{
	if (gpio < 16)
		GPIOISR1Handler_Tbl[gpio]= pISR;
	else
		if (gpio < 32)
			GPIOISR2Handler_Tbl[gpio-16]= pISR;
		else
			if (gpio < NUM_OF_GPIOS)
				GPIOISR3Handler_Tbl[gpio-32]= pISR;
}

/*
** Get the irq handler.  USB plug/unplug uses this.
*/
void *GPIO_GetGPIOIRQRoutine(int gpio)
{
	if (gpio < 16)
		return GPIOISR1Handler_Tbl[gpio];

	if (gpio < 32)
		return GPIOISR2Handler_Tbl[gpio-16];

	if (gpio < NUM_OF_GPIOS)
		return GPIOISR3Handler_Tbl[gpio-32];

	return 0;
}

/*
** The actual handware interrupt goes here for processing.
*/
void GPIOHandler(int irq, void *dev_id, struct pt_regs * regs)
{

   register UINT32 i;
   register UINT32 dwGPIO1Stat, dwGPIO2Stat, dwGPIO3Stat;
   register UINT32 BitPosition;

   register int oldlevel; 

#ifdef INSTRUMENTATION
   register int oldTskGPIO;
#endif /* INSTRUMENTATION */

   oldlevel = INTLOCK(); 

#ifdef INSTRUMENTATION
   oldTskGPIO= ReadGPIOData(GPOUT_TASKRDY_LED);
   WriteGPIOData(GPOUT_TASKRDY_LED, LED_ON);
#endif /* INSTRUMENTATION */

   while (1)
   {
      /* Each GPIO ISR only contains 16 bits */
      dwGPIO1Stat = (*p_GRP0_INT_STAT & *p_GRP0_INT_MASK) & 0xFFFF;
      dwGPIO2Stat = (*p_GRP1_INT_STAT & *p_GRP1_INT_MASK) & 0xFFFF;
      dwGPIO3Stat = (*p_GRP2_INT_STAT & *p_GRP2_INT_MASK) & 0xFFFF;
      
      if ( (dwGPIO1Stat | dwGPIO2Stat | dwGPIO3Stat) == 0)
         break;

      for (i = 0, BitPosition = 1; dwGPIO1Stat; i++, BitPosition<<=1)
         if (dwGPIO1Stat & BitPosition)
         {
            if (GPIOISR1Handler_Tbl[i] != 0)
   	           (*GPIOISR1Handler_Tbl[i])(i);
            else
			   {
	           if (pGPIO_ISR[i] != 0)
   				  (*(pGPIO_ISR)[i])(i);

               HW_REG_WRITE (p_GRP0_INT_STAT, BitPosition);
			   }

		      dwGPIO1Stat &= ~(BitPosition); 
         }

      for (i = 0, BitPosition = 1; dwGPIO2Stat; i++, BitPosition<<=1)
         if (dwGPIO2Stat & BitPosition)
            {
            if (GPIOISR2Handler_Tbl[i] != 0)
   	           (*GPIOISR2Handler_Tbl[i])(i+16);
            else
			   {
	           if (pGPIO_ISR[i+16] != 0)
   				  (*(pGPIO_ISR)[i+16])(i+16);

               HW_REG_WRITE (p_GRP1_INT_STAT, BitPosition);
			   }

		 dwGPIO2Stat &= ~(BitPosition); 
         }

      for (i = 0, BitPosition = 1; dwGPIO3Stat; i++, BitPosition<<=1)
	     if (dwGPIO3Stat & BitPosition)
            {
            if (GPIOISR3Handler_Tbl[i] != 0)
   	         (*GPIOISR3Handler_Tbl[i])(i+32);
            else
			   {
	           if (pGPIO_ISR[i+32] != 0)
   				  (*(pGPIO_ISR)[i+32])(i+32);

               HW_REG_WRITE (p_GRP2_INT_STAT, BitPosition);
			   }

		 dwGPIO3Stat &= ~(BitPosition); 
         }
   }

#ifdef INSTRUMENTATION
   WriteGPIOData(GPOUT_TASKRDY_LED, oldTskGPIO);
#endif /* INSTRUMENTATION */

   HW_REG_WRITE (PIC_TOP_ISR_IRQ, INT_GPIO);
   INTUNLOCK(oldlevel);
}


void GPIOB10_Handler( int gpio_num )
{
   #if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
      if (pF2_IRQ1_ISR == 0)
      {
         ClearGPIOIntStatus(GPIOINT_F2_IRQ1);
         return;
      }

      /* Clearing level */
      pF2_IRQ1_ISR(pFalconContext);
      ClearGPIOIntStatus(GPIOINT_F2_IRQ1);
   #else
      ClearGPIOIntStatus(GPIO10);
   #endif
}

void GPIOB11_Handler( int gpio_num )
{
   #if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
      if (pF2_IRQ2_ISR == 0)
      {
         ClearGPIOIntStatus(GPIOINT_F2_IRQ2);
         return;
      }

      /* Clearing level */
      pF2_IRQ2_ISR(pFalconContext);
      ClearGPIOIntStatus(GPIOINT_F2_IRQ2);
   #else
      ClearGPIOIntStatus(GPIO10);
   #endif
}


void GPIOB14_Handler( int gpio_num )
{
	#ifdef CONFIG_BD_RUSHMORE
		#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
		/* Clearing level */
		if ( (HWSTATE.eUARTState == HW_UART) && (HWSTATE.g_pUARTChan) )
		{
			// Smart/Dumb Terminal
			spUartInt(HWSTATE.g_pUARTChan);
		}
		ClearGPIOIntStatus(GPIOINT_UART2);   
		#endif
	#else		
		ClearGPIOIntStatus(GPIO14);
	#endif
}
    

void GPIOB24_Handler( int gpio_num )
{
	#ifdef CONFIG_BD_HASBANI
		#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
		/* Clearing level */
		if ( (HWSTATE.eUARTState == HW_UART) && (HWSTATE.g_pUARTChan) )
		{
			// Smart/Dumb Terminal
			spUartInt(HWSTATE.g_pUARTChan);
		}
		ClearGPIOIntStatus(GPIOINT_UART2);   
		#endif
	#else		
		ClearGPIOIntStatus(GPIO24);
	#endif
}
    

void GPIOB25_Handler(int gpio_num)
{
      //Linux Console
   #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	   void rs_interrupt(int irq, void * dev_id, struct pt_regs * regs) ;
	   rs_interrupt( GPIOINT_UART1, NULL, NULL ) ;
   #endif
   ClearGPIOIntStatus(GPIOINT_UART1);  
}

void GPIOB30_Handler( int gpio_num )
{
	#ifdef CONFIG_BD_MACKINAC
		#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
		/* Clearing level */
		if ( (HWSTATE.eUARTState == HW_UART) && (HWSTATE.g_pUARTChan) )
		{
			// Smart/Dumb Terminal
			spUartInt(HWSTATE.g_pUARTChan);
		}
		ClearGPIOIntStatus(GPIOINT_UART2);
		#endif
	#else
		ClearGPIOIntStatus(GPIO30);
	#endif
}

EXPORT_SYMBOL(GPIO_SetGPIOIRQCallback);
