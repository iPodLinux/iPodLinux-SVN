/****************************************************************************
 *
 *  Name:			DSLBASE.C
 *
 *  Description:	Basic DSL routines for DMT pump control
 *
 *  Copyright:		(c) 2001 Conexant Systems Inc.
 *
 ****************************************************************************
 *
 *  $Author: davidm $
 *  $Revision: 1.1 $
 *  $Modtime: 11/06/02 9:31a $
 *
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/init.h>

#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
#include "bsptypes.h"
#include "bspcfg.h"
#include "gpio.h"
#include "dslbase.h"
#include "cnxtbsp.h"
#include "OsTools.h"
#include "cnxttimer.h"
#include "cnxtirq.h"

extern void *pFalconContext ;
extern void (*pF2_IRQ1_ISR)(void *);
extern void (*pF2_IRQ2_ISR)(void *);

/****************************************************************
 ****************************************************************/

void FalconScanEnable(BOOL State)
{
	WriteGPIOData(GPOUT_F2_SCANEN,State);	/* GPIO30(P50) , GPIO26(ZIG)*/
}

/****************************************************************
 ****************************************************************/

/* Showtime in F2 IRQ handlers */
/* GPIO26 for Pyramid, GPIO19 for Ziggy */
void SetGPIOF2PInt(BOOL value)
{
//	WriteGPIOData(GPOUT_TXD_LED,!value);
}

/****************************************************************
 ****************************************************************/

/* DSL Rx */
void SetDslRxDataInd(BOOL value)
{
   #if COMMON_TX_RX_INDICATION
   WriteGPIOData(GPOUT_TXD_LED,!value);
   #else
   WriteGPIOData(GPOUT_RXD_LED,!value);
   #endif
}

/****************************************************************
 ****************************************************************/

/* DSL Tx */
void SetDslTxDataInd(BOOL value)
{
   WriteGPIOData(GPOUT_TXD_LED,!value);
}

/****************************************************************
 ****************************************************************/

/* Showtime in critical section */
/* GPIO27 for Pyramid, GPIO20 for Ziggy */
void SetGPIOF2PCS(BOOL value)
{
//	WriteGPIOData(GPOUT_RXD_LED,!value);
}

/****************************************************************
 ****************************************************************/

void LinedriverPower(BOOL State)
{
	WriteGPIOData(GPOUT_LDRVPWR,State);	/* GPIO13 */	
}

/****************************************************************
 ****************************************************************/

void AFEReset(BOOL State)
{
	WriteGPIOData(GPOUT_AFERESET,!State);	/* GPIO8 */	
}

/****************************************************************
 ****************************************************************/

void FalconReset(BOOL State)
{
	WriteGPIOData(GPOUT_F2RESET,State);	/* GPIO6(P46), GPIO9(P50) */	
}

/****************************************************************
 ****************************************************************/

void FalconPowerDown(BOOL State)
{
	WriteGPIOData(GPOUT_F2PWRDWN, State);		/* GPIO0(P46), GPIO12(P50) */
}

/****************************************************************
 ****************************************************************/

/* Showtime code registers callbacks */
void SetF2Interrupts(void (*(pIRQ1))(void *),void (*(pIRQ2))(void *), void *pContext)
{
	UINT32 OldLevel = 0;

	OldLevel = intLock();

	pFalconContext = pContext ;
	if (pIRQ1)
	{
		pF2_IRQ1_ISR= pIRQ1;

		GPIO_SetGPIOIRQRoutine(GPIOINT_F2_IRQ1, GPIOB10_Handler);
		SetGPIOInputEnable( GPIOINT_F2_IRQ1, GP_INPUTON );

		SetGPIOIntEnable(GPIOINT_F2_IRQ1, IRQ_OFF);
#ifdef LEVEL_MODE_ADSL
		SetGPIOIntMode (GPIOINT_F2_IRQ1, GP_IRQ_MODE_LEVEL);
#endif
		SetGPIOIntPolarity (GPIOINT_F2_IRQ1, GP_IRQ_POL_NEGATIVE);
		SetGPIOIntEnable(GPIOINT_F2_IRQ1, IRQ_ON);
	}
	else
	{
		SetGPIOIntEnable(GPIOINT_F2_IRQ1, IRQ_OFF);
		ClearGPIOIntStatus(GPIOINT_F2_IRQ1);
		pF2_IRQ1_ISR= pIRQ1;
	}

	if (pIRQ2)
	{
		pF2_IRQ2_ISR= pIRQ2;

		GPIO_SetGPIOIRQRoutine(GPIOINT_F2_IRQ2, GPIOB11_Handler);
		SetGPIOInputEnable( GPIOINT_F2_IRQ2, GP_INPUTON );

		SetGPIOIntEnable(GPIOINT_F2_IRQ2, IRQ_OFF);
#ifdef LEVEL_MODE_ADSL
		SetGPIOIntMode (GPIOINT_F2_IRQ2, GP_IRQ_MODE_LEVEL);
#endif
		SetGPIOIntPolarity (GPIOINT_F2_IRQ2, GP_IRQ_POL_NEGATIVE);
		SetGPIOIntEnable(GPIOINT_F2_IRQ2, IRQ_ON);
	}
	else
	{
		SetGPIOIntEnable(GPIOINT_F2_IRQ2, IRQ_OFF);
		ClearGPIOIntStatus(GPIOINT_F2_IRQ2);
		pF2_IRQ2_ISR= pIRQ2;
	}

	intUnlock(OldLevel);

}

/****************************************************************
 ****************************************************************/

#ifndef LEVEL_MODE_ADSL
static int oldValue[32];
#endif

static int DisableInterruptCount= 0;

/****************************************************************
 ****************************************************************/

void ADSL_CS_Init(void)
{
	DisableInterruptCount = 0;
	pF2_IRQ1_ISR= pF2_IRQ2_ISR= 0;
}

/****************************************************************
 ****************************************************************/

void ADSL_EnableIRQ1_2(void)
{
	register UINT32 temp;

	if (pF2_IRQ1_ISR == 0 && pF2_IRQ2_ISR == 0)
		return;

	temp= intLock();

#ifdef LEVEL_MODE_ADSL

	if (DisableInterruptCount)
		DisableInterruptCount--;
	if (DisableInterruptCount == 0)
	{
		/* Indicate out of critical section for debug */
		SetGPIOF2PCS(FALSE);

		if (pF2_IRQ1_ISR)
			SetGPIOIntEnable(GPIOINT_F2_IRQ1, 1);
		if (pF2_IRQ2_ISR)
			SetGPIOIntEnable(GPIOINT_F2_IRQ2, 1);
	}

	intUnlock(temp);

#else /* LEVEL_MODE_ADSL */

	if (DisableInterruptCount == 0)
	{
		intUnlock(temp);
		return;
	}

	DisableInterruptCount--;
	if (DisableInterruptCount == 0)
		/* Indicate out of critical section for debug */
		SetGPIOF2PCS(FALSE);

	/* Allow for nesting, push values onto a stack */
	intUnlock(temp);
	intUnlock(oldValue[DisableInterruptCount]);

#endif /* LEVEL_MODE_ADSL */
}

/****************************************************************
 ****************************************************************/

void ADSL_DisableIRQ1_2(void)
{
	register UINT32 temp;

	if (pF2_IRQ1_ISR == 0 && pF2_IRQ2_ISR == 0)
		return;

	temp = intLock();

#ifdef LEVEL_MODE_ADSL

	DisableInterruptCount++;
	if (pF2_IRQ1_ISR)
		SetGPIOIntEnable(GPIOINT_F2_IRQ1, 0);
	if (pF2_IRQ2_ISR)
		SetGPIOIntEnable(GPIOINT_F2_IRQ2, 0);

	/* Indicate start of critical section for debug */
	SetGPIOF2PCS(TRUE);

	intUnlock(temp);

#else /* LEVEL_MODE_ADSL */

	oldValue[DisableInterruptCount]=temp;
	if (DisableInterruptCount == 0)
		/* Indicate start of critical section for debug */
		SetGPIOF2PCS(TRUE);
	DisableInterruptCount++;

#endif /* LEVEL_MODE_ADSL */
}


/****************************************************************
 ****************************************************************/

void ADSL_EnableIRQ2(void)
{
	register UINT32 temp;

	if (pF2_IRQ2_ISR == 0)
		return;

	temp= intLock();

#ifdef LEVEL_MODE_ADSL

	if (DisableInterruptCount)
		DisableInterruptCount--;
	if (DisableInterruptCount == 0)
	{
		/* Indicate out of critical section for debug */
		SetGPIOF2PCS(FALSE);

		if (pF2_IRQ2_ISR)
			SetGPIOIntEnable(GPIOINT_F2_IRQ2, 1);
	}

	intUnlock(temp);

#else /* LEVEL_MODE_ADSL */

	if (DisableInterruptCount == 0)
	{
		intUnlock(temp);
		return;
	}

	DisableInterruptCount--;
	if (DisableInterruptCount == 0)
		/* Indicate out of critical section for debug */
		SetGPIOF2PCS(FALSE);

	/* Allow for nesting, push values onto a stack */
	intUnlock(temp);
	intUnlock(oldValue[DisableInterruptCount]);

#endif /* LEVEL_MODE_ADSL */
}

/****************************************************************
 ****************************************************************/

void ADSL_DisableIRQ2(void)
{
	register UINT32 temp;

	if (pF2_IRQ2_ISR == 0)
		return;

	temp = intLock();

#ifdef LEVEL_MODE_ADSL

	DisableInterruptCount++;
	if (pF2_IRQ2_ISR)
		SetGPIOIntEnable(GPIOINT_F2_IRQ2, 0);

	/* Indicate start of critical section for debug */
	SetGPIOF2PCS(TRUE);

	intUnlock(temp);

#else /* LEVEL_MODE_ADSL */

	oldValue[DisableInterruptCount]=temp;
	if (DisableInterruptCount == 0)
		/* Indicate start of critical section for debug */
		SetGPIOF2PCS(TRUE);
	DisableInterruptCount++;

#endif /* LEVEL_MODE_ADSL */
}


/*********************************************************************************
    Function     : void DpResetWiring(SYS_WIRING_TYPE wire_pair)
    Description  : This routine will deenergize the relay used for wire switching.
**********************************************************************************/

void DpResetWiring( void )	
{
	// Outer pair
		WriteGPIOData( GPOUT_OUTER_PAIR, 0 );
	// Inner pair
		WriteGPIOData( GPOUT_INNER_PAIR, 0 );

	// Short/Long loop switch
		WriteGPIOData( GPOUT_SHORTLOOPSW, 1 );

}/*  End of function HwResetWiring() */


/*********************************************************************************
    Function     : void DpSetAFEHybridSelect( UINT8	 Select_Line, BOOLEAN State)
    Description  : This routine will deenergize the relay used for wire switching.
**********************************************************************************/
void DpSetAFEHybridSelect(
 	UINT8					  Select_Line, 
	BOOL					  State
)
{
	switch (Select_Line)
	{
		case 1:
			WriteGPIOData( GPOUT_SHORTLOOPSW, State );
			break;
		case 2:
			break;
	}
}

/*********************************************************************************
    Function     : void DpSwitchHookStateEnq( UINT8	 Select_Line, BOOLEAN State)
    Description  : This routine reads remote off-hook hardware circuitry
**********************************************************************************/
BOOL DpSwitchHookStateEnq(void)
{
	return ( ReadGPIOData( GPIN_OH_DET ) );
}


EXPORT_SYMBOL(DpResetWiring);
EXPORT_SYMBOL(FalconReset);
EXPORT_SYMBOL(FalconPowerDown);
EXPORT_SYMBOL(SetF2Interrupts);
EXPORT_SYMBOL(ADSL_CS_Init);
EXPORT_SYMBOL(ADSL_EnableIRQ1_2);
EXPORT_SYMBOL(ADSL_DisableIRQ1_2);
EXPORT_SYMBOL(DpSetAFEHybridSelect);
EXPORT_SYMBOL(DpSwitchHookStateEnq);
EXPORT_SYMBOL(FalconScanEnable);
EXPORT_SYMBOL(ADSL_DisableIRQ2);
EXPORT_SYMBOL(ADSL_EnableIRQ2);
EXPORT_SYMBOL(AFEReset);
EXPORT_SYMBOL(LinedriverPower);
#endif	//if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
