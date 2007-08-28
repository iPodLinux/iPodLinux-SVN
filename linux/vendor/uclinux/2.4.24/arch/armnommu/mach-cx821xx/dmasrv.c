/****************************************************************************
*
*	Name:			dmasrv.c
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

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/

#include	<linux/module.h>
#include    <linux/init.h>
#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
#include "bsptypes.h"
//#include "epic_bsptypes.h"
#include "bspcfg.h"
#include "dmasrv.h"
#include <asm/arch/dma.h>
#include "cnxtbsp.h"

#if DSL_DMA
	#include "dsl.h"
#endif


/* DMA channel structures */
DMA_CHANNEL DmaChannelTbl[ DMA_CHANNEL_MAX ];


/****************************************************************************
 *
 *  Name:         void DMAInit( void )
 *
 *  Description:  DMA initialization
 *
 *  Inputs:       none
 *
 *  Outputs:      none
 *
 ***************************************************************************/
void DmaInit( void )
{
   register int uDMACh;

   /* For all DMA channels */
   for ( uDMACh = 0; uDMACh < DMA_CHANNEL_MAX; uDMACh++ )
   {
      /* Mark channel closed */
      DmaChannelTbl[ uDMACh ].bOpen = FALSE;

      /* Mark channel disabled */
      DmaChannelTbl[ uDMACh ].bEnabled = FALSE;

      /* Set callback function pointer and parameter to NULL */
      DmaChannelTbl[ uDMACh ].pDMACallback = 0;
      DmaChannelTbl[ uDMACh ].pUserData = 0;
   }
}

BOOL DmaSendBuffer
(
	UINT32		uDMACh, 
	UINT32*		pBuffer,
	UINT32		uBufferSize
)
{
	BOOL	bSuccess	= FALSE;

    bSuccess = ((pBuffer == NULL) ? 
      DMAFlushChannelBuff(uDMACh) : 
      AddDMABufferList(uDMACh, pBuffer, uBufferSize));

	return bSuccess;
}

/****************************************************************************
 *
 *  Name:         BOOL DmaResetChannel( UINT32 uDMACh )
 *
 *  Description:  Resets a DMA channel
 *
 *  Inputs:       UINT8 uDMACh = DMA channel number
 *
 *  Outputs:      Returns TRUE if successful, FALSE if not
 *
 ***************************************************************************/
static BOOL DmaResetChannel( UINT32 uDMACh )
{
   BOOL bSuccess= FALSE;
   BOOL bState = FALSE;

   /* Make sure the channel number is valid */
   if ( uDMACh < DMA_CHANNEL_MAX)
   {
      /* Disable DMA channel before resetting */
      if ( DmaChannelTbl[ uDMACh ].bOpen &&
           DmaChannelTbl[ uDMACh ].bEnabled)
      {
         bState = TRUE;
         DmaDisableChannel(uDMACh);
      }

      switch (uDMACh)
      {
         case DMA_CHANNEL_EMAC1_TX:
         case DMA_CHANNEL_EMAC1_RX:
         case DMA_CHANNEL_EMAC2_TX:
         case DMA_CHANNEL_EMAC2_RX:
         case DMA_CHANNEL_M2M_IN:
         case DMA_CHANNEL_M2M_OUT:
         case DMA_CHANNEL_DSL_TX:
         case DMA_CHANNEL_DSL_RX:
         case DMA_CHANNEL_USB_TX_EP3:
         case DMA_CHANNEL_USB_TX_EP2:
         case DMA_CHANNEL_USB_TX_EP1:
         case DMA_CHANNEL_USB_TX_EP0:
         case DMA_CHANNEL_USB_RX_EP3:
         case DMA_CHANNEL_USB_RX_EP2:
         case DMA_CHANNEL_USB_RX_EP1:
         case DMA_CHANNEL_USB_RX_EP0:
         default:
            break;
      }

      if (bState)
      {
         /* Restore channel state to Open */
         DmaEnableChannel(uDMACh);
      }

      bSuccess = TRUE;
   }

   return bSuccess;
}

/****************************************************************************
 *
 *  Name:         BOOL DmaEnableChannel( UINT32 uDMACh )
 *
 *  Description:  Enables a DMA channel
 *
 *  Inputs:       UINT8 uDMACh = DMA channel number
 *
 *  Outputs:      Returns TRUE if successful, FALSE if not
 *
 ***************************************************************************/
BOOL DmaEnableChannel( UINT32 uDMACh )
{
   BOOL bSuccess = FALSE;

   if (DmaChannelTbl[ uDMACh ].bEnabled != TRUE)
   {
      /* Make sure DMA channel is completely reset before enabling */
      DmaResetChannel(uDMACh);

      /* Make sure the channel number is valid */
      if ( uDMACh < DMA_CHANNEL_MAX )
      {
         /* Need to enable DMA interface if applicable */
         switch (uDMACh)
         {
            case DMA_CHANNEL_EMAC1_TX:
            case DMA_CHANNEL_EMAC1_RX:
            case DMA_CHANNEL_EMAC2_TX:
            case DMA_CHANNEL_EMAC2_RX:
            case DMA_CHANNEL_M2M_IN:
            case DMA_CHANNEL_M2M_OUT:
               break;

		#if DSL_DMA
		    case DMA_CHANNEL_DSL_TX:
			   DSL_ClearStatus(DMA_CHANNEL_DSL_TX);
			   DSL_DMAEnable(DMA_CHANNEL_DSL_TX);
  			   DSL_EnableInterface(DMA_CHANNEL_DSL_TX);
               break;
            case DMA_CHANNEL_DSL_RX:
			   DSL_ClearStatus(DMA_CHANNEL_DSL_RX);
               DSL_DMAEnable(DMA_CHANNEL_DSL_RX);
  			   DSL_EnableInterface(DMA_CHANNEL_DSL_RX);
               break;
		#endif

            case DMA_CHANNEL_USB_TX_EP3:
            case DMA_CHANNEL_USB_TX_EP2:
            case DMA_CHANNEL_USB_TX_EP1:
            case DMA_CHANNEL_USB_TX_EP0:
            case DMA_CHANNEL_USB_RX_EP3:
            case DMA_CHANNEL_USB_RX_EP2:
            case DMA_CHANNEL_USB_RX_EP1:
            case DMA_CHANNEL_USB_RX_EP0:
               break;

            default:
               return bSuccess;
         }

         /* Need to enable DMA interrupt if applicable */

         /* Mark the channel as enabled */
         DmaChannelTbl[ uDMACh ].bEnabled = TRUE;
         bSuccess = TRUE;
      }
   }

   return bSuccess;
}

/****************************************************************************
 *
 *  Name:         BOOL DmaDisableChannel( UINT8 uDMACh )
 *
 *  Description:  Disables a DMA channel
 *
 *  Inputs:       UINT8 uDMACh = DMA channel number
 *
 *  Outputs:      Returns TRUE if successful, FALSE if not
 *
 ***************************************************************************/
BOOL DmaDisableChannel( UINT32 uDMACh )
{
   BOOL bSuccess = FALSE;

   /* Make sure the channel number is valid */
   if ( uDMACh < DMA_CHANNEL_MAX )
   {

      /* Need to disable DMA interface if applicable */
      switch (uDMACh)
      {
         case DMA_CHANNEL_EMAC1_TX:
         case DMA_CHANNEL_EMAC1_RX:
         case DMA_CHANNEL_EMAC2_TX:
         case DMA_CHANNEL_EMAC2_RX:
         case DMA_CHANNEL_M2M_IN:
         case DMA_CHANNEL_M2M_OUT:
            break;

	#if DSL_DMA
         case DMA_CHANNEL_DSL_TX:
            DSL_DMADisable(DMA_CHANNEL_DSL_TX);
			DSL_ClearStatus(DMA_CHANNEL_DSL_TX);
            break;
         case DMA_CHANNEL_DSL_RX:
            DSL_DMADisable(DMA_CHANNEL_DSL_RX);
			DSL_ClearStatus(DMA_CHANNEL_DSL_RX);
            break;
	#endif

         case DMA_CHANNEL_USB_TX_EP3:
         case DMA_CHANNEL_USB_TX_EP2:
         case DMA_CHANNEL_USB_TX_EP1:
         case DMA_CHANNEL_USB_TX_EP0:
         case DMA_CHANNEL_USB_RX_EP3:
         case DMA_CHANNEL_USB_RX_EP2:
         case DMA_CHANNEL_USB_RX_EP1:
         case DMA_CHANNEL_USB_RX_EP0:
            break;

         default:
            return bSuccess;
      }

      /* Mark the channel as disabled */
      DmaChannelTbl[ uDMACh ].bEnabled = FALSE;
      bSuccess = TRUE;
   }

   return bSuccess;
}

BOOL DmaOpenChannel(UINT32 uDMACh)
{
   BOOL bSuccess= FALSE;

   /* Make sure the channel number is valid */
   if ( uDMACh < DMA_CHANNEL_MAX )
   {
      /* For now always attempt to enable DMA channel. */
      /* Return SUCCESS regardless of whether DMA      */
      /* channel was enabled.                          */
      DmaEnableChannel(uDMACh);

      /* Mark the channel as open and enabled */
      DmaChannelTbl[ uDMACh ].bOpen = TRUE;

	  bSuccess= TRUE;
   }

   return (bSuccess);
}

/****************************************************************************
 *
 *  Name:         BOOL DmaCloseChannel( UINT8 uDMACh )
 *
 *  Description:  Closes a DMA channel
 *
 *  Inputs:       UINT8 uDMACh = DMA channel number
 *
 *  Outputs:      Returns TRUE if successful, FALSE if not
 *
 ***************************************************************************/
BOOL DmaCloseChannel( UINT32 uDMACh )
{
   BOOL bSuccess = FALSE;

   /* Make sure the channel number is valid */
   if ( uDMACh < DMA_CHANNEL_MAX )
   {
      bSuccess = DmaDisableChannel(uDMACh);

      /* Mark the channel as closed */
      if (bSuccess == TRUE)
         DmaChannelTbl[ uDMACh ].bOpen = FALSE;

   }

   return (bSuccess);
}


/****************************************************************************
 *
 *  Name:         BOOL DMASetCallback( UINT8 uDMACh, void ( *pDMACallback )( void ) )
 *
 *  Description:  Sets the callback function pointer for DMA interrupts
 *
 *  Inputs:       UINT8 uDMACh = DMA channel number
 *             void ( *pDMACallback )( void ) = callback function pointer
 *
 *  Outputs:      Returns TRUE if successful, FALSE if not
 *
 ***************************************************************************/
BOOL DMASetCallback( UINT8 uDMACh, PDMACALLBACK pDMACallback, void* pUserData )
{
   BOOL bSuccess;

   /* Make sure the channel number is valid */
   if ( uDMACh < DMA_CHANNEL_MAX )
   {
      /* Store the callback function pointer */
      DmaChannelTbl[ uDMACh ].pDMACallback = pDMACallback;
      DmaChannelTbl[ uDMACh ].pUserData = pUserData;

      bSuccess = TRUE;
   }

   else
   {
      bSuccess = FALSE;
   }

   return bSuccess;
}


EXPORT_SYMBOL(DMASetCallback);
EXPORT_SYMBOL(DmaOpenChannel);
EXPORT_SYMBOL(DmaChannelTbl);
#endif	//if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
