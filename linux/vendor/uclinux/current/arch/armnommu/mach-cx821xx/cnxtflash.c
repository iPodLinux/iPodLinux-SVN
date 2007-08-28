/****************************************************************************
 *
 *  Name:		cnxtflash.c
 *
 *  Description:	P52/Cx8211x flash programmer.
 *			Programs the Intel 28F320B3 and related flashes on board
 *
 *  Copyright (c) 1999-2002 Conexant Systems, Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 *  Include Files
 *-------------------------------------------------------------------------*/
#include <asm/arch/bsptypes.h>
#include "cnxtflash.h"
#include <asm/arch/hardware.h>
#include <linux/kernel.h> // for printk
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h> // for memcpy
#include <asm/system.h>
#include <linux/spinlock.h>

#define CNXT_FLASH_WRITE_START		0x040C000
#define CNXT_FLASH_MAX_SEGMENTS		8
#define CNXT_FLASH_SEGMENT_SIZE		2048
#define UNUSED __attribute__((unused))
#define CNXT_FLASH_DRIVER_VERSION	"001"
#define CNXT_FLASH_MIN_BLOCK_SIZE	8 * 1024 // 8KBYTES

#if 0
#define DPRINTK(format, args...) printk("cnxtflash.c: " format, ##args)
#else
#define DPRINTK(format, args...)
#endif

/*---------------------------------------------------------------------------
 *  Module Function Prototypes
 *-------------------------------------------------------------------------*/
BOOL CnxtFlashWrite( UINT16 * pflashStartAddr, UINT16 * psdramStartAddr, UINT32 size );
BOOL CnxtFlashRead( UINT16 * pflashStartAddr, UINT16 * psdramStartAddr, UINT32 size );
BOOL CnxtFlashProg( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes );
BOOL CnxtFlashVerifyDeviceID( UINT16 *uDeviceID );
BOOL CnxtFlashEraseBlock( UINT16* pBlockAddr, UINT16 *uDeviceID );
BOOL CnxtFlashProgramLocation( UINT16* pSrcAddr, UINT16* pDestAddr );
void CnxtFlashClearStatusReg( void );
BOOL CnxtFlashWaitForDone( BOOL );
BOOL CnxtFlashVerifyMemory( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes );
void CnxtFlashSetupSST ( void );
void CnxtFlashToggleBit ( void ); 
DWORD CnxtFlashCalcCRC32( BYTE *pData, DWORD dataLength );

static CNXT_FLASH_SEGMENT_T cnxtFlashSegments[CNXT_FLASH_MAX_SEGMENTS];
static BYTE cnxtFlashBuffer[CNXT_FLASH_MIN_BLOCK_SIZE];
static BOOL cnxtFlashLocked[CNXT_FLASH_MAX_SEGMENTS];

static int __init UNUSED CnxtFlashInit( void )
{
	int Status = 0;
	int i;
	BYTE  * startOfSegmentAddress = (BYTE *)CNXT_FLASH_WRITE_START;
	DWORD	headerCRC;
	DWORD	headerSize = sizeof( CNXT_FLASH_SEGMENT_T );

	// Substract out the dataLength, headerCRC and dataCRC from CRC32 calulations
	DWORD 	headerDataSize = headerSize - sizeof( DWORD ) - sizeof( DWORD ) - sizeof(DWORD);
	
	printk("Conexant Flash Driver version: %s\n", CNXT_FLASH_DRIVER_VERSION );

	for( i = 0; i < CNXT_FLASH_MAX_SEGMENTS; i++ )
	{

		// Mark each segment as locked.
		cnxtFlashLocked[i] = TRUE;
		
		// Load flash segment headers 
		memcpy( 
			&cnxtFlashSegments[i], 
			startOfSegmentAddress,
			headerSize
		);	
		
		headerCRC = CnxtFlashCalcCRC32( (BYTE *) &cnxtFlashSegments[i], headerDataSize );

		if( headerCRC != cnxtFlashSegments[i].headerCRC )
		{
			printk("Initializing Flash Segment %d\n", i + 1);

			cnxtFlashSegments[i].segmentNumber = i;	
			cnxtFlashSegments[i].segmentSize   = CNXT_FLASH_SEGMENT_SIZE;
			cnxtFlashSegments[i].startFlashAddress = (DWORD)startOfSegmentAddress;
			cnxtFlashSegments[i].startDataAddress = (DWORD)startOfSegmentAddress + headerSize;
			cnxtFlashSegments[i].owner = i;					

			// Calculate the new headerCRC and save it.
			headerCRC = CnxtFlashCalcCRC32( (BYTE *) &cnxtFlashSegments[i], headerDataSize );
			cnxtFlashSegments[i].headerCRC = headerCRC;
			
			// Store new header info. along with headerCRC to the flash segment.
			CnxtFlashWrite( 
				(UINT16 *)startOfSegmentAddress, 
				(UINT16 *)&cnxtFlashSegments[i], 
				headerSize
			); 

		} 

		startOfSegmentAddress += CNXT_FLASH_SEGMENT_SIZE;

	}

	return Status;
}

static void __exit UNUSED CnxtFlashExit( void )
{
}

// Add CnxtFlashInit to the '__initcall' list so it will
// be called at startup
module_init( CnxtFlashInit );
module_exit( CnxtFlashExit );
/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashOpen
 *
 *  Description:	Opens the specified flash segment only if 'owner'
 *			is correct for the segment and data actually
 *			exist.
 *
 *  Inputs:		
 *			
 *  Outputs:		
 *
 ***************************************************************************/
CNXT_FLASH_STATUS_E CnxtFlashOpen( CNXT_FLASH_SEGMENT_E segment )
{
	CNXT_FLASH_STATUS_E Status = CNXT_FLASH_SUCCESS;
	DWORD	headerSize = sizeof( CNXT_FLASH_SEGMENT_T );
	DWORD   dataCRC;

	// Check for a valid segment number.
	if( segment < CNXT_FLASH_SEGMENT_1 || segment > CNXT_FLASH_SEGMENT_8 )
	{
		return FALSE;
	}
	
	// Check for owner
	if( segment != cnxtFlashSegments[segment].owner )
	{
		printk("Incorrect owner for Flash Segment!\n" );
		return CNXT_FLASH_OWNER_ERROR;
	}
	
	DPRINTK(
		"CnxtFlashOpen: &cnxtFlashSegments[%d] = %x\n", 
		segment, 
		&cnxtFlashSegments[segment]
	);
	DPRINTK(
		"cnxtFlashSegments[%d].startFlashAddress = %x\n", 
		segment,
		cnxtFlashSegments[segment].startFlashAddress
	);

	// Copy the header from flash.
	memcpy( 
		(UINT16*)&cnxtFlashSegments[segment], 
		(UINT16*)cnxtFlashSegments[segment].startFlashAddress,
		headerSize
	);	
	

	if( cnxtFlashSegments[segment].dataLength > CNXT_FLASH_SEGMENT_SIZE )
		cnxtFlashSegments[segment].dataLength = CNXT_FLASH_SEGMENT_SIZE;

	if( cnxtFlashSegments[segment].startDataAddress >= ( CNXT_FLASH_WRITE_START + CNXT_FLASH_MIN_BLOCK_SIZE ) )
			return CNXT_FLASH_DATA_ERROR;		
	else {
		// Calculate the new segment data CRC.
		dataCRC = CnxtFlashCalcCRC32( 
			(BYTE *) cnxtFlashSegments[segment].startDataAddress, 
			cnxtFlashSegments[segment].dataLength 
		);

	}

	DPRINTK(
		"cnxtFlashSegments[%d].dataCRC = %x        dataCRC = %x\n",
		segment,
		cnxtFlashSegments[segment].dataCRC,
		dataCRC
	);

	// Check the data CRC
	if( cnxtFlashSegments[segment].dataCRC != dataCRC )
		Status = CNXT_FLASH_DATA_ERROR;

	// Mark the segment as unmarked.
	cnxtFlashLocked[segment] = FALSE;
	return Status;
}
/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashReadRequest
 *
 *  Description:	Read Flash API.
 *
 *  Inputs:		
 *
 *  Outputs:		
 *
 ***************************************************************************/
BOOL CnxtFlashReadRequest( 
	CNXT_FLASH_SEGMENT_E segment, 
	UINT16 * psdramStartAddr, 
	UINT32 size 
)
{

// Make sure data pointer is not NULL.
	if( !psdramStartAddr )
		return FALSE;

	// Make sure maximum size is not exceeded.
	if( size > CNXT_FLASH_SEGMENT_SIZE - sizeof( CNXT_FLASH_SEGMENT_T ) )
		return FALSE;

	// Check for a valid segment number.
	if( segment < CNXT_FLASH_SEGMENT_1 || segment > CNXT_FLASH_SEGMENT_8 )
	{
		return FALSE;
	} else {
		// Copy the data from flash.
		memcpy( 
			(UINT16*)psdramStartAddr, 
			(UINT16*)cnxtFlashSegments[segment].startDataAddress,
			size
		);	

		return TRUE;		
	}
}
/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashWriteRequest
 *
 *  Description:	Write Flash API.
 *
 *  Inputs:		
 *
 *  Outputs:		
 *
 ***************************************************************************/
BOOL CnxtFlashWriteRequest( 
	CNXT_FLASH_SEGMENT_E segment, 
	UINT16 * psdramStartAddr, 
	UINT32 size 
)
{
	DWORD dataCRC;
	DWORD headerSize = sizeof( CNXT_FLASH_SEGMENT_T );
	DWORD startOfSegment;
	DWORD flashBlock;
	BOOL bSuccess = FALSE;

	// Make sure data pointer is not NULL.
	if( !psdramStartAddr ) {
		return bSuccess;
	}

	// Make sure there is enough room for the data.
	if( size > CNXT_FLASH_SEGMENT_SIZE - sizeof( CNXT_FLASH_SEGMENT_T ) )
	{
		return bSuccess;
	}

	// Check for a valid segment number.
	if( segment < CNXT_FLASH_SEGMENT_1 || segment > CNXT_FLASH_SEGMENT_8 )
	{
		return bSuccess;
	} else {

		// Make sure the segment is not locked.
		if( cnxtFlashLocked[segment] ) {
			printk( "Flash Segment Locked! (Try opening it first)\n" );
			return FALSE;
		}

		if( size > CNXT_FLASH_SEGMENT_SIZE - headerSize )
			return FALSE;
		else
			cnxtFlashSegments[segment].dataLength = size;
	
		// Calculate the new segment data CRC.
		dataCRC = CnxtFlashCalcCRC32( 
			(BYTE *) psdramStartAddr, 
			size 
		);

		cnxtFlashSegments[segment].dataCRC = dataCRC;
		
		DPRINTK(
			"cnxtFlashSegments[%d].dataCRC = %x\n", 
			segment,
			cnxtFlashSegments[segment].dataCRC
		);
		DPRINTK(
			"headerSize = %x\n",
			headerSize
		);

		if( segment > CNXT_FLASH_SEGMENT_4 )
			flashBlock = CNXT_FLASH_SEGMENT_5;
		else
			flashBlock = CNXT_FLASH_SEGMENT_1;

		DPRINTK(
			"flashBlock = %d\n",
			flashBlock
		);

		// Read in the minimum flash block size (8kbytes)
		memcpy(
			&cnxtFlashBuffer[0],
			(BYTE *)cnxtFlashSegments[flashBlock].startFlashAddress,
			CNXT_FLASH_MIN_BLOCK_SIZE	
		);

		// Copy the flash segment header

		startOfSegment = segment * CNXT_FLASH_SEGMENT_SIZE;
		DPRINTK(
			"startOfSegment = %x\n", 
			startOfSegment
		);

		memcpy(
			&cnxtFlashBuffer[startOfSegment], 
			&cnxtFlashSegments[segment], 
			headerSize
		 );

		// Copy the data
		memcpy( 
			&cnxtFlashBuffer[startOfSegment + headerSize],
			psdramStartAddr,
			size
		);			
		
		DPRINTK(
			"cnxtFlashSegments[%d].startFlashAddress = %x\n",
			flashBlock,
			cnxtFlashSegments[flashBlock].startFlashAddress
		);
		DPRINTK(
			"CNXT_FLASH_MIN_BLOCK_SIZE: %x\n",
			CNXT_FLASH_MIN_BLOCK_SIZE
		);

		bSuccess = CnxtFlashWrite( 
				(UINT16 *)cnxtFlashSegments[flashBlock].startFlashAddress, 
				(UINT16 *)&cnxtFlashBuffer[0], 
				CNXT_FLASH_MIN_BLOCK_SIZE
		); 

		if( !bSuccess )  {

			return FALSE;
		}

	}

	return TRUE;
}

/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashWrite
 *
 *  Description:	Writes data to flash.
 *
 *  Inputs:		
 *
 *  Outputs:		
 *
 ***************************************************************************/
BOOL CnxtFlashWrite( UINT16 * pflashStartAddr, UINT16 * psdramStartAddr, UINT32 size )
{

	
	BOOL 		bSuccess;

	bSuccess = CnxtFlashProg(
			psdramStartAddr,	// Location of data SDRAM
			pflashStartAddr,	// Location to place data in flash
			size			// The size in bytes
	);

	DPRINTK( "CnxtFlashProg: bSuccess = %d\n", bSuccess );

	return bSuccess;
}


/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashProg( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes )
 *
 *  Description:	Programs the flash
 *
 *  Inputs:		UINT16* pSrcAddr = where data to program is
 *			UINT16* pDestAddr = where to start programming the flash
 *			UINT32 uBytes = number of bytes to program
 *
 *  Outputs:		Returns TRUE if programming is successful, FALSE if not
 *
 ***************************************************************************/

BOOL CnxtFlashProg( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes )
{
	BOOL bSuccess;
	UINT16* pSrcAddrChk;
	UINT16* pDestAddrChk;
	UINT32 uBytesChk;
	UINT16 uDeviceID;

	/* Save the addresses and byte count for later validation */
	pSrcAddrChk = pSrcAddr;
	pDestAddrChk = pDestAddr;
	uBytesChk = uBytes;

	/* Make sure the right device is installed */
	bSuccess = CnxtFlashVerifyDeviceID(&uDeviceID);

	// Erase a 8kbyte block beginning at pDestAddr
	bSuccess &= CnxtFlashEraseBlock( pDestAddr, &uDeviceID );

	/* While we haven't finished all locations and there hasn't been an error... */
	while( uBytes && bSuccess )
	{

		/* Program a location */
		bSuccess = CnxtFlashProgramLocation( pSrcAddr, pDestAddr );

		/* Update memory pointers and byte counter */
		++pSrcAddr;
		++pDestAddr;
		uBytes -= 2;
	}
	
	*pCUIReg = READ_ARRAY;
	  
	/* Verify the data */
	if(bSuccess) bSuccess = CnxtFlashVerifyMemory( pSrcAddrChk, pDestAddrChk, uBytesChk );
	
	return bSuccess;
}


/****************************************************************************
 *
 *  Name:			BOOL CnxtFlashVerifyDeviceID( void )
 *
 *  Description:	Verifies the device ID
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns TRUE if the correct device is installed, FALSE if not
 *
 ***************************************************************************/

BOOL CnxtFlashVerifyDeviceID( UINT16 *uDeviceID )
{
	BOOL bSuccess;
	UINT16 uMfrID;
	
#ifdef INTEL_FLASH
	/* Go into "read identifer" mode */
	*pCUIReg = READ_IDENTIFIER;
#else
	CnxtFlashSetupSST();
	*p5555Reg = READ_IDENTIFIER;
#endif

	uMfrID = *pMfrIDReg;

	*uDeviceID = *pDeviceIDReg;
	*uDeviceID = *pDeviceIDReg;


	/* Check them */
	if ( ((uMfrID == MFR_ID_INTEL)
			|| (uMfrID == MFR_ID_SST) 
			|| (uMfrID == MFR_ID_AMD)
			|| (uMfrID == MFR_ID_STM)
			|| (uMfrID == MFR_ID_MXIC)
			|| (uMfrID == MFR_ID_SHARP)) &&
		 ((*uDeviceID == DEVICE_ID_INTELB3_BB) 
		 	|| (*uDeviceID == DEVICE_ID_INTELC3_BB)
			|| (*uDeviceID == DEVICE_ID_INTELB3_BB_4M)
		 	|| (*uDeviceID == DEVICE_ID_SST)
		 	|| (*uDeviceID == DEVICE_ID_AMD_TB)
		 	|| (*uDeviceID == DEVICE_ID_STM_TB)
		 	|| (*uDeviceID == DEVICE_ID_SHARP_BB)
		 	|| (*uDeviceID == DEVICE_ID_INTELC3_TB)
		 	|| (*uDeviceID == DEVICE_ID_INTELB3_TB)
			|| (*uDeviceID == DEVICE_ID_MXIC_BB)
		 	|| (*uDeviceID == DEVICE_ID_AMD_BB)
		 	|| (*uDeviceID == DEVICE_ID_STM_BB)) )
	{
		bSuccess = TRUE;
	}
	else
	{
		bSuccess = FALSE;
	}

	/* Return to "read array" mode */
	*pCUIReg = READ_ARRAY;

	return bSuccess;
}


/****************************************************************************
 *
 *  Name:			BOOL CnxtFlashEraseBlock( UINT16* pBlockAddr )
 *
 *  Description:	Erases a block of the flash
 *
 *  Inputs:			UINT16* pBlockAddr = address of the start of the block
 *
 *  Outputs:		none
 *
 ***************************************************************************/

BOOL CnxtFlashEraseBlock( UINT16* pBlockAddr, UINT16 *uDeviceID )
{
	BOOL bSuccess=TRUE;

#ifdef INTEL_FLASH
	if(
		(
			((*uDeviceID == 0x8897)||(*uDeviceID == 0x8891)||(*uDeviceID == 0x88c3)||(*uDeviceID == 0x00e9))
				&&	
			( 
				((pBlockAddr < BOOT_BLOCK_END) && (((UINT32 )pBlockAddr & ( UINT32 )0x1fff) == ( UINT32 )0x000)) 
					|| ((( UINT32 )pBlockAddr & 0xffff ) == 0x0000 )
			)
		)
		||	(( (*uDeviceID == 0x8890)||(*uDeviceID == 0x88c2) ) && ( ((pBlockAddr >= MAIN_BLOCK_END) 
					&& (((UINT32 )pBlockAddr & ( UINT32 )0x1fff) == ( UINT32 )0x000)) 
		||	((( UINT32 )pBlockAddr & 0xffff ) == 0x0000 ) ))
	)

	{
		
		/* Clear the status register */
		CnxtFlashClearStatusReg();

		*pCUIReg = UNLOCK_SETUP;
		*pBlockAddr = ERASE_CONFIRM;

		// Write the 'block erase' (erase setup) command to the CUI register.
		*pCUIReg = ERASE_SETUP;
		
		// Write some data to the start address of the block. This address 
		// will be latched internally when the 'erase confirm' command is
		// issued.
		*pBlockAddr = ERASE_CONFIRM;

		// Write block erase confirm command to the CUI register.
		*pCUIReg = ERASE_CONFIRM;

		/* Wait for erase complete */
		bSuccess &= CnxtFlashWaitForDone( FALSE );

		/* Return to "read array" mode */
		*pCUIReg = READ_ARRAY;
	}

#else
	#ifdef SST_FLASH
	if ( ( ( pBlockAddr < BOOT_BLOCK_END ) AND ( ( ( UINT32 )pBlockAddr & ( UINT32 )0xfff ) EQUALS ( UINT32 )0x000 ) ) OR
	   ( ( ( UINT32 )pBlockAddr & 0xffff ) EQUALS 0x0000 ) )
	{
		CnxtFlashSetupSST();
		*p5555Reg = ERASE_SETUP;
		CnxtFlashSetupSST();
		if(	pBlockAddr >= BOOT_BLOCK_END )
		{
			*pBlockAddr = BLOCK_ERASE_CONFIRM;
		}
		else
			*pBlockAddr = SECTOR_ERASE_CONFIRM;	
			
		while(!(*pBlockAddr & DQ7));
		//CnxtFlashToggleBit();
		
		/* Return to "read array" mode */
		*pCUIReg = READ_ARRAY;
	}
	#else
	if ( (*uDeviceID == 0x22c4 AND ( ((pBlockAddr >= MAIN_BLOCK_END) AND ( ( (UINT32)pBlockAddr == (UINT32)0x5f8000) 
												OR ( ( UINT32 )pBlockAddr == ( UINT32 )0x5fa000 ) 
												OR ( ( UINT32 )pBlockAddr == ( UINT32 )0x5fc000 ))) 
		OR (((UINT32)pBlockAddr & 0xffff) EQUALS 0x0000) ) )
		OR (*uDeviceID == 0x2249 AND ( ((pBlockAddr < BOOT_BLOCK_END) AND ( ( (UINT32)pBlockAddr == (UINT32)0x406000) 
												OR ( ( UINT32 )pBlockAddr == ( UINT32 )0x404000 ) 
												OR ( ( UINT32 )pBlockAddr == ( UINT32 )0x408000 ) )) 
		OR (((UINT32)pBlockAddr & 0xffff) EQUALS 0x0000) ) ) )
	{
		CnxtFlashSetupSST();
		*p5555Reg = ERASE_SETUP;
		CnxtFlashSetupSST();
		
		*pBlockAddr = SECTOR_ERASE_CONFIRM;	
			
		while(!(*pBlockAddr & DQ7));
		//CnxtFlashToggleBit();
		
		/* Return to "read array" mode */
		*pCUIReg = READ_ARRAY;
	}
	#endif
#endif
	return bSuccess;
}


/****************************************************************************
 *
 *  Name:			BOOL CnxtFlashProgramLocation( UINT16* pSrcAddr, UINT16* pDestAddr )
 *
 *  Description:	Programs a location in the flash
 *
 *  Inputs:			UINT16* pSrcAddr = address of where to get the data
 *					UINT16* pDestAddr = flash address to program
 *
 *  Outputs:		Returns TRUE if programming successful, FALSE if not
 *
 ***************************************************************************/

BOOL CnxtFlashProgramLocation( UINT16* pSrcAddr, UINT16* pDestAddr )
{
	BOOL bSuccess=TRUE;
#ifdef INTEL_FLASH
	/* Clear status register */
	CnxtFlashClearStatusReg();

	#ifdef SHARP_FLASH
		/* Wait for reading of status reg */
		bSuccess = CnxtFlashWaitForDone( FALSE );
	#endif
		
	/* Write program setup */
	*pCUIReg = PROGRAM_SETUP;
	
	/* Program the location */
	*pDestAddr = *pSrcAddr;

	/* Wait for program complete */
	bSuccess &= CnxtFlashWaitForDone( FALSE );

	/* Return to "read array" mode */
	*pCUIReg = READ_ARRAY;

#else
	CnxtFlashSetupSST();
	*p5555Reg = PROGRAM_SETUP;
	*pDestAddr = *pSrcAddr;
	while(*pDestAddr != *pSrcAddr );
	//CnxtFlashToggleBit();

	*pCUIReg = READ_ARRAY;
#endif


	return bSuccess;
}


/****************************************************************************
 *
 *  Name:			void CnxtFlashClearStatusReg( void )
 *
 *  Description:	Clears the status register
 *
 *  Inputs:			none
 *
 *  Outputs:		none
 *
 ***************************************************************************/
#ifdef INTEL_FLASH
void CnxtFlashClearStatusReg( void )
{
	*pCUIReg = CLEAR_STATUS_REG;
	return;
}
#endif


/****************************************************************************
 *
 *  Name:			BOOL CnxtFlashWaitForDone( void )
 *
 *  Description:	Waits for a process to complete, and checks results
 *
 *  Inputs:			none
 *
 *  Outputs:		Returns TRUE if process successful
 *
 ***************************************************************************/
#ifdef INTEL_FLASH
BOOL CnxtFlashWaitForDone( BOOL debugON )
{
	BOOL bSuccess=TRUE;
	UINT16 uStatus;

	/* Write the "read status register" command */
	*pCUIReg = READ_STATUS_REG;

	/* Wait for the operation to finish */
	do
	{
		/* Read the status register */
		uStatus = *pCUIReg;

		/* Wait for the "state machine ready" bit */
	} while( !( uStatus & WSMS_READY ) );


	if( debugON ) 
	{

		printk(
			"uStatus = %x\n",
			uStatus
		);

		uStatus = *pCUIReg;
		
		printk(
			"uStatus = %x\n",
			uStatus
		);

	} 

	/* Check for errors */
	if ( !( uStatus & FLASH_ERR ) )
	{
		bSuccess = TRUE;
	}

	else
	{
		bSuccess = FALSE;
	}

	return bSuccess;
}
#else
void CnxtFlashToggleBit(void)
{
	UINT16 uFirst_Read, uSecond_Read;
	do
	{
		/* Read the status register */
		uFirst_Read = *pCUIReg;
		uSecond_Read = *pCUIReg;

		/* Wait for the "state machine ready" bit */
	} while( ( uFirst_Read & TOGGLE_BIT ) != ( uSecond_Read & TOGGLE_BIT ) );

	return;
}
#endif


/****************************************************************************
 *
 *  Name:		BOOL CnxtFlashVerifyMemory( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes )
 *
 *  Description:	Compares memory locations
 *
 *  Inputs:		UINT16* pSrcAddr = source address
 *			UINT16* pDestAddr = destination address
 *			UINT32 uBytes = number of bytes to check
 *
 *  Outputs:		Returns BOOL TRUE if memories are the same, FALSE if not
 *
 ***************************************************************************/

BOOL CnxtFlashVerifyMemory( UINT16* pSrcAddr, UINT16* pDestAddr, UINT32 uBytes )
{
	BOOL bSuccess;
	bSuccess = TRUE;

	while( uBytes && bSuccess )
	{
		if ( *pDestAddr++ != *pSrcAddr++ )
		{
			bSuccess = FALSE;
		}

		uBytes -= 2;
	}

	return bSuccess;
}

/****************************************************************************
 *
 *  Name:		DWORD CnxtFlashCalcCRC32
 *
 *  Description:	Performs a 32 bit CRC on pData
 *
 *  Inputs:		DWORD		 crc32_val,		
 *			BYTE		*pData,
 *			DWORD		dataLength	
 *			
 *
 *  Outputs:		Returns the calculated CRC value.
 *
 ***************************************************************************/
// CRC32 Lookup Table generated from Charles Michael
//  Heard's CRC-32 code
static DWORD crc32_tbl[256] =
{
0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

#define INITIAL_CRC_VAL		0xffffffff  // negative one

DWORD CnxtFlashCalcCRC32
(
	BYTE		*pData,
	DWORD		dataLength	
)
{
	DWORD	crc32_val = INITIAL_CRC_VAL; 

    // Calculate a CRC32 value
	while ( dataLength-- )
	{
		crc32_val = ( crc32_val << 8 ) ^ crc32_tbl[(( crc32_val >> 24) ^ *pData++ ) & 0xff];
	}
	
	return (crc32_val);

}

#ifndef INTEL_FLASH
void CnxtFlashSetupSST(void)
{
	*p5555Reg = CMD_AA;
	*p2AAAReg = CMD_55;

	return;
}
#endif


EXPORT_SYMBOL( CnxtFlashWriteRequest );
EXPORT_SYMBOL( CnxtFlashReadRequest );
EXPORT_SYMBOL( CnxtFlashOpen );
