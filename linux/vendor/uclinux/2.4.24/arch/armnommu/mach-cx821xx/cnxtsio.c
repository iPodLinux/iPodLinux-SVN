/****************************************************************************
****	Copyright (c) 2001, 2002
****	Conexant Systems Inc. (formerly Rockwell Semiconductor Systems)
****
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

****************************************************************************/
/***************************************************************************
**
**	MODULE NAME:
**		cnxtsio (Conexant Serial Input/Output)
**
**	FILE NAME:            
**		cnxtsio.c
**
**	ABSTRACT:
**		This serial driver is totally invisible to Linux. It is used solely by
**		the ADSL DMT Dumb and Smart Terminal debug interfaces. It interfaces to
**		the BSP via sysserial.c.
**
*******************************************************************************/

/******************************************************************************
** $Archive: /Test/BSP/Lineo2.4.6/linux/arch/armnommu/mach-cx821xx/cnxtsio.c $
** $Revision: 1.1 $
** $Date: 2003/08/28 11:41:23 $
*******************************************************************************/



//*********************************
// Includes
//*********************************
#include	<linux/module.h>
#include	<linux/init.h>

#if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
#include <asm/errno.h>
#include "bsptypes.h"
#include "bspcfg.h"
#include "syslib.h"
#include "cnxtbsp.h"
#include "cnxtsio.h"
#include "OsTools.h"
#include "cnxttimer.h"
#include "cnxtirq.h"



//*********************************
// Externs
//*********************************
extern CNXT_UART spUartChan[1];



//*********************************
// Register Addresses
//*********************************
//	Read Mode
#define Ser_RHR					0x0		// Rx Hold Register (FIFO)
#define Ser_ISR					0x2		// Interrupt Status Register
#define	Ser_LSR					0x5		// Line Status Register
#define	Ser_MSR					0x6		// Modem Status Register

// Write Mode
#define Ser_THR 				0x0		// Tx Hold Register (FIFO)
#define Ser_FCR					0x2		// FIFO Control Register

//	Read/Write Mode
#define Ser_DLL					0x0		// Baud Rate Generator LSB (LCR & 0x80 == 0x80
#define Ser_DLM					0x1		// Baud Rate Generator MSB (LCR & 0x80 == 0x80
#define Ser_IER					0x1		// Interrupt Enable Register
#define Ser_LCR					0x3		// Line Control Register
#define Ser_MCR					0x4		// Modem Control Register
#define Ser_SPR					0x7		// Scratchpad Register



//*********************************
// Register Masks
//*********************************

// Interrupt Enable Register masks
#define IER_RDR					0x01
#define IER_THR					0x02

// Interrupt Status Register masks
#define ISR_REG_MASK			0x0f
#define ISR_RDA					0x04
#define ISR_RTO					0x0C
#define ISR_THR					0x02
#define ISR_NONE				0x01

// Interrupt Status Register masks

// Line Status Register masks
#define LSR_THE					0x20
#define LSR_RDA					0x01
#define LSR_OVRN				0x02



//*********************************
// Data Configuration values
//*********************************

// Baud Rate
#define BAUD_RATE				57600
#define MSB_BAUD				((115200/BAUD_RATE)>>8)
#define LSB_BAUD				((115200/BAUD_RATE)&0xFF)
#define LCR_DIVISOR_LATCH		0x80

// Parity and Word Length
#define LCR_8N1					0x03



//*********************************
// Register Access Macros
//*********************************
#define SPUART_REG_READ(pChan, reg, result) \
	result = (*(volatile unsigned short *)((UINT32)pChan->regs + (reg*pChan->regAddr)))

#define SPUART_REG_WRITE(pChan, reg, data) \
	(*(volatile unsigned short *)((UINT32)pChan->regs + (reg*pChan->regAddr)))	 = (data)




//*********************************
// Local Prototypes
//*********************************
BOOLEAN uart_fifo_get_char
(
	UART_FIFO_T	*FifoCtx,
	BYTE		*charbuf	// Place to put data retrieved from the fifo
);

UINT16 uart_fifo_put_chars
(
	UART_FIFO_T	*FifoCtx,
	BYTE		*charbuf,	// Pointer to data to be put on fifo
	UINT16		charbufsize	// Number of characters to be put on fifo
) ;



/*******************************************************************************
*
* spUartDevInit - initialise an SPUART channel
*
* This routine initialises some CNXT_SIO function pointers and then resets
* the chip in a quiescent state.  Before this routine is called, the BSP
* must already have initialised all the device addresses, etc. in the
* CNXT_UART structure.
*
* RETURNS: N/A
*******************************************************************************/
eUARTType spUartDevInit ( CNXT_UART *pUARTChan )
{
	static eUARTType spUartInitChannel(CNXT_UART *pUARTChan)  ;

	eUARTType eStatus;

	UINT32 oldlevel = intLock();

	HWSTATE.g_pUARTChan = pUARTChan;

	/* reset the chip */
	eStatus = spUartInitChannel(pUARTChan);

	pUARTChan->xmtr_uart_fifo.get_idx = 0;
	pUARTChan->xmtr_uart_fifo.put_idx = 0;
	pUARTChan->xmtr_uart_fifo.entry_cnt = 0;

	pUARTChan->rcvr_uart_fifo.get_idx = 0;
	pUARTChan->rcvr_uart_fifo.put_idx = 0;
	pUARTChan->rcvr_uart_fifo.entry_cnt = 0;

	intUnlock(oldlevel);

	return(eStatus);
}

/*******************************************************************************
*
* spUartInitChannel - initialise UART
*
* This routine performs hardware initialisation of the UART channel.
*
* RETURNS: N/A
*******************************************************************************/

static eUARTType spUartInitChannel(CNXT_UART *pUARTChan)
{
	UINT8	ichar;
	UINT32	i;

	/* Check to see if external UART is present */
	SPUART_REG_READ(pUARTChan,Ser_LSR, ichar);
	if ( (ichar == 0xFF) || ( (ichar & 0x60) != 0x60 ) )
	{
		return(NO_UART);
	}

	/* Enable and reset FIFO */
	// Rx FIFO interrupt at 8 chars (.694 ms at 115200 bps)
	SPUART_REG_WRITE(pUARTChan, Ser_FCR, 0x87);

	/* Set up baud rate and character format */
	SPUART_REG_WRITE(pUARTChan, Ser_LCR, LCR_DIVISOR_LATCH);
	SPUART_REG_WRITE(pUARTChan, Ser_DLL, LSB_BAUD);
	SPUART_REG_WRITE(pUARTChan, Ser_DLM, MSB_BAUD);
	SPUART_REG_WRITE(pUARTChan, Ser_LCR, LCR_8N1);

	/* Empty out Rx FIFO of junk */
	for (i = 60000; i > 0; i--)
	{
		SPUART_REG_READ(pUARTChan, Ser_LSR, ichar);
		if((ichar & LSR_RDA) == 0x00)
		{
			break;
		}

		SPUART_REG_READ(pUARTChan,Ser_RHR, ichar);
	}

	// Did we fail?
	if (i == 0)
	{
		return (NO_UART);
	}

	// Configure Modem Control Reg for DTR=0, RTS=0, 
	SPUART_REG_WRITE(pUARTChan, Ser_MCR, 0x0B);

	// Read Line Status Reg because ...?
	SPUART_REG_READ(pUARTChan, Ser_LSR, ichar);
	
	// Enable RX interrupt
	SPUART_REG_WRITE(pUARTChan, Ser_IER, IER_RDR);

	return(HW_UART);
}


/******************************************************************************
*
* spUartInt - interrupt level processing
*
* This routine handles interrupts from the UART.
*
* RETURNS: N/A
******************************************************************************/
void spUartInt(CNXT_UART *pUARTChan)	/* ptr to struct describing channel */
{

	UINT8	intStatus;

	// Get the copy of the uart ISR register
	SPUART_REG_READ( pUARTChan, Ser_ISR, intStatus );
	intStatus &= ISR_REG_MASK;

	while ( intStatus != ISR_NONE )
	{
		/* process all pending interrupts */

		// NOTE: The first character write to the tx
		// fifo should clear out the ISR_THR bit.
		if ( intStatus == ISR_THR )
		{
			char	outChar;
			UINT32	i;

			for ( i = 0; i < 16; i++)
			{
				// Get a character to transmit from the fifo
				if ( uart_fifo_get_char (&pUARTChan->xmtr_uart_fifo, &outChar) == TRUE )
				{
					SPUART_REG_WRITE( pUARTChan, Ser_THR, outChar);
				}
				else
				{
					// Since there are no more characters in the xmit
					// fifo to transmit then disable the xmit fifo empty
					// interrupt bit because we are not concerned now
					// when it goes empty.	The next time characters are
					// written to the fifo it will be reenabled.

					UINT8	IERStatus = 0;

					SPUART_REG_READ( pUARTChan, Ser_IER, IERStatus );
					IERStatus &= ~IER_THR;
					SPUART_REG_WRITE(pUARTChan, Ser_IER, IERStatus );

					// No more characters in the xmtr fifo so
					// exit the for loop now.
					break;
				}
			}
		}

		// if RX Data Ready or Rx Data Timeout
		else if( (intStatus == ISR_RDA) || (intStatus == ISR_RTO) )
		{
			#define SIZE_OF_RXBUF 20 // HW FIFO size + a few spares
			char RxBuf [SIZE_OF_RXBUF] ;
			UINT16 CharCnt = 0 ;
			UINT8 lsrStatus ;
			char rcvChar ;

			// Do forever
			while (1)
			{
				// if there is RX FIFO overrun
				SPUART_REG_READ(pUARTChan, Ser_LSR, lsrStatus);
				if ( ( lsrStatus & LSR_OVRN ) != 0x00 )
				{
					printk ("ADSL Serial Port Overrun!\n" ) ;
				}

				// if there is NO data in RX FIFO
				if ( (lsrStatus & LSR_RDA) == 0x00 )
				{
					// exit loop
					break;
				}

				// Read a Char
				SPUART_REG_READ(pUARTChan, Ser_RHR, rcvChar);
				
				// Stuff in local Rx buffer
				RxBuf [CharCnt++] = rcvChar ;
				
				// If local Rx buffer full
				if ( CharCnt == SIZE_OF_RXBUF )
				{
					uart_fifo_put_chars ( &pUARTChan->rcvr_uart_fifo, &RxBuf[0], CharCnt ) ;
					CharCnt = 0 ;
				}
			}
			
			// If we still have unqueued RX Data
			if ( CharCnt != 0 )
			{
				// Queue it
				uart_fifo_put_chars ( &pUARTChan->rcvr_uart_fifo, &RxBuf[0], CharCnt ) ;
			}
		}
		else
		{
			printk ("Unknown Status: %x\n", intStatus ) ;
		}

		// read ISR for next iteration
		SPUART_REG_READ( pUARTChan, Ser_ISR, intStatus);
		intStatus &= ISR_REG_MASK;
	}
}



/*******************************************************************************

FUNCTION NAME:
		uart_fifo_get_char

ABSTRACT:
		Get a character from the fifo

DETAILS:
		Returns TRUE if successful, FALSE if not successful.

*******************************************************************************/

BOOLEAN uart_fifo_get_char
(
	UART_FIFO_T	*FifoCtx,
	BYTE		*charbuf	// Place to put data retrieved from the fifo
)
{
	// Protect the fifo manipulation operations from interrupts
	UINT32 oldlevel = intLock();

	// Is there data on the fifo?
	if ( FifoCtx->entry_cnt > 0 )
	{
		// Get data from the fifo context given to us
		*charbuf = FifoCtx->fifo[ FifoCtx->get_idx++ ];

		// Process the get index to account for queue wrapping
		if(	FifoCtx->get_idx >= SIZE_OF_UART_FIFO )
		{
			FifoCtx->get_idx = 0;
		}

		// Indicate an entry was removed from the queue
		FifoCtx->entry_cnt--;

		// Response successfully dequeued
		intUnlock(oldlevel);
		return TRUE;
	}
	else
	{
		// No response to remove from the queue.
		intUnlock(oldlevel);
		return FALSE;
	}
}


/*******************************************************************************

FUNCTION NAME:
		uart_fifo_put_char

ABSTRACT:
		Put characters into the fifo

DETAILS:
		Returns number of characters not put on fifo.

*******************************************************************************/

UINT16 uart_fifo_put_chars
(
	UART_FIFO_T	*FifoCtx,
	BYTE		*charbuf,	// Pointer to data to be put on fifo
	UINT16		charbufsize	// Number of characters to be put on fifo
)
{
	UINT32	oldlevel;
	CHAR	*pbuf;
	UINT16	index;
	UINT16	numcharsput;

	// Protect the fifo manipulation operations from interrupts
	oldlevel = intLock();

	pbuf = charbuf;

	// Is there room for all of the data on the queue?
	if ( (SIZE_OF_UART_FIFO - FifoCtx->entry_cnt) > charbufsize )
	{
		index = charbufsize;
		numcharsput = charbufsize;
	}
	else
	{
		index = (SIZE_OF_UART_FIFO - FifoCtx->entry_cnt);
		numcharsput = index;
	}

	while( index > 0 )
	{
		// Put the character into the fifo
		FifoCtx->fifo[ FifoCtx->put_idx++ ] = *pbuf++;

		// Process the put index to account for queue wrapping
		if(	FifoCtx->put_idx >= SIZE_OF_UART_FIFO )
		{
			FifoCtx->put_idx = 0;
		}

		// Indicate an entry was placed on the queue
		FifoCtx->entry_cnt++;
		index--;
	}

	// All of some of the characters successfully put on fifo
	intUnlock(oldlevel);
	return numcharsput;
}


/*******************************************************************************

FUNCTION NAME:
		uart_xmtr_fifo_put_chars

ABSTRACT:
		Put characters into the xmtr fifo

DETAILS:
		Returns TRUE if successful, FALSE if not successful.

*******************************************************************************/

BOOLEAN uart_xmtr_fifo_put_chars
(
	BYTE		*charbuf,	// Pointer to data to be put on fifo
	UINT16		charbufsize	// Number of characters to be put on fifo
)
{

	BYTE		*pChar;
	UINT16		numcharsput;
	UINT16		numcharsleft;
	CNXT_UART	*pUARTChan = &spUartChan[0];

	numcharsleft = charbufsize;
	pChar = charbuf;

	while( 1 )
	{
		UINT8 IERStatus ;

		// Put the characters into the transmitter fifo
		numcharsput = uart_fifo_put_chars( &pUARTChan->xmtr_uart_fifo, pChar, numcharsleft );

		//adjust the character pointer and the character count.
		pChar += numcharsput;
		numcharsleft -= numcharsput;

		// Is the transmit interrupt disabled?
		// (It gets disabled in the ISR when it empties the FIFO)
		if
		(
			( HW_UART == HWSTATE.eUARTState)
		&&
			
			( ! (SPUART_REG_READ( pUARTChan, Ser_IER, IERStatus ) & IER_THR ) )
		)
		{
			// Put the first character into the uart
			// to start the interrupt driven draining process

			UINT8 IERStatus ;
			char outChar ;

			// (There may be no chars if ISR has already drained the FIFO between
			// the time we put them in and the time we reach this statement)
			if ( uart_fifo_get_char(&pUARTChan->xmtr_uart_fifo, &outChar) )
			{
				// output first char
				SPUART_REG_WRITE(pUARTChan, Ser_THR, outChar);

				// Reenable tx interrupt since ISR disables it when it
				// empties software FIFO and that must be the case if the
				// HW TX FIFO (aka Tx Hold Reg) is empty.
				SPUART_REG_READ( pUARTChan, Ser_IER, IERStatus );
				IERStatus |= IER_THR;
				SPUART_REG_WRITE(pUARTChan, Ser_IER, IERStatus );
			}
		}

		// Did the xmtr fifo accept all of the characters?
		if ( numcharsleft == 0)
		{
			// Then we are through.
			return TRUE;
		}

		// It did not so delay a while
		// and try again.
		TaskDelayMsec(10);
	}
}


/*******************************************************************************

FUNCTION NAME:
		uart_rcvr_fifo_get_char

ABSTRACT:
		Gets a character from the rcvr fifo

DETAILS:
		Returns TRUE if there was a character, FALSE if not.

*******************************************************************************/

BOOLEAN uart_rcvr_fifo_get_char
(
	BYTE		*charbuf	// Place to put character from fifo
)
{
	CNXT_UART	*pUARTChan = &spUartChan[0];
	int		status;
	BYTE	data;

	status = uart_fifo_get_char ( &pUARTChan->rcvr_uart_fifo, &data ) ;

	if ( status )
	{
		 // We got a character.
		 *charbuf = data;
		 return TRUE;
	}
	else
	{
		// Either there was no received characters
		// OR an error occured.
		return FALSE;
	}
}

EXPORT_SYMBOL(uart_xmtr_fifo_put_chars);
EXPORT_SYMBOL(uart_rcvr_fifo_get_char);

#endif	//if defined (CONFIG_CNXT_ADSL)  ||  defined (CONFIG_CNXT_ADSL_MODULE)
