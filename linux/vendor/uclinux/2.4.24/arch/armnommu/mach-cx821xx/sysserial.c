/****************************************************************************
*
*	Name:			sysserial.c
*
*	Description:	CX82100 serial device initialization
*
*	Copyright:		(c) 2001 Conexant Systems Inc.
*
*****************************************************************************/

/*
modification history
--------------------
01a,31aug98,mbp  written.
*/

/*
DESCRIPTION
This file contains the board-specific routines for serial channel
initialization of the CX82100 Development environment.

SEE ALSO:
.I "CX82100 Silicon System Design Engineering Specification"
*/

#include <linux/module.h>
#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)

#include "bsptypes.h"
#include "bspcfg.h"
#include "syslib.h"
#include <asm/io.h>
//#include <asm/arch/cnxthw.h>
#include "cnxtbsp.h"

#include "cnxtirq.h"
#include "gpio.h"
#include <asm/arch/irq.h>

#include "OsTools.h"
#include "cnxtsio.h"

/* device initialization structure */

typedef struct
{
	UINT	vector;
	UINT8	*baseAdrs;
	UINT	regSpace;
	UINT	intLevel;
} CNXT_SPUART_CHAN_PARAS;


/* Local data structures */

static CNXT_SPUART_CHAN_PARAS devParas[] =
{
	{
		INT_VEC_GPIO,
		(UINT8 *)SERIALPORT1,
		UART_REG_ADDR_INTERVAL,
		INT_LVL_GPIO
	}
};
 
CNXT_UART spUartChan[1];


/* forward declarations */

/******************************************************************************
*
* sysSerialHwInit - initialize the BSP serial devices to a quiesent state
*
* This routine initializes the BSP serial device descriptors and puts the
* devices in a quiesent state.  It is called from sysHwInit() with
* interrupts locked.
*
* RETURNS: N/A
*
* SEE ALSO: sysHwInit()
*/

BOOL OursysSerialHwInit (void)
{
	#if DEBUG_CONSOLE_ON_UART_2
		return TRUE;
	#else
		spUartChan[0].regAddr 	= devParas[0].regSpace;
		spUartChan[0].regs 	= devParas[0].baseAdrs;

		/*
		* Initialise UART
		*/

		return ( spUartDevInit ( &spUartChan[0] ) );
	#endif
}

/******************************************************************************
*
* sysSerialHwInit2 - connect BSP serial device interrupts
*
* This routine connects the BSP serial device interrupts.  It is called from
* sysHwInit2().  Serial device interrupts could not be connected in
* sysSerialHwInit() because the kernel memory allocator was not initialized
* at that point, and intConnect() may call malloc(). 
* 
* RETURNS: N/A 
*
* SEE ALSO: sysHwInit2()
*/

void sysSerialHwInit2 (void)
{
   int oldlevel;

	oldlevel = intLock();

	#if DEBUG_CONSOLE_ON_UART_2
		// Do nothing
	#else
		#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE

			#ifdef CONFIG_BD_RUSHMORE
				GPIO_SetGPIOIRQRoutine(GPIOINT_UART2, GPIOB14_Handler);
			#endif

			#ifdef CONFIG_BD_HASBANI
				GPIO_SetGPIOIRQRoutine(GPIOINT_UART2, GPIOB24_Handler);
			#endif

			#ifdef CONFIG_BD_MACKINAC
				GPIO_SetGPIOIRQRoutine(GPIOINT_UART2, GPIOB30_Handler);
			#endif

			SetGPIODir( GPIOINT_UART2, GP_INPUT );
			SetGPIOInputEnable( GPIOINT_UART2, GP_INPUTON );

			SetGPIOIntEnable(GPIOINT_UART2, IRQ_OFF);

			#if 1
				SetGPIOIntPolarity (GPIOINT_UART2, GP_IRQ_POL_POSITIVE);
			#else
				SetGPIOIntMode (GPIOINT_UART2, GP_IRQ_MODE_LEVEL);
				SetGPIOIntPolarity (GPIOINT_UART2, GP_IRQ_POL_NEGATIVE);
			#endif

			SetGPIOIntEnable(GPIOINT_UART2, IRQ_ON);

		#endif
	#endif

	intUnlock(oldlevel);
}

#endif	//if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
