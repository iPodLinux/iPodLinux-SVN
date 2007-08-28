/***********************************************************************
 *
 *  Copyright 2003 by FS Forth-Systeme GmbH.
 *  All rights reserved.
 *
 *  $Id: ns7520_eth.c,v 1.1 2003/08/28 23:34:23 davidm Exp $
 *  @Author: Markus Pietrek
 *  @Descr: The ethernet driver for the NS7520. It is partly based on
 *          the netarm_eth.c and snull.c and some hints (of yet
 *          undocumented bugs)
 *          are taken from NetOS 5.1 sources [3] and NET+50/20M Hardware
 *          Reference Manual [4]
 *          Currently it is only tested on UNC20.
 *          The only PHY supported yet is the LXT971A
 *          For a list of references [1] and [2] have a look at
 *          ns7520_eth.h
 *          Link status changes 10/100 MBit or full/half are not detected
 *          yet. This requires a timer polling PHY_LXT971_STAT2 for LINKCHG
 *  @References: NS7520_errata_90000352.pdf [5]
 *  
 ***********************************************************************/
/***********************************************************************
 *
 *  @History:
 *     2003/08/28 : Adapted version by Art Shipkowski <art@videon-central.com>
 *                  released to uClinux-dist sources.
 *     2003/06/16 : fixed a buffer overrun in interrupt rx
 *     2003/06/05 : fixed some minor issues with the order of multicast
 *                  initialization, comments and names. As of a bug of the chip
 *                  itself, multicasts are not filtered by the NS7520 :-(
 *     2003/05/20 : BugID 200: ethernet: IRQ31 is locking the system 
 *                  ethernet transmit lock on underrun fixed
 *     2003/04/25 : BugID 153: too many packet looses on ping -f
 *                  fixed race condition transmit, use IDONE DMA Interrupt
 *                  correctly for transmit and not any longer for receive.
 ************************************************************************/

#include <linux/module.h>	// MODULE_AUTHOR
#include <linux/init.h>		// module_init
#include <linux/sched.h>	// request_irq
#include <linux/netdevice.h>    // net_device
#include <linux/etherdevice.h>  // eth_type_trans
#include <linux/ioport.h>	// request_region
#include <linux/timer.h>	// add_timer

#include <asm/irq.h>		// IRQ_*
#include <asm/delay.h>		// udelay
#include <asm/memory.h>  // virt_to_bus and friends
#if 0
#include <asm/arch/netarm_dma.h>    // netarm_dmaChannelEnable
#include <asm/arch/modnet_eeprom.h> // NaNvramReadEthParams
#else
#include <asm/arch/netarm_registers.h>
#endif
#include "ns7520_eth.h"		    // all the register stuff

#define DEBUG_INIT		1
#define DEBUG_MAJOR		2
#define DEBUG_MINOR		3
#define DEBUG_INTERRUPT		4
#define DEBUG_ETH		5
#define DEBUG_MII		6
#define DEBUG_MII_LOW		7
#define DEBUG_ALL		DEBUG_ETH

//#define DEBUG_VERBOSE		DEBUG_MINOR

#ifdef DEBUG_VERBOSE
# define DEBUG_ARGS( n, args... ) \
	if( n <= DEBUG_VERBOSE ) \
		printk( KERN_INFO __FILE__ ": " args )
# define DEBUG_FN( n ) \
	if( n <= DEBUG_VERBOSE ) \
		printk( KERN_INFO "%s:line %d\n", __FUNCTION__,__LINE__ );
# define ASSERT(expr, func) \
	if(!(expr)) { \
        	printk( KERN_ERR "Assertion failed! %s:%s:line %d %s\n", \
        	__FILE__,__FUNCTION__,__LINE__,(#expr));  \
        	func }
#else
# define DEBUG_ARGS( n, expr, args... )
# define DEBUG_FN( n )
# define ASSERT(expr, func)
#endif // DEBUG

// use default FS Forth-Systeme GmbH MAC address
#define NS7520_DEFAULT_MAC	  { 0x00, 0x04, 0xf3, 0x00, 0x06, 0x35 }

#define NS7520_ETH_REGION1_START  NS7520_ETH_MODULE_BASE + NS7520_ETH_EGCR
#define NS7520_ETH_REGION1_LEN	  (NS7520_ETH_ERSR - NS7520_ETH_EGCR + 4 )
#define NS7520_ETH_REGION2_START  NS7520_ETH_MODULE_BASE + NS7520_ETH_MAC1 
#define NS7520_ETH_REGION2_LEN    (NS7520_ETH_SA3 - NS7520_ETH_MAC1 + 4 )
#define NS7520_ETH_REGION3_START  NS7520_ETH_MODULE_BASE + NS7520_ETH_SAFR
#define NS7520_ETH_REGION3_LEN	  4
#define NS7520_ETH_REGION4_START  NS7520_ETH_MODULE_BASE + NS7520_ETH_HT1
#define NS7520_ETH_REGION4_LEN	  (NS7520_ETH_HT4 - NS7520_ETH_HT1 + 4 )

#define NS7520_TIMEOUT		  5*HZ // in jiffies
#define NS7520_MII_NEG_DELAY      5*HZ  // in jiffies

#define NS7520_DRIVER_NAME	  "ns7520_eth"

#define NS7520_RX_DESC		  24 // number of receive buffer descriptors
#define NS7520_ETH_FRAME_LEN	  1520 // NS7520 accepts 1520 byte packets
				       // instead of 1518

#define NS7520_POLYNOMIAL	  0x4c11db6L // taken from [1] p183
#define NS7520_LEFT		  0          // left rotation
#define NS7520_RIGHT		  1          // right rotation

/* ns7520_private contains private data to this module */
struct ns7520_private_t
{
	netarm_dmaBufferDescFb_t aRxDmaBuffer[ NS7520_RX_DESC ]; // receive
								// buffer desc 
	netarm_dmaBufferDescFb_t txDmaBuffer; // transmit buffer desc

	struct net_device_stats stats; // the ethernet interface statistics
	struct sk_buff* pTxSkb; // receive packet buffers
	struct sk_buff* apRxSkb[ NS7520_RX_DESC ]; // receive packet buffers
	spinlock_t lock;	// locking for multiple processes

	struct timer_list rxLockupTimer; // timer for receive lockups
	unsigned long rxFifoPackets; // packets since last rxLockupTimer event
	volatile unsigned char rxLockupTimerDisable;  // if 1, rxLockupTimer
                                                      // doesn't refresh
	unsigned short uiLastLinkStatus; // stores the last detected link
                                         // information 10/100 half/full auto
	                                 // 0xff is empty
	unsigned int unLastRxDmaDescIndex; // last dma desc index used
};

static unsigned char aucMACAddr[ ETH_ALEN ] = NS7520_DEFAULT_MAC;

static PhyType PhyDetected = PHY_NONE;
static unsigned int nPhyMaxMdioClock = 250000; // defaults to 2.5 MHz

static int  ns7520_init_module( void );
static void ns7520_cleanup_module( void );
static int  ns7520_init_device( struct net_device* pDev );
static int  ns7520_open( struct net_device* pDev );
static int  ns7520_release( struct net_device* pDev );
static int  ns7520_start_xmit( struct sk_buff* pSkb, struct net_device* pDev );
static struct net_device_stats* ns7520_stats( struct net_device* pDev );
static void ns7520_set_multicast_list( struct net_device* pDev );
static int  ns7520_set_mac_address( struct net_device* pDev, void *pMacAddr);
static void ns7520_rx_interrupt( int nIrq, void* vpDev_id,struct pt_regs* pRegs);
static void ns7520_tx_interrupt( int nIrq, void* vpDev_id,struct pt_regs* pRegs);
static void ns7520_irqs_disable( void );
static void ns7520_irqs_enable( void );
static void netarm_dma_disable( char cMode );
static void netarm_dma_enable( char cMode, struct net_device* pDev  );
static char ns7520_eth_reset( struct net_device* pDev );
static void ns7520_eth_enable( struct net_device* pDev );
static void ns7520_rx_fifo_reset( struct net_device* pDev );
static void ns7520_tx_fifo_reset( struct net_device* pDev );
static void ns7520_rx_lockup_detect( unsigned long ulDev );
static void ns7520_rx_lockup_timer_enable( struct net_device* pDev );
static void ns7520_rx_lockup_timer_disable( struct net_device* pDev );
static void ns7520_tx_timeout( struct net_device* pDev );
static void ns7520_link_force( struct net_device* pDev );
static void ns7520_link_auto_negotiate( struct net_device* pDev );
static void ns7520_link_update_egcr( struct net_device* pDev );
static void ns7520_link_print_changed( struct net_device* pDev );

// the PHY stuff

static char ns7520_mii_identify_phy( void );
static unsigned short ns7520_mii_read( unsigned short uiRegister );
static void ns7520_mii_write( unsigned short uiRegister, unsigned short uiData );
static unsigned int ns7520_mii_get_clock_divisor( unsigned int unMaxMDIOClk );
static unsigned int ns7520_mii_poll_busy( void );

// the multicast stuff

static void ns7520_load_mca_hash_table( struct dev_mc_list* pMcList );
static void ns7520_set_hash_bit( unsigned char* pucHashTable, int nBit );
static int ns7520_calculate_hash_bit( unsigned char* pucMac );
static unsigned short ns7520_rotate( unsigned short uiReg,
				     int nDirection,
				     int nBits );

// declared later due to ns7520_init_device
struct net_device ns7520_devs =
{
    name: "eth%d",		// ethernet device
    init: ns7520_init_device,	// init function
};


/***********************************************************************
 * @Function: ns7520_init_device
 * @Return: 0 or 1 on failure
 * @Descr: configures and registers the ethernet driver in the stack
 ***********************************************************************/

static int ns7520_init_device( struct net_device* pDev )
{
	DEBUG_FN( DEBUG_INIT );

	ether_setup( pDev ); // assign default values

	pDev->open              = ns7520_open;
	pDev->stop              = ns7520_release;
	pDev->hard_start_xmit   = ns7520_start_xmit;
	pDev->get_stats         = ns7520_stats;
	pDev->set_multicast_list= ns7520_set_multicast_list;
	pDev->set_mac_address   = ns7520_set_mac_address;
	pDev->tx_timeout        = ns7520_tx_timeout;
	pDev->watchdog_timeo    = NS7520_TIMEOUT;

	pDev->base_addr 	= NS7520_ETH_MODULE_BASE;
	pDev->features		= NETIF_F_HW_CSUM;
	pDev->flags		= IFF_MULTICAST | IFF_BROADCAST;

	SET_MODULE_OWNER( pDev );

	spin_lock_init( & ((struct ns7520_private_t *) pDev->priv)->lock );

	return 0;
}


/***********************************************************************
 * @Function: ns7520_open
 * @Return: 0 on success otherwise error
 * @Descr: reserves memory, ports and interrupts, initializes ethernet
 *         driver
 ***********************************************************************/

static int ns7520_open( struct net_device* pDev )
{
	int nRes = -EBUSY;
	struct ns7520_private_t* pPriv;
	int i;
	
	DEBUG_FN( DEBUG_INIT );

	MOD_INC_USE_COUNT;

	// request memory regions & interrupts

	if( request_region( NS7520_ETH_REGION1_START,
			    NS7520_ETH_REGION1_LEN,
			    NS7520_DRIVER_NAME ) == NULL )
	    goto error_region1;
	if( request_region( NS7520_ETH_REGION2_START,
			    NS7520_ETH_REGION2_LEN,
			    NS7520_DRIVER_NAME ) == NULL )
	    goto error_region2;
	if( request_region( NS7520_ETH_REGION3_START,
			    NS7520_ETH_REGION3_LEN,
			    NS7520_DRIVER_NAME ) == NULL )
	    goto error_region3;
	if( request_region( NS7520_ETH_REGION4_START,
			    NS7520_ETH_REGION4_LEN,
			    NS7520_DRIVER_NAME ) == NULL )
	    goto error_region4;
	if( request_irq( IRQ_DMA01, 
			 ns7520_rx_interrupt,
			 SA_SAMPLE_RANDOM,
			 NS7520_DRIVER_NAME " receive",
			 pDev ) )
	    goto error_irq_rx;
	if( request_irq( IRQ_DMA02, 
			 ns7520_tx_interrupt,
			 SA_SAMPLE_RANDOM,
			 NS7520_DRIVER_NAME " transmit",
			 pDev ) )
	    goto error_irq_tx;

	// allocate private field
	pPriv = pDev->priv = kmalloc(sizeof(struct ns7520_private_t),GFP_KERNEL);
	if( pPriv == NULL )
	{
	    nRes = -ENOMEM;
	    goto error_nomem;
	}
	
	memset( pPriv, 0, sizeof(struct ns7520_private_t) );
	// just to be sure
	ASSERT( NULL == 0, return -ENOMEM; );
	
	// preallocate any receive skb buffers for DMA transfer
	// !we receive the rx interrupt after one packet has been copied!
	for( i = 0; i < NS7520_RX_DESC; i++ )
	{
	    pPriv->apRxSkb[ i ] = dev_alloc_skb( NS7520_ETH_FRAME_LEN + 2 );
	    if( pPriv->apRxSkb[ i ] == NULL )
	    {
		nRes = -ENOMEM;
		goto error;
	    }
	    pPriv->apRxSkb[ i ]->dev = pDev;
	}

	// all ok, continue with hardware initialization
	// from here we have access to ports

	memcpy( pDev->dev_addr, aucMACAddr, ETH_ALEN );


	ns7520_irqs_disable();
	netarm_dma_disable( 3 );

	if( !ns7520_eth_reset( pDev ) )
	{
	    nRes = -ENODEV;
	    goto error;
	}

	ns7520_eth_enable( pDev );
	ns7520_irqs_enable();
	ns7520_rx_lockup_timer_enable( pDev );
	
	// assign hardware address

	*get_eth_reg_addr(NS7520_ETH_SA1)=pDev->dev_addr[5]<<8|pDev->dev_addr[4];
	*get_eth_reg_addr(NS7520_ETH_SA2)=pDev->dev_addr[3]<<8|pDev->dev_addr[2];
	*get_eth_reg_addr(NS7520_ETH_SA3)=pDev->dev_addr[1]<<8|pDev->dev_addr[0];

	netif_start_queue( pDev );

	return 0;

	// release IRQ
  error:
	if( pPriv != NULL )
	{
	    // free skb buffers
	    for( i = 0; i < NS7520_RX_DESC; i++ )
	    {
		if( pPriv->apRxSkb[ i ] != NULL )
		    dev_kfree_skb( pPriv->apRxSkb[ i ] );
		else
		    // no more buffers allocated
		    break;
	    }
	    kfree( pPriv );
	}
  error_nomem:
	free_irq( IRQ_DMA02, NULL );
  error_irq_tx:
	free_irq( IRQ_DMA01, NULL );
  error_irq_rx:
	release_region( NS7520_ETH_REGION4_START, NS7520_ETH_REGION4_LEN );
  error_region4:
	release_region( NS7520_ETH_REGION3_START, NS7520_ETH_REGION3_LEN );
  error_region3:
	release_region( NS7520_ETH_REGION2_START, NS7520_ETH_REGION2_LEN );
  error_region2:
	release_region( NS7520_ETH_REGION1_START, NS7520_ETH_REGION1_LEN );
  error_region1:
	MOD_DEC_USE_COUNT;
	return nRes;
}


/***********************************************************************
 * @Function: ns7520_release
 * @Return: 0 on success otherwise error
 * @Descr: releases memory, ports and interrupts. Disables chip
 ***********************************************************************/

static int ns7520_release( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;
	int i;
	
	DEBUG_FN( DEBUG_INIT );

	netif_stop_queue( pDev ); // can't transmit any more

	// disable hardware

	ns7520_irqs_disable();
	netarm_dma_disable( 3 );

	// clear outstanding interrupts
	*get_dma_reg_addr( NETARM_DMA_CHANNEL_1A, NETARM_DMA_STATUS ) =
	    NETARM_DMA_STATUS_IP_MA;
	*get_dma_reg_addr( NETARM_DMA_CHANNEL_2, NETARM_DMA_STATUS ) =
	    NETARM_DMA_STATUS_IP_MA;

	// disable receive and txmit

	*get_eth_reg_addr( NS7520_ETH_EGCR ) &= ~(NS7520_ETH_EGCR_ERX |
						  NS7520_ETH_EGCR_ERXDMA |
						  NS7520_ETH_EGCR_ETX |
						  NS7520_ETH_EGCR_ETXDMA );

	*get_eth_reg_addr( NS7520_ETH_MAC1 ) &= ~NS7520_ETH_MAC1_RXEN;

	// disable timer
	ns7520_rx_lockup_timer_disable( pDev );
	
	// free ressources
	// interrupts and regions

	free_irq( IRQ_DMA02, pDev );
	free_irq( IRQ_DMA01, pDev );
	release_region( NS7520_ETH_REGION4_START, NS7520_ETH_REGION4_LEN );
	release_region( NS7520_ETH_REGION3_START, NS7520_ETH_REGION3_LEN );
	release_region( NS7520_ETH_REGION2_START, NS7520_ETH_REGION2_LEN );
	release_region( NS7520_ETH_REGION1_START, NS7520_ETH_REGION1_LEN );

	// DMA buffers
	for( i = 0; i < NS7520_RX_DESC; i++ )
	    dev_kfree_skb( pPriv->apRxSkb[ i ] );

	if( pPriv->pTxSkb != NULL )
	{
	    // we won't transmit it any longer
	    dev_kfree_skb( pPriv->pTxSkb );
	    pPriv->pTxSkb = NULL;
	}
	
	kfree( pPriv );


	MOD_DEC_USE_COUNT;

	return 0;
}


/***********************************************************************
 * @Function: ns7520_start_xmit
 * @Return: 0 if packet assigned for transmission otherwise error
 * @Descr: prepares and send a packet to hardware transmit. Only one DMA
 *         buffer desc is used.
 ***********************************************************************/

static int ns7520_start_xmit( struct sk_buff* pSkb, struct net_device* pDev )
{
	int nLen;
	struct ns7520_private_t* pPriv = (struct ns7520_private_t *) pDev->priv;
	unsigned long ulFlags;

	DEBUG_FN( DEBUG_INTERRUPT );
	
        spin_lock_irqsave( &pPriv->lock, ulFlags );

	nLen = pSkb->len < ETH_ZLEN ? ETH_ZLEN : pSkb->len;
	pDev->trans_start = jiffies; // save the timestamp
	
	// Remember the skb, so we can free it at interrupt time
	// in case the last one has not been freed, give warning
	ASSERT( pPriv->pTxSkb == NULL, ; );
	pPriv->pTxSkb = pSkb;

	// set up DMA buffer descriptor
	pPriv->txDmaBuffer.l.bits.srcPtr = (unsigned int)virt_to_bus(pSkb->data);
	pPriv->txDmaBuffer.h.bits.bufLen = pSkb->len;
	pPriv->txDmaBuffer.h.bits.full   = 1;

	// packet is transmitted anywhere later, stop queue until then
	netif_stop_queue( pDev );

	// retrigger DMA transfer by clearing NRIP
	*get_dma_reg_addr( NETARM_DMA_CHANNEL_2, NETARM_DMA_STATUS ) |= 
	    NETARM_DMA_STATUS_IP_NR;

	spin_unlock_irqrestore( &pPriv->lock, ulFlags );

	return 0;
}


/***********************************************************************
 * @Function: ns7520_stats
 * @Return: a pointer to the device statistics
 * @Descr: the device receive, transmit and error statistics
 ***********************************************************************/

static struct net_device_stats* ns7520_stats( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;
	
	DEBUG_FN( DEBUG_MINOR );
	
	return &pPriv->stats;
}


/***********************************************************************
 * @Function: ns7520_set_multicast_list
 * @Return: nothing
 * @Descr: updates the multicast list and enabled/disables promiscuous
 *         mode
 *         Always accept broadcasts addresses
 ***********************************************************************/

static void ns7520_set_multicast_list( struct net_device* pDev )
{
	unsigned int unSAFR;

    	DEBUG_FN( DEBUG_MINOR );

	unSAFR = *get_eth_reg_addr( NS7520_ETH_SAFR );

	// disable all multicast modes
	unSAFR &= ~( NS7520_ETH_SAFR_PRO |
		     NS7520_ETH_SAFR_PRM |
		     NS7520_ETH_SAFR_PRA );

	if( pDev->flags & IFF_PROMISC )
	{
	    DEBUG_ARGS( DEBUG_MINOR, "Promiscuous mode\n" );
	    *get_eth_reg_addr( NS7520_ETH_SAFR ) = unSAFR | NS7520_ETH_SAFR_PRO;
	    return;
	} else if( pDev->flags & IFF_ALLMULTI )	{
	    DEBUG_ARGS( DEBUG_MINOR, "Accept all multicast packets\n" );
	    *get_eth_reg_addr( NS7520_ETH_SAFR ) = unSAFR |
		NS7520_ETH_SAFR_BROAD |
		NS7520_ETH_SAFR_PRM;
	    return;
	} else if( !(pDev->flags & IFF_MULTICAST) ) {
	    // TODO: check if broadcasts will be delivered correctly or if they
	    // need multicasts to be enabled
	    DEBUG_ARGS( DEBUG_MINOR,
			"Accept only direct addressed and broadcast packets\n" );
	    *get_eth_reg_addr( NS7520_ETH_SAFR ) = unSAFR |
		NS7520_ETH_SAFR_BROAD;
	    return;
	}

	DEBUG_ARGS( DEBUG_MINOR,
		    "Accept multicast list\n" );

	// store multicast list of kernel into chip
	ns7520_load_mca_hash_table( pDev->mc_list );

	*get_eth_reg_addr( NS7520_ETH_SAFR ) = unSAFR |
	    NS7520_ETH_SAFR_BROAD |
	    NS7520_ETH_SAFR_PRA;

}

/***********************************************************************
 * @Function: ns7520_set_mac_address
 * @Return: nothing
 * @Descr: sets the mac address based upon what we are told to set it to
 ***********************************************************************/
static int 
ns7520_set_mac_address(struct net_device* pDev, void *pMacAddr)
{
	int i;
	struct sockaddr *addr=pMacAddr;

	if (netif_running(pDev))
		return -EBUSY;
 
	/* Set the device copy of the Ethernet address
	*/
	memcpy(pDev->dev_addr, addr->sa_data, ETH_ALEN);
      
	/* Set our copy of the Ethernet address 
	*/
	for (i = 0; i < ETH_ALEN; i++)
	{
		aucMACAddr[i] = pDev->dev_addr[i];
	}

	/* Set station address.
	*/
	*get_eth_reg_addr(NS7520_ETH_SA1)=pDev->dev_addr[5]<<8|pDev->dev_addr[4];
	*get_eth_reg_addr(NS7520_ETH_SA2)=pDev->dev_addr[3]<<8|pDev->dev_addr[2];
	*get_eth_reg_addr(NS7520_ETH_SA3)=pDev->dev_addr[1]<<8|pDev->dev_addr[0];
  
	return 0;
}
/***********************************************************************
 * @Function: ns7520_rx_interrupt
 * @Return: nothing
 * @Descr: DMA receive hardware interrupt handler, runs with interrupts
 *         enabled
 ***********************************************************************/

static void ns7520_rx_interrupt( int nIrq, void* vpDev_id, struct pt_regs* pRegs)
{
	struct net_device* pDev = (struct net_device*) vpDev_id;
	struct ns7520_private_t* pPriv;
	unsigned int unIndex;
	unsigned int unDmaStatus;
	unsigned int unCurrentDmaDescIndex;
	
	DEBUG_FN( DEBUG_INTERRUPT );

	ASSERT( pDev != NULL, return; ); // a little bit paranoid here

	// lock device
	pPriv = (struct ns7520_private_t*) pDev->priv;
	spin_lock( &pPriv->lock );

	// now handle hardware request

	unDmaStatus = *get_dma_reg_addr(NETARM_DMA_CHANNEL_1A,NETARM_DMA_STATUS);

	// acknowledge pending interrupts, IE_ are cleared on interrupt
	// interrupt handler
	*get_dma_reg_addr( NETARM_DMA_CHANNEL_1A, NETARM_DMA_STATUS ) =
	    unDmaStatus | NETARM_DMA_STATUS_IP_MA |
	    NETARM_DMA_STATUS_IE_NC |
	    NETARM_DMA_STATUS_IE_EC |
	    NETARM_DMA_STATUS_IE_NR |
	    NETARM_DMA_STATUS_IE_CA;

	// now check the status fields
	// IP_NR may be called even if no interrupt enable is set
	if( (unDmaStatus &
	     (NETARM_DMA_STATUS_IP_EC |
	      NETARM_DMA_STATUS_IP_CA |
	      NETARM_DMA_STATUS_IP_PC) ) ||
	    ((unDmaStatus & (NETARM_DMA_STATUS_IP_NR|NETARM_DMA_STATUS_IE_NR ))==
	     (NETARM_DMA_STATUS_IE_NR | NETARM_DMA_STATUS_IP_NR)))
	{
	    printk( KERN_WARNING NS7520_DRIVER_NAME
		    ": error interrupt, status is 0x%x\n", unDmaStatus );
	}

	// check for full buffer and give it to kernel
	// last descriptor modified by DMA control so we don't have to check all
	unCurrentDmaDescIndex = ((*get_dma_reg_addr( NETARM_DMA_CHANNEL_1A,
						     NETARM_DMA_CONTROL )) &
				 NETARM_DMA_CONTROL_INDEX ) / 8;
	ASSERT( unCurrentDmaDescIndex < NS7520_RX_DESC, return; ); // skip
								   // package
	unIndex = pPriv->unLastRxDmaDescIndex;

	do
	{
	    if( pPriv->aRxDmaBuffer[ unIndex ].h.bits.full )
	    {
		if( pPriv->aRxDmaBuffer[unIndex].h.bits.status & 
		    NS7520_ETH_ERSR_RXOK )
		{
		    // update statistics
		    pPriv->stats.rx_packets++;
		    pPriv->stats.rx_bytes +=
			pPriv->aRxDmaBuffer[ unIndex ].h.bits.bufLen;

		    // I guess the user would be glad to get this packet, so
		    // give it 
        if (pPriv->apRxSkb[ unIndex ]->data_len)
        {
          printk("[%d]skb->data_len bad value %d\n", unIndex, pPriv->apRxSkb[ unIndex ]->data_len);
        }
		    skb_put( pPriv->apRxSkb[ unIndex ],
			     pPriv->aRxDmaBuffer[ unIndex ].h.bits.bufLen - 4 );
		    pPriv->apRxSkb[ unIndex ]->protocol =
			eth_type_trans( pPriv->apRxSkb[ unIndex ],
				    pDev );
		    pPriv->apRxSkb[ unIndex ]->ip_summed = CHECKSUM_UNNECESSARY;
	    
		    skb_reserve( pPriv->apRxSkb[ unIndex ], 2 ); // align 16byte
		    netif_rx( pPriv->apRxSkb[ unIndex ] );

		    // allocate new skb buffer as control of old one is given to
		    // kernel and reset dma buffer
		    pPriv->apRxSkb[unIndex] = dev_alloc_skb(
			NS7520_ETH_FRAME_LEN + 2 );
		    if( pPriv->apRxSkb[ unIndex ] == NULL )
			// no memory is serious enough to panic
			panic( NS7520_DRIVER_NAME
			       ": couldn't allocate next skb buffer\n" );
		    pPriv->apRxSkb[ unIndex ]->dev = pDev;

		    pPriv->aRxDmaBuffer[ unIndex ].h.bits.full = 0;
		    // buffer size has been set to read bytes by dma controller
		    pPriv->aRxDmaBuffer[unIndex].h.bits.bufLen =
			NS7520_ETH_FRAME_LEN;
		    pPriv->aRxDmaBuffer[ unIndex ].h.bits.status = 0;
		    // set new buffer address
		    pPriv->aRxDmaBuffer[ unIndex ].l.bits.srcPtr =
			(unsigned int)virt_to_bus(pPriv->apRxSkb[unIndex]->data);
		} else {
		    pPriv->stats.rx_errors++;
		    if( pPriv->aRxDmaBuffer[ unIndex ].h.bits.status &
			(NS7520_ETH_ERSR_RXCRC |
			 NS7520_ETH_ERSR_RXDR |
			 NS7520_ETH_ERSR_RXCV ) )
			pPriv->stats.rx_crc_errors++;
		    
		    if( pPriv->aRxDmaBuffer[ unIndex ].h.bits.status &
			(NS7520_ETH_ERSR_RXLNG |
			 NS7520_ETH_ERSR_RXSHT) )
			pPriv->stats.rx_frame_errors++;
		    
		    if( pPriv->aRxDmaBuffer[ unIndex ].h.bits.status &
			NS7520_ETH_ERSR_ROVER )
		    {
			DEBUG_ARGS( DEBUG_MAJOR,
				    "Receive Overrun, resetting FIFO\n" );
			// a reset of RX FIFO is still necessary with NS7520
			ns7520_rx_fifo_reset( pDev );
		    }

		    if( pPriv->aRxDmaBuffer[ unIndex ].h.bits.full )
		    {
			// reset to default values in case an error happened
			// and the full bit has been set already by DMA,
			// if not unset, it would the DMA controller hangs at
			// this descriptor => "IRQ31 disabled"
			pPriv->aRxDmaBuffer[ unIndex ].h.bits.bufLen =
			    NS7520_ETH_FRAME_LEN;
			pPriv->aRxDmaBuffer[ unIndex ].h.bits.status = 0;
			pPriv->aRxDmaBuffer[ unIndex ].h.bits.full = 0;
		    }
		}
	    }
	    unIndex++;
	    if( unIndex >= NS7520_RX_DESC )
		// wrap to beginning of desc List
		unIndex = 0;
	} while( unIndex != unCurrentDmaDescIndex );

	pPriv->unLastRxDmaDescIndex = unIndex;
	
	// unlock device
	spin_unlock( &pPriv->lock );
}

/***********************************************************************
 * @Function: ns7520_tx_interrupt
 * @Return: nothing
 * @Descr: DMA transmit hardware interrupt handler, runs with interrupts
 *         enabled
 ***********************************************************************/

static void ns7520_tx_interrupt( int nIrq, void* vpDev_id, struct pt_regs* pRegs)
{
	struct net_device* pDev = (struct net_device*) vpDev_id;
	struct ns7520_private_t* pPriv;
	unsigned int unDmaStatus;
	unsigned int unTxStatus;
	char cFreeSkb = 0;

	DEBUG_FN( DEBUG_INTERRUPT );

	ASSERT( pDev != NULL, return; ); // a little bit paranoid here

	// lock device
	pPriv = (struct ns7520_private_t*) pDev->priv;
	spin_lock( &pPriv->lock );

	// now handle hardware request
	unDmaStatus = *get_dma_reg_addr(NETARM_DMA_CHANNEL_2,NETARM_DMA_STATUS);
	unTxStatus = *get_eth_reg_addr( NS7520_ETH_ETSR );

	// acknowledge pending interrupts
	*get_dma_reg_addr( NETARM_DMA_CHANNEL_2, NETARM_DMA_STATUS ) =
	    unDmaStatus | NETARM_DMA_STATUS_IP_MA;

	// now check the status fields
	// IP_NR may be called even if no interrupt enable is set
	if( (unDmaStatus &
	     (NETARM_DMA_STATUS_IP_EC |
	      NETARM_DMA_STATUS_IP_CA |
	      NETARM_DMA_STATUS_IP_PC) ) ||
	    ((unDmaStatus & (NETARM_DMA_STATUS_IP_NR|NETARM_DMA_STATUS_IE_NR ))==
	     (NETARM_DMA_STATUS_IE_NR | NETARM_DMA_STATUS_IP_NR)))
	{
	    DEBUG_ARGS( DEBUG_MINOR,
			"error interrupt, status is 0x%x\n", unDmaStatus );
	}

	if( (unTxStatus & NS7520_ETH_ETSR_TXOK) == NS7520_ETH_ETSR_TXOK )
	{
	    pPriv->stats.tx_packets++;
	    pPriv->stats.tx_bytes +=  pPriv->pTxSkb->len;
	    cFreeSkb = 1;
	} else if( unTxStatus & ( NS7520_ETH_ETSR_TXAL |
				  NS7520_ETH_ETSR_TXAED |
				  NS7520_ETH_ETSR_TXAEC |
				  NS7520_ETH_ETSR_TXAUR |
				  NS7520_ETH_ETSR_TXAJ |
				  NS7520_ETH_ETSR_TXDEF |
				  NS7520_ETH_ETSR_TXCRC |
				  NS7520_ETH_ETSR_TXCOLC ) )
	{
	    char cRetransmit = 1;

	    // the packets have been flushed and not been transmitted, so
	    // retrigger them
	    pPriv->stats.tx_errors++;
	    if( unTxStatus & (NS7520_ETH_ETSR_TXAL |
			      NS7520_ETH_ETSR_TXAEC ) )
		pPriv->stats.collisions++;
	    else if( unTxStatus & NS7520_ETH_ETSR_TXAED )
		pPriv->stats.tx_carrier_errors++;
	    else if( unTxStatus & NS7520_ETH_ETSR_TXDEF )
	    {
		pPriv->stats.tx_carrier_errors++;
		cRetransmit = 0;
		cFreeSkb = 1;
	    }

	    if( (unTxStatus & (NS7520_ETH_ETSR_TXAUR | NS7520_ETH_ETSR_TXCOLC))&&
		pPriv->txDmaBuffer.h.bits.full == 1 &&
		!unDmaStatus )
	    {
		// see [5] ethernet transmit considerations
		DEBUG_ARGS( DEBUG_MAJOR,
			    "aborted transmit" );
		ns7520_tx_fifo_reset( pDev );
	    }
	    
	    if( unTxStatus & (NS7520_ETH_ETSR_TXAJ |
			      NS7520_ETH_ETSR_TXCRC ) )
	    {
		DEBUG_ARGS( DEBUG_MINOR,
			    "packet tx skipped, status is 0x%x\n",
			    unTxStatus );
		
		cRetransmit = 0;
		cFreeSkb = 1;
		pPriv->stats.tx_aborted_errors++;
	    }

	    if( cRetransmit )
	    {
		// the DMA has already transferred the data into the Ethernet
		// FIFO and cleared the full bit.
		pPriv->txDmaBuffer.h.bits.full = 1;
		
		// retrigger DMA transfer by clearing NRIP
		*get_dma_reg_addr( NETARM_DMA_CHANNEL_2, NETARM_DMA_STATUS ) |= 
		    NETARM_DMA_STATUS_IP_NR;
	    }
	} else
	    DEBUG_ARGS( DEBUG_MAJOR,
			"unknown error, status is 0x%x\n",
			unTxStatus );

	if( cFreeSkb )
	{
	    // data has been send, free buffer
	    dev_kfree_skb_irq( pPriv->pTxSkb );
	    pPriv->pTxSkb = NULL;
	    // next packet can be send
	    netif_wake_queue( pDev );
	}
	
	// unlock device
	spin_unlock( &pPriv->lock );
}

static void ns7520_irqs_disable( void )
{
	DEBUG_FN( DEBUG_ETH );

	disable_irq( IRQ_DMA01 );
	disable_irq( IRQ_DMA02 );
}

static void ns7520_irqs_enable( void )
{
	DEBUG_FN( DEBUG_ETH );

	enable_irq( IRQ_DMA01 );
	enable_irq( IRQ_DMA02 );
}

/***********************************************************************
 * @Function: netarm_dma_disable
 * @Return: nothing
 * @Descr: disable the DMA channels and clear pending interrupts
 *         bit 0 of cMode means enable transmit, bit 1 of cMode means enable
 *         receive
 ***********************************************************************/

static void netarm_dma_disable( char cMode )
{
	DEBUG_FN( DEBUG_MINOR );

	if( ( cMode & 2 ) == 2 )
	{
	    netarm_dmaChannelDisable( NETARM_DMA_CHANNEL_1A );
	    netarm_dmaChannelDisable( NETARM_DMA_CHANNEL_1B );
	    netarm_dmaChannelDisable( NETARM_DMA_CHANNEL_1C );
	    netarm_dmaChannelDisable( NETARM_DMA_CHANNEL_1D );
	}
	
	if( ( cMode & 1 ) == 1 )
	    netarm_dmaChannelDisable( NETARM_DMA_CHANNEL_2 );
}

/***********************************************************************
 * @Function: netarm_dma_enable
 * @Return: nothing
 * @Descr: enables the DMA channels, clears the interrupts and
 *         configures buffer pointers
 *         bit 0 of cMode means enable transmit, bit 1 of cMode means enable
 *         receive
 ***********************************************************************/

static void netarm_dma_enable( char cMode, struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;
	int i;

	DEBUG_FN( DEBUG_MINOR );

	if( ( cMode & 2 ) == 2 )
	{
	    // configure channels for transmit and receive, clear pending,
	    netarm_dmaBufferDescInit( pPriv->aRxDmaBuffer,
				      NS7520_RX_DESC,
				      0 );
	
	    for( i = 0; i < NS7520_RX_DESC; i++ )
	    {
		// link the src pointer to the skb buffer
		pPriv->aRxDmaBuffer[ i ].l.bits.srcPtr =
		    virt_to_bus( pPriv->apRxSkb[ i ]->data );
		pPriv->aRxDmaBuffer[ i ].h.bits.bufLen = NS7520_ETH_FRAME_LEN;
	    }

	    // use NRIP for receive lockup interrupts
	    netarm_dmaChannelEnable( NETARM_DMA_CHANNEL_1A,
				     pPriv->aRxDmaBuffer,
				     NETARM_DMA_CONTROL_BB_100 |
				     NETARM_DMA_CONTROL_MODE_PER2MEM |
				     ((pPriv->uiLastLinkStatus &
				       PHY_LXT971_STAT2_100BTX) ?
				      NETARM_DMA_CONTROL_BTE_4 :
				      NETARM_DMA_CONTROL_BTE_1 ),
				     NETARM_DMA_STATUS_IP_MA |
				     NETARM_DMA_STATUS_IE_NC |
				     NETARM_DMA_STATUS_IE_EC |
				     NETARM_DMA_STATUS_IE_NR |
				     NETARM_DMA_STATUS_IE_CA );
	}
	
	    
	if( ( cMode & 1 ) == 1 )
	{
	    // [3] eth_enable_dma uses IDONE for txmit
	    netarm_dmaBufferDescInit( &pPriv->txDmaBuffer, 1, 1 );
	    pPriv->txDmaBuffer.l.bits.last = 1;
	
	    netarm_dmaChannelEnable( NETARM_DMA_CHANNEL_2,
				     &pPriv->txDmaBuffer,
				     NETARM_DMA_CONTROL_BB_100 |
				     NETARM_DMA_CONTROL_MODE_MEM2PER |
				     ((pPriv->uiLastLinkStatus &
				       PHY_LXT971_STAT2_100BTX) ?
				      NETARM_DMA_CONTROL_BTE_4 :
				      NETARM_DMA_CONTROL_BTE_1 ),
				     NETARM_DMA_STATUS_IP_EC |
				     NETARM_DMA_STATUS_IE_EC );
	}
}

/***********************************************************************
 * @Function: ns7520_eth_reset
 * @Return: 0 on failure otherwise 1
 * @Descr: resets the ethernet interface and the PHY,
 *         performs auto negotiation or fixed modes
 *         Clears MIB statistics
 ***********************************************************************/

static char ns7520_eth_reset( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;

	DEBUG_FN( DEBUG_MINOR );

	// reset and initialize PHY

	*get_eth_reg_addr( NS7520_ETH_MAC1 ) &= ~NS7520_ETH_MAC1_SRST;

	// we don't support hot plugging of PHY, therefore we don't reset
	// PhyDetected and nPhyMaxMdioClock here. The risk is if the setting is
	// incorrect the first open
	// may detect the PHY correctly but succeding will fail
	// For reseting the PHY and identifying we have to use the standard
	// MDIO CLOCK value 2.5 MHz only after hardware reset
	// After having identified the PHY we will do faster

	*get_eth_reg_addr( NS7520_ETH_MCFG ) =
	    ns7520_mii_get_clock_divisor( nPhyMaxMdioClock );

	// reset PHY
	ns7520_mii_write( PHY_COMMON_CTRL, PHY_COMMON_CTRL_RESET );
	ns7520_mii_write( PHY_COMMON_CTRL, 0 );

	udelay( 300 );		// [2] p.70 says 300us reset recovery time

	// MII clock has been setup to default, ns7520_mii_identify_phy should
	// work for all

	if( !ns7520_mii_identify_phy() )
	{
	    printk( KERN_ERR NS7520_DRIVER_NAME ": Unsupported PHY, aborting\n");
	    return 0;
	}

	// now take the highest MDIO clock possible after detection
	*get_eth_reg_addr( NS7520_ETH_MCFG ) =
	    ns7520_mii_get_clock_divisor( nPhyMaxMdioClock );


	// PHY has been detected, so there can be no abort reason and we can
	// finish initializing ethernet

	pPriv->uiLastLinkStatus = 0xff; // undefined
	
#if 0    
  // use parameters defined
  ns7520_link_force( pDev );
#else
  ns7520_link_auto_negotiate( pDev );
#endif  

	if( PhyDetected == PHY_LXT971A )
	    // set LED2 to link mode
	    ns7520_mii_write( PHY_LXT971_LED_CFG,
			      PHY_LXT971_LED_CFG_LINK_ACT << 
			      PHY_LXT971_LED_CFG_SHIFT_LED2);

	return 1;
}

/***********************************************************************
 * @Function: ns7520_eth_enable
 * @Return: nothing
 * @Descr: configures ethernet for link mode, enables FIFO and DMA
 ***********************************************************************/


static void ns7520_eth_enable( struct net_device* pDev )
{
	DEBUG_FN( DEBUG_MINOR );

	// of course we want broadcasts
	netarm_dma_enable( 3, pDev );

	*get_eth_reg_addr( NS7520_ETH_MAC1 ) = NS7520_ETH_MAC1_RXEN;
	*get_eth_reg_addr( NS7520_ETH_SUPP ) = NS7520_ETH_SUPP_JABBER;
	*get_eth_reg_addr( NS7520_ETH_MAC1 ) = NS7520_ETH_MAC1_RXEN;

	// the kernel may give packets < 60 bytes, for example arp
	*get_eth_reg_addr( NS7520_ETH_MAC2 ) = NS7520_ETH_MAC2_CRCEN |
	    NS7520_ETH_MAC2_PADEN |
	    NS7520_ETH_MAC2_HUGE;

	// enable receive FIFO and DMA
	// enable transmit FIFO and DMA
	// clear soft reset and enable receive logic, use 10/100 Mbps MII
	*get_eth_reg_addr( NS7520_ETH_EGCR ) =
	    NS7520_ETH_EGCR_ETXWM_75 |
	    NS7520_ETH_EGCR_ERX |
	    NS7520_ETH_EGCR_ERXDMA |
	    NS7520_ETH_EGCR_ETX |
	    NS7520_ETH_EGCR_ETXDMA |
	    NS7520_ETH_EGCR_PNA |
	    NS7520_ETH_EGCR_ITXA;

	*get_eth_reg_addr( NS7520_ETH_EGCR ) |=
	    NS7520_ETH_EGCR_ERXREG;
}


/***********************************************************************
 * @Function: ns7520_rx_fifo_reset
 * @Return: nothing
 * @Descr: resets the RX FIFO after an EFE/DMA module RX DMA stall
 *         condition [4]. Errata
 ***********************************************************************/

static void ns7520_rx_fifo_reset( struct net_device* pDev )
{
	DEBUG_FN( DEBUG_MAJOR );
	
	// disable DMA
	*get_eth_reg_addr( NS7520_ETH_EGCR ) &= ~NS7520_ETH_EGCR_ERXDMA;
	*get_eth_reg_addr( NS7520_ETH_EGCR ) &= ~NS7520_ETH_EGCR_ERX;
	netarm_dma_disable( 2 );

	udelay( 20 );

	// reinit DMA
	netarm_dma_enable( 2, pDev );
	*get_eth_reg_addr( NS7520_ETH_EGCR ) |= NS7520_ETH_EGCR_ERXDMA |
	    NS7520_ETH_EGCR_ERX;
}

/***********************************************************************
 * @Function: ns7520_tx_fifo_reset
 * @Return: nothing
 * @Descr: resets the TX FIFO as described in [5], Ethernet transmitter
 *         considerations 
 ***********************************************************************/

static void ns7520_tx_fifo_reset( struct net_device* pDev )
{
	DEBUG_FN( DEBUG_MAJOR );
	
	// disable DMA
	*get_eth_reg_addr( NS7520_ETH_EGCR ) &= ~NS7520_ETH_EGCR_ETXDMA;
	*get_eth_reg_addr( NS7520_ETH_EGCR ) &= ~NS7520_ETH_EGCR_ETX;
	netarm_dma_disable( 1 );

	// reinit DMA
	netarm_dmaChannelDisable( NETARM_DMA_CHANNEL_2 );
	netarm_dma_enable( 1, pDev );
	*get_eth_reg_addr( NS7520_ETH_EGCR ) |= NS7520_ETH_EGCR_ETXDMA |
	    NS7520_ETH_EGCR_ETX;
}


static void ns7520_rx_lockup_detect( unsigned long ulDev )
{
	struct net_device* pDev = (struct net_device*) ulDev;
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;

	DEBUG_FN( DEBUG_INTERRUPT );

	if( pPriv->rxFifoPackets != pPriv->stats.rx_packets )
	    pPriv->rxFifoPackets = pPriv->stats.rx_packets;
	else
	{
	    // check for lockup
	    // from NetARM 50 Hardware Reference Manual EFE/DMA module RX DMA
	    // stall condition
	    if( (*get_eth_reg_addr( NS7520_ETH_EGSR ) &
		 (NS7520_ETH_EGSR_RXBR | NS7520_ETH_EGSR_RXREGR ) ) ==
		NS7520_ETH_EGSR_RXBR )
	    {
		DEBUG_ARGS( DEBUG_MAJOR,
			    "Receive lockup detected, resetting FIFO\n" );
		ns7520_rx_fifo_reset( pDev );
	    }
	}

	if( !test_bit( 0, (void*) &pPriv->rxLockupTimerDisable ) )
	    // refresh the timer if not in disabling mode
	    mod_timer( &pPriv->rxLockupTimer,
		       jiffies + HZ);
}

/***********************************************************************
 * @Function: ns7520_rx_lockup_timer_enable
 * @Return: void
 * @Descr: in case the RX FIFO has locked up (NetARM+50 Errata
 *         EFE/DMA module RX DMA stall condition) is still existing in
 *         NS7520, this timer resets the RX FIFO
 ***********************************************************************/

static void ns7520_rx_lockup_timer_enable( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;

	DEBUG_FN( DEBUG_MINOR );

	// in case we have been reopened it may be != 0
	pPriv->rxLockupTimerDisable = 0;
	pPriv->rxFifoPackets = pPriv->stats.rx_packets;

	init_timer( &pPriv->rxLockupTimer );
	pPriv->rxLockupTimer.function = ns7520_rx_lockup_detect;
	pPriv->rxLockupTimer.data = (unsigned long) pDev;
	pPriv->rxLockupTimer.expires = jiffies + HZ;
	add_timer( &pPriv->rxLockupTimer );
}

/***********************************************************************
 * @Function: ns7520_rx_lockup_timer_disable
 * @Return: void
 * @Descr: in case the RX FIFO has locked up (NetARM+50 Errata
 *         EFE/DMA module RX DMA stall condition) is still existing in
 *         NS7520, this timer resets the RX FIFO
 ***********************************************************************/

static void ns7520_rx_lockup_timer_disable( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;

	DEBUG_FN( DEBUG_MINOR );

	// disable timer so it won't try to modificate itself before
	// del_timer_sync runs
	set_bit( 0, &pPriv->rxLockupTimerDisable );
	del_timer_sync( &pPriv->rxLockupTimer );
}


/***********************************************************************
 * @Function: ns7520_tx_timeout
 * @Return: nothing
 * @Descr: handles transmit timeouts by flushing the tx buffer
 ***********************************************************************/

static void ns7520_tx_timeout( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;

	DEBUG_FN( DEBUG_MINOR );

	// handle interrupt manually
	// in case there is just a receive lockup, the packet will be sent
	// multiple times though. The receiver should handle this
	ns7520_tx_interrupt( 0, pDev, NULL );

	pPriv->stats.tx_errors++;
}

/***********************************************************************
 * @Function: ns7520_link_force
 * @Return: void
 * @Descr: configures eth and MII to use the link mode defined in
 *         ucLinkMode
 ***********************************************************************/

static void ns7520_link_force( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;
	unsigned short uiControl;
	
	DEBUG_FN( DEBUG_MAJOR );

	uiControl = ns7520_mii_read( PHY_COMMON_CTRL );
	uiControl &= ~( PHY_COMMON_CTRL_SPD_MA |
			PHY_COMMON_CTRL_AUTO_NEG |
			PHY_COMMON_CTRL_DUPLEX );

	pPriv->uiLastLinkStatus = 0;
	
#if 0  
  /* Left around to help someone add commandline options later */
	if( ( ucLinkMode & MODNET_EEPROM_AUTONEG_SPEED_MASK ) ==
	    MODNET_EEPROM_AUTONEG_SPEED_100 )
	{
	    uiControl |= PHY_COMMON_CTRL_SPD_100;
	    pPriv->uiLastLinkStatus |= PHY_LXT971_STAT2_100BTX;
	} else
	    uiControl |= PHY_COMMON_CTRL_SPD_10;

	if( ( ucLinkMode & MODNET_EEPROM_AUTONEG_DUPLEX_MASK ) ==
	    MODNET_EEPROM_AUTONEG_DUPLEX_FULL )
	{
	    uiControl |= PHY_COMMON_CTRL_DUPLEX;
	    pPriv->uiLastLinkStatus |= PHY_LXT971_STAT2_DUPLEX_MODE;
	}
#endif

	ns7520_mii_write( PHY_COMMON_CTRL, uiControl );

	ns7520_link_print_changed( pDev );
	ns7520_link_update_egcr( pDev );
}

/***********************************************************************
 * @Function: ns7520_link_auto_negotiate
 * @Return: void
 * @Descr: performs auto-negotation of link.
 ***********************************************************************/

static void ns7520_link_auto_negotiate( struct net_device* pDev )
{
	unsigned long ulStartJiffies;
	unsigned short uiStatus;

	DEBUG_FN( DEBUG_MAJOR );

	// run auto-negotation
	// define what we are capable of
	ns7520_mii_write( PHY_COMMON_AUTO_ADV,
			  PHY_COMMON_AUTO_ADV_100BTXFD |
			  PHY_COMMON_AUTO_ADV_100BTX |
			  PHY_COMMON_AUTO_ADV_10BTFD |
			  PHY_COMMON_AUTO_ADV_10BT |
			  PHY_COMMON_AUTO_ADV_802_3 );
	// start auto-negotiation
	ns7520_mii_write( PHY_COMMON_CTRL,
			  PHY_COMMON_CTRL_AUTO_NEG |
			  PHY_COMMON_CTRL_RES_AUTO );
	// wait for completion

	ulStartJiffies = jiffies;
	while( jiffies < ulStartJiffies + NS7520_MII_NEG_DELAY )
	{
	    uiStatus = ns7520_mii_read( PHY_COMMON_STAT );
	    if((uiStatus & (PHY_COMMON_STAT_AN_COMP|PHY_COMMON_STAT_LNK_STAT)) ==
	       (PHY_COMMON_STAT_AN_COMP | PHY_COMMON_STAT_LNK_STAT) ) 
	    {
		// lucky we are, auto-negotiation succeeded
		ns7520_link_print_changed( pDev );
		ns7520_link_update_egcr( pDev );

		return;
	    }
	}

	DEBUG_ARGS( DEBUG_MAJOR, "auto-negotiation timed out\n" );
	// ignore invalid link settings
}

/***********************************************************************
 * @Function: ns7520_link_update_egcr
 * @Return: void
 * @Descr: updates the EGCR and MAC2 link status after mode change or
 *         auto-negotation
 ***********************************************************************/

static void ns7520_link_update_egcr( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;
	unsigned int unEGCR;
	unsigned int unMAC2;
	unsigned int unIPGT;

	DEBUG_FN( DEBUG_ETH );

	unEGCR = *get_eth_reg_addr( NS7520_ETH_EGCR );
	unMAC2 = *get_eth_reg_addr( NS7520_ETH_MAC2 );
	unIPGT = *get_eth_reg_addr( NS7520_ETH_IPGT ) & ~NS7520_ETH_IPGT_IPGT;

	unEGCR &= ~NS7520_ETH_EGCR_EFULLD;
	unMAC2 &= ~NS7520_ETH_MAC2_FULLD;
	if( (pPriv->uiLastLinkStatus & PHY_LXT971_STAT2_DUPLEX_MODE) 
	    == PHY_LXT971_STAT2_DUPLEX_MODE )
	{
	    unEGCR |= NS7520_ETH_EGCR_EFULLD;
	    unMAC2 |= NS7520_ETH_MAC2_FULLD;
	    unIPGT |= 0x15;	// see [1] p. 167
	} else
	    unIPGT |= 0x12;	// see [1] p. 167

	*get_eth_reg_addr( NS7520_ETH_MAC2 ) = unMAC2;
	*get_eth_reg_addr( NS7520_ETH_EGCR ) = unEGCR;
	*get_eth_reg_addr( NS7520_ETH_IPGT ) = unIPGT;
}

/***********************************************************************
 * @Function: ns7520_link_print_changed
 * @Return: void
 * @Descr: checks whether the link status has changed and if so prints
 *         the new mode
 ***********************************************************************/

static void ns7520_link_print_changed( struct net_device* pDev )
{
	struct ns7520_private_t* pPriv = (struct ns7520_private_t*) pDev->priv;
	unsigned short uiStatus;
	unsigned short uiControl;
	
	DEBUG_FN( DEBUG_ETH );

	uiControl = ns7520_mii_read( PHY_COMMON_CTRL );

	if( (uiControl & PHY_COMMON_CTRL_AUTO_NEG) == PHY_COMMON_CTRL_AUTO_NEG )
	{
	    // PHY_COMMON_STAT_LNK_STAT is only set on autonegotiation
	    uiStatus = ns7520_mii_read( PHY_COMMON_STAT );

	    if( !( uiStatus & PHY_COMMON_STAT_LNK_STAT) )
	    {
		printk( KERN_WARNING NS7520_DRIVER_NAME ": link down\n" );
		netif_carrier_off( pDev );
	    } else {
		if( !netif_carrier_ok( pDev ) )
		    // we are online again, enable it
		    netif_carrier_on( pDev );
		
		if( PhyDetected == PHY_LXT971A )
		{
		    uiStatus = ns7520_mii_read( PHY_LXT971_STAT2 );
		    uiStatus &= (PHY_LXT971_STAT2_100BTX |
				 PHY_LXT971_STAT2_DUPLEX_MODE |
				 PHY_LXT971_STAT2_AUTO_NEG );
		    
		    // mask out all uninteresting parts
		}
		// other PHYs must store there link information in uiStatus
		// as PHY_LXT971
	    }
	} else {
	    //  mode has been forced, so uiStatus should be the same as the last
	    //  link status, enforce printing
	    uiStatus = pPriv->uiLastLinkStatus;
	    pPriv->uiLastLinkStatus = 0xff;
	}
	
	if( uiStatus != pPriv->uiLastLinkStatus )
	{
	    // save current link status
	    pPriv->uiLastLinkStatus = uiStatus;

	    // print new link status 
	    
	    printk( KERN_INFO NS7520_DRIVER_NAME
		    ": link mode %i Mbps %s duplex %s\n",
		    (uiStatus & PHY_LXT971_STAT2_100BTX) ? 100 : 10,
		    (uiStatus & PHY_LXT971_STAT2_DUPLEX_MODE)?"full":"half",
		    (uiStatus & PHY_LXT971_STAT2_AUTO_NEG)?"(auto)":"" );
	}
}

/***********************************************************************
 * the MII low level stuff
 ***********************************************************************/

/***********************************************************************
 * @Function: ns7520_mii_identify_phy
 * @Return: 1 if supported PHY has been detected otherwise 0
 * @Descr: checks for supported PHY and prints the IDs.
 ***********************************************************************/

static char ns7520_mii_identify_phy( void )
{
	unsigned short uiID1;
	unsigned short uiID2;
	unsigned char* szName;
	char cRes = 0;
	
	DEBUG_FN( DEBUG_MII );
	
	PhyDetected = (PhyType) uiID1 = ns7520_mii_read( PHY_COMMON_ID1 );

	switch( PhyDetected )
	{
	    case PHY_LXT971A:
		szName = "LXT971A";
		uiID2 = ns7520_mii_read( PHY_COMMON_ID2 );
		nPhyMaxMdioClock = PHY_LXT971_MDIO_MAX_CLK;
		cRes = 1;
		break;
	    case PHY_NONE:
	    default:
		// in case uiID1 == 0 && uiID2 == 0 we may have the wrong
		// address or reset sets the wrong NS7520_ETH_MCFG_CLKS
		
		uiID2 = 0;
		szName = "unknown";
		nPhyMaxMdioClock = PHY_MDIO_MAX_CLK;
		PhyDetected = PHY_NONE;
	}

	printk( KERN_INFO NS7520_DRIVER_NAME
		": PHY (0x%x, 0x%x) = %s detected\n",
		uiID1,
		uiID2,
		szName );

	return cRes;
}


/***********************************************************************
 * @Function: ns7520_mii_read
 * @Return: the data read from PHY register uiRegister
 * @Descr: the data read may be invalid if timed out. If so, a message
 *         is printed but the invalid data is returned.
 *         The fixed device address is being used.
 ***********************************************************************/

static unsigned short ns7520_mii_read( unsigned short uiRegister )
{
	DEBUG_FN( DEBUG_MII_LOW );

	// write MII register to be read
	*get_eth_reg_addr( NS7520_ETH_MADR ) = NS7520_ETH_PHY_ADDRESS|uiRegister;

	*get_eth_reg_addr( NS7520_ETH_MCMD ) = NS7520_ETH_MCMD_READ;

	if( !ns7520_mii_poll_busy() )
	    printk( KERN_WARNING NS7520_DRIVER_NAME
		    ": MII still busy in read\n" );
	// continue to read

	// as of [3] MII.c ns7520_mii_read the bit is not cleared automatically
	// by the NS7520
	*get_eth_reg_addr( NS7520_ETH_MCMD ) = 0;

	return (unsigned short) (*get_eth_reg_addr( NS7520_ETH_MRDD ) );
}


/***********************************************************************
 * @Function: ns7520_mii_write
 * @Return: nothing
 * @Descr: writes the data to the PHY register. In case of a timeout,
 *         no special handling is performed but a message printed
 *         The fixed device address is being used.
 ***********************************************************************/

static void ns7520_mii_write( unsigned short uiRegister, unsigned short uiData )
{
	DEBUG_FN( DEBUG_MII_LOW );

	// write MII register to be written
	*get_eth_reg_addr( NS7520_ETH_MADR ) = NS7520_ETH_PHY_ADDRESS|uiRegister;

	*get_eth_reg_addr( NS7520_ETH_MWTD ) = uiData;

	if( !ns7520_mii_poll_busy() )
	    printk( KERN_WARNING NS7520_DRIVER_NAME 
		    ": MII still busy in write\n");
}


/***********************************************************************
 * @Function: ns7520_mii_poll_busy
 * @Return: 0 if timed out otherwise the remaing timeout
 * @Descr: waits until the MII has completed a command or it times out
 *         code may be interrupted by hard interrupts.
 *         It is not checked what happens on multiple actions when
 *         the first is still being busy and we timeout.
 ***********************************************************************/

static unsigned int ns7520_mii_poll_busy( void )
{
	// I don't have any real figures how long an MDI read or write
	// can last, so I took it from [3] MII.c (ns7520_mii_poll_busy). But
	// in [2] p.24 we have about 40 bits and on [2] 
	// p.69 we see a clock time of 125ns => about 6us each. Add a
	// little bit of overhead. So these amount of iterations
	// should be enough 
	unsigned int unTimeout = 1000;

	DEBUG_FN( DEBUG_MII_LOW );

	while( (( *get_eth_reg_addr( NS7520_ETH_MIND ) & NS7520_ETH_MIND_BUSY) 
		== NS7520_ETH_MIND_BUSY ) && 
	       unTimeout )
	       unTimeout--;

	return unTimeout;
}

/***********************************************************************
 * @Function: ns7520_mii_get_clock_divisor
 * @Return: the clock divisor that should be used in NS7520_ETH_MCFG_CLKS
 * @Descr: if no clock divisor can be calculated for the
 *         current SYSCLK and the maximum MDIO Clock, a warning is printed
 *         and the greatest divisor is taken
 ***********************************************************************/

static unsigned int ns7520_mii_get_clock_divisor( unsigned int unMaxMDIOClk )
{
	struct 
	{
		unsigned int unSysClkDivisor;
		unsigned int unClks; // field for NS7520_ETH_MCFG_CLKS
	} PHYClockDivisors[] =
	{ 
	    {  4, NS7520_ETH_MCFG_CLKS_4 },
	    {  6, NS7520_ETH_MCFG_CLKS_6 },
	    {  8, NS7520_ETH_MCFG_CLKS_8 },
	    { 10, NS7520_ETH_MCFG_CLKS_10 },
	    { 14, NS7520_ETH_MCFG_CLKS_14 },
	    { 20, NS7520_ETH_MCFG_CLKS_20 },
	    { 28, NS7520_ETH_MCFG_CLKS_28 }
	};
	
	int nIndexSysClkDiv;
	int nArraySize = sizeof(PHYClockDivisors) / sizeof(PHYClockDivisors[0]);
	unsigned int unClks = NS7520_ETH_MCFG_CLKS_28; // defaults to
	// greatest div

	DEBUG_FN( DEBUG_INIT );

	for( nIndexSysClkDiv=0; nIndexSysClkDiv < nArraySize; nIndexSysClkDiv++ )
	{
	    // find first sysclock divisor that isn't higher than 2.5 MHz clock
	    if( NETARM_XTAL_FREQ /
		PHYClockDivisors[nIndexSysClkDiv].unSysClkDivisor<=unMaxMDIOClk)
	    {
		unClks = PHYClockDivisors[ nIndexSysClkDiv ].unClks;
		break;
	    }
	}

	DEBUG_ARGS( DEBUG_INIT,
		    "Taking MDIO Clock bit mask 0x%0x for max clock %i\n",
		    unClks,
		    unMaxMDIOClk );
		    
	// return greatest divisor
	return unClks;
}

/***********************************************************************
 * MULTICAST ADDRESS HASH TABLE FUNCTIONS
 * Taken from netarm_eth.c and [1]
 ***********************************************************************/


/***********************************************************************
 * @Function: ns7520_load_mca_hash_table
 * @Return: nothing
 * @Descr: This function generates the multicast address hash table from the
 *         multicast addresses contained in the provided linked list.  This
 *         function loads this table into the Ethernet controller.
 ***********************************************************************/

static void ns7520_load_mca_hash_table( struct dev_mc_list* pMcList )
{
	unsigned short      auiHashTable[ 4 ];
	struct dev_mc_list* pMcListEntry;
	int                 nBitNumber;
	
	// Generate the hash table.
	
	memset( auiHashTable, 0, sizeof( auiHashTable ) );
	
	for( pMcListEntry = pMcList;
	     pMcListEntry != NULL;
	     pMcListEntry = pMcListEntry->next)
	{
	    DEBUG_ARGS( DEBUG_MINOR,
			"  multicast address:  %X %X %X %X %X %X\n",
			pMcListEntry->dmi_addr[0], pMcListEntry->dmi_addr[1],
			pMcListEntry->dmi_addr[2], pMcListEntry->dmi_addr[3],
			pMcListEntry->dmi_addr[4], pMcListEntry->dmi_addr[5] );

	    nBitNumber = ns7520_calculate_hash_bit( pMcListEntry->dmi_addr );
	    ns7520_set_hash_bit( (unsigned char *) auiHashTable, nBitNumber );
	}
	DEBUG_ARGS( DEBUG_MINOR,
		    "  hash table:  %hX %hX %hX %hX\n",
		    auiHashTable[ 0 ], auiHashTable[ 1 ],
		    auiHashTable[ 2 ], auiHashTable[ 3 ] );    

	// Load the hash table into the Ethernet controller.

	*get_eth_reg_addr( NS7520_ETH_HT1 ) = auiHashTable[ 0 ];
	*get_eth_reg_addr( NS7520_ETH_HT2 ) = auiHashTable[ 1 ];
	*get_eth_reg_addr( NS7520_ETH_HT3 ) = auiHashTable[ 2 ];
	*get_eth_reg_addr( NS7520_ETH_HT4 ) = auiHashTable[ 3 ];
}

/***********************************************************************
 * @Function: ns7520_set_hash_bit
 * @Return: nothing
 * @Descr: sets a bit in the hash table
 ***********************************************************************/

static void ns7520_set_hash_bit( unsigned char* pucHashTable, int nBit )
{
	int nByteIndex;
	int nBitIndex;
	
	nByteIndex = nBit >> 3;
	nBitIndex = nBit & 7;
	pucHashTable[ nByteIndex ] |= (1 << nBitIndex );
}


/***********************************************************************
 * @Function: ns7520_calculate_hash_bit
 * @Return: bit position to set in hash table
 * @Descr:  This routine calculates the bit to set in the CRC hash table
 *          to enable receipt of the specified multicast address.
 ***********************************************************************/

static int ns7520_calculate_hash_bit( unsigned char* pucMac )
{
	unsigned short  unCopyMac[ 3 ];
	unsigned short* punMac;
	unsigned short  unBp;
	unsigned short  unBx;
	unsigned long   ulCrc;
	int             nResult;
	int             nMacWord;
	int             nBitIndex;

	// pucMac may not be 32 bit aligned
	memcpy( unCopyMac, pucMac, sizeof( unCopyMac ) );
	
	punMac = unCopyMac;
	ulCrc = 0xffffffffL;
	
	for( nMacWord = 0; nMacWord < 3; nMacWord++ )
	{
	    unBp = *punMac;                            // get word of address
	    punMac++;
	    
	    for( nBitIndex = 0; nBitIndex < 16; nBitIndex++ )
	    {
		unBx = (unsigned short) (ulCrc >> 16);  // get high word of crc
		unBx = ns7520_rotate(unBx,NS7520_LEFT,1); // bit 31 to lsb
		unBx ^= unBp;                           // combine with incoming
		ulCrc <<= 1;                           // shift crc left 1 bit
		unBx &= 1;                             // get control bit
		if( unBx )                             // if bit set
		    ulCrc ^= NS7520_POLYNOMIAL;        // xor crc with polynomial
		ulCrc |= unBx;                         // or in control bit
		unBp = ns7520_rotate( unBp, NS7520_RIGHT, 1 );
	    }
	}
	
	// CRC calculation done.  The 6-bit result resides in bits locations
	// 28:23.
	
	nResult = (ulCrc >> 23) & 0x3f;
	
	return nResult;
}


/***********************************************************************
 * @Function: ns7520_rotate
 * @Return: rotated short
 * @Descr: 
 *      rotates an unsigned short a number of bit
 *      reg         value to rotate
 *      direction   NS7520_LEFT or NS7520_RIGHT
 *      bits        # of bits to rotate
 ***********************************************************************/

static unsigned short ns7520_rotate( unsigned short uiReg,
				     int nDirection,
				     int nBits )
{
	int i;
	
	if( nDirection == NS7520_LEFT )
	{
	    for( i = 0; i < nBits; i++ )
	    {
		if( uiReg & 0x8000 )            // if high bit set
		    uiReg = (uiReg << 1) | 1;   // shift left and or in 1
		else
		    uiReg <<= 1;
	    }
	} else {                                // if ror
	    for( i = 0; i < nBits; i++ )
	    {
		if( uiReg & 1 )                 // if low bit set
		    uiReg = (uiReg>>1)|0x8000;  // shift right and or in hi bit 
		else
		    uiReg >>= 1;
	    }
	}

	return uiReg;
}

// ns7520_init_module, ns7520_cleanup_module at end as of linux convention

/***********************************************************************
 * @Function: ns7520_init_module
 * @Return: 0 if successfull otherwise error
 * @Descr: initialized the NS7520 ethernet interface. Doesn't perform
 *         any hardware detection.
 ***********************************************************************/

static int ns7520_init_module( void )
{
	int nRes = 0;
	
	printk( KERN_INFO "NS7520 Ethernet Driver Initialized\n" );

	if( ( nRes = register_netdev( &ns7520_devs ) ) )
	{
	    printk( KERN_INFO NS7520_DRIVER_NAME ": Error %i registering %s\n",
		    nRes,
		    ns7520_devs.name );

	    return ENODEV;
	}

	return 0;
}


/***********************************************************************
 * @Function: ns7520_cleanup_module
 * @Return: nothing
 * @Descr: deinitialized module
 ***********************************************************************/

static void ns7520_cleanup_module( void )
{
	printk( KERN_INFO "NS7520 Ethernet Driver removing\n" );

	unregister_netdev( &ns7520_devs );
	// unregister_netdev accesses the stats, so kfree must be later
        kfree( ns7520_devs.priv );
}

module_init( ns7520_init_module );
module_exit( ns7520_cleanup_module );

MODULE_AUTHOR( "Markus Pietrek <mpietrek@fsforth.de>");
MODULE_DESCRIPTION("NS7520 Ethernet Driver On UNC20 From FS Forth Systeme GmbH");
MODULE_LICENSE("GPL");
