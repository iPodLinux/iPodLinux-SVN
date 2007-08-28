/***********************************************************************
 *  s3c2500.c : Samsung S3c2500x/a/b ethernet driver for linux

    S3c2500 Communication Processor
    Copyright (c) 2002-2003 Arcturus Networks Inc. <www.ArcturusNetworks.com>
			    by MaTed mated@ArcturusNetworks.com 

      based on isa-skeleton: A network driver outline for linux.
	    Written 1993-94 by Donald Becker.

		Copyright 2002 Arcturus Networks Inc.

	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

	The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
	Center of Excellence in Space Data and Information Sciences
	   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

	This file is the first go at creating a driver for the ethernet driver
        used by the S3C SOC. The core is the 10/100 MB Sierra Research core
        for which there doesn't appear to be a driver for the Linux
        operating system.

	Known Limitations:
	- Because the S3c controller is "intelligent", it manages the
	buffer list on its own. Therefore you create the Buffer List for
	the controller without knowing the length required. A maximum MTU
	buffer is pre-allocated regardless of the actual requirements. Either
	the controller firmware can be changed to pick buffers whose size
	is closer to the actual size required, or we can memcpy (ugh!!!) to
	a smaller buffer.
    
    - We need to invalidate the cache for the received buffer. If we do it
    after we know the length, then we are doing during interrupt time (but
    we are only invalidating the section we need to (also faster). If we do
    it when we are preallocating the buffers, we must do the entire buffer,
    which takes longer. Best solution would be to move it up one level, but
    we should only do it for ethernet packets.

	- module support is either non-existant or not tested (it was a SOC
	afterall)

*************************************************************************/

/************************************************************************
 * modification history
 * --------------------
 *	07/02/02 MaTed <mated@ArcturusNetworks.com>
 *			- eth0 is the LAN side for some of the MIB application stuff.
 *			  - on the 2500 (ArcNet board), eth1 is the Kendin switch, so we
 *				need to reverse the assignments so the Kendin is eth0 (LAN 
 *				side)
 *			- PHY's will be changing on the new 2500 board as well
 *			  - want to table drive the PHY.
 *			  - going from two LSI PHYs to an intel and a Kendin switch
 *
 *	06/07/02 MaTed <mated@ArcturusNetworks.com>
 *			- Algorithm change - tx now will interrupt every packet
 *			  - CPU saving over accurate count of collisions
 * 	05/13/02 MaTed <mated@ArcturusNetworks.com>
 *			- Initial port to s3c2500x using s3c4530 as base
 *			- * Nomemclature change frame descriptors are now buffer
 *				descriptors - there are no frame descriptors, make the
 *				change in your mind for now
 * 	10/29/01 MaTed <mated@ArcturusNetworks.com> first go 
 *	- PLEASE set TABS==4
 ************************************************************************/
/*
  Sources:
  - source is available from Arcturus Networks
 
  Finally, keep in mind that the Linux kernel is has an API, not
  ABI. Proprietary object-code-only distributions are not permitted
  under the GPL.
*/


/***************************************
 * CONFIGURATION
 ***************************************/
// MiniProfiling enable
#define MPROFILE	0		// enable miniprofile
// Next is so globals only set up here
#define S3C2500_ETHERNET	1	

/***********************************************
 * INCLUDES - includes are in the included file
 ***********************************************/
#include "s3c2500.h"

#include <asm/s3cmprofile.h>	// for the mprofiling

#ifdef CONFIG_ETH_S3C2500	/* skip the entire file if not configured 	*/

/*
 * The K should give us a unique tag (date / time / revision) when pulled from
 * BitKeeper - see bitkeeper keywords
 */
static const char *version =
"S3C2500: ETH driver. Version 1.14 <www.ArcturusNetworks.com>\n";

#define s3c_mdelay(n) mdelay(n)	// renamed for debugging of mdelay timing

/************* DEBUG **********/
// Also see .h file for NET_DEBUG (the higher the #, more debug code included

#define VERBOSE 5		// Higher the number, The more output

#define	DEBUG_CRC_OFF	0
#define DEBUG_PHY_DUMP	0
#define	PHY_DEBUG		0

#define DEBUG_RX_DUMP	0	// for dumping Rx Structures
#define DEBUG_SKB_DATALEN	0	// debugging skb->data_len problem (closed)

#define MATED_RX_OF_BUG	0	// log and print last 10 rx buffer sizes
                            //  on RX Overflow
/*********** end debug *******/

/*********************** MORE Configuration *********************************/
// Define zero or more of the following
// - can also be used to force a 100 Full duplex link
#if defined(CONFIG_BOARD_EVS3C2500RGP) || defined(CONFIG_BOARD_S3C2500REFRGP)
	// for eth0 ( swapped from ethernet 1) channel 0- Kendin on VX
#define	PHY_0_FORCE	PHY_CONTROL_SPEED_100_FULL
#endif
#if 0		// 0-> autonegotiate
// for eth1 ( swapped from ethernet 0) channel 1 -INTEL  
//#define	PHY_1_FORCE PHY_CONTROL_SPEED_100_FULL
#define	PHY_1_FORCE PHY_CONTROL_SPEED_100_HALF
#endif

/****************************************************************************
 * A bit of an introduction for the algorithms used herein
 * - This driver has been rewritten to use the more traditional copy buffers
 *   - the copy routine is more optimized in that it is rolled out and
 *     we have preknowledge that it begins on a WORD boundary with the first
 *     two bytes being dummies
 * - the transmit complete interrupt is not used
******************************************************************************/

/*
 * The name of the card. Is used for messages and in the requests for
 * io regions, irqs and dma channels
 */
// static const char* cardname = "s3c2500";

/**********************************************
Compile time directives
***********************************************/
#define NO_INCLUDE_MAC_RX_INT	1		// do not include (or set up) the
										//  MAC receive interrupt
										//  - Currently unused
#define	ENABLE_DESTRUCTOR		0		// use destructor to set
										//  - Needs work if enabled (previous
										//    version)
/************************************************************************
	"Knobs" that adjust features and parameters.
*******************************************************************/

/* Allow setting MTU to a larger size, bypassing the normal ethernet setup. */
/**** Currently unused ...MaTed--- ****/
static const int mtu = 1500;

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
#define RX_ALL_PKTS	1
#if !RX_ALL_PKTS	/* We have to handle all the rx pkts, 
					   latency problems ...MaTed */
#define MAXINT_WORK_RX		RX_NUM_FD/ 16	// set to one eight of Rx buffers
//#define MAXINT_WORK_RX		RX_NUM_FD/ 4	// set to one quarter of Rx buffers
/*
#define MAXINT_WORK_RX		RX_NUM_FD/ 2	// set to one half of Rx buffers
*/
//static int max_interrupt_work = MAXINT_WORK;
#endif

/******************************************************
 * Internal S3C2500 data structures
 * Should have made this into an array of 
 *  two structures --- Maybe later ...MaTed---
 ******************************************************/
static	UINT32	numS3c2500Macs	= 0;	// number of macs
#if 1	// force reversal of eth0 and eth1 assignements
static	CSR * 	MacBases[ ETH_MAXUNITS] = {(CSR *) 0xf00c0000,
										   (CSR *) 0xf00a0000};
static	UINT32	PhyHwAddr[ ETH_MAXUNITS] = { MPHYHWADDR1, MPHYHWADDR0};
static	UINT32	IRQNumTx[ETH_MAXUNITS] = { ETH1_TX_IRQ, ETH0_TX_IRQ};
static	UINT32	IRQNumRx[ETH_MAXUNITS] = { ETH1_RX_IRQ, ETH0_RX_IRQ};
static	char *	IntNames[2 * ETH_MAXUNITS ]
            = {"eth1-s3cTX", "eth1-s3cRX", "eth0-s3cTX", "eth0-s3cRX"};
static	char *	MacName[]
	= {"S3c2500-1", "S3c2500-0", "unknown"};
static	char *	MacHWAddr[]	= {"HWADDR1", "HWADDR0", "unknown"};
#else
static	CSR * 	MacBases[ ETH_MAXUNITS] = {(CSR *) 0xf00a0000,
										   (CSR *) 0xf00c0000};
static	UINT32	PhyHwAddr[ ETH_MAXUNITS] = { MPHYHWADDR0, MPHYHWADDR1};
static	UINT32	IRQNumTx[ETH_MAXUNITS] = { ETH0_TX_IRQ, ETH1_TX_IRQ};
static	UINT32	IRQNumRx[ETH_MAXUNITS] = { ETH0_RX_IRQ, ETH1_RX_IRQ};
static	char *	IntNames[2 * ETH_MAXUNITS ]
            = {"eth0-s3cTX", "eth0-s3cRX", "eth1-s3cTX", "eth1-s3cRX"};
static	char *	MacName[]
//	= {"ethaddr0", "ethaddr1", "ethaddr2", "unknown"};
//	= {"S3c2500-0", "S3c2500-1", "S3c2500-2", "unknown"};
	= {"S3c2500-0", "S3c2500-1", "unknown"};
static	char *	MacHWAddr[]	= {"HWADDR0", "HWADDR1", "unknown"};
#endif

  /*
   * Structures for S3c2500 frame descriptors
   * - These four structure will be allocated on a boundary so 
   *   so the structures can be accessed without cacheing
   * - For now we don't align, - need special linker script and area
   */
typedef struct 
{
RxFD	___align16	rxFdArray [ETH_MAXUNITS] [ RX_NUM_FD];
TxFD	___align16	txFdArray [ETH_MAXUNITS] [ TX_NUM_FD];
#if CONFIG_WBUFFER
TxBUFF	___align16	txFdBuffers [ETH_MAXUNITS] [ TX_NUM_FD];
#endif // CONFIG_WBUFFER
} FDAArea;

FDAArea	* FDescUncached;	// pointer to area kmalloc'd

#if MATED_RX_OF_BUG			// printk the previous buffer lengths
#define RX_OF_MAX_LOG	10
// It would be NICE to init this array, but it's only debug
UINT32	rx_len_array[ ETH_MAXUNITS] [RX_OF_MAX_LOG];
	// index into rx_len_array- where next will go in
UINT32	rx_len_index[ETH_MAXUNITS] = {0,0};
int position_output = 0;
#endif

/************************************************************************
 * Internal Function Declarations - all static
 ************************************************************************/

static _INLINE_ UINT16 incIndex( UINT16 index, UINT16 max);
static _INLINE_ int PHYWait( UINT32 chan );
static int WritePhy( UINT32 chan, UINT32 reg, UINT32 data );
static int ReadPhy( UINT32 chan, UINT32 reg, UINT32 * data );
static _INLINE_ int txFull( ND dev );
static void SetupPHY( UINT32 chan, UINT32 mode );
static ULONG PHYCheckCable(  S3C2500_MAC * np);
static int PhyInit( ND dev );
static ULONG SetupTxFDs( ND dev );
static ULONG SetupRxFDs( ND dev);
static _INLINE_ void EnableInt( UINT32 chan );
static _INLINE_ void DisableInt( UINT32 chan );
static _INLINE_ void EnableTxRx( UINT32 chan );
static _INLINE_ void DisableTxRx( UINT32 chan );
static int Reset( UINT32 chan );
static void SetupRegistersTx( ND dev );
static void SetupRegistersRx( ND dev );
//static _INLINE_ void SetUpTxFrmPtrRegister( ND dev);
static _INLINE_ ULONG TxRxInit( ND dev);
static int SetMacAddr( ND dev, void * addr);
static int SetUpARC( ND dev, void * addr);
#if ENABLE_IOCTL
static _INLINE_ void SetSpeed100( UINT32 chan );
static _INLINE_ void SetSpeed10( UINT32 chan );
#endif	// ENABLE_IOCTL

/* Index to functions, as function prototypes. */

int __init	s3c2500_probe( ND dev);
static int	s3c2500_InitVars( ND dev, UINT32 chan);
static _INLINE_ int s3c2500_request_irq_mem( ND dev, UINT32 chan);

static int	s3c2500_probe1( ND dev, UINT32 chan);
static _INLINE_ int s3c2500LogTxMACError( ND dev, int status);
static _INLINE_ int s3c2500LogRxBdmaError( ND dev, int status);
static void	s3c2500_tx_timeout( ND dev);
static _INLINE_ int 	s3c2500InitPriv( ND dev, UINT32 chan );

static int	s3c2500_open( ND dev);
static int	s3c2500_send_packet( struct sk_buff *skb, ND dev);
/* two interrupt routines - with one helper */
static void	s3c2500_Tx( int irq, void *dev_id, struct pt_regs * regs);
//static _INLINE_ void TxPktCompletion( ND dev);
static void TxPktCompletion( ND dev);
static void	s3c2500_Rx( int irq, void *dev_id, struct pt_regs * regs);
#if DEBUG_RX_DUMP
static void dumpRx( UINT32 chan, S3C2500_MAC * priv, char * where);
static void RXFDS_Dump( RxFDS * fds, char * str);
#endif
#if	ENABLE_DESTRUCTOR 
/ We cause an error, because there are problems with this  ...MaTed---
static void	s3c2500Destructor( struct sk_buff *skb);
#endif
static int	s3c2500_close( ND dev);
static struct net_device_stats	*s3c2500_get_stats( ND dev);
static void	set_multicast_list( ND dev);
static void	HardwareSetFilter( ND dev);

#if DEBUG_PHY_DUMP
static void DumpPhyReg( UINT32 chan);
#endif

/************************************
 * Temporary routines
 ************************************/
#define S3C2500_TEMP_ROUT1 1
#if S3C2500_TEMP_ROUT1 
// what do I need to include to get this one
#define abs(x) (((int)x < 0)? -x : x)
#endif

#if 0	// All the setup is down elsewhere now
// need the real versions when data cache get enabled
/*****************************************************
 * s3c2500_uncache_region( start, size )
 * Description:
 *		This routine force memory starting at "start" for a length
 *	of "size" bytes to always be uncached memory
 * NOTES:
 *	- the start parameter must be on a memory boundary of "size", i.e.
 *	if size is 1 MB, then start must be on a 1MB boundary.
 *	- kmalloc is guarenteed to return allocated memory on the 
 *	boundary 
 * Parameters:
 *		start	- pointer to the first location of the region to be
 *				  made uncachable
 *				- must start on boundary of "size"
 *		size	- number of bytes for the region to be uncached
 *				- this value MUST be a power of 2
 * Returns:
 *		S3C2500_SUCCESS == 0 	- successfull
 *		-1						- Address invalid
 *								- probably address is outside of ram
 *		-2						- invalid size parameter
 *		-3						- No free region
 *								- Only 4 available
 *****************************************************/
int _INLINE_ s3c2500_uncache_region( void * start_addr, unsigned int size)
{
  // the third parm == 0 => data
  return( ask_s3c2500_uncache_region( start_addr, size, 0) );
}

# if 0	// I don't believe we will be using these routines
int s3c_invalidate_cache( void * start_address, unsigned int length)
{
  return (S3C2500_SUCCESS);
}
int s3c_flush_cache_region( void * start_addr, unsigned int size)
{
  asm("				\
");
       
  return (S3C2500_SUCCESS);
}
# endif
#endif
/************************************************************************
 * Utility routines / defines
 ************************************************************************/
// NOTE: assumes baseAddr will reflect channel number (A/B)
#define WriteCSR( reg, value) \
		((volatile UINT32)(((CSR *)( baseAddr))->reg) = (value))
#define ReadCSR( reg) ((volatile UINT32)(((CSR *) (baseAddr))->reg))

/***************************************************************************
 *	Function Name:      txFull
 *
 *  Purpose:	Returns 0 if tx Queue in not full, !=0 if full
 *
 *  Inputs:     ND dev
 ***************************************************************************/
static _INLINE_ int 
txFull( ND dev)
{
  S3C2500_MAC *	np = (S3C2500_MAC *) dev->priv;
  return (np->ringTx.num) >= (np->ringTx.max);
}
/***************************************************************************
 *	Function Name:      incIndex
 *
 *  Purpose:	Returns new index
 *
 *  Inputs:     index and max
 ***************************************************************************/
static _INLINE_ UINT16
incIndex( UINT16 index, UINT16 max)
{
  return ( (++index > max) ? 0 : index); 
}
/*************************************************************************
 * PHYWait - Waits for Station Management to become non-busy
 *
 * This inline routine is a helper to wait for Station Management to 
 *	become non-busy - ie for the command to complete
 *
 * returns	S3C2500_SUCCESS if succesfully written
 *       	other returns indicate timeout or other error
 *************************************************************************/
static _INLINE_ int 
PHYWait( UINT32 chan)
{
  UINT32 i = 0;
  CSR * baseAddr = MacBases[ chan ];

  while (( ReadCSR( SmCtl)) & MPHYbusy)
  {
    if ( i++ >= PHY_BUSYLOOP_COUNT)
    {
      if ( VERBOSE > 2)
      {
		printk( "S3C2500-PHY: *** BUSY Timeout! ***\n");
      }
      return( S3C2500_TIMEOUT);
    }
	s3c_mdelay( PHY10MSDELAY);	// 10 millisecond delay per loop
  }
  return( S3C2500_SUCCESS);
}
/*************************************************************************
 * WritePHY - writes data to phy
 *
 * This routine is a helper to write Phy data
 *
 * data is written to reg of the PHY at addr
 * returns	S3c2500_SUCCESS if succesfully written
 *       	other returns indicate timeout or other error
 *************************************************************************/
static int 
WritePhy( UINT32 chan, UINT32 reg, UINT32 data)
{
  int 		rc;
  CSR * 	baseAddr = MacBases[ chan ];
  UINT32	addr = PhyHwAddr[ chan ];

  if ( S3C2500_SUCCESS != ( rc= PHYWait( chan )))
	return( rc);
  WriteCSR( SmData, data);  	/* set data */
  WriteCSR( SmCtl, MPHYwrite | MPHYbusy | reg | addr);	// send WRITE cmd
#if PHY_DEBUG
  printk( "S3C2500-PHY: wrote 0x%08x to PHY%d register 0x%x\n",
		  (int) data, (int) addr, (int) reg);
#endif
  return( PHYWait( chan ));
}
/*************************************************************************
 * ReadPHY - reads data from phy
 *
 * This routine is a helper to read Phy data
 *
 * data is read from reg of the PHY at addr
 * returns	S3C2500_SUCCESS if succesfully read
 *       	other returns indicate timeout or other error
 *************************************************************************/
static int 
ReadPhy( UINT32 chan, UINT32 reg, UINT32 * data)
{
  int 		rc;
  CSR * 	baseAddr = MacBases[ chan ];
  UINT32	addr = PhyHwAddr[ chan ];

  	// wait for any previous command to complete
  if ( S3C2500_SUCCESS != ( rc= PHYWait( chan )))
	return( rc);
  WriteCSR( SmCtl, MPHYbusy | reg | addr );	// send READ cmd
  // wait for read command to complete
  if ( S3C2500_SUCCESS != ( rc= PHYWait( chan )))
	return( rc);
  *data = ReadCSR( SmData);  	/* get data */
#if PHY_DEBUG
    printk( "S3C2500-PHY: read 0x%08x from PHY%d register 0x%x\n",
			(int) *data, (int) addr, (int) reg);
#endif
  return (rc);					/* return success */
}
/*************************************************************************
 * SetupPHY - initialize and configure the PHY device.
 *
 * This routine initializes and configures the PHY device 
 *	***** NOTE: the LED values used for initialization are overwritten
 *
 * Loopback=disabled
 * Speed = 100/10
 * Auto-Negotiation Restart = mode enabled --- FIXME NOTE: Assumption that
 *									   -- FIXME		Negotiation had not
 *										- FIXME		previously run ???
 * Power Down = 0 normal
 * Isolate = 0 normal
 * Restart Auto-Negotiation = 0
 * Duplex = 1 Full / 0 Half
 * Collision Test = 0 Disabled
 *
 **** parm mode values *****
 * For Full Dup 10Mbps:  0x0100
 * For Half Dup 10Mbps:  0x0000
 * For Full Dup 100Mbps: 0x2100
 * For Half Dup 100Mbps: 0x2000
 * For Auto Negotiate:   0x1000		- half duplex 10 advertised
 * For Auto Negotiate:   0x1100		- full/half duplex 10 advertised
 * For Auto Negotiate:   0x3000		- half duplex 10/100 advertised
 * For Auto Negotiate:   0x3100		- full/half duplex 10/100 advertised
 * For Restart AutoNeg:  0x0200		- current values retained
 *
 *************************************************************************/
static void 
SetupPHY( UINT32 chan, UINT32 mode) 
{
  UINT32	j;		// loop counter - what else would I be
  UINT32	data;	// save the data read back from the control register

  /* First reset the PHY */
  WritePhy( chan, PHYCtl, PHYCtlReset);
  /*
   * Read the Reset bit(0.15) of the PHY to see if the reset is completed.
   */
  for ( j = 0; j < PHY_RESETLOOP_COUNT; j++)
  {
    if (S3C2500_SUCCESS != ReadPhy( chan, PHYCtl, &data))
	{
	  if (VERBOSE > 2)
		{
		  printk( "S3C2500-PHY: RESET command not accepted! ***\n");
		}
	  return;
	}
    if ( (data & PHY_RESET) == 0)
    {
      if ( VERBOSE > 2 )
      {
		printk( "S3C2500-PHY reset\n");
      }
      break;
    }
	s3c_mdelay( PHY10MSDELAY);	// 10 mS - Hopefully someone will do something
	                //  to mdelay so it might actually do something
	                //  these time slices allocated to it. Right now
                    //  it's a hard loop
  }
#ifdef PHY_INTEL
  s3c_mdelay( PHY10MSDELAY);	// just in case
  // change the LED to right link/activity LEFT->speed
  WritePhy( chan, PHYLedCfg, PHYLEDCFG);
#endif

#if PHY_DEBUG
  printk( "S3C2500-PHY: setting up mode-%08x\n", (int) mode);
#endif
  if (S3C2500_SUCCESS != ReadPhy( chan, PHYCtl, &data))
  {
	if (VERBOSE > 2)
	  {
		printk( "S3C2500-PHY: Read mode command not accepted! ***\n");
	  }
	return;
  }
#if PHY_DEBUG
  printk( "S3C2500-PHY: Original mode-%08x\n", (int) data);
#endif
  /*
   * Rules for mode
   *  if b12 is set	- autonegotiation is enabled -> for the other bits set
   *				- advertizing will match 
   *  if b12 is reset - no autonegotian, adverting is not changed
   *  b13 set to 100 MbS (includes 10 if autoneg) 
   *  b13 reset -> 10 Mbs only 
   *  b8 set FULL DUPLEX (autonegiate -> HALF Duplex included)
   *  b8 reset Half Duplex
   */
  if (0 == (mode & PHY_CONTROL_AUTONEG))
  {	// No autonegotiate - just write mode and done
	if (S3C2500_SUCCESS != WritePhy( chan, PHYCtl, mode))
	{
	if (VERBOSE > 2)
	  {
		printk( "S3C2500-PHY: Mode change failed for Control\n");
	  }
	}
	return;		// no autonegotiation - no advertising
  }

#if 1	// allow mode control
  // Advertise to the world what we will negotiate
  if (S3C2500_SUCCESS != ReadPhy( chan, PHYANegAd, &data))
  {
	if (VERBOSE > 2)
	  {
		printk( "S3C2500-PHY: Read Negotiate Advertize not accepted! ***\n");
	  }
	return;
  }
  data &= ~PHYANegAdMask;
  switch (mode & (PHY_CONTROL_SELECT_SPEED | PHY_CONTROL_DUPLEXMODE))
  {
  case PHY_CONTROL_SELECT_SPEED | PHY_CONTROL_DUPLEXMODE:
	data |= PHYB8 | PHYB7 | PHYB6 | PHYB5;
	break;
  case PHY_CONTROL_SELECT_SPEED:
	data |= PHYB7 | PHYB5;
	break;
  case PHY_CONTROL_DUPLEXMODE:
	data |= PHYB6 | PHYB5;
	break;
  case 0:
	data |= PHYB5;
	break;
  }
#else
  // put in everything
  data = PHYLEDANEG;
#endif  // end of allow mode control
  if (S3C2500_SUCCESS != WritePhy( chan, PHYANegAd, data))
  {
	if (VERBOSE > 2)
	  {
		printk( "S3C2500-PHY: Mode change failed for Advertise\n");
	  }
	return;
  }
  /* since we are auto negotiating, force a restart */
  mode |= PHY_CONTROL_RESTART_AUTONEG;	// just in case
  if (S3C2500_SUCCESS != WritePhy( chan, PHYCtl, mode))
  {
	if (VERBOSE > 2)
	  {
		printk( "S3C2500-PHY: Autonegotiate failed to restart\n");
	  }
	return;
  }
  // check for the end of the negotiation start
  j = 0;
  ReadPhy( chan, PHYCtl, &data);
  while ( data & PHY_CONTROL_RESTART_AUTONEG)
  {
    if ( j++ >= PHY_NEGLOOP_COUNT)
    {
	  if ( VERBOSE > 1)
      {
		printk( "S3C2500-PHY: * Restart AutoNegotiate did not complete ");
		printk( " Will check again later ***\n");
      }
      return;
    }
	s3c_mdelay( PHY10MSDELAY);	// 10 millisecond delay per loop
	ReadPhy( chan, PHYCtl, &data);
  }
#ifdef PHY_INTEL
  s3c_mdelay( PHY10MSDELAY);	// just in case
  // change the LED to right link/activity LEFT->speed
  WritePhy( chan, PHYLedCfg, PHYLEDCFG);
#endif
  return;
}
/*************************************************************************
  * PHYCheckCable - Check for Cable connection to the PHY 972.
  *
  *
  * RETURNS: S3C2500_SUCCESS, or S3C2500_FAIL
  * ** * NOTE: routine now sets ph
  * Needs to be modified according to the PHY, and 
  * needs to be modified according to the SPEC of Level1 972
  *
  **************************************************************************/
static UINT32 
PHYCheckCable( S3C2500_MAC * np)
{
  UINT32	data, data2;
  UINT32	chan = np->channel;

  if (S3C2500_SUCCESS != ReadPhy( chan, PHYStat2, &data2))
  {
	if ( VERBOSE > 2)
	  {
		printk( "S3C2500-CheckCable: Read status 2 command not accepted! ***\n");
	  }
	return (S3C2500_FAIL);
  }
#ifdef PHY_LSI
  if (S3C2500_SUCCESS != ReadPhy( chan, PHYStat, &data))
  {
	if ( VERBOSE > 2)
	  {
		printk( "S3C2500-CheckCable: Read status command not accepted! ***\n");
	  }
	return (S3C2500_FAIL);
  }
#else
#ifdef PHY_INTEL
  data = data2;				// I know this is klugy ...MaTed---
#endif
#endif  // else PHY_LSI
#if PHY_DEBUG
  printk("S3C2500-CheckCable: Status = 0x%x - Status 2 = 0x%x\n", 
		 (int) data, (int) data2);
#endif
  if (!(data & PHY_STATUS_LINK_UP))
    return( S3C2500_FAIL);
  else if ( VERBOSE )
	{
	  if (data2 & PHY_STATUS_LINK_100)
	  {
		printk( "S3C2500-PHY: running at 100 Mbps, ");
		np->phySpeedNeg = 2;		// 0 and 3 are autoneg indicators
	  }
	  else
	  {
		printk( "S3C2500-PHY: set for 10 Mbps,  ");
		np->phySpeedNeg = 1;		// 0 and 3 are autoneg indicators
	  }
	  
	  if(data2 & PHY_STATUS_LINK_DUPLEX)
	  {
		printk( "FULL Duplex Mode.\n");
		np->phyDpxNeg = 1;
	  }
	  else
	  {
		printk( "HALF Duplex Mode. \n");
		np->phyDpxNeg = 0;
	  }
	}
  return S3C2500_SUCCESS;
}
/*************************************************************************
 * PhyInit - initialize and configure the PHY device.
 *
 * This routine initialize and configure the PHY device 
 *
 * RETURNS: S3C2500_SUCCESS, or S3C2500_FAIL
 * Needs to be modified according to the PHY, and 
 * needs to be modified according to the SPEC of Level1 972.
 **************************************************************************/
static int 
PhyInit( ND dev)
{
  S3C2500_MAC *	np = (S3C2500_MAC *) dev->priv;
  UINT32	chan = np->channel;
  CSR *		baseAddr = (CSR *) (dev->base_addr);
  UINT32	status = S3C2500_SUCCESS;
  UINT32	i;
#if !(defined( PHY_0_FORCE ) && defined( PHY_1_FORCE ))
  static const UINT32 modeList[] = 
  {
	PHY_CONTROL_AUTONEG_HALF_10,	PHY_CONTROL_SPEED_10_HALF,
	PHY_CONTROL_SPEED_100_HALF,		PHY_CONTROL_AUTONEG_HALF_100,
	PHY_CONTROL_AUTONEG_FULL_10, 	PHY_CONTROL_SPEED_10_FULL,
	PHY_CONTROL_SPEED_100_FULL,		PHY_CONTROL_AUTONEG_FULL_100
  };	
#endif

  /* Kendin switch has not PHY - setup and return */
#if defined(CONFIG_BOARD_EVS3C2500RGP) || defined(CONFIG_BOARD_S3C2500REFRGP)
  if (chan == 0)
	{
		printk( "S3C2500-PHY: running at 100 Mbps, ");
		np->phySpeedNeg = 2;		// 0 and 3 are autoneg indicators
		printk( "FULL Duplex Mode.\n");
		np->phyDpxNeg = 1;
		return ( S3C2500_SUCCESS);
	}	  
#endif   // CONFIG_BOARD_EVS3C2500RGP or CONFIG_BOARD_S3C2500REFRGP

#if DEBUG_PHY_DUMP
  DumpPhyReg( chan );
#endif	//DEBUG_PHY_DUMP

  // Setup duplex and speed to pass on
  //   automatic get 0
  // setup Station Management data -- NOTE: Incomplete Data
#if defined( PHY_0_FORCE ) && defined( PHY_1_FORCE )
  SetupPHY( chan, (chan) ? PHY_1_FORCE : PHY_0_FORCE);
#else
  #if defined( PHY_0_FORCE )
    SetupPHY( chan, (chan) 
			  ? modeList[ np->phySpeed + (np->phyDpx << 2)]
			  : PHY_0_FORCE);
  #endif
  #if defined( PHY_1_FORCE )
	SetupPHY( chan, (chan) 
			  ? PHY_1_FORCE
			  : modeList[ np->phySpeed + (np->phyDpx << 2)]);
  #endif
  #if !(defined( PHY_0_FORCE ) || defined( PHY_1_FORCE ))
	SetupPHY( chan, modeList[ np->phySpeed + (np->phyDpx << 2)]);
  #endif
#endif

  for ( i = 0; i < PHY_BUSYLOOP_COUNT; i++) 
  {
	if ( S3C2500_SUCCESS == (status = PHYCheckCable( np )))
	{
	  if ( VERBOSE )
      {
		printk( "S3C2500-PHY: Cable detected\n");
	  }
	  break;
	}
	s3c_mdelay( PHY10MSDELAY);	// 10 mS
  }
  if ( status != S3C2500_SUCCESS)
  {
    printk( "S3C2500: **** We believe NO Cable is connected to MAC %s %08lx\n", 
			dev->name, (UINT32) baseAddr);
  }
#if DEBUG_PHY_DUMP
  DumpPhyReg( chan );
#endif	//DEBUG_PHY_DUMP

#if 0		// no failure if no cable
  return ( status);
#else
  return ( S3C2500_SUCCESS);
#endif
} 
/*********************************************************************
 *	Function Name:  SetupTxFDs
 *
 * Purpose: For transmit frame and buffer descriptors
 *          - set up initial values for these.
 *	    	- setup queue pointers
 *			- FDS setup as a ring 
 *
 *	** Assumes called just after dev set up, and priv defined
 *
 * Inputs:  Pointer to dev structure
 *
 * Return Value:
 *	S3C2500_SUCCESS
 *	S3C2500_FAIL
 *********************************************************************/
static ULONG 
SetupTxFDs( ND dev )
{
  int 			i;
  S3C2500_MAC *	np = dev->priv;			// pointer to private are
  RINGBUFF 	  *	ringTx = & np->ringTx;	// pointer to tx ring buffer
  int 			baseAddr = dev->base_addr;

  if (0 == np)		// we could drop this test
  {
    return ( S3C2500_FAIL);
  }
  /* Setup the ring indices */
  ringTx->first
    = ringTx->last = ringTx->num = 0;	// no buffers in use
  ringTx->max = TX_NUM_FD - 1;

  /* for all the tx frames */
  for(i = 0; i < TX_NUM_FD; i++) 
  {  
#if CONFIG_WBUFFER
	// this needs checking - WBUFFER never in use yet ...MaTed---
	np->txFd[ i ].FDDataPtr = np->txBuff[ i ].FDData;
#else	// FDDataPtr gets setup in send_packet, if no write caching
	// if you need to debug, might want to set the corresponding SKB
	// in txBuff (txSkb) to NULL - it's not a required thing

#endif // CONFIG_WBUFFER
//	np->txFd[ i ].FDLength = 0x0000;		// don't need this either
	np->txFd[ i ].FDStat = TxWidSet16;		// ownership and preset offset
  }	
  if ( VERBOSE > 1)
  {
    printk( KERN_INFO "SetupTxFDs-for %s, First buff %x, Last %x\n",
	    dev->name, (int) &np->txFd[ 0 ],
	    (int) &np->txFd[ TX_NUM_FD - 1]);
  }
  /*
   * Now tell the MAC about the buffers
   */
  WriteCSR( TxPtr, np->txFd);
  WriteCSR( TxCnt, 0);		// none ready to go
  return ( S3C2500_SUCCESS);
}
/***********************************************************************
 * Function Name:	SetupRxFDs                              
 *                                                                          
 * Purpose: Prepare memory for receive buffers and frame descriptors
 *
 *	** Assumes called just after dev set up, and priv defined
 *
 * Inputs:  Pointer to dev structure
 *
 * Return Value:
 *	S3C2500_SUCCESS
 *	S3C2500_FAIL
 ***********************************************************************/
static ULONG SetupRxFDs( ND dev)
{
  int 				i;
  S3C2500_MAC * np = (S3C2500_MAC *) dev->priv;	// pointer to the private 
                                                //  s3c2500 area
  RINGBUFF 			* ringRx = & np->ringRx;	// pointer to rx ring buffer
  int 				baseAddr = dev->base_addr;

  if ( 0 == np)
  {
    return ( S3C2500_FAIL);
  }
  /* Setup the ring indices */
  ringRx->first
	= 0;	// no buffers in use
  // ** NOTE last is set one behind first
  ringRx->max = ringRx->last = RX_NUM_FD - 1;

  /*
   * Initialize the FD Area. Give the ownership of each 16
   * byte block to the controller - ready to RX. (The patterns of
   * 0xBBB etc is for debugging only!)
   ** NOTE We do the last one - even though 
   ** NOTE: We don't do the last one, spacer for EOL
   */
  for ( i = 0; i < RX_NUM_FD; i++)
  {
	/*
	 * Each FD points to a large enough buffer so that for each
	 * incoming packet will fit in one buffer.
	 *
	 */
	np->rxSkb[ i ].FDskb 
	  = __dev_alloc_skb( MAX_PKT_SIZE, GFP_ATOMIC | GFP_DMA);
	np->rxFd[ i ].FDDataPtr = np->rxSkb[ i ].FDskb->data;	/* point to skb 
															   data buffer */
	np->rxFd[ i ].FDLength = PKT_BUF_SZ;	// think the compiler is smart
	np->rxFd[ i ].FDStat = OWNERSHIP16;		//  enough to put these two lines 
	                                        //  together??
	skb_reserve( np->rxSkb[ i ].FDskb, 2);         // alignment 

    if (NET_DEBUG > 1) // mated_debugging
	{
	  ASSERT( np->rxSkb[ i ].FDskb->list == 0);
	}
  }
  // we aren't going to stop the BDMA engine from getting to the 
  //	last one, It stops itself
  // Need to put in the EOL (last one owned by CPU)
  // NOTE: ALL entries including the last one have an uncached SKB assigned
  //		We just mark it as unused
  np->rxFd[ RX_NUM_FD - 1 ].FDStat = 0;
  np->rxFd[ RX_NUM_FD - 1 ].FDLength = 0;

  //  np->rxFd[ RX_NUM_FD - 1 ].FDDataPtr = NULL;	// point to Nothing

  if ( VERBOSE > 1)
  {
    printk( "SetupRxFDs-for %s\n", dev->name);
  }
  /*
   * Now tell the MAC about the buffers
   */
  WriteCSR( RxPtr, np->rxFd);
  WriteCSR( RxCnt, RX_NUM_FD - 1);	/* one less than number of buffers
									   IE, the last one in the list */
  return ( S3C2500_SUCCESS);
}
/**********************************************************************
 * Function Name:	EnableInt
 *
 *   Purpose:       Enables the Interrupt for the Dev
 *					- Interrupt enables are in 5 different registers
 *   
 *   Parameters:    Device
 ***********************************************************************/
static _INLINE_ void 
//static void 
EnableInt( UINT32 chan)
{
  CSR * baseAddr = MacBases[ chan ];

  // Street proofing
  if (NET_DEBUG > 4)
	{
	  if (chan != 0 && chan != 1)
		printk ("SC32500: EnableInt - BAD CHANNEL [%d] specified !! \n",
				(UINT16) chan );
	}
  // temp disable (is this needed??)
	// disable all temporarily
  WriteCSR( TxIntEn, ReadCSR( TxIntEn ) & ~ TxINTMask );
  WriteCSR( RxIntEn, ReadCSR( RxIntEn ) & ~ RxINTMask );

  /* Enable Interrupts */
  /* first logic level */
  enable_irq( IRQNumRx[ chan ]);
  enable_irq( IRQNumTx[ chan ]);
  /* then from the hardware */
  s3c2500_unmask_irq( IRQNumRx[ chan ]);;
  s3c2500_unmask_irq( IRQNumTx[ chan ]);

  /* Enable MAC and BDMA interrupts - ones we want to use */
  WriteCSR( TxIntEn, ReadCSR( TxIntEn ) | TxINTMask );
  WriteCSR( RxIntEn, ReadCSR( RxIntEn ) | RxINTMask );
}
/**********************************************************************
 * Function Name:	DisableInt
 *
 *   Purpose:   Disables the Interrupt for the specified Dev
 *   
 *   Parameters:    Device
 ***********************************************************************/
static _INLINE_ void 
DisableInt( UINT32 chan )
{
  CSR * baseAddr = MacBases[ chan ];

  // Street proofing
  if ( NET_DEBUG > 4 )
	{
	  if (chan != 0 && chan != 1)
		printk ("SC32500: DisableInt - BAD CHANNEL specified !! \n");
	}
	// disable all
  WriteCSR( TxIntEn, ReadCSR( TxIntEn ) & ~ TxINTMask );
  WriteCSR( RxIntEn, ReadCSR( RxIntEn ) & ~ RxINTMask );

  /* Disable Interrupts */
  /* first logic level */
  disable_irq( IRQNumRx[ chan ]);
  disable_irq( IRQNumTx[ chan ]);
  /* then from the hardware */
  s3c2500_mask_irq( IRQNumRx[ chan ]);;
  s3c2500_mask_irq( IRQNumTx[ chan ]);

}
/**********************************************************************
 * Function Name:	EnableTxRx
 *
 *   Purpose:   Starts transmission and reception.
 *   
 *   Parameters:    Ethernet channel
 ***********************************************************************/
static _INLINE_ void 
EnableTxRx( UINT32 chan )
{
  CSR * baseAddr = MacBases[ chan ];
  // Tx Enable AND not Tx Halt
  /* Enable Transmission */
  WriteCSR( TxCtl, (( ReadCSR( TxCtl)) | BTxEN));

  WriteCSR( MacTxCtl, (( ReadCSR( MacTxCtl)) | MTxEn) & ~MTxHalt);

  /* Enable Reception    */
  WriteCSR( RxCtl, (( ReadCSR( RxCtl)) | BRxEN));
  // rx Enable AND not Rx Halt
  WriteCSR( MacRxCtl, (( ReadCSR( MacRxCtl)) | MRxEn) & ~MRxHalt);

  /* Turn off the Rx/Tx halts */
  WriteCSR( MacCtl, (( ReadCSR( MacCtl)) & ~(MHaltReq | MHaltImm)));
}
/**********************************************************************
 * Function Name:	DisableTxRx
 *
 *   Purpose:   Stops transmission and reception.
 *   
 *   Parameters:    Ethernet channel
 ***********************************************************************/
static _INLINE_ void 
DisableTxRx( UINT32 chan)
{
  CSR * baseAddr = MacBases[ chan ];

  /* Turn on both the Rx/Tx halts - immediate and request */
  WriteCSR( MacCtl, (( ReadCSR( MacCtl)) | MHaltReq | MHaltImm));

  /* Disable Transmission - disable and request halts (redundant??) */
  WriteCSR( TxCtl, (( ReadCSR( TxCtl)) & ~BTxEN));
  WriteCSR( MacTxCtl, (( ReadCSR( MacTxCtl)) | MTxHalt) & ~MTxEn);

  /* Disable Reception - disable and request halts (redundant??) */
  WriteCSR( RxCtl, (( ReadCSR( RxCtl)) & ~BRxEN));
  // rx Enable AND not Rx Halt
  WriteCSR( MacRxCtl, (( ReadCSR( MacRxCtl)) | MRxHalt) & ~MRxEn);
}
/*************************************************************************
 * Function Name:	Reset
 *
 * Purpose:	To reset the MAC.
 *
 * Return Value:    S3C2500_FAIL on failure
 *                  S3C2500_SUCCESS on success
 **************************************************************************/
static int
Reset( UINT32 chan)
{
  UINT32 	i = 0;
  CSR * 	baseAddr = MacBases[ chan ];

  // Stop all tx / rx
  //WriteCSR( MacCtl, (( ReadCSR( MacCtl)) | MCtlHaltImm));
  DisableTxRx( chan);
  s3c_mdelay( PHY10MSDELAY);	// 10 mS
  // reset the BDMA TX block
  WriteCSR( TxCtl, BTxRS);
  s3c_mdelay( PHY10MSDELAY);	// 10 mS
  // reset the BDMA RX block
  WriteCSR( RxCtl, BRxRS);
  s3c_mdelay( PHY10MSDELAY);	// 10 mS

  // Software reset of all MAC control and status register 
  //   and MAC state machine
  WriteCSR( MacCtl, MReset);
  // bit is cleared when done...wait
  while (MReset & ReadCSR( MacCtl))
  {
	if (i++ >  MCtlReset_BUSY_LOOP)
	  break;
	s3c_mdelay( PHY10MSDELAY);	// 10 mS
  }
  if ( NET_DEBUG > 1 )
	{
	  if (i >= MCtlReset_BUSY_LOOP)
		printk( "SC3: MAC did not come out of RESET :-( \n");
	}
  // setup the BDMA rx Word Alignment (must be done directly after reset)
  WriteCSR( RxCtl, BRxSetup_RESET);

  return ( S3C2500_SUCCESS);
}
/*************************************************************************
 * Function Name:	SetupRegistersTX
 *
 * Purpose:	To setup the transmit register values
 *		NOTE: We assume that only the BRxCtl register has any which needs
 *			  to be preserved.
 **************************************************************************/
//static _INLINE_ void 
static void 
SetupRegistersTx( ND dev )
{
  //  S3C2500_MAC * priv = dev->priv;
  CSR * 	baseAddr = (CSR *) dev->base_addr;

  WriteCSR( TxCnt, TX_NUM_FD);		// The number of Tx FDs in the above arrary
  WriteCSR( TxCtl, BTxSetup);		// Buffered DMA transmit control reg

  WriteCSR( MacTxCtl, MTxCtlSetup);	// Transmit Control
  return;
}
/*************************************************************************
 * Function Name:	SetupRegistersRX
 *
 * Purpose:	To setup the receive register values
 * 			NOTE: if a register has both RX and TX it's in here
 **************************************************************************/
//static _INLINE_ void 
static void 
SetupRegistersRx( ND dev )
{
  //  S3C2500_MAC * np = dev->priv;
  CSR * 	baseAddr = (CSR *) dev->base_addr;
  UINT32	i;

  // BDMA rx setup
  WriteCSR( RxCtl,BRxSetup_RESET);	// Buffered DMA receive control reg

  // Number of buffer descriptors
  WriteCSR( RxCnt, RX_NUM_FD);		// Number of buffer descriptors
  // setup maximum Rx frame buffer sizes ...FIXME ...MaTed--- CHECK this
  WriteCSR( RxSz, MAX_PKT_SIZE | ((PKT_BUF_SZ) << 16));	// Receive Frame 
                                    // maximum size
  for (i = 0; i < 32; i++)
  {
	WriteCSR( Cam[i], 0xffffffff);	// CAM Content, set for broadcast
  }
  WriteCSR( CamCtl, CAMCtlSETUP);	// CAM Control - Accept anything
#if DEBUG_CRC_OFF
  WriteCSR( MacRxCtl, MRxEn | MIgnoreCRC);	// Rx Control - NO CRC Check
#else
  WriteCSR( MacRxCtl, MRxCtlSETUP);	// Rx Control
#endif
  WriteCSR( MacCtl, MCtlSETUP);		// Full Duplex 100 (might change later)

  WriteCSR( CamEn, 0x04);			// CAM Enable, third entry

  return;
}
/*************************************************************************
 *	Function Name:		TxRxInit
 *
 *   Purpose:	Set up the Ethernet to receive and transmit data.
 *
 *			NOTE: Check the order that things happen
 *				It needs to be checked (too many routines) FIXME ...MaTed---
 *
 *   Return Value:  S3C2500_FAIL on failure
 *                  S3C2500_SUCCESS on success
 *					- Always Succeeds
 **************************************************************************/
static _INLINE_ ULONG 
TxRxInit( ND dev)
{
  S3C2500_MAC * np = dev->priv;
  CSR * 	baseAddr = (CSR *) dev->base_addr;
  UINT32	chan = np->channel;

  SetupRegistersTx( dev );
  SetupRegistersRx( dev );
  DisableTxRx( chan );
  DisableInt( chan );
  // ...MaTed--- changed to always bring up the interface
  PhyInit( dev);
  /* Make sure the MAC knows the speed and duplex that has been set up */
  /* ** NOTE: we can remove all the other places we have set up duplex 
	 and speed */
  if (np->phyDpxNeg == 0)	// set to negotiated speed, not default
  {	// forcing HALF duplex, default is FULL
	WriteCSR( MacCtl, MCtlSETUP & ~MFullDup);
  }
  return (S3C2500_SUCCESS);
}


static int
SetMacAddr( ND dev, void * addrin)
{
  struct sockaddr *addr = addrin;

  if (netif_running(dev))
    return -EBUSY;

  memcpy( dev->dev_addr, addr->sa_data, dev->addr_len);

  return SetUpARC(dev, addrin);
}
/*************************************************************************
 * Function Name:		SetUpARC
 *
 * Purpose:	Setup the ARC memory MAC HW address...
 *			- NOTE Only sets up one entry - 3rd entry
 *					- 0 and 1 and 18 are reserved for MAC Control packets
 *
 * Parameters:	basePort    - base address of the MAC register set
 *				addr
 *			    - pointer to array of 6 bytes for the MAC HW 
 *				address
 **************************************************************************/
static int
SetUpARC( ND dev, void * addrin)
{
  //  S3C2500_MAC * np = dev->priv;
  CSR * 	baseAddr = (CSR *) dev->base_addr;
  UINT32	save;
  UCHAR	   * addr = (UCHAR *) addrin;

  /* NOTE: This routine assumes little endian setup of UINT32 */

  /* don't update while active */
  save = ReadCSR( CamEn);		// we leave other bits alone (later)
  WriteCSR( CamEn, 0x00000000);	// don't use any ARC entries
  /* Enter Address at third entry - hard code */
  WriteCSR( Cam[3], addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24 );
  WriteCSR( Cam[4], (ReadCSR( Cam[4]) & 0xffff0000) | addr[4] | addr[5] << 8);

  /* Enable Third Address - b2 */
  save |= 0x8;
  WriteCSR( CamEn, save);
  /* Enable ARC Compare +BCAST */
  //  WriteCSR( basePort, ARCCtl, (ARC_CompEn | ARC_BroadAcc | ARC_StationAcc));
  // Make it accept anything within dest
  WriteCSR( CamCtl, MCompEn | MBroad | MGroup | MStation);
  return ( 0);
}
#ifdef ENABLE_IOCTL	
....NOTE this section not coded  ...MaTed---
// Still unsure how IOCTLs coordinate with other calls
/*************************************************************************
 * Function Name:	SetSpeed100
 *
 * Purpose:	To Set the MAC in 100 mode.
 *	NOTE: The actual speed is determined by the PHY
 *	that is connected to this MAC.
 *	NOTE: this routine is not used - for later implemtation of IOCTL
 *        ** routine needs check and FIXME ** ...MaTed---
 *		  ** needs to talk to the PHY
 **************************************************************************/
static _INLINE_ void 
SetSpeed100( void)
{
  WriteCSR( MacCtl, (( ReadCSR(MacCtl)) & ~MLOOP10) );

  // Need to reprogram the PHY as well
  /* Should we wait for the MLLINK10 bit to be low ??? */
}
/*************************************************************************
 * Function Name:	SetSpeed10
 *
 * Purpose:	To Set the MAC in 10 MBPS mode.
 *	NOTE: The speed of the PHY should match this speed.
 *	NOTE: this routine is not used - for later implemtation of IOCTL
 *        ** routine needs check and FIXME ** ...MaTed---
 *		  ** needs to talk to the PHY
 **************************************************************************/
static _INLINE_ void 
SetSpeed10( void)
{
  WriteCSR( MacCtl, (( ReadCSR( MacCtl)) | MLOOP10 ));

  // Need to reprogram the PHY as well
	/* Should we wait for the MLINK10 bit to be high ???  */
}
#endif	// ENABLE_IOCTL
/************************************************************************
 * The following routines do the work
 ************************************************************************/
/************************************************************************
 * Check for a network adaptor of this type, and return '0' iff one exists.
 *  - we just allocate them the number of times I'm called
 *  NOTE: we ignore the following values for base_addr ...MaTed
 * If dev->base_addr == 0, probe all likely locations.
 * If dev->base_addr == 1, always return failure.
 * If dev->base_addr == 2, allocate space for the device and return success
 * (detachable devices only).
 ************************************************************************/
int __init 
s3c2500_probe( ND dev)
{
  // check if all allocated
  if (numS3c2500Macs >= ETH_MAXUNITS)
	return (-ENODEV);
  // SET_MODULE_OWNER( dev);
  dev->irq = IRQNumTx[ numS3c2500Macs ];	// just put in tx (rx is plus 1)
  dev->base_addr = (int) MacBases[ numS3c2500Macs ];// just put start base addr
  //  ((S3C2500_MAC *) dev->priv)->channel = numS3c2500Macs;
  if ( check_mem_region( dev->base_addr, MAC_IO_EXTENT))
    return (-ENODEV);
  if ( s3c2500_probe1( dev, numS3c2500Macs) == S3C2500_SUCCESS)
  {
	numS3c2500Macs++;		   		// increment the number of s3c2500s found
    return (S3C2500_SUCCESS);
  }
  return (-ENODEV);
}
/************************************************************************
 * This routine initializes all the vars in dev and private areas
 ************************************************************************/
static int
s3c2500_InitVars( ND dev, UINT32 chan)
{
  /* Initialize the device structure. */
  if ( dev->priv == NULL)
  {
    dev->priv = kmalloc( sizeof( S3C2500_MAC), GFP_KERNEL);
    if ( dev->priv == NULL)
	{
      return ( -ENOMEM);
	}
	s3c2500InitPriv( dev, chan);	// initialize the Mac Private structure
    spin_lock_init( &((S3C2500_MAC *) dev->priv)->txLock);
    spin_lock_init( &((S3C2500_MAC *) dev->priv)->rxLock);
  }
  dev->open				= &s3c2500_open;
  dev->stop				= &s3c2500_close;
  dev->hard_start_xmit	= &s3c2500_send_packet;
  dev->get_stats		= &s3c2500_get_stats;
  dev->set_multicast_list = &set_multicast_list;

  dev->tx_timeout		= &s3c2500_tx_timeout;
  dev->watchdog_timeo	= MY_TX_TIMEOUT; 
  dev->set_mac_address	= &SetMacAddr;
  /*
   * Fill in the rest of the fields of the device structure 
   * with ethernet values. 
   */
#if	ENABLE_DESTRUCTOR 
  //... Destructors weren't always called - memory leaks in on...MaTed---
  priv->destructor 	= &s3c2500Destructor;
#endif
  /*
   * The following can be implemented but hasn't been
   */
  //  dev->do_ioctl		= &s3c2500_do_ioctl;
  return ( S3C2500_SUCCESS);
}
/************************************************************************
 * This is the real probe routine. Linux has a history of friendly device
 * probes on the ISA bus. A good device probes avoids doing writes, and
 * verifies that the correct device exists and functions.
 * -- This version is hard coded to just find only the hard coded addresses
 *		It will only be called NUM_HW_ADDR times.
 * NOTE: now gets HW address from external routine. If there is no
 *  bootloader, then this routine must still return a HW address ...MaTed ---
 ************************************************************************/
// #define MATED_SKIPMAC 1	// external routine NOT available for MAC address
static int __init 
s3c2500_probe1( ND dev, UINT32 chan)
{
  int 	    		i;
  static unsigned 	version_printed = 0;
  char *			ptr;
  int 				rc;		//return code

  //  void * 			tptr;			/* temporary pointer */

#ifdef MATED_SKIPMAC
  char		dummyMacAddr[6] = {0,2,4,8,16,32};
#endif

  if ( version_printed++ == 0)
  {
	/* This section just gets called once, that's why the code doesn't
	   seem like it belongs here */
	// Forcing ethernet h/w to reset, just so soft reset doesn't fail
	Reset( 0 );
	Reset( 1 );

	//	if (NET_DEBUG > 2)
	if (CONFIG_ETH_DMA_PRIORITY)	/* change the priority of the 
									   DMA channels */
	{
#if CONFIG_DMA_PRIORITY_ROBIN 	// force the DMA priority type
	  printk("S3C2500: DMA arbitration set to round-robin:\n");
	  /* Forcing DMA arbitration to round-robin */
	  *(volatile unsigned int *)SYSCFG &= 0x3ffffffe; 
#else
	  printk("S3C2500: DMA arbitration set to fixed priority:\n");
	  /* Forcing DMA arbitration to fixed priority round-robin */
	  *(volatile unsigned int *)SYSCFG |= 0x00000001; 
#endif
	  printk("  * HPRIR-0x%lx and HPRIF-0x%lx changed to ",
			 *((UINT32 *) HPRIR), *((UINT32 *) HPRIF));
	
	  /*****************************************************
	   * test code to up DMA priority of GMDA, eth0 and eth1
	   * 0x110fe0 gives (37 total - 133 Mhz bus))
	   * HDLCC 2/37 of 1/2 the bus
	   * HDLCB 2/37 of 1/2 the bus
	   * HDLCA 1/37 of 1/2 the bus
	   * eth0 (I reverse it)
	   *       16/37 of 1/2 the bus
	   * eth1 (reversed in software)
	   *       15/37 of the bus
	   * GDMA  1/37 of 1/2 the bus
       ** but because GDMA is not used, there is no card for HDLCA
	   ** HDLCC gets 2, HDLCB - 3, eth0 - 16 and eth1 - 16
	   ** (3.6, 5.4, 9.8, and 9.8 Mhz)
	   ******************************************************/

	  *((UINT32 *) HPRIR) = 0x110fe0;

	  /*********************************************************
	   * give Controller 0 (which is eth1 -WAN) highest priority
	   * then eth0, HDLCB, HDLCC, GDMA, and HDLCA
	   *********************************************************/

	  *((UINT32 *) HPRIF) = 0x305421;

	  printk("HPRIF-0x%lx and HPRIR-0x%lx \n ", 
			 *((UINT32 *) HPRIR), *((UINT32 *) HPRIF));
	}

	/*** This section rewritten - with new page_alloc2() modification ***/ 
#if !defined(CONFIG_UNCACHED_MEM)
#error	"**** ERROR: this version of the driver requires CONFIG_UNCACHED_MEM;"
#endif
	// request uncache memory
	
	if (NULL == (FDescUncached = kmalloc( sizeof(FDAArea), GFP_DMA)))
	  return( S3C2500_FAIL);

	if (NET_DEBUG )
	{
	  printk( KERN_INFO "%s", version);
	  if (NET_DEBUG > 3)
	  {
		printk( KERN_INFO 
				"S3C2500: Buffer Address = [0x%p]\n", 
				FDescUncached);
	  }
	}
  }

  printk( KERN_INFO "%s: %s found at %08lx, ", dev->name, MacName[ chan ], 
		  dev->base_addr);
  /* Fill in the 'dev' fields. */
#ifdef MATED_SKIPMAC
  ptr = dummyMacAddr;
#else
  ptr = get_MAC_address( MacHWAddr[ chan ]);
#endif

  memcpy( dev->dev_addr, ptr, NUM_HW_ADDR);
  for ( i = 0; i < NUM_HW_ADDR; i++)
  {
    printk("%02x.", dev->dev_addr[ i]);
  }
  printk( "\n");
  dev->addr_len = NUM_HW_ADDR;
  if ( s3c2500_request_irq_mem( dev, chan))
  {
	return ( -EAGAIN);
  }
  Reset( chan );			// Force initial conditions
  DisableInt( chan );
  ether_setup( dev);
  DisableInt( chan);
  SetupRegistersTx( dev );
  SetupRegistersRx( dev );
  DisableTxRx( chan );
  DisableInt( chan );
  //  return( s3c2500_InitVars( dev));
  if (S3C2500_SUCCESS != (rc = s3c2500_InitVars( dev, chan)))
	{
	  return( rc);
	}
  // might be a bit redundant with the initial probe -- But ...
  // Zero out the stats
  memset( &((S3C2500_MAC *) dev->priv)->stats, 0, 
		  sizeof( struct net_device_stats ));
  return( S3C2500_SUCCESS );
}
/*******************************************
 * Blocks the multiple requests to set IRQ
 *******************************************/
static int	run_request = 0;
/**********************************************************************
 * s3c2500_request_irq_mem - routine to set the interrupt vectors and mem
 **********************************************************************/
static _INLINE_ int
s3c2500_request_irq_mem( ND dev, UINT32 chan)
{
  int		 	rc;

  if (run_request++ < numS3c2500Macs)
	{
	  return (S3C2500_SUCCESS);
	}
  /*
   * The ethernet MACS have fixed interrupts, allocate the interrupt
   * vector now. There is no point in waiting since no other device
   * can use the interrupt, and this marks the irq as busy. Jumpered
   * interrupts are typically not reported by the boards. 
   *
   * The S3C2500 allocates 2 interrupts for each of the two MACs
   */

  if ( (rc = request_irq( IRQNumTx[ chan ], &s3c2500_Tx, SA_INTERRUPT,
						 IntNames[ 2 * chan ], dev)))
  {
    printk( KERN_WARNING "%s: unable to get IRQ %d (irqval=%d).\n",
	    dev->name, (UINT16) IRQNumTx[ chan ], (UINT16) rc);
    return ( -EAGAIN);
  }
  if ( (rc = request_irq( IRQNumRx[ chan ], &s3c2500_Rx, SA_INTERRUPT,
						  IntNames [ 1 + 2 * chan ], dev)))
  {
    printk( KERN_WARNING "%s: unable to get IRQ %d (irqval=%d).\n",
	    dev->name, (UINT16) IRQNumRx[ chan ], (UINT16) rc);
	free_irq( IRQNumTx[ chan ], dev);	// free the one we got
    return ( -EAGAIN);
  }
  /* Grab the region so that no one else tries to probe our ioports. */
  if (! request_mem_region( dev->base_addr, sizeof(CSR),
							dev->name))
  {
    printk( KERN_WARNING "%s: unable to get memory/io address region %lx\n",
	    dev->name, dev->base_addr);
	free_irq( IRQNumTx[ chan ], dev);
	free_irq( IRQNumRx[ chan ], dev);
    return (-EAGAIN);
  }
  run_request = 1;
  return (rc);
}
/**********************************************************************
 * s3c2500_release_irq_mem - routine to release the interrupt vectors and mem
 **********************************************************************/
static _INLINE_ int
s3c2500_release_irq_mem( ND dev)
{
  UINT32		chan = ((S3C2500_MAC *) dev->priv)->channel;

  if (run_request < numS3c2500Macs)	/* did we allocate them yet */
	{
	  return( S3C2500_SUCCESS);
	}
  /* release all the interrupts */
	free_irq( IRQNumTx[ chan ], dev);
	free_irq( IRQNumRx[ chan ], dev);

  /* Release the region so that some else (including ourselves) can have it */
  release_mem_region( dev->base_addr, sizeof(CSR) );

  run_request--;	/* someone else can use it now */
  return ( S3C2500_SUCCESS);
}
/***********************************************************************
 * log transmit MAC errors - status register is
 * *** NOT *** the same as the
 *		status in the FD block - juse the FD.status (multiple packets)
 * ** Coded for the FD block (txtimeout has to be modified)
 *	returns S3C2500_FAIL if error
 *			S3C2500_SUCCESS if status did not show error
 ***********************************************************************/
static _INLINE_ int 
 s3c2500LogTxMACError( ND dev, int status)
{
  S3C2500_MAC * priv = dev->priv;
  CSR *			baseAddr = (CSR *) (dev->base_addr);
  UINT32		RegStat = ReadCSR( MacTxStat);

  if (status & TxS_Coll)
  {
	priv->stats.collisions ++;	// we got at least one collision
  }
  if ( RegStat & MCollCnt)
  {
	priv->stats.collisions += ( RegStat & MCollCnt) >> 8;	// add collisions
  }
#if 0
  // If the transmission is complete, we ignore FIFO underrun
  if (status & TxS_Comp)
	status &= ~TxS_Under;
#endif
  if (0 == (status 
			& (TxS_ExColl + TxS_LateColl + TxS_Defer + TxS_Paused
			   + TxS_MaxDefer + TxS_Halted + TxS_NCarr + TxS_Par 
			   + TxS_Under + TxS_SQErr)))
  {
	return ( S3C2500_SUCCESS);
  }
  priv->stats.tx_errors++;				// we have an error
  // now log specific error
  if ( status & (TxS_ExColl))
  {
	priv->stats.collisions += 16;	// at least 16 collisions occurred
  }
  if ( status & (TxS_MaxDefer + TxS_Paused + TxS_Halted + TxS_ExColl 
				 + TxS_Halted))
    priv->stats.tx_aborted_errors++;
  if ( status & (TxS_LateColl))
    priv->stats.tx_window_errors++;
  if ( status & TxS_NCarr)
    priv->stats.tx_carrier_errors++;
  if ( status & (TxS_Par + TxS_Under))	// Underrun is here??
    priv->stats.tx_fifo_errors++;

  if (NET_DEBUG > 3 && 0)	// special output for debugging
  {
	CSR         *	baseAddr 	= (CSR *) (dev->base_addr);
	if (status & TxS_Under)
	{
	  printk(" **TxFIFO underrun- Controller Cnt-%ld First-%d Status-0x%x\n",
			 ReadCSR( TxCnt), priv->ringTx.first, status);
	}
  }

  if ( status & (TxS_SQErr + TxS_Defer))
    priv->stats.tx_heartbeat_errors++;
#if 0	// there are circumstances when the  transmitter has to be restarted
        // I haven't found one yet, but as a reminder ...MaTed---
  if ( status & (TXS__PARITY + ???))
	// restart transmitter
#endif
  return ( S3C2500_FAIL);
}
/***********************************************************************
 * log receive BDMA errors
 *	returns S3C2500_FAIL if error
 *			S3C2500_SUCCESS if status did not show error
 * - status passed should be from the F Descriptor
 ***********************************************************************/
static _INLINE_ int 
s3c2500LogRxBdmaError( ND dev, int status)
{
  S3C2500_MAC * 	priv = dev->priv;

  // Check if there were errors
  if (0 == (status 
	    & (RxS_OvMax + RxS_AlignErr + RxS_CrcErr + RxS_OFlow +
		   RxS_MUFS + RxS_RxPar + RxS_Halted )))
//	    & (RxS_OvMax + RxS_AlignErr + RxS_CrcErr +
  {
    return ( S3C2500_SUCCESS);
  }
  priv->stats.rx_errors++;
  // now log specific errors
  if ( status & ( RxS_OvMax + RxS_MUFS ))
    priv->stats.rx_length_errors++;
  if ( status & ( RxS_CrcErr ))
    priv->stats.rx_crc_errors++;
  if ( status & ( RxS_AlignErr ))
    priv->stats.rx_frame_errors++;

  if (NET_DEBUG > 3 && 0)	// special output for debugging
  {
	CSR         *	baseAddr 	= (CSR *) (dev->base_addr);
	if (status & RxS_OFlow)
	{
#if (MATED_RX_OF_BUG)		// printk the previous buffer lengths
	  {
		int i;
		int index = rx_len_index[ priv->channel];

		printk("*Rx OF(%ld)- BRXBDCNT-%ld First-%d Stat-0x%x IntStat-0x%lx\n"
			   " TxNum [%6ld] ",
			   priv->channel, ReadCSR( RxCnt), 
			   priv->ringRx.first, status, ReadCSR(RxStat),
			   priv->stats.tx_packets);
		for (i = 0; i < RX_OF_MAX_LOG; i++)
		{
		  index = (index == 0) ? RX_OF_MAX_LOG - 1 : index - 1;
		  printk("%4ld ", rx_len_array[ priv->channel][ index]);
		}
		printk("-\n");
	  }
#endif // MATED_RX_OF_BUG

#define SAMSUNG_WANTED_DEBUG 0
	  if ( SAMSUNG_WANTED_DEBUG)
	  {
		printk("*RxF OF(%ld)- BRXBDCNT-%ld BDMARXCON-0x%lx BDMATXCON-0x%lx",
			   priv->channel, ReadCSR( RxCnt), 
			   ReadCSR( RxCtl), ReadCSR( TxCtl));
		printk(" MACCON-0x%lx MACRXCON-0x%lx MACTXCON-0x%lx\n",
			   ReadCSR( MacCtl), ReadCSR( MacRxCtl), ReadCSR( MacTxCtl));
		printk(" *CAMCON-0x%lx CAMEN-0x%lx First-%d Stat-0x%x IntStat-0x%lx\n",
			   ReadCSR( CamCtl), ReadCSR( CamEn),
			   priv->ringRx.first, status, ReadCSR(RxStat));
	  }
	}
  }

  if ( status & ( RxS_OFlow + RxS_RxPar ))
    priv->stats.rx_fifo_errors++;
  if ( status & ( RxS_Halted ))	// may be other updaters to this
    priv->stats.rx_missed_errors++;
  return ( S3C2500_FAIL);
}
/***********************************************************************
 * Transmit timeout
 *	Kernel calls this routine when a transmit has timed out
 ***********************************************************************/
static void 
s3c2500_tx_timeout( ND dev)
{
  S3C2500_MAC *	priv = dev->priv;
  CSR *			baseAddr = (CSR *) (dev->base_addr);
  UINT32		BStat = ReadCSR( TxStat);
  UINT32		MacStat  = ReadCSR( MacTxStat);

  if (NET_DEBUG > 3)
	{
	  printk( KERN_ERR "%s: transmit timed out: NUM= %d, MacTXStatus=0x%08lX ",
			  dev->name, priv->ringTx.num, MacStat );
	  printk( KERN_ERR "MacBDMAStatus=0x%08lX ", BStat );
	}
  /********************************************************
   * NOTE: This section probably needs more work ...MaTed--- FIXME
   * - much of this has been fixed with the OPEN/CLOSE updates
   *   but we should add more to reset the Ethernet controller
   *   as well - NOTE: reset now occurs in CLOSE routine
  ********************************************************/
  // We are going to reset the MAC
  //  - all packets - both rx and tx may be discarded
  //  - have to change logic to allow errors to stay around

	// Can't call error routine - bits no longer the same;
	//(void) s3c2500LogTxMACError( dev, MacStatus);
	//  (void) s3c2500LogTxBDMAError( dev, BDMAStatus);
  if ( MacStat & MCollCnt)
  {
	priv->stats.collisions += ( MacStat & MCollCnt) >> 8;	// add collisions
  }
  if (0 == (BStat 
	    & (ExColl + Underflow + DeferErr + NoCarr + LateColl + TxParErr)))
  {
	priv->stats.tx_errors++;
	// now log specific error
	if ( BStat & ExColl)
	  priv->stats.collisions += 16;	// at least 16 collisions occurred
	if ( BStat & (TxParErr + Underflow))	// Underrun is here??
	  priv->stats.tx_fifo_errors++;
	if ( BStat &  DeferErr)
	  priv->stats.tx_heartbeat_errors++;
	if ( BStat & NoCarr)
	  priv->stats.tx_carrier_errors++;
	if ( BStat &  LateColl)
	  priv->stats.tx_window_errors++;
	//	if ( BStat & (TxS_MaxDefer + TxS_Paused + TxS_Halted + TxS_ExColl 
	//				   + TxS_Halted))
	//	  priv->stats.tx_aborted_errors++;
  }

  // FIXME ...MaTed--- the close  / open has not been fully tested
  s3c2500_close( dev);

  s3c2500_open( dev);
  // Kick the MAC to check if there is something to send
  // - this is unnecessary, unless we add code to only do the
  //   close/open every 2nd or third time ...MaTed
  //WriteCSR( DMACtl, ReadCSR( DMACtl) | DMA_CTL_TxWakeUp);

  /* If we have space available to accept new transmit
   * requests, wake up the queueing layer.  This would
   * be the case if the chipset_init() call above just
   * flushes out the tx queue and empties it.
   *
   * If instead, the tx queue is retained then the
   * netif_wake_queue() call should be placed in the
   * TX completion interrupt handler of the driver instead
   * of here.
   */
  if (!txFull( dev))
    netif_wake_queue( dev);
  dev->trans_start = jiffies;		// reset the start time
  return;
}
/************************************************************************
 * initialize all the data in private area
 ************************************************************************/
static _INLINE_ int 
s3c2500InitPriv( ND dev, UINT32 chan )
{
  S3C2500_MAC * np 			= dev->priv;
  CSR *			baseAddr 	= (CSR *) (dev->base_addr);
  RET_CODE	  	rc;

  memset( np, 0, sizeof( S3C2500_MAC));
  
  /* setup the MAC HW address */
  SetUpARC( dev, dev->dev_addr);
  /* setup channel number */
  ((S3C2500_MAC *) dev->priv)->channel = numS3c2500Macs;

  // ******** Enhancement FIXME-pickup duplex and speed from commandline 
  // ********  OR from Bootloader envirnment variable ...MaTed---
  // - if you want to force, userland force later
  // np->phySpeed = 2;		// Force 100
  // np->phySpeed = 0;		// automatic MII mode force speed = 10
  np->phySpeed = 3;			// automatic MII mode 
  //  np->phySpeed = 1;		// Force 10 Mb/s endec

  /* Mac CNTL Full Duplex */
  np->phyDpx = 1;			/* Full Duplex */
  //np->phyDpx = 0;			/* Half Duplex, if MII_Conn is High?? */

  /* set channel */
  np->channel = chan;

  /* set Frame Descriptors */
  np->rxFd = FDescUncached->rxFdArray[ chan ];
  np->txFd = FDescUncached->txFdArray[ chan ];

#if (CONFIG_WBUFFER)
	np->txBuff = FDescUncached->txFdBuffers[ chan ];
#endif

  // Setup in SetupTxFDs
  WriteCSR( RxPtr, np->rxFd);	/* Rx frame descriptor start 	*/
  /* Setup indices into Frame Descriptor array */
  np->ringRx.first = np->ringRx.num = 0;
  np->ringRx.max = np->ringRx.last = RX_NUM_FD-1;		// maximum value
  /* Setup the ring buffer indices */
  np->ringTx.first = np->ringTx.last = np->ringTx.num = 0;
  np->ringTx.max = TX_NUM_FD-1;

  /***  NOTE this array needs to be in a region that is non-cachable 	***/
  WriteCSR( TxPtr, np->txFd);// changed to address of fixed array
  /*
   * NOW set up the S3c2500 MAC struct
   */
  if ((rc = SetupTxFDs( dev)))
    return (rc);
  if ((rc = SetupRxFDs( dev)))
    return (rc);
  // no error
  return ( S3C2500_SUCCESS);
}
/************************************************************************
 * Open/initialize the board. This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is non-reboot way to recover if something goes wrong.
 ***********************************************************************/
static int
s3c2500_open( ND dev)
{
  S3C2500_MAC * np = (S3C2500_MAC *) dev->priv;
  //  UINT		basePort = dev->base_addr;
  RET_CODE		rc;

#if MPROFILE		// clear miniprofile
  {
	int 		i;
	profileAddrIndex = 0;	/* start at the beginning */
	for (i = 0; i < ProfileArraySize; i++)
	  {
		profileArray[ i ].code = 0;
	  }
  }
#endif
  /*
   * This is used if the interrupt line can turned off (shared).
   * - Can we share? - might as well register
   */
#if 0	// open only in probe routine - it is an embedded device ...MaTed---
  rc = s3c2500_request_irq_mem( dev, np->channel);
  TDEBUG("S3C2500_OPEN: request_irq_mem rc=%d\n", rc);
  if (rc)
	{
	  printk( KERN_ERR "S3C2500: Cannot get irq/mem rc=%d\n", rc);
	  return -EAGAIN;
	}
#endif
  // We are leaving priv intact
  //  if (S3C2500_SUCCESS != (rc = s3c2500_InitVars( dev)))
  //    return (rc);
  /* Reset the hardware here. Don't forget to set the station address. */
  if (S3C2500_SUCCESS != (rc = TxRxInit( dev)))
	{
	  printk( KERN_ERR "S3C2500: Phys init failed \n");
	  return (rc);
	}
  EnableTxRx( np->channel );
  EnableInt( np->channel );

  //  priv->open_time = jiffies;
  /* We are now ready to accept transmit requests from
   * the queueing layer of the networking.
   */
  netif_start_queue( dev);

  return S3C2500_SUCCESS;
}
/************************************************************************
 * Description:
 *	This routine is called by the upper layers send out a SKB on the device
 *		specified by dev
 * NOTES:This will only be invoked if your driver is _not_ in XOFF state.
 * What this means is that you need not check it, and that this
 * invariant will hold if you make sure that the netif_*_queue()
 * calls are done at the proper times.
 ************************************************************************/
static int s3c2500_send_packet( struct sk_buff *skb, ND dev)
{
  S3C2500_MAC 	  *	np 			= dev->priv;
  int 				baseAddr 	= dev->base_addr;
  //  short			length;
  //  UCHAR		   	  *	buf 	= skb->data;
  TxFD 			  *	fdPtr 		= np->txFd;
#if ! CONFIG_WBUFFER
  TxBUFF		  * txSkb		= np->txSkb;
#endif // ! CONFIG_WBUFFER
  RINGBUFF		  *	ringTx 		= & np->ringTx;
  UINT16			first 		= ringTx->first;
  UINT32			temp;

  enterCnt(0x300);		// ******-******* Profiling code
  // make minimum length for TX
  //  Don't need this, the controller does it for me
  //length = ETH_MIN_LEN < skb->len ? skb->len : ETH_MIN_LEN;

  /* If some error occurs while trying to transmit this
   * packet, you should return '1' from this function.
   * In such a case you _may not_ do anything to the
   * SKB, it is still owned by the network queueing
   * layer when an error is returned.  This means you
   * may not modify any SKB fields, you may not free
   * the SKB, etc.
   */

  if (NET_DEBUG > 5)
	{
	  printk( KERN_ERR "%s: Send: %3d len=%d SKB=%p\n", 
			  dev->name, (unsigned int) first, (unsigned int) skb->len, skb);
	}
  if (NET_DEBUG > 6)
	{
	  int i;
	  printk( KERN_ERR "S3C2500_SEND: Length %d\n - data=", skb->len);
	  for ( i = 0; i < skb->len; i++)
		{
		  printk("%02x ", skb->data[ i]);
		}
	  printk("\n");
	}
  if (NET_DEBUG > 3)
	{
	  ASSERT( atomic_read( &skb->users) == 1);
	}

#if (NET_DEBUG > 3 && 0) // temp code to verify full size packet transfer
                         //  for TESTING only (NET_DEBUG should be higher)
                         //  ...MaTed---
  if (skb->len != 1514)	// maximum size packet
  {
	printk(" T%1ld-%3d %4d ", np->channel, first, skb->len);
	if (++position_output > 5)
	{
	  position_output = 0;
	  printk("\n");
	}
  }
#endif

  /* This is the most common case for modern hardware.
   * The spinlock protects this code from the TX complete
   * hardware interrupt handler.  Queue flow control is
   * thus managed under this lock as well.
   */
  spin_lock_irq( &np->txLock);

  TxPktCompletion( dev );		// clear out the packets already sent

  /* Check to see if there is room */
  if (txFull( dev ))	// < to guarentee at least one spot
  {
    printk( KERN_ERR "TX overflow on %s\n", dev->name);
	// MAKE Sure transmitter was enabled -- maybe that will clear it up 
	//	WriteCSR( TxCtl, temp | BTxEN);
	//	WriteCSR( MacTxCtl, (ReadCSR( MacTxCtl) | MTxCtlTxEn) & ~MTxHalt);
	EnableTxRx( np->channel);
	spin_unlock_irq( &np->txLock);
    return ( -1);	// tell upper layer to requeue
  }
  if (NET_DEBUG > 4)
  {
	  /* another paranoid check (next should be owned by system */
	if (fdPtr[ first ].FDStat & OWNERSHIP)
	{
	  printk( KERN_ERR "TX ownership problem on %s\n", dev->name);
	  spin_unlock_irq( &np->txLock);
	  return (-1);
	}
  }

  enterCnt(0x301);		// ******-******* Profiling code

#if (CONFIG_WBUFFER)
  {
	/* 
	 * - copy the data to the transmit buffer
	 * - we skip the first two bytes of the tx buffer (just filler)
	 * because we want the copy to be word to word aligned
	 */
	memcpy( np->txBuff[ first].FDData + 2, skb->data, skb->len);
	// leave this in until we check if it's faster to copy the extra two
	//  bytes ...MaTed---
	//  memcpy( np->txBuff[ first], skb->data - 2, skb->len + 2);
  }
#else
  {
	txSkb[ first].FDskb = skb;
	fdPtr[ first ].FDDataPtr = skb->data;
  }
#endif 
  fdPtr[ first ].FDLength = skb->len;
  if (NET_DEBUG > 3)
  {
	ASSERT( atomic_read( &skb->users) == 1);
  }
  if (NET_DEBUG > 5)
  {
	if (atomic_read( &skb->users) != 1)
	{
	  printk("SKB:%08lx, users:%lx\n",
			 (UINT32) &skb, 
			 (UINT32) atomic_read( &skb->users));
	}
  }
  if (CONFIG_WBUFFER)
  {
	dev_kfree_skb_irq (skb);		// done with the skb - free it
  }

  ringTx->num ++;			// another buffer in the ring
  ringTx->first = incIndex( first ,ringTx->max);
  dev->trans_start = jiffies;
  enterCnt(0x302);		// ******-******* Profiling code
  fdPtr[ first ].FDStat = TxWidSet16 + OWNERSHIP16;	// 2 byte offset,
                                                    // clear stat
                                                    // Pass to BDMA

  // Let the MAC know there is something to send (in case it stopped )
  // ...Note the check is from example code (why?), extra comp is for compliler
  if (! ((temp = ReadCSR( TxCtl)) & BTxEN))
  {
	WriteCSR( TxCtl, temp | BTxEN);
  }
  WriteCSR( MacTxCtl, ReadCSR( MacTxCtl) | MTxEn);

#if !NO_TX_COMP_INT	// check to see if we need to turn this back on 
                        // - probably extraneous
  // turn on tx completion interrupt
  WriteCSR( TxIntEn, ReadCSR( TxIntEn ) | TxCompIE );
#endif

  enterCnt(0x303);		// ******-******* Profiling code
  /* If we just used up the very last entry in the
   * TX ring on this device, tell the queueing
   * layer to send no more.
   */
  if (txFull( dev))
  {
    netif_stop_queue( dev);
  }

  enterCnt(0x304);		// ******-******* Profiling code
#if NO_TX_COMP_INT
  TxPktCompletion( dev );
#endif

  /* 
   * When the TX completion hw interrupt arrives (or TxPktCompletion()
   * is called), most of the transmit statistics are updated.
   */
  spin_unlock_irq( &np->txLock);

  enterCnt(0x3ff);		// ******-******* Profiling code
  return 0;
}
/************************************************************************
 * Helper routine to collect stats on tx completion and release skbs
 * MUST be called with spin_lock already invoked
 ************************************************************************/
//static _INLINE_ void
static void
TxPktCompletion( ND dev)
{
  S3C2500_MAC 	  *	np 			= dev->priv;
  int 				baseAddr 	= dev->base_addr;
  TxFD 			  *	txFd 		= np->txFd;
  RINGBUFF		  *	ringTx 		= & np->ringTx;
#if ! CONFIG_WBUFFER
  TxFD 			  *	fdPtr 		= np->txFd;
  TxBUFF		  * txSkb		= np->txSkb;
#endif // ! CONFIG_WBUFFER
  UINT32			S3cTxCnt;
  UINT16			MissCnt;
  UINT16			last;

  // set the count where the Mac transmit Pointer is at - don't try to 
  //  catch the next one "going" to happen
  S3cTxCnt = ReadCSR( TxCnt);

  while ( (last = ringTx->last) != S3cTxCnt)	// It's possible that there 
	                                //  was nothing sent
  {
	/*** Only in here if MAC processed at least one packet */
    // checks to see if there was an error as well
	if (S3C2500_SUCCESS == s3c2500LogTxMACError( dev, txFd[ last ].FDStat))
	{
	  np->stats.tx_bytes += np->txFd->FDLength;	// total bytes transmitted
	  // NOTE: only good packets were transmitted (whereas rx can get bad)
	  np->stats.tx_packets++;			// total packets transmitted
	}
	else
	{ // a tx error of some kind occurred
	  // clear EMISSCNT register , by read (example code)
	  MissCnt = (UINT16) ReadCSR( MissCnt);
#if 0	// This seems wrong, might need something but not this ...MaTed---
	  SetupRegistersTx( dev );		// force Tx registers if error
#endif
	}
	/*
	 * The following stuff we have to do regardless if the packet
	 *	transmission had errors or not
	 */
	// update the number of collisions
	//  *** NOTE: if multiple packets, this is probably wrong ...MaTed---
	np->stats.collisions += (ReadCSR( MacTxStat) & MCollCnt) >> MCollShift;
	if ( ! CONFIG_WBUFFER)
	{
	  dev_kfree_skb_irq( txSkb[ last].FDskb );
	  /* This has been moved out of the optional NET_DEBUG section
	   *  because s3c2500_close routine needs to know which SKBs 
	   *  are currently still occupying locations (I know I should 
	   *  have only cleared the ones between the indices, BUT...MaTed---
	   */
	  txSkb[ last ].FDskb = NULL;
	  if (NET_DEBUG > 2)
	  {
		fdPtr[ last ].FDDataPtr = NULL;
	  }
	}
	// Clearing parts of the FD structure (probably not needed ??? )
	if (NET_DEBUG > 2)
	{
	  txFd[ last ].FDLength = 0x0000;		// turn off length
	}

	if (NET_DEBUG > 1)
	{
	  // Might have to move this outside the conditional
	  txFd[ last ].FDStat = 0x0000;		// Turn off status, keep ownership
	}
	// Move on to the next in the list
	ringTx->last = incIndex( last, ringTx->max);
	ringTx->num--;
  }
}
/************************************************************************
 * This handles TX complete events posted by the device interrupt.
 * - includes the following
 *	- transmit of a frame completed (*** Do we want this enabled??? **)
 *	- Sent all frames (stopped by owner bit)
 *		- it would be nice to use this interrupt rather than the MAC "Send"
 *		per packet, but error status and busy cause probs 
 *		- the interrupt should be disabled serves no purpose
 *		- two parts to this, we disable the interrupt (see EnableInt )
 *		but enable the stop (no skip frame)
 *	- Interrupt on control packet after MAC sends
 *		- we don't currently send control packets, so this int is never
 *		envoked.
 *		- we will leave it enabled though
 *	- Tx Buffer Empty
 *		- leaving this off too
 ************************************************************************/
void s3c2500_Tx( int irq, void *dev_id, struct pt_regs * regs)
{
  ND 			dev 		= dev_id;
  CSR 		  *	baseAddr 	= (CSR *) (dev->base_addr);
  S3C2500_MAC *	np 			= dev->priv;
  UINT32		status;

  enterCntVal(0x100, ReadCSR( TxStat));		// ******-******* Profiling code
  spin_lock( &np->txLock);
  status = ReadCSR( TxStat);	// save interrupting status to clear

#if ! NO_TX_COMP_INT	// do we have packet completion interrrupt turned off?
  TxPktCompletion( dev);
#endif

  // checks to see if there was an additional (MAC/BDMA internal) error as well
#if 0
  if ( status & BTxEmpty )
  {
	printk( "%s: Invalid interrupt! Transmit Buffer Empty \n", dev->name);
  } else if (status & BTxNO )
#else
  if (status & BTxNO )
#endif
  {
#if 0	// occurs end of every transmit - can optimize around this? ...MaTed---
	if (NET_DEBUG > 4)
	  {
		printk( "%s: Shutting Tx Down - No owner for TX buffer [0x%08x]\n",
				dev->name, ((int) ReadCSR( TxPtr)));
	  }
#endif
#if 0	// the transmit Enable is automatically cleared on Not Owner
	// Shut down the transmitter - nothing to send
	WriteCSR( TxCtl, (( ReadCSR( TxCtl)) & ~BTxEN));
#endif
	// Disable the NoOwnership Interrupt
	WriteCSR( TxIntEn, ReadCSR( TxIntEn ) & ~BTxNOIE );
  } else if ((NET_DEBUG > 3) && status & TxCFcomp )
	{	/* we currently aren't supporting control packets */
	  printk( "%s: Invalid interrupt! Control Packet Sent \n", dev->name);
	}
  // Now we clear the interrupts
  WriteCSR( TxStat, ReadCSR(TxStat) & (~(status & TxINTMask)));
  // done with critical section
  spin_unlock( &np->txLock);
  enterCnt(0x1FF);		// ******-******* Profiling code
}
/************************************************************************
 *NOTE this needs updating ...MaTed---
 * Rx interrupt - Frame received (others ??)
 * We have a good packet(s), get it/them out of the buffers.
 * - we take both BL and FDA from the MAC.first
 *   - move them to the front.last
 *	 - buffer is stripped off, added to skb head, skb address stored in ??
 *	- continue while MAC.first != MAC.last
 * 	- other things to check / do
 *		- ensure all four entries are set to SYS_OWNS_xx
 * Additional code - if the next position is owned by the CPU,
 *	we will drop the current packet, and stay in the same place
 ************************************************************************/
static void
s3c2500_Rx( int irq, void *dev_id, struct pt_regs * regs)
{
  ND			dev 		= dev_id;
  S3C2500_MAC *	np	 		= (S3C2500_MAC *)dev->priv;
  CSR         *	baseAddr 	= (CSR *) (dev->base_addr);
  RINGBUFF    *	ringRx		= & np->ringRx;
  RxFD		  *	rxFd		= np->rxFd;
  UINT32		bMacRxStat 	= 0;		// MAC Rx Status interrupt reset bits
  UINT32 		bRxStat		= 0;		// BDMA and frame errors interrupt 
                                        //  reset bits
  UINT16	first;			// first received buffer went here
  UINT16	last;			// the last buffer is the spacer - no length
                            //  and owned by CPU
//  UINT32	i;				// we comment this so no one freaks about it
                            //  loop counter
//  UINT16	next;			// next buffer to be used
  UINT32	temp;
  UINT16	MacRxCnt;		// H/W Receive Index
  UINT32	MacRxStat;		// MAC Rx Status - special errors
                            // - only check 8, 11
  UINT32 	RxStat;			// BDMA and frame errors
                            // - only look at bit 0, 17, 18
                            // - bits 1-5 take from frame desc status
                            // - 16 is the interrupt and must be cleared
                            // - 19, 20 and 21 we ignore for now
  UINT16	MissCnt;		// missed error count temp hold
  UINT32	status;			// Frame Status
  UINT16	len;			// Frame Length
#if !RX_ALL_PKTS
  int 		boguscount = MAXINT_WORK_RX;	// maximum number of 
#endif
  struct sk_buff *	skb;	// so we can access skb structures

  enterCnt(0x200);		// ******-******* Profiling code
  /*
   * This protects us from concurrent execution of
   * our dev->hard_start_xmit function above.
   */
  spin_lock( &np->rxLock);

  // get the two status registers - we don't check all bits because 
  //  we haven't enabled them - if you enable other rx features, add the check
  // - Have made assumption that things like BMRXSTAT.BRxMSO cannot occur
  //   without BMRXINTEN.BRxMSOIE being sset
  RxStat = ReadCSR( RxStat);
  MacRxStat = ReadCSR( MacRxStat);

  // checks to see if there was an error as well
  if (RxStat & BRxNO )
  {
	bRxStat |= BRxNO;			// set bit for clear interrupts below

	// get continuous interrupt ...MaTed---

	// we assume that the later release of the buffer will fix things
	// BDMA rx setup
	// Is this enough to recover ...MaTed---
    //  - we are willing to toss any packets we 
    //  - currently have, but there is more needed
	/* FIXME: there must be something to do HERE ...MaTed--- */

	if (NET_DEBUG > 3)
	{
	  printk( "%s: Interrupt! Not owner of RX buffer \n", dev->name);
	}
	if (NET_DEBUG > 4)
    {	// show me the internals - did we run out of space?
	  printk("  first-%d last-%d MacRxCnt-%ld max-%d\n",
			  ringRx->first, ringRx->last, ReadCSR( RxCnt), ringRx->max);
	  for(temp=0; temp <ringRx->max; temp +=4)
	  {
		printk("  %3ld Len-%d Stat-0x%x Len-%d Stat-0x%x Len-%d Stat-0x%x"
			   " Len-%d Stat-0x%x\n ",
			   temp,
			   rxFd[ temp ].FDLength, rxFd[ temp ].FDStat,
			   rxFd[ temp + 1].FDLength, rxFd[ temp + 1].FDStat,
			   rxFd[ temp + 2].FDLength, rxFd[ temp + 2].FDStat,
			   rxFd[ temp + 3].FDLength, rxFd[ temp + 3].FDStat);
	  }
	}
  }
  if (RxStat & MissRoll )
  {
	bRxStat |= MissRoll;		// set bit for clear interrupts at end
	MissCnt = (UINT16) ReadCSR( MissCnt);	// clears interrupting condition as
	                                        //  well
	if (NET_DEBUG > 1)
	  {
		printk( "%s: Interrupt! Missed Error Count rolled over - %d\n", 
				dev->name, MissCnt);
	  }
  }

  // *** I don't think I need this unless MACRXCON.MShortEn is set
  // ***  but I leave it in until I get an answer back from Samsung ...MaTed---
  if (MacRxStat & MRxShort )
  {
	bMacRxStat |= MRxShort;		// set bit for clear interrupts at end
	// continue with processing
	if (NET_DEBUG > 3 )
	{
	  printk( "%s: Short Rx Frame - discarded MacRxStat = %0lx, RxStat " \
			  "= %0lx\n",
			  dev->name, MacRxStat, RxStat);
	}
  }
#if 0 // Code not needed if we keep the spacing (for full condition 1 blank)
  /*******************************************
   * New code to anticipate upcoming possible 
   * Not Owned interrupt. We will maintain two 
   * empty FDs (owned by BDMA) in order to not
   * get the interrupt. This means that if we 
   * cannot keep the buffer spacing, we drop
   * the oldest buffers in the list (not ideal,
   * but it should work. The two FD following
   * MacRxCnt will be dropped if they are occupied
   *******************************************/
#define MAC_RX_CNT_EMPTY	2	// number of entries to keep empty
  for (i = 0; i < MAC_RX_CNT_EMPTY; i++)
  {
	temp = incIndex (MacRxCnt, ringRx->max);
	if (ringRx->first == temp && 
		(0 == (rxFd[ temp ].FDStat & OWNERSHIP16 )))
    {
	  // give the buffer to BDMA and drop the packet
	  rxFd[ temp ].FDLength = PKT_BUF_SZ;
	  rxFd[ temp ].FDStat = OWNERSHIP16;
	  // we gave it back, advance
	  ringRx->first = incIndex(ringRx->first, ringRx->max);
	  // update stats to indicate we tossed the packet
	  np->stats.rx_missed_errors ++;
	  np->stats.rx_errors ++;
	}
  }
#endif // 0

  // reset all the interrupting sources, clear to get next receive
  //  the ones that were set are cleared 
  // is there a problem with setting ones which were not set ???
  // ** this is how the DIAG code does it (but it's not correct)
  bRxStat |= BRxEarly;	// this I never use but seems to be on, so clear anyway
  bRxStat |= BRxDone;	/* setup to clear the Done interrupt (compiler should
						 * stick these two together ...MaTed---)
						 */
#if 0
  printk("----bRxStat=0x%lx RxStat=0x%lx bMacRxStat=0x%lx MacRxStat=0x%lx\n",
		 bRxStat, ReadCSR( RxStat),
		 bMacRxStat, ReadCSR( MacRxStat));
#endif

  WriteCSR( RxStat, RxStat & ~bRxStat);
  WriteCSR( MacRxStat, bMacRxStat);

  MacRxCnt = ReadCSR( RxCnt);	/* Count that the controller is using 
								 *  - Just handle ones we have the interrupt
								 *	for. We could reloop and check, but we'd 
								 *  still have the interrupt to handle
								 *	so ...MaTed---
								 */
#if !RX_ALL_PKTS
  // boguscount makes sure we don't spend too much time in ISR
  while (MacRxCnt != ringRx->first && boguscount-- > 0)
#else
  while (MacRxCnt != ringRx->first)
#endif
  {
	enterCnt(0x201);		// ******-******* Profiling code
	first = ringRx->first;	// we make a copy because it's faster
	                        //  - should change to pointers
	// NOW we deal with the FD stuff
    status = rxFd[ first ].FDStat;
    len = rxFd[ first ].FDLength;

	if (len == 0)
	{ /* 
	   *   This happens when the MAC determines there is an error
	   * Since most of the time something else has caused the MAC to
	   * set the status. Just need to make sure we do NOT let the
	   * packet get through with an error.
	   *	NOTE the error check must match the one in s3c2500LogRdmaError
	   */

	  if (NET_DEBUG > 4)
	  {
		printk( "%s: rx'd ZERO length packet Status=%0lx... Skipped\n",
				dev->name, status);
	  }

	  // check if other errors set
	  // If No error is previously set, set as CrcErr
	  if (0 == (status 
			  & (RxS_OvMax + RxS_AlignErr + RxS_CrcErr + RxS_OFlow +
				 RxS_MUFS + RxS_RxPar + RxS_Halted )))
	  {
		  status |= RxS_CrcErr;	// forces CRC stat to increment
		// No break - treat as error in packet
		//break;
	  }
	}
#if 1		// Sanity check -- can probably turn this off ...MaTed---
	if ( status & OWNERSHIP16)
	{ // This should not happen
	  if (NET_DEBUG > 3)		// raised from 2 to 3
	  {
		printk( "%s: Encountered rx packet not owned\n", dev->name);
	  }
	  // We should do something more here to recover ...MaTed---
	  bRxStat |= BRxDone;			// setup to clear the Done interrupt
	  status |= RxS_CrcErr;	// forces CRC stat to increment
	  // No break - treat as error in packet
	  //break;
	}
#endif	// 1 - Sanity check

#if (MATED_RX_OF_BUG)	// Aquire len data to ring
	{
#if 0
	  printk("R%1ld-%3d %4d-%ld\n", np->channel, first, len, 
			 rx_len_index[ np->channel]);
	  if (++position_output > 5)
	  {
		position_output = 0;
		printk("\n");
	  }
#endif
	  rx_len_array[ np->channel] [rx_len_index[ np->channel]]
		= len;
	  rx_len_index[ np->channel] ++;
	  if (rx_len_index[ np->channel] == RX_OF_MAX_LOG )
		rx_len_index[ np->channel] = 0;
	}
#endif (MATED_RX_OF_BUG)	// Aquire len data to ring

	// check if the frame was received without error
	if (S3C2500_SUCCESS == s3c2500LogRxBdmaError( dev, status))
	{	// good frame received
	  skb = np->rxSkb[ first ].FDskb;
      np->stats.rx_bytes += len;
	  np->stats.rx_packets++;
	  // update lengths
	  skb->tail += len;
	  skb->len += len;
	  skb->dev = dev;
	  skb->protocol = eth_type_trans( skb, dev);
	  dev->last_rx = jiffies;
#if 0
  	  skb = dev_alloc_skb( len + 2 );
	  if (skb == NULL)
	  {
		printk(KERN_NOTICE "%s: Memory squeeze, dropping packet. \n",
			   dev->name);
		np->stats.rx_dropped++;
#if 0	/* we now clear the interrupts on the way in ...MaTed */
		bRxStat |= BRxDone;			// setup to clear the Done interrupt
#endif
		break;
	  }
	  /* 'skb->data' points to the start of sk_buff data area. */
	  skb_put( skb, len);			/* update the length in the skb */

      np->stats.rx_bytes += len;
	  np->stats.rx_packets++;
#endif
	  enterCnt(0x202);		// ******-******* Profiling code
	  if (NET_DEBUG > 5)
	  {
		printk( KERN_ERR "%s: Rx: Before: %d %d len=%d SKB=0x%p DATA=0x%p\n", 
				dev->name, (unsigned int) ringRx->first,
				(unsigned int) ringRx->last,
				(unsigned int) skb->len, skb, skb->data);
	  }
	  if ((NET_DEBUG > 6) || DEBUG_CRC_OFF)
	  {
		int i,j;
		printk( KERN_ERR "S3C2500_Rx: Length %d\n", skb->len);
		for ( j = 0; j < (skb->len & ~ 15); j += 16)
		{
		  for ( i = j; i < j + 16; i += 2)
		  {
			printk("%04x ", 
				   (UINT16) (skb->data[ i + 1 ] + (skb->data[ i ]<<8)));
		  }
		  printk("\n");
		}
		for ( i = (skb->len & ~ 15); i < skb->len; i += 2)
		{
		  printk("%04x ",
				 (UINT16) (skb->data[ i + 1 ] + (skb->data[ i ]<<8)));
		}
		printk("\n");
	  }
	  if (DEBUG_CRC_OFF)
		skb->len -= 4;	// drop CRC

	  enterCnt(0x203);		// ******-******* Profiling code
#if (NET_DEBUG > 3 && 0) // just to test rx maximum rate 
	                     //   -- and overflow error ...MaTed---
	  dev_kfree_skb_irq( skb );
#else
      netif_rx( skb);					// give the SKB to the kernel
#endif
	  skb = __dev_alloc_skb( MAX_PKT_SIZE, GFP_ATOMIC | GFP_DMA);
	  np->rxSkb[ first ].FDskb = skb;
	  rxFd[ first ].FDDataPtr 
		= skb->data;			/* point to skb  data buffer */
	  skb_reserve( skb, 2);		/* 16 bit align the IP header
								    - original buffer is 32 bit aligned */
#if 0	// done later -keeping empty space
	  rxFd[ first ].FDLength = PKT_BUF_SZ;	// think the compiler is smart
	  rxFd[ first ].FDStat = OWNERSHIP16;	//  enough to put these two lines 
	                                        //  together??
#endif
	}

	last = ringRx->last;		// pointer to the old spacer
	// Mark the new frame Descriptor as not usable - spacer
	rxFd[ first ].FDLength = 0;
	rxFd[ first ].FDStat = 0;
	// Advance first (on error we still advance - no purpose to resuse)
	ringRx->last = ringRx->first;		// space of one
	ringRx->first = incIndex( first, ringRx->max);

	// The one that was the spacer is now the newest one for the BMDA engine
	rxFd[ last ].FDLength = PKT_BUF_SZ;
	rxFd[ last ].FDStat = OWNERSHIP16;		// last thing to do is give access

#if 0	// We clear the interrupts before the while loop ...MaTed---
	// we will turn off the bits after the do {}while ...MaTed---
	// have to clear to get next frame receive interrupt (table 7-25.)
	bRxStat |= BRxDone;			// setup to clear the Done interrupt
                                //  - requires write a zero to it
#endif
	if (NET_DEBUG > 5)
	{
	  skb = np->rxSkb[ first ].FDskb;
	  printk( KERN_ERR "%s: Rx: Bottom: %d %d len=%d first-SKB=0x%p "
			  "first-DATA=0x%p\n", 
			  dev->name, (unsigned int) ringRx->first,
			  (unsigned int) ringRx->last,
			  (unsigned int) skb->len, skb, skb->data);
	}
	// if there are no buffers left, we're done anyways (last one is marker)
	if (rxFd[ ringRx->first].FDDataPtr == NULL)
	  break;
  }

#if 0	// I'll rid of this after I verify that the above is true ...MaTed
  // have to clear to get next receive
  WriteCSR( BdmaStat, ReadCSR( BdmaStat) | bdmaStatus);
#endif

  /*
   * I think we can move the following two enables up 
   * into the error handling. Rx will only be disabled 
   * on Not Owner (and only on RxCtl)
   */
  if ( !( BRxEN & (temp = ReadCSR( RxCtl))))
	WriteCSR( RxCtl, temp  | BRxEN);

#if 0	// only RxCtl gets rx disabled
  if ( !( MRxEn & (temp = ReadCSR( MacRxCtl))))
	WriteCSR( MacRxCtl, temp  | MRxEn);
#endif
	
  spin_unlock( &priv->rxLock);		// Again do I need this ???
  enterCnt(0x2ff);		// ******-******* Profiling code
  return;
}
#if DEBUG_RX_DUMP
/************************************************************************
 * 	Dump Rx data structures
 *	
 *
 *
 *
 ************************************************************************/
static void
dumpRx( S3C2500_MAC * priv, char * where )
{
  BPTR	* Rx 		= &priv->qRx;
  BPTR	* RxWait 	= &priv->qRxWait;

  printk( KERN_INFO "\n   ***** %s *****\n", where);
  printk( KERN_INFO "** Rx first-[%p] last-[%p] num-[%ld]\n",
		  Rx->first, Rx->last, Rx->num);
  RXFDS_Dump( (RxFDS *) (Rx->first), "First");
  if (Rx->first != NULL)
	RXFDS_Dump( (RxFDS *) (((RxFDS *) (Rx->first))->fd.FDNext),
				"First->Next");
  RXFDS_Dump( (RxFDS *) (Rx->last), "Last");
  if (Rx->last != NULL)
	RXFDS_Dump( (RxFDS *) (((RxFDS *) (Rx->last))->fd.FDNext),
				"Last->Next");

  printk( KERN_INFO "** Wait first-[%p] last-[%p] num-[%ld]\n",
		  RxWait->first, RxWait->last, RxWait->num);
  RXFDS_Dump( (RxFDS *) (RxWait->first), "Wait First");
  if (RxWait->first != NULL)
	RXFDS_Dump( (RxFDS *) (((RxFDS *) (RxWait->first))->fd.FDNext),
				"Wait First->Next");
  RXFDS_Dump( (RxFDS *) (RxWait->last), "Wait Last");
  if (RxWait->last !=NULL)
	RXFDS_Dump( (RxFDS *) (((RxFDS *) (RxWait->last))->fd.FDNext),
				"Wait Last->Next");
  return;
}
/***********************************************************************
 * Dump RxFDS Struct
 ***********************************************************************/
static void
RXFDS_Dump( RxFDS * fds, char * str)
{
  if (fds == NULL)
	{
	  printk( KERN_INFO "   ---- %s NULL Address ----\n", str);
	  return;
	}
  printk( KERN_INFO "  ---- %s Ptr-[%p] Next-[%p] Data-[%p] Len-[0x%x] Skb-[%p]\n",
		  str, fds, fds->fd.FDNext, fds->fd.FDDataPtr, fds->fd.FDLength,
		  fds->skb);
  //  printk( KERN_INFO "  ---- SKB  HeadNext-[%p] Data-[%p] Len-[0x%x] Skb-[%p]\n",
  return;
}
#endif // DEBUG_RX_DUMP
/************************************************************************
 * static int s3c2500_close( ND dev)
 * Description:
 *	- The inverse routine to net_open().
 * Parameters:
 *	- Pointer to network device structure - everthing resloved through 
 *	this pointer
 * NOTES:
 *	- Sections have been '#if 0' out to make the open / close pairs 
 *	work (ifconfig up / down).
 * 	- We not only release the buffers here, we reallocate them as well.
 *	We should move the reallocation to the open(), but that means 
 *	straightening out more code. Since this is a core process, we have
 *	the buffers ALWAYS allocated. 
 ************************************************************************/
static int
s3c2500_close( ND dev)
{
  S3C2500_MAC 		* np	 	= dev->priv;
  UINT32			chan		= np->channel;
  UINT32			i;
  //  int 			basePort	= dev->base_addr;
#if 1 // debug ...MaTed
  // RINGBUFF 	  	* ringTx 	= & np->ringTx;	// pointer to tx ring buffer
  RINGBUFF 		* ringRx 	= & np->ringRx;	// pointer to rx ring buffer
#endif
  //  struct	sk_buff	* skb;

#if MPROFILE		// dump miniprofile to console
  UINT32			calcTemp;		// temp to calculate time
#endif

  netif_stop_queue( dev);

  /* Flush the Tx and disable Rx here. */
  DisableTxRx( chan );
  DisableInt( chan );

  Reset( chan );				// Reset the H/W
  mdelay( MDELAY_RESET);	// just in case - Let ethernet MAC complete reset

  // Release all TX buffers we own - please no memory leaks
  spin_lock_irq( &np->txLock);
  /* for all the tx frames */
  for( i = 0; i < TX_NUM_FD; i++) 
  {  
	// we have to release any skb's we have
#if !CONFIG_WBUFFER
	if (np->txSkb[ i ].FDskb != NULL)
    {
	  ASSERT( atomic_read( &(np->txSkb[ i ].FDskb->users)) == 1);
	  dev_kfree_skb( np->txSkb[ i ].FDskb );
	  np->txSkb[ i ].FDskb = NULL;
	}
#else // !CONFIG_WBUFFER
	// the Data buffers stay in FDAArea, pointers reset in SetupTxFDs() nothing
	//	to do
#endif // !CONFIG_WBUFFER
  }
  /* Reset all the tx frame descriptors */
  SetupTxFDs( dev);
  spin_unlock_irq( &np->TxLock);

  spin_lock_irq( &np->rxLock);

  if (NET_DEBUG > 4)
  {
	printk( " s3c2500_close: Rx First-%d Last-%d\n", 
			ringRx->first, ringRx->last );
  }

  // Release all the Rx SKB's (we reallocate later (actually in a moment)
  for ( i = 0; i < RX_NUM_FD ; i++)
  {
	if (1 && NET_DEBUG > 3)
	{
	  ASSERT( atomic_read( &(np->rxSkb[ i ].FDskb->users)) == 1);
	}
	
	if (1 && NET_DEBUG > 3)
	{
	  ASSERT( np->rxSkb[ i ].FDskb->list == 0);
	}
	dev_kfree_skb( np->rxSkb[ i ].FDskb );
  }
  /* Reset all the rx frame descriptors */
  /* Yes this would be better in the open() routine, but for now */
  SetupRxFDs( dev );

#if 0		// ...MaTed--- need later for insmod, but not yet
  /* Release the region */
  s3c2500_release_irq_mem( dev );
#endif

  spin_unlock_irq( &np->RxLock);

#if 0		// ...MaTed--- need later for insmod, but not yet
  /* release the priv area with the buffers and all the strcutures */
  kfree( np);
#endif

#if MPROFILE		// dump miniprofile to console
  printk("  Profile index = %ld\r\n", profileAddrIndex);
  printk("index  Code  Address     Jiffie   Counter Difference Time-(83Mhz)\r\n");
  for (i = 0; i < ProfileArraySize; i++)
	{
	  if (profileArray[ i ].code != 0)
		{
		  if ( (profileArray[ i ].code & 0xff) == 0)
			{
			  printk("\t\t------------------------------------------------\n");
			}
		  printk("%5ld, %04x, %8p, %8ld, %8ld, %8ld %10ld uS\r\n",
				 i,
				 profileArray[ i ].code,
				 profileArray[ i ].addr,
				 profileArray[ i ].jiffie,
				 profileArray[ i ].counter,
				 calcTemp = ( i == 0) ? profileArray [ i ].counter
				           : profileArray[ i ].counter
				             - profileArray[ i - 1].counter,
#if 1	// 83 Mhz
				 ((calcTemp * 12048)+500000) / 1000000);
#else	// 75 MHz
				 ((calcTemp * 13333)+500000) / 1000000);
#endif		  
	      mdelay(60);		// to compensate for console serial bug ...MaTed---
		}
	}
  // Clear the values - just in case
  for (i = 0; i < ProfileArraySize; i++)
	{
	  profileArray[ i ].code 	= 0;
	  profileArray [ i ].addr 	= 0;
	  profileArray [ i ].jiffie	= 0;
	  profileArray [ i ].counter= 0;
	}
  profileAddrIndex = 0;
#endif
  return 0;
}
/************************************************************************
 * Get the current statistics.
 * This may be called with the card or closed.
 ************************************************************************/
static struct net_device_stats *s3c2500_get_stats( ND dev)
{
  S3C2500_MAC *	priv = (S3C2500_MAC *) dev->priv;

  return (&priv->stats);
}
/************************************************************************
 * Set or clear the multicast filter for this adaptor.
 * num_addrs == -1	Promiscuous mode, receive all packets
 * num_addrs == 0	Normal mode, clear multicast list
 * num_addrs > 0	Multicast mode, receive normal and MC packets,
 *			and do best-effort filtering.
 ************************************************************************/
static void
set_multicast_list( ND dev)
{
  int 	baseAddr	= dev->base_addr;

  WriteCSR( CamEn, 0);			// turn off ARC usage for now

  if (dev->flags & IFF_PROMISC)
  {
    /* Enable promiscuous mode */
    WriteCSR( CamCtl, ReadCSR( CamCtl) | MNegCAM);
  }
  else if ((dev->flags & IFF_ALLMULTI) || dev->mc_count > HW_MAX_ADDRS)
  {
    WriteCSR( CamCtl, MCompEn | MBroad | MGroup);
  }
  else if (dev->mc_count)
  {
    /* Walk the address list, and load the filter */
    HardwareSetFilter( dev);
    
    WriteCSR( CamCtl, ReadCSR( CamCtl) | MBroad);
  }
  else // setup for normal
  {
    /* setup the MAC HW address */
    SetUpARC( dev, dev->dev_addr);

    WriteCSR( CamEn, 1 << 2);	    // Enable only the third entry
    WriteCSR( CamCtl,  (MCompEn | MGroup | MBroad | MStation));
  }
}
/************************************************************************
 * HardwareSetFilter
 ************************************************************************/
static void HardwareSetFilter( ND dev )
{
  UINT	    			i 			= 0;
  int 					baseAddr	= dev->base_addr;
  struct dev_mc_list  *	ptr 		= dev->mc_list;
  //  UINT32			temp;
  //  UINT16 		  *	addrHalfPtr;

  UINT32				enableMask 	= 0;

  /*
   * NOTE: this section has never been tested ...MaTed---
   */
  while (ptr && i < HW_MAX_ADDRS - 1)
  {
	if ( i & 1)
    {	// odd entry starting at the third location 
	  WriteCSR( Cam[ 3 + (i * 3) / 2],
				((0x0000ffff & ReadCSR( Cam[ 3 + (i * 3) / 2] ))
				| (ptr->dmi_addr[ 1 ] << 24 )
				| (ptr->dmi_addr[ 0 ] << 16 )));
	  WriteCSR( Cam[4 + (i * 3) / 2],
				(ptr->dmi_addr[ 5 ] << 24)
				| (ptr->dmi_addr[ 4 ] << 16)
				| (ptr->dmi_addr[ 3 ] << 8)
				| (ptr->dmi_addr[ 2 ]));
	}
	else
	{
	  WriteCSR( Cam[3 + (i * 3) / 2],
				((ptr->dmi_addr[ 3 ] << 24)
				| (ptr->dmi_addr[ 2 ] << 16)
				| (ptr->dmi_addr[ 1 ] << 8)
				| (ptr->dmi_addr[ 0 ])));
	  WriteCSR( Cam[4 + (i * 3) / 2],
				(0xffff0000 & ReadCSR( Cam[4 + (i * 3) / 2] ))
				| (ptr->dmi_addr[ 4 ] )
				| (ptr->dmi_addr[ 5 ] << 8));
	}
	enableMask |= (1 << ( i + 2 ));
    i++;
	ptr = ptr->next;
  }
  WriteCSR( CamEn, enableMask);	    // Enable all the entries
  if (i != dev->mc_count)
  {
	printk("S3C2500: %s - MultiCast Count (%d) != pointers (%d)\n",
		   dev->name, dev->mc_count, i);
  }
  return;
}

/*********************************************
 * Module support
 *********************************************/

#ifdef MODULE
// ***** NOTE: support not yet completed ...MaTed --

static struct net_device this_device;
static int io = 0x300;
static int irq;
static int dma;
static int mem;

int init_module(void)
{
  int result;

  if (io == 0)
    printk(KERN_WARNING "%s: You shouldn't use auto-probing with insmod!\n",
	   cardname);

  /* Copy the parameters from insmod into the device structure. */
  this_device.base_addr = io;
  this_device.irq       = irq;
  this_device.dma       = dma;
  this_device.mem_start = mem;
  this_device.init      = netcard_probe;

  if ((result = register_netdev(&this_device)) != 0)
    return result;

  return 0;
}

void
cleanup_module(void)
{
  /* No need to check MOD_IN_USE, as sys_delete_module() checks. */
  unregister_netdev(&this_device);
  /*
   * If we don't do this, we can't re-insmod it later.
   * Release irq/dma here, when you have jumpered versions and
   * allocate them in net_probe1().
   */
  /*
    free_irq(this_device.irq, dev);
    free_dma(this_device.dma);
  */
  release_mem_region(this_device.base_addr, NETCARD_IO_EXTENT);

  if (this_device.priv)
    kfree(this_device.priv);
}

#endif /* MODULE */

#if DEBUG_PHY_DUMP
/*********************************************
 * Dump all the phy registers
 *********************************************/
void DumpPhyReg( UINT32 chan)
{
  int i;
  UINT32	Array[4];

  printk( "**** Phy register dump \n");
  for (i=0; i<=32; i+=4)
  {
	(void) ReadPhy( chan, i+0, &Array[0]);
	(void) ReadPhy( chan, i+1, &Array[1]);
	(void) ReadPhy( chan, i+2, &Array[2]);
	(void) ReadPhy( chan, i+3, &Array[3]);

	printk(" Addr[%d], 0x%08x - 0x%08x - 0x%08x - 0x%08x\n",
		   i, (int) Array[0], (int) Array[1], 
		   (int) Array[2], (int) Array[3]);
  }
}
#endif	//DEBUG_PHY_DUMP

/*
 * Local variables:
 *  compile-command:
 *	gcc -D__KERNEL__ -Wall -Wstrict-prototypes -Wwrite-strings
 *	-Wredundant-decls -O2 -m486 -c s3c2500.c.c
 *  version-control: t
 *  kept-new-versions: 5
 *  tab-width: 4
 *  c-indent-level: 2
 * End:
 */
#endif // CONFIG_ETH_S3C2500
