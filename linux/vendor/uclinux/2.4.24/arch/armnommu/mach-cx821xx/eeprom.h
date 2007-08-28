/****************************************************************************
*
*	Name:			eeprom.h
*
*	Description:	EEPROM access routines header file
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
*  $Modtime: 2/28/02 8:00a $
****************************************************************************/

#ifndef EEPROM_H
#define EEPROM_H

#define EEPROM_WRITE	0xFE
#define EEPROM_READ		0x01

#define SDA						   GPIO15
#define SCL						   GPIO16
#define NUM_TEST_BYTES			4
#define EEPROM_ADDRESS			0x100						   /* assume using end of page */
#define SLAVE_ADDRESS			0xA0						   /* bit 1 is page block number */
#define PAGE_BLOCK_0			   0
#define PAGE_BLOCK_1			   2
#define PAGE_BLOCK_BYTES		256
#define EEPROM_TBL_SIZE_DB		(2 * PAGE_BLOCK_BYTES)  /* EEPROM USB tables are 2 page blocks */
#define EEPROM_SIG				( UINT32 )0x55555555		/* Signature, to see if the EEPROM has been programmed */

#ifdef IKOS
#define START_SETUP_TIME		0
#define START_HOLD_TIME			0
#define STOP_SETUP_TIME			0
#define STOP_HOLD_TIME			0
#define SCL_HIGH_TIME			0
#define SCL_LOW_TIME			   0
#define SCL_LOW_TIME_DIVIDEBY2	0
#define WRITE_CYCLE_TIME		0

#else
#define START_SETUP_TIME		5							   /* all are in microseconds */
#define START_HOLD_TIME			4
#define STOP_SETUP_TIME			5
#define STOP_HOLD_TIME			START_SETUP_TIME
#define SCL_HIGH_TIME			4
#define SCL_LOW_TIME			   (10-SCL_HIGH_TIME)		/* assume 100 KHz clock frequency */
#define SCL_LOW_TIME_DIVIDEBY2	(SCL_LOW_TIME >> 1)  /* don't divide by 2 */
#define WRITE_CYCLE_TIME		15000						   /* cycle time is 15 msec max */
#endif


extern UINT8 EEPROMTbl[ EEPROM_TBL_SIZE_DB ];

/* Pointers to tables within EERPOM table */
extern UINT8* Dev_Desc_Tbl;			/* Device descriptor data */
extern UINT8* Dev_Cfg_Tbl;          /* Device configuration data */
extern UINT8* LanguageID_Str_Tbl;	/* Language ID */
extern UINT8* Manufacturer_Str_Tbl;	/* Manufacturer string */
extern UINT8* Product_Str_Tbl;		/* Product String */
extern UINT8* Serial_Str_Tbl;			/* Serial Number String */
extern UINT8* Mac_Address_Str_Tbl;  /* Mac Address String */

extern UINT32 HostSigValue;


BOOL EEPROMTest( void );
BOOL ReadEEPROM( UINT8 uSlaveAddr, UINT8 uWordAddr, UINT8* pRAMAddr, UINT16 uBytes );
BOOL WriteEEPROM( UINT8 uSlaveAddr, UINT8 uWordAddr, UINT8* pRAMAddr, UINT8 uBytes );
BOOL ProgramEEPROM( UINT8 uSlaveAddr, UINT8* pRAMAddr, UINT16 uBytes );

void InitEEPROM(UINT8* pRomTable);
void CopyDefaultData(UINT8* pRomTable);

/* Serial Number related functions */
void CheckSerialNum(UINT8* pRomTable, UINT8* pSerialStrTable);
BOOL IsSerialNumValid(UINT8* pSerialStrTable);
void GenValidSerialNum(UINT8* pSerialStrTable);

void InitEEPROMImage( void );
  #if 0
  void sysSerialCheck();
  #endif
#endif

/****************************************************************************
 *
 *  $Log: eeprom.h,v $
 *  Revision 1.1  2003/08/28 11:41:23  davidm
 *
 *  Add in the Conexant platform files
 *
 *  Revision 1.1  2003/06/29 14:28:18  gerg
 *  Import of the Conexant 82100 code (from Conexant).
 *  Compiles, needs to be tested on real hardware.
 *
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
 * 1     6/13/01 8:56a Richarjc
 * Initial checkin of the Linux kernel and BSP source code for the Golden
 * Gate development board based upon the CX82100 Home Networking
 * Processor.
 * 
 *    Rev 1.1   Jul 18 2000 17:03:28   nguyenpa
 * Add code to read EMAC address from
 * EEPROM into BSP HW structure.
 * 
 * 
 *    Rev 1.0   Mar 28 2000 16:33:18   nguyenpa
 * Add EEPROM support into BSP.
 * 
 *    Rev 1.0   28 Feb 2000 17:16:02   blackldk
 * Bootloader ARM project
 * 
 *    Rev 1.1   11 Jan 2000 17:19:04   braun
 * EEPROM access now functional. Added EEPROM test to
 * POST code.
 * 
 *    Rev 1.0   22 Sep 1999 16:58:46   chen2p
 *  First merge of Host IF, GPIO,Memory, and
 *  Timer tests.
 *
 ***************************************************************************/

