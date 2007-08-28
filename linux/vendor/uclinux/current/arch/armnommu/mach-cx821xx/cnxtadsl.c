/****************************************************************************
*
*	Name:				CNXTADSL.C
*
*
*	Copyright:		(c) 2001 Conexant Systems Inc.
*
*****************************************************************************
*	$Author: davidm $
*	$Revision: 1.1 $
*	$Modtime: 11/06/02 9:31a $
****************************************************************************/

#include    <linux/kernel.h>
#include    <linux/types.h>
#include    <linux/sched.h>
#include    <linux/unistd.h>
#include    "OsTools.h"
#include    <linux/module.h>
#include    <linux/init.h>

#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
#include    "bsptypes.h"
#include    "bspcfg.h"

#include    "gpio.h"
#include    "cnxttimer.h"
#include    "cnxtirq.h"
#include    "cnxtbsp.h"
#include    "pwrmanag.h"
#include    "dmasrv.h"
#include    "syslib.h"

#include    "cardinfo.h"
#include    "dsl.h"
#include    "dsl.h"
#include    "dslbase.h"
#include    "hwio.h"

#include    "cnxtadsl.h"

#define MAX_SHOWTIME_PARMS_ARRAY_SIZE 18
static UINT32 ShowtimeParmsLastIndex= 0;
static CARDPARAM ShowtimeParms[MAX_SHOWTIME_PARMS_ARRAY_SIZE];

static int GetDataLastIndex = 0;
static void CardGetInfo0(PCARDDATA pCardData);
static void CardGetInfo1(PCARDDATA pCardData);
static void CardGetInfo2(PCARDDATA pCardData);

#define LED_DELAY_PERIOD   250 /* 250 ms */

void CnxtAdslLEDTask(void *sem)
{
	BOOL HandShakeLED;
	BOOL ShowtimeLED;

	struct task_struct *tsk = current ;
	tsk->session = 1 ;
	tsk->pgrp = 1 ;
	strcpy ( tsk->comm, "CnxtAdslLEDTask" ) ;

	/* sigstop and sigcont will stop and wakeup kupdate */
	spin_lock_irq(&tsk->sigmask_lock);
	sigfillset(&tsk->blocked);
	siginitsetinv(&current->blocked, sigmask(SIGCONT) | sigmask(SIGSTOP));
	recalc_sigpending(tsk);
	spin_unlock_irq(&tsk->sigmask_lock);

	up((struct semaphore *)sem);
	
	while(1)
	{
		/* Condensed Showtime State to two Booleans - In Showtime and In Training */
		ShowtimeLED= FALSE;
		HandShakeLED= FALSE;

		switch (HWSTATE.dwLineStatus)
		{
			case HW_IO_MODEM_STATUS_ACTIVATED:
				ShowtimeLED= TRUE;
				break;

			case HW_IO_MODEM_STATUS_CHANNEL_ANALYSIS:
			case HW_IO_MODEM_STATUS_TRANSCEIVER_TRAINING:
			case HW_IO_MODEM_STATUS_EXCHANGE:
			case HW_IO_MODEM_STATUS_ACTIVATION:
				HandShakeLED= TRUE;
				break;
		}

		/* Blink Showtime LED if in Training */
		if (ShowtimeLED || HandShakeLED)
		{
			WriteGPIOData( GPOUT_SHOWTIME_LED,LED_ON );
		}

		/* This logic is for a blinking Showtime LED */
		if (!ShowtimeLED)
		{
			WriteGPIOData( GPOUT_SHOWTIME_LED,LED_OFF );
		}

		TaskDelayMsec(LED_DELAY_PERIOD);

	}
}


/* Initialization called from Showtime initialization */
BOOL ADSL_Init(void)
{
    HWSTATE.eADSLLineState = ADSL_HW_INIT ;
	return HWSTATE.eADSLLineState == ADSL_HW_INIT ? TRUE : FALSE;
}

/****************************************************************
 ****************************************************************/

/* CP request for a new state */
BOOL ADSL_RequestedLineState(BOOL Up)
{
	switch (HWSTATE.eADSLLineState)
	{
		case ADSL_SHOWTIME_UP:
		case ADSL_SHOWTIME_DOWN:
		case ADSL_HW_INIT:
			HWSTATE.eADSLRequestLineState =
			   Up ? ADSL_REQUEST_SHOWTIME_UP : ADSL_REQUEST_SHOWTIME_DOWN;
			return TRUE;

		default:
			return FALSE;
	}
}

/****************************************************************
 ****************************************************************/

/* Task 4 gets requested state */
UINT32 ADSL_GetRequestedLineState(void)
{
	return HWSTATE.eADSLRequestLineState;
}

/****************************************************************
 ****************************************************************/

UINT32 ADSL_LinkStatus(void)
{
	return HWSTATE.dwLineStatus;
}

/****************************************************************
 ****************************************************************/

UINT32 ADSL_GetDownstreamDataRate(void)
{
	return HWSTATE.dwADSLDownRate;
}

/****************************************************************
 ****************************************************************/

UINT32 ADSL_GetUpstreamDataRate(void)
{
	return HWSTATE.dwADSLUpRate;
}

/****************************************************************
 ****************************************************************/

/* State machine for current ADSL state */
UINT32 ADSL_UpdateState
(
	BOOL InShowtime,
	UINT32 dwADSLDownRate,
	UINT32 dwADSLUpRate,
	UINT32 dwLineStatus,
	PHW_IO_TRANSCEIVER_STATUS_T		pHwIoTransceiverStatus
)
{
	UINT32 bReturn = TRUE;

	/* Update BSP's state description of the Showtime code */
	HWSTATE.dwADSLDownRate 				= dwADSLDownRate;
	HWSTATE.dwADSLUpRate				= dwADSLUpRate;
	HWSTATE.dwLineStatus				= dwLineStatus;

	HWSTATE.Transmit_State				= pHwIoTransceiverStatus->Transmit_State;
	HWSTATE.Receive_State				= pHwIoTransceiverStatus->Receive_State;
	HWSTATE.Process_State				= pHwIoTransceiverStatus->Process_State;
	HWSTATE.Up_SNR_Margin_Cur			= pHwIoTransceiverStatus->Up_SNR_Margin.Mar_Cur;
	HWSTATE.Up_SNR_Margin_Min			= pHwIoTransceiverStatus->Up_SNR_Margin.Mar_Min;
	HWSTATE.Up_SNR_Margin_MinBin		= pHwIoTransceiverStatus->Up_SNR_Margin.Mar_Min_Bin;
	HWSTATE.Down_SNR_Margin_Cur			= pHwIoTransceiverStatus->Down_SNR_Margin.Mar_Cur;
	HWSTATE.Down_SNR_Margin_Min			= pHwIoTransceiverStatus->Down_SNR_Margin.Mar_Min;
	HWSTATE.Down_SNR_Margin_Min_Bin		= pHwIoTransceiverStatus->Down_SNR_Margin.Mar_Min_Bin;
	HWSTATE.Up_Attn						= pHwIoTransceiverStatus->Up_Attn;
	HWSTATE.Down_Attn					= pHwIoTransceiverStatus->Down_Attn;
	HWSTATE.Tx_Power					= pHwIoTransceiverStatus->Tx_Power;
	HWSTATE.Up_Bits_Per_Frame			= pHwIoTransceiverStatus->Up_Bits_Per_Frame;
	HWSTATE.Down_Bits_Per_Frame			= pHwIoTransceiverStatus->Down_Bits_Per_Frame;
	HWSTATE.Startup_Attempts			= pHwIoTransceiverStatus->Startup_Attempts;
	HWSTATE.Up_CRC_Errors				= pHwIoTransceiverStatus->Up_CRC_Errors;
	HWSTATE.Down_CRC_Errors				= pHwIoTransceiverStatus->Down_CRC_Errors;
	HWSTATE.Up_FEC_Errors				= pHwIoTransceiverStatus->Up_FEC_Errors;
	HWSTATE.Down_FEC_Errors				= pHwIoTransceiverStatus->Down_FEC_Errors;
	HWSTATE.Up_HEC_Errors				= pHwIoTransceiverStatus->Up_HEC_Errors;
	HWSTATE.Down_HEC_Errors				= pHwIoTransceiverStatus->Down_HEC_Errors;
	HWSTATE.Retrain_Attempts			= pHwIoTransceiverStatus->Retrain_Attempts;

	/*
	** Request should be processed already so it is safe to clear request.
	** For a request ADSL_DISABLE means "no request".
	*/
	HWSTATE.eADSLRequestLineState = ADSL_DISABLE;

	switch (HWSTATE.eADSLLineState)
	{
		case ADSL_DISABLE:
			break;

		case ADSL_HW_INIT:
			if (InShowtime)
			{
				/* If HWSTATE.eADSLLineState != ADSL_SHOWTIME_UP then DSL */
				/* DMA will not be enabled. */
				HWSTATE.eADSLLineState = ADSL_SHOWTIME_UP;
				DmaOpenChannel(DMA_CHANNEL_DSL_TX);
				DmaOpenChannel(DMA_CHANNEL_DSL_RX);
				#if CONFIG_CNXT_SOFTSAR
					//FrameALLineUp( SSpDrvCtrl );
				#endif
			}
			break;

		case ADSL_SHOWTIME_UP:
			if (!InShowtime)
			{
				HWSTATE.eADSLLineState = ADSL_SHOWTIME_DOWN;
				HWSTATE.dwADSLDownRate = 0;
				HWSTATE.dwADSLUpRate = 0;
				DmaCloseChannel(DMA_CHANNEL_DSL_RX);
				DmaCloseChannel(DMA_CHANNEL_DSL_TX);
				#if CONFIG_CNXT_SOFTSAR
					//FrameALLineDown(  SSpDrvCtrl );
				#endif
			}
			break;

		case ADSL_SHOWTIME_DOWN:
			if (InShowtime)
			{
				HWSTATE.eADSLLineState = ADSL_SHOWTIME_UP;
				DmaOpenChannel(DMA_CHANNEL_DSL_TX);
				DmaOpenChannel(DMA_CHANNEL_DSL_RX);
				#if CONFIG_CNXT_SOFTSAR
					//FrameALLineUp(  SSpDrvCtrl );
				#endif
			}
			break;

		default:
			break;
	}

	return bReturn;
}

/****************************************************************
 ****************************************************************/

void CardGetInfo0(PCARDDATA pCardData)
{
	pCardData->dwCount= 6;

	pCardData->Param[0].dwIndex=CC_CFG_DOWNRATE;
	pCardData->Param[0].dwValue=HWSTATE.dwADSLDownRate;

	pCardData->Param[1].dwIndex=CC_CFG_UPRATE;
	pCardData->Param[1].dwValue=HWSTATE.dwADSLUpRate;

	pCardData->Param[2].dwIndex=CC_CFG_LINKSTATUS;
	pCardData->Param[2].dwValue=HWSTATE.eADSLLineState;

	pCardData->Param[3].dwIndex=CC_CFG_LINESTATUS;
	pCardData->Param[3].dwValue=HWSTATE.dwLineStatus;

	pCardData->Param[4].dwIndex=CC_CFG_MACADDRHIGH;
	pCardData->Param[4].dwValue=HWSTATE.dwMACAddressHigh;

	pCardData->Param[5].dwIndex=CC_CFG_MACADDRLOW;
	pCardData->Param[5].dwValue=HWSTATE.dwMACAddressLow;
}

void CardGetInfo1(PCARDDATA pCardData)
{
#if 0
	pCardData->dwCount= 7;

	pCardData->Param[0].dwIndex=CC_CFG_CAPOCCUPATIONDNSTR;
	pCardData->Param[0].dwValue=HWSTATE.R_relCapacityOccupationDnstr;

	pCardData->Param[1].dwIndex=CC_CFG_NOISEMARGINDNSTR;
	pCardData->Param[1].dwValue=HWSTATE.R_noiseMarginDnstr;

	pCardData->Param[2].dwIndex=CC_CFG_OUTPUTPOWERDNSTR;
	pCardData->Param[2].dwValue=HWSTATE.R_outputPowerDnstr;

	pCardData->Param[3].dwIndex=CC_CFG_ATTENDNSTR;
	pCardData->Param[3].dwValue=HWSTATE.R_attenuationDnstr;

	pCardData->Param[4].dwIndex=CC_CFG_CAPOCCUPATIONUPSTR;
	pCardData->Param[4].dwValue=HWSTATE.R_relCapacityOccupationUpstr;

	pCardData->Param[5].dwIndex=CC_CFG_NOISEMARGINUPSTR;
	pCardData->Param[5].dwValue=HWSTATE.R_noiseMarginUpstr;

	pCardData->Param[6].dwIndex=CC_CFG_OUTPUTPOWERUPSTR;
	pCardData->Param[6].dwValue=HWSTATE.R_outputPowerUpstr;
#endif
}

void CardGetInfo2(PCARDDATA pCardData)
{
#if 0
	pCardData->dwCount= 1;

	pCardData->Param[0].dwIndex=CC_CFG_ATTENUPSTR;
	pCardData->Param[0].dwValue=HWSTATE.R_attenuationUpstr;
#endif
}

void CardGetInfo(PCARDDATA pCardData)
{
	switch (GetDataLastIndex)
	{
	case 0:
		CardGetInfo0(pCardData);
		GetDataLastIndex++;
		break;

	case 1:
		CardGetInfo1(pCardData);
		GetDataLastIndex++;
		break;

	default:
		CardGetInfo2(pCardData);
		GetDataLastIndex=0;
		break;
	}
}

/****************************************************************
 ****************************************************************/

void ADSL_GetShowtimeParm(UINT32 index, UINT32* pdwValue)
{
	register UINT32 i;

	for (i=0; i<ShowtimeParmsLastIndex; ++i)
		if (index == ShowtimeParms[i].dwIndex)
		{
//			*pdwValue= ShowtimeParms[i].dwValue;
			break;
		}
}

/****************************************************************
 ****************************************************************/
														  
/* USB sends over the parm data and the following routines
   save the data until Showtime is ready for the data.  This
   code assumes that new entries are added at the end. */
void CardPutData(PCARDDATA pCardData)
{
	register UINT32 ic,js;
	register UINT32 NumberOfEntries= pCardData->dwCount;
	register PCARDPARAM pdwData= pCardData->Param;

	for (ic=0; ic < NumberOfEntries; ++ic)
	{
		for (js=0; js < ShowtimeParmsLastIndex; ++js)
			if (ShowtimeParms[js].dwIndex == pdwData[ic].dwIndex)
			{
				ShowtimeParms[js].dwValue= pdwData[ic].dwValue;
				break;
			}
		if (js >= ShowtimeParmsLastIndex && js < MAX_SHOWTIME_PARMS_ARRAY_SIZE)
			ShowtimeParms[ShowtimeParmsLastIndex++]= pdwData[ic];
	}
}

EXPORT_SYMBOL(ADSL_Init);
EXPORT_SYMBOL(ADSL_GetRequestedLineState);
EXPORT_SYMBOL(ADSL_UpdateState);
EXPORT_SYMBOL(ADSL_RequestedLineState);
#endif	//if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
