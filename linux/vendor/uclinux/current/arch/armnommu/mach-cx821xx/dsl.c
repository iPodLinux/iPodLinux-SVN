/****************************************************************************
 *
 *  Name:			DSL.c
 *
 *  Description:	DSL routines
 *
 *  Copyright:		(c) 1999 Conexant Systems Inc.
 *
 ****************************************************************************
 *
 *  $Author: davidm $
 *  $Revision: 1.1 $
 *  $Modtime: 11/08/02 12:49p $
 *
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/

#include	<linux/module.h>
#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
#include "bspcfg.h"
#include "bsptypes.h"

#include "cnxtbsp.h"
#include "cnxtirq.h"
#include "dsl.h"
#include "dmasrv.h"
#include "dslisr.h"
#include <asm/arch/dma.h>

extern UINT32 GetRxTimeOutValue(UINT32 usTime);
extern BOOL sysTimerDelay( UINT32 uMicroSec );

/*---------------------------------------------------------------------------
 *  Module Function Prototypes
 *-------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 *  Module Variable Definitions
 *-------------------------------------------------------------------------*/

extern EPINFOSTRUCT DSLInfo;

/* DSL local variable definition */

UINT32 DSL_ISR_img;
UINT32 DSL_IER_img;
UINT32 DSL_CTRL_img;
UINT32 DSLPacketSize;

/* all DSL type definition */
DSL_PACKET_RXBLK Dsl_Packet_Rxblk[DSL_MAX_RX_BUFF];

RXBUFFERSTRUCT DslRxBuffer;
RXBUFFERSTRUCT DslTxBuffer;

/****************************************************************************
 *
 *  Name:			void DSL_init( void )
 *
 *  Description:	Initializes the DSL with DSL RX enable
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/

void DSL_init(void)
{

	/* Reset the ADSL interface */
	*pDSL_CTRL = DSLCTR_SRESET;
	sysTimerDelay(100);  /* 100us */
	*pDSL_CTRL = 0;

	/* Set IDLE , no operation and no interrupts */
	DSL_CTRL_img = 0;
	*pDSL_CTRL = DSL_CTRL_img;

	DSL_IER_img  = 0;
	*pDSL_IER = DSL_IER_img;

	DSL_ClearStatus(DMA_CHANNEL_DSL_TX);
	DSL_ClearStatus(DMA_CHANNEL_DSL_RX);
	return ;
}


void InitDSLTxBuffer(void)
{
	register UINT32 i, uBlkNum;
	register volatile UINT32 * ptr; 	

	/* Init TX DMA link list buffer */
	ptr = pDSL_DMA_TX_BUFF;

	for( uBlkNum = 0; uBlkNum < DSL_MAX_TX_BUFF; uBlkNum++ )
	{
		/* Prefill buffer with pattern */
		for ( i = 0; i < DSL_DMA_BLOCK_DWORDS; i++ )
		{
			*ptr++ = 0x11223344;
		}

		/* Init Tx buffer link informations to make a circular TX block */
      /* If last Tx block then points back to the top Tx block */
		if ( uBlkNum  == (DSL_MAX_TX_BUFF - 1) )
		{
	    	*ptr = (UINT32)pDSL_DMA_TX_BUFF;
		}
      	/* Else points to next block */
		else
		{
			*ptr = (UINT32) (ptr + 2);
		}
		    
		ptr++;

		/* Init the size of the next block, in QWORDS */
		*ptr++ = DSL_DMA_BLOCK_QWORDS;
	}

 	/* Init DSL TX buffer structure */
	DslTxBuffer.pdwRxStart = pDSL_DMA_TX_BUFF;
	DslTxBuffer.pdwRxPtr   = pDSL_DMA_TX_BUFF;
	DslTxBuffer.pdwRxEnd   = (volatile UINT32 *) ((UINT8 *) pDSL_DMA_TX_BUFF + DSL_TXBUFF_SIZE);
	DslTxBuffer.pcData     = (volatile UINT8 *)  pDSL_DMA_TX_BUFF;
	DslTxBuffer.BuffSize   = DSL_TXBUFF_SIZE;

	DSLInfo.pTxHead = DslTxBuffer.pdwRxStart;
	DSLInfo.pTxTail = DslTxBuffer.pdwRxEnd;
	DSLInfo.pTxCurr = DslTxBuffer.pdwRxPtr;

	DSLInfo.bTXWA = TRUE;

	DSLInfo.TxTotalPending = 0;
	DSLInfo.TxPendingInc = 0;
	DSLInfo.TxUnderrun = 0;
	DSLInfo.TxIRQCount = 0;
	DSLInfo.TxRejectCnt = 0;
	DSLInfo.TxPacket = 0;
	DSLInfo.bTxSendMode = FALSE;
	DSLInfo.bTxEnable = TRUE;
	DSLInfo.TxBufferSize = DslTxBuffer.BuffSize;

   /* Init DSL Tx DMA registers */
	*DMA5_PTR1 = (UINT32)DslTxBuffer.pdwRxStart;	/* Set start of buffer pointers */
	*DMA5_CNT1 = DSL_DMA_BLOCK_QWORDS;					/* Fixed TX buffer size to start */
	*pDSL_TXCC_DMA = 0 ;								   /* TX block size to be sent */
}


void InitDSLRxBuffer(void)
{
	register UINT32 i;

	/* And the DSL RX buffer structure */
	DslRxBuffer.pdwRxStart = pDSL_DMA_RX_BUFF;
	DslRxBuffer.pdwRxPtr = pDSL_DMA_RX_BUFF;
   
	if (DSL_RXBUFF_SIZE > DSL_MAX_RXBUFF_SIZE)
	{
		DslRxBuffer.BuffSize = DSL_MAX_RXBUFF_SIZE;
	}
	else
	{
		DslRxBuffer.BuffSize = DSL_RXBUFF_SIZE;
	}
   
	DslRxBuffer.pdwRxEnd = (volatile UINT32 *) ((UINT8 *) pDSL_DMA_RX_BUFF + DslRxBuffer.BuffSize);
	DslRxBuffer.pcData   = (volatile UINT8 *) pDSL_DMA_RX_BUFF;

	memset((UINT32 *)DslRxBuffer.pdwRxStart, 0x96, DslRxBuffer.BuffSize);

	for (i = 0; i < (DslRxBuffer.BuffSize/DSL_MAX_PACKET_SIZE); i++)
	{ 
 		Dsl_Packet_Rxblk[i].ptr = DslRxBuffer.pdwRxStart + (DSL_DMA_BLOCK_DWORDS * i) ;
 		Dsl_Packet_Rxblk[i].ID = i;
	}

	DSLInfo.RxOverrun = 0;
	DSLInfo.RxIRQCount = 0;
	DSLInfo.RxIRQThresh = 0;
	DSLInfo.RxTimeOut = 0;
	DSLInfo.RxPacket = 0;
	DSLInfo.RxBadPacket = 0;
	DSLInfo.RxTotalPending = 0;
	DSLInfo.bRxEnable = TRUE;

	/* Init DSL Rx DMA registers */
	*DMA6_PTR1 = (UINT32)pDSL_DMA_RX_BUFF;
	*DMA6_CNT1 = DSL_MAX_RX_BUFF * DSL_DMA_BLOCK_QWORDS;
	
	/* Indicated RX block size available to DSL */
	*pDSL_RXCC_DMA = (DSL_MAX_RX_BUFF) << 16;		
}


/****************************************************************************
 *
 *  Name:			BOOL DSLWriteBuffer(volatile UINT32* RxBuff,UINT32 ByteCount)
 *
 *  Description:	Buffer DSL with data and enable TX
 *
 *  Inputs:			none
 *
 *  Outputs:		return TRUE if OK
 *
 ***************************************************************************/

BOOL DSLWriteBuffer(volatile UINT32* RxBuff,UINT32 ByteCount)
{
   register UINT32 i;
   volatile UINT8 * bpDSLPtr = (volatile UINT8 *)DslTxBuffer.pdwRxPtr;
   UINT8 * bpRxBuff = (UINT8 *)RxBuff; 
   register UINT32 DSLTXCurrentPending = 0;
   register UINT32 DSLTXCurrentPendingInc = 1;

   /* Parms checking */
   if ( (bpDSLPtr == 0) || (RxBuff == 0) || (ByteCount == 0) )
      return FALSE;

   DSLTXCurrentPending = (((*pDSL_TXCC_DMA) & 0x0000ff00 ) >> 8);

   /* Check if TX buffer is overflowed */
	/* Make sure TX buffer is completely empty in the case of single packet mode */
	if (
        ((DSLTXCurrentPending + DSLInfo.TxPendingInc + (ByteCount/DSL_MAX_PACKET_SIZE)) > DSL_MAX_TX_BUFF)
      )
	{
   	   return FALSE;
	}

   /* Error checking related to DSL work around.  Must wait until */
   /* finish sending before accepting new packets */
	if ( (DSLInfo.bTXWA == TRUE) && (DSLInfo.bTxSendMode == TRUE) )
   {
      return FALSE;
   }

	/* For now we assume there is no packing of 53-bytes ATM cell into 64-bytes */
	/* USB packet from the host driver.  Thus we will transfer exactly 56 bytes */
	/* at a time. If this is a full ATM cell then do the quick copy. */
	if (ByteCount == DSL_MAX_PACKET_SIZE)
	{
	  memcpy(DslTxBuffer.pdwRxPtr, RxBuff, ByteCount);
	}
	else
	{
		ByteCount = DSL_DMA_DATA;

		/*	Prepare data for DSL TX buffer */
		for (i=0;i<ByteCount;i++)
		{
			*bpDSLPtr++ = *bpRxBuff++; 

			if ( !( (i+1) % (DSL_DMA_DATA) ) )
			{
				bpDSLPtr += 11;  				/* To next TX block after 3 dummy bytes and LINK fields */
				if ( bpDSLPtr >= (volatile UINT8 * )DslTxBuffer.pdwRxEnd)
				{
					bpDSLPtr = (volatile UINT8 *) DslTxBuffer.pdwRxStart; 
				}
				if ( (i+1) < ByteCount )
				{
					/* Assume TX buffer are contiguous memory */
					DSLTXCurrentPendingInc +=1;
				}
			}
		}
	}

	/* Update next DSL TX pointers */
	for ( i = 0 ; i < DSLTXCurrentPendingInc ; i++ )
	{ 
		/* Get next TX buffer pointer from embedded link list */
	 	DslTxBuffer.pdwRxPtr = (UINT32 *)(*(DslTxBuffer.pdwRxPtr + DSL_DMA_BLOCK_DWORDS) );
	} 

   DSLInfo.pTxCurr = DslTxBuffer.pdwRxPtr;
   
   DSLInfo.TxPendingInc += DSLTXCurrentPendingInc;

   /* If there is a handle then let hight layer decides when to flush. */
   if (DmaChannelTbl[DMA_CHANNEL_DSL_TX].pUserData)
   {
		/* Only auto-flush if the max packet size is reached. */ 
		if ( (DSLTXCurrentPending + DSLInfo.TxPendingInc) >= DSL_MAX_TX_BUFF )
		{
			DMAFlushChannelBuff(DMA_CHANNEL_DSL_TX);
		}
   }

   else
   {
      if (DSLInfo.bTXWA)
      {
         DMAFlushChannelBuff(DMA_CHANNEL_DSL_TX);
      }
   
      else
      {
         DSLInfo.TxTotalPending += DSLTXCurrentPendingInc;
         *pDSL_TXCC_DMA = DSLTXCurrentPendingInc  ;	/* # of TX block  to be sent */
      }
   }

   return TRUE;
}


/****************************************************************************
 *
 *  Name:			void DSL_DMADisable()
 *
 *  Description:	Disable DSL DMA and interrupts.  Interrupts vectors left
 *					   in place.   
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/

UINT32 DSL_DMADisable(UINT8 uDMACh)
{
   register int OldLevel;
   register UINT32 bReturn;

   bReturn = FALSE;

   OldLevel = intLock();

   switch (uDMACh)
   {
      case DMA_CHANNEL_DSL_TX:
         DSL_Disable_Int(DMA_CHANNEL_DSL_TX);
	      /* Clear any pending DMA tranfers */
	      *pDSL_CTRL_DMA = (DSLCTRDMA_TX_CLRQWCNT | DSLCTRDMA_TX_CLRPEND);
         bReturn = TRUE;
         break;
   
      case DMA_CHANNEL_DSL_RX:
         DSL_Disable_Int(DMA_CHANNEL_DSL_RX);
	      /* Clear any pending DMA tranfers */
	      *pDSL_CTRL_DMA = (DSLCTRDMA_RX_CLRQWCNT | DSLCTRDMA_RX_CLRPEND);
         bReturn = TRUE;
         break;

      default:
         break;
   }

   intUnlock(OldLevel); 
   return (bReturn);
}


/****************************************************************************
 *
 *  Name:			void DSL_DMAEnable()
 *
 *  Description:	Enable DSL DMA and interrupts.  Interrupts vectors left
 *					   in place.   
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/

UINT32 DSL_DMAEnable(UINT8 uDMACh)
{
   register UINT32 OldLevel;
   register UINT32 bReturn;

   bReturn = FALSE;

   OldLevel = intLock();

   switch (uDMACh)
   {
      case DMA_CHANNEL_DSL_TX:
         InitDSLTxBuffer();
         DSL_Enable_Int(DMA_CHANNEL_DSL_TX);
         bReturn = TRUE;
         break;
   
      case DMA_CHANNEL_DSL_RX:
         InitDSLRxBuffer();
         DSL_Enable_Int(DMA_CHANNEL_DSL_RX);
         bReturn = TRUE;
         break;

      default:
         break;
   }

   intUnlock(OldLevel); 
   return (bReturn);
}


void DSL_EnableInterface(UINT8 uDMACh)
{
   switch (uDMACh)
   {
      case DMA_CHANNEL_DSL_TX:
         /* Configure TX DSL to start sending */
         DSL_CTRL_img |= DSLCTR_TXENABLE ;
         *pDSL_CTRL = DSL_CTRL_img;
         break;
   
      case DMA_CHANNEL_DSL_RX:
         /* Init DSL control register for normal operation */
         DSL_CTRL_img |= DSLCTR_RXENABLE | DSLCTR_AIE | DSLCTR_IDLE_INSERT ;  
         *pDSL_CTRL = DSL_CTRL_img;
         break;

      default:
         break;
   }
}


void DSL_ClearStatus(UINT8 uDMACh)
{
	register UINT32 OldLevel;

	/* Disable all DSL related interrupts */
	OldLevel = intLock(); 

	switch (uDMACh)
	{
		case DMA_CHANNEL_DSL_TX:
			/* clear tx status bits if set */
			*pDSL_ISR = 0x000040F0;
			DSL_ISR_img	= *pDSL_ISR;

			/* Clear any pending DMA tranfers */
			*pDSL_CTRL_DMA = ( DSLCTRDMA_TX_CLRQWCNT | DSLCTRDMA_TX_CLRPEND);

			/* Clear DSL global tx interrupts if set */
			*((UINT32*)INT_STAT) = (INT_DMA5 | INT_ADSL_ERR);
			break;

		case DMA_CHANNEL_DSL_RX:
			/* clear rx status bits if set */
			*pDSL_ISR = 0x0003800F;
			DSL_ISR_img	= *pDSL_ISR;

			/* Clear any pending rx DMA tranfers */
			*pDSL_CTRL_DMA = (DSLCTRDMA_RX_CLRQWCNT | DSLCTRDMA_RX_CLRPEND );

			/* Clear DSL global rx interrupts if set */
			*((UINT32*)INT_STAT) = (INT_DMA6 | INT_ADSL_ERR);
			break;

		default:
			break;
	}
				   
	intUnlock(OldLevel); 
}


/****************************************************************************
 *
 *  Name:			void SetDSLInterrupts(void)
 *
 *  Description:	Enable DSL RX and interrupts,also TX interrupts 
 *  call by external task init
 *  to start DSL RX flow   
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/

void DSL_Enable_Int(UINT8 uDMACh)
{
   register UINT32 OldLevel;

   OldLevel = intLock();

   switch (uDMACh)
   {
      case DMA_CHANNEL_DSL_TX:
         /* Since we are using DSL Tx Underrun to terminate transmission, */
         /* there is no need to enable INT_DMA5 and DSLINT_TX_PKTXFRDONE  */
         /* interrupts . */
         /* Enable DSL TX interrupt */
         /*DSL_TX_Int(IRQ_ON);*/
         DSL_TXUnderrun_Int(IRQ_ON);
         break;
   
      case DMA_CHANNEL_DSL_RX:
		/* Enable DSL RX Packet Done and Overrun interrupts */
		DSL_RX_Int(IRQ_ON);
		DSL_RXOverrun_Int(IRQ_ON);

		DSLInitRxTimer();

		DSL_RXTimeout_Int(IRQ_ON);

		/* Setup RX threshold level */
		DSL_RXThresh_Int(IRQ_ON, 10);
		break;

      default:
         break;
   }

   intUnlock(OldLevel); 
}


void DSL_Disable_Int(UINT8 uDMACh)
{
   register UINT32 OldLevel;

   OldLevel = intLock();

   switch (uDMACh)
   {
      case DMA_CHANNEL_DSL_TX:
         DSL_TX_Int(IRQ_OFF);
         DSL_TXUnderrun_Int(IRQ_OFF);
         break;
   
      case DMA_CHANNEL_DSL_RX:
         DSL_RXThresh_Int(IRQ_OFF, 0);
         DSL_RXTimeout_Int(IRQ_OFF);
         DSL_RX_Int(IRQ_OFF);
         DSL_RXOverrun_Int(IRQ_OFF);
         break;

      default:
         break;
   }

   intUnlock(OldLevel); 
}


void DSL_TX_Int(UINT8 IntState)
{
   if (IntState == IRQ_ON)
   {
      DSL_IER_img |= DSLINT_TX_PKTXFRDONE ;
      *pDSL_IER  = DSL_IER_img;
   }
   else
   {
      DSL_IER_img &= (~DSLINT_TX_PKTXFRDONE) ;
      *pDSL_IER  = DSL_IER_img;
   }
}


void DSL_TXUnderrun_Int(UINT8 IntState)
{
   if (IntState == IRQ_ON)
   {
      DSL_IER_img |= DSLINT_TX_UNDERRUN ;
      *pDSL_IER  = DSL_IER_img;
   }
   else
   {
      DSL_IER_img &= (~DSLINT_TX_UNDERRUN) ;
      *pDSL_IER  = DSL_IER_img;
   }
}


void DSL_RXOverrun_Int(UINT8 IntState)
{
   if (IntState == IRQ_ON)
   {
      DSL_IER_img |= DSLINT_RX_OVERRUN ;
      *pDSL_IER  = DSL_IER_img;
   }
   else
   {
      DSL_IER_img &= (~DSLINT_RX_OVERRUN) ;
      *pDSL_IER  = DSL_IER_img;
   }
}


void DSL_RX_Int(UINT8 IntState)
{
   if (IntState == IRQ_ON)
   {
      DSL_IER_img |= DSLINT_RX_PKTXFRDONE ;
      *pDSL_IER  = DSL_IER_img;
   }
   else
   {
      DSL_IER_img &= (~DSLINT_RX_PKTXFRDONE) ;
      *pDSL_IER  = DSL_IER_img;
   }
}

void DSLInitRxTimer(void)
{
	UINT32 TimerValue;

	/* Setup DSL Receive WATCHDOG timer */
	UINT32 usTime = 0;
	TimerValue = 0x0;

	if (HWSTATE.dwADSLDownRate)
	{
		/* Convert from Kbits/sec to packet/sec */
		usTime = (HWSTATE.dwADSLDownRate * 1000)/(8 * 56);
		
		/* Convert to time/packet in us unit.  We want time out to be */
		/* 3 packet-time at a particular line rate. */
		usTime = (DSL_TIMEOUT_PACKET_TIME * 1000000)/usTime;
		if (usTime)
		{
		    TimerValue = GetRxTimeOutValue(usTime);
		}
	}

	*pDSL_RXTIMER = TimerValue;
}

void DSL_RXTimeout_Int(UINT8 IntState)
{
   if (IntState == IRQ_ON)
   {
      DSL_IER_img  |= (DSLINT_RX_WATCHDOG);
      *pDSL_IER  = DSL_IER_img;
   }
   else
   {
      DSL_IER_img  &= (~DSLINT_RX_WATCHDOG);
      *pDSL_IER  = DSL_IER_img;
   }
}

void DSL_RXThresh_Int(UINT8 IntState, UINT32 ThresholdLevel)
{
   if (IntState == IRQ_ON)
   {
      *pDSL_RXCC_DMA2 = ThresholdLevel;
      DSL_IER_img  |= (DSLINT_RX_THRESHOLD);
      *pDSL_IER  = DSL_IER_img;
   }
   else
   {
      DSL_IER_img  &= (~DSLINT_RX_THRESHOLD);
      *pDSL_IER  = DSL_IER_img;
      *pDSL_RXCC_DMA2 = ThresholdLevel;
   }
}
#endif	//if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
