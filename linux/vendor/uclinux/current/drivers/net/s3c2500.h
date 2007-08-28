/***********************************************************************/
/*                                                                     */
/* Samsung s3c2500x/a/b Ethernet Driver Header                         */
/* Copyright 2002 Arcturus Networks Inc.                               */
/*                                                                     */
/***********************************************************************/
/* s3c2500.h */
/*
 * modification history
 * --------------------
 *
 * 	05/13/01 MaTed <MaTed@ArcturusNetworks.com>
 *			- Initial port to s3c2500x using s3c4530 as base
 *			- * Nomemclature change frame descriptors are now buffer
 *				descriptors - there are no frame descriptors, make the
 *				change in your mind for now
 *			- ** NOTE: Section Numbers refer to an obsoleted version
 *			  **       of the S3c2500X manual :-(
 *	10/22/01 MaTed
 *			- Initial version for linux - Samsung
 *			- please set TABS == 4
 *
 */

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/if.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
//#include <linux/mm.h>
//#include <asm/bootst.h>		// only for bgetenv ...MaTed---
#include <asm/arch/irq.h>
#include <asm/arch/hardware.h>
#include <asm/irq.h>

#if 0	// do we need this ...MaTed---
#include <asm/addrspace.h>
#endif

/*************************
 * Configuration
 * FIXME: These should move up to configuration
 *************************/
#define CONFIG_WBUFFER	0				/* 0 => no wbuffer 
										   => no write caching */
#define CONFIG_ETH_DMA_PRIORITY 1		/* we will change the priority 
										   of the DMA channels */
#define CONFIG_DMA_PRIORITY_ROBIN 0		/* force the DMA priority type 
										   to round-robin
										   (0=> fixed priority */
/****************
 * DEBUG MACROS
 ***************/
/* use 0 for production, 1 for verification, > 2 for debug 	*/
/* NOTE: NET_DEBUG must be defined ...MaTed---				*/
#ifndef NET_DEBUG
#define NET_DEBUG 1		// lots of debug at level 6 (highest so far is 7)
//#define NET_DEBUG 3		// lots of debug at level 6 (highest so far is 7)
//#define NET_DEBUG 7		// lots of debug at level 6 (highest so far is 7)
#endif

#if NET_DEBUG > 0
// NOTE: NO panic used when in a driver - I might be in the kernel
#define ASSERT(expr) \
  if(!(expr)) \
  { \
    printk( "\n" __FILE__ ":%d: Assertion " #expr " failed!\n",__LINE__); \
  }
#define TDEBUG(x...) printk(KERN_DEBUG ## x)
#else
#define ASSERT(expr)
#define TDEBUG(x...)
#endif

/********************
 * Macro Definitions
 ********************/
#ifndef ___align16
#define ___align16 __attribute__((__aligned__(16)))
#endif 
/*
 * Setup some shorthand
 */
//	#define CSR  ((MAC_CSR)(dev->offset))

// so we can source debug by nulling this	
#define _INLINE_ inline

// just some more shorthand
#define ND struct net_device *

/***********************
 * Replacement Defines
 ***********************/
#define NUM_HW_ADDR		6		// number of bytes in ethernet HW address 

/***************************************
 * CONFIGURATION
 ***************************************/
// Only one of the following can be defined
#if defined(CONFIG_BOARD_EVS3C2500RGP) || defined(CONFIG_BOARD_S3C2500REFRGP)
#define	PHY_INTEL	1	/* define if Intel PHY - LEDs and some others 	*/
						/*  Forces Kendin switch on primary channel		*/
#endif
#ifdef CONFIG_BOARD_SMDK2500
#define	PHY_LSI		1	/* define if LSI L80225						  	*/
#endif

// If this is set, the TxPktCompletion (free's tx'd packets) will be 
//  called at the end of the send_pkt routine instead of the Tx completion
//  interrupt.
// No tx Completion interrupt at end of packet, just seems to take up time
#define NO_TX_COMP_INT	1

// Have to make sure that destructor is always called and
//  no one ever frees the skb data before I trust this
//  would have a very hard bug to trace or replicate ...MaTed---
// **** This is disabled because in 2.4.17, some SKBs do NOT call the
// **** destructor routine => memory leak. Might want to turn it back
// **** on if the problem(s) are fixed -- should be MORE efficient than
// **** repeatedly deleting and creating SKB's
#undef ZERO_COPY_BUFFER

// IOCTL setup not fully implemented, just some routines
#undef ENABLE_IOCTL

/***********************************
 *
 * S3c2500 Memory configuration defines
 *
 ***********************************/
#if  (L1_CACHE_BYTES == 32)
#define L1_CACHE_SHIFT 5
#elif (L1_CACHE_BYTES == 16)
  #define L1_CACHE_SHIFT 4
  #else
    **************** BAD L1 CACHE LINE not 16 or 32 ***********
#endif

/***********************************
 *
 * S3c2500 Ethernet MAC Configuration
 *
 ***********************************/
#define HW_MAX_CAM		((32 * 4) / NUM_HW_ADDR)	/* number of entries 
													   in CAM for address	*/
#define HW_MAX_ADDRS	HW_MAX_CAM - 5		// maximum usable ARC entries
											//  0, 1 and 18 reserved
											// we ignore 19 and 20
#define ETH_MAXUNITS	2		// number of ethernet channels
#define ETH_MIN_LEN		64		// minimum length to xmit is 64
								//  Verify OK for all protocols
/* * * *  Tunable defines * * * */
/**** NOTE: Next 2 parameters must be power of 2 ****/
/****       Must match with SET_BRxNBD 			 ****/
#define RX_NUM_FD		128		// Max number of receive frames to set up
								// (HARD MAX)
/**** See SET_BTxNBD for matching value (power of 2) ...MaTed--- ****/
#if NO_TX_COMP_INT
#define TX_NUM_FD		64		// Maximum number of transmit frames to set up
#else
#define TX_NUM_FD		64		// Maximum number of transmit frames to set up
#endif
/* The number of low I/O ports used by the ethercard.????...MaTed---????? */
#define MY_TX_TIMEOUT  ((400*HZ)/1000)
#define MDELAY_RESET	500		// Set for 1/2 Sec - needs checking to shorten

/*********************
 * Error Return Codes
 *********************/
typedef int					RET_CODE;

#define	S3C2500_SUCCESS		0		// function call was successful
#define S3C2500_FAIL		-1		// general error code
#define S3C2500_TIMEOUT		-1000	// timeout error

/**********************
 * S3C2500 Buffer Sizes
 **********************/
#define MAX_PKT_SIZE	1536	// bigger than need be ? (but 3x512)
//#define MAX_FRAME_SIZE	MAX_PKT_SIZE - 4 // controller is four less
//#define MAX_FRAME_SIZE	1522

// Realize that this is bigger than needs to be - does this cause a problem?
//   Yes for MTU generation ??? Force 1514 ...MaTed---
//  -- the upper layers break down tx stuff, rx?? ...MaTed---
//#define PKT_BUF_SZ  	MAX_PKT_SIZE - 22	// Size of each Rx buffer
#define PKT_BUF_SZ  	1520	// Size of each Rx buffer

/* Reserve all the stuff - nothing between used (each adapter has this 
 *  size from the base address of the ...MaTed--- */
#define MAC_IO_EXTENT	(0xB00FC - 0xA0000)

/****************
 * EXTERNS
 ****************/


/*********************************************************************
 * typedefs
 * - wish these were predefined somewhere else ...MaTed---
 *********************************************************************/
typedef unsigned int	UINT;
typedef	unsigned long	ULONG;
typedef unsigned long	UINT32;
typedef unsigned short	UINT16;
typedef unsigned char	UINT8;
typedef	unsigned char	UCHAR;

/***************************************************
 *  Ethernet MAC Base address and interrupt vectors
 ***************************************************/
// Each Mac has Two interrupts
#define ETH0_TX_IRQ		SRC_IRQ_ETH_TX0	/* TX IRQ for ETHERNET 0			*/
#define ETH0_RX_IRQ		SRC_IRQ_ETH_RX0	/* RX IRQ for ETHERNET 0			*/
#define ETH1_TX_IRQ		SRC_IRQ_ETH_TX1	/* TX IRQ for ETHERNET 1			*/
#define ETH1_RX_IRQ		SRC_IRQ_ETH_RX1	/* RX IRQ for ETHERNET 1			*/

/***********************************************
 * Misc Addresses
 ***********************************************/
#if 0
#define	WriteBufferEn	0x04	/* Write Buffer enable bit mask				*/
#endif

/*********************************************************
 * Interrupt Controller (we ignore External ints for now)
 *********************************************************/
#define INTMOD		0xF0140000	// Internal interrupt mode register
                                //  Setting a bit => FIQ mode
#define INTMSK      0xF0140008	// Interrupt Mask 

#define INTPND      (SRB_BASE+0x4004)

// Interrupt Priority registers (16.4.3)
#define INTPRI0     0xF0140020
#define INTPRI1     0xF0140024
#define INTPRI2     0xF0140028
#define INTPRI3     0xF014002C
#define INTPRI4     0xF0140030
#define INTPRI5     0xF0140034
#define INTPRI6     0xF0140038
#define INTPRI7     0xF014003C
#define INTPRI8     0xF0140040
#define INTPRI9     0xF0140044

#define INTOFFSET   0xF014001C
// Returns valuse for highest priorty int pending (0x15->0x18 for Ethernet)
#define INTOSET_FIQ 0xF0140018
#define INTOSET_IRQ 0xF014001C

/***********************************************
 * Ethernet BDMA Register Address
 ***********************************************/
#define BDMATXCON	0x0000		/* Buffered DMA Transmit Control			*/
#define BDMARXCON	0x0004		/* Buffered DMA Receive Control				*/
#define BDMATXDPTR	0x0008		/* BDMA Transmit Frame Descriptor Start
								   Address									*/
#define BDMARXDPTR	0x000C		/* BDMA Receive Frame Descriptor Start
								   Address									*/
#define BTXBDCNT	0x0010		/* BDMA Tx buffer descriptor counter		*/
#define BRXBDCNT	0x0014		/* BDMA Rx buffer descriptor counter		*/
#define BMTXINTEN	0x0018		/* BDMA/MAC Tx Interrupt enable
								   register						  			*/
#define BMRXINTEN	0x001C		/* BDMA/MAC Rx Interrupt enable
								   register									*/
#define BMTXSTAT	0x0020		/* BDMA/MAC Tx Status register				*/
#define BMRXSTAT	0x0024		/* BDMA/MAC Rx Status register				*/
#define BDMARXLEN  	0x0028		/* BDMA Receive frame Size					*/
#define CFTXSTAT	0x0030		/* Transmit control frame status			*/
/***********************************************
 * Ethernet MAC Register Address
 ***********************************************/
#define MACCON		0x10000		/* MAC control								*/
#define CAMCON		0x10004		/* CAM control								*/
#define MACTXCON    0x10008		/* Transmit control							*/
#define MACTXSTAT   0x1000C		/* Transmit status							*/
#define MACRXCON    0x10010		/* Receive control							*/
#define MACRXSTAT   0x10014		/* Receive status							*/
#define STADATA		0x10018		/* Station management data					*/
#define STACON		0x1001C		/* Station management control and 
								   address									*/
#define CAMEN		0x10028		/* CAM enable								*/
#define MISSCNT		0x1003C		/* Missed error count						*/
#define PZCNT		0x10040		/* Pause count								*/
#define RMPZCNT		0x10044		/* Remote pause count						*/
#define CAM     	0x10080		/* CAM content 32 words 0080-00fc			*/
/***********************************************
 * Ethernet MAC Register Offsets
 ***********************************************/
#define ETH0OFF		0xF00A0000	/* Offsetset for register - Ethernet 0		*/
#define ETH1OFF		0xF00C0000	/* Offsetset for register - Ethernet 1		*/

/*********************************************************
 * Buffered DMA Transmit Control Register Masks (7.5.1.1)
 * BDMATXCON - offset 0x0000
 *********************************************************/
#define BTxNBD		0x000f	/* BDMA Tx # of Buffer Descriptors (Power 
							   of 2)										*/
#define BTxMSL		0x0070	/* BDMA Tx Start Level							*/
#define BTxEN		0x0400	/* BDMA Tx enable NOTE: BDMATXDPTR set 1st		*/
#define BTxRS		0x0800	/* BDMA Tx reset - set to '1' to reset Tx		*/

#if NO_TX_COMP_INT
#define SET_BTxNBD	6		/*  - 64 TX Frames Descriptors
								  - Must match with TX_NUM_FD 				*/
#else
#define SET_BTxNBD	6		/*  - 64 TX Frames Descriptors
								  - Must match with TX_NUM_FD 				*/
#endif
//#define SET_BTxMSL	0x0070	/* 7/8 fill BDMA Tx Start Level				*/
//#define SET_BTxMSL	0x0060	/* 6/8 fill BDMA Tx Start Level				*/
#define SET_BTxMSL	0x0020	/* 2/8 fill BDMA Tx Start Level				*/
//#define SET_BTxMSL	0x0000	/* No wait Tx Start Level					*/

#define BTxSetup	(SET_BTxNBD | SET_BTxMSL)

/*********************************************************
 * Buffered DMA Receive Control Register (7-.5.1.2)
 * BDMARXCON - offset 0x0004
 *********************************************************/
#define BRxNBD		0x000f	/* BDMA Rx # of Buffer Descriptors (Power 
							   of 2) - 128									*/
#define BRxWA		0x0030	/* BDMA Rx word alignment (0->3 bytes)			*/
#define BRxEN		0x0400	/* BDMA Rx Enable, BDMARXDPTR must be set		*/
#define BRxRS		0x0800	/* BDMA Rx reset - set to '1' to reset Tx		*/

#define SET_BRxNBD	7		/* - 128 Rx Frames Descriptors
								 - Must match with RX_NUM_FD */

#define SET_BRxWA	0x0020	/* Set for 2 byte alignment, LINUX TCP/IP stack */

/*  RxAlign                     bits 4 to 5   */
#define BRxAlignSkip0	(0x00<<4)	/* Do not skip any bytes on rx			*/
#define BRxAlignSkip1	(0x01<<4)	/* Skip 1st byte on rx					*/
#define BRxAlignSkip2	(0x02<<4)	/* Skip 2nd byte on rx					*/
#define BRxAlignSkip3	(0x03<<4)	/* Skip 3rd byte on rx					*/

#define	BRxSetup_RESET	(SET_BRxNBD | BRxAlignSkip2 )

#if 0 // previous setups from 4530 code
#define BRxSetup	(BRxBurstSize | BRxSTSKO | BRxMAINC | BRxDIE |
					 BRxNLIE | BRxNOIE | BRxMSOIE | BRxLittle |
					 BRxAlignSkip2 | BRxEN | BRxEmpty )
#define BRxSetup	(BRxBurstSize | BRxSTSKO | BRxMAINC | BRxDIE | \
					 BRxNLIE | BRxNOIE | BRxLittle | \
					 BRxAlignSkip2 | BRxEN )
#endif
/*********************************************************
 * BDMA Transmit Buffer Descriptor Start Address (7.5.1.3)
 * BDMATXDPTR - offset 0x0008
 *********************************************************/

/*********************************************************
 * BDMA Receive Buffer Descriptor Start Address (7.5.1.4)
 * BDMARXDPTR - offset 0x000C
 *********************************************************/

/*********************************************************
 * BDMA Transmit Buffer Counter (7.5.1.5)
 * BTXBDCNT - offset 0x0010
 *********************************************************/
#define BDMATXCNT	0x0fff	/* BDMA Tx buffer descriptor Counter,
							   Current address = BDMATXDPTR 
							   + (BTXBDCNT << 3)							*/
/*********************************************************
 * BDMA Receive Buffer Counter (7.5.1.6)
 * BRXBDCNT - offset 0x0014
 *********************************************************/
#define BDMARXCNT	0x0fff	/* BDMA Rx buffer descriptor Counter,
							   Current address = BDMARXDPTR 
							   + (BRXBDCNT << 3)							*/
/*********************************************************
 * BDMA/MAC Transmit Interrupt Enable (7.5.1.7)
 * BMTXINTEN - offset 0x0018
 *********************************************************/
#define ExCollIE	0x0001	/* Enable MAC Tx excessive collision-ExColl		*/
#define UnderflowIE	0x0002	/* Enable MAC Tx underflow-Underflow 		 	*/
#define DeferErrIE	0x0004	/* Enable MAC Tx deferral-DeferErr			 	*/
#define NoCarrIE	0x0008	/* Enable MAC Tx no carrier-NoCarr				*/
#define LateCollIE	0x0010	/* Enable MAC Tx late collision-LateColl		*/
#define TxParErrIE	0x0020	/* Enable MAC Tx transmit parity-TxParErr		*/
#define TxCompIE	0x0040	/* Enable MAC Tx completion-TxComp				*/
#define TxCFcompIE	0x10000	/* Tx complete to send control frame 
							   interrupt enable-TxCFcomp					*/
#define BTxNOIE		0x20000	/* BDMA Tx not owner interrupt enable-BTxNO		*/
#define BTxEmptyIE	0x40000	/* BDMA Tx Buffer empty interrupt enable
							   -BTxEmpty									*/
#if NO_TX_COMP_INT
#define TxINTMask	(ExCollIE | UnderflowIE | DeferErrIE | NoCarrIE \
                     | LateCollIE | TxParErrIE | TxCFcompIE)
	 /*
                     | LateCollIE | TxParErrIE | TxCFcompIE \
                     | BTxNOIE | BTxEmptyIE)
	 */
#else
#define TxINTMask	(ExCollIE | UnderflowIE | DeferErrIE | NoCarrIE \
                     | LateCollIE | TxParErrIE | TxCompIE | TxCFcompIE \
                     | BTxNOIE | BTxEmptyIE) 
#endif
/*********************************************************
 * BDMA/MAC Transmit Interrupt Status (7.5.1.8)
 * BDTXSTAT - offset 0x0020
 *********************************************************/
#define ExColl		0x0001	/* MAC Tx excessive collision-ExColl			*/
#define Underflow	0x0002	/* MAC Tx underflow-Underflow 		 			*/
#define DeferErr	0x0004	/* MAC Tx deferral-DeferErr			 			*/
#define NoCarr		0x0008	/* MAC Tx no carrier-NoCarr						*/
#define LateColl	0x0010	/* MAC Tx late collision-LateColl				*/
#define TxParErr	0x0020	/* MAC Tx transmit parity-TxParErr				*/
#define TxComp		0x0040	/* MAC Tx completion-TxComp						*/
#define TxCFcomp	0x10000	/* Tx complete to send control frame-TxCFcomp	*/
#define BTxNO		0x20000	/* BDMA Tx not owner-BTxNO						*/
#define BTxEmpty	0x40000	/* BDMA Tx Buffer empty-BTxEmpty				*/

/*********************************************************
 * BDMA/MAC Receive Interrupt Enable (7.5.1.9)
 * BDRXINTIE - offset 0x001C
 *********************************************************/
#define MissRollIE	0x0001	/* MAC Rx missed roll-MissRoll 			 		*/
#define AlignErrIE	0x0002	/* MAC Rx alignment-AlignErr					*/
#define CRCErrIE	0x0004	/* MAC Rx CRC error-CRCErr						*/
#define OverflowIE	0x0008	/* MAC Rx overflow-Overflow						*/
#define LongErrIE	0x0010	/* MAC Rx long error-LongErr					*/
#define RxParErrIE	0x0020	/* MAC Rx receive parity-RxParErr				*/
#define BRxDoneIE	0x10000	/* BDMA Rx done for every received 
							   frames-BRxDone								*/
#define BRxNOIE		0x20000	/* BDMA Rx not owner interrupt-BRxNO			*/
#define BRxMSOIE	0x40000	/* BDMA Rx maximum size over interrupt
							   -BRxMSO										*/
#define BRxFullIE	0x80000	/* BDMA Rx buffer(BRxBUFF) Overflow 
							   Interrupt-BRxFull							*/
#define BRxEarlyIE	0x100000	/* BDMA Rx early notification 
								   interrupt-BRxEarly						*/
#if 0	// don't want to handle all these ...MaTed---
	    // - most will done by the frame having the status bit set in the
	    // status section of the RX Frame descriptor
#define RxINTMask	(MissRollIE | AlignErrIE | CRCErrIE | OverflowIE \
                    | LongErrIE | RxParErrIE | BRxNOIE | BRxDoneIE \
                    | BRxMSOIE | BRxFullIE | BRxEarlyIE) 
#else
#define RxINTMask	( MissRollIE | BRxNOIE | BRxDoneIE ) 
#endif
/*********************************************************
 * BDMA/MAC Receive Interrupt Status (7.5.1.10)
 * BDRXSTAT - offset 0x0024
 *********************************************************/
#define MissRoll	0x0001	/* MAC Rx missed roll-MissRoll 			 		*/
#define AlignErr	0x0002	/* MAC Rx alignment-AlignErr					*/
#define CRCErr		0x0004	/* MAC Rx CRC error-CRCErr						*/
#define Overflow	0x0008	/* MAC Rx overflow-Overflow						*/
#define LongErr		0x0010	/* MAC Rx long error-LongErr					*/
#define RxParErr	0x0020	/* MAC Rx receive parity-RxParErr				*/
#define BRxDone		0x10000	/* BDMA Rx done for every received 
							   frames-BRxDone								*/
#define BRxNO		0x20000	/* BDMA Rx not owner interrupt-BRxNO			*/
#define BRxMSO		0x40000	/* BDMA Rx maximum size over interrupt
							   -BRxMSO										*/
#define BRxFull		0x80000	/* BDMA Rx buffer(BRxBUFF) Overflow 
							   Interrupt-BRxFull							*/
#define BRxEarly	0x100000	/* BDMA Rx early notification 
								   interrupt-BRxEarly						*/
#define BRxFRF		0x200000	/* An additional data frame is received
								   in the BDMA receive buffer				*/
#define BRxBUFF		0x7c00000	/* Number of frames in BRxBUFF				*/

/*********************************************************
 * BDMA Receive Frame Size (7.5.1.11)
 * BDRXSTAT - offset 0x0028
 *********************************************************/
#define BRxBS	0x00000FFF	/* This register value specifies the buffer
							   size allocated to each buffer descriptor.
							   Thus, for an incoming frame larger than
							   the BRxBS, multiple buffer descriptors
							   are used for the frame reception.
							   Note: BRxBS value has to keep multiples
							   of 16 in byte unit. For long packet 
							   reception larger than 1518 bytes, the 
							   BRxBS should be at least 4 bytes larger 
							   than the BRxMFS or less than 1518 bytes.		*/
#define BRxMFS	0x0FFF0000	/* BDMA Maximum Rx Frame Size (BRxMFS) This 
							   register value controls how many bytes 
							   per frame can be saved to memory. If 
							   the received frame size exceeds these 
							   values, an error condition is reported.
							   Note: BRxMFS value has to keep multiples 
							   of 16 in byte unit							*/

/*********************************************************
 * MAC Transmit Control Frame Status (7.5.2.1)
 * The transmit control frame status register, CFTXSTAT provides
 * the status of a MAC control frame as it is sent to a remote station.
 * This operation is controlled by the MSdPause bit in the transmit 
 * control register, MACTXCON. It is the responsibility of the BDMA 
 * engine to read this register and to generate an interrupt to notify
 * the system that the transmission of a MAC control packet has been
 * completed.
 * CFTXSTAT - offset 0x0030
 *********************************************************/
#define MACCTLTXSTAT	0xFFFF	/* A 16-bit value indicating the status of
								   a MAC control packet as it is sent to a 
								   remote station. Read by the BDMA engine.	*/

/*********************************************************
 * MAC Control Register  (7.5.2.2)
 * The MAC control register provides global control and status 
 * information for the MAC. The missed roll/link10 bit is a status 
 * bit. All other bits are MAC control bits. MAC control register 
 * settings affect both transmission and reception. After a reset 
 * is complete, the MAC controller clears the reset bit. Not all
 * PHYs support full-duplex operation. (Setting the MAC loopback
 * bit overrides the full-duplex bit.) Also, some 10-Mb/s PHYs may
 * interpret the loop10 bit to control different functions, and
 * manipulate the link10 bit to indicate a different status condition.
 * MACCON - offset 0x10000
 *********************************************************/
#define MHaltReq	0x0001	/* Halt request (MHaltReq) Set this bit to
							   stop data frame transmission and
							   reception as soon as Tx/Rx of any current
							   frames has been completed					*/
#define MHaltImm	0x0002	/* Halt immediate (MHaltImm) Set this bit
							   to immediately stop all transmission
							   and reception 								*/
#define MReset		0x0004	/*  Software reset (MReset) Set this bit
								to reset all MAC control and status
								register and MAC state machines. This
								bit is automatically cleared				*/
#define MFullDup	0x0008	/*  Full-duplex Set this bit to start
								transmission while reception is in
								progress									*/
#define MLoopBack	0x0010	/*  MAC loopback (MLoopBack) Set this bit
								to cause transmission signals to be
								presented as input to the receive
								circuit without leaving the controller		*/
#define MMIIOFF		0x0040	/*  MII-OFF Use this bit to select the
								connection mode. If this bit is set to
								one, 10 M bits/s interface will select
								the 10 M bits/s endec. Otherwise, the
								MII will be selected						*/
#define MLOOP10		0x0080	/*  Loop 10 Mb/s (MLOOP10) If this bit is
								set, the Loop_10 external signal is
								asserted to the 10-Mb/s endec				*/
#define MMDCOFF		0x1000	/* MDC-OFF Clear this bit to enable the MDC
							   clock generation for power management. If
							   it is set to one, the MDC clock
							   generation is disabled						*/
#define MLINK10		0x8000	/*  Link status 10 Mb/s (MLINK10),
								read-only This bit value is read as a
								buffered signal on the link 10 pin.			*/

#define MCtlReset_BUSY_LOOP	100 /* Number of loops to wait for MReset bit	*/
#define MCtlSETUP	(MFullDup)
#define MCtlFullDupShift	3
/*********************************************************
 * CAM Control Register (7.5.2.3
 *	CAMCON - offset 0x0x10004
 *********************************************************/
#define MStation	0x0001	/* Station accept (MStation) Set this bit
							   to accept unicast (i.e. station) frames  	*/
#define MGroup		0x0002	/* Group accept (MGroup) Set this bit to 
							   accept multicast (i.e. group) frames. 		*/
#define MBroad		0x0004	/* Broadcast accept (MBroad) Set this bit to
							   accept broadcast frames. 					*/
#define MNegCAM		0x0008	/* Negative CAM (MNegCAM) Set this bit to
							   enable the Negative CAM comparison mode. 	*/
#define MCompEn		0x0010	/* Compare enable (MCompEn) Set this bit to
							   enable the CAM comparison mode.				*/
#define CAMCtlSETUP     (MStation | MGroup | MBroad | MCompEn)

/*********************************************************
 * MAC Transmit Control Register (7.5.2.4)
 * MACTXCON - offset 0x10008
 *********************************************************/
#define MTxEn		0x0001	/* Transmit enable (MTxEn) Set this bit to 
							   enable transmission. To stop transmission 
							   immediately, clear the transmit enable
							   bit to  0									*/
#define MTxHalt		0x0002	/* Transmit halt request (MTxHalt) Set this
							   bit to halt the transmission after
							   completing the transmission of any
							   current frame								*/
#define MNoPad		0x0004	/* Suppress padding (MNoPad) Set this bit
							   not to generate pad bytes for frames of 
							   less than 64 bytes							*/
#define MNoCRC		0x0008	/* Suppress CRC (MNoCRC) Set this bit to 
							   suppress addition of a CRC at the end of 
							   a frame.										*/
#define MFBack		0x0010	/* Fast back-off (MFBack) Set this bit to 
							   use faster back-off times for testing 		*/
#define MNoDef		0x0020	/* No defer (MNoDef) Set this bit to disable
							   the defer counter. (The defer counter 
							   keeps counting until the carrier sense 
							   (CrS) bit is turned off.						*/
#define MSdPause	0x0040	/* Send Pause (MSdPause) Set this bit to 
							   send a pause command or other MAC control 
							   frame. The send pause bit is 
							   automatically cleared when a complete MAC 
							   control frame has been transmitted. 
							   Writing a  0  to this register bit has 
							   no effect									*/
#define MSQEn		0x0080	/* MII 10-Mb/s SQE test mode enable (MSQEn)
							   Set this bit to enable MII 10-Mb/s SQE
							   test mode									*/

#define MTxCtlSetup (0)		/* None of these on								*/

/*********************************************************
 * MAC Transmit Status Register (7.5.2.5)
 * A transmission status flag is set in the transmit status
 * register, MACTXSTAT, whenever the corresponding event
 * occurs. In addition, an interrupt is generated if the
 * corresponding enable bit in the transmit control register
 * is set. A MAC TxFIFO parity error sets TxParErr, and also
 * clears MTxEn, if the interrupt is enabled.
 * MACTXSTAT - offset 0x1000C
 *********************************************************/
#define MACTXERR	0x00ff	/*  These bits are equivalent to the
								BMTXSTAT.7~0								*/
#define MCollCnt	0x0f00	/* Transmission collision count (MCollCnt)
							   This 4-bit value is the count of 
							   collisions that occurred while 
							   successfully transmitting the frame			*/
#define MTxDefer	0x1000	/* Transmission deferred (MTxDefer) This
							   bit is set if transmission of a frame 
							   was deferred because of a delay during
							   transmission									*/
#define SQEErr		0x2000	/* Signal quality error (SQEErr) According
							   to the IEEE802.3 specification, the SQE
							   signal reports the status of the PMA (MAU
							   or transceiver) operation to the MAC
							   layer. After transmission is complete and
							   1.6 ms has elapsed, a collision detection
							   signal is issued for 1.5 ms to the MAC
							   layer. This signal is called the SQE test
							   signal. The MAC sets this bit if this
							   signal is not reported within the IFG
							   time of 6.4ms 								*/
#define MTxHalted	0x4000	/* Transmission halted (MTxHalted) This bit
							   is set if the MTxEn bit is cleared or the
							   MHaltImm bit is set							*/
#define MPaused		0x8000	/* Paused (MPaused) This bit is set if
							   transmission of frame was delayed due to
							   a Pause being received						*/
#define MCollShift	0x08	/* Amount to shift collisions */
/*********************************************************
 * MAC Receive Control Register (7.5.2.6)
 * MACRXCON - offset 0x10010
 *********************************************************/
#define MRxEn		0x0001	/* Receive enable (MRxEn) Set this bit to
							   1  to enable MAC receive operation. If
							   0 , stop reception immediately				*/
#define MRxHalt		0x0002	/* Receive halt request (MRxHalt) Set
							   this bit to halt reception after
							   completing the reception of any
							   current frame								*/
#define MLongEn		0x0004	/* Long enable (MLongEn) Set this bit to
							   receive frames with lengths greater
							   than 1518 bytes								*/
#define MShortEn	0x0008	/* Short enable (MShortEn) Set this bit
							   to receive frames with lengths less
							   than 64 bytes								*/
#define MStripCRC	0x0010	/* Strip CRC value (MStripCRC) Set this
							   bit to check the CRC, and then strip
							   it from the message							*/
#define MPassCtl	0x0020	/* Pass control frame (MPassCtl) Set this
							   bit to enable the passing of control
							   frames to a MAC client						*/
#define MIgnoreCRC	0x0040	/* Ignore CRC value (MIgnoreCRC) Set this
							   bit to disable CRC value checking			*/

// From usage- we enable rx interrupts later ...MaTed---
#define MRxCtlSETUP             (MRxEn | MStripCRC )

/*********************************************************
 * MAC Receive Status Register (7.5.2.7)
 * MACRXSTAT - offset 0x10014
 *********************************************************/
#define MACRXERR	0x00ff	/* These bits are equal to the BMRXSTAT.7~		*/
#define MRxShort	0x0100	/* Short Frame Error (MRxShort) This bit is
							   set if the frame was received with short
							   frame										*/
#define MRx10Stat	0x0200	/* Receive 10-Mb/s status (MRx10Stat) This
							   bit is set to  1  if the frame was
							   received over the 7-wire interface or to
							   0  if the frame was received over the MII	*/
#define MRxHalted	0x0400	/* Reception halted (MRxHalted) This bit is
							   set if the MRxEn bit is cleared or the
							   MHaltImm bit is set							*/
#define MCtlRecd	0x0800	/* Control frame received (MCtlRecd) This
							   bit is set if the frame received is a
							   MAC control frame (type = 0x8808), if the
							   CAM recognizes the frame address, and if
							   the frame length is 64 bytes					*/

/*********************************************************
 * MAC Station Management Data Register (7.5.2.8)
 * STADATA - offset 0x10018
 *********************************************************/
#define STAMANDATA	0xffff	/* This register contains a 16-bit data
							   value for the station management
							   function.									*/

/*********************************************************
 * MAC Station Management Data Control and Address Register 
 *		(7.5.2.9)
 * The MAC controller provides support for reading and
 * writing3 station management data to the PHY. Setting
 * options in station management registers does not
 * affect the controller. Some PHYs may not support the
 * option to suppress preambles after the first operation.
 * STACON - offset 0x1001c
 *********************************************************/
#define MPHYRegAddr	0x001F	/* PHY register address (MPHYRegAddr) A
							   5-bit address, contained in the PHY,
							   of the register to be read or written		*/
					// RGP boards have Intel PHY at address 0
#if defined(CONFIG_BOARD_EVS3C2500RGP) || defined(CONFIG_BOARD_S3C2500REFRGP)
#define MPHYHWADDR0	0x00 	/* PHY H/W Address 0x1							*/
#define MPHYHWADDR1	0x00 	/* PHY H/W Address 0x2							*/
#endif
#ifdef CONFIG_BOARD_SMDK2500	// SMDK has LSI PHYs at strappable addresses
#define MPHYHWADDR0	0x20 	/* PHY H/W Address 0x1							*/
#define MPHYHWADDR1	0x40 	/* PHY H/W Address 0x2							*/
#endif
#define MPHYaddr	0x03E0	/* PHY address (MPHYaddr) The 5-bit
							   address of the PHY device to be read
							   or written									*/
#define MPHYwrite	0x0400	/* Write (MPHYwrite) To initiate a write
							   operation, set this bit to  1 . For a
							   read operation, clear it to  0 				*/
#define MPHYbusy	0x0800	/* Busy bit (MPHYbusy) To start a read or
							   write operation, set this bit to 1.
							   The MAC controller clears the Busy bit
							   automatically when the operation is
							   completed								   	*/
#define MPreSup		0x1000	/* Preamble suppress (MPreSup) If you set
							   this bit, the preamble is not sent to
							   the PHY. If it is clear, the preamble
							   is sent										*/
#define MMDCrate	0xE000	/* MDC clock rate (MMDCrate) Controls the
							   MDC period. The default value is `011.
							   MDC period = MMDCrate ´ 4 + 32 Example)
							   MMDCrate = 011,
							   MDC period = 44 x (1/system clock			*/

/*********************************************************
 * CAM Enable Register (7.5.2.10)
 * CAMEN - offset 0x10028
 *********************************************************/
#define CAMENBIT	0x1fffff	/* Set the bits in this 21-bit value to
								   selectively enable CAM locations 20
								   through 0. To disable a CAM location,
								   clear the appropriate bit.	   			*/

/*********************************************************
 * MAC Missed Error Count Register (7.5.2.11)
 * The value in the missed error count register, MISSCNT,
 * indicates the number of frames that were discarded due
 * to various type of errors. Together with status
 * information on frames transmitted and received, the
 * missed error count register and the two pause count
 * registers provide the information required for station
 * management. Reading the missed error counter register
 * clears the register. It is then the responsibility of
 * software to maintain a global count with more bits of
 * precision. The counter rolls over from 0x7FFF to 0x8000
 * and sets the corresponding bit in the MAC control
 * register. It also generates an interrupt if the
 * corresponding interrupt enable bit is set. If station
 * management software wants more frequent interrupts,
 * you can set the missed error count register to a value
 * closer to the rollover value of 0x7FFF. For example,
 * setting a register to 0x7F00 would generate an
 * interrupt when the count value reaches 256 occurrences.
 * MISSCNTT - offset 0x1003C
 *********************************************************/
#define MissErrCnt	0xFFFF	/* (MissErrCnt) The number of valid
							   packets rejected by the MAC unit
							   because of MAC RxFIFO overflows,
							   parity errors, or because the MRxEn
							   bit was cleared. This count does not
							   include the number of packets
							   rejected by the CAM.							*/

/*********************************************************
 * MAC Received Pause Count Register (7.5.2.12)
 * The received pause count register, PZCNT, stores the
 * current value of the 16-bit received pause counter.
 * MISSCNT - offset 0x10040
 *********************************************************/
#define MACPCNT		0xFFFF	/* The count value indicates the number of
							   time slots the transmitter was paused
							   due to the receipt of control pause
							   operation frames from the MAC.				*/

/*********************************************************
 * MAC Remote Pause Count Register (7.5.2.13)
 *	RMPZCNT  - offset 0x10044
 *********************************************************/
#define MACRMPZCNT	0xFFFF	/* The count value indicates the number
							   of time slots that a remote MAC was
							   paused as a result of its sending
							   control pause operation frames.				*/

/*********************************************************
 * Content Addressable Memory (CAM) Register (7.5.2.14)
 * CAM - offset 0x10080 ~ 0x100FC
 *********************************************************/
#define CAMVAL	0xFFFFFFFF	/* The CPU uses the CAM content register as
							   data for destination address. To activate
							   the CAM function, you must set the
							   appropriate enable bits in CAM enable
							   register.									*/
/**************************************************/

/**********************************************
 * Structure Definitions
 **********************************************/
/* Combined BDMA and MAC Registers for S3c2500 */
typedef struct
{
  volatile UINT32 TxCtl;	/* 0x00 Buffered DMA transmit control reg		*/
  volatile UINT32 RxCtl;	/* 0x04 Buffered DMA receive control reg		*/
  volatile UINT32 TxPtr;	/* 0x08 Tx frame descriptor start addr			*/
  volatile UINT32 RxPtr;	/* 0x0c Rx frame descriptor start addr			*/
  volatile UINT32 TxCnt;	/* 0x10 BDMA Tx buffer descriptor counter		*/
  volatile UINT32 RxCnt;	/* 0x14 BDMA Rx buffer descriptor counter 		*/
  volatile UINT32 TxIntEn;	/* 0x18 BDMA/MAC Tx Interrupt enable
							   register 									*/
  volatile UINT32 RxIntEn;	/* 0x1c BDMA/MAC Rx Interrupt enable
							   register 									*/
  volatile UINT32 TxStat;	/* 0x20 BDMA/MAC Tx Status register				*/
  volatile UINT32 RxStat;	/* 0x24 BDMA/MAC Rx Status register				*/
  volatile UINT32 RxSz;		/* 0x28 Receive Frame Size						*/
  volatile UINT32 TxCf;		/* 0x30 Transmit control frame status			*/
  UINT8  reserve1 [0xffd0];	/* spacer to next 64KB block					*/
  volatile UINT32 MacCtl;	/* 0x10000 MAC Control							*/
  volatile UINT32 CamCtl;	/* 0x10004 CAM Control							*/
  volatile UINT32 MacTxCtl;	/* 0x10008 MAC Tx Control						*/
  volatile UINT32 MacTxStat;/* 0x1000c MAC Tx Status						*/
  volatile UINT32 MacRxCtl;	/* 0x10010 MAC Rx Control					   	*/
  volatile UINT32 MacRxStat;/* 0x10014 MAC Rx Status						*/
  volatile UINT32 SmData;	/* 0x10018 Station Management Data				*/
  volatile UINT32 SmCtl;	/* 0x1001c Station Management Control 
							   and address									*/
  UINT8 reserve2 [8];		/* spacer										*/
  volatile UINT32 CamEn;	/* 0x10028 CAM Enable							*/
  UINT8 reserve3 [16];		/* spacer										*/
  volatile UINT32 MissCnt;	/* 0x1003c Missed Error Count					*/
  volatile UINT32 PauseCnt;	/* 0x10040 Received Pause command Count	*/
  volatile UINT32 RemPauseCnt; /* 0x10044 Remote Pause Cnt (we sent 
								  PAUSE)									*/
  UINT8 reserve4 [0x38];	/* spacer										*/
  volatile UINT32 Cam[60];	/* 0x10080 -> 0x100fc, CAM Content Only 1st
							   21*6 bytes used								*/
} CSR;

/**********************************************************
 * Frame Descriptor, Buffer Descriptor related parameters.
 **********************************************************/

#define	OWNERSHIP	0x80000000	/* bit set for BDMA owns FD					*/
#define OWNERSHIP16	0x8000		/* bit set for BDMA owns FD - 16 bit access */
#if 0
#define RELOAD		0x10000		/* bit set tells addRxSKBs to allocate new
								   skb - only set when an error occurred
								   NOTE that we get to reuse the stat only
								   because we KNOW that bottom two bits of
								   16 bit are always 2 (FOR US ONLY) 		*/
#define RELOAD16	0x0001		/* 16 bit access							*/
#endif // 0 Will remove after checking manual
/*****************************
 * Rx Descriptor Control Bits
 *****************************/
#define SKIPBD	0x40000000	/* Set to skip this Buffer Descriptor when 
							   ownership is cleared							*/
#define SOFR	0x20000000	/* Set by BDMA to indicate Start of Frame 		*/
#define EOFR	0x10000000	/* Set by BDMA to indicate End of Frame			*/
#define DONEFR	0x08000000	/* Set by BDMA on the first BD when the
							   reception of a frame finished and it
							   used multiple BD's. 							*/
/*****************************
 * Rx Descriptor Status Bits
 *****************************/
             /* SET IF ....									*/
#define RxS_OvMax	0x0400		/* Over Maximum Size (BRxMFS)				*/
#define RxS_Halted	0x0200		/* The reception of next frame is
								   halted when MACCON.1 (MHaltImm)
								   is set, or when MACRXCON.0
								   (MRxEn) is clear.						*/
#define RxS_Rx10	0x0100		/* Packet rx'd on 7 wire interface
								   -reset -> rx'd on MII					*/
#define RxS_Done	0x0080		/* The reception process by the BDMA
								   is done.									*/
#define RxS_RxPar	0x0040		/* MAC rx FIFO detected parity error		*/
#define RxS_MUFS	0x0020		/* Set when the size of the Rx frame is
							       larger than the Maximum Untagged Frame
							       Size(1518bytes) if the long packet is
							       not enabled in the MAC Rx control
							       register.								*/
#define RxS_OFlow	0x0010		/* MAC rx FIFO was full when byte rx'd		*/
#define RxS_CrcErr	0x0008		/* CRC error OR PHY asserted Rx_er			*/
#define RxS_AlignErr	0x0004	/* Frame length != mutliple of 8 bits
								   and the CRC was invalid					*/


#define lRxS_OvMax	0x04000000	/* Over Maximum Size (BRxMFS)				*/
#define lRxS_Halted	0x02000000	/* The reception of next frame is
								   halted when MACCON.1 (MHaltImm)
								   is set, or when MACRXCON.0
								   (MRxEn) is clear.						*/
#define lRxS_Rx10	0x01000000	/* Packet rx'd on 7 wire interface
								   -reset -> rx'd on MII					*/
#define lRxS_Done	0x00800000	/* The reception process by the BDMA
								   is done.									*/
#define lRxS_RxPar	0x00400000	/* MAC rx FIFO detected parity error		*/
#define lRxS_MUFS	0x00200000	/* Set when the size of the Rx frame is
								   larger than the Maximum Untagged Frame
								   Size(1518bytes) if the long packet is
								   not enabled in the MAC Rx control
								   register.								*/
#define lRxS_OFlow	0x00100000	/* MAC rx FIFO was full when byte rx'd		*/
#define lRxS_CrcErr	0x00080000	/* CRC error OR PHY asserted Rx_er			*/
#define lRxS_AlignErr	0x00040000	/* Frame length != mutliple of 8 bits
									   and the CRC was invalid				*/

/*****************************
 * Tx Descriptor Status Bits
 * - read only status bits
 *****************************/
#define TxS_Paused	0x2000		/* transmission paused due to the
								   reception of a Pause control frame		*/
#define TxS_Halted	0x1000		/* The transmission of the next frame
								   is halted when MACCON.1 (MHaltImm)
								   is set, or when MACTXCON.0 (MTxEn)
								   is clear.								*/
#define TxS_SQErr	0x0800		/* SQE error								*/
#define TxS_Defer	0x0400		/* transmission deferred					*/
#define TxS_Coll	0x0200		/* The collision is occured in
								   half-duplex. The current frame will 
								   be retried								*/
#define TxS_Comp	0x0100		/* tx complete or discarded 				*/
#define TxS_Par		0x0080		/* tx FIFO parity error 					*/
#define TxS_LateColl	0x0040	/* tx coll after 512 bit times 				*/
#define TxS_NCarr	0x0020		/* carrier sense not detected 				*/
#define TxS_MaxDefer	0x0010	/* tx defered for max_defer (24284-bit
								  times). Frame aborted						*/
#define TxS_Under	0x0008		/* tx (FIFO) underrun 						*/
#define TxS_ExColl	0x0004		/* 16 collisions occured, aborted			*/


#define lTxS_Paused	0x20000000	/* transmission paused due to the
								   reception of a Pause control frame		*/
#define lTxS_Halted	0x10000000	/* The transmission of the next frame
								   is halted when MACCON.1 (MHaltImm)
								   is set, or when MACTXCON.0 (MTxEn)
								   is clear.								*/
#define lTxS_SQErr	0x08000000	/* SQE error								*/
#define lTxS_Defer	0x04000000	/* transmission deferred					*/
#define lTxS_Coll	0x02000000	/* The collision is occured in
								   half-duplex. The current frame will 
								   be retried								*/
#define lTxS_Comp	0x01000000	/* tx complete or discarded 				*/
#define lTxS_Par	0x00800000	/* tx FIFO parity error 					*/
#define lTxS_LateColl	0x00400000	/* tx coll after 512 bit times 			*/
#define lTxS_NCarr	0x00200000	/* carrier sense not detected 				*/
#define lTxS_MaxDefer	0x00100000	/* tx defered for max_defer (24284-bit
								   times). Frame aborted					*/
#define lTxS_Under	0x00080000	/* tx (FIFO) underrun 						*/
#define lTxS_ExColl	0x00040000	/* 16 collisions occured, aborted			*/

/*****************************
 * Tx Widget Bits
 *****************************/
#define TxWidALIGN	0x00030000	/* Alignment of the buffer					*/
#define TxWidSetup	0x00020000	/* two byte offset							*/
#define TxWidSet16	0x0002		/* two byte offset							*/

/*****************************
 * Tx Descriptor Length Bits
 *****************************/
#define TxLEN		0x0000FFFF	/* Tx Length 								*/

/*****************************************************************
 * PHYS defines
 *****************************************************************/
/**********************************************
 * PHY - LX972 / L80225 register and defines
 *    -NOTE references from intel documentation
 **********************************************/

//#define	PHYAddr			0x00001		/* Address of the PHY on the SMDK2500 	*/
/******* registers *********/
#define PHYCtl		0 	// Control Register Refer to Table 37 on page 58 
#define PHYStat		1	// Status Register #1 Refer to Table 38 on page 58
#define	PHYId1		2 	// PHY Identification Register 1 Refer to Table 39
                        //	on page 59
#define PHYId2		3	// PHY Identification Register 2 Refer to 
						//	Table 40 on page 60
#define PHYANegAd	4	// Auto-Negotiation Advertisement Register
                        //	Refer to Table 41 on page 61
#define PHYANegLink	5	// Auto-Negotiation Link Partner Base Page Ability 
					    //	Register Refer to Table 42 on page 62
#ifdef PHY_INTEL
#define PHYANegEx	6	// Auto-Negotiation Expansion Register Refer to 
					    //	Table 43 on page 63
#define PHYANegNext 7	// Auto-Negotiation Next Page Transmit Register
					    //	Refer to Table 44 on page 63
#define PHYANegLPart 8 	// Auto-Negotiation Link Partner Received Next Page
					    //	Register Refer to Table 45 on page 64
#define PHY100Ctl	9 	// 1000BASE-T/100BASE-T2 Control Register Not 
					    //	Implemented
#define PHY100Stat	10	// 1000BASE-T/100BASE-T2 Status Register Not 
					    //	Implemented
#define PHYExStat	15	// Extended Status Register Not Implemented
#define	PHYPortCfg	16 	//	Port Configuration Register Refer to Table 
					    //	46 on page 64
#define PHYStat2	17	// Status Register #2 Refer to Table 47 on page 65

#define PHYIntEn	18	// Interrupt Enable Register Refer to Table 48 
						//	on page 66
#define PHYIntStat	19	// Interrupt Status Register Refer to Table 49
						//	on page 66
#define	PHYLedCfg	20	// LED Configuration Register Refer to Table 50
						//	on page 68
		// 21- 29 Reserved
#define	PHYTxCtl	30 	// Transmit Control Register Refer to Table 51
						//	on page 69
#endif
#ifdef PHY_LSI
#define PHYStat2	18	// Phy Status - Duplex and speed
#endif
/*** Only some of the defines - ones I used ...MaTed ***/
#define PHY10MSDELAY	10		// should be 10 for 10 MS delay...FIXME
                                //  but I think the mdelay routine is wrong
#define	PHYCtlReset		0x8000	// reset
#define PHYANegAdMask	0x1e0	// bits for duplex and speed (972)
#define PHYB8			0x100	// bit 8
#define PHYB7			0x080	// bit 7
#define PHYB6			0x040	// bit 6
#define PHYB5			0x020	// bit 5
#if 0	// testing leds
#define	PHYLEDCFG		0xA8F2	// LED 1 (left - blink fast
								//     2 (right - ON
#else
// NOTES: leds are wired reverse from HEI
# if 0
#define	PHYLEDCFG		0x0DF2	// LED 1 (left - link and activity
								//     2 (right - Speed)
# else
#define	PHYLEDCFG		0xD0F2	// LED 1 (right - Speed)
								//     2 (left - link and activity
# endif
#endif
#define	PHYLEDANEG		0x1e1	// Advertize AutoNeg all speed / duplex

#define PHY_BUSYLOOP_COUNT	100	// each loop is 10mS -> 1S
#define PHY_RESETLOOP_COUNT	10	// number of times to check reset 
							    //  10 ms between checks - manual
								//  states 300 uS max fro reset to work
#define	PHY_NEGLOOP_COUNT	150	// 1.5 seconds for Autoneg to complete

#define PHY_RESET		0x8000
/**** Following are from previous version of the PHY routines -- delete ***/
#ifdef PHY_INTEL
#define PHY_LINK_UP		0x0400
#define PHYMODE_100		0x4000 /* for 100 mbps								*/
#define PHYFULL_DUPLEX	0x0200

#define SPEED_AUTO		0xFFFFFF9F	/* Set the MAC speed to Automatic mode.
									   AND operation 						*/
#define SPEED_10		0xFFFFFFBF	/* Set the MAC speed to Automatic mode.
									   OR operation 						*/
#endif

#define PHY_10MBS       1
#define PHY_100MBS      2
#define PHY_AUTO_SPEED  0

// These don't really match up
#define PHY_FULL_DPX    1
#define PHY_HALF_DPX    0
#define PHY_AUTO_DPX    2

/**********************************
 * Bit Defines for PHY972_CONTROL
 **********************************/
#define PHY_CONTROL_RESET           0x8000
#define PHY_CONTROL_LOOPBACK        0x4000
#define PHY_CONTROL_SELECT_SPEED    0x2000
#define PHY_CONTROL_AUTONEG			0x1000
#define PHY_CONTROL_POWERDOWN       0x0800
#define PHY_CONTROL_ISOLATE         0x0400
#define PHY_CONTROL_RESTART_AUTONEG 0x0200
#define PHY_CONTROL_DUPLEXMODE      0x0100
#define PHY_CONTROL_COLLISION_TEST  0x0080
#define PHY_CONTROL_SPEED_10_HALF   0x0000
#define PHY_CONTROL_SPEED_10_FULL   (PHY_CONTROL_DUPLEXMODE)
#define PHY_CONTROL_SPEED_100_HALF  (PHY_CONTROL_SELECT_SPEED )
#define PHY_CONTROL_SPEED_100_FULL  (PHY_CONTROL_SELECT_SPEED \
			                            | PHY_CONTROL_DUPLEXMODE)
#define PHY_CONTROL_AUTONEG_HALF_10	(0x1000 | PHY_CONTROL_RESTART_AUTONEG)
#define PHY_CONTROL_AUTONEG_FULL_10	(0X1100 | PHY_CONTROL_RESTART_AUTONEG)
#define PHY_CONTROL_AUTONEG_HALF_100	(0x3000 | PHY_CONTROL_RESTART_AUTONEG)
#define PHY_CONTROL_AUTONEG_FULL_100	(0X3100 | PHY_CONTROL_RESTART_AUTONEG)
 
/*****************************************
 * Bit Defines for PHY972_STATUS_REG_ONE
 *****************************************/
    /* not used                            0x8000  */
#ifdef	PHY_INTEL
#define PHY_STATUS_REG_TWO_MODE_100             0x4000
#define PHY_STATUS_REG_TWO_TX_STATUS            0x2000
#define PHY_STATUS_REG_TWO_RX_STATUS            0x1000
#define PHY_STATUS_REG_TWO_COLLISION_STATUS     0x0800
#define PHY_STATUS_REG_TWO_LINK                 0x0400
#define PHY_STATUS_REG_TWO_DUPLEX_MODE          0x0200
#define PHY_STATUS_REG_TWO_AUTO_NEGOTIATION     0x0100
#define PHY_STATUS_REG_TWO_AUTO_NEGO_COMPELETE  0x0080
    /* not used                                 0x0040  */
#define PHY_STATUS_REG_TWO_POLARITY             0x0020
#define PHY_STATUS_REG_TWO_PAUSE                0x0010
#define PHY_STATUS_REG_TWO_ERROR                0x0008
    /* not used                                 0x0004  */
    /* not used                                 0x0002  */
    /* not used                                 0x0001  */
#endif
#ifdef	PHY_LSI
#define PHY_STATUS_REG_LINK                 	0x0004
#define PHY_STATUS_REG_TWO_MODE_100             0x0080
#define PHY_STATUS_REG_TWO_DUPLEX_MODE          0x0040
#endif

/*******************************************
 * redefinition for merged status 
 *******************************************/
#ifdef PHY_LSI
#define PHY_STATUS_LINK_UP	PHY_STATUS_REG_LINK		// mask for determinig if
                                                    //  link up
#define PHY_STATUS_LINK_100	PHY_STATUS_REG_TWO_MODE_100	// mask for 100 Mbs
#define PHY_STATUS_LINK_DUPLEX	PHY_STATUS_REG_TWO_DUPLEX_MODE	// mask for
                                                                // full duplex
#endif
#ifdef PHY_INTEL
#define PHY_STATUS_LINK_UP	PHY_LINK_UP		// mask for determining if
                                            //  link up
#define PHY_STATUS_LINK_100	PHYMODE_100		// mask for 100 Mbs
#define PHY_STATUS_LINK_DUPLEX	PHYFULL_DUPLEX	// mask for full duplex
#endif

/*********************************************************************
 +---------------+---------------+---------------+---------------+
  Byte 3          Byte 2          Byte 1          Byte 0
 +---------------+---------------+---------------+---------------+
                        RX FRAME DESCRIPTOR
 +---------------------------------------------------------------+
 |                          Buffer Pointer	    		         |0x00
 +---------+---------------------+-------------------------------+
 |  Flags  |      Status         |                               |
 |O B S E D|M H 1 D P U O C A - -|	          Frame Length       |0x04
 +-+-+-+-+-+---------------------+-------------------------------+
  3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
      FLAGS
 	O - Ownership 1->BDMA, 0->CPU
   B - Skip BD 
   S - SOF - First BD for a frame
   S - EOF - Last BD for a frame
   D - Done (set on first BD when frame finished)
      Status
   M - MSO - bigger than max size
   H - Halted
   1 - Rx'd over 10 Mbps interface
   D - BRxDone
   U - Parity Error
   O - Overflow in FIFO
   C - CRC error
   A - Alignment error

                        TX FRAME DESCRIPTOR
 +---------------------------------------------------------------+
 |                          Buffer Pointer	    		         |0x00
 +---------------------------+---+-------------------------------+
 |            Status         |   |                               |
 |O - - H Q D C F P L N E U X|Wid|	          Tx Length          |0x04
 +-+-+-+-+-+---------------------+-------------------------------+
  3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
      Status
 	O - Ownership 1->BDMA, 0->CPU
   H - Halted
   Q - SQEErr - Signal Quality Error
   D - Defer - Transmission of the frame was deferred
   C - Coll - Collision occurred in half-duplex - will be retried
   F - Finished - The transmission has Completed
   P - ParErr - Parity error in FIFO
   L - Late Coll - The collision occurred after 64 byte times
   N - NoCarr - No Carrier Sense was detected during MAC Tx
   E - DeferErr - Frame aborted - deferred 24284-bit times
   U - Underflow - FIFO underflow
   X - ExColl - Frame aborted - 16 consecutive collisions
      Widget
   Wid - Transmission alignment - number of bytes

*******************************************************************/

/***********************************************
 * RX Frame Descriptors for Ethernet MAC of S3c2500
 * - this structure needs to be accessed as
 * uncached memory.
 ***********************************************/
typedef struct _RxFD
{
  volatile	UCHAR *		FDDataPtr;
  volatile	UINT16      FDLength;
  volatile	UINT16		FDStat;		// Ownership, Flags and Status
} RxFD;

/************************************************
 * RX Frame Control
 * - parallels RxFD structure to keep data in it.
 *   I.E. same index as RxFD
 * - just keeps the SKB
 * - total structure must be word multiple
 * *FIXME: get rid of of the structure definition
 * *FIXME:  it just adds another layer ...MaTed
 ************************************************/
typedef struct _RxFrCtl
{
  struct sk_buff * FDskb;		/* the skb handles are preallocated uncached
								   skb's. Always keep an entry here 		*/
}RxFrCtl;
/***********************************************
 * TX Frame Descriptors for Ethernet MAC of S3c2500
 * - this structure needs to be accessed as
 * uncached memory.
 ***********************************************/
typedef struct _TxFD
{
  volatile	UCHAR	*FDDataPtr;	// Pointer to data to be tx'd
  volatile	UINT16	FDLength;	// Length of the Data
  volatile	UINT16	FDStat;		// Ownership, Status and Widget
} TxFD;

/************************************************
 * TX Frame Buffer
 * - parallels TxFD structure  to keep data in it.
 *   I.E. same index for both
 * - these buffers need to be in uncached memory
 * - total structure must be word multiple
 ************************************************/
typedef struct _TxBUFF
{
#if CONFIG_WBUFFER
  UCHAR	FDData[ MAX_PKT_SIZE];	/* must be word multiple the first 2 bytes
								   are filler which gives the upper protocol
								   layers aligned access to the header. The 
								   BDMA engine needs the Data buffer aligned
								   on a word boundary (so we just tell the
								   BDMA to skip the first 2 bytes */
#else
  struct sk_buff * FDskb;		/* the skb handles are saved so we can
								   free them after the controller has
								   finished sending the skb data 		*/
#endif // CONFIG_WBUFFER
} TxBUFF;

/**********************
 * Control structures
 **********************/
/********************************
 * Simple Ring Buffer Struct
 * - Should check if UINT16 better than UINT32 in code generation ...MaTed---
 ********************************/
typedef struct _RINGBUFF
{
  UINT16	first;	// indexfirst in the linked list (first empty)
  UINT16	last;	// last in the linked list (last used)
  UINT16	num;	// number of elements in the linked list
                    //  mostly for stats, full and empty detection
  UINT16	max;	// Maximum index of elements in the ring
} RINGBUFF;
  
/**********************************************************
 * PRIV structure
 * Information that need to be kept for each MAC / board
 *  Use net_device.priv to keep pointer to this struct
 **********************************************************/
typedef struct
{  /* MAC specific information */
   /*
   * configuration parameters,
   * - user can override later in /proc?? (still have to do this)
   */
  UINT8		phySpeed;		// 00-Auto, 0x1-10 Mbs, 0x2-100Mbs, 0x3-Auto
  UINT8		phyDpx;			// duplex 00-Half, 0x1-Full (not valid with??)
  UINT16	tcbTxThresh;	// Transmit Threshold for fifo dma xfer

  /*
   * Keep track of Negotiated speeds
   */
  UINT8		phySpeedNeg;	// 0x1-10 Mbs, 0x2-100Mbs
  UINT8		phyDpxNeg;		// duplex 00-Half, 0x1-Full
  /*
   * Keep track of which controller this structure applies to
   * - for the 2500 it is either 0 or 1
   */
  UINT32	channel;

  /* Tx control lock.  This protects the transmit buffer ring
   * state along with the "tx full" state of the driver.  This
   * means all netif_queue flow control actions are protected
   * by this lock as well.
   */
  spinlock_t	txLock;

  /* Rx control lock.  This protects the receive buffer ring
   * state of the driver.
   * NOTE: we might not need this - check ...MaTed---
   */
  spinlock_t	rxLock;

  /*
   * Need a storage place to save which SKB destructor routine to use
   */
  void (*destructor)(struct sk_buff *);   /* Destruct function */
/************************************************************************
 * For Receive and Transmit, S3c2500 Buffer Control Structure.		*
 * Each Structre points to it own buffer space and contains interface   *
 * related information. The structre contains the link list for the     *
 * buffer.                                                              *
 ************************************************************************/
  /*
   * Queue control into data structures to S3C2500 MAC
   */
  RINGBUFF	ringRx;	// Receive Frame Descriptor ring buffer pointers
  RINGBUFF	ringTx;	// Transmit Frame Descriptor ring buffer pointers

  /*
   * Pointers to Data structures to communicate with S3C2500 MAC
   * NOTE: these structures must be accessed uncached
   */
  RxFD	*	rxFd;		// Pointer to receive frame descriptors
  TxFD	*	txFd;		// Pointer to transmit frame descriptors

  /*
   * Arrays of skb pointers which keep Data associated with S3c2500 frame
   *	descriptors.
   */
  RxFrCtl	rxSkb[ RX_NUM_FD];	// Pointer to array of receive buffers
#if !CONFIG_WBUFFER		// Skb's if no WBUFF, Uncached Buffers if WBUFF
  TxBUFF 	txSkb[ TX_NUM_FD];	// Pointer to array of transmit skb pointer
#endif // !CONFIG_WBUFFER
  /*
   * Statistics 
   */
  struct net_device_stats	stats;		// standard statistics
  struct
  {
	UINT32	BRxNl;	// BDMARxPtr has null list
	UINT32	BRxNo;	// BDMA rx is not the owner of the current data frame
					//	pointed to. Rx is stopped
	UINT32	BTxCp;	// BDMA tx indicates MAC sent a control packet
	UINT32	BTxNl;	// BDMATxPtr has a null list
	UINT32	BTxNo;	// BDMA tx is not the owner of the current data frame
					//	pointed to. Tx is stopped
  } bdmaStats;
} S3C2500_MAC;

/********************************************************************
 * Prototypes for external routines
 ********************************************************************/
extern void enable_samsung_irq( int int_num);
extern void disable_samsung_irq( int int_num);
//extern inline void invalidate_dcache_line( unsigned long addr);
extern unsigned char * get_MAC_address( char * name);
//extern int get_ethernet_addr( char * ethaddr_name, char *ethernet_addr);
//extern int s3c_flush_cache_region( void * start_addr, unsigned int size);
//extern int s3c_invalidate_cache( void * start_address, unsigned int length);

extern int ask_s3c2500_uncache_region( void * start_addr, unsigned int size,
									  unsigned int flag);
