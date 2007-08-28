/****************************************************************************
*
*	Name:			dma.c
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
*  $Modtime: 11/08/02 12:49p $
****************************************************************************/

#include	<linux/module.h>
#include    <linux/init.h>
#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
#include "bsptypes.h"
#include "bspcfg.h" 

#include "dmasrv.h"
#include "cnxtirq.h"
#include "cnxtbsp.h"

#if DSL_DMA
	#include "dsl.h"
	extern EPINFOSTRUCT DSLInfo;
#endif

BOOL AddDMABufferList ( UINT32 uDMACh,UINT32* pSBuff,UINT32 uBuffSize)
{
   UINT32 OldLevel;
   register BOOL bReturn;
   
   /* Note : uBuffSize from BM must be <= 64 */
   if ( (pSBuff == 0) || (uBuffSize > BULK_MAX_PACKET_SIZE) )
      return FALSE;

   /* Initialize variables to error and no BM operation (pEPBM = 0) */
   bReturn = FALSE;

   OldLevel = INTLOCK();

   switch ( uDMACh )
   {

  #if DSL_DMA
      case DMA_CHANNEL_DSL_TX :			/* 5 */
		bReturn = DSLWriteBuffer(pSBuff,uBuffSize);
		if (bReturn == 0)
		{
			DSLInfo.TxRejectCnt += 1;
		}
		break;
  #endif

      case DMA_CHANNEL_USB_TX_EP1 :   /* 11 */
      case DMA_CHANNEL_USB_TX_EP2 :   /* 10 */
      case DMA_CHANNEL_USB_TX_EP3	:  /* 9  */
      case DMA_CHANNEL_USB_TX_EP0 :   /* 13 */
      case DMA_CHANNEL_EMAC1_TX :
      case DMA_CHANNEL_EMAC2_TX :
      default :
         break;
   }

   INTUNLOCK(OldLevel);
   return bReturn;
} 		


BOOL DMAFlushChannelBuff(UINT32 uDMACh)
{
   UINT32 OldLevel = 0;
   register BOOL bReturn = FALSE;

   /* Need to turn off interrupts here since background task is modifying a */
   /* variable that is accessible also from interrupt context. */
   OldLevel = INTLOCK();

   switch ( uDMACh )
   {
      case DMA_CHANNEL_USB_TX_EP0 : /* 13 */
      case DMA_CHANNEL_USB_TX_EP1 : /* 11 */
      case DMA_CHANNEL_USB_TX_EP3 : /* 9  */
      case DMA_CHANNEL_USB_TX_EP2 : /* 10 */
      case DMA_CHANNEL_USB_RX_EP0 :
      case DMA_CHANNEL_USB_RX_EP1 :
      case DMA_CHANNEL_USB_RX_EP2 :
      case DMA_CHANNEL_USB_RX_EP3 :
         break;

      case DMA_CHANNEL_M2M_IN :
      case DMA_CHANNEL_M2M_OUT:
         break;

  #if DSL_DMA
     case DMA_CHANNEL_DSL_TX :			/* 5 */
        if (DSLInfo.bTXWA)
        {
           if ( (DSLInfo.TxPendingInc != 0) && (DSLInfo.bTxSendMode == FALSE) )
           {
              DSLInfo.TxTotalPending += DSLInfo.TxPendingInc;
              *pDSL_TXCC_DMA = DSLInfo.TxPendingInc;	/* # of TX block  to be sent */
              DSLInfo.TxPendingInc = 0;
	            DSLInfo.bTxSendMode = TRUE;
	            bReturn = TRUE;
           }
        }

        else if (DSLInfo.TxPendingInc)
        {
           DSLInfo.TxTotalPending += DSLInfo.TxPendingInc;
           *pDSL_TXCC_DMA = DSLInfo.TxPendingInc;	/* # of TX block  to be sent */
           DSLInfo.TxPendingInc = 0;
           DSLInfo.bTxSendMode = TRUE;
           bReturn = TRUE;
        }
        break;

     case DMA_CHANNEL_DSL_RX :
  #endif

      case DMA_CHANNEL_EMAC1_TX :
      case DMA_CHANNEL_EMAC2_TX :
      case DMA_CHANNEL_EMAC1_RX :
      case DMA_CHANNEL_EMAC2_RX :
      default :
         break;
   }

   INTUNLOCK(OldLevel);
   return bReturn;
}


EXPORT_SYMBOL(DMAFlushChannelBuff);
EXPORT_SYMBOL(AddDMABufferList);
#endif	//if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
