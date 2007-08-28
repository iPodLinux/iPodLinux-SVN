/***********************************************************************/
/*                                                                     */
/* Samsung s3c4530a Ethernet Driver Header                             */
/* Copyright 2001 Arcturus Networks Inc and Lineo Canada Corp.         */
/*                                                                     */
/***********************************************************************/
/* s3c4530.h */
/*
 * modification history
 * --------------------
 *
 *	10/22/01	MaTed
 *			- Initial version for linux 
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
//#include <asm/bootst.h>		// only for bgetenv ...MaTed---
#include <asm/arch/irq.h>
#include <asm/irq.h>

#if 0	// do we need this ...MaTed---
#include <asm/addrspace.h>
#endif

/***************************************
 * We want ours - not from hardware.h
 ***************************************/
#undef BDMATXCON   0x9000		/* Buffered DMA Transmit Control			*/
#undef BDMARXCON   0x9004		/* Buffered DMA Receive Control				*/
#undef BDMATXPTR   0x9008		/* BDMA Transmit Frame Descriptor Start
								   Address									*/
#undef BDMARXPTR   0x900C		/* BDMA Receive Frame Descriptor Start
								   Address									*/
#undef BDMARXLSZ   0x9010		/* BDMA Receive frame maximum Size			*/
#undef BDMASTAT    0x9014		/* BDMA Status								*/
// I don't know what the next one is yet ...MaTed---
#undef ETXSTAT     0x9040		/* Transmit control frame status 			*/
#undef CAM     	0x9100		/* CAM content 32 words 9100-917c			*/
#undef BDMATXBUF   0x9200		/* Tx Buffer - test mode only				*/
#undef BDMARXBUF	0x9800		/* Rx Buffer - test mode only				*/
#undef BDMARXBUF1	0x9900		/* Rx Buffer - test mode only				*/
/***********************************************
 * Ethernet MAC Register Address
 ***********************************************/
#undef MACON       0xA000		/* MAC control								*/
#undef CAMCON      0xA004		/* CAM control								*/
#undef MATXCON     0xA008		/* Transmit control							*/
#undef MATXSTAT    0xA00C		/* Transmit status							*/
#undef MARXCON     0xA010		/* Receive control							*/
#undef MARXSTAT    0xA014		/* Receive status							*/
#undef STADATA     0xA018		/* Station management data					*/
#undef STACON      0xA01C		/* Station management control and address	*/
#undef CAMEN       0xA028		/* CAM enable								*/
#undef EMISSCNT    0xA03C		/* Missed error count						*/
#undef EPZCNT      0xA040		/* Pause count								*/
#undef ERMPZCNT    0xA044		/* Remote pause count						*/

#undef HW_MAX_ADDRS	HW_MAX_CAM - 5		// maximum usable ARC entries
#undef BRxEmpty	0x010000	/* Rx buffer empty interrupt enable			*/
#undef BRxEarly	0x020000	/* Rx early notify interrupt enable			*/
#undef	BRxRDF		0x000001	/* Rx Done frame							*/
#undef	BRxNL		0x000002	/* Rx Null List								*/
#undef	BRxNO		0x000004	/* Rx Not Owner								*/
#undef	BRxMSO		0x000008	/* Rx Maximum Size Over						*/
#undef	BRxFRF		0x000080	/* One more frame in the BDMA receive Buffer*/
#undef	BRxNFR		0x00ff00	/* Total number of data frames currently
								   in the BDMA receive buffer				*/
#undef	BTxCCP		0x010000	/* Tx completed sending control packet		*/
#undef	BTxNL		0x020000	/* Tx Null List								*/
#undef	BTxNO		0x040000	/* Tx Not Owner								*/
#undef	BTxEmpty	0x100000	/* Tx Buffer Empty							*/


/****************
 * DEBUG MACROS
 ***************/
/* use 0 for production, 1 for verification, >2 for debug */
#ifndef NET_DEBUG
#define NET_DEBUG 1
#endif

#if NET_DEBUG > 0
// NOTE: NO panic used when in a driver - I might be in the kernel
#define ASSERT(expr) \
  if(!(expr)) { \
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

/****************
 * CONFIGURATION
 ****************/
// Use skb's, but dma straight into them -- exclusive to below ...MaTed---
#define ZERO_COPY_SKB	1

// Have to make sure that destructor is always called and
//  no one ever frees the skb data before I trust this
//  would have a very hard bug to trace or replicate ...MaTed---
#undef ZERO_COPY_BUFFER

// IOCTL setup not fully implemented, just some routines
#undef ENABLE_IOCTL

/***********************************
 *
 * S3c Memory configuration defines
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
 * S3c Ethernet MAC Configuration
 *
 ***********************************/
#define HW_MAX_CAM		((32 * 4) / NUM_HW_ADDR)	/* number of entries 
													   in CAM for address	*/
#define HW_MAX_ADDRS	HW_MAX_CAM - 5		// maximum usable ARC entries
											//  0, 1 and 18 reserved
											// we ignore 19 and 20
#define ETH_MAXUNITS	1

#define TX_THRESH_HOLD	512		// value for tuning tx threshold (minimum value
								// that works is best)
								//  - passed to sierra controller 
#define	TX_POLL_CTR		0x80	// Count between polls, 0 to disable
#define ETH_MIN_LEN		64		// minimum length to xmit is 64
								//  Verify OK for all protocols
/* Tunable defines */
#define RX_NUM_FD		256		// Max number of receive frames to set up
								// (HARD MAX)
#if 1
#define TX_NUM_FD		64		// Maximum number of transmit frames to set up
#else
#define TX_NUM_FD		32		// Maximum number of transmit frames to set up
								// (SOFT MAX) -- NOTE: do not make too big or 
#endif
#define RX_NUM_SKB		R`X_NUM_FD	// Total number of receive
									// buffers allocated
#define RX_NOTE_BUF		RX_NUM_FD / 2	// Notify on half full

#define TX_NUM_SKB		TX_NUM_FD	// Total number of receive
									// buffers allocated
#define TX_NOTE_BUF		TX_NUM_FD / 2	// Notify on half frames unused

/* The number of low I/O ports used by the ethercard.????...MaTed---????? */
#define MY_TX_TIMEOUT  ((400*HZ)/1000)

/*********************
 * Error Return Codes
 *********************/
typedef int					RET_CODE;

#define	S3C_SUCCESS		0	// function call was successful
#define S3C_FAIL		-1	// general error code
#define S3C_TIMEOUT		-1000	// timeout error

/**********************
 * S3C Buffer Sizes
 **********************/
#define MAX_PKT_SIZE		1536		// bigger than need be ? (but 3x512)
//#define MAX_FRAME_SIZE	1522
// Realize that this is bigger than needs to be - does this cause a problem?
//  -- the upper layers break down tx stuff, rx?? ...MaTed---
#define PKT_BUF_SZ  		MAX_PKT_SIZE	// Size of each Rx buffer

/* Reserve all the stuff - nothing between used ...MaTed--- */
#define MAC_IO_EXTENT	(0xAFFF - 0x9000)

/****************
 * EXTERNS
 ****************/


/*********************************************************************
 * typedefs
 *********************************************************************/
typedef unsigned int	UINT;
typedef	unsigned long	ULONG;
typedef unsigned long	UINT32;
typedef unsigned short	UINT16;
typedef unsigned char	UINT8;
typedef	unsigned char	UCHAR;

/*****************
 *  Ethernet MAC
 *****************/
#define SRB_BASE        0x03FF0000
#define	BDMA_BASE		SRB_BASE+0x9000	/* Base address to BDMA Controller	*/
#define	MAC_BASE		SRB_BASE+0xa000	/* Base address to MAC Controller	*/
#define	CACHE_TAG_RAM	0x11000000		/* base address to Tag RAM			*/

#define	UNCACHE_ADDR	0x04000000		/* forces to uncached access		*/
#define UnCache( x) (UNCACHE_ADDR | (UINT32) (x)) 

// Each Mac has four interrupts
#define MAC_RX_IRQ		19			/* RX IRQ for MAC						*/
#define MAC_TX_IRQ		18			/* TX IRQ for MAC						*/
#define BDMA_RX_IRQ		17			/* RX IRQ for BDMA						*/
#define BDMA_TX_IRQ		16			/* TX IRQ for BDMA						*/

/***********************************************
 * Misc Addresses
 ***********************************************/
//#define	SYSCFG		0x0000	/* System configuration register			*/

#define	WriteBufferEn	0x04	/* Write Buffer enable bit mask				*/

/****************************************
 * Interrupt Controller
 ****************************************/
#define INTMOD      (SRB_BASE+0x4000)
#define INTPND      (SRB_BASE+0x4004)
#define INTMSK      (SRB_BASE+0x4008)
#define INTPRI0     (SRB_BASE+0x400C)
#define INTPRI1     (SRB_BASE+0x4010)
#define INTPRI2     (SRB_BASE+0x4014)
#define INTPRI3     (SRB_BASE+0x4018)
#define INTPRI4     (SRB_BASE+0x401C)
#define INTPRI5     (SRB_BASE+0x4020)
#define INTOFFSET   (SRB_BASE+0x4024)
#define INTOSET_FIQ (SRB_BASE+0x4030)
#define INTOSET_IRQ (SRB_BASE+0x4034)

#define _IRQ0       0
#define _IRQ1       1
#define _IRQ2       2
#define _IRQ3       3
#define _URTTx0     4
#define _URTRx0     5
#define _URTTx1     6
#define _URTRx1     7
#define _GDMA0      8
#define _GDMA1      9
#define _TC0        10
#define _TC1        11
#define _HDLCATx    12
#define _HDLCARx    13
#define _HDLCBTx    14
#define _HDLCBRx    15
#define _BDMATx     16
#define _BDMARx     17
#define _MACTx      18
#define _MACRx      19
#define _I2C        20
#define _GLOBAL     21

/***********************************************
 * Ethernet BDMA Register Address
 ***********************************************/
#define BDMATXCON   0x9000		/* Buffered DMA Transmit Control			*/
#define BDMARXCON   0x9004		/* Buffered DMA Receive Control				*/
#define BDMATXPTR   0x9008		/* BDMA Transmit Frame Descriptor Start
								   Address									*/
#define BDMARXPTR   0x900C		/* BDMA Receive Frame Descriptor Start
								   Address									*/
#define BDMARXLSZ   0x9010		/* BDMA Receive frame maximum Size			*/
#define BDMASTAT    0x9014		/* BDMA Status								*/
// I don't know what the next one is yet ...MaTed---
#define ETXSTAT     0x9040		/* Transmit control frame status 			*/
#define CAM     	0x9100		/* CAM content 32 words 9100-917c			*/
#define BDMATXBUF   0x9200		/* Tx Buffer - test mode only				*/
#define BDMARXBUF	0x9800		/* Rx Buffer - test mode only				*/
#define BDMARXBUF1	0x9900		/* Rx Buffer - test mode only				*/
/***********************************************
 * Ethernet MAC Register Address
 ***********************************************/
#define MACON       0xA000		/* MAC control								*/
#define CAMCON      0xA004		/* CAM control								*/
#define MATXCON     0xA008		/* Transmit control							*/
#define MATXSTAT    0xA00C		/* Transmit status							*/
#define MARXCON     0xA010		/* Receive control							*/
#define MARXSTAT    0xA014		/* Receive status							*/
#define STADATA     0xA018		/* Station management data					*/
#define STACON      0xA01C		/* Station management control and address	*/
#define CAMEN       0xA028		/* CAM enable								*/
#define EMISSCNT    0xA03C		/* Missed error count						*/
#define EPZCNT      0xA040		/* Pause count								*/
#define ERMPZCNT    0xA044		/* Remote pause count						*/

/***********************************************
 * Ethernet BDMA Register Offsets (from 0x9000)
 ***********************************************/
#define BOTXCON   	0x0000		/* Buffered DMA Transmit Control			*/
#define BORXCON   	0x0004		/* Buffered DMA Receive Control				*/
#define BOTXPTR   	0x0008		/* BDMA Transmit Frame Descriptor Start
								   Address									*/
#define BORXPTR   	0x000C		/* BDMA Receive Frame Descriptor Start
								   Address									*/
#define BORXLSZ   	0x0010		/* BDMA Receive frame maximum Size			*/
#define BOSTAT    	0x0014		/* BDMA Status								*/
// I don't know what the next one is yet ...MaTed---
#define BOETXSTAT	0x0040		/* Transmit control frame status 			*/
#define BOCAM     	0x0100		/* CAM content 32 words 9100-917c			*/
#define BOTXBUF		0x0200		/* Tx Buffer - test mode only				*/
#define BORXBUF1	0x0800		/* Rx Buffer - test mode only				*/
#define BORXBUF2	0x0900		/* Rx Buffer - test mode only				*/
/***********************************************
 * Ethernet MAC Register Offsets (from (0x9000)
 ***********************************************/
#define MOCON       0x1000		/* MAC control								*/
#define MOCAMCON    0x1004		/* CAM control								*/
#define MOTXCON     0x1008		/* Transmit control							*/
#define MOTXSTAT    0x100C		/* Transmit status							*/
#define MORXCON     0x1010		/* Receive control							*/
#define MORXSTAT    0x1014		/* Receive status							*/
#define MOSTADATA	0x1018		/* Station management data					*/
#define MOSTACON    0x101C		/* Station management control and address	*/
#define MOCAMEN     0x1028		/* CAM enable								*/
#define MOEMISSCNT  0x103C		/* Missed error count						*/
#define MOEPZCNT	0x1040		/* Pause count								*/
#define MOERMPZCNT	0x1044		/* Remote pause count						*/
/*********************************************************
 * Buffered DMA Transmit Control Register (7-23 -> 7-25)
 * BDMATXCON - offset 0x9000
 *********************************************************/
#define	BTxBRST_MASK	0x001f	/* Mask for DMA Burst Size (pg 7-23)		*/
#define BTxSTSKO		0x0020	/* Tx stop/skip by owner bit				*/

#define BTxCCPIE		0x0080	/* Tx complete to send control packet
								   interrupt enable							*/
#define BTxNLIE			0x0100	/* Tx Null List interrupt enable			*/
#define BTxNOIE			0x0200	/* Tx not owner interrupt enable			*/
#define BTxEMPTY		0x0400	/* Tx buffer empty interrupt bit			*/
#define BTxMSL			0x3800	/* transmit to Mac Tx Start Level			*/
#define BTxEN			0x4000	/* Tx Enable								*/
#define BTxRS			0x8000	/* Tx reset - just the BDMA tx Block		*/
/* Interrrupt enable bits */
//	#define BTxINTMask		(BTxSTSKO | BTxCCPIE | BTxNLIE | BTxNOIE 
//								| BTxEMPTY)
//#define BTxINTMask		(BTxSTSKO | BTxCCPIE | BTxNLIE )
#define BTxINTMask		(BTxSTSKO | BTxCCPIE )
/*  TxM Start Level                     bits 11 to 13   */
#define BTxFill		(6<<11)  	/* Fill n / 8th's of BDMA tx before MAC		*/
/* setup size for rx dma burst size FIXME...MaTed--- */
#define BTxBurstSIZE	(16)	/* 16 words == 64 bytes		  				*/

//#define BTxSetup	(BTxBurstSIZE | BTxSTSKO | BTxCCPIE | BTxNLIE | BTxNOIE 
//					 | BTxEMPTY | BTxFill | BTxEN )
#define BTxSetup	(BTxBurstSIZE | BTxSTSKO | BTxFill )
//#define BTxSETUP	(BtxBurstSIZE | BtxFill)	// 16 word dma burst, fill 2/8
												//  of MAC Start Level
/*********************************************************
 * Buffered DMA Receive Control Register (7-26 -> 7-29)
 * BDMARXCON - offset 0x9004
 *********************************************************/
#define BRxBRST		0x00001f	/* Mask for DMA Burst size					*/
#define BRxSTSKO	0x000020	/* Stop/Skip frame by owner bit				*/
#define BRxMAINC	0x000040	/* RX memory address inc / dec				*/
#define BRxDIE		0x000080	/* every received frame interrupt enable	*/
#define BRxNLIE		0x000100	/* Null List interrupt enable				*/
#define BRxNOIE		0x000200	/* Not owner interrupt enable				*/
#define BRxMSOIE	0x000400	/* Maximum size over interrupt enable		*/
#define BRxLittle	0x000800	/* Little / Big Endian (SET 1)				*/
//***** NOTE WA must be set ONCE after reset - forced in Reset()
#define BRxWA		0x003000	/* Rx word alignment "10" -> 2				*/
#define BRxEN		0x004000	/* Rx Enable								*/
#define BRxRS		0x008000	/* Rx reset									*/
#define BRxEmpty	0x010000	/* Rx buffer empty interrupt enable			*/
#define BRxEarly	0x020000	/* Rx early notify interrupt enable			*/

#define BRxINTMask	(BRxSTSKO | BRxDIE | BRxNLIE | BRxNOIE | BRxMSOIE \
					 | BRxEmpty | BRxEarly)

/*  RxAlign                     bits 12 to 13   */
#define BRxAlignSkip0	(0x00<<12)  /* Do not skip any bytes on rx			*/
#define BRxAlignSkip1	(0x01<<12)  /* Skip 1st byte on rx					*/
#define BRxAlignSkip2	(0x02<<12)  /* Skip 2nd byte on rx					*/
#define BRxAlignSkip3	(0x03<<12)  /* Skip 3rd byte on rx					*/
/* setup size for rx dma burst size FIXME...MaTed--- */
//#define BRxBurstSize	(16)	/* 16 words == 64 bytes		  				*/
#define BRxBurstSize	(0x1f)	/* 32 words == 128bytes	(max value)			*/

// Not sure of MAINC, Empty, En (sooner or later), some other ENables
// ...MaTed---
#define	BRxSetup_RESET	(BRxBurstSize | BRxSTSKO | BRxMAINC | \
						 BRxLittle | BRxAlignSkip2 )
//#define BRxSetup	(BRxBurstSize | BRxSTSKO | BRxMAINC | BRxDIE |
//					 BRxNLIE | BRxNOIE | BRxMSOIE | BRxLittle |
//					 BRxAlignSkip2 | BRxEN | BRxEmpty )
#define BRxSetup	(BRxBurstSize | BRxSTSKO | BRxMAINC | BRxDIE | \
					 BRxNLIE | BRxNOIE | BRxLittle | \
					 BRxAlignSkip2 | BRxEN )

/*********************************************************
 * Buffered DMA Status Register (7-32 -> 7-34)
 * BDMASTAT - offset 0x9014
 *********************************************************/
#define	BRxRDF		0x000001	/* Rx Done frame							*/
#define	BRxNL		0x000002	/* Rx Null List								*/
#define	BRxNO		0x000004	/* Rx Not Owner								*/
#define	BRxMSO		0x000008	/* Rx Maximum Size Over						*/
#define	BRxStEmpty	0x000010	/* Rx buffer Empty							*/
#define	BRxSEarly	0x000020	/* Early Notification						*/
#define	BRxFRF		0x000080	/* One more frame in the BDMA receive Buffer*/
#define	BRxNFR		0x00ff00	/* Total number of data frames currently
								   in the BDMA receive buffer				*/
#define	BTxCCP		0x010000	/* Tx completed sending control packet		*/
#define	BTxNL		0x020000	/* Tx Null List								*/
#define	BTxNO		0x040000	/* Tx Not Owner								*/
#define	BTxEmpty	0x100000	/* Tx Buffer Empty							*/
/*********************************************************
 * MAC Control Register (7-36)
 * MACON - offset 0xA000
 *********************************************************/
#define	MCtlHaltReq		0x0001	/* Halt request								*/
#define	MCtlHaltImm		0x0002	/* Halt immediate							*/
#define	MCtlReset		0x0004	/* Software reset							*/
#define	MCtlFullDup		0x0008	/* Full Duplex								*/
#define	MCtlLoop		0x0010	/* Set for loopback testing					*/
#define	MCtlMII_Off		0x0040	/* set to connect to 10 MbS endec, 0 for
								   MII										*/
#define	MCtlLoop10		0x0080	/* Bit asserted to the 10MbS endec (set 0)	*/
#define	MCtlMissRoll	0x0400	/* Bit set when the missed error counter 
								   rolls over								*/
#define	MCtlMDC_OFF		0x1000	/* Clear this bit to enable MDC clock 
								   generation for power management			*/
#define	MCtlEnMissRoll	0x2000	/* Enable missed error counter rollover
								   interrupt								*/
#define	MCtlLink10		0x8000	/* Link Status 10 Mb/S to Link10 pin		*/
// Which IRQ does this interrupt go to ...MaTed-- not implemented FIXME
#define	MCtlINTMask		MCtlEnMissRoll
#define MCtlReset_BUSY_LOOP		100
#define MCtlSETUP	(MCtlFullDup )

#define MCtlFullDupShift	3
#define MCtl10Shift1		6
#define MCtl10Shift2		7
/*********************************************************
 * MAC CAM Control Register (7-37)
 * CAMCON - offset 0xA004
 *********************************************************/
#define StationAcc	0x00000001	/* Accept Station/Unicast addresses			*/
#define GroupAcc	0x00000002	/* Accept Group/Multicast addresses			*/
#define BroadAcc	0x00000004	/* Accept the Broadcast addresses			*/
#define NegCAM		0x00000008	/* Reject, rather than accept
								   CAM-recognized entries					*/
#define CompEn		0x00000010	/* Enable compare mode						*/

#if 0	// definition above calculates the value ...MaTed--- 
#define HW_MAX_ADDRS	21		/* CAM consists of 32 words					*/
#endif
#define CAMCtlSETUP	(StationAcc | GroupAcc | BroadAcc | CompEn)

/*********************************************************
 * MAC Transmit Control Register (7-38)
 * MACTXCON - offset 0xA008
 *********************************************************/
#define	MTxCtlTxEn			0x0001	/* Transmit Enable, clear stop
									   immediate   							*/
#define	MTxCtlTxHalt		0x0002	/* Transmit Halt, set stops after current
									   Packet								*/
#define	MTxCtlNoPad			0x0004	/* Suppress Padding for pkts <64 bytes	*/
#define	MTxCtlNoCRC			0x0008	/* Suppress CRC							*/
#define	MTxCtlFBack			0x0010	/* Fast Back-off - testing only			*/
#define	MTxCtlNoDef			0x0020	/* No defer - disable the counter counting
									   until the carrier sense bit is turned
									   off 									*/
#define	MTxCtlSdPause		0x0040	/* Send Pause (or other MAC control)	*/
#define	MTxCtlSQEn			0x0080	/* SQE test mode - set to enable MII bit
																		   	*/
#define	MTxCtlEnUnder		0x0100	/* Enable tx FIFO underun interrupt		*/
#define	MTxCtlEnDefer		0x0200	/* Enable max deferral time interrupt	*/
#define	MTxCtlEnNCarr		0x0400	/* Enable no carrier while tx interrupt	*/
#define	MTxCtlEnExColl		0x0800	/* Enable >16 collisions during tx
									   interrupt							*/
#define	MTxCtlEnLateColl	0x1000	/* Enable Late Collision interrupt		*/
#define	MTxCtlEnTxParity	0x2000	/* Enable MAC tx FIFO parity interrupt 	*/
#define	MTxCtlEnComp		0x4000	/* Enable completion (tx or discard)
									   interrupt							*/
#define MTxCtlINTMask	(MTxCtlEnUnder | MTxCtlEnDefer | MTxCtlEnNCarr | \
						 MTxCtlEnExColl | MTxCtlEnLateColl |		\
						 MTxCtlEnTxParity)
// #define MTxCtlSetup	(MTxCtlTxEn | MTxCtlSQEn |  MTxCtlEnUnder |  
#define MTxCtlSetup	(MTxCtlEnUnder | 	\
					 MTxCtlEnDefer | MTxCtlEnNCarr | 				\
					 MTxCtlEnExColl | MTxCtlEnLateColl | 			\
					 MTxCtlEnTxParity)

/*********************************************************
 * MAC Transmit Status Register (7-39)
 * MACTXSTAT - offset 0xA00C
 *********************************************************/
#define MTxStTxColl		0x000f	/* Transmit Collision Count					*/
#define MTxStExColl		0x0010	/* Excessive Collision - tx aborted			*/
#define MTxStTxDefer	0x0020	/* Transmit deferred - pkt tx was deferred	*/
#define MTxStPause		0x0040	/* tx delayed due to rx of PAUSE			*/
#define MTxStIntTx		0x0080	/* Transmit caused interrupt				*/
#define MTxStUnder		0x0100	/* Underrun - tx FIFO went empty during tx	*/
#define MTxStDefer		0x0200	/* Set if MAC defers tx because of
								   MAX_DEFERAL								*/
#define MTxStNCarr		0x0400	/* Set if no carrier sense during
								   transmission of packet					*/
#define MTxStSQE		0x0800	/* Signal Quality Error - see description	*/
#define MTxStLateColl	0x1000	/* Late Collision - collision after 64 
								   byte times								*/
#define MTxStTxPar		0x2000	/* Tx FIFO parity error (manual typo)		*/
#define MTxStComp		0x4000	/* Completion - MAC tx or discard 1 pkt		*/
#define MTxStTxHalted	0x8000	/* tx halted by TxEn of HaltImm bits		*/
/*********************************************************
 * MAC Receive Control Register (7-40)
 * MACRXCON - offset 0xA010
 *********************************************************/
#define	MRxCtlRxEn		0x0001	/* Rx Enable								*/
#define	MRxCtlRxHalt	0x0002	/* Rx Halt Request (after rx of current
								   pkt)										*/
#define	MRxCtlLongEn	0x0004	/* Long Enable - receive packets >1518 Bytes*/
#define	MRxCtlShortEn	0x0008	/* Allow rx of pkt < 64 bytes				*/
#define	MRxCtlStripCRC	0x0010	/* Checks CRC and then strips from msg		*/
#define	MRxCtlPassCtl	0x0020	/* Enable passing of control pkts to MAC
								   client									*/
#define	MRxCtlIgnoreCRC	0x0040	/* Disable CRC value checking				*/
#define	MRxCtlEnAlign	0x0100	/* Enable alignment interrupt - length is
								   not a multile of 8 bits and CRC is
								   invalid									*/
#define	MRxCtlEnCRCErr	0x0200	/* Enable CRC error interrupt - invalid
								   CRC or PHY asserts Rx_er					*/
#define	MRxCtlEnOver	0x0400	/* Enable overflow interrupt - rx when MAC
								   rx FIFO is full							*/
#define	MRxCtlEnLongErr	0x0800	/* Enable interrupt on long pkts - see
								   description								*/
#define	MRxCtlEnRxPar	0x2000	/* Enable a rx parity interrupt if MAC rx
								   FIFO  detects parity error				*/
#define	MRxCtlEnGood	0x4000	/* Enable good (error-free rx of a complete 
								   data packet) interrupt					*/
#define MRxCtlINTMask	(MRxCtlEnAlign | MRxCtlEnCRCErr | MRxCtlEnOver |	\
						 MRxCtlEnLongErr | MRxCtlEnRxPar | MRxCtlEnGood)
//#define MRxCtlSETUP		(MRxCtlRxEn | MRxCtlStripCRC | MRxCtlINTMask )
// From usage- we enable rx interrupts later ...MaTed---
#define MRxCtlSETUP		(MRxCtlRxEn | MRxCtlStripCRC )
/*********************************************************
 * MAC Receive Status Register (7-41)
 * MACRXSTAT - offset 0xA014
 *********************************************************/
#define MRxStCtlRead	0x0020	/* Set if MAC Control frame	(type=8808H)
								   matches CAM and length == 64 			*/
#define MRxStIntRx		0x0040	/* Interrupt on Rx	*/
#define MRxStRx10Stat	0x0080	/* set if rx packet over 7 wire, reset if
								   rx'd	 over MII							*/
#define MRxStAlignErr	0x0100	/* Set if fram length in bits not a
								   multiple	of 8 and CRC invalid 			*/
#define MRxStCRCErr		0x0200	/* CRC does not compute OR PHY asserted
								   Rx_er									*/
#define MRxStOverflow	0x0400	/* Set if MAC rx FIFO full when it was
								   needed to store a rx'd byte				*/
#define MRxStLongErr	0x0800	/* Set if MAC rx'd frame > 1518 bytes		*/
#define MRxStRxPar		0x2000	/* Set if parity error in MAC rx FIFO		*/
#define MRxStGood		0x4000	/* Set if pkt with NO errors rx'd			*/
#define MRxStRxHalted	0x8000	/* Set if rx halted by RxEn->0 or HaltImm
								   set										*/
/*************************************************************
 * MAC Station Management Data Control and Address (pg 7-43)
 * STACON - offset 0xA01C
 *************************************************************/
#define MStAddr		0x001f	/* 5 bit address contained in PHY of the register
							   to be read or written						*/
#define MStPHY		0x03e0	/* 5 bit address of the PHY to be r/w			*/
#define MStWr		0x0400	/* Set to 1 to iniate write op					*/
#define MStBusy		0x0800	/* to start r/w op set to 1, MAC clears when
							   cmd completes								*/
#define MStPreSup	0x1000	/* Preamble Suppress							*/
#define MStMDC		0xe000	/* MDC clock rating see doc						*/

/**************************************************/


/**********************************************
 * Structure Definitions
 **********************************************/
#if 0 // ...MaTed--- just use the combined one
/* BDMA Registers for S3c - offset to 0x9000 */
typedef struct bdma_csr
{
  volatile UINT32 TxCtl;		/* 0x00 Buffered DMA transmit control reg	*/
  volatile UINT32 RxCtl;		/* 0x04 Buffered DMA receive control reg	*/
  volatile UINT32 TxPtr;		/* 0x08 Tx frame descriptor start addr		*/
  volatile UINT32 RxPtr;		/* 0x0c Rx frame descriptor start addr		*/
  volatile UINT32 RxLSz;		/* 0x10 Receive Frame maximum size			*/
  volatile UINT32 BdmaStat;		/* 0x14 BDMA Status							*/
  volatile UINT32 reserve1[10];	/* 0x18, 1c, 20, 24, 28, 2c, 30, 34, 38, 3c */
  volatile UINT32 TxCtlFrStat;	/* 0x40 Transmit control frame status (MAC)	*/
  volatile UINT32 reserve2[47];	/* 0x44 - 0xfc								*/
  volatile UINT32 Cam[ 32];		/* 0x100-17f								*/
} BDMA_CSR;
/* MAC Registers for S3c - offset to 0xA000 */
typedef struct mac_csr
{
  volatile UINT32 MacCtl;		/* 0x00 MAC Control							*/
  volatile UINT32 CamCtl;		/* 0x04 CAM Control							*/
  volatile UINT32 MacTxCtl;		/* 0x08 Transmit Control					*/
  volatile UINT32 MacTxStat;	/* 0x0c Transmit Status						*/
  volatile UINT32 MacRxCtl;		/* 0x10 Receive Control						*/
  volatile UINT32 MacRxStat;	/* 0x14 Receive Status						*/
  volatile UINT32 SmData;		/* 0x18 Station Management Data				*/
  volatile UINT32 SmCtl;		/* 0x1c Station Management Control 
								   and address								*/
  volatile UINT32 reserve1[2];	/* 0x20 and 0x24 reserved					*/
  volatile UINT32 CamEn;		/* 0x28 CAM Enable							*/
  volatile UINT32 reserve2[4];	/* 0x2c, 0x30, 0x34, 0x38 reserve			*/
  volatile UINT32 MissCnt;		/* 0x3c Missed Error Count					*/
  volatile UINT32 PauseCnt;		/* 0x40 Received Pause command Count		*/
  volatile UINT32 RemPauseCnt;	/* 0x44 Remote Pause Count (we sent PAUSE)	*/
} MAC_CSR;
#endif // if 0
/* Combined BDMA and MAC Registers for S3c */
typedef struct
{
  volatile UINT32 TxCtl;		/* 0x00 Buffered DMA transmit control reg	*/
  volatile UINT32 RxCtl;		/* 0x04 Buffered DMA receive control reg	*/
  volatile UINT32 TxPtr;		/* 0x08 Tx frame descriptor start addr		*/
  volatile UINT32 RxPtr;		/* 0x0c Rx frame descriptor start addr		*/
  volatile UINT32 RxLSz;		/* 0x10 Receive Frame maximum size			*/
  volatile UINT32 BdmaStat;		/* 0x14 BDMA Status							*/
  volatile UINT32 reserve1[10];	/* 0x18, 1c, 20, 24, 28, 2c, 30, 34, 38, 3c */
  volatile UINT32 TxCtlFrStat;	/* 0x40 Transmit control frame status (MAC)	*/
  volatile UINT32 reserve2[47];	/* 0x44 - 0xfc								*/
  volatile UINT32 Cam[64];		/* 0x100 -> 0x1ff, CAM Content Only 1st
								   21*6 bytes used							*/
  volatile UCHAR  TestTxBuff[256];	/* 0x200 -> 0x2ff, Tx Test buffer		*/
  UCHAR		reserve3[0x500];		/* 0x300 -> 0x7ff						*/
  volatile UCHAR  TestRxBuff1[256];	/* 0x800 -> 0x8ff, Rx Test buffer		*/
  volatile UCHAR  TestRxBuff2[256];	/* 0x900 -> 0x9ff, Rx test buffer		*/
  UCHAR		reserve4[0x600];	/* 0xa00 -> 0xfff							*/
  volatile UINT32 MacCtl;		/* 0x1000 MAC Control						*/
  volatile UINT32 CamCtl;		/* 0x1004 CAM Control						*/
  volatile UINT32 MacTxCtl;		/* 0x1008 Transmit Control					*/
  volatile UINT32 MacTxStat;	/* 0x100c Transmit Status					*/
  volatile UINT32 MacRxCtl;		/* 0x1010 Receive Control					*/
  volatile UINT32 MacRxStat;	/* 0x1014 Receive Status					*/
  volatile UINT32 SmData;		/* 0x1018 Station Management Data			*/
  volatile UINT32 SmCtl;		/* 0x101c Station Management Control 
								   and address								*/
  volatile UINT32 reserve5[2];	/* 0x1020 and 0x24 reserved					*/
  volatile UINT32 CamEn;		/* 0x1028 CAM Enable						*/
  volatile UINT32 reserve6[4];	/* 0x102c, 0x30, 0x34, 0x38 reserve			*/
  volatile UINT32 MissCnt;		/* 0x103c Missed Error Count				*/
  volatile UINT32 PauseCnt;		/* 0x1040 Received Pause command Count		*/
  volatile UINT32 RemPauseCnt;	/* 0x1044 Remote Pause Cnt (we sent PAUSE)	*/
} CSR;

/**********************************************************
 * Frame Descriptor, Buffer Descriptor related parameters.
 **********************************************************/

#define	OWNERSHIP			0x80000000	/* bit to set for BDMA owns FD		*/

/*****************************
 * Rx Descriptor Status Bits
 *****************************/
									/* SET IF ....						*/
#define	RxS_OvMax			0x0008	/* Over Maximum Size				*/
#define	RxS_CtlRcv			0x0020	/* MAC Control Packet Rx'd			*/
#define	RxS_IntRx			0x0040	/* Interrupt on Rx					*/
#define RxS_Rx10Stat		0x0080	/* Packet rx'd on 7 wire interface
									   -reset -> rx'd on MII			*/
#define	RxS_AlignErr		0x0100	/* Frame length != mutliple of 8 bits
									   and the CRC was invalid			*/
#define	RxS_CrcErr			0x0200	/* CRC error OR PHY asserted Rx_er	*/
#define	RxS_Overflow		0x0400	/* MAC rx FIFO was full when byte 
									   received							*/
#define	RxS_LongErr  		0x0800	/* rx'd frame > 1518 bytes			*/
#define RxS_RxPar			0x2000	/* MAC rx FIFO detected parity error*/
#define RxS_Good			0x4000	/* Successfully received a packet
									   with no errors					*/
#define RXS_RxHalted		0x8000	/* rx interrupted by user (rxEn or
									   haltimm)							*/
/*****************************
 * Tx Descriptor Status Bits
 *****************************/
#define TxCollCnt_mask  0x000f		/* # tx collisions						*/
#define TxCollCnt_shift      0
#define TxExColl        0x0010		/* 16 collisions occured				*/
#define TxDefer         0x0020		/* transmission defered					*/
#define TxPaused        0x0040		/* transmission paused 					*/
#define TxInt           0x0080		/* pkt tx caused an interrupt 			*/
#define TxUnder         0x0100		/* tx underrun 							*/
#define TxMaxDefer      0x0200		/* tx defered for max_defer 			*/
#define TxNCarr         0x0400		/* carrier sense not detected 			*/
#define TxSQErr         0x0800		/* SQE error 							*/
#define TxLateColl      0x1000		/* tx coll after 512 bit times 			*/
#define TxPar           0x2000		/* tx FIFO parity error 				*/
#define TxComp          0x4000		/* tx complete or discarded 			*/
#define TxHalted        0x8000		/* tx halted OR error 					*/

/*****************************
 * Tx Widget Bits
 *****************************/
#define TxWidNoPAD	0x0001			/* set for no padding					*/
#define TxWidCRC	0x0002			/* set for no CRC 						*/
#define TxWidIE	 	0x0004			/* Set for interrupt after completion	*/
#define TxWidLITTLE	0x0008			/* Set for Little endian buffer			*/
#define TxWidINC	0x0010			/* set for frame data pointer increment	*/
#define TxWidALIGN	0x0060			/* Alignment of the buffer				*/

#define TxWidSetup	(TxWidIE | TxWidLITTLE | TxWidINC | (2 << 5))

/*****************************************************************
 * PHYS defines
 *****************************************************************/
/**********************************************
 * PHY - LX972 register and defines
 **********************************************/
#define	PHYAddr			0x0001		/* Address of the LX972 for HEI board	*/
/******* registers *********/
#define PHYCtl		0 	// Control Register Refer to Table 37 on page 58 
#define PHYStat		1	// Status Register #1 Refer to Table 38 on page 58
#define	PHYId1		2 	// PHY Identification Register 1 Refer to Table 39
						//	on page 59
#define PHYId2		3	// PHY Identification Register 2 Refer to 
						//	Table 40 on page 60
#define PHYANegAd	4	// Auto-Negotiation Advertisement Register
						//	Refer to Table 41 on page 61
#define	PHYANegLink 5	// Auto-Negotiation Link Partner Base Page Ability 
						//	Register Refer to Table 42 on page 62
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
#define PHYIntEn	18 	// Interrupt Enable Register Refer to Table 48 
						//	on page 66
#define PHYIntStat	19	// Interrupt Status Register Refer to Table 49
						//	on page 66
#define	PHYLedCfg	20	// LED Configuration Register Refer to Table 50
						//	on page 68
		// 21- 29 Reserved
#define	PHYTxCtl	30 	// Transmit Control Register Refer to Table 51
						//	on page 69
/*** Only some of the defines - ones I used ...MaTed ***/
#define	PHYCtlReset				0x8000	// reset
#define PHYANegAdMask			0x1e0	// bits for duplex and speed (972)
#define PHYB8					0x100	// bit 8
#define PHYB7					0x080	// bit 7
#define PHYB6					0x040	// bit 6
#define PHYB5					0x020	// bit 5
#define	PHYLEDCFG				0xD0F2	// LED 1 (left - link and activity
										//     2 (right - Speed)
#define	PHYLEDANEG				0x1e1	// Advertize AutoNeg all speed / duplex

#define PHY_BUSYLOOP_COUNT		100		// each loop is 10mS -> 1S
#define PHY_RESETLOOP_COUNT		10		// number of times to check reset 
										//  10 ms between checks - manual
										//  states 300 uS max fro reset to work
#define	PHY_NEGLOOP_COUNT		150		// 1.5 seconds for Autoneg to complete

/**** Following are from previous version of the PHY routines -- delete ***/
#define PHY_RESET		0x8000
#define PHY_LINK_UP		0x0400
#define PHYMODE_100		0x4000 /* for 100 mbps*/
#define PHYFULL_DUPLEX	0x0200

#define SPEED_AUTO		0xFFFFFF9F	/* Set the MAC speed to Automatic mode.
									   AND operation */
#define SPEED_10		0xFFFFFFBF	/* Set the MAC speed to Automatic mode.
									   OR operation */

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
#define PHY_CONTROL_AUTONEG		    0x1000
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
    /* not used                                     0x8000  */
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

/*******************************************
 +--------+---------+--------+-------------+
  Byte 3     Byte 2    Byte 1     Byte 0
 +--------+---------+--------+-------------+
            RX FRAME DESCRIPTOR
 +--+--------------------------------------+
 |O |        Frame Data Pointer			   | 0x00
 +--+--------------------------------------+
 |              Reserved			       | 0x04
 +------------------+----------------------+
 |   Rx Status      |		Frame Length   | 0x08	
 +------------------+----------------------+
 |       Next Frame Descriptor Pointer     | 0x0c
 +-----------------------------------------+
	O - Ownership 1->BDMA, 0->CPU


           TX FRAME DESCRIPTOR
 +--+--------------------------------------+
 |O |        Frame Data Pointer			   | 0x00
 +--+-------------------------+--+-+-+-+-+-+
 |              Reserved	  |WA|A|L|T|C|P| 0x04
 +------------------+---------+--+-+-+-+-+-+
 |   Tx Status      |		Frame Length   | 0x08	
 +------------------+----------------------+
 |       Next Frame Descriptor Pointer     | 0x0c
 +-----------------------------------------+
	O - Ownership: 1->BDMA, 0->CPU
	P - No Padding:	1->No Padding Mode, 0->Padding enabled
	C - No CRC: 1->No CRC, 0->CRC enabled
	T - TX Interrupt Enable: 1->Interrupt Enable after tx of this frame
	L - Little Endian: 1->little endian, 0->Big Endian mode
	A - Frame data pointer increment: 1->increment. 0-> decrement
	WA- Widget Alignment: (cf fig 7-7)

**********************************************/


#if 0	// bit patterns didn't compile well ...MaTed
/***********************************************
 * Rx Status for Ethernet MAC of S3c
 ***********************************************/
typedef struct _RxST
{
  UINT	reserve1 	: 3;
									/* SET IF ....						*/
  UINT	OvMax 	: 1;	//	0x0008	Over Maximum Size
  UNIT	reserve2	: 1;
  UINT	CtlRxd		: 1;	//	0x0020	MAC Control Packet Rx'd
  UINT	IntRx		: 1;	//	0x0040	Interrupt on Rx
qq  UINT	Rx10Stat	: 1;	//	0x0080	Packet rx'd on 7 wire interface
							//		   -reset -> rx'd on MII
  UINT	AlignErr	: 1;	//	0x0100	Frame length != mutliple of 8 bits
							//			and the CRC was invalid
  UINT	CRCErr		: 1;	//	0x0200	CRC error OR PHY asserted Rx_er
  UINT	OverflowErr	: 1;	//	0x0400	MAC rx FIFO was full when byte 
							//			received
  UINT	LongErr		: 1;	//	0x0800	rx'd frame > 1518 bytes
  UINT	reserve3	: 1;
  UINT	RxParErr	: 1;	//	0x2000	MAC rx FIFO detected parity error
  UINT	RxGood		: 1;	//	0x4000	Successfully received a packet
							//			with no errors
  UINT	RxHalted	: 1;	//	0x8000	rx interrupted by user (rxEn or
							//		   haltimm)
} RxST;
/***********************************************
 * Tx Status for Ethernet MAC of S3c
 ***********************************************/
typedef struct _TxST
{
  UINT CollCnt	:4;	//	0x000f	# tx collisions
  UINT ExColl	:1;	//	0x0010	16 collisions occured
  UINT Defer	:1;	//	0x0020	transmission defered
  UINT Paused	:1;	//	0x0040	transmission paused
  UINT IntTx	:1;	//	0x0080	pkt tx caused an interrupt
  UINT Under	:1;	//	0x0100	tx underrun
  UINT Defer	:1;	//	0x0200	defered for max_defer
  UINT NoCarr	:1;	//	0x0400	carrier sense not detected
  UINT SQErr	:1;	//	0x0800	SQE error
  UINT LateColl :1;	//	0x1000	tx coll after 512 bit times
  UINT TxPar	:1;	//	0x2000	tx FIFO parity error
  UINT Comp		:1;	//	0x4000	tx complete or discarded
  UINT TxHalted	:1;	//	0x8000	tx halted OR error
} TxST;
/******************************
 * Tx Modes (FD)
 ******************************/
typedef struct _TxMODE
{
  UINT P	:1;	// No Padding:	1->No Padding Mode, 0->Padding enabled
  UINT C	:1;	// No CRC: 1->No CRC, 0->CRC enabled
  UINT T	:1;	// TX Interrupt Enable: 1->Interrupt Enable after tx 
				//  of this frame
  UINT L	:1;	// Little Endian: 1->little endian, 0->Big Endian mode
  UINT A	:1;	// Frame data pointer increment: 1->increment. 0-> decrement
  UINT WA	:1;	// Widget Alignment: (cf fig 7-7)
} TxMODE;
#endif	// if 0
/***********************************************
 * RX Frame Descriptors for Ethernet MAC of S3c
 ***********************************************/
typedef struct _RxFD
{
  volatile	UCHAR		*	FDDataPtr;
  UINT32       				reserved;
  volatile	UINT16        	FDLength;
  //  volatile	RxST		FDStat;
  volatile	UINT16			FDStat;
  volatile	struct _RxFD *	FDNext;
} RxFD;
/******************************
 * RX Frame Descriptor System 
 ******************************/
typedef struct _RxFDS
{
  RxFD				fd;			// rx frame descriptor
  struct sk_buff *	skb;		// address of preallocated SKB
  // NOTE :: FIXME ...MaTed--- the next probably isn't needed
  UINT32			reserve[ 3 ];	// NOTE: keeping Quad word alligned
}RxFDS;
/***********************************************
 * TX Frame Descriptors for Ethernet MAC of S3c
 ***********************************************/
typedef struct _TxFD
{
  volatile	UCHAR		  *	FDDataPtr;	// Pointer to data to be tx'd
  //volatile	TXMODES		FDModes;   	
  volatile	UCHAR			FDModes;   	
  UCHAR						reserve2;
  UINT16		        	reserve1;
  //  volatile	TxST        FDStat;
  volatile	UINT16	       	FDLength;
  volatile	UINT16	       	FDStat;
  volatile	struct _TxFD  *	FDNext;
} TxFD;
/******************************
 * TX Frame Descriptor System 
 ******************************/
typedef struct _TxFDS
{
  TxFD				fd;			// tx frame descriptor
  struct sk_buff *	skb;		// skb that is being txmitted
  // NOTE :: FIXME ...MaTed--- the next probably isn't needed
  UINT32 *			reserve[ 3 ];	// NOTE: keeping Quad word alligned
} TxFDS;


/*****************************************
 * Single Rx Buffer
 *****************************************/
typedef struct _RxBUF
{
  UCHAR	buffer[ PKT_BUF_SZ ];
} RxBUF;
/**********************
 * Control structures
 **********************/
/********************************
 * Simple Buffer Pointer Struct
 ********************************/
typedef struct _BPTR
{
  void *	first;		// first in the linked list
  void *	last;		// last in the linked list
  UINT32	num;		// number of elements in the linked list
                        //  mostly for stats
} BPTR;
  
/**********************************************************
 * PRIV structure
 * Information that need to be kept for each MAC / board
 *  Use net_device.priv to keep pointer to this struct
 **********************************************************/
typedef struct
{  /* MAC specific infomation */
   /*
   * configuration parameters,
   * - user can override later in /proc?? (still have to do this)
   */
  UINT8		phySpeed;		// 00-Auto, 0x1-10 Mbs, 0x2-100Mbs, 0x3-Auto
  UINT8		phyDpx;			// duplex 00-Half, 0x1-Full (not valid with??)
  UINT16	tcbTxThresh;	// Transmit Threshold for fifo dma xfer
 
  /* Tx control lock.  This protects the transmit buffer ring
   * state along with the "tx full" state of the driver.  This
   * means all netif_queue flow control actions are protected
   * by this lock as well.
   */
  spinlock_t	lock;

  /*
   * Need a storage place to save which SKB destructor routine to use
   */
  void (*destructor)(struct sk_buff *);   /* Destruct function */
/************************************************************************
 * For Receive and Transmit, S3c Buffer Control Structure.		*
 * Each Structre points to it own buffer space and contains interface   *
 * related information. The structre contains the link list for the     *
 * buffer.                                                              *
 ************************************************************************/
  /*
   * Queue control into data structures to S3C MAC
   */
  BPTR	qRx;		// Receive Frame Descriptor Ready List
  BPTR	qRxWait;	// Receive Frame Descriptor Not Ready List
  BPTR	qTx;		// Transmit Frame Descriptor List

  /*
   * Data structures to communicate with S3C MAC
   */
  RxFDS ___align16	rxFd[ RX_NUM_FD ];
  TxFDS	___align16	txFd[ TX_NUM_FD ];
  /*** Statistics ***/
  struct net_device_stats	stats;		// standard statistics
  struct
  {
	UINT32	BRxNl;		// BDMARxPtr has null list
	UINT32	BRxNo;		// BDMA rx is not the owner of the current data frame
						//	pointed to. Rx is stopped
	UINT32	BTxCp;		// BDMA tx indicates MAC sent a control packet
	UINT32	BTxNl;		// BDMATxPtr has a null list
	UINT32	BTxNo;		// BDMA tx is not the owner of the current data frame
						//	pointed to. Tx is stopped
  } bdmaStats;
} S3C_MAC;


/********************************************************************
 * Prototypes for external routines
 ********************************************************************/
extern void enable_samsung_irq( int int_num);
extern void disable_samsung_irq( int int_num);
extern inline void invalidate_dcache_line( unsigned long addr);
extern unsigned char * get_MAC_address( char * name);
//extern int get_ethernet_addr( char * ethaddr_name, char *ethernet_addr);
