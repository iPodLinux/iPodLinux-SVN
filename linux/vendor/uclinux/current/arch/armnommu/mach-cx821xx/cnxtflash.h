/****************************************************************************
 *
 *  Name:		cnxtflash.h
 *
 *  Description:	Flash programmer header file
 *
 *  Copyright (c) 1999-2002 Conexant Systems, Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *
 ***************************************************************************/
#ifndef _CNXTFLASH_H_
#define _CNXTFLASH_H_

/* comment SST_FLASH and SHARP_FLASH	=> program INTEL
   comment INTEL_FLASH and SHARP_FLASH  => program SST
   comment SST_FLASH			        => program SHARP
   comment ALL							=> program AMD or ST-Micro or MXIC
*/
#define INTEL_FLASH 	1
#define SHARP_FLASH		1
//#define SST_FLASH		1

#ifdef INTEL_FLASH
	#define START_SIZE			0x100000
	#define MAC_SIZE			0xfc		// 180 bytes
	#define PARMS_SIZE			0x2000
	#define BSP_SIZE			0x170000
	#define FFS_SIZE			0x100000
	#define	READ_ARRAY			( UINT16 )0xff
	#define PROGRAM_SETUP		( UINT16 )0x40
	#define ERASE_SETUP			( UINT16 )0x20
	#define UNLOCK_SETUP		( UINT16 )0x60
	#define ERASE_CONFIRM		( UINT16 )0xd0
	#define READ_STATUS_REG		( UINT16 )0x70
	#define READ_QUERY		( UINT16 )0x98
	#define CLEAR_STATUS_REG	( UINT16 )0x50
	#define WSMS_READY			( UINT16 )( 1 << 7 )
	#define ERASE_ERR			( UINT16 )( 1 << 5 )
	#define PROG_ERR			( UINT16 )( 1 << 4 )
	#define Vpp_ERR				( UINT16 )( 1 << 3 )
	#define BLOCK_LOCK_ERR		( UINT16 )( 1 << 1 )
	#define FLASH_START_ADDR	( UINT16* )0x400000
	//#define FLASH_MAC_ADDR		( UINT16* )0x40a000
	//#define FLASH_PARMS_ADDR	( UINT16* )0x40c000
	//#define FLASH_BSP_ADDR		( UINT16* )0x410000
	#define FLASH_FFS_ADDR		( UINT16* )0x500000

#else
	#ifdef SST_FLASH
		#define FLASH_START_ADDR	( UINT16* )0x400000
		#define FLASH_MAC_ADDR		( UINT16* )0x40a000
		#define FLASH_PARMS_ADDR	( UINT16* )0x40c000
		#define FLASH_BSP_ADDR		( UINT16* )0x410000
		#define FLASH_FFS_ADDR		( UINT16* )0x450000
		#define START_SIZE			0x40000
		#define MAC_SIZE			0xfc
		#define PARMS_SIZE			0x2000
		#define BSP_SIZE			0x170000
		#define FFS_SIZE			0x1b0000
	
		//These are used for the SST version of Flash
		#define p5555Reg			( volatile UINT16* ) 0x40aaaa
		#define p2AAAReg			( volatile UINT16* ) 0x405554
		#define READ_ARRAY			( UINT16 ) 0xf0
		#define ERASE_SETUP			( UINT16 ) 0x80
		#define SECTOR_ERASE_CONFIRM	( UINT16 ) 0x30
		#define BLOCK_ERASE_CONFIRM	( UINT16 ) 0x50
		#define PROGRAM_SETUP		( UINT16 ) 0xa0
		#define CMD_AA				( UINT16 ) 0xaa
		#define CMD_55				( UINT16 ) 0x55
		#define DQ7					( UINT16 ) 0x80
		#define TOGGLE_BIT			( UINT16 )( 1 << 6 )
	#else
		#define FLASH_MAC_ADDR		( UINT16* )0x408000
		#define FLASH_START_ADDR	( UINT16* )0x400000
		#define FLASH_PARMS_ADDR	( UINT16* )0x404000
		#define FLASH_BSP_ADDR		( UINT16* )0x410000
		#define FLASH_FFS_ADDR		( UINT16* )0x450000
		#define START_SIZE			0x40000
		#define MAC_SIZE			0xfc
		#define PARMS_SIZE			0x2000
		#define BSP_SIZE			0x170000
		#define FFS_SIZE			0x1b0000

		#define p5555Reg				( volatile UINT16* ) 0x40aaaa
		#define p2AAAReg				( volatile UINT16* ) 0x405554
		#define READ_ARRAY				( UINT16 ) 0xf0
		#define ERASE_SETUP				( UINT16 ) 0x80
		#define SECTOR_ERASE_CONFIRM	( UINT16 ) 0x30
		#define BLOCK_ERASE_CONFIRM		( UINT16 ) 0x50
		#define PROGRAM_SETUP			( UINT16 ) 0xa0
		#define CMD_AA					( UINT16 ) 0xaa
		#define CMD_55					( UINT16 ) 0x55
		#define DQ7						( UINT16 ) 0x80
		#define TOGGLE_BIT				( UINT16 )( 1 << 6 )
	#endif
#endif

#define MFR_ID_INTEL			0x0089
#define DEVICE_ID_INTELB3_BB	0x8891
#define DEVICE_ID_INTELB3_TB	0x8890
#define DEVICE_ID_INTELC3_BB  	0x88c3
#define DEVICE_ID_INTELC3_TB  	0x88c2
#define DEVICE_ID_INTELB3_BB_4M	0x8897

#define MFR_ID_SST				0x00bf
#define DEVICE_ID_SST			0x2782

#define MFR_ID_AMD				0x0001
#define DEVICE_ID_AMD_TB		0x22c4
#define DEVICE_ID_AMD_BB		0x2249

#define MFR_ID_MXIC				0x00c2
#define DEVICE_ID_MXIC_BB		0x2249

#define MFR_ID_STM				0x0020
#define DEVICE_ID_STM_TB		0x22c4
#define DEVICE_ID_STM_BB		0x2249

#define MFR_ID_SHARP			0x00b0
#define DEVICE_ID_SHARP_BB		0x00e9

#define READ_IDENTIFIER			( UINT16 )0x90

#define SDRAM_START_ADDR		( UINT16* )0x800000
//#define SDRAM_MAC_ADDR		( UINT16* )0x80a000
//#define SDRAM_PARMS_ADDR		( UINT16* )0x80c000
//#define SDRAM_BSP_ADDR		( UINT16* )0x810000
#define SDRAM_FFS_ADDR			( UINT16* )0x900000

#define BOOT_BLOCK_END			( UINT16* )0x410000
#define MAIN_BLOCK_END			( UINT16* ) 0x5f0000
//#define FLASH_SIZE				( UINT32 )0x200000
#define NUM_BOOT_BLOCKS			8

#define pMfrIDReg			( volatile UINT16* )( FLASH_START_ADDR )
#define pDeviceIDReg			( volatile UINT16* )0x400002
#define pCUIReg				( volatile UINT16* )0x400000
#define pDeviceSizeReg			( volatile UINT16* )0x400027

#define WSMS_READY				( UINT16 )( 1 << 7 )
#define ERASE_ERR				( UINT16 )( 1 << 5 )
#define PROG_ERR				( UINT16 )( 1 << 4 )
#define Vpp_ERR					( UINT16 )( 1 << 3 )
#define BLOCK_LOCK_ERR			( UINT16 )( 1 << 1 )

#define FLASH_ERR	( UINT16 )( ERASE_ERR | PROG_ERR | Vpp_ERR | BLOCK_LOCK_ERR )

#include <asm/arch/cnxtflash.h>

#endif 





