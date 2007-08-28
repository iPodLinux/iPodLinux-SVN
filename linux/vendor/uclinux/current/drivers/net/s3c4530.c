/***********************************************************************
 *  s3c.c : Samsung S3C4530A ethernet driver for linux

    S3C4530A Communication Processor
    Copyright (c) Copyright 2001-2002 Arcturus Networks Inc.
                                      by MaTed <www.ArcturusNetworks.com>

      based on isa-skeleton: A network driver outline for linux.
	    Written 1993-94 by Donald Becker.

		(c) Copyright 2001 Arcturus Networks Inc.

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
	- Because the S3c controller is so "intelligent", it manages the
	buffer list on it's own. Therefore you create the Buffer List for
	the controller without knowing the length required. A maximum MTU
	buffer is pre-allocated regardless of the actual requirements. Either
	the controller firmware can be changed to pick buffers whose size
	is closer to the actual size required, or we can memcpy (ugh!!!) to
	a smaller buffer.

	- module support is either non-existant or not tested (it was a SOC
	afterall)

*************************************************************************/

/************************************************************************
 * modification history
 * --------------------
 *
 * 08/16/02     Michael Durrant <mdurrant@ArcturusNetworks.com>
 * 10/29/01	MaTed (Ted Ma)      First go
 *	- PLEASE set TABS==4
 ************************************************************************/
/*
  Sources:
  - source is available from Arcturus Networks Inc. 
    www.ArcturusNetworks.com 
 
  Finally, keep in mind that the Linux kernel is has an API, not
  ABI. Proprietary object-code-only distributions are not permitted
  under the GPL.
*/
#include "s3c4530.h"

#ifdef CONFIG_ETH_S3C4530	/* skip the entire file if not configured 	*/

static const char *version =
"s3c4530.c: Network driver by Arcturus Networks Inc. <www.ArcturusNetworks.com>\n";

/************* DEBUG **********/
#define VERBOSE y
#ifdef VERBOSE
UINT	verbose = 5;
#endif

#define DEBUG_PHY_DUMP	0
#define	PHY_DEBUG		0

#define DEBUG_RX_DUMP	0	// for dumping Rx Structures

// #define DumpJiffies(x) printk("----s3c:%s Jiffies=[%x]\n",x, jiffies);
#define DEBUG_SINT 0		// debugging interrupts (s3c ones)
#if DEBUG_SINT
#define SINT(x, y) printk("S3INT %s=%0x\n", x, y);
#else
#define SINT(x, y)
#endif

#define DEBUG_INCTXPTR	0	// debugging incrementing txPtr
#if DEBUG_INCTXPTR
S3C_MAC * PrivPtr;			// for debugging when dev / priv structs NA
#define CHECK_ITXPTR(instr)	\
if (((int) ReadCSR( TxPtr)) != (int) PrivPtr->qTx.first) { \
  printk( "DEBUG_INCTXPTR: %s:read %x ", \
		 (char *) #instr, (int) ReadCSR( TxPtr)); \
  if (0 == (0x3 & ((int) ReadCSR(TxPtr)))) \
	printk( "DataPtr=0x%x\n", \
		   (int) (((TxFD *) (ReadCSR( TxPtr)))->FDDataPtr)); \
	else \
	  printk( "DataPtr is not word aligned !! \n"); \
	printk( "           ***: setting to %x\n", \
			(int) WriteCSR(TxPtr, PrivPtr->qTx.first)); }
#else
#define CHECK_ITXPTR(x)
#endif
#define MATED_DEBUG	0		/* Tracking down bad "grow" ? Mostly verify 
							   it is grow that kills things ...MaTed---*/
#if MATED_DEBUG
#define SMEM_CHECK( index) \
	if (((*((int *)(0x7cfea8))) & 3) != 0) { \
      printk("\n @@@@* * MEMORY STOMPED!\n"); \
    } \
	printk("     * * Memory check! %s\n   ---- 0x%x-0x7cfea8 = 0x%x\n", \
		   #index, (int) ((int *)(0x7cfea8)), (*((int *)(0x7cfea8)))); \
	printk("     FILE [%s] FUNCTION [%s] - LINE #%d\n", \
		   __FILE__, __FUNCTION__, __LINE__); \
    (*((int *)(0x7cfea8))) = (*((int*)(0x7cfea8))) & ~3; \
	printk("    @* Memory set to 0x%x\n", (*((int *)(0x7cfea8))) );
#define SMEM_CHECKQ( index) \
	if (((*((int *)(0x7cfea8))) & 3) != 0) { \
      printk("\n @@@@* * MEMORY STOMPED!\n"); \
	printk("     * * Memory check! %s 0x%x-0x7cfea8 = 0x%x\n", \
		   #index, (int) ((int *)(0x7cfea8)), (*((int *)(0x7cfea8)))); \
	printk("     FILE [%s] FUNCTION [%s] - LINE #%d\n", \
		   __FILE__, __FUNCTION__, __LINE__); \
    (*((int *)(0x7cfea8))) = (*((int*)(0x7cfea8))) & ~3; \
	printk("    @* Memory set to 0x%x\n", (*((int *)(0x7cfea8))) ); \
    }
#else
#define SMEM_CHECK( index)
#endif

/*********** end debug *******/

/*
  A bit of an introduction for the algorithms used herein
  - implemented with zero copy.
    - receive - skb's are preallocated with a maximum sized buffer and the 
				data is dma'd into the preallocated buffer. We may add a 
				section to copy the buffer down so we don't use as much
				memory resources - but that's still to be implemented
			  - Has the disadvantage of potentially	thrashing memory by 
				alloc / dealloc SKB's.
	- transmit - skb's are attached to the ring buffer. When the MAC 
				 either transmits the packet or discards it (we don't
				 guarentee delivery at this level), the skb is freed
  - all rx buffers are pre-allocated when the MAC is opened
  - we will use one interrupt per packet sent or received *****FIXME mated
    - we cold drop the TX interrupt - just remember to enable if backpressure 
	stops tx_send_packet
*/
/*
 * The name of the card. Is used for messages and in the requests for
 * io regions, irqs and dma channels
 */
static const char* cardname = "s3c";

/*
 * Setup a base address where offset < 4024 (2**12 - max 
 *	immediate offset for ARM)
 */
static CSR * const baseAddr = ((CSR *) ((BDMA_BASE) | 0x04000000));	// uncached

/**********************************************
Compile time directives
***********************************************/
#define NO_INCLUDE_MAC_RX_INT	1		// do not include (or set up) the
										//  MAC receive interrupt
#define	ENABLE_DESTRUCTOR		0		// use destructor to set 
/************************************************************************
	"Knobs" that adjust features and parameters.
    - Set the copy breakpoint for the copy-only-tiny-frames scheme.
      - Setting to > 1512 effectively disables this feature.
	  *** NOT implemented *** FixMe ...MaTed
*******************************************************************/
static const int rx_copybreak = 1536;

/* Allow setting MTU to a larger size, bypassing the normal ethernet setup. */
/**** Currently unused ...MaTed--- ****/
static const int mtu = 1500;

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
//  We might need this back - right now handle as many as buffers allow
// *** Hard coded in s3c_tx and s3c_rx ...MaTed---
//static int max_interrupt_work = 32; 

static UINT net_debug = NET_DEBUG;

/******************************************************
 * Internal S3C data structures
 ******************************************************/
UINT32	numS3cMacs	= 0;	// number of macs
char *		MacName[]
//	= {"ethaddr0", "ethaddr1", "ethaddr2", "unknown"};
	= {"HWADDR0", "HWADDR1", "HWADDR2", "unknown"};

/************************************************************************
 * Internal Function Declarations - all static
 ************************************************************************/

static _INLINE_ int PHYWait( void);
static int WritePhy( UINT32 addr, UINT32 reg, UINT32 data);
static int ReadPhy( UINT32 addr, UINT32 reg, UINT32 * data);
static _INLINE_ int txFull( ND dev);
static void SetupPHY( UINT32 mode);
static ULONG PHYCheckCable( void);
static int PhyInit( ND dev);
static ULONG SetupTxFDs( ND dev );
static ULONG SetupRxFDs( ND dev);
static _INLINE_ void EnableInt( void );
static _INLINE_ void DisableInt( void);
static _INLINE_ void EnableTxRx( void );
static _INLINE_ void DisableTxRx( void );
static int Reset( void);
static void SetupRegistersTx( void);
static void SetupRegistersRx( void);
static _INLINE_ void SetUpTxFrmPtrRegister( ND dev);
static _INLINE_ ULONG TxRxInit( ND dev);
static int SetMacAddr( ND dev, void * addr);
static int SetUpARC( ND dev, void * addr);
#if ENABLE_IOCTL
static _INLINE_ void SetSpeed100( void);
static _INLINE_ void SetSpeed10( void);
#endif	// ENABLE_IOCTL
// ***** FIXME ** InvalidateCacheLIne should move to other file ...MaTed---
_INLINE_ void InvalidateCacheLine( UINT32 bufaddr);
_INLINE_ void InvalidateBuffer( void * bufaddr, UINT length);
_INLINE_ void FlushCache(void);

/* Index to functions, as function prototypes. */

int __init	s3c_probe( ND dev);
static int	s3c_InitVars( ND dev);
static _INLINE_ int s3c_request_irq_mem( ND dev);

static int	s3c_probe1( ND dev);
static _INLINE_ int s3cLogTxMACError( ND dev, int status);
static _INLINE_ int s3cLogRxBdmaError( ND dev, int status);
static void	s3c_tx_timeout( ND dev);
static _INLINE_ int 	s3cInitPriv( ND dev );

static int	s3c_open( ND dev);
static int	s3c_send_packet( struct sk_buff *skb, ND dev);
/* four interrupt routines - with one helper */
static void	s3c_MacTx( int irq, void *dev_id, struct pt_regs * regs);
static _INLINE_ void TxPktCompletion( ND dev);
static void	s3c_BdmaTx( int irq, void *dev_id, struct pt_regs * regs);
#if ! NO_INCLUDE_MAC_RX_INT
static void	s3c_MacRx( int irq, void *dev_id, struct pt_regs * regs);
#endif
static void	s3c_BdmaRx( int irq, void *dev_id, struct pt_regs * regs);
static void s3cAddRxSKBs( ND dev, UINT32 MaxIn, UINT32 MinIn );
#if DEBUG_RX_DUMP
static void dumpRx( S3C_MAC * priv, char * where);
static void RXFDS_Dump( RxFDS * fds, char * str);
#endif
#if	ENABLE_DESTRUCTOR 
static void	s3cDestructor( struct sk_buff *skb);
#endif
static int	s3c_close( ND dev);
static struct net_device_stats	*s3c_get_stats( ND dev);
static void	set_multicast_list( ND dev);
static void	HardwareSetFilter( ND dev);

#if DEBUG_PHY_DUMP
static void DumpPhyReg( void);
#endif

/************************************
 * Temporary routines
 ************************************/
#define S3C_TEMP_ROUTINES 0
#if S3C_TEMP_ROUTINES
static int get_ethernet_addr(char * MatchName, char * addr);
#endif

/************************************************************************
 * Utility routines
 ************************************************************************/
// NOTE: assumes base is already KSEG1 ????
#if 0
#define WriteCSR(base, reg, value) \
		((volatile UINT32)(((CSR *)( base))->reg) = (value))
#define ReadCSR(base,  reg) ((volatile UINT32)(((CSR *) (base))->reg))
#else
#define WriteCSR(reg, value) (baseAddr->reg = (UINT32) (value))
#define ReadCSR( reg) (baseAddr->reg)
#endif

#if 0	// ...MaTed--- don't need this here
/***************************************************************************
 * Interface to boot environment variables
 ***************************************************************************/
int		errno; 		// the macros want this - not friendly  ...MaTed---
// _bsc1(char *, readbenv,   int, a)
//_bsc1(char *, getbenv, char *, a)
// _bsc1(int,    setbenv, char *, a)
#endif
                                         
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
  return ((TX_NUM_FD - 1) <= ((S3C_MAC *)dev->priv)->qTx.num);
}
/*************************************************************************
 * PHYWait - Waits for Station Management to become non-busy
 *
 * This inline routine is a helper to wait for Station Management to 
 *	become non-busy - ie for the command to complete
 *
 * returns	S3c_SUCCESS if succesfully written
 *       	other returns indicate timeout or other error
 *************************************************************************/
static _INLINE_ int 
PHYWait( void)
{
  UINT32 i = 0;

  while (( ReadCSR( SmCtl)) & MStBusy)
  {
    if ( i++ >= PHY_BUSYLOOP_COUNT)
    {
#ifdef VERBOSE
      if ( verbose)
      {
		printk( "S3C4530-PHY: *** BUSY Timeout! ***\n");
      }
#endif
      return( S3C_TIMEOUT);
    }
	mdelay( 10);	// 10 millisecond delay per loop
  }
  return( S3C_SUCCESS);
}
/*************************************************************************
 * WritePHY - writes data to phy972
 *
 * This routine is a helper to write Phy data
 *
 * data is written to reg of the PHY at addr
 * returns	S3c_SUCCESS if succesfully written
 *       	other returns indicate timeout or other error
 *************************************************************************/
static int 
WritePhy( UINT32 addr, UINT32 reg, UINT32 data)
{
  int rc;

  if ( S3C_SUCCESS != ( rc= PHYWait()))
	return( rc);
  WriteCSR( SmData, data);  	/* set data */
  WriteCSR( SmCtl, MStWr | MStBusy | reg | addr << 5);	// send WRITE cmd
#if PHY_DEBUG
    printk( "S3C4530-PHY: wrote 0x%08x to PHY%d register 0x%x\n",
			(int) data, (int) addr, (int) reg);
#endif
	return( PHYWait());
}
/*************************************************************************
 * ReadPHY - reads data from phy972
 *
 * This routine is a helper to read Phy data
 *
 * data is read from reg of the PHY at addr
 * returns	S3C_SUCCESS if succesfully read
 *       	other returns indicate timeout or other error
 *************************************************************************/
static int 
ReadPhy( UINT32 addr, UINT32 reg, UINT32 * data)
{
  int rc;

  	// wait for any previous command to complete
  if ( S3C_SUCCESS != ( rc= PHYWait()))
	return( rc);
  WriteCSR( SmCtl, MStBusy | reg | addr << 5);	// send READ cmd
  // wait for read command to complete
  if ( S3C_SUCCESS != ( rc= PHYWait()))
	return( rc);
  *data = ReadCSR( SmData);  	/* get data */
#if PHY_DEBUG
    printk( "S3C4530-PHY: read 0x%08x from PHY%d register 0x%x\n",
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
 * LevelOne LXT972
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
SetupPHY( UINT32 mode) 
{
  UINT32	j;
  UINT32	data;

  /* First reset the PHY */
  WritePhy( PHYAddr, PHYCtl, PHYCtlReset);
  /*
   * Read the Reset bit(0.15) of the PHY to see if the reset is completed.
   */
  for ( j = 0; j < PHY_RESETLOOP_COUNT; j++)
  {
    if (S3C_SUCCESS != ReadPhy( PHYAddr, PHYCtl, &data))
	{
#ifdef VERBOSE
	  printk( "S3C4530-PHY: RESET command not accepted! ***\n");
#endif
	  return;
	}
    if ( (data & PHY_RESET) == 0)
    {
#ifdef VERBOSE
      if ( verbose)
      {
		printk( "S3C-PHY reset\n");
      }
#endif
      break;
    }
	mdelay( 10);	// 10 mS
  }

  mdelay( 10);	// just in case
  // change the LED to right lin/activity LEFT->speed
  WritePhy( PHYAddr, PHYLedCfg, PHYLEDCFG);

#if PHY_DEBUG
  printk( "S3C4530-PHY: setting up mode-%08x\n", (int) mode);
#endif
  if (S3C_SUCCESS != ReadPhy( PHYAddr, PHYCtl, &data))
  {
#ifdef VERBOSE
	printk( "S3C4530-PHY: Read mode command not accepted! ***\n");
#endif
	return;
  }
#if PHY_DEBUG
  printk( "S3C4530-PHY: Original mode-%08x\n", (int) data);
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
	if (S3C_SUCCESS != WritePhy( PHYAddr, PHYCtl, mode))
	{
#ifdef VERBOSE
		printk( "S3C4530-PHY: Mode change failed for Control\n");
#endif
	}
	return;		// no autonegotiation - no advertising
  }

#if 1
  // Advertise to the world what we will negotiate
  if (S3C_SUCCESS != ReadPhy( PHYAddr, PHYANegAd, &data))
  {
#ifdef VERBOSE
	printk( "S3C4530-PHY: Read Negotiate Advertize not accepted! ***\n");
#endif
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
#endif
  if (S3C_SUCCESS != WritePhy( PHYAddr, PHYANegAd, data))
  {
#ifdef VERBOSE
	printk( "S3C4530-PHY: Mode change failed for Advertise\n");
#endif
	return;
  }
  /* since we are auto negotiating, force a restart */
  mode |= PHY_CONTROL_RESTART_AUTONEG;	// just in case
  if (S3C_SUCCESS != WritePhy( PHYAddr, PHYCtl, mode))
  {
#ifdef VERBOSE
	printk( "S3C4530-PHY: Autonegotiate failed to restart\n");
#endif
	return;
  }
#if 1 // let the next routine check for the end of the negotiation
  j = 0;
  ReadPhy( PHYAddr, PHYCtl, &data);
  while ( data & PHY_CONTROL_RESTART_AUTONEG)
  {
    if ( j++ >= PHY_NEGLOOP_COUNT)
    {
#ifdef VERBOSE
      if ( verbose)
      {
		printk( "S3C4530-PHY: * Restart AutoNegotiate did not complete ");
		printk( " Will check again later ***\n");
      }
#endif
      return;
    }
	mdelay( 10);	// 10 millisecond delay per loop
	ReadPhy( PHYAddr, PHYCtl, &data);
  }
#endif
  return;
}
/*************************************************************************
 * PHYCheckCable - Check for Cable connection to the PHY 972.
 *
 *
 * RETURNS: S3C_SUCCESS, or S3C_FAIL
 * Needs to be modified according to the PHY, and 
 * needs to be modified according to the SPEC of Level1 972
 **************************************************************************/
static UINT32 
PHYCheckCable( void)
{
  UINT32	data;

  if (S3C_SUCCESS != ReadPhy( PHYAddr, PHYStat2, &data))
  {
#ifdef VERBOSE
	printk( "S3C-CheckCable: Read status 2 command not accepted! ***\n");
#endif
	return (S3C_FAIL);
  }
#if PHY_DEBUG
  printk("S3C-CheckCable: Status 2 = 0x%x\n", (int) data);
#endif
  if (!(data & PHY_LINK_UP))
    return( S3C_FAIL);
#ifdef VERBOSE
  else
  {
	if ( verbose)
    {
	  if (data & PHYMODE_100)
		printk( "S3C4530-PHY: running at 100 Mbps, ");
	  else
		printk( "S3C4530-PHY: set for 10 Mbps,  ");
	  
	  if(data & PHYFULL_DUPLEX)
		printk( "FULL Duplex Mode.\n");
	  else
		printk( "HALF Duplex Mode. \n");
	}
  }
#endif
  return S3C_SUCCESS;
}
/*************************************************************************
 * PhyInit - initialize and configure the PHY device.
 *
 * This routine initialize and configure the PHY device 
 *
 * RETURNS: S3C_SUCCESS, or S3C_FAIL
 * Needs to be modified according to the PHY, and 
 * needs to be modified according to the SPEC of Level1 972.
 **************************************************************************/
static int 
PhyInit( ND dev)
{
  S3C_MAC *	np = (S3C_MAC *) dev->priv;
 
  UINT32	status = S3C_SUCCESS;
   UINT32	i;
  static const UINT32 modeList[] = 
  {
	PHY_CONTROL_AUTONEG_HALF_10,	PHY_CONTROL_SPEED_10_HALF,
	PHY_CONTROL_SPEED_100_HALF,	PHY_CONTROL_AUTONEG_HALF_100,
	PHY_CONTROL_AUTONEG_FULL_10, 	PHY_CONTROL_SPEED_10_FULL,
	PHY_CONTROL_SPEED_100_FULL,	PHY_CONTROL_AUTONEG_FULL_100
  };	
  // Setup duplex and speed to pass on
  //   automatic get 0
  // setup Station Management data -- NOTE: Incomplete Data
#define MATED_PHYTEST 0
#if MATED_PHYTEST
  //  SetupPHY( PHY_CONTROL_AUTONEG_FULL_10);
  SetupPHY( PHY_CONTROL_SPEED_100_FULL);
#else
  SetupPHY( modeList[ np->phySpeed + (np->phyDpx << 2)]);
#endif

  for ( i = 0; i < PHY_BUSYLOOP_COUNT; i++) 
  {
	if ( S3C_SUCCESS == (status = PHYCheckCable( )))
	{
#ifdef VERBOSE
	  if ( verbose)
      {
		printk( "S3C4530-PHY: Cable detected\n");
	  }
#endif
	  break;
	}
	mdelay( 10);	// 10 mS
  }
  //#ifdef VERBOSE
  if ( status != S3C_SUCCESS)
  {
    printk( "S3C4530-PHY: **** NO Cable is connected to MAC %s %08lx\n", 
			dev->name, (UINT32) baseAddr);
  }
  //#endif
#if DEBUG_PHY_DUMP
  DumpPhyReg();
#endif	//DEBUG_PHY_DUMP

#if 0		// no failure if no cable
  return ( status);
#else
  return ( S3C_SUCCESS);
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
 *	S3C_SUCCESS
 *	S3C_FAIL
 *********************************************************************/
static ULONG 
SetupTxFDs( ND dev )
{
  int i;

  TxFDS * tptr;		// temporary pointer
  TxFDS * lastPtr;	// keep last pointer to update
  S3C_MAC *	priv;	//pointer to private area

  if (0 == (priv =  dev->priv))
  {
    return ( S3C_FAIL);
  }

  /* Setup the Queue pointers */
  tptr	// pointer to first in ring
    = priv->qTx.first
    = priv->qTx.last
    = &priv->txFd[ 0];
  priv->qTx.num = 0;	// no buffers in use

  lastPtr
    = &priv->txFd[ TX_NUM_FD - 1];

  /* for all the tx frames */
  for(i = 0; i < TX_NUM_FD; i++) 
  {  
    /*  CPU Ownership and Uncached and index and crash ( not word aligned) */
    tptr->fd.FDDataPtr	= (UCHAR *) (0x4000000 | (i * 4 + 1));
    tptr->fd.FDStat	    = 0x00000000;
    tptr->fd.FDLength	= 0x0000;			// nothing there yet
	//	tptr->fd.FDModes	= TxWidSetup;		// set widget bits (pg 7-18)
	tptr->fd.FDModes	= 0;				// Do setup only for packet being sent/
	tptr->skb			= (struct sk_buff *) NULL;

    lastPtr->fd.FDNext	= (TxFD *) tptr;	// Cast just for warning
	//  link is for top "half"
    lastPtr = tptr++;
  }	
#ifdef VERBOSE
  if ( verbose)
  {
    printk( KERN_INFO "SetupTxFDs-for %s, First buff %x, Last %x\n",
	    dev->name, (int) &priv->txFd[ 0 ],
	    (int) &priv->txFd[ TX_NUM_FD - 1]);
  }
#endif
  InvalidateBuffer( &priv->txFd, TX_NUM_FD * sizeof( TxFDS));
  /* now tell the MAC about the beginning of the TX buffers */
  /*** NOPE gets done in TxRxInit */
  // WriteCSR( TxPtr, priv->qTx.first);
  return ( S3C_SUCCESS);
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
 *	S3C_SUCCESS
 *	S3C_FAIL
 ***********************************************************************/
static ULONG SetupRxFDs( ND dev)
{
  int 				i;
  S3C_MAC			* priv;		// pointer to the private s3c area
  RxFDS				* ftPtr;	// temp pointer for FDA (rx fram descriptor)
  RxFDS				* fLastPtr;	// last pointer for FDA
  RxFDS				* saveLastPtr;	// last pointer for FDA
  struct sk_buff 	* skb;

  if ( 0 == (priv = (S3C_MAC *) dev->priv))
  {
    return ( S3C_FAIL);
  }
  /*
   * Setup the Queue pointers
   */
  /** First the Fd **/
  ftPtr	// pointer to first in ring
    = priv->qRx.first
    = &priv->rxFd[ 0];
  priv->qRx.num = RX_NUM_FD;	// buffers in use
  priv->qRx.last
    = fLastPtr
	= saveLastPtr
    = (RxFDS *) &priv->rxFd[ RX_NUM_FD - 1];	// last one

  /* 
   * The Wait list only keeps track of FDs which don't have SKB's
   * - filled and put back on end of Rx queue later
   */
  priv->qRxWait.first
	= priv->qRxWait.last
	= NULL;						// point to nothing
  priv->qRxWait.num = 0;		// all given to MAC

  /*
   * Initialize the FD Area. Give the ownership of each 16
   * byte block to the controller - ready to RX. (The patterns of
   * 0xBBB etc is for debugging only!)
   ** NOTE: There is no EOL bit (original code had)
   */
#if MATED_DEBUG
  SMEM_CHECK("****** S3C: SetupRxFDs before loop before delay ***:");
  {
	int iMaTed;
	for (iMaTed=0; iMaTed<500; iMaTed++)
	  printk(" %c",8);
  }
#endif
  SMEM_CHECK("****** S3C: SetupRxFDs before loop after delay ***:");
  for ( i = 0; i < RX_NUM_FD; i++)
  {
	/*
	 * Each FD points to a large enough buffer so that for each
	 * incoming packet will fit in one buffer.
	 *
	 */
	// we preallocate SKBs
	skb = dev_alloc_skb( PKT_BUF_SZ);
	if (skb == NULL)
    {
	  printk( KERN_NOTICE "%s: Unable to allocate buffers.\n",
			  dev->name);
	}
	//	SMEM_CHECK("S3C: RxFDs- in skb Allocate loop after alloc");
	ASSERT( 0 == (OWNERSHIP & (UINT32) skb->data));	// address space limited to 
											//  lower 2GB
	//get data offset
	// We invalidate cache after mac dma's data (might be shorter)
	// InvalidateBuffer( skb->data, PKT_BUF_SZ);	// don't want old contents
	// But we do have to make sure we don't have outstanding writes to 
	//  the buffer
	FlushCache();
	ftPtr->fd.FDDataPtr = (UCHAR *) ((UINT32)skb->data | OWNERSHIP);
	ftPtr->skb = skb;		// save for later
	InvalidateBuffer( (void *) ftPtr->fd.FDDataPtr, PKT_BUF_SZ);
	skb->dev = dev;
#if	ENABLE_DESTRUCTOR 
	skb->destructor = priv->destructor;
#endif
	skb_reserve( skb, 2);			// 16 byte alignment - for TCP
	ftPtr->fd.FDLength = PKT_BUF_SZ;
	ftPtr->fd.FDStat = 0;				// Mac wil fill this in

    // Now do the links	
    fLastPtr->fd.FDNext   = (RxFD *) ftPtr;
    fLastPtr = ftPtr++;
	SMEM_CHECK("S3C: RxFDs- in skb Allocate loop at end");
  }
  SMEM_CHECK("****** S3C: SetupRxFDs after setup loop ***:");
#if 1	// we aren't going to stop the BDAM engine from getting to the 
		//	last one, It stops itself
  // Need to put in the EOL
  // We should just fix the above loop not to set the OWNERSHIP bit on the
  //   last one, but it's only initialization ...MaTed---
  saveLastPtr->fd.FDDataPtr = (UCHAR *) 
	(~OWNERSHIP & (UINT) saveLastPtr->fd.FDDataPtr);
#endif

    priv->rxFd[ RX_NUM_FD - 1].fd.FDNext = (RxFD *) NULL;	// last one

#ifdef VERBOSE
  if ( verbose)
  {
    printk( "SetupRxFDs-for %s\n", dev->name);
  }
#endif
  // This invalidates the header structures
  InvalidateBuffer( &priv->rxFd, RX_NUM_FD * sizeof( RxFDS));
  /*
   * Now tell the MAC about the buffers
   */
  WriteCSR( RxPtr, priv->qRx.first);
  return ( S3C_SUCCESS);
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
EnableInt( void)
{
  // temp disable (is this needed??)
	// disable all temporarily
  WriteCSR( TxCtl, ReadCSR( TxCtl ) & ~ BTxINTMask );
  WriteCSR( RxCtl, ReadCSR( RxCtl )	& ~ BRxINTMask );
  WriteCSR( MacCtl, ReadCSR( MacCtl ) & ~ MCtlINTMask );
  WriteCSR( MacTxCtl, ReadCSR( MacTxCtl ) & ~ (MTxCtlINTMask | MTxCtlEnComp ));
  WriteCSR( MacRxCtl, ReadCSR( MacRxCtl ) & ~ MRxCtlINTMask );
  
  /* Enable Interrupts */
  /* first logic level */
#if !NO_INCLUDE_MAC_RX_INT
  enable_irq( MAC_RX_IRQ);
#endif
  enable_irq( MAC_TX_IRQ);
  enable_irq( BDMA_RX_IRQ);
  enable_irq( BDMA_TX_IRQ);
  /* then from the hardware */
#if !NO_INCLUDE_MAC_RX_INT
  s3c4530_unmask_irq( MAC_RX_IRQ);
#endif
  s3c4530_unmask_irq( MAC_TX_IRQ);
  s3c4530_unmask_irq( BDMA_RX_IRQ);
  s3c4530_unmask_irq( BDMA_TX_IRQ);

  /* Enable MAC and BDMA interrupts - everything for now */
  WriteCSR( TxCtl, ReadCSR( TxCtl ) | BTxINTMask );
  WriteCSR( RxCtl, ReadCSR( RxCtl )	| BRxINTMask );
  // Right Now I don't even know what vector this goes to
  //  It only contains a error counter rolls over I don't 
  //  have a reason for it. FIXME later ...MaTed---
  //  WriteCSR( MacCtl, ReadCSR( MacCtl ) | MXtlINTMask );
  WriteCSR( MacTxCtl, ReadCSR( MacTxCtl ) | MTxCtlINTMask );
  WriteCSR( MacRxCtl, ReadCSR( MacRxCtl ) | MRxCtlINTMask );

  CHECK_ITXPTR("EnableInt")
}
/**********************************************************************
 * Function Name:	DisableInt
 *
 *   Purpose:   Disables the Interrupt for the specified Dev
 *   
 *   Parameters:    Device
 ***********************************************************************/
static _INLINE_ void 
DisableInt( void )
{
	// disable all temporarily
  WriteCSR( TxCtl, ReadCSR( TxCtl ) & ~ BTxINTMask );
  WriteCSR( RxCtl, ReadCSR( RxCtl )	& ~ BRxINTMask );
  // See above routine for comments
  //  WriteCSR( MacCtl, ReadCSR( MacCtl ) & ~ MXtlINTMask );
  WriteCSR( MacTxCtl, ReadCSR( MacTxCtl ) & ~ (MTxCtlINTMask | MTxCtlEnComp));
  WriteCSR( MacRxCtl, ReadCSR( MacRxCtl ) & ~ MRxCtlINTMask );
  
  /* Disable Interrupt */
  /* first the H/W */
#if !NO_INCLUDE_MAC_RX_INT
  s3c4530_mask_irq( MAC_RX_IRQ);
#endif
  s3c4530_mask_irq( MAC_TX_IRQ);
  s3c4530_mask_irq( BDMA_RX_IRQ);
  s3c4530_mask_irq( BDMA_TX_IRQ);
  /* then the logical */
#if !NO_INCLUDE_MAC_RX_INT
  disable_irq( MAC_RX_IRQ);
#endif
  disable_irq( MAC_TX_IRQ);
  disable_irq( BDMA_RX_IRQ);
  disable_irq( BDMA_TX_IRQ);

  CHECK_ITXPTR("DisableInt")
}
/**********************************************************************
 * Function Name:	EnableTxRx
 *
 *   Purpose:   Starts transmission and reception.
 *   
 *   Parameters:    MAC basePort
 ***********************************************************************/
static _INLINE_ void 
EnableTxRx( void )
{
  /* Enable Transmission */
  // -- Only enable the Tx when there is something to send moved 
  //     to s3c_send_packet
  // WriteCSR( TxCtl, (( ReadCSR( TxCtl)) | BTxEN));
  // tx Enable AND not Tx Halt
  WriteCSR( MacTxCtl, (( ReadCSR( MacTxCtl)) | MTxCtlTxEn) & ~MTxCtlTxHalt);

  /* Enable Reception    */
  WriteCSR( RxCtl, (( ReadCSR( RxCtl)) | BRxEN));
  // rx Enable AND not Rx Halt
  WriteCSR( MacRxCtl, (( ReadCSR( MacRxCtl)) | MRxCtlRxEn) & ~MRxCtlRxHalt);

  CHECK_ITXPTR("EnableTxRx")
}
/**********************************************************************
 * Function Name:	DisableTxRx
 *
 *   Purpose:   Stops transmission and reception.
 *   
 *   Parameters:    MAC basePort.
 ***********************************************************************/
static _INLINE_ void 
DisableTxRx( void)
{
  /* Cancel Transmission */
  WriteCSR( TxCtl, (( ReadCSR( TxCtl)) & ~ BTxEN));
  // disable Tx and cause immediate halt
  WriteCSR( MacTxCtl, (( ReadCSR( MacTxCtl)) & ~ MTxCtlTxEn) | MTxCtlTxHalt);
  /* Cancel Reception    */
  WriteCSR( RxCtl, (( ReadCSR( RxCtl)) & ~BRxEN));
  // rx disable AND immediate Rx Halt
  WriteCSR( MacRxCtl, (( ReadCSR( MacRxCtl)) & ~ MRxCtlRxEn) | MRxCtlRxHalt);

  CHECK_ITXPTR("DisableTxRx")
}
/*************************************************************************
 * Function Name:	Reset
 *
 * Purpose:	To reset the MAC.
 *
 * Return Value:    S3C_FAIL on failure
 *                  S3C_SUCCESS on success
 **************************************************************************/
static int
Reset( void)
{
  UINT32 	i = 0;

  // Stop all tx / rx
  WriteCSR( MacCtl, (( ReadCSR( MacCtl)) | MCtlHaltImm));
  mdelay( 10);	// 10 mS
  // reset the BDMA TX block
  WriteCSR( TxCtl, BTxRS);
  // reset the BDMA RX block
  WriteCSR( RxCtl, BRxRS);

  // Software reset of all MAC control and status register 
  //   and MAC state machine
  WriteCSR( MacCtl, MCtlReset);
  while (MCtlReset & ReadCSR( MacCtl))
  {
	if (i++ >  MCtlReset_BUSY_LOOP)
	  break;
	mdelay( 10);	// 10 mS
  }
#if NET_DEBUG > 1
  if (i >= MCtlReset_BUSY_LOOP)
	printk( "SC3: MAC did not come out of RESET :-( \n");
#endif
  // setup the BDMA rx Word Alignment (must be done directly after reset
  WriteCSR( RxCtl, BRxSetup_RESET);

  CHECK_ITXPTR("Reset")

  return ( S3C_SUCCESS);
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
SetupRegistersTx( void)
{

  WriteCSR( TxCtl, BTxSetup);		// Buffered DMA transmit control reg
  // setup in TxRxInit
  // WriteCSR( TxPtr, priv->qTx.first); // Tx frame descriptor start addr
  WriteCSR( MacTxCtl, MTxCtlSetup);	// Transmit Control

  CHECK_ITXPTR("SetupRegistersTx")
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
SetupRegistersRx( void)
{
  UINT32	i;

  // BDMA rx setup
  WriteCSR( RxCtl,BRxSetup);	// Buffered DMA receive control reg
  // Setup in SetupTxFDs
  // WriteCSR( RxPtr;		/* 0x0c Rx frame descriptor start addr		*/
  WriteCSR( RxLSz, PKT_BUF_SZ);	// Receive Frame maximum size
  for (i = 0; i < 32; i++)
  {
	WriteCSR( Cam[i], 0xffffffff);		// CAM Content, set for broadcast
  }
  WriteCSR( CamCtl, CAMCtlSETUP);		// CAM Control - Accept anything
  WriteCSR( MacRxCtl, MRxCtlSETUP);		// Rx Control
  WriteCSR( MacCtl, MCtlSETUP);			// Full Duplex 100

  WriteCSR( CamEn, 0x04);				// CAM Enable, third entry
  CHECK_ITXPTR("SetupRegisterRx")
}
/*************************************************************************
 * Function Name:	SetUpTxFrmPtrRegister
 *
 * Purpose:	tell the mac where the Tx Frames are
 **************************************************************************/
static _INLINE_ void
SetUpTxFrmPtrRegister( ND dev)
{
  S3C_MAC * 	priv = dev->priv;

  // NOTE: TxPtr doesn't care about uncached / cached (only bits 0:25)
  WriteCSR( TxPtr, priv->qTx.first);
}
/*************************************************************************
 *	Function Name:		TxRxInit
 *
 *   Purpose:	Set up the Ethernet to receive and transmit data.
 *
 *			NOTE: Check the order that things happen
 *				It needs to be checked (too many routines) FIXME ...MaTed---
 *
 *   Return Value:  S3C_FAIL on failure
 *                  S3C_SUCCESS on success
 *					- Always Succeeds
 **************************************************************************/
static _INLINE_ ULONG 
TxRxInit( ND dev)
{
  //  Reset( dev);
  SetUpTxFrmPtrRegister( dev);	// do this before setting BDMA registers
  SetupRegistersTx( );
  SetupRegistersRx( );
  DisableTxRx( );
  DisableInt( );
  // ...MaTed--- changed to always bring up the interface
  PhyInit( dev);

  CHECK_ITXPTR("TxRxInit")

  return (S3C_SUCCESS);
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
  UINT32	save;
  UCHAR	   * addr = (UCHAR *) addrin;

  /* NOTE: This routine assumes little endian setup of UINT32 */

  /* don't update while active */
  save = ReadCSR( CamEn);		// we leave other bits alone (later)
  WriteCSR( CamEn, 0x00000000);	// don't use any ARC entries
  /* Enter Address at third entry - hard code */
  WriteCSR( Cam[3], addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24 );
  WriteCSR( Cam[4], (ReadCSR( Cam[4]) & 0xffff0000) | addr[4] | addr[5] << 8);
#if 0
  // ** NOTE you could speed this up by using a UINT32 and
  //    a UINT16 = statement (regardless of endian), but this is clearer
  //	...MaTed---
  WriteCSR( Cam [2 * 6 + 0 ], addr [0]);
  WriteCSR( Cam [2 * 6 + 1 ], addr [1]);
  WriteCSR( Cam [2 * 6 + 2 ], addr [2]);
  WriteCSR( Cam [2 * 6 + 3 ], addr [3]);
  WriteCSR( Cam [2 * 6 + 4 ], addr [4]);
  WriteCSR( Cam [2 * 6 + 5 ], addr [5]);
#endif
  /* Enable Third Address */
  save |= 0x8;
  WriteCSR( CamEn, save);
  /* Enable ARC Compare +BCAST */
  //  WriteCSR( basePort, ARCCtl, (ARC_CompEn | ARC_BroadAcc | ARC_StationAcc));
  // Make it accept anything within dest
  WriteCSR( CamCtl, CompEn | BroadAcc | GroupAcc | StationAcc);
  return ( 0);
}
#ifdef ENABLE_IOCTL
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
  WriteCSR( MacCtl, (( ReadCSR(MacCtl)) & MCtlLoop10) );
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
  WriteCSR( MacCtl, (( ReadCSR( MacCtl)) & ~MCtlLoop10 );
}
#endif	// ENABLE_IOCTL
/************************************************************************
 * Function Name:	InvalidateCacheLine
 * Purpose:	Invalidate cache which contains the address
 * 		- we reduce to start on 16 byte boundary (should own to
 *			lower 16 byte boundary 
 * 		- should "own" to next higher 16 byte boundary
 *		** Using two different algorithms - one just uses the tagline
 *		**  flushes the cache lines regardless of the tags
 *		** The other checks the tags and only does something if it
 *		**	matches the buffer address
/ ***********************************************************************/
_INLINE_ void 
InvalidateCacheLine( UINT32 bufaddr)
{
  UINT32 *	TagRamPtr = (UINT32 *) CACHE_TAG_RAM;

#if 1	// fastest way - intrusive
  TagRamPtr[ 0xff & (bufaddr >> L1_CACHE_SHIFT)] = 0;
#else
#if 1	// preserve tags !?? FOR WHY
  TagRamPtr[ 0xff & (bufaddr >> L1_CACHE_SHIFT)] &= 0X3FFFFFF;
#else	// only zero if 14 bit tag matches
{ 
  UINT32	Tag;
  UINT32	tagline

  Tag &= 0x3fff000;
  tagline =  0xff & (bufaddr >> L1_CACHE_SHIFT);
  if ( (Tag >> (16 - L1_CACHE_SHIFT) == (0x3fff & TagRamPtr[ tagline ]))
  {
	TagRomPtr[ tagline } &= 0xbfffffff;
  }
  else
  {	// checking against Set 1 tag
	/ / / / NOTE: the  following not fixed for L1_CACHE_SHIFT / / ...MaTed---
	if ( (Tag << 3) == (0x1fff8000 & TagRamPtr[ tagline ]))
    {
	  TagRomPtr[ tagline } &= 0x7fffffff;	// top bit
	}
  }
#endif
#endif
  return;
}
/************************************************************************
 * Function Name:	InvalidateBuffer
 * Purpose:	Invalidate buffer cache
 * 		- must start on 16 byte boundary
 * 		- must "own" to next higher 16 byte boundary from end of buffer
 *      NOTE: can be 32 bytes in cache line
/ ***********************************************************************/
_INLINE_ void 
InvalidateBuffer( void * bufaddr, UINT length)
{
  // by computed goto would be optimized - for now call routine
  UINT32 end; 

  end = ((ULONG) bufaddr + length + L1_CACHE_BYTES) & ~(L1_CACHE_BYTES -1);
  bufaddr = (void *) ((UINT) bufaddr & ~(L1_CACHE_BYTES - 1));
  while ( (ULONG) bufaddr < end )
  {
    InvalidateCacheLine( (UINT32) bufaddr );
    bufaddr += L1_CACHE_BYTES;
  }
  return;
}
/************************************************************************
 * Function Name:	FlushCache
 *
 * Purpose:	Ensure no outstanding writes
 *		- we are using write through - I'm not even sure this is 
 *		required, but ...MaTed---
 ***********************************************************************/
//static _INLINE_ void 
void 
FlushCache(void)
{
#define	TEST_WRITE_FLUSH 0
#if TEST_WRITE_FLUSH
  unsigned long xflags;

  UINT32	* ad1, * ad2, * ad3, * ad4;		// address to mod / check
  /* NOTE: original test had 4096 as array sizes - kills kernel, but is the 
	 proper test ...MaTed */
  UINT32	val1[100], val2[100], val3[100], val4[100];
  UINT32	sav1, sav2, sav3, sav4;

  // NOTE: this test cannot execute from inside interrupt routine
  local_irq_save(xflags);
  ad1 = val1;
  ad2 = val2;
  ad3 = val3;
  ad4 = val4;

  sav1 = *ad1;
  sav2 = *ad2;
  sav3 = *ad3;
  sav4 = *ad4;

  *((UINT32 *) (SYSCFG)) &= ~WriteBufferEn;	// bit off

  ASSERT( sav1 == *ad1);
  ASSERT( sav2 == *ad2);
  ASSERT( sav3 == *ad3);
  ASSERT( sav4 == *ad4);

  *((UINT32 *) (SYSCFG)) |= WriteBufferEn;	// bit on

  ASSERT( sav1 == *ad1);
  ASSERT( sav2 == *ad2);
  ASSERT( sav3 == *ad3);
  ASSERT( sav4 == *ad4);

  local_irq_restore(xflags);
#else
  *((UINT32 *) (SYSCFG)) &= ~WriteBufferEn;	// bit off
  *((UINT32 *) (SYSCFG)) |= WriteBufferEn;	// bit on

#endif
}
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
s3c_probe( ND dev)
{
  SMEM_CHECK("****** S3C probe ***:");
  // check if all allocated
  if (numS3cMacs >= ETH_MAXUNITS)
	return (-ENODEV);
  // SET_MODULE_OWNER( dev);
  dev->irq = BDMA_RX_IRQ;				// just put in 1 of four
  dev->base_addr = (UINT32) baseAddr;	// just put start base addr

  if ( check_mem_region( dev->base_addr, MAC_IO_EXTENT))
    return (-ENODEV);
  if ( s3c_probe1( dev) == S3C_SUCCESS)
  {
	numS3cMacs++;					// increment the number of s3cs found
	SMEM_CHECK("S3C Leave probe:");
    return S3C_SUCCESS;
  }
  return (-ENODEV);
}
/************************************************************************
 * This routine initializes all the vars in dev and private areas
 ************************************************************************/
static int
s3c_InitVars( ND dev)
{
  S3C_MAC * priv = dev->priv;			// initializtion will be overwritten 1st time

  //  SMEM_CHECK("****** S3C Entering InitVars ***:");
  /* Initialize the device structure. */
  if ( dev->priv == NULL)
  {
    dev->priv = kmalloc( sizeof( S3C_MAC), GFP_KERNEL);
    if ( dev->priv == NULL)
      return ( -ENOMEM);
#if DEBUG_INCTXPTR
	PrivPtr = (S3C_MAC *) dev->priv;
#endif
    s3cInitPriv( dev);	// initialize the Mac Private structure
    priv = (S3C_MAC *) dev->priv;
    spin_lock_init( &priv->lock);
  }
  dev->open				= &s3c_open;
  dev->stop				= &s3c_close;
  dev->hard_start_xmit	= &s3c_send_packet;
  dev->get_stats		= &s3c_get_stats;
  dev->set_multicast_list = &set_multicast_list;

  dev->tx_timeout		= &s3c_tx_timeout;
  dev->watchdog_timeo	= MY_TX_TIMEOUT; 
  dev->set_mac_address	= &SetMacAddr;
  /*
   * Fill in the rest of the fields of the device structure 
   * with ethernet values. 
   */
#if	ENABLE_DESTRUCTOR 
  priv->destructor 	= &s3cDestructor;
#endif
  /*
   * The following can be implemented but hasn't been
   */
  //  dev->do_ioctl		= &s3c_do_ioctl;
  SMEM_CHECK("****** S3C Exiting InitVars ***:");
  return ( S3C_SUCCESS);
}
/************************************************************************
 * This is the real probe routine. Linux has a history of friendly device
 * probes on the ISA bus. A good device probes avoids doing writes, and
 * verifies that the correct device exists and functions.
 * -- This version is hard codedto just find the one hard coded address
 *		It will only be called once.
 * NOTE: now gets HW address from external routine. If there is no
 *  bootloader, then this routine must still return a HW address ...MaTed ---
 ************************************************************************/
static int __init 
s3c_probe1( ND dev)
{
  int 	    i;
  static unsigned version_printed = 0;
  char *	ptr;
  int 		rc;		//return code
  S3C_MAC *	priv;

  if (net_debug  &&  version_printed++ == 0)
    printk( KERN_INFO "%s", version);

  printk( KERN_INFO "\n%s: %s found at %08x, ", dev->name, cardname, 
		  (UINT) baseAddr);
  /* Fill in the 'dev' fields. */

  ptr = get_MAC_address("dev0");

  memcpy( dev->dev_addr, ptr, NUM_HW_ADDR);
  for ( i = 0; i < NUM_HW_ADDR; i++)
  {
    printk("%02x.", dev->dev_addr[ i]);
  }
  printk( "\n");
  dev->addr_len = NUM_HW_ADDR;
  if ( s3c_request_irq_mem( dev))
  {
	return ( -EAGAIN);
  }
  Reset( );			// Force initial conditions
  DisableInt( );
  ether_setup( dev);
  DisableInt( );
  SetupRegistersTx( );
  SMEM_CHECK("****** S3C-between setupTx and Rx Reg ***:");
  SetupRegistersRx( );
  SMEM_CHECK("****** S3C-after Setup Rx registers ***:");
  DisableTxRx( );
  DisableInt( );
  //  return( s3c_InitVars( dev));
  if (S3C_SUCCESS != (rc = s3c_InitVars( dev)))
	return( rc);
  // might be a bit redundant with the initial probe -- But ...
  // Zero out the stats
  priv = dev->priv;		// wasn't init'd before this
  memset( &priv->stats, 0, sizeof( struct net_device_stats ));
  return( S3C_SUCCESS );
}
/*******************************************
 * Blocks the multiple requests to set IRQ
 *******************************************/
static int	run_request = 0;
/**********************************************************************
 * s3c_request_irq_mem - routine to set the interrupt vectors and mem
 **********************************************************************/
static _INLINE_ int
s3c_request_irq_mem( ND dev)
{
  int		 	rc;

  if (run_request)
	return (S3C_SUCCESS);
  /*
   * The ethernet MACS have fixed interrupts, allocate the interrupt
   * vector now. There is no point in waiting since no other device
   * can use the interrupt, and this marks the irq as busy. Jumpered
   * interrupts are typically not reported by the boards. MAC 0 is 
   * fixed at IRQ 10, MAC 1 is fixed IRQ11
   *
   * The S3C4530A allocates 4 interrupts to the MAC, do all four
   */

#if !NO_INCLUDE_MAC_RX_INT		// skip this one - useless ...MaTed---
  if ( (rc = request_irq( MAC_RX_IRQ, &s3c_MacRx, 0, cardname, dev)))
  {
    printk( KERN_WARNING "%s: unable to get IRQ %d (irqval=%d).\n",
	    dev->name, MAC_RX_IRQ, rc);
    return ( -EAGAIN);
  }
#endif
  if ( (rc = request_irq( MAC_TX_IRQ, &s3c_MacTx, 0, cardname, dev)))
  {
    printk( KERN_WARNING "%s: unable to get IRQ %d (irqval=%d).\n",
	    dev->name, MAC_TX_IRQ, rc);
#if !NO_INCLUDE_MAC_RX_INT		// skip this one - useless ...MaTed---
	free_irq( MAC_RX_IRQ, dev);
#endif
    return ( -EAGAIN);
  }
  if ( (rc = request_irq( BDMA_RX_IRQ, &s3c_BdmaRx, 0, cardname, dev)))
  {
    printk( KERN_WARNING "%s: unable to get IRQ %d (irqval=%d).\n",
	    dev->name, BDMA_RX_IRQ, rc);
#if !NO_INCLUDE_MAC_RX_INT		// skip this one - useless ...MaTed---
	free_irq( MAC_RX_IRQ, dev);
#endif
	free_irq( MAC_TX_IRQ, dev);
    return ( -EAGAIN);
  }
  if ( (rc = request_irq( BDMA_TX_IRQ, &s3c_BdmaTx, 0, cardname, dev)))
  {
    printk( KERN_WARNING "%s: unable to get IRQ %d (irqval=%d).\n",
	    dev->name, BDMA_TX_IRQ, rc);
#if !NO_INCLUDE_MAC_RX_INT		// skip this one - useless ...MaTed---
	free_irq( MAC_RX_IRQ, dev);
#endif
	free_irq( MAC_TX_IRQ, dev);
	free_irq( BDMA_RX_IRQ, dev);
    return ( -EAGAIN);
  }
  /* Grab the region so that no one else tries to probe our ioports. */
  if (! request_mem_region( (UINT32) baseAddr, sizeof(CSR), cardname))
  {
    printk( KERN_WARNING "%s: unable to get memory/io address region %x\n",
	    dev->name, (UINT) baseAddr);
#if !NO_INCLUDE_MAC_RX_INT		// skip this one - useless ...MaTed---
	free_irq( MAC_RX_IRQ, dev);
#endif
	free_irq( MAC_TX_IRQ, dev);
	free_irq( BDMA_RX_IRQ, dev);
	free_irq( BDMA_TX_IRQ, dev);
    return (-EAGAIN);
  }
  run_request = 1;
  return (rc);
}
/**********************************************************************
 * s3c_release_irq_mem - routine to release the interrupt vectors and mem
 **********************************************************************/
static _INLINE_ int
s3c_release_irq_mem( ND dev)
{
  if (run_request == 0)	/* did we allocate them yet */
	return( S3C_SUCCESS);

  /* release all the interrupts */
#if !NO_INCLUDE_MAC_RX_INT
  free_irq( MAC_RX_IRQ, dev);
#endif
  free_irq( MAC_TX_IRQ, dev);
  free_irq( BDMA_RX_IRQ, dev);
  free_irq( BDMA_TX_IRQ, dev);

  /* Release the region so that some else (including ourselves) can have it */
  release_mem_region( (UINT32) baseAddr, sizeof(CSR) );

  run_request = 0;	/* someone else can use it now */
  return ( S3C_SUCCESS);
}
/***********************************************************************
 * log transmit MAC errors - status register is the same as the
 *		status in the FD block - juse the FD.status (multiple packets)
 *	returns S3C_FAIL if error
 *			S3C_SUCCESS if status did not show error
 ***********************************************************************/
static _INLINE_ int 
 s3cLogTxMACError( ND dev, int status)
{
  S3C_MAC * 	priv = dev->priv;

  priv->stats.collisions += status & TxCollCnt_mask;
  if (0 == (status 
	    & (TxExColl + TxLateColl + TxDefer + TxPaused + TxMaxDefer + 
		   TxHalted + TxNCarr + TxPar + TxUnder + TxSQErr)))
  {
    return ( S3C_SUCCESS);
  }
  priv->stats.tx_errors++;				// we have an error
  // now log specific error
  if ( status & (TxExColl))
	   priv->stats.collisions += 16;	// at least 16 collisions occurred
  if ( status & (TxMaxDefer + TxPaused + TxHalted + TxExColl + TxHalted))
    priv->stats.tx_aborted_errors++;
  if ( status & (TxLateColl))
    priv->stats.tx_window_errors++;
  if ( status & TxNCarr)
    priv->stats.tx_carrier_errors++;
  if ( status & (TxPar + TxUnder))	// Underrun is here??
    priv->stats.tx_fifo_errors++;
  if ( status & (TxSQErr + TxDefer))
    priv->stats.tx_heartbeat_errors++;
#if 0	// there are circumstances when the  transmitter has to be restarted
  if ( status & (TX_PARITY + ???))
	// restart transmitter
#endif
  return ( S3C_FAIL);
}
/***********************************************************************
 * log receive BDMA errors
 *	returns S3C_FAIL if error
 *			S3C_SUCCESS if status did not show error
 * - status passed should be from the F Descriptor
 ***********************************************************************/
static _INLINE_ int 
s3cLogRxBdmaError( ND dev, int status)
{
  S3C_MAC * 	priv = dev->priv;


#if 0
  if ( 0 != status & RxSGood)
#else   // Alternately we could check the errors
  if (0 == (status 
	    & (RxS_OvMax + RxS_AlignErr + RxS_CrcErr + RxS_Overflow +
		   RxS_LongErr + RxS_RxPar + RXS_RxHalted )))
#endif
  {
    return ( S3C_SUCCESS);
  }
  priv->stats.rx_errors++;
  // now log specific errors
  if ( status & ( RxS_OvMax + RxS_LongErr ))
    priv->stats.rx_length_errors++;
#if 0
// NOTE: this is an error which should get updated in the RX BDMA interrupt
  if ( status & ())
    priv->stats.rx_over_errors++;
#endif
  if ( status & ( RxS_CrcErr ))
    priv->stats.rx_crc_errors++;
  if ( status & ( RxS_AlignErr ))
    priv->stats.rx_frame_errors++;
  if ( status & ( RxS_Overflow + RxS_RxPar ))
    priv->stats.rx_fifo_errors++;
  if ( status & ( RXS_RxHalted ))	// may be other updaters to this
    priv->stats.rx_missed_errors++;
  return ( S3C_FAIL);
}
/***********************************************************************
 * Transmit timeout
 *	Kernel calls this routine when a transmit has timed out
 ***********************************************************************/
static void 
s3c_tx_timeout( ND dev)
{
  S3C_MAC *		priv = dev->priv;
  //  UINT32		basePort = dev->base_addr;
  UINT32		MacStatus  = ReadCSR( MacTxStat);

#if (NET_DEBUG > 3)
  UINT32		BDMAStatus  = ReadCSR( BdmaStat);

  printk( KERN_ERR "%s: transmit timed out: NUM= %ld, MacTXStatus=0x%08lX ",
		  dev->name, priv->qTx.num, MacStatus );
  printk( KERN_ERR "MacBDMAStatus=0x%08lX ", BDMAStatus );

#endif
  /********************************************************
NOTE: This section needs a lot more work ...MaTed--- FIXME
  ********************************************************/
  // We are going to reset the MAC
  //  - all packets - both rx and tx may be discarded
  //  - have to change logic to allow errors to stay around

  priv->stats.tx_errors++;
  (void) s3cLogTxMACError( dev, MacStatus);
  //  (void) s3cLogTxBDMAError( dev, BDMAStatus);

  // FIXME ...MaTed--- the close  / open has not been checked
  s3c_close( dev);

  s3c_open( dev);
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
s3cInitPriv( ND dev )
{
  S3C_MAC	* priv 	= dev->priv;
  RET_CODE	  rc;

  memset( priv, 0, sizeof( S3C_MAC));
  
  /* setup the MAC HW address */
  SetUpARC( dev, dev->dev_addr);

  // **Enhancement FIXME-pickup duplex and speed from commandline ...MaTed---
  // - if you want to force, userland force later
  // priv->phySpeed = 2;		// Force 100
  // priv->phySpeed = 0;		// automatic MII mode force speed = 10
  priv->phySpeed = 3;			// automatic MII mode 
  //  priv->phySpeed = 1;		// Force 10 Mb/s endec
  WriteCSR( MacCtl, (ReadCSR( MacCtl) & ~(MCtlMII_Off + MCtlLoop10))
			| (priv->phySpeed << MCtl10Shift1)
			| (priv->phySpeed << MCtl10Shift2));

  /* Mac CNTL Full Duplex */
  priv->phyDpx = 1;				/* Full Duplex */
  //priv->phyDpx = 0;		/* Half Duplex, if MII_Conn is High?? */
  WriteCSR( MacCtl, (ReadCSR( MacCtl) & ~ MCtlFullDup )
			| priv->phyDpx << MCtlFullDupShift);
  /*
   * NOW set up the S3c MAC struct
   */
  if ((rc = SetupTxFDs( dev)))
    return (rc);
  SMEM_CHECK("****** S3C Between SetupTxFDs and RxFDs ***:");
  if ((rc = SetupRxFDs( dev)))
    return (rc);
  // no error
  return ( S3C_SUCCESS);
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
s3c_open( ND dev)
{
  //  S3C_MAC *priv = (S3C_MAC *) dev->priv;
  //  UINT		basePort = dev->base_addr;
  RET_CODE	rc;

#if 0	// why doesn't this get called during ifconfig ....MaTed---
  printk( KERN_INFO "\n******** s3c_open called ********** \n");
#endif
#if 0
  // moved this section from the probe routine
  Reset( );			// Force initial conditions
  DisableInt( );
  ether_setup( dev);
  DisableInt( );
  SetupRegistersTx( );
  SMEM_CHECK("****** S3C-between setupTx and Rx Reg ***:");
  SetupRegistersRx( );
  SMEM_CHECK("****** S3C-after Setup Rx registers ***:");
  DisableTxRx( );
  DisableInt( );
  s3c_InitVars( dev);
#endif
  //  SetupRegistersTx( );
  //SetupRegistersRx( );
  //DisableTxRx( );
  //DisableInt( );
  /*
   * This is used if the interrupt line can turned off (shared).
   * - Can we share? - might as well register
   */
  //  if ( s3c_request_irq_mem( dev))
  rc = s3c_request_irq_mem( dev);
  TDEBUG("S3C_OPEN: request_irq_mem rc=%d\n", rc);
  if (rc)
  {
	printk( KERN_ERR "S3C4530: Cannot get irq/mem rc=%d\n", rc);
    return -EAGAIN;
  }

  /*
   * Always allocate the DMA channel after the IRQ,
   * and clean up on failure. NO DMA used
   */
  //  Reset( dev);		// reset the MAC
  // We are leaving priv intact
  //  if (S3C_SUCCESS != (rc = s3c_InitVars( dev)))
  //    return (rc);
  /* Reset the hardware here. Don't forget to set the station address. */
  if (S3C_SUCCESS != (rc = TxRxInit( dev)))
  {
	printk( KERN_ERR "S3C4530: Phys init failed \n");
	return (rc);
  }
  // The following is done in TXRXInit
  //  SetupRegistersTx( dev);
  //  SetupRegistersRx( dev);
  //  DisableTxRx( dev->base_addr);
  //  SetUpTxFrmPtrRegister( dev);
  //  PhyInit( dev);
  // let the controller go again

  EnableTxRx( );
  EnableInt( );

  //  priv->open_time = jiffies;
  /* We are now ready to accept transmit requeusts from
   * the queueing layer of the networking.
   */
  netif_start_queue( dev);

  return S3C_SUCCESS;
}
/************************************************************************
 This will only be invoked if your driver is _not_ in XOFF state.
 * What this means is that you need not check it, and that this
 * invariant will hold if you make sure that the netif_*_queue()
 * calls are done at the proper times.
 ************************************************************************/
static int s3c_send_packet( struct sk_buff *skb, ND dev)
{
  S3C_MAC 		  *	priv = dev->priv;
  //  int 				basePort = dev->base_addr;
  //  short			length;
  UCHAR		   	  *	buf = skb->data;
  BPTR *			bptr = &priv->qTx;
  TxFDS *			fdbdPtr;
#if (NET_DEBUG > 4)
  TxFDS *			tempPtr;
#endif
  UINT32			temp;

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
#if (NET_DEBUG > 4)
  /* Paranoid check to see if there is room - by pointers */
  /* ?? I don't need this inside */
  if (bptr->first == ((TxFDS *) (bptr->last))->fd.FDNext)
  {
    printk( KERN_ERR "TX overflow on %s\n", dev->name);
	// MAKE Sure transmitter was enabled -- maybe that will clear it up 
	if (! ((temp = ReadCSR( TxCtl)) & BTxEN))
	  WriteCSR( TxCtl, temp | BTxEN);
	WriteCSR( MacTxCtl, ReadCSR( MacTxCtl) | MTxCtlTxEn);
    return ( -1);
  }
#endif
#if (NET_DEBUG > 4)
  /* another paranoid check (next should be owned by system */
  tempPtr = (TxFDS *) (((TxFDS *) bptr->last)->fd.FDNext);
  if (((UINT32) ((TxFDS *) UnCache((UINT32)tempPtr))->fd.FDNext) & OWNERSHIP)
  {
    printk( KERN_ERR "TX ownership problem on %s\n", dev->name);

    return (-1);
  }
#endif
#if (NET_DEBUG > 5)
  printk( KERN_ERR "S3C_SEND: 0%08X Skb length %d\n", 
		  (unsigned int) dev->base_addr, (unsigned int) skb->len);
  //  {
	//	int i;
	//	printk( KERN_ERR "S3C_SEND: Skb length %d\n - data=", skb->len);
	//	for ( i = 0; i < skb->len; i++)
	//	{
	//	  printk( KERN_ERR "[%02x] ", skb->data[ i]);
	//	}
	//	printk( KERN_ERR "\n");
  //  }
#endif
#if (NET_DEBUG > 3)
  ASSERT( atomic_read( &skb->users) == 1);
#endif
#if 0	//we don't invalidate the cache for writes only for reads
  InvalidateBuffer( buf, skb->len);
#endif
  /* This is the most common case for modern hardware.
   * The spinlock protects this code from the TX complete
   * hardware interrupt handler.  Queue flow control is
   * thus managed under this lock as well.
   */
  spin_lock_irq( &priv->lock);

  fdbdPtr = (TxFDS *) bptr->last;
  // move to next buffer in list
  bptr->last = (void *) ((UINT32)(fdbdPtr->fd.FDNext));

  fdbdPtr->fd.FDLength = skb->len;

  fdbdPtr->fd.FDModes	= TxWidSetup;		// set widget bits (pg 7-18)
  //fdbdPtr->fd.FDModes = TxWidIE | TxWidLITTLE | TxWidINC;

  fdbdPtr->skb = skb;
#if (NET_DEBUG > 3)
  ASSERT( atomic_read( &skb->users) == 1);
#endif
  bptr->num++;				// added one buffer  

  // Pass Control and buffer address
  fdbdPtr->fd.FDDataPtr = (UCHAR *) ((((UINT32) buf) - 2 )| OWNERSHIP);
  FlushCache();				// just in case
  // Let the MAC know there is something to send (in case it stopped )
  // ...Note the check is from example code (why?), extra comp is for compliler
  if (! ((temp = ReadCSR( TxCtl)) & BTxEN))
	WriteCSR( TxCtl, temp | BTxEN);
  WriteCSR( MacTxCtl, ReadCSR( MacTxCtl) | MTxCtlTxEn);
  dev->trans_start = jiffies;

  TxPktCompletion( dev );

  /* If we just used up the very last entry in the
   * TX ring on this device, tell the queueing
   * layer to send no more.
   */
  if (txFull( dev))
  {
    netif_stop_queue( dev);
	// turn on tx completion interrupt
	WriteCSR( MacTxCtl, ReadCSR( MacTxCtl ) | MTxCtlEnComp );
  }

  /* When the TX completion hw interrupt arrives, this
   * is when the transmit statistics are updated.
   */
  spin_unlock_irq( &priv->lock);

  return 0;
}
/************************************************************************
 * This handles TX complete events posted by the MAC portion of the
 * device via interrupts.
 ************************************************************************/
void s3c_MacTx( int irq, void *dev_id, struct pt_regs * regs)
{
  ND 				dev = dev_id;
  S3C_MAC *			priv = dev->priv;

  /* This protects us from concurrent execution of
   * our dev->hard_start_xmit function above.
   */
  spin_lock( &priv->lock);

  TxPktCompletion( dev );

  /* If we had stopped the queue due to a "tx full"
   * condition, and space has now been made available,
   * wake up the queue.
   **** Since we alway do a netif_wake_queue in the interrupt routine
   ****  whats this for??? ...MaTed--- (it's in the skeleton)
   */
#if 1 	// since we always call netif_wake_queue WHY?
  if ( netif_queue_stopped( dev) && ! txFull( dev))
  {
	netif_wake_queue( dev);
	// turn off tx completion interrupt
	WriteCSR( MacTxCtl, ReadCSR( MacTxCtl ) & ~MTxCtlEnComp );
  }
#endif
  spin_unlock( &priv->lock);
  FlushCache();
}
/************************************************************************
 * Helper routine to collect stats on tx completion and release skbs
 * MUST be called with spin_lock already invoked
 ************************************************************************/
static _INLINE_ void
TxPktCompletion( ND dev)
{
  S3C_MAC *			priv = dev->priv;
  BPTR *			txPtr;
  TxFDS *			UtxPtrFirst;
  UINT32			status;
  struct sk_buff *	skb;
  UINT32			S3cTxPtr;
  UINT16	  		MissedErrorCnt;

  txPtr = &priv->qTx;
  UtxPtrFirst = (TxFDS *) (UnCache (txPtr->first));
  // set the point where the Mac transmit Pointer is at - don't try to 
  //  catch the next one "going" to happen
  S3cTxPtr = UnCache( ReadCSR( TxPtr));

  SINT(" MTx:stat", ((int)  UtxPtrFirst->fd.FDStat));
  //  SINT(" Mac Tx interrupt: TxPtr", ((int) S3cTxPtr ));
  while ((UINT32) UtxPtrFirst != S3cTxPtr)	// It's possible that there 
											//  is nothing to send
  {
	/*** Only in here if MAC processed at least one packet */
    // checks to see if there was an error as well
    status = (UINT32)  UtxPtrFirst->fd.FDStat;
	if (S3C_SUCCESS == s3cLogTxMACError( dev, status))
	{
	  skb = ((TxFDS*)(txPtr->first))->skb;
	  priv->stats.tx_bytes += skb->len;	// total bytes transmitted
	  // NOTE: only good packets were transmitted (whereas rx can get bad)
	  priv->stats.tx_packets++;			// total packets transmitted
	}
	else
	{
	  // a tx error of some kind occurred
      priv->stats.tx_errors++;	// could/should move this into routine
	  // clear EMISSCNT register , by read (example code)
	  MissedErrorCnt = ReadCSR( MissCnt);
	  SetupRegistersTx( );		// force Tx registers if error
	}
	/*
	 * The following stuff we have to do regardless if the packet
	 *	transmission had errors or not
	 */
	// update the number of collisions
	priv->stats.collisions += ReadCSR( MacTxStat) & MTxStTxColl;
	// Clearing parts of the FD structure (probably not needed ??? )
	UtxPtrFirst->fd.FDModes = 0x0;		// turn off the Mode bits
	UtxPtrFirst->fd.FDLength = 0x0;		// turn off the Length
	UtxPtrFirst->fd.FDStat = 0x0;		// turn off the Status bits
	// get pointer to skb where we saved it
	skb = UtxPtrFirst->skb;
#if (NET_DEBUG > 4)
	ASSERT( atomic_read( &skb->users) == 1);
#endif
#if (NET_DEBUG > 5)
	if (atomic_read( &skb->users) != 1)
	{
	  printk("SKB:%08x, txPtrFirst:%08x, users:%x\n",
			 (UINT32) skb, (UINT32) UtxPtrFirst, (UINT32) skb->users);
	}
#endif
	dev_kfree_skb_irq (skb);		// release it regardless
	// Move on to the next in the list
	txPtr->first = (void *) ((TxFD *)(txPtr->first))->FDNext;
	FlushCache();
	UtxPtrFirst = (TxFDS *) (UNCACHE_ADDR | (UINT32) (txPtr->first));
	txPtr->num--;
  }
}
/************************************************************************
 * This handles TX complete events posted by the BDMA portion of the
 * device via interrupts.
 * - includes the following
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
 *	- Null List Interrupt
 *		- this is bad - should never happen
 *		- enabled as debug tool
 *	- Tx Buffer Empty
 *		- leaving this off too
 ************************************************************************/
void s3c_BdmaTx( int irq, void *dev_id, struct pt_regs * regs)
{
  ND 		dev = dev_id;
  S3C_MAC *	priv = dev->priv;
  UINT32	status;

  SINT("BTx:Stat", ((int) ReadCSR(BdmaStat)) );
  CHECK_ITXPTR("EnableInt");
  /* This protects us from concurrent execution of
   * our dev->hard_start_xmit function above.
   */
  spin_lock( &priv->lock);

  // checks to see if there was an error as well
  status = ReadCSR( BdmaStat);
  WriteCSR( BdmaStat, status & 0xFFFF0000);	// From example code (clear all)
  if ( status & BTxEmpty )
  {
	printk( "%s: Invalid interrupt! Transmit Buffer Empty \n", dev->name);
  } else if (status & BTxNO )
  {
	printk( "%s: Invalid interrupt! No owner for TX buffer [0x%08x]\n",
			dev->name, ((int) ReadCSR( TxPtr)));
  } else if (status & BTxNL )
  {
	printk( "%s: Invalid interrupt! Transmit Null List \n", dev->name);
  } else if (status & BTxCCP )
  {
	printk( "%s: Invalid interrupt! Control Packet Sent \n", dev->name);
  }
  spin_unlock( &priv->lock);
}
/************************************************************************
 * We have a good packet(s), get it/them out of the buffers.
 * - we take both BL and FDA from the MAC.first
 *   - move them to the front.last
 *	 - buffer is stripped off, added to skb head, skb address stored in ??
 *	- continue while MAC.first != MAC.last
 * 	- other things to check / do
 *		- ensure all four entries are set to SYS_OWNS_xx
 ************************************************************************/
#if !NO_INCLUDE_MAC_RX_INT
static void
s3c_MacRx( int irq, void *dev_id, struct pt_regs * regs)
{
  ND 				dev 	= dev_id;
  UINT				status;

  status = ReadCSR( MacRxStat);
  SINT("MRx: Stat", ((int) status) );

  if (status & MRxStGood)
	return;

  if ( status & (MRxStAlignErr | MRxStCRCErr | MRxStOverflow
				 | MRxStLongErr | MRxStRxPar | MRxStRxHalted))
  {
#if NET_DEBUG
	printk( "%s: MAC Rx Error: 0x%x\n", dev->name, status);
#endif
  }
  return;
}
#endif
/************************************************************************
 * BDMA receive interrupt - special conditions
 ************************************************************************/
static void
s3c_BdmaRx( int irq, void *dev_id, struct pt_regs * regs)
{
  ND		dev 	= dev_id;
  S3C_MAC *	priv 	= (S3C_MAC *)dev->priv;
  UINT32	status;
  UINT32	bdmaStatus;

  UINT 				boguscount = RX_NUM_FD / 4;	// set to one quarter 
												//  of buffers - was 10
  BPTR *			Rx			= &priv->qRx;	// is & extraneous?
  BPTR *			RxWait		= &priv->qRxWait;
  struct sk_buff *	skb			= NULL;	 // initialize just to get rid 
										//  of warning
  UINT				len;
  UINT32			MacRxPtr;
  RxFD *			nextFirstPtr;
  UINT32			temp;

  // Rx First
#define FdFirst 	((RxFDS *) ( Rx->first))
  // Rx Last
#define FdLast 		((RxFDS *) ( Rx->last))
  // Wait first - when skb cannot be filled now
#define FdWaitFirst ((RxFDS *) ( RxWait->first))
  // Wait last
#define FdWaitLast 	((RxFDS *) ( RxWait->last))

  // UnCached versions
  // Rx First
#define UFdFirst 		((RxFDS *) ( UnCache( (UINT32) Rx->first)))
  // Rx Last
#define UFdLast 		((RxFDS *) ( UnCache( (UINT32) Rx->last)))
  // Wait first - when skb cannot be filled now
#define UFdWaitFirst 	((RxFDS *) ( UnCache( (UINT32) RxWait->first)))
  // Wait last
#define UFdWaitLast 	((RxFDS *) ( UnCache( (UINT32) RxWait->last)))

  SINT("BRx: Stat", ((int) ReadCSR( BdmaStat)) );
  /* This protects us from concurrent execution of
   * our dev->hard_start_xmit function above.
   */
  spin_lock( &priv->lock);
  MacRxPtr = UnCache( ReadCSR( RxPtr));
  bdmaStatus = ReadCSR( BdmaStat);

  //  SINT(" Mac Rx interrupt: First Stat", ((int) UFdFirst->fd.FDStat));

  while (MacRxPtr != (UINT32) UFdFirst)
  {
	// checks to see if there was an error as well
	if ( bdmaStatus & BRxNL )
    {
	  printk( "%s: Invalid interrupt! Rx dma Pointer is NULL\n", dev->name);
	  WriteCSR( BdmaStat, bdmaStatus | BRxNL);	// clear status register

	  // Serious error -- needs a few things reset 
	  // -- Is this too much though ....MaTed---
	  TxRxInit( dev);
	  EnableTxRx( );
	  EnableInt( );
	  break;
	} else if (bdmaStatus & BRxNO )
	{
	  printk( "%s: Invalid Interrupt! Not owner of RX buffer \n", dev->name);
	  WriteCSR( BdmaStat, bdmaStatus | BRxNO);	// clear status register
	  SetupRegistersRx( );	// Is this enough to recover ...MaTed---
	  break;
	} else if (bdmaStatus & BRxMSO )
    {
	  printk( "%s: Invalid interrupt! Rx frame bigger than buffer\n", 
			  dev->name);
	  WriteCSR( BdmaStat, bdmaStatus | BRxMSO);	// clear status register
	  // still have to do something to the data structures
	  //  I will assume that some other error in the FD status will flag
#if 0
	} else if (bdmaStatus & BRxStEmpty )
    {
	  printk( "%s: Invalid interrupt! Rx Buffer Empty\n", dev->name);
#endif
	}
	// NOW we deal with the FD stuff
    status = UFdFirst->fd.FDStat;
    len = UFdFirst->fd.FDLength;

	if (len == 0)
	{ // This should not happen
#if (NET_DEBUG >3)
	  printk( "%s: rx'd ZERO length packet... Skipped\n", dev->name);
#endif
	  status |= RxS_LongErr;	// forces length stat to increment
		// No break - treat as error in packet
		//break;
	}
#if 1		// Sanity check -- can probably turn this off ...MaTed---
	if ( ((UINT32) (UFdFirst->fd.FDDataPtr)) & OWNERSHIP)
	{ // This should not happen
#if (NET_DEBUG >3)
	  printk( "%s: Encountered rx packet not owned\n", dev->name);
#endif
	  break;
	}
#endif	// 1 - Sanity check

	nextFirstPtr = (RxFD *) (FdFirst->fd.FDNext);	// save the new First
	if (S3C_SUCCESS == s3cLogRxBdmaError( dev, status))
    {
	  skb = UFdFirst->skb;
      priv->stats.rx_bytes += len;
	  priv->stats.rx_packets++;
	  // update lengths 
	  skb->tail += len;
	  skb->len += len;
	  InvalidateBuffer( skb->data, len);// want the real data
	  skb->protocol = eth_type_trans( skb, dev);
      netif_rx( skb);					// give the SKB to the kernel

	  // Now move Frame Descriptor to the front of the WAIT list
	  // This does the first half of pulling it out of the circular queue
	  //	by putting the buffer on the wait list
	  (RxFDS *) (FdFirst)->fd.FDNext = FdWaitFirst;
	  FdWaitFirst = FdFirst;
	  
	  RxWait->num ++;		// keep track that we added it to the wait list
	  Rx->num --;			// will be incremented in destructor
	  //	  FlushCache();			// Force out to memory

	}
	else
	{   // On error we'll stay with the original skb
	  // we had an error in the packet, we're keeping the skb
	  FdFirst->fd.FDLength = PKT_BUF_SZ;	// reset in case
	  // Want to put this one at the end of the list (obtuse reasons)
	  FdFirst->fd.FDNext = NULL;	// last one can has zero for next
									//	to stop BDMA engine
	  FdFirst->fd.FDDataPtr = (UCHAR*) ((UINT32) (FdLast->fd.FDDataPtr) 
									   & ~OWNERSHIP);
	  // Link first to the one last one - link was already to here (should
	  //  have been to here anyway (mind skb Memory Squeeze)
	  // Give the FDA back to the MAC
	  // First check if the last one in the list was still ours
	  ASSERT( 0 == (OWNERSHIP & (UINT32) (FdLast->fd.FDDataPtr)));

	  // Last thing to do (just in case) is to link to the end
	  ((RxFD *) FdLast)->FDNext = (RxFD *) (FdFirst);
	  // Give ownership to MAC / BDMA of the FD
 	  FdLast->fd.FDDataPtr = (UCHAR*) ((UINT32) (FdLast->fd.FDDataPtr) 
									   |= OWNERSHIP);
	  FdLast = FdFirst;			// moved first to back
	}
	// advance the first
	FdFirst = nextFirstPtr;		// save the new First
	// have to clear to get next receive ??required?? ...MaTed---
	WriteCSR( BdmaStat, ReadCSR( BdmaStat) & ~BRxRDF);
	FlushCache();
	// if there are no buffers left, we're done anyways (last one is marker)
	if (FdFirst->fd.FDDataPtr == NULL)
	  break;
	if (!boguscount--)
	  break;	// don't want to spend too much time in ISR
  }
#if !ENABLE_DESTRUCTOR
  // Note try playing with the MinIn parm (1 is for debugging)
  s3cAddRxSKBs( dev, RX_NUM_FD / 4, 1);		// put back SKBs into rx Q
#endif

  // have to clear to get next receive
  WriteCSR( BdmaStat, ReadCSR( BdmaStat) | bdmaStatus);

  if ( !( BRxEN & (temp = ReadCSR( RxCtl))))
	WriteCSR( RxCtl, temp  | BRxEN);

  if ( !( MRxCtlRxEn & (temp = ReadCSR( MacRxCtl))))
	WriteCSR( MacRxCtl, temp  | MRxCtlRxEn);
	
  spin_unlock( &priv->lock);		// Again do I need this ???
  return;
}
/************************************************************************
 * This routine adds as many SKBs as the Rx Wait queue requires IF they are
 *	available.
 * The number of SKBs added depends on the qRxWait.num - number of entries
 * 	in the wait queue. Wait queue is filled with skbs, setup and then 
 *	reattached to the Rx Queue. We assign a maximum of MaxIn skb's
 *	MinIn defines how many have to be on the Wait Queue before we cycle
 *	through to fill them
 *
 * NOTE: special handling required if all Rx queue has been depleted
 *	
 * NOTE: If called in an interrupt context, the caller must own the
 *	priv->lock. If NOT an interrupt, the priv-> must not be owned ...MaTed---
 ************************************************************************/
static void
s3cAddRxSKBs( ND dev, UINT32 MaxIn, UINT32 MinIn )
{
  S3C_MAC *	priv 	= (S3C_MAC *)dev->priv;
  BPTR	  * Rx		= &priv->qRx;
  BPTR	  * RxWait	= &priv->qRxWait;
  //  UINT32	SaveNum	= Rx->num;
  UINT32	numXfer = 0;
  RxFDS *	tempPtr;
  RxFDS *	nextTempPtr	= NULL;
  struct sk_buff *	skb;

  // Rx First
#define First 	((RxFDS *) ( Rx->first))
  // Rx Last
#define Last 	((RxFDS *) ( Rx->last))
  // After Last
#define After	((RxFDS *) (Last->fd.FDNext))
  // Wait first - when skb cannot be filled now
#define FdWaitFirst ((RxFDS *) ( RxWait->first))
  // Wait last
#define FdWaitLast 	((RxFDS *) ( RxWait->last))

  if (RxWait->num < MinIn)
	return;

  // Move spinlock outside of the loop. Needs algorithm change and
  //  much care to put it back inside the loop - may require mutiple
  //  lock / unlock sequences ...MaTed---
  if ( !in_irq())
	spin_lock_irq( &priv->lock);		// Lock it down
  tempPtr = FdWaitFirst;	// keep track of the Head of the list
  while (RxWait->num && MaxIn --)
  {
	// get a newr skb to fill waiting head of wait list
	/* Malloc up new skb header. Let's leave the headroom */
	skb = dev_alloc_skb( PKT_BUF_SZ);      
	if (skb == NULL)
	{
	  printk(KERN_NOTICE "%s: Memory squeeze.\n", dev->name);
	  break;	// add it next time
	}
	InvalidateBuffer( (void *) skb->data, PKT_BUF_SZ);
	skb->dev = dev;
#if ENABLE_DESTRUCTOR
	skb->destructor = priv->destructor;
#endif
	// NOTE: Assumption made that b31 of skb->data is 0 *********
	FdWaitFirst->fd.FDDataPtr
	  = (UCHAR *) ((UINT32) (skb->data) | OWNERSHIP);	// BDMA owns
	// reserve after saving the buffer address
	// NOTE Should combine this reserve with the dev_alloc_skb ->alloc_skb
	//  - for efficiency only, but skb->data would have to be corrected
	skb_reserve( skb, 2);		// 16 byte align
	FdWaitFirst->skb = skb;			// save for later reception
	FdWaitFirst->fd.FDLength = PKT_BUF_SZ;
	nextTempPtr = FdWaitFirst;
	FdWaitFirst = FdWaitFirst->fd.FDNext;
	RxWait->num --;
	numXfer ++;
  }
  // the Wait list first has advanced over the ones we moved
  // Last one attached to have a NULL Next Frame Descriptor and no Ownership
  if (nextTempPtr == NULL)	// Nothing to do if nothing was done
  {
	return;
  }
  nextTempPtr->fd.FDDataPtr
	= (UCHAR *) (~OWNERSHIP & (UINT32) (nextTempPtr->fd.FDDataPtr));
  nextTempPtr->fd.FDNext = NULL;
  // WaitFirst is pointing to the beginning of the wait queue
  // we move from nextTempPtr to tempPtr back to end of main queue
  // *** I'm not sure what the order should be for the next two instructions
  //  **  or if they should be uncached operations
  Last->fd.FDNext = (RxFD *) tempPtr;
  Last->fd.FDDataPtr =(UCHAR *) ((UINT32) ( Last->fd.FDDataPtr) | OWNERSHIP);
  Last = nextTempPtr;	// move the last pointer
  Rx->num += numXfer;	// number that we put back
  FlushCache();			// Force out to memory
  // Moved the spin lock outside the loop - need to change things to move it
  //  back in ...MaTed---
	if ( !in_irq())
	  spin_unlock_irq( &priv->lock);		// unlock it
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
dumpRx( S3C_MAC * priv, char * where )
{
  BPTR	* Rx = &priv->qRx;
  BPTR	* RxWait = &priv->qRxWait;

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
#endif
#if ENABLE_DESTRUCTOR
/************************************************************************
 * The destructor routine called when SKB is finally dead
 *  - put the buffer back onto the correct free list and 
 *    set the skb->head == 0
 *	- called indirectly through s3cDestructor0 / 1
 ****NOTE: this routine cannot be called in an interrupt context
 ************************************************************************/
static void
s3cDestructor( struct sk_buff *skb)
{
  ND		dev		= skb->dev;
  //  S3C_MAC * priv 	= dev->priv;

  SINT("Destruct: SKB", ((int) skb));

  if ( ! dev )	// check for NULL dev - why does this happen
	return;

  // The MinIn parm is just a guess - needs to be tuned ...MaTed---
  s3cAddRxSKBs( dev, RX_NUM_FD, RX_NUM_FD/16 );
}
#endif
#if 0	// old copy delete later - might want the code still
/************************************************************************
 * The destructor routine called when SKB is finally dead
 *  - put the buffer back onto the correct free list and 
 *    set the skb->head == 0
 *	- called indirectly through s3cDestructor0 / 1
 ****NOTE: this routine cannot be called in an interrupt context
 ************************************************************************/
static void
s3cDestructor( struct sk_buff *skb)
{
  ND		dev		= skb->dev;
  S3C_MAC * priv 	= dev->priv;
  BPTR	  * Rx		= &priv->qRx;
  BPTR	  * RxWait	= &priv->qRxWait;
  UINT 				boguscount = RX_NUM_FD; 	// set to # of buffers
  
  struct sk_buff	* newSkb;

  SINT("Destruct: SKB", ((int) skb));

  // Rx First
#define FdFirst 	((RxFDS *) ( Rx->first))
  // Rx Last
#define FdLast 		((RxFDS *) ( Rx->last))
  // Wait first - when skb cannot be filled now
#define FdWaitFirst ((RxFDS *) ( RxWait->first))
  // Wait last
#define FdWaitLast 	((RxFDS *) ( RxWait->last))

  // UnCached versions
  // Rx First
#define UFdFirst 		((RxFDS *) ( UnCache( (UINT32) Rx->first)))
  // Rx Last
#define UFdLast 		((RxFDS *) ( UnCache( (UINT32) Rx->last)))
  // Wait first - when skb cannot be filled now
#define UFdWaitFirst 	((RxFDS *) ( UnCache( (UINT32) RxWait->first)))
  // Wait last
#define UFdWaitLast 	((RxFDS *) ( UnCache( (UINT32) RxWait->last)))

  // Clean up the old skb so we never see it again
  //  - Already done by skb_headerinit
  //skb->dev = 0;
  //skb->destructor = NULL;

#if 0
  printk("Destruct:dev=%x\n",(int) dev);
#endif
  if ( (! dev)  || in_irq() )	// check for NULL dev - why does this happen
	return;

  // The front buffers may need skb attached back - then attached onto the
  //  active buffers in the MAC list
  while( RxWait->num)		// put all the buffers back in that we need
							//  if we can get them
  {
	// first get another skb to replace the one we gave away
	/* Malloc up new skb header. */
	newSkb = dev_alloc_skb( PKT_BUF_SZ);      
	if (newSkb == NULL)
	{
	  //  No buffers, let's get out of here
	  break;	
	}
	spin_lock_irq( &priv->lock);		// Again do I need this - I think so???
	newSkb->dev = dev;
	newSkb->destructor = priv->destructor;
	// NOTE: Assumption made that b31 of skb->data is 0 *********
	FdWaitFirst->fd.FDDataPtr = newSkb->data;
	// reserve after saving the buffer address
	skb_reserve( newSkb, 2);			// 16 byte align
	FdWaitFirst->skb = newSkb;			// save for later reception
	FdWaitFirst->fd.FDLength = PKT_BUF_SZ;
	// Put the buffer at the end of the normal list
	/******************************************************************
	* NOTE: Algorithm change - There is no EOL BIT, so we have to be
	* more careful. The BMDA engine will stop itself, we just have to 
	* make sure we don't give it back before everything is ready (last)
	* - Make corresponding change in setup routine
	*******************************************************************/
	// Link first wait to the one last normal active
	FdLast->fd.FDNext = (RxFD *) FdWaitFirst;
	// Give the FDA back to the MAC
	FdLast = FdWaitFirst;			// moved first wait to back
	FdWaitFirst = FdWaitFirst->fd.FDNext;
	//	FdWaitLast->fd.FDNext = FdWaitFirst;	// circular back to front
	FdLast->fd.FDNext = (RxFD *) FdFirst;		// make it circular
	RxWait->num--;				// keep track that we took it off 
									//	the wait list
	Rx->num++;					// and put it on ready list
	FdLast->fd.FDDataPtr = (UCHAR *) (((UINT32) (FdLast->fd.FDDataPtr)) 
									  |= OWNERSHIP);	// give back to MAC
	// have to clear to get next receive - just in case
	WriteCSR( BdmaStat, ReadCSR( BdmaStat) & ~BRxRDF);
	FlushCache();
	if (!boguscount--)
	  break;	// don't want to spend too much time in ISR
	spin_unlock_irq( &priv->lock);		// Again do I need this ???
  }
  if ( netif_queue_stopped( dev) && ! txFull( dev))
    netif_wake_queue( dev);
  return;
}
#endif
/************************************************************************
 * The inverse routine to net_open().
 ************************************************************************/
static int
s3c_close( ND dev)
{
  S3C_MAC *	priv 		= dev->priv;
  //  int 		basePort	= dev->base_addr;
  //  BPTR *	TxPtr 		= &priv->qTx;
  //  BPTR *	RxPtr		= &priv->qRx;

  //  TxFDS *	txPtrFirst;
  //  struct sk_buff *skb;

  netif_stop_queue( dev);

  /* Flush the Tx and disable Rx here. */
  DisableTxRx();
  DisableInt();
  //  Reset();				// Shut it down

  // Release all buffers we own - please no memory leaks
  // * first the TX buffers
  spin_lock_irq( &priv->lock);
#if 0		// ...MaTed--- need later for insmod, but not yet
  // We really should release the skb's -- when we get open to be called
  while (TxPtr->num != 0)	// It's possible that there is nothing
  {
	txPtrFirst = (TxFDS *) (TxPtr->first);
	// get pointer to skb where we saved it
	skb = txPtrFirst->skb;
	ASSERT( atomic_read( &skb->users) == 1);
	dev_kfree_skb (skb);		// release it
	// move to next buffer
	TxPtr->first = (void *) (((TxFDS *)(TxPtr->first))->fd.FDNext);
	TxPtr->num--;
  }

  // release all the rx skb's
  while (RxPtr->num != 0)	// It's possible that there is nothing
  {
	// release it
	dev_kfree_skb ( ((RxFDS *) (RxPtr->first))->skb);
	RxPtr->first = (void *) (((RxFDS *) (RxPtr->first))->fd.FDNext);
	RxPtr->num--;
  }
#endif
  // NOTE: do not have any skb's on the WAIT list with current algorithm

#if 0		// ...MaTed--- need later for insmod, but not yet
  /* Release the region */
  s3c_release_irq_mem( dev );
#endif

  spin_unlock_irq( &priv->lock);

#if 0		// ...MaTed--- need later for insmod, but not yet
  /* release the priv area with the buffers and all the strcutures */
  kfree( priv);
#endif

  return 0;
}
/************************************************************************
 * Get the current statistics.
 * This may be called with the card or closed.
 ************************************************************************/
static struct net_device_stats *s3c_get_stats( ND dev)
{
  S3C_MAC *priv = (S3C_MAC *)dev->priv;

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
  //  int		ioaddr = dev->base_addr;

  WriteCSR( CamEn, 0);			// turn off ARC usage for now

  if (dev->flags & IFF_PROMISC)
  {
    /* Enable promiscuous mode */
    WriteCSR( CamCtl, ReadCSR( CamCtl) | NegCAM);
  }
  else if ((dev->flags & IFF_ALLMULTI) || dev->mc_count > HW_MAX_ADDRS)
  {
    WriteCSR( CamCtl, CompEn | BroadAcc | GroupAcc);
  }
  else if (dev->mc_count)
  {
    /* Walk the address list, and load the filter */
    HardwareSetFilter( dev);
    
    WriteCSR( CamCtl, ReadCSR( CamCtl) | BroadAcc);
  }
  else // setup for normal
  {
    /* setup the MAC HW address */
    SetUpARC( dev, dev->dev_addr);

    WriteCSR( CamEn, 1 << 2);	    // Enable only the third entry
    WriteCSR( CamCtl,  (CompEn | GroupAcc | BroadAcc | StationAcc));
  }
}
/************************************************************************
 * HardwareSetFilter
 ************************************************************************/
static void HardwareSetFilter( ND dev )
{
  UINT	    i = 0;
  //  UINT32    port = dev->base_addr;
  struct    dev_mc_list * ptr = dev->mc_list;
  //  UINT32	temp;
  //  UINT16 *	addrHalfPtr;

  UINT32	enableMask = 0;

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
	printk("S3C4530: %s - MultiCast Count (%d) != pointers (%d)\n",
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
void DumpPhyReg(void)
{
  int i;
  UINT32	Array[4];

  printk( "**** Phy register dump \n");
  for (i=0; i<=32; i+=4)
  {
	(void) ReadPhy( PHYAddr, i+0, &Array[0]);
	(void) ReadPhy( PHYAddr, i+1, &Array[1]);
	(void) ReadPhy( PHYAddr, i+2, &Array[2]);
	(void) ReadPhy( PHYAddr, i+3, &Array[3]);

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
 *	-Wredundant-decls -O2 -m486 -c s3c.c.c
 *  version-control: t
 *  kept-new-versions: 5
 *  tab-width: 4
 *  c-indent-level: 2
 * End:
 */
#endif // CONFIG_SAMSUNG_S3C

#if S3C_TEMP_ROUTINES
static int get_ethernet_addr(char * MatchName, char * addr)
{
#if 1	// pull the hw eternet address from the boot loader
  char *p;
  if ((p = getbenv (ETH1_ENV_NAME)) == ((char *) 0))
  {
	//	printf ("Error: No MAC for S3C4530!\n");
	return -ENODEV;
  }
  //  printf ("S3C4530 MAC=[%s]\n", p);
  while (*p != 0)
	*addr++ = *p++;
  return (0);		// returns success
#else
  /* this dummy routine always returns a fixed IP in addr */
  addr[0] = 192;
  addr[1] = 168;
  addr[2] = 15;
  addr[3] = 2;
  return (0);		// returns success
#endif
}
#endif
