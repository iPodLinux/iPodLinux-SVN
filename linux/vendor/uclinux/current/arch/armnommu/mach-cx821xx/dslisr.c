/****************************************************************************
 *
 *  Name:			DSLISR.c
 *
 *  Description:	DSL Interrupt handler routines
 *
 *  Copyright:		(c) 2001 Conexant Systems Inc.
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

#include <linux/module.h>
#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
#include "bspcfg.h"
#include "bsptypes.h"
#include "cnxtbsp.h"
#include "cnxtirq.h"
#include "dmasrv.h"
#include "dsl.h"
#include "dslisr.h"
#include <asm/arch/dma.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#define DSL_DMA_RX_DEBUG 0
#define DSL_DMA_TX_DEBUG 0

extern void SetDslRxDataInd(BOOL value);
extern void SetDslTxDataInd(BOOL value);

EPINFOSTRUCT DSLInfo;

#if DSL_DMA_RX_DEBUG
UINT32 rxpendtbl[RX_PENDING_ITEM];
UINT32 rxpendoffset=0;
#endif

#if DSL_DMA_TX_DEBUG
/* Check for pointer1  */
UINT32 Dma5_ptr1=0xffffffff;
UINT32 Dma5_ptr1_soft=0xffffffff;
UINT32 Dma5_cnt1=0xffffffff;
UINT32 Dma5_cnt1_good = 0xffffffff;	

UINT32 Dma5_ptr1_nequal_cnt=0;
#endif

/***************************************************************************/
/***************************************************************************/
void DSL_RX_Handler( void);
DECLARE_TASKLET( dsl_rx_tasklet, DSL_RX_Handler, 0 );


void DSL_RX_Handler( void)
{
   register UINT32 PreviousTotalRxPending, NewRxPending;
   register UINT32 ByteMove;

   DSLInfo.RxIRQCount += 1;

   /* Clear RX packet done status */
   *pDSL_ISR = DSLINT_RX_PKTXFRDONE;

   /* Turn ADSL RX Data Indication on */
   SetDslRxDataInd(TRUE);

   /* Calculate number of new pending ATM cells */
   PreviousTotalRxPending = DSLInfo.RxTotalPending;
   DSLInfo.RxTotalPending = ((*pDSL_RXCC_DMA & DSL_RX_PENDING_FLD ) >> DSL_RX_PENDING_SHIFT);
   NewRxPending = DSLInfo.RxTotalPending - PreviousTotalRxPending;
   DSLInfo.RxPacket += NewRxPending;

   #if DSL_DMA_RX_DEBUG
   /* write the pending cell to structure for observation */
   if ( ( rxpendoffset += 1 ) >= RX_PENDING_ITEM )
      rxpendoffset=0;

   rxpendtbl[rxpendoffset] = NewRxPending;
   #endif

   if (DmaChannelTbl[DMA_CHANNEL_DSL_RX].pDMACallback != NULL)
   {
      ByteMove = 0;

      /* If Timeout has occurred then read out all data from DSL Rx buffer. */
	   while (DSLInfo.RxTotalPending > ByteMove)
	   { 
         if (DmaChannelTbl[DMA_CHANNEL_DSL_RX].pDMACallback( DmaChannelTbl[DMA_CHANNEL_DSL_RX].pUserData,
                                                             (UINT32 *)DslRxBuffer.pdwRxPtr,
                                                             56
                                                           ) )
         {
            DSLRxBufferUpdate();
         }
         else
         {
            goto fini;
         }
      }
   }
   
fini:
   /* Turn ADSL RX Data Indication off */
   SetDslRxDataInd(FALSE);

}


void DSLRxBufferUpdate(void)
{
   /* Update next DSL RX pointers */
   DslRxBuffer.pdwRxPtr += DSL_DMA_BLOCK_DWORDS;
   if ( DslRxBuffer.pdwRxPtr >= DslRxBuffer.pdwRxEnd ) 	
      DslRxBuffer.pdwRxPtr= DslRxBuffer.pdwRxStart ;

   /* Decrement Rx total pending */
   DSLInfo.RxTotalPending -= 1;
   *pDSL_RXCC_DMA = (UINT32)((DSL_MAX_RX_BUFF) << DSL_RXBUFFSIZE_SHIFT) | (UINT32)1;	
}


/****************************************************************************
 *
 *  Name:			void DSL_TX_DMA_ISR( void )
 *
 *  Description:	DSL TX DMA interrupt service routine
 *
 *  Inputs:			IntStat
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void DSL_TX_DMA_ISR(void )
{
    register UINT32 OldLevel; 

	OldLevel=INTLOCK(); 

	DSLInfo.TxIRQCount += 1;

	*pDSL_ISR =  DSLINT_TX_PKTXFRDONE;
   *((UINT32*)INT_STAT) = Int_DSL_TX_DMA;

	INTUNLOCK(OldLevel);
}

/****************************************************************************
 *
 *  Name:			void DSL_ERR_Handler( void )
 *
 *  Description:	DSL error ISR
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/

void DSL_ERR_Handler( void)
{
   /* Check for Over/under run interrupt */
   register UINT32 IsrStat;		
   register UINT32 OldLevel; 

   OldLevel=intLock(); 

   IsrStat = (*pDSL_ISR & *pDSL_IER & DSLINT_STATUS_MASK);
   
   while (IsrStat)
   {
		if ( IsrStat & DSLINT_RX_WATCHDOG )
		{	
			/* clear DSL RX TIMEOUT pending interrupt */
			*pDSL_ISR = DSLINT_RX_WATCHDOG;
			
			/* clear global interrupt	*/
			*((UINT32*)INT_STAT) = Int_DSL;

			DSLInfo.RxTimeOut += 1;
//			DSL_RX_Handler();				

			tasklet_schedule( &dsl_rx_tasklet );
		}

		if ( IsrStat & DSLINT_RX_THRESHOLD )
		{
			DSLInfo.RxIRQThresh += 1;
			
			/* clear DSL RX TIMEOUT pending interrupt */
			*pDSL_ISR = DSLINT_RX_THRESHOLD;
			
			/* clear global interrupt	*/
			*((UINT32*)INT_STAT) = Int_DSL;
//			DSL_RX_Handler();				
			
			tasklet_schedule( &dsl_rx_tasklet );

		}
		
		if ( IsrStat &  DSLINT_TX_PKTXFRDONE )
		{

			if (DmaChannelTbl[DMA_CHANNEL_DSL_TX].pUserData)
			{
				if (DmaChannelTbl[DMA_CHANNEL_DSL_TX].pDMACallback != NULL)
				{
			   		//DmaChannelTbl[DMA_CHANNEL_DSL_TX].pDMACallback( DmaChannelTbl[DMA_CHANNEL_DSL_TX].pUserData, 0, 0);
					printk("DSLINT_TX_PKTFRDONE\n");
				}
			}
		}

		if ( IsrStat &  DSLINT_TX_UNDERRUN )
		{

			DSL_TX_Underrun_ISR();

			if (DmaChannelTbl[DMA_CHANNEL_DSL_TX].pUserData)
			{
				if (DmaChannelTbl[DMA_CHANNEL_DSL_TX].pDMACallback != NULL)
				{
			   		DmaChannelTbl[DMA_CHANNEL_DSL_TX].pDMACallback( DmaChannelTbl[DMA_CHANNEL_DSL_TX].pUserData, 0, 0);
				}
			}
		}

		if ( IsrStat &  DSLINT_RX_OVERRUN )
		{
			DSL_RX_Overrun_ISR();
		}

		IsrStat = (*pDSL_ISR & *pDSL_IER & DSLINT_STATUS_MASK);
	}

	intUnlock(OldLevel);
}

/****************************************************************************
 *
 *  Name:			void DSL_TX_Underrun_ISR( void )
 *
 *  Description:	DSL TX underrun ISR
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void DSL_TX_Underrun_ISR( void )
{
   UINT32 PacketSent = 0;

   DSLInfo.TxUnderrun += 1;

	*pDSL_ISR =  DSLINT_TX_UNDERRUN;
	*((UINT32*)INT_STAT) = Int_DSL;

   /* Read current pending TX register */
   PacketSent = DSLInfo.TxTotalPending - (((*pDSL_TXCC_DMA) & 0x0000ff00 ) >> 8);
   if (DSLInfo.TxTotalPending >= PacketSent)
      DSLInfo.TxTotalPending -= PacketSent;
   DSLInfo.TxPacket += PacketSent ;

   if (DSLInfo.TxTotalPending == 0)
   {
      if (DSLInfo.bTxSendMode == TRUE)
      {
         #if DSL_DMA_TX_DEBUG
         /* Check if current HW pointer1 matches SW pointer */
         if ( *DMA5_PTR1 != (UINT32)DslTxBuffer.pdwRxPtr ) 
         {
	         Dma5_ptr1 = *DMA5_PTR1;
	         Dma5_cnt1 = *DMA5_CNT1;	
	         Dma5_ptr1_soft = (UINT32)DslTxBuffer.pdwRxPtr;
	         Dma5_ptr1_nequal_cnt++;
         }
         else
	         Dma5_cnt1_good = *DMA5_CNT1;	
         #endif
		 
         /* This fix the DSL data shift problem */
         if (DSLInfo.bTXWA)
         {
            *DMA5_PTR1 = (UINT32)DslTxBuffer.pdwRxPtr;   /* Set start of buffer pointers */
            *DMA5_CNT1 = DSL_DMA_BLOCK_QWORDS;		     /* Fixed TX buffer size to start */
         }
          
         DSLInfo.bTxSendMode = FALSE;
      }
   }
}

/****************************************************************************
 *
 *  Name:			void DSL_RX_Overrun_ISR(void )
 *
 *  Description:	DSL RX overrun ISR
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void DSL_RX_Overrun_ISR( void )
{
   DSLInfo.RxOverrun += 1;

   /* Disable DSL receiver */
   DSL_CTRL_img &= ~(DSLCTR_RXENABLE);  
   *pDSL_CTRL = DSL_CTRL_img;

   /* Wait for DSL receiver to go into idle state */
   while ( !(*pDSL_CTRL & DSLCTR_RX_IDLE) );

	/* Clear any pending Rx DMA tranfers */
	*pDSL_CTRL_DMA = (DSLCTRDMA_RX_CLRQWCNT | DSLCTRDMA_RX_CLRPEND);

   /* Clear Rx overrun interrupt status */
   *pDSL_ISR =  DSLINT_RX_OVERRUN;
   *((UINT32*)INT_STAT) = Int_DSL;

   /* clear DSL RX TIMEOUT pending interrupt */
	*pDSL_ISR = DSLINT_RX_THRESHOLD;
   
   /* clear DSL RX TIMEOUT pending interrupt */
	*pDSL_ISR = DSLINT_RX_WATCHDOG;

   /* clear global interrupt	*/
   *((UINT32*)INT_STAT) = Int_DSL;

   /* Now DSL receiver is idle, we can reset DMA registers */
   *DMA6_PTR1 = (UINT32)pDSL_DMA_RX_BUFF;
   *DMA6_CNT1 = DSL_MAX_RX_BUFF * DSL_DMA_BLOCK_QWORDS;
   DslRxBuffer.pdwRxPtr=pDSL_DMA_RX_BUFF; 
   DSLInfo.RxTotalPending = 0;

   /* Reenable DSL receiver */
   DSL_CTRL_img |= DSLCTR_RXENABLE;  
   *pDSL_CTRL = DSL_CTRL_img;

   /* Kick start DSL receiver out of idle */
   DSL_CTRL_img |= DSLCTR_RX_KICK ;  
   *pDSL_CTRL = DSL_CTRL_img;
   DSL_CTRL_img &= ~(DSLCTR_RX_KICK) ;

}



#endif	//if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
