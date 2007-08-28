/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/vgatypes.h,v 1.0 2001/06/15 21:23:00 dawes Exp $ */
/*
 * General type definitions for universal mode switching modules
 *
 * Copyright 2002, 2003 by Thomas Winischhofer, Vienna, Austria
 *
 * If distributed as part of the linux kernel, the contents of this file
 * is entirely covered by the GPL.
 *
 * Otherwise, the following terms apply:
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holder not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: 	Thomas Winischhofer <thomas@winischhofer.net>
 *		Silicon Integrated Systems
 *
 */
#ifndef _VGATYPES_
#define _VGATYPES_

#ifdef LINUX_XF86
#include "xf86Pci.h"
#endif

#ifdef LINUX_KERNEL  /* We don't want the X driver to depend on kernel source */
#include <linux/ioctl.h>
#endif

#ifndef TC
#define far
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef NULL
#define NULL    0
#endif

#ifndef CHAR
typedef char CHAR;
#endif

#ifndef SHORT
typedef short SHORT;
#endif

#ifndef LONG
typedef long  LONG;
#endif

#ifndef UCHAR
typedef unsigned char UCHAR;
#endif

#ifndef USHORT
typedef unsigned short USHORT;
#endif

#ifndef ULONG
typedef unsigned long ULONG;
#endif

#ifndef PUCHAR
typedef UCHAR far *PUCHAR;
#endif

#ifndef PUSHORT
typedef USHORT far *PUSHORT;
#endif

#ifndef PULONG
typedef ULONG far *PULONG;
#endif

#ifndef PVOID
typedef void far *PVOID;
#endif
#ifndef VOID
typedef void VOID;
#endif

#ifndef BOOLEAN
typedef UCHAR BOOLEAN;
#endif

#ifndef bool
typedef UCHAR bool;
#endif

#ifndef VBIOS_VER_MAX_LENGTH
#define VBIOS_VER_MAX_LENGTH         4
#endif

#ifndef LINUX_KERNEL   /* For kernel, this is defined in sisfb.h */
#ifndef SIS_CHIP_TYPE
typedef enum _SIS_CHIP_TYPE {
    SIS_VGALegacy = 0,
#ifdef LINUX_XF86
    SIS_530,	/* TW */
    SIS_OLD,	/* TW */
#endif
    SIS_300,
    SIS_630,
    SIS_730,
    SIS_540,
    SIS_315H,   /* SiS 310 */
    SIS_315,
    SIS_315PRO, /* SiS 325 */
    SIS_550,
    SIS_650,
    SIS_740,
    SIS_330,
    SIS_660,
    SIS_760,
    MAX_SIS_CHIP
} SIS_CHIP_TYPE;
#endif
#endif

#ifndef SIS_VB_CHIP_TYPE
typedef enum _SIS_VB_CHIP_TYPE {
    VB_CHIP_Legacy = 0,
    VB_CHIP_301,
    VB_CHIP_301B,
    VB_CHIP_301LV,
    VB_CHIP_302,
    VB_CHIP_302B,
    VB_CHIP_302LV,
    VB_CHIP_301C,
    VB_CHIP_UNKNOWN, /* other video bridge or no video bridge */
    MAX_VB_CHIP
} SIS_VB_CHIP_TYPE;
#endif

#ifndef SIS_LCD_TYPE
typedef enum _SIS_LCD_TYPE {
    LCD_INVALID = 0,
    LCD_800x600,
    LCD_1024x768,
    LCD_1280x1024,
    LCD_1280x960,
    LCD_640x480,
    LCD_1600x1200,
    LCD_1920x1440,
    LCD_2048x1536,
    LCD_320x480,       /* FSTN, DSTN */
    LCD_1400x1050,
    LCD_1152x864,
    LCD_1152x768,
    LCD_1280x768,
    LCD_1024x600,
    LCD_640x480_2,     /* FSTN, DSTN */
    LCD_640x480_3,     /* FSTN, DSTN */
    LCD_848x480,
    LCD_CUSTOM,
    LCD_UNKNOWN
} SIS_LCD_TYPE;
#endif

#ifndef PSIS_DSReg
typedef struct _SIS_DSReg
{
  UCHAR  jIdx;
  UCHAR  jVal;
} SIS_DSReg, *PSIS_DSReg;
#endif

#ifndef SIS_HW_DEVICE_INFO

typedef struct _SIS_HW_DEVICE_INFO  SIS_HW_DEVICE_INFO, *PSIS_HW_DEVICE_INFO;

typedef BOOLEAN (*PSIS_QUERYSPACE)   (PSIS_HW_DEVICE_INFO, ULONG, ULONG, ULONG *);

struct _SIS_HW_DEVICE_INFO
{
    PVOID  pDevice;              /* The pointer to the physical device data structure
                                    in each OS or NULL for unused. */
    UCHAR  *pjVirtualRomBase;    /* base virtual address of VBIOS ROM Space */
                                 /* or base virtual address of ROM image file. */
                                 /* if NULL, then read from pjROMImage; */
                                 /* Note:ROM image file is the file of VBIOS ROM */

    BOOLEAN UseROM;		 /* TW: Use the ROM image if provided */

    UCHAR  *pjCustomizedROMImage;/* base virtual address of ROM image file. */
                                 /* wincE:ROM image file is the file for OEM */
                                 /*       customized table */
                                 /* Linux: not used */
                                 /* NT   : not used  */
                                 /* Note : pjCustomizedROMImage=NULL if no ROM image file */

    UCHAR  *pjVideoMemoryAddress;/* base virtual memory address */
                                 /* of Linear VGA memory */

    ULONG  ulVideoMemorySize;    /* size, in bytes, of the memory on the board */
    ULONG  ulIOAddress;          /* base I/O address of VGA ports (0x3B0) */
    UCHAR  jChipType;            /* Used to Identify SiS Graphics Chip */
                                 /* defined in the data structure type  */
                                 /* "SIS_CHIP_TYPE" */

    UCHAR  jChipRevision;        /* Used to Identify SiS Graphics Chip Revision */
    UCHAR  ujVBChipID;           /* the ID of video bridge */
                                 /* defined in the data structure type */
                                 /* "SIS_VB_CHIP_TYPE" */

    USHORT usExternalChip;       /* NO VB or other video bridge (other than  */
                                 /* SiS video bridge) */
                                 /* if ujVBChipID = VB_CHIP_UNKNOWN, */
                                 /* then bit0=1 : LVDS,bit1=1 : trumpion, */
                                 /* bit2=1 : CH7005 & no video bridge if */
                                 /* usExternalChip = 0. */
                                 /* Note: CR37[3:1]: */
                                 /*             001:SiS 301 */
                                 /*             010:LVDS */
                                 /*             011:Trumpion LVDS Scaling Chip */
                                 /*             100:LVDS(LCD-out)+Chrontel 7005 */
                                 /*             101:Single Chrontel 7005 */
				 /* TW: This has changed on 315 series! */

    ULONG  ulCRT2LCDType;        /* defined in the data structure type */
                                 /* "SIS_LCD_TYPE" */
                                     
    BOOLEAN bIntegratedMMEnabled;/* supporting integration MM enable */
                                      
    BOOLEAN bSkipDramSizing;     /* True: Skip video memory sizing. */
    PSIS_DSReg  pSR;             /* restore SR registers in initial function. */
                                 /* end data :(idx, val) =  (FF, FF). */
                                 /* Note : restore SR registers if  */
                                 /* bSkipDramSizing = TRUE */

    PSIS_DSReg  pCR;             /* restore CR registers in initial function. */
                                 /* end data :(idx, val) =  (FF, FF) */
                                 /* Note : restore cR registers if  */
                                 /* bSkipDramSizing = TRUE */

    PSIS_QUERYSPACE  pQueryVGAConfigSpace; /* Get/Set VGA Configuration  */
                                           /* space */
 
    PSIS_QUERYSPACE  pQueryNorthBridgeSpace;/* Get/Set North Bridge  */
                                            /* space  */

    UCHAR  szVBIOSVer[VBIOS_VER_MAX_LENGTH];

    UCHAR  pdc;			/* TW: PanelDelayCompensation */
    
#ifdef LINUX_KERNEL
    BOOLEAN Is301BDH;
#endif        

#ifdef LINUX_XF86
    PCITAG PciTag;		/* PCI Tag for Linux XF86 */
#endif
};
#endif


/* TW: Addtional IOCTL for communication sisfb <> X driver        */
/*     If changing this, sisfb.h must also be changed (for sisfb) */

#ifdef LINUX_XF86  /* We don't want the X driver to depend on the kernel source */

/* TW: ioctl for identifying and giving some info (esp. memory heap start) */
#define SISFB_GET_INFO    0x80046ef8  /* Wow, what a terrible hack... */

/* TW: Structure argument for SISFB_GET_INFO ioctl  */
typedef struct _SISFB_INFO sisfb_info, *psisfb_info;

struct _SISFB_INFO {
	unsigned long sisfb_id;         /* for identifying sisfb */
#ifndef SISFB_ID
#define SISFB_ID	  0x53495346    /* Identify myself with 'SISF' */
#endif
 	int    chip_id;			/* PCI ID of detected chip */
	int    memory;			/* video memory in KB which sisfb manages */
	int    heapstart;               /* heap start (= sisfb "mem" argument) in KB */
	unsigned char fbvidmode;	/* current sisfb mode */

	unsigned char sisfb_version;
	unsigned char sisfb_revision;
	unsigned char sisfb_patchlevel;

	unsigned char sisfb_caps;	/* sisfb's capabilities */

	int    sisfb_tqlen;		/* turbo queue length (in KB) */

	unsigned int sisfb_pcibus;      /* The card's PCI ID */
	unsigned int sisfb_pcislot;
	unsigned int sisfb_pcifunc;

	unsigned char sisfb_lcdpdc;
	
	unsigned char sisfb_lcda;

	unsigned long sisfb_vbflags;
	unsigned long sisfb_currentvbflags;

	int sisfb_scalelcd;
	unsigned long sisfb_specialtiming;

	char reserved[219]; 		/* for future use */
};
#endif

#ifndef BUS_DATA_TYPE
typedef enum _BUS_DATA_TYPE {
    ConfigurationSpaceUndefined = -1,
    Cmos,
    EisaConfiguration,
    Pos,
    CbusConfiguration,
    PCIConfiguration,
    VMEConfiguration,
    NuBusConfiguration,
    PCMCIAConfiguration,
    MPIConfiguration,
    MPSAConfiguration,
    PNPISAConfiguration,
    MaximumBusDataType
} BUS_DATA_TYPE, *PBUS_DATA_TYPE;
#endif

#ifndef PCI_TYPE0_ADDRESSES
#define PCI_TYPE0_ADDRESSES             6
#endif

#ifndef PCI_TYPE1_ADDRESSES
#define PCI_TYPE1_ADDRESSES             2
#endif

#ifndef PCI_COMMON_CONFIG
typedef struct _PCI_COMMON_CONFIG {
    USHORT  VendorID;                   /* (ro)                 */
    USHORT  DeviceID;                   /* (ro)                 */
    USHORT  Command;                    /* Device control       */
    USHORT  Status;
    UCHAR   RevisionID;                 /* (ro)                 */
    UCHAR   ProgIf;                     /* (ro)                 */
    UCHAR   SubClass;                   /* (ro)                 */
    UCHAR   BaseClass;                  /* (ro)                 */
    UCHAR   CacheLineSize;              /* (ro+)                */
    UCHAR   LatencyTimer;               /* (ro+)                */
    UCHAR   HeaderType;                 /* (ro)                 */
    UCHAR   BIST;                       /* Built in self test   */

    union {
        struct _PCI_HEADER_TYPE_0 {
            ULONG   BaseAddresses[PCI_TYPE0_ADDRESSES];
            ULONG   CIS;
            USHORT  SubVendorID;
            USHORT  SubSystemID;
            ULONG   ROMBaseAddress;
            ULONG   Reserved2[2];

            UCHAR   InterruptLine;      /*                    */
            UCHAR   InterruptPin;       /* (ro)               */
            UCHAR   MinimumGrant;       /* (ro)               */
            UCHAR   MaximumLatency;     /* (ro)               */
        } type0;


    } u;

    UCHAR   DeviceSpecific[192];

} PCI_COMMON_CONFIG, *PPCI_COMMON_CONFIG;
#endif

#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field)    ((LONG)&(((type *)0)->field))
#endif

#ifndef PCI_COMMON_HDR_LENGTH
#define PCI_COMMON_HDR_LENGTH (FIELD_OFFSET (PCI_COMMON_CONFIG, DeviceSpecific))
#endif

#endif

