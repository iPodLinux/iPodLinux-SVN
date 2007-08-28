/****************************************************************************
*
*	Name:			eeprom.c
*
*	Description:	EEPROM access routines
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
*  $Modtime: 8/06/02 7:22a $
****************************************************************************/

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/

#include <linux/ptrace.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/arch/bspcfg.h>
#include <asm/arch/bsptypes.h>
#include <asm/arch/gpio.h>
#include <asm/arch/usbdef.h>
#include "eeprom.h"
#include "defeepromdata.h"
#include <asm/arch/OsTools.h>
#include <asm/arch/cnxttimer.h>

//#include "typedef.h"

/* EEPROM data table containing USB enumeration infos */
/* Holds all the tables read from the EEPROM */
UINT8 EEPROMTbl[ EEPROM_TBL_SIZE_DB ];

/* Pointers to tables within EERPOM table */
UINT8* Dev_Desc_Tbl;			/* Device descriptor data */
UINT8* Dev_Cfg_Tbl;          /* Device configuration data */
UINT8* LanguageID_Str_Tbl;	/* Language ID */
UINT8* Manufacturer_Str_Tbl;	/* Manufacturer string */
UINT8* Product_Str_Tbl;		/* Product String */
UINT8* Serial_Str_Tbl;			/* Serial Number String */
UINT8* Mac_Address_Str_Tbl;  /* Mac Address String */

UINT32 HostSigValue;


/*---------------------------------------------------------------------------
 *  Module Function Prototypes
 *-------------------------------------------------------------------------*/
void InitI2CEEPROMIf( void );
void IssueSTART( void );
void IssueSTOP( void );
void IssueACK( void );
BOOL WaitForACK( void );
void WriteEEPROMByte( UINT8 uData );
void WriteEEPROMBit( BOOL bBit );
UINT8 ReadEEPROMByte( void );
BOOL ReadEEPROMBit( void );

/****************************************************************************
 *
 *  Name:			BOOL ReadEEPROM( UINT8 uSlaveAddr, 
 *									 UINT8 uWordAddr, 
 *									 UINT8* pRAMAddr, 
 *									 UINT16 uBytes )
 *
 *  Description:	Reads data from the EEPROM and stores it in RAM
 *
 *  Inputs:			UINT8 uSlaveAddr = EEPROM slave address (should be 1010000 for Euphrates)
 *					UINT8 uWordAddr = Starting address of data in the EEPROM
 *					UINT8* pRAMAddr = Starting address of where to store the data in RAM
 *					UINT16 uBytes = Number of bytes to read
 *
 *  Outputs:		Returns TRUE if successful, FALSE if not
 *
 ***************************************************************************/
BOOL ReadEEPROM( UINT8 uSlaveAddr, UINT8 uWordAddr, UINT8* pRAMAddr, UINT16 uBytes )
{
	BOOL bSuccess;

	/* Assume we fail */
	bSuccess = FALSE;

	/* according to EEPROM datasheet, slave address must be 1010xxyz, where */
	/*  x is address on board     */
	/*  y is page block address   */
	/*  z is read/write bit       */
	/*assert( 0xA0 == (uSlaveAddr & 0xF0) );
	assert( uBytes <= PAGE_BLOCK_BYTES );*/

	/* Init. the GPIO pins used for the serial interface */
	InitI2CEEPROMIf();

	/* Issue the "START" command */
	IssueSTART();

	/* Write the device slave address, as a dummy write cycle */
	WriteEEPROMByte( uSlaveAddr & EEPROM_WRITE );

	/* Wait for the device to ACK */
	if ( WaitForACK() )
	{
		/* Write the starting word address */
		WriteEEPROMByte( uWordAddr );

		/* Wait for the device to ACK */
		if ( WaitForACK() )
		{
			/* Issue the "START" command for the random read procedure */
			IssueSTART();

			/* Write the device slave address, for reading this time */
			WriteEEPROMByte( uSlaveAddr | EEPROM_READ );

			/* Wait for the device to ACK */
			if ( WaitForACK() )
			{
				/* While we haven't read all the desired bytes */
				while ( uBytes )
				{
					/* Read a byte from the EEPROM and store it */
					*pRAMAddr = ReadEEPROMByte();

					/*  ACK the byte except for the last one */
					if ( 1 != uBytes )
						IssueACK();

					/* Update the RAM address and byte counter */
					++pRAMAddr;
					--uBytes;
				}

				/* Tell the EEPROM we're done */
				IssueSTOP();

				/* Flag a success */
				bSuccess = TRUE;
			}
		}
	}

	return bSuccess;
}

/****************************************************************************
 *
 *  Name:			BOOL ProgramEEPROM( UINT8 uSlaveAddr, UINT8* pRAMAddr, UINT16 uBytes )
 *
 *  Description:	Programs an EEPROM page block.  Page block number is 
 *					spec'd in the slave address.
 *
 *  Inputs:			UINT8 uSlaveAddr = EEPROM slave address (should be 1010111 for Euphrates)
 *					UINT8* pRAMAddr = Starting address in RAM of where to get the data
 *					UINT16 uBytes = Number of bytes to write
 *
 *  Outputs:		Returns TRUE if successful, FALSE if not
 *
 ***************************************************************************/
BOOL ProgramEEPROM( UINT8 uSlaveAddr, UINT8* pRAMAddr, UINT16 uBytes )
{
	BOOL bSuccess;		/* Whether it worked or not */
	UINT8 uWordAddr;	/* Address of data in the EEPROM page block */
	UINT8 uPageBytes;	/* Number of bytes in the current page to program */

	bSuccess = TRUE;

	/*assert( uBytes <= PAGE_BLOCK_BYTES );*/

	/* Start from the beginning of the EEPROM */
	uWordAddr = 0;

	/* While there are bytes to program and there's been no error */
	while( uBytes && bSuccess )
	{
		/* See if there's a full page left to program */
		if ( uBytes >= 16 )
		{
			uPageBytes = 16;
		}

		else
		{
			uPageBytes = uBytes;
		}

		/* Program a page */
		bSuccess = WriteEEPROM( uSlaveAddr, uWordAddr, pRAMAddr, uPageBytes );

		/* If the page was programmed correctly... */
		if ( bSuccess )
		{
			/* Update the byte count and addresses */
			uWordAddr += uPageBytes;
			pRAMAddr += uPageBytes;
			uBytes -= uPageBytes;

			/* Delay 15 msec (15000 microsec ) until the EEPROM is ready */
			sysTimerDelay( WRITE_CYCLE_TIME );
		}
	}
	
	return bSuccess;
}

/****************************************************************************
 *
 *  Name:			BOOL WriteEEPROM( UINT8 uSlaveAddr, 
 *									 UINT8 uWordAddr, 
 *									 UINT8* pRAMAddr, 
 *									 UINT8 uBytes )
 *
 *  Description:	Writes data to the EEPROM from RAM
 *
 *  Inputs:			UINT8 uSlaveAddr = EEPROM slave address (should be 1010111 for Euphrates)
 *					UINT8 uWordAddr = Starting address of data in the EEPROM
 *					UINT8* pRAMAddr = Starting address in RAM of where to get the data
 *					UINT8 uBytes = Number of bytes to write
 *
 *  Outputs:		Returns TRUE if successful, FALSE if not
 *
 ***************************************************************************/
BOOL WriteEEPROM( UINT8 uSlaveAddr, UINT8 uWordAddr, UINT8* pRAMAddr, UINT8 uBytes )
{
	BOOL bSuccess;

	/* Assume we fail */
	bSuccess = FALSE;

	/* according to EEPROM datasheet, slave address must be 1010xxyz, where */
	/*  x is address on board */
	/*  y is page block address */
	/*  z is read/write bit */
	/*assert( 0xA0 == (uSlaveAddr & 0xF0) );*/

	/* only allow 16 bytes max since this is limit of page write mode */
	/*assert( uBytes <= 16 );*/

	/* Init. the GPIO pins used for the serial interface */
	InitI2CEEPROMIf();

	/* Issue the "START" command */
	IssueSTART();

	/* Write the device slave address */
	WriteEEPROMByte( uSlaveAddr & EEPROM_WRITE );

	/* Wait for the device to ACK */
	if ( WaitForACK() )
	{
		/* Write the starting word address */
		WriteEEPROMByte( uWordAddr );

		/* Wait for the device to ACK */
		if ( WaitForACK() )
		{
			/* While we haven't written all the desired bytes */
			while ( uBytes )
			{
				/* Write a byte to the EEPROM */
				WriteEEPROMByte( *pRAMAddr );

				/* Wait for the device to ACK */
				if ( WaitForACK() ==  FALSE )
				{
					uBytes=0;	/* force loop exit with bSuccess remaining FALSE */
				}
				else
				{
				 	/* Update the RAM address and byte counter */
					++pRAMAddr;
					--uBytes;
					bSuccess = TRUE;
				}
			}
			/* Tell the EEPROM we're done */
			IssueSTOP();
		}
	}

	return bSuccess;
}

/****************************************************************************
 *
 *  Name:			void InitEEPROMIf( void )
 *
 *  Description:	Initializes the pins used as a serial interface to the EEPROM
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void InitI2CEEPROMIf( void )
{
	/* Drive the clock high and make it an output */
	WriteGPIOData( SCL, 1 );
	SetGPIODir( SCL, GP_OUTPUT );

   /* This ugly sequence is to make sure that the EEPROM*/
   /* is forced back to know default state. */
	/* Issue 7 clk pulses, then IssueSTOP. */
	sysTimerDelay( SCL_HIGH_TIME );
	WriteGPIOData ( SCL, 0 );
	sysTimerDelay( SCL_LOW_TIME );
	WriteGPIOData( SCL, 1 );

	sysTimerDelay( SCL_HIGH_TIME );
	WriteGPIOData ( SCL, 0 );
	sysTimerDelay( SCL_LOW_TIME );
	WriteGPIOData( SCL, 1 );

	sysTimerDelay( SCL_HIGH_TIME );
	WriteGPIOData ( SCL, 0 );
	sysTimerDelay( SCL_LOW_TIME );
	WriteGPIOData( SCL, 1 );

	sysTimerDelay( SCL_HIGH_TIME );
	WriteGPIOData ( SCL, 0 );
	sysTimerDelay( SCL_LOW_TIME );
	WriteGPIOData( SCL, 1 );

	sysTimerDelay( SCL_HIGH_TIME );
	WriteGPIOData ( SCL, 0 );
	sysTimerDelay( SCL_LOW_TIME );

	WriteGPIOData( SCL, 1 );
	sysTimerDelay( SCL_HIGH_TIME );
	WriteGPIOData ( SCL, 0 );
	sysTimerDelay( SCL_LOW_TIME );
	WriteGPIOData( SCL, 1 );

	sysTimerDelay( SCL_HIGH_TIME );
	WriteGPIOData ( SCL, 0 );
	sysTimerDelay( SCL_LOW_TIME );
	WriteGPIOData( SCL, 1 );
 
	IssueSTOP ();

	/* Make sure the data pin an input for now */
	SetGPIODir( SDA, GP_INPUT );

	return;
}
/****************************************************************************
 *
 *  Name:			void IssueSTART( void )
 *
 *  Description:	Issue a START command to the EEPROM
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void IssueSTART( void )
{
	/* Drive SCL high */
	WriteGPIOData( SCL, 1 );

	/* Drive SDA high */
	WriteGPIOData( SDA, 1 );
	
	/* Make SDA an output */
	SetGPIODir( SDA, GP_OUTPUT );

	/* SysTimerDelay for START setup time */
	sysTimerDelay( START_SETUP_TIME );

	/* Drive SDA low */
	WriteGPIOData( SDA, 0 );

	/* SysTimer1Delay for START hold time  */
	sysTimerDelay( START_HOLD_TIME );
	
	/* Drive SCL low */
	WriteGPIOData( SCL, 0 );

	return;
}

/****************************************************************************
 *
 *  Name:			void IssueSTOP( void )
 *
 *  Description:	Issue a STOP command to the EEPROM
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void IssueSTOP( void )
{
	/* Drive SCL low */
	WriteGPIOData( SCL, 0 );

	/* Make SDA an output */
	SetGPIODir( SDA, GP_OUTPUT );

	/* Drive SDA low */
	WriteGPIOData( SDA, 0 );
	
	/* SysTimer1Delay for STOP setup time */
	sysTimerDelay( STOP_SETUP_TIME );

	/* Drive SCL high */
	WriteGPIOData( SCL, 1 );

	/* SysTimer1Delay for STOP setup time */
	sysTimerDelay( STOP_SETUP_TIME );

	/* Drive SDA high */
	WriteGPIOData( SDA, 1 );

	/* SysTimer1Delay for write cycle time */
	sysTimerDelay( WRITE_CYCLE_TIME );
	
	return;
}

/****************************************************************************
 *
 *  Name:			void IssueACK( void )
 *
 *  Description:	Issue an ACK to the EEPROM
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void IssueACK( void )
{
	/* Write a 0 out to the EEPROM */
	WriteEEPROMBit( 0 );

	return;
}

/****************************************************************************
 *
 *  Name:			BOOL WaitForACK( void )
 *
 *  Description:	Waits for an ACK from the EEPROM
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns TRUE if ACK received, FALSE if not
 *
 ***************************************************************************/
BOOL WaitForACK( void )
{
	BOOL bSuccess;

	/* Read a bit from the EEPROM and check that it's a 0 */
	if ( !ReadEEPROMBit() )
	{
		bSuccess = TRUE;
	}

	else
	{
		bSuccess = FALSE;
	}

	return bSuccess;
}

/****************************************************************************
 *
 *  Name:			void WriteEEPROMByte( UINT8 uData )
 *
 *  Description:	Writes a byte out to the EEPROM
 *
 *  Inputs:			UINT8 uData = byte to write
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void WriteEEPROMByte( UINT8 uData )
{
	UINT8 uBitCnt;

	for ( uBitCnt = 8; uBitCnt > 0; uBitCnt-- )
	{
		/* Data is written MSB first */
		if ( uData & 0x80 )
		{
			WriteEEPROMBit( 1 );
		}

		else
		{
			WriteEEPROMBit( 0 );
		}

		uData <<= 1;
	}

	return;
}

/****************************************************************************
 *
 *  Name:			void WriteEEPROMBit( BOOL bBit )
 *
 *  Description:	Writes a bit out to the EEPROM
 *
 *  Inputs:			BOOL bBit = bit to write
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void WriteEEPROMBit( BOOL bBit )
{
	/* SysTimer1Delay 1/2 clock low time */
	sysTimerDelay( SCL_LOW_TIME_DIVIDEBY2 );

	/* Make SDA an output */
	SetGPIODir( SDA, GP_OUTPUT );

	/* Drive SDA to the desired bit state */
	WriteGPIOData( SDA, bBit );

	/* SysTimer1Delay 1/2 clock low time  */
	sysTimerDelay( SCL_LOW_TIME_DIVIDEBY2 );
	
	/* Drive SCL high */
	WriteGPIOData( SCL, 1 );

	/* SysTimer1Delay clock high time */
	sysTimerDelay( SCL_HIGH_TIME );

	/* Drive SCL low */
	WriteGPIOData( SCL, 0 );

	return;
}

/****************************************************************************
 *
 *  Name:			UINT8 ReadEEPROMByte( void )
 *
 *  Description:	Reads a byte from the EEPROM
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns UINT8 the byte read
 *
 ***************************************************************************/
UINT8 ReadEEPROMByte( void )
{
	UINT8 uData;
	UINT8 uBitCnt;

	uData = 0;

	for ( uBitCnt = 8; uBitCnt > 0; uBitCnt-- )
	{
		/* Data is read MSB first */
		uData <<= 1;

		/* Read a bit and put it into the byte  */
		uData |= ReadEEPROMBit();
	}

	return uData;
}

/****************************************************************************
 *
 *  Name:			BOOL ReadEEPROMBit( void )
 *
 *  Description:	Reads a bit from the EEPROM
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns BOOL bit read
 *
 ***************************************************************************/
BOOL ReadEEPROMBit( void )
{
	BOOL bBit;

	/* Make SDA an input */
	SetGPIODir( SDA, GP_INPUT );

	/* SysTimer1Delay clock low time */
	sysTimerDelay( SCL_LOW_TIME );

	/* Drive SCL high */
	WriteGPIOData( SCL, 1 );

	/* Wait since this routine used for sensing acknowledge */
	sysTimerDelay( SCL_HIGH_TIME );

	/* Bit value = SDA state */
	bBit = ReadGPIOData( SDA );

	/* Drive SCL low */
	WriteGPIOData( SCL, 0 );

	/* SysTimer1Delay clock high time */
	/*sysTimerDelay( SCL_HIGH_TIME ); */

	return bBit;
}
/****************************************************************************
 *
 *  Name:			BOOL EEPROMTest ( void )
 *
 *  Description:	Test for verifying writing/reading of EEPROM using GPIOs
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns BOOL write/read success
 *
 ***************************************************************************/
 BOOL EEPROMTest( void )
{
	BOOL   bSuccess;
	UINT8  uSlaveAddr, uWordAddr, uBytes, i;
	static UINT8 uReadData[NUM_TEST_BYTES];
	static UINT8 uWriteData[NUM_TEST_BYTES];

	bSuccess = FALSE;

	/********************************
	 * Write/read to the last addresses so as not to overwrite potential data.
	 *  EEPROM size is in EEPROM.h and is a byte address.
	 *******************************/

	/* Use first page - page block number is bit 7 of the slave address, 2K page size */
	uSlaveAddr = SLAVE_ADDRESS;
	uWordAddr = EEPROM_ADDRESS - NUM_TEST_BYTES;
	uBytes = NUM_TEST_BYTES;

	/* initialize write and read buffers   */
	for (i=0; i<NUM_TEST_BYTES; i++)
	{
		uWriteData[i] = 0x55 + i;
		uReadData[i] = 0;
	}
	

	/* Write and read data, then do the same to verify the write was successful */
	if ( WriteEEPROM( uSlaveAddr, uWordAddr, &uWriteData[0], uBytes ) == TRUE )
	{
	 	if ( ReadEEPROM( uSlaveAddr, uWordAddr, &uReadData[0], uBytes ) == TRUE )
		{
			/* entire test is successful if gotten to this point.  Assume data matches. */
		  	bSuccess = TRUE;
		  	for (i=0; i<NUM_TEST_BYTES; i++)
			{
				if (uWriteData[i] != uReadData[i])
				{
					bSuccess = FALSE;
				}
			}
		}
	}

	if ( TRUE == bSuccess )
	{
		/* assume second set of write/reads fail */
		bSuccess = FALSE;
		/* initialize write and read buffers */
		for (i=0; i<NUM_TEST_BYTES; i++)
		{
			uWriteData[i] = 0xAA + i;
			uReadData[i] = 0;
		}

		if ( WriteEEPROM( uSlaveAddr, uWordAddr, &uWriteData[0], uBytes ) == TRUE )
		{
		 	if ( ReadEEPROM( uSlaveAddr, uWordAddr, &uReadData[0], uBytes ) == TRUE )
			{
				/* entire test is successful if gotten to this point.  Assume data matches. */
			  	bSuccess = TRUE;
			  	for (i=0; i<NUM_TEST_BYTES; i++)
				{
					if (uWriteData[i] != uReadData[i])
					{
						bSuccess = FALSE;
					}
				}
			}
		}
	}
	return bSuccess;
}


 /****************************************************************************
 *
 *  Name:			static void InitEEPROM(UINT8* pRomTable)
 *
 *  Description:	Programs the EEPROM with the device descriptor and configuration
 *					tables.
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void InitEEPROM(UINT8* pRomTable)
{
	/* Init the tables with default values */
	CopyDefaultData(pRomTable);

   /* Only attempt to program EEPROM from BSP if not building for production */
   /* to catch EEPROM corruption problem. */
   #ifdef EEPROM_WRITE
	/* Program the EEPROM, if it exists */
	if ( HWSTATE.bEEPROMState )
	{
		ProgramEEPROM( SLAVE_ADDRESS | PAGE_BLOCK_0, pRomTable, PAGE_BLOCK_BYTES );
		ProgramEEPROM( SLAVE_ADDRESS | PAGE_BLOCK_1, (pRomTable + PAGE_BLOCK_BYTES), ( EEPROM_TBL_SIZE_DB - PAGE_BLOCK_BYTES ) );
	}
   #endif

	return;
}

/****************************************************************************
 *
 *  Name:			static void CopyDefaultData(UINT8* pRomTable)
 *
 *  Description:	Copies default EEPROM data into the data table
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void CopyDefaultData(UINT8* pRomTable)
{
	UINT32 i;
	UINT8* pDefPtr;

	pDefPtr = ( UINT8* )&DefEEPROMData;

	for ( i = 0; i < EEPROM_TBL_SIZE_DB; i++ )
	{
		pRomTable[ i ] = *pDefPtr++;
	}
			
	return;
}



void sysSerialCheck( void )
{
   CheckSerialNum(EEPROMTbl, Serial_Str_Tbl);
}

/****************************************************************************
 *
 *  Name:			static void CheckSerialNum(UINT8* pRomTable, UINT8* pSerialStrTable)
 *
 *  Description:	Checks that the serial number string is valid,
 *					and generates and saves a random one if not
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void CheckSerialNum(UINT8* pRomTable, UINT8* pSerialStrTable)
{
	/* If the serial number is not valid. */
	if ( !IsSerialNumValid(pSerialStrTable) )
	{
		/* Generate a random one and store it in the RAM table. */
		GenValidSerialNum(pSerialStrTable);

		/* If the EEPROM exists, program it. */
		if (HWSTATE.bEEPROMState)
		{
			/* Program the EEPROM, so we have the same serial number next time. */
			ProgramEEPROM( SLAVE_ADDRESS | PAGE_BLOCK_0, pRomTable, PAGE_BLOCK_BYTES );
			ProgramEEPROM( SLAVE_ADDRESS | PAGE_BLOCK_1, (pRomTable + PAGE_BLOCK_BYTES), ( EEPROM_TBL_SIZE_DB - PAGE_BLOCK_BYTES ) );
		}
	}

	return;
}

/****************************************************************************
 *
 *  Name:			static BOOL IsSerialNumValid(UINT8* pRomTable, UINT8* pSerialStrTable)
 *
 *  Description:	Checks to see if the serial number string is valid
 *					The string is unicode.  All 0xff's is considered invalid
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns TRUE if the string is valid, FALSE if not
 *
 ***************************************************************************/
BOOL IsSerialNumValid(UINT8* pSerialStrTable)
{
	BOOL bValid;		/* Whether the string is valid or not */
	UINT8 uByteCnt;	/* Number of bytes in the string */
	UINT8* pStrPtr;	/* Pointer to string bytes */

	bValid = FALSE;

	/* First byte of the serial number string table is the byte count, including 2 byte header. */
	uByteCnt = *pSerialStrTable - 2;
	pStrPtr = pSerialStrTable + 2;

	/* Check if all string bytes are 0xff. */
	while( uByteCnt && !bValid )
	{
		if ( *pStrPtr != 0xff )
		{
			bValid = TRUE;
		}

		uByteCnt--;
		pStrPtr++;
	}

	return bValid;
}

/****************************************************************************
 *
 *  Name:			static void GenValidSerialNum(UINT8* pSerialStrTable)
 *
 *  Description:	
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void GenValidSerialNum(UINT8* pSerialStrTable)
{
	UINT8* pStrPtr;		/* Pointer to string bytes */
	UINT16 uTimerValue;	/* Timer 1 is used to generate a random number */

	/* Hex-to-unicode table for lower 4 characters. */
	static UINT8 uLowTbl[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5' };

	/* Hex-to-unicode table for higher 4 characters. */
	static UINT8 uHiTbl[] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1', '2', '3', '4', '5', '6' };

	int i;

	/* We will generate a 16-byte string, plus a 2-byte header. */
	*pSerialStrTable = SERIAL_NUM_STR_LEN;
	*( pSerialStrTable + 1 ) = DESC_TYPE_STRING;

	pStrPtr = pSerialStrTable + 2;

	/* Init. the string to all 0's. */
	for ( i = 0; i < ( SERIAL_NUM_STR_LEN - 2 ); i += 2 )
	{
		pStrPtr[ i ] = 0;
		pStrPtr[ i + 1 ] = '0';
	}

	/* If the EEPROM exists, generate a random serial number.  Otherwise, just go with 0's. */
	if (HWSTATE.bEEPROMState)
	{
		/* Read System timer 1, which was turned on at power-on. */
		uTimerValue = *(UINT16 *)0x350020;

		/* Each nibble of the 16-bit timer value is used to generate 2 characters of the */
		/* serial number string. */
		for ( i = 1; i < ( ( SERIAL_NUM_STR_LEN - 2 ) / 2 ); i += 2 )
		{
			pStrPtr[ i ] = uLowTbl[ uTimerValue & 0xf ];
			pStrPtr[ i + 8 ] = uHiTbl[ uTimerValue & 0xf ];

			uTimerValue >>= 4;
		}
	}
	
	return;
}

/****************************************************************************
 *
 *  Name:			void InitEEPROMImage( void )
 *
 *  Description:	Initializes the device description and configuration
 *					   tables from EEPROM.  If this fails, it uses default values
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/

void InitEEPROMImage( void )
{
	UINT32* pAddr = 0;

	/* Read device descriptor and configuration information from the EEPROM */
	if ( ReadEEPROM( SLAVE_ADDRESS | PAGE_BLOCK_0, 0, EEPROMTbl, PAGE_BLOCK_BYTES ) )
	{
		HWSTATE.bEEPROMState = TRUE;
		ReadEEPROM( SLAVE_ADDRESS | PAGE_BLOCK_1, 0, EEPROMTbl + PAGE_BLOCK_BYTES, ( EEPROM_TBL_SIZE_DB - PAGE_BLOCK_BYTES ) );
	}

   /* EEPROM is not present */
	else
	{
		HWSTATE.bEEPROMState = FALSE;
	}

	/* If the EEPROM signature is not present, init the EEPROM and tables with default values from code. */
	if ( *( UINT32* )EEPROMTbl != EEPROM_SIG )
	{
		InitEEPROM(EEPROMTbl);
	}

	/* Init the table pointers */
	pAddr = ( UINT32* )EEPROMTbl;
	Dev_Desc_Tbl = &EEPROMTbl[ 0 ] + pAddr[ 1 ];
	Dev_Cfg_Tbl = &EEPROMTbl[ 0 ] + pAddr[ 2 ];
	LanguageID_Str_Tbl = &EEPROMTbl[ 0 ] + pAddr[ 3 ];
	Manufacturer_Str_Tbl = &EEPROMTbl[ 0 ] + pAddr[ 4 ];
	Product_Str_Tbl = &EEPROMTbl[ 0 ] + pAddr[ 5 ];
	Serial_Str_Tbl = &EEPROMTbl[ 0 ] + pAddr[ 6 ];
   Mac_Address_Str_Tbl = &EEPROMTbl[ 0 ] + pAddr[ 7 ];

   /* Build the host signature pattern from CONEXANT_VENDOR_ID and P5X_PRODUCT_ID. */
   HostSigValue = (UINT32)(*(Dev_Desc_Tbl+11) << 24);
   HostSigValue |= (UINT32)(*(Dev_Desc_Tbl+10) << 16);
   HostSigValue |= (UINT32)(*(Dev_Desc_Tbl+9) << 8);
   HostSigValue |= (UINT32)(*(Dev_Desc_Tbl+8));

   /* Determine whether board is self or bus powered */
   HWSTATE.HWPowerMode = (UINT32)(*(Dev_Cfg_Tbl+7) & (BUS_POWERED_ATTRIB | SELF_POWERED_ATTRIB));
   /* Store MAC address to report to host later */
   HWSTATE.dwMACAddressLow = *((UINT32 *)Mac_Address_Str_Tbl);
   HWSTATE.dwMACAddressHigh = *((UINT32 *)(Mac_Address_Str_Tbl + 4));

	return;
}

EXPORT_SYMBOL(ReadEEPROM);


/*************************************************************
 *  Parser data from USBDeviceConfiguration table to
 *  format understand by USB SANDCORE
 *  INPUT :     Device Descriptor Table pointer
 *              Device Configuration Table pointer
 *              USB Endpoints Info Struct Pointer
 *              USB General Info struct Pointer
 *       
 *  RETURN :    P50 ERR status 
 *************************************************************/

/****************************************************************************
 *
 *  $Log: eeprom.c,v $
 *  Revision 1.1  2003/08/28 11:41:23  davidm
 *
 *  Add in the Conexant platform files
 *
 *  Revision 1.1  2003/06/29 14:28:18  gerg
 *  Import of the Conexant 82100 code (from Conexant).
 *  Compiles, needs to be tested on real hardware.
 *
 * 
 * 6     8/12/02 7:27a Richarjc
 * Added missing include to remove a compiler warning.
 * 
 * 5     7/08/02 5:43p Webstejm
 * Changed references to LnxTools.h to OsTools.h
 * 
 * 4     5/28/02 8:49a Richarjc
 * The dma code needs to be made more modular to support an Epic "dma
 * module".
 * 
 * 3     4/08/02 2:09p Richarjc
 * Modified to make the Conexant emac driver a loadable module so as to
 * keep the source code proprietary.
 * 
 * 2     2/28/02 8:26a Richarjc
 * Added Conexant ownership and GPL notices.
 * 
 * 1     2/22/02 11:47a Palazzjd
 * Lineo Beta
 *  Revision 1.1  2001/12/12 14:26:32  oleks
 *  OZH: Added Conexant "Golden Gate" support.
 *
 * 
 * 3     10/10/01 4:32p Lewisrc
 * rename #include of lnxtools.h to LnxTools.h because we now use the
 * mixed case form because we are now sharing it with Linux Tigris and
 * that is how it is in that project.
 * 
 * 2     8/28/01 2:47p Lewisrc
 * Merging Hasbani P52 codebase - changing CX82100 type names to generic
 * CNXT or CX821XX
 * Swapping Console and Dumb Terminal to console on first UART to match
 * GoldenGate
 * Updating kernel compiler from 2.91 to 2.96 to match app compiler
 * 
 * renamed include files from p52*.h to cnxt*.h
 * 
 * 1     6/13/01 8:56a Richarjc
 * Initial checkin of the Linux kernel and BSP source code for the Golden
 * Gate development board based upon the CX82100 Home Networking
 * Processor.
 * 
 *    Rev 1.5   Aug 29 2000 16:30:02   nguyenpa
 * Use EEPROM_WRITE define to control
 * whether BSP will write to EEPROM or not.
 * Default is NO.
 * 
 *    Rev 1.4   15 Aug 2000 14:25:28   nguyenpa
 * For release build no longer attempt to
 * write to EEPROM.
 * 
 *    Rev 1.3   14 Aug 2000 16:19:50   nguyenpa
 * Port init changes which will force the EEPROM
 * to known default state at start up.
 * 
 *    Rev 1.2   09 Aug 2000 14:37:56   nguyenpa
 * Changes to DMA API and BM initialization
 * for HDC.
 * 
 *    Rev 1.1   Jul 18 2000 17:01:32   nguyenpa
 * Add code to read EMAC address from
 * EEPROM into BSP HW structure.
 * 
 *    Rev 1.0   Mar 28 2000 16:15:44   nguyenpa
 *  
 * 
 *    Rev 1.0   28 Feb 2000 17:15:58   blackldk
 * Bootloader ARM project
 * 
 *    Rev 1.2   11 Jan 2000 17:19:02   braun
 * EEPROM access now functional. Added EEPROM test to
 * POST code.
 * 
 *    Rev 1.1   30 Sep 1999 07:15:34   braun
 * SysTimerDelay call change only
 * 
 *    Rev 1.0   22 Sep 1999 16:58:34   chen2p
 *  First merge of Host IF, GPIO,Memory, and
 *  Timer tests.
 *
 ***************************************************************************/

