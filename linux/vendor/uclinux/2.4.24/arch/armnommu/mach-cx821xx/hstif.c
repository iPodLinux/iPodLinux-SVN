/****************************************************************************
*
*	Name:			hstif.c
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
*  $Modtime: 8/19/02 6:57p $
****************************************************************************/

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/
#include <asm/arch/bsptypes.h>
#include <asm/arch/hstif.h>

/****************************************************************************
 *
 *  Name:			void HSTInit( void )
 *
 *  Description:	Initializes the host interface
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void HSTInit( void )
{

	#if CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
		/* Read wait states */
		*pHST_RwstReg  = ( 7 << FLASH_WS ) |
						 ( 7 << HCS1_WS ) |
						 ( 17 << FALCON_WS ) |  // old 20  new 17
						 ( 7 << V90_WS ) |
						 ( 7 << UART_WS );

		/* Write wait states */
		*pHST_WwstReg  = ( 7 << FLASH_WS ) |
						 ( 7 << HCS1_WS ) |
						 ( 5 << FALCON_WS ) |	// old 20  new 5
						 ( 7 << V90_WS ) |
						 ( 7 << UART_WS );

		/* All chip selects use separate read and writes */
		*pHST_Xfer_CntlReg = 0;

		/* Read chip select setup and hold timers */
		*pHST_Read_Cntl1Reg = ( 2 << HCS1_TCH ) |
							  ( 2 << FALCON_TCH ) |  // old 15  new 2
							  ( 0 << V90_TCH ) |
							  ( 1 << UART_TCH ) |
							  ( 2 << HCS1_TCS ) |
							  ( 2 << FALCON_TCS ) |  // old 15  new 2
							  ( 0 << V90_TCS ) |
							  ( 1 << UART_TCS );

		/* Read address setup and hold timers */
		*pHST_Read_Cntl2Reg = ( 2 << HCS1_TAH ) |
							  ( 1 << FALCON_TAH ) | // old 15  new 1
							  ( 0 << V90_TAH ) |
							  ( 1 << UART_TAH ) |
							  ( 2 << HCS1_TAS ) |
							  ( 1 << FALCON_TAS ) | // old 15  new 1
							  ( 0 << V90_TAS ) |
							  ( 1 << UART_TAS );

		/* Write chip select setup and hold timers */
		*pHST_Write_Cntl1Reg = ( 2 << HCS1_TCH ) |
							  ( 2 << FALCON_TCH ) | // old 15  new 2
							  ( 0 << V90_TCH ) |
							  ( 1 << UART_TCH ) |
							  ( 2 << HCS1_TCS ) |
							  ( 2 << FALCON_TCS ) | // old 15  new 2
							  ( 0 << V90_TCS ) |
							  ( 1 << UART_TCS );

		/* Write address setup and hold timers */
		*pHST_Write_Cntl2Reg = ( 2 << HCS1_TAH ) |
							  ( 6 << FALCON_TAH ) | // old 15  new 6
							  ( 0 << V90_TAH ) |
							  ( 1 << UART_TAH ) |
							  ( 2 << HCS1_TAS ) |
							  ( 1 << FALCON_TAS ) | // old 15  new 1
							  ( 0 << V90_TAS ) |
							  ( 1 << UART_TAS );

	#else	// #ifdef CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE
	
		/* Read wait states */
		*pHST_RwstReg  = ( 7 << FLASH_WS ) |
				 ( 7 << HCS1_WS ) |
			       	 ( 7 << UART_WS );

		/* Write wait states */
		*pHST_WwstReg  = ( 7 << FLASH_WS ) |
				 ( 7 << HCS1_WS ) |
			       	 ( 7 << UART_WS );

		/* All chip selects use separate read and writes */
		*pHST_Xfer_CntlReg = 0;

		/* Read chip select setup and hold timers */
		*pHST_Read_Cntl1Reg = ( 2 << HCS1_TCH ) |
				      ( 1 << UART_TCH ) |
		                      ( 2 << HCS1_TCS ) |
				      ( 1 << UART_TCS );

		/* Read address setup and hold timers */
		*pHST_Read_Cntl2Reg = ( 2 << HCS1_TAH ) |
				      ( 1 << UART_TAH ) |
				      ( 2 << HCS1_TAS ) |
				      ( 1 << UART_TAS );

		/* Write chip select setup and hold timers */
		*pHST_Write_Cntl1Reg = ( 2 << HCS1_TCH ) |
				       ( 1 << UART_TCH ) |
				       ( 2 << HCS1_TCS ) |
				       ( 1 << UART_TCS );

		/* Write address setup and hold timers */
		*pHST_Write_Cntl2Reg = ( 2 << HCS1_TAH ) |
				       ( 1 << UART_TAH ) |
				       ( 2 << HCS1_TAS ) |
				       ( 1 << UART_TAS );
	#endif // #ifdef CONFIG_CNXT_ADSL || CONFIG_CNXT_ADSL_MODULE #else

	return;
}


