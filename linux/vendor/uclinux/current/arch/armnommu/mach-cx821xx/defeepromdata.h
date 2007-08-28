/****************************************************************************
*
*	Name:			defeepromdata.h
*
*	Description:	Default USB enummeration data, in case the EEPROM's
*					not programmed
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
*  $Modtime: 2/28/02 7:58a $
****************************************************************************/

#ifndef DEFEEPROMDATA_H
#define DEFEEPROMDATA_H

/* String lengths */
#define  LANG_ID_STR_LEN			4
#define  MFR_ID_STR_LEN          4
#define  PROD_ID_STR_LEN			30
#define  SERIAL_NUM_STR_LEN		18
#define  MAC_ADDRESS_STR_LEN		6

/* WARNING - THESE SIZES MUST BE DWORD ALIGNED */
#define TBL_HEADER_SIZE          32
#define	DESC_TBL_SIZE			   20
#define	DEV_TBL_SIZE			   68
#define	LANG_ID_TBL_SIZE		   4
#define	MFR_ID_TBL_SIZE			4
#define	PROD_ID_TBL_SIZE		   32
#define	SERIAL_NUM_TBL_SIZE		20
#define	MAC_ADDRESS_TBL_SIZE		8

/* WARNING - THESE OFFSETS MUST BE DWORD ALIGNED */
#define DESC_TBL_START  			( TBL_HEADER_SIZE )
#define DEV_TBL_START	   		( DESC_TBL_START + DESC_TBL_SIZE )
#define LANG_ID_TBL_START	   	( DEV_TBL_START + DEV_TBL_SIZE )
#define MFR_ID_TBL_START		   ( LANG_ID_TBL_START + LANG_ID_TBL_SIZE )
#define PROD_ID_TBL_START		   ( MFR_ID_TBL_START + MFR_ID_TBL_SIZE )
#define SERIAL_NUM_TBL_START	   ( PROD_ID_TBL_START + PROD_ID_TBL_SIZE )
#define MAC_ADDRESS_TBL_START	   ( SERIAL_NUM_TBL_START + SERIAL_NUM_TBL_SIZE )

struct DefEEPROMDataStruct
{
	UINT32 uSig;
	UINT8* pPtrs[ 7 ];
	UINT8  uData[ 480 ];
};

static struct DefEEPROMDataStruct DefEEPROMData =
{
   EEPROM_SIG,

   /* Pointers to the descriptor, config, language, and string tables */
   {
	   ( UINT8* )DESC_TBL_START,			/* Device dexcriptor table */
	   ( UINT8* )DEV_TBL_START,			/* Device configuration table */
	   ( UINT8* )LANG_ID_TBL_START,		/* Language ID table */
	   ( UINT8* )MFR_ID_TBL_START,	   /* Manufacturer string */
	   ( UINT8* )PROD_ID_TBL_START,		/* Product string */
	   ( UINT8* )SERIAL_NUM_TBL_START,	/* Serial number string */
	   ( UINT8* )MAC_ADDRESS_TBL_START	/* Mac Address string */
   },

   { 
      /* DEVICE DESCRIPTOR TABLE */
      DEV_DESC_LENGTH,	               /* size in bytes */
	   DESC_TYPE_DEV,		               /* device type */
	   (UINT8)USB_SPEC_110,	            /* USB version 1.1 */
	   (UINT8)(USB_SPEC_110>>8),  
	   P50_DEV_CLASS,					      /* Device Class */
	   P50_DEV_SUBCLASS,				      /* Subclass */
	   P50_DEV_PROTOCOL,				      /* Device protocol */
	   USB_MAX_PACKET_SIZE,		         /* Maximum packet size */
	   (UINT8)CONEXANT_VENDOR_ID,			/* Conexant Vendor ID */
	   (UINT8)(CONEXANT_VENDOR_ID>>8),
	   (UINT8)P50_PRODUCT_ID,				/* Product ID */
	   (UINT8)(P50_PRODUCT_ID>>8),
	   (UINT8)P50_DEV_ID,				   /* Device Ver 1.0 */
	   (UINT8)(P50_DEV_ID>>8),
	   DESC_STRING_MANUFACTURER,
	   DESC_STRING_PRODUCT,
	   DESC_STRING_SERIAL,
	   MAX_NUM_CONFIG,					   /* Number of Configuration */
	   
      0, 0,                            /* Padding for DWORD alignment */

	   /* DEVICE CONFIGURATION TABLE */
      /* Config descriptor section: 9 bytes*/
      CONFIG_DESC_LENGTH,					/* size in bytes */
      DESC_TYPE_CONFIG,	               /* Config type */
      (UINT8) (CONFIG_DESC_LENGTH + INTER_DESC_LENGTH + (EP_DESC_LENGTH * (MAX_NUM_ENDPOINT - 2))),
      (UINT8) ((CONFIG_DESC_LENGTH + INTER_DESC_LENGTH + (EP_DESC_LENGTH * (MAX_NUM_ENDPOINT - 2))) >> 8),
      NUM_OF_INTERFACE,    				/* Total # of Interface */
      CONFIG_VALUE,   				      /* Config ID */
      CONFIG_STR,   				         /* Config str. index */
      CONFIG_ATTRIBUTES,  				   /* Attribute => bus power,remote wake up */
      MAX_POWER_CONSUME,   				/* Max power = 500 mA */
     
      /* Interface descriptor section: 9 bytes*/
      INTER_DESC_LENGTH,					/* Length */
      DESC_TYPE_INTER ,                /* Interface type */
      INTER_NUM,        					/* Interface ID */
      ALT_SETTING,					      /* Alt Interface for this one */
      MAX_NUM_ENDPOINT - 2,				/* Number of physical endpoints used by this interface */
      INTER_CLASS,					      /* Class code */
      INTER_SUBCLASS,   					/* Sub Class code */
      INTER_PROTOCOL,   					/* Protocol */
      INTER_STR,					         /* Str. index */
           
      /* Endpoint descriptor section: 7 bytes  */
      EP_DESC_LENGTH,					   /* Length */
      DESC_TYPE_EP,		               /* EP type  */
      0x80 | ENDPOINT1,	               /* EP1 IN */
      EP_BULK,			                  /* EP type */
      (UINT8) BULK_MAX_PACKET_SIZE,		/* LSB Max Packet size */
      (UINT8) (BULK_MAX_PACKET_SIZE >> 8),  /* MSB Max Packet size */
      BULK_POLL_INTERVAL,					/* Polling Interval */

      EP_DESC_LENGTH,					   /* Length  */
      DESC_TYPE_EP,		               /* EP type */
      0x00 | ENDPOINT1,	               /* EP1 OUT */
      EP_BULK,			                  /* EP type */
      (UINT8) BULK_MAX_PACKET_SIZE,		/* LSB Max Packet size */
      (UINT8) (BULK_MAX_PACKET_SIZE >> 8),  /* MSB Max Packet size */
      BULK_POLL_INTERVAL,					/* Polling Interval */

      EP_DESC_LENGTH,					   /* Length  */
      DESC_TYPE_EP,		               /* EP type */
      0x80 | ENDPOINT2,	               /* EP2 IN  */
      EP_BULK,			                  /* EP type */
      (UINT8) BULK_MAX_PACKET_SIZE,		/* LSB Max Packet size */
      (UINT8) (BULK_MAX_PACKET_SIZE >> 8),  /* MSB Max Packet size */
      BULK_POLL_INTERVAL,					/* Polling Interval */

      EP_DESC_LENGTH,					   /* Length  */
      DESC_TYPE_EP,		               /* EP type */
      0x00 | ENDPOINT2,	               /* EP2 OUT */
      EP_BULK,			                  /* EP type */
      (UINT8) BULK_MAX_PACKET_SIZE,		/* LSB Max Packet size */
      (UINT8) (BULK_MAX_PACKET_SIZE >> 8),  /* MSB Max Packet size */
      BULK_POLL_INTERVAL,					/* Polling Interval */

      EP_DESC_LENGTH,					   /* Length  */
      DESC_TYPE_EP,		               /* EP type */
      0x80 | ENDPOINT3,	               /* EP3 IN  */
      EP_BULK,			                  /* EP type */
      (UINT8) BULK_MAX_PACKET_SIZE,		/* LSB Max Packet size */
      (UINT8) (BULK_MAX_PACKET_SIZE >> 8),  /* MSB Max Packet size */
      BULK_POLL_INTERVAL,					/* Polling Interval */

      EP_DESC_LENGTH,					   /* Length  */
      DESC_TYPE_EP,		               /* EP type */
      0x00 | ENDPOINT3,	               /* EP3 OUT */
      EP_BULK,			                  /* EP type */
      (UINT8) BULK_MAX_PACKET_SIZE,		/* LSB Max Packet size */
      (UINT8) (BULK_MAX_PACKET_SIZE >> 8),  /* MSB Max Packet size */
      BULK_POLL_INTERVAL,					/* Polling Interval */

      EP_DESC_LENGTH,					   /* Length  */
      DESC_TYPE_EP,		               /* EP type */
      0x80 | ENDPOINT4,	               /* EP4 IN  */
      EP_INTERRUPT,		               /* EP type */
      (UINT8) INT_MAX_PACKET_SIZE,     /* LSB Max packet size */
      (UINT8) (INT_MAX_PACKET_SIZE >> 8), /* MSB Max packet size */
      INT_POLL_INTERVAL,

      0,					                  /* Padding, for DWORD alignment */

      /* LANGUAGE ID TABLE */
      LANG_ID_STR_LEN,	               /* Total size in bytes */
      DESC_TYPE_STRING,	               /* String type */
      (UINT8) USB_MS_ENGLISH_ID,		   /* LSB Language ID code */
      (UINT8) (USB_MS_ENGLISH_ID >> 8),/* MSB Language ID code */

	   /* MANUFACTURER STRING TABLE */
	   MFR_ID_STR_LEN,		            /* Total size in bytes */
	   DESC_TYPE_STRING,	               /* String type */

		/* The actual string, in unicode.  Number of bytes must match size */
	   0x00, '-',

	   /* PRODUCT ID STRING */
	   PROD_ID_STR_LEN,	               /* Total size in bytes */
	   DESC_TYPE_STRING,	               /* String type */

	   /* The actual string, in unicode.  Number of bytes must match size */
	   'A', 0x00, 'D', 0x00, 'S', 0x00, 'L', 0x00, ' ',
	   0x00, 'U', 0x00, 'S', 0x00, 'B', 0x00, ' ',
	   0x00, 'M', 0x00, 'O', 0x00, 'D', 0x00, 'E', 0x00, 'M', 0x00,
	   0x00, 0x00,		                  /* Padding for DWORD alignment */

	   /* SERIAL NUMBER STRING */
	   SERIAL_NUM_STR_LEN,	            /* Total size in bytes */
	   DESC_TYPE_STRING,  	            /* String type */

	   /* The actual string, in unicode.  Number of bytes must match size */
	   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	   0x00, 0x00,		                  /* Padding for DWORD alignment */

	   /* MAC ADDRESS STRING */
		/* This is a dummy value */
		0x00,	0x50,	0x00,	0xCD,	0x30,	0x02,	0x00,	0x00
   }
};

#endif

/****************************************************************************
 *
 *  $Log: defeepromdata.h,v $
 *  Revision 1.1  2003/08/28 11:41:23  davidm
 *
 *  Add in the Conexant platform files
 *
 *  Revision 1.1  2003/06/29 14:28:17  gerg
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
 *    Rev 1.4   27 Jul 2000 17:00:32   nguyenpa
 * The pointer offset in the default SW 
 * eeprom table was not correct.
 * 
 *    Rev 1.3   20 Jul 2000 13:40:36   nguyenpa
 * Change the default MAC address in case
 * the EEPROM is not present.
 * 
 *    Rev 1.2   Jul 18 2000 17:03:28   nguyenpa
 * Add code to read EMAC address from
 * EEPROM into BSP HW structure.
 * 
 * 
 *    Rev 1.1   Jun 29 2000 11:13:26   nguyenpa
 * Change the byte reversal problem
 * with PRODUCT ID STRING 
 * 
 *    Rev 1.0   Mar 28 2000 16:34:16   nguyenpa
 *  
 * 
 *    Rev 1.1   03 Mar 2000 15:05:22   blackldk
 * More default value changes
 * 
 *    Rev 1.0   28 Feb 2000 17:15:50   blackldk
 * Bootloader ARM project
 * 
 ***************************************************************************/

