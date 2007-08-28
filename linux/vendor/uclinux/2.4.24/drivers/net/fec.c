/*
 * Fast Ethernet Controller (FEC) driver for Motorola MPC8xx.
 * Copyright (c) 1997 Dan Malek (dmalek@jlc.net)
 *
 * This version of the driver is specific to the FADS implementation,
 * since the board contains control registers external to the processor
 * for the control of the LevelOne LXT970 transceiver.  The MPC860T manual
 * describes connections using the internal parallel port I/O, which
 * is basically all of Port D.
 *
 * Right now, I am very watseful with the buffers.  I allocate memory
 * pages and then divide them into 2K frame buffers.  This way I know I
 * have buffers large enough to hold one frame within one buffer descriptor.
 * Once I get this working, I will use 64 or 128 byte CPM buffers, which
 * will be much more memory efficient and will easily handle lots of
 * small packets.
 *
 * Much better multiple PHY support by Magnus Damm.
 * Copyright (c) 2000 Ericsson Radio Systems AB.
 *
 * Support for FEC controller of ColdFire/5272/5282/5280.
 * Copyright (c) 2001-2003 Greg Ungerer (gerg@snapgear.com)
 *
 * Added first level of IOCTL interface
 * Copyright (c) 2002 Arcturus Networks Inc. 
 *               by MaTed (Ted Ma - mated@ArcutrusNetworks.com)
 *
 * CHANGES
 *   2003/06/18 ...MaTed
 * - Fixed scanning of PHYS to scan ALL
 * - Generalized interrupt vector. Was Magic numbers - now defines
 *   near beginning of this file
 * - replaced magic numbers by defines
 * - fixed open / close pair. ifconfig up/down now builds / tears
 *   down the descriptors. skb's allocated and released. Some of
 *   the code previously in init() has migrated to open()
 * - Added support for uC5272 v1.3 board with PHY interrupt enabled
 * - Added support for uC5282 v1.0 board with LXT971
 * - shut down queue when link is down (and wake it later when
 *   queue comes back up
 *   2003/08/01  .. Phil Wilshire (PSW)
 *      moved the fec enable to later in the init sequence
 *      stops hangs on the 5282EVB board with some bootloaders
 *   2003/09/03  -  Heiko Degenhardt
 *      - Added support for the RTL8201BL.
 *      - changed fecp->fec_mii_speed and fep->phy_speed to 0x1a
 *        (orig: 0x0e), because that seems to be more correct at
 *        64MHz.
 *        ATTENTION! I currently don't know if that breaks other
 *        stuff!
 *      - Omitted the " && phytype != 0" in the if clause in
 *        the if clause in mii_discover_phy.
 *        ATTENTION! I currently don't know if that breaks other
 *        stuff!
 *   2003/09/23  - Michael Durrant 
 *      - Added back the " && phytype != 0" so that we ensure we are 
 *        using valid phytypes not 0x0000 or 0xffff.
 *        patch from (patch from Ted Ma)
 *      - The fecp->fec_mii_speed and fep->phy_speed needs to be a
 *        caluclation specific to the MCF5272FEC, MCF5282FEC 
 *        and system clocks.  (patch from Michael Leslie) 
 *   2003/10/21  - Heiko Degenhardt
 *      - Correction/redo of support for the RTL8201BL (of DNP/5280)
 *        (backed out by Gerg because of "collisions" with other
 *        changes in cvs)
 *   2003-10-11  - jens at familie-heilig.net
 *      - Added support National 8384 (Future/NetBurner "Badge" board)
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>

#if defined(CONFIG_M5272) || defined(CONFIG_M5282) || defined(CONFIG_M5280)
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/virtconvert.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include "fec.h"
#define __clear_user(a,l)       memset(a, 0, l)
#else
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include "commproc.h"
#endif

#if defined(CONFIG_FEC_IOCTL)
#include <linux/fec_ioctl.h>		// only for IOCTL code
#endif


/* List of PHYs we wish to support.
*/
#define CONFIG_FEC_LXT970
#define CONFIG_FEC_LXT971
#define CONFIG_FEC_QS6612
#define CONFIG_FEC_AM79C874
#define CONFIG_FEC_LXT972
#define CONFIG_FEC_DP83847
/* 
 * We ifdef our changes to our board to not affect other plattforms
 */
#if defined(CONFIG_DNP5280)
#define CONFIG_FEC_RTL8201BL
#endif

static int opened = 0;

/*
 * Defines for the PHY Interrupt Status Change
 */
#if defined(CONFIG_M5272)
#define M5272_PHY_STAT_INT
# if defined(CONFIG_BOARD_UC5272)
extern unsigned char fec_hwaddr[6];
#define PHY_INT			MCF_INT_INT6
#define PHY_START_ADDR	1
#define ICR_PHY_REG		(MCF_MBAR + MCFSIM_ICR4)
#define ICR_PHY_MASK_IPL	0x77777777
#  if defined(CONFIG_UC5272_PHY_INT)
 #define ICR_PHY_MASK	0xFF0FFFFF
 #define ICR_PHY_SETUP	0x00d00000
 #define ICR_MASK_IP	0x00800000
#  else /* CONFIG_UC5272_PHY_INT */
 #undef  M5272_PHY_STAT_INT
 #define ICR_PHY_MASK	0xFFFFFFFF
 #define ICR_PHY_SETUP	0x00000000
 #define ICR_MASK_IP	0x00000000
#  endif /* CONFIG_UC5272_PHY_INT */
# else /* CONFIG_BOARD_UC5272 */
#  if defined(CONFIG_FEC_KS8995M)
  #undef  M5272_PHY_STAT_INT
  #define PHY_START_ADDR	5
  #define ICR_PHY_MASK	0xFFFFFFFF
  #define ICR_PHY_SETUP	0x00000000
  #define ICR_MASK_IP	0x00000000
#  else /*CONFIG_FEC_KS8995M */
// These are values for ColdFire 5272 SIM
#define PHY_INT			MCF_INT_INT2
  #define PHY_START_ADDR	0
  #define ICR_PHY_REG		(MCF_MBAR + MCFSIM_ICR1)
  #define ICR_PHY_MASK_IPL	0x77777777
  #define ICR_PHY_MASK	0x70777777
  #define ICR_MASK_IP	0x08000000
  #define ICR_PHY_SETUP	0x0d000000
#  endif /* CONFIG_FEC_KS8995M */
# endif /* CONFIG_BOARD_UC5272 */
#endif /* CONFIG_M5272 */

/**********
 * Debug
 * - Currently only added for IOCTL Functions
 *   But feel free to add more ...MaTed---
 **********/
#define FEC_DEBUG 1  // higher numbers give more debug 7 is highest
                     //  so far ...MaTed---

/*
 * Define the fixed address of the FEC hardware.
 */
#if defined(CONFIG_M5272)
static volatile fec_t   *fec_hwp = (volatile fec_t *) (MCF_MBAR + 0x840);
static ushort		my_enet_addr[] = { 0x00d0, 0xcf00, 0x0072 };
#elif (defined(CONFIG_M5282) || defined(CONFIG_M5280)) 
static volatile fec_t   *fec_hwp = (volatile fec_t *) (MCF_MBAR + 0x1000);
static ushort		my_enet_addr[] = { 0x00d0, 0xcf00, 0x0082 };
#else
static volatile fec_t	*fec_hwp = &(((immap_t *)IMAP_ADDR)->im_cpm.cp_fec)
static ushort		my_enet_addr[3];
#endif


/* Forward declarations of some structures to support different PHYs
*/

typedef struct {
	uint mii_data;
	void (*funct)(uint mii_reg, struct net_device *dev);
} phy_cmd_t;

typedef struct {
	uint id;
	char *name;

	const phy_cmd_t *config;
	const phy_cmd_t *startup;
	const phy_cmd_t *ack_int;
	const phy_cmd_t *shutdown;
} phy_info_t;

/* The number of Tx and Rx buffers.  These are allocated from the page
 * pool.  The code may assume these are power of two, so it it best
 * to keep them that size.
 * We don't need to allocate pages for the transmitter.  We just use
 * the skbuffer directly.
 */
#if 0
#define FEC_ENET_RX_PAGES	4
#define FEC_ENET_RX_FRSIZE	2048
#define FEC_ENET_RX_FRPPG	(PAGE_SIZE / FEC_ENET_RX_FRSIZE)
#define RX_RING_SIZE		(FEC_ENET_RX_FRPPG * FEC_ENET_RX_PAGES)
#define TX_RING_SIZE		8	/* Must be power of two */
#define TX_RING_MOD_MASK	7	/*   for this to work */
#else
#define FEC_ENET_RX_PAGES	16
#define FEC_ENET_RX_FRSIZE	2048
#define FEC_ENET_RX_FRPPG	(PAGE_SIZE / FEC_ENET_RX_FRSIZE)
#define RX_RING_SIZE		(FEC_ENET_RX_FRPPG * FEC_ENET_RX_PAGES)
#define TX_RING_SIZE		16	/* Must be power of two */
#define TX_RING_MOD_MASK	15	/*   for this to work */
#endif

/* Interrupt events/masks.
*/
#define FEC_ENET_HBERR	((uint)0x80000000)	/* Heartbeat error */
#define FEC_ENET_BABR	((uint)0x40000000)	/* Babbling receiver */
#define FEC_ENET_BABT	((uint)0x20000000)	/* Babbling transmitter */
#define FEC_ENET_GRA	((uint)0x10000000)	/* Graceful stop complete */
#define FEC_ENET_TXF	((uint)0x08000000)	/* Full frame transmitted */
#define FEC_ENET_TXB	((uint)0x04000000)	/* A buffer was transmitted */
#define FEC_ENET_RXF	((uint)0x02000000)	/* Full frame received */
#define FEC_ENET_RXB	((uint)0x01000000)	/* A buffer was received */
#define FEC_ENET_MII	((uint)0x00800000)	/* MII interrupt */
#define FEC_ENET_EBERR	((uint)0x00400000)	/* SDMA bus error */
#define FEC_ENET_UMINT	((uint)0x00200000)	/* Unmasked Interrupt Status */
#define FEC_ENET_ALLINT	((uint)0xffe00000)	/* all the bits */

/* The FEC stores dest/src/type, data, and checksum for receive packets.
 */
#define PKT_MAXBUF_SIZE		1518
#define PKT_MINBUF_SIZE		64
#define PKT_MAXBLR_SIZE		1520

/* The FEC buffer descriptors track the ring buffers.  The rx_bd_base and
 * tx_bd_base always point to the base of the buffer descriptors.  The
 * cur_rx and cur_tx point to the currently available buffer.
 * The dirty_tx tracks the current buffer that is being sent by the
 * controller.  The cur_tx and dirty_tx are equal under both completely
 * empty and completely full conditions.  The empty/ready indicator in
 * the buffer descriptor determines the actual condition.
 */
struct fec_enet_private {
	/* The saved address of a sent-in-place packet/buffer, for skfree(). */
	struct	sk_buff* tx_skbuff[TX_RING_SIZE];
	ushort	skb_cur;
	ushort	skb_dirty;

	/* CPM dual port RAM relative addresses.
	*/
	cbd_t	*rx_bd_base;		/* Address of Rx and Tx buffers. */
	cbd_t	*tx_bd_base;
	cbd_t	*cur_rx, *cur_tx;		/* The next free ring entry */
	cbd_t	*dirty_tx;	/* The ring entries to be free()ed. */
	struct	net_device_stats stats;
	uint	tx_full;
	spinlock_t lock;

	uint	phy_id;
	uint	phy_id_done;
	uint	phy_status;
	uint	phy_speed;
	phy_info_t	*phy;
	struct tq_struct phy_task;

	uint	sequence_done;

	uint	phy_addr;

	int	link;
	int	old_link;
	int	full_duplex;

#if defined(CONFIG_FEC_IOCTL)
  /*************************************
   * global saves for fec_ioctl routines
   *************************************/
  u16		fec_ioctl_regval;	// saved value for last register requested
  u16		fec_ioctl_reg;		// last requested register
  u16		fec_ioctl_wait;		/* have a register value waiting
								     0 => no outstanding register value
									 1 => there is register value 
									    (max allowed is one outstanding queued
										 read request)
									 2 => waiting for queued command to finish
								*/
  u16		fec_ioctl_reserve;	/* Reserved to word align */
#endif
};

static int fec_enet_open(struct net_device *dev);
static int fec_enet_start_xmit(struct sk_buff *skb, struct net_device *dev);
static void fec_enet_mii(struct net_device *dev);
static void fec_enet_interrupt(int irq, void * dev_id, struct pt_regs * regs);
static void  fec_enet_tx(struct net_device *dev);
static void  fec_enet_rx(struct net_device *dev);
static int fec_enet_close(struct net_device *dev);
static struct net_device_stats *fec_enet_get_stats(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static void fec_restart(struct net_device *dev, int duplex);
static void fec_stop(struct net_device *dev);
static void	fec_enet_setup_hw_p1( struct net_device *dev);
static void fec_enet_setup_hw_p2( struct net_device *dev);
static void mii_discover_phy(uint mii_reg, struct net_device *dev);

#if defined(CONFIG_FEC_IOCTL)
static int fec_ioctl( struct net_device *dev, struct ifreq *rq, int cmd);
#endif

/* MII processing.  We keep this as simple as possible.  Requests are
 * placed on the list (if there is room).  When the request is finished
 * by the MII, an optional function may be called.
 */
typedef struct mii_list {
	uint	mii_regval;
	void	(*mii_func)(uint val, struct net_device *dev);
	struct	mii_list *mii_next;
} mii_list_t;

#define		NMII	20
mii_list_t	mii_cmds[NMII];
mii_list_t	*mii_free;
mii_list_t	*mii_head;
mii_list_t	*mii_tail;

static int	mii_queue(struct net_device *dev, int request, 
				void (*func)(uint, struct net_device *));

/* Make MII read/write commands for the FEC.
*/
#define mk_mii_read(REG)	(0x60020000 | ((REG & 0x1f) << 18))
#define mk_mii_write(REG, VAL)	(0x50020000 | ((REG & 0x1f) << 18) | \
						(VAL & 0xffff))
#define mk_mii_end	0

/* Transmitter timeout.
*/
#define TX_TIMEOUT (2*HZ)

/* Register definitions for the PHY.
*/

#define MII_REG_CR          0  /* Control Register                         */
#define MII_REG_SR          1  /* Status Register                          */
#define MII_REG_PHYIR1      2  /* PHY Identification Register 1            */
#define MII_REG_PHYIR2      3  /* PHY Identification Register 2            */
#define MII_REG_ANAR        4  /* A-N Advertisement Register               */ 
#define MII_REG_ANLPAR      5  /* A-N Link Partner Ability Register        */
#define MII_REG_ANER        6  /* A-N Expansion Register                   */
#define MII_REG_ANNPTR      7  /* A-N Next Page Transmit Register          */
#define MII_REG_ANLPRNPR    8  /* A-N Link Partner Received Next Page Reg. */

/* values for phy_status */

#define PHY_CONF_ANE	0x0001  /* 1 auto-negotiation enabled */
#define PHY_CONF_LOOP	0x0002  /* 1 loopback mode enabled */
#define PHY_CONF_SPMASK	0x00f0  /* mask for speed */
#define PHY_CONF_10HDX	0x0010  /* 10 Mbit half duplex supported */
#define PHY_CONF_10FDX	0x0020  /* 10 Mbit full duplex supported */ 
#define PHY_CONF_100HDX	0x0040  /* 100 Mbit half duplex supported */
#define PHY_CONF_100FDX	0x0080  /* 100 Mbit full duplex supported */ 

#define PHY_STAT_LINK	0x0100  /* 1 up - 0 down */
#define PHY_STAT_FAULT	0x0200  /* 1 remote fault */
#define PHY_STAT_ANC	0x0400  /* 1 auto-negotiation complete	*/
#define PHY_STAT_SPMASK	0xf000  /* mask for speed */
#define PHY_STAT_10HDX	0x1000  /* 10 Mbit half duplex selected	*/
#define PHY_STAT_10FDX	0x2000  /* 10 Mbit full duplex selected	*/ 
#define PHY_STAT_100HDX	0x4000  /* 100 Mbit half duplex selected */
#define PHY_STAT_100FDX	0x8000  /* 100 Mbit full duplex selected */


static int
fec_enet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct fec_enet_private *fep;
	volatile fec_t	*fecp;
	volatile cbd_t	*bdp;

	fep = dev->priv;
	fecp = (volatile fec_t*)dev->base_addr;

	if (!fep->link) {
		/* Link is down or autonegotiation is in progress. */
	    netif_stop_queue(dev);
		return 1;
	}

	/* Fill in a Tx ring entry */
	bdp = fep->cur_tx;

#if !defined(final_version)
	if (bdp->cbd_sc & BD_ENET_TX_READY) {
		/* Ooops.  All transmit buffers are full.  Bail out.
		 * This should not happen, since dev->tbusy should be set.
		 */
		printk("%s: tx queue full!.\n", dev->name);
		return 1;
	}
#endif

	/* Clear all of the status flags.
	 */
	bdp->cbd_sc &= ~BD_ENET_TX_STATS;

	/* Set buffer length and buffer pointer.
	*/
	bdp->cbd_bufaddr = __pa(skb->data);
	bdp->cbd_datlen = skb->len;

#if (defined(CONFIG_M5282) || defined(CONFIG_M5280))
	/*
	 *	This is a work around for a problem on the 5282/5280. The FEC
	 *	hardware will transmit an extra 4 bytes at the front of
	 *	the packet if its memory buffer is not aligned on a 
	 *	4 byte address. Checking with Motorola to see what is
	 *	going one here...
	 */
	if (bdp->cbd_bufaddr & 0x2) {
		int i;
		unsigned char *p = (unsigned char *) bdp->cbd_bufaddr;
		for (i = bdp->cbd_datlen-1; (i >= 0); i--)
			p[i+2] = p[i];
		bdp->cbd_bufaddr = (int) &p[2];
	}
#endif

	/* Save skb pointer.
	*/
	fep->tx_skbuff[fep->skb_cur] = skb;

	fep->stats.tx_bytes += skb->len;
	fep->skb_cur = (fep->skb_cur+1) & TX_RING_MOD_MASK;

#if (!defined(CONFIG_M5272) && !defined(CONFIG_M5282) && !defined(CONFIG_M5280))
	/* Push the data cache so the CPM does not get stale memory
	 * data.
	 */
	flush_dcache_range((unsigned long)skb->data,
			   (unsigned long)skb->data + skb->len);
#endif

	spin_lock_irq(&fep->lock);

	/* Send it on its way.  Tell FEC its ready, interrupt when done,
	 * its the last BD of the frame, and to put the CRC on the end.
	 */

	bdp->cbd_sc |= (BD_ENET_TX_READY | BD_ENET_TX_INTR
			| BD_ENET_TX_LAST | BD_ENET_TX_TC);


	dev->trans_start = jiffies;

	/* Trigger transmission start */
	fecp->fec_x_des_active = 0x01000000;

	/* If this was the last BD in the ring, start at the beginning again.
	*/
	if (bdp->cbd_sc & BD_ENET_TX_WRAP) {
		bdp = fep->tx_bd_base;
	} else {
		bdp++;
	}

	if (bdp == fep->dirty_tx) {
		fep->tx_full = 1;
		netif_stop_queue(dev);
	}

	fep->cur_tx = (cbd_t *)bdp;

	spin_unlock_irq(&fep->lock);

	return 0;
}

static void
fec_timeout(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;

	printk("%s: transmit timed out.\n", dev->name);
	fep->stats.tx_errors++;
#if !defined(final_version)
	{
	int	i;
	cbd_t	*bdp;

	printk("Ring data dump: cur_tx %lx%s, dirty_tx %lx cur_rx: %lx\n",
	       (unsigned long)fep->cur_tx, fep->tx_full ? " (full)" : "",
	       (unsigned long)fep->dirty_tx,
	       (unsigned long)fep->cur_rx);

	bdp = fep->tx_bd_base;
	printk(" tx: %u buffers\n",  TX_RING_SIZE);
	for (i = 0 ; i < TX_RING_SIZE; i++) {
		printk("  %08x: %04x %04x %08x\n", 
		       (uint) bdp,
		       bdp->cbd_sc,
		       bdp->cbd_datlen,
		       (int) bdp->cbd_bufaddr);
		bdp++;
	}

	bdp = fep->rx_bd_base;
	printk(" rx: %lu buffers\n",  (unsigned long) RX_RING_SIZE);
	for (i = 0 ; i < RX_RING_SIZE; i++) {
		printk("  %08x: %04x %04x %08x\n",
		       (uint) bdp,
		       bdp->cbd_sc,
		       bdp->cbd_datlen,
		       (int) bdp->cbd_bufaddr);
		bdp++;
	}
	}
#endif
	fec_restart(dev, 0);
	netif_wake_queue(dev);
}

/* The interrupt handler.
 * This is called from the MPC core interrupt.
 */
static	void
fec_enet_interrupt(int irq, void * dev_id, struct pt_regs * regs)
{
	struct	net_device *dev = dev_id;
	volatile fec_t	*fecp;
	uint	int_events;

	fecp = (volatile fec_t*)dev->base_addr;

	/* Get the interrupt events that caused us to be here.
	*/
	while ((int_events = fecp->fec_ievent) != 0) {
		fecp->fec_ievent = int_events;
#if (!defined(CONFIG_M5282) && !defined(CONFIG_M5280))
		if ((int_events & (FEC_ENET_HBERR | FEC_ENET_BABR |
				   FEC_ENET_BABT | FEC_ENET_EBERR)) != 0) {
			printk("FEC ERROR %x\n", int_events);
		}
#endif

		/* Handle receive event in its own function.
		 */
		if (int_events & FEC_ENET_RXF)
			fec_enet_rx(dev);

		/* Transmit OK, or non-fatal error. Update the buffer
		   descriptors. FEC handles all errors, we just discover
		   them as part of the transmit process.
		*/
		if (int_events & FEC_ENET_TXF)
			fec_enet_tx(dev);

		if (int_events & FEC_ENET_MII)
			fec_enet_mii(dev);

	}
}


static void
fec_enet_tx(struct net_device *dev)
{
	struct	fec_enet_private *fep;
	volatile cbd_t	*bdp;
	struct	sk_buff	*skb;

	fep = dev->priv;
	spin_lock(&fep->lock);
	bdp = fep->dirty_tx;

	while ((bdp->cbd_sc&BD_ENET_TX_READY) == 0) {
		if (bdp == fep->cur_tx && fep->tx_full == 0) break;

		skb = fep->tx_skbuff[fep->skb_dirty];
		/* Check for errors. */
		if (bdp->cbd_sc & (BD_ENET_TX_HB | BD_ENET_TX_LC |
				   BD_ENET_TX_RL | BD_ENET_TX_UN |
				   BD_ENET_TX_CSL)) {
			fep->stats.tx_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_HB)  /* No heartbeat */
				fep->stats.tx_heartbeat_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_LC)  /* Late collision */
				fep->stats.tx_window_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_RL)  /* Retrans limit */
				fep->stats.tx_aborted_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_UN)  /* Underrun */
				fep->stats.tx_fifo_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_CSL) /* Carrier lost */
				fep->stats.tx_carrier_errors++;
		} else {
			fep->stats.tx_packets++;
		}

#if !defined(final_version)
		if (bdp->cbd_sc & BD_ENET_TX_READY)
			printk("HEY! Enet xmit interrupt and TX_READY.\n");
#endif
		/* Deferred means some collisions occurred during transmit,
		 * but we eventually sent the packet OK.
		 */
		if (bdp->cbd_sc & BD_ENET_TX_DEF)
			fep->stats.collisions++;
	    
		/* Free the sk buffer associated with this last transmit.
		 */
		dev_kfree_skb_any(skb);
		fep->tx_skbuff[fep->skb_dirty] = NULL;
		fep->skb_dirty = (fep->skb_dirty + 1) & TX_RING_MOD_MASK;
	    
		/* Update pointer to next buffer descriptor to be transmitted.
		 */
		if (bdp->cbd_sc & BD_ENET_TX_WRAP)
			bdp = fep->tx_bd_base;
		else
			bdp++;
	    
		/* Since we have freed up a buffer, the ring is no longer
		 * full.
		 */
		if (fep->tx_full) {
			fep->tx_full = 0;
			if (netif_queue_stopped(dev))
				netif_wake_queue(dev);
		}
	}
	fep->dirty_tx = (cbd_t *)bdp;
	spin_unlock(&fep->lock);
}


/* During a receive, the cur_rx points to the current incoming buffer.
 * When we update through the ring, if the next incoming buffer has
 * not been given to the system, we just set the empty indicator,
 * effectively tossing the packet.
 */
static void
fec_enet_rx(struct net_device *dev)
{
	struct	fec_enet_private *fep;
	volatile fec_t	*fecp;
	volatile cbd_t *bdp;
	struct	sk_buff	*skb;
	ushort	pkt_len;
	__u8 *data;

	fep = dev->priv;
	fecp = (volatile fec_t*)dev->base_addr;

	/* First, grab all of the stats for the incoming packet.
	 * These get messed up if we get called due to a busy condition.
	 */
	bdp = fep->cur_rx;

while (!(bdp->cbd_sc & BD_ENET_RX_EMPTY)) {

#if !defined(final_version)
	/* Since we have allocated space to hold a complete frame,
	 * the last indicator should be set.
	 */
	if ((bdp->cbd_sc & BD_ENET_RX_LAST) == 0)
		printk("FEC ENET: rcv is not +last\n");
#endif

	if (!opened)
		goto rx_processing_done;

	/* Check for errors. */
	if (bdp->cbd_sc & (BD_ENET_RX_LG | BD_ENET_RX_SH | BD_ENET_RX_NO |
			   BD_ENET_RX_CR | BD_ENET_RX_OV)) {
		fep->stats.rx_errors++;       
		if (bdp->cbd_sc & (BD_ENET_RX_LG | BD_ENET_RX_SH)) {
		/* Frame too long or too short. */
			fep->stats.rx_length_errors++;
		}
		if (bdp->cbd_sc & BD_ENET_RX_NO)	/* Frame alignment */
			fep->stats.rx_frame_errors++;
		if (bdp->cbd_sc & BD_ENET_RX_CR)	/* CRC Error */
			fep->stats.rx_crc_errors++;
		if (bdp->cbd_sc & BD_ENET_RX_OV)	/* FIFO overrun */
			fep->stats.rx_fifo_errors++;
	}

	/* Report late collisions as a frame error.
	 * On this error, the BD is closed, but we don't know what we
	 * have in the buffer.  So, just drop this frame on the floor.
	 */
	if (bdp->cbd_sc & BD_ENET_RX_CL) {
		fep->stats.rx_errors++;
		fep->stats.rx_frame_errors++;
		goto rx_processing_done;
	}

	/* Process the incoming frame.
	 */
	fep->stats.rx_packets++;
#if (!defined(CONFIG_M5282) && !defined(CONFIG_M5280))
	if (fecp->fec_max_frm_len & 0x40000000)
		fep->stats.multicast++;
#endif
	pkt_len = bdp->cbd_datlen;
	fep->stats.rx_bytes += pkt_len;
	data = (__u8*)__va(bdp->cbd_bufaddr);

	/* This does 16 byte alignment, exactly what we need.
	 * The packet length includes FCS, but we don't want to
	 * include that when passing upstream as it messes up
	 * bridging applications.
	 */
	skb = dev_alloc_skb(pkt_len-4);

	if (skb == NULL) {
		printk("%s: Memory squeeze, dropping packet.\n", dev->name);
		fep->stats.rx_dropped++;
	} else {
		skb->dev = dev;
		skb_put(skb,pkt_len-4);	/* Make room */
		eth_copy_and_sum(skb,
				 (unsigned char *)__va(bdp->cbd_bufaddr),
				 pkt_len-4, 0);
		skb->protocol=eth_type_trans(skb,dev);
		netif_rx(skb);
	}
rx_processing_done:

	/* Clear the status flags for this buffer.
	*/
	bdp->cbd_sc &= ~BD_ENET_RX_STATS;

	/* Mark the buffer empty.
	*/
	bdp->cbd_sc |= BD_ENET_RX_EMPTY;

	/* Update BD pointer to next entry.
	*/
	if (bdp->cbd_sc & BD_ENET_RX_WRAP)
		bdp = fep->rx_bd_base;
	else
		bdp++;
	
	/* Doing this here will keep the FEC running while we process
	 * incoming frames.  On a heavily loaded network, we should be
	 * able to keep up at the expense of system resources.
	 */
	fecp->fec_r_des_active = 0x01000000;
} /* while (!(bdp->cbd_sc & BD_ENET_RX_EMPTY)) */

	fep->cur_rx = (cbd_t *)bdp;
}


static void
fec_enet_mii(struct net_device *dev)
{
	struct	fec_enet_private *fep;
	volatile fec_t *ep;
	mii_list_t *mip;
	uint mii_reg;

	fep = (struct fec_enet_private *)dev->priv;
	ep = fec_hwp;
	mii_reg = ep->fec_mii_data;

	if ((mip = mii_head) == NULL) {
		printk("MII and no head!\n");
		return;
	}

	if (mip->mii_func != NULL)
		(*(mip->mii_func))(mii_reg, dev);

	mii_head = mip->mii_next;
	mip->mii_next = mii_free;
	mii_free = mip;

	if ((mip = mii_head) != NULL)
		ep->fec_mii_data = mip->mii_regval;
}

static int
mii_queue(struct net_device *dev, int regval, void (*func)(uint, struct net_device *))
{
	struct fec_enet_private *fep;
	unsigned long flags;
	mii_list_t *mip;
	int retval;

	/* Add PHY address to register command.
	*/
	fep = dev->priv;
	regval |= fep->phy_addr << 23;

	retval = 0;

	save_flags(flags);
	cli();

	if ((mip = mii_free) != NULL) {
		mii_free = mip->mii_next;
		mip->mii_regval = regval;
		mip->mii_func = func;
		mip->mii_next = NULL;
		if (mii_head) {
			mii_tail->mii_next = mip;
			mii_tail = mip;
		} else {
			mii_head = mii_tail = mip;
			fec_hwp->fec_mii_data = regval;
		}
	} else {
		retval = 1;
	}

	restore_flags(flags);

	return(retval);
}

static void mii_do_cmd(struct net_device *dev, const phy_cmd_t *c)
{
	int k;

	if(!c)
		return;

	for(k = 0; (c+k)->mii_data != mk_mii_end; k++) {
		mii_queue(dev, (c+k)->mii_data, (c+k)->funct);
	}
}

static void mii_parse_sr(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_LINK | PHY_STAT_FAULT | PHY_STAT_ANC);

	if (mii_reg & 0x0004)
		*s |= PHY_STAT_LINK;
	if (mii_reg & 0x0010)
		*s |= PHY_STAT_FAULT;
	if (mii_reg & 0x0020)
		*s |= PHY_STAT_ANC;
}

static void mii_parse_cr(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_CONF_ANE | PHY_CONF_LOOP);

	if (mii_reg & 0x1000)
		*s |= PHY_CONF_ANE;
	if (mii_reg & 0x4000)
		*s |= PHY_CONF_LOOP;
}

static void mii_parse_anar(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_CONF_SPMASK);

	if (mii_reg & 0x0020)
		*s |= PHY_CONF_10HDX;
	if (mii_reg & 0x0040)
		*s |= PHY_CONF_10FDX;
	if (mii_reg & 0x0080)
		*s |= PHY_CONF_100HDX;
	if (mii_reg & 0x00100)
		*s |= PHY_CONF_100FDX;
}

#if 0
static void mii_disp_reg(uint mii_reg, struct net_device *dev)
{
	printk("reg %u = 0x%04x\n", (mii_reg >> 18) & 0x1f, mii_reg & 0xffff);
}
#endif

/* ------------------------------------------------------------------------- */
/* The Level one LXT970 is used by many boards				     */

#if defined(CONFIG_FEC_LXT970)

#define MII_LXT970_MIRROR    16  /* Mirror register           */
#define MII_LXT970_IER       17  /* Interrupt Enable Register */
#define MII_LXT970_ISR       18  /* Interrupt Status Register */
#define MII_LXT970_CONFIG    19  /* Configuration Register    */
#define MII_LXT970_CSR       20  /* Chip Status Register      */

static void mii_parse_lxt970_csr(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK);

	if (mii_reg & 0x0800) {
		if (mii_reg & 0x1000)
			*s |= PHY_STAT_100FDX;
		else
			*s |= PHY_STAT_100HDX;
	} else {
		if (mii_reg & 0x1000)
			*s |= PHY_STAT_10FDX;
		else
			*s |= PHY_STAT_10HDX;
	}
}

static phy_info_t phy_info_lxt970 = {
	0x07810000, 
	"LXT970",

	(const phy_cmd_t []) {  /* config */
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		{ mk_mii_write(MII_LXT970_IER, 0x0002), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		/* read SR and ISR to acknowledge */
		
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_LXT970_ISR), NULL },

		/* find out the current status */
		
		{ mk_mii_read(MII_LXT970_CSR), mii_parse_lxt970_csr },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_write(MII_LXT970_IER, 0x0000), NULL },
		{ mk_mii_end, }
	},
};
	
#endif /* CONFIG_FEC_LXT970 */

/* ------------------------------------------------------------------------- */
/* The Level one LXT971 is used on some of my custom boards                  */

#if defined(CONFIG_FEC_LXT971)

/* register definitions for the 971 */

#define MII_LXT971_PCR       16  /* Port Control Register     */
#define MII_LXT971_SR2       17  /* Status Register 2         */
#define MII_LXT971_IER       18  /* Interrupt Enable Register */
#define MII_LXT971_ISR       19  /* Interrupt Status Register */
#define MII_LXT971_LCR       20  /* LED Control Register      */
#define MII_LXT971_TCR       30  /* Transmit Control Register */

/* 
 * I had some nice ideas of running the MDIO faster...
 * The 971 should support 8MHz and I tried it, but things acted really
 * wierd, so 2.5 MHz ought to be enough for anyone...
 */

static void mii_parse_lxt971_sr2(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK | PHY_STAT_LINK | PHY_STAT_ANC);

	if (mii_reg & 0x0400) {
		fep->link = 1;
		*s |= PHY_STAT_LINK;
	} else {
		fep->link = 0;
	}
	if (mii_reg & 0x0080)
		*s |= PHY_STAT_ANC;
	if (mii_reg & 0x4000) {
		if (mii_reg & 0x0200)
			*s |= PHY_STAT_100FDX;
		else
			*s |= PHY_STAT_100HDX;
	} else {
		if (mii_reg & 0x0200)
			*s |= PHY_STAT_10FDX;
		else
			*s |= PHY_STAT_10HDX;
	}
	if (mii_reg & 0x0008)
		*s |= PHY_STAT_FAULT;
}

static phy_info_t phy_info_lxt971 = {
	0x0001378e, 
	"LXT971",
	
	(const phy_cmd_t []) {  /* config */  
		/* limit to 10MBit because my protorype board 
		 * doesn't work with 100. */
#if (defined(CONFIG_M5272) || defined(CONFIG_M5282) || defined(CONFIG_M5280))
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_read(MII_LXT971_SR2), mii_parse_lxt971_sr2 },
#else
		{ mk_mii_write(MII_REG_ANAR, 0x061), NULL }, /* 10 MBit */
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
#endif
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		{ mk_mii_write(MII_LXT971_IER, 0x00f2), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_write(MII_LXT971_LCR, 0xd422), NULL }, /* LED config */
		/* Somehow does the 971 tell me that the link is down
		 * the first read after power-up.
		 * read here to get a valid value in ack_int */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr }, 
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		/* find out the current status */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_LXT971_SR2), mii_parse_lxt971_sr2 },
		/* we only need to read ISR to acknowledge */
		{ mk_mii_read(MII_LXT971_ISR), NULL },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_write(MII_LXT971_IER, 0x0000), NULL },
		{ mk_mii_end, }
	},
};

#endif /* CONFIG_FEC_LXT971 */


/* ------------------------------------------------------------------------- */
/* The Level one LXT972 is used on the Arcturus Networks uC5272/uC5282 dimm  */

#if defined(CONFIG_FEC_LXT972)

/* register definitions for the 972 */

#define MII_LXT972_PCR       16  /* Port Control Register     */
#define MII_LXT972_SR2       17  /* Status Register 2         */
#define MII_LXT972_IER       18  /* Interrupt Enable Register */
#define MII_LXT972_ISR       19  /* Interrupt Status Register */
#define MII_LXT972_LCR       20  /* LED Control Register      */
#define MII_LXT972_TCR       30  /* Transmit Control Register */

static void mii_parse_lxt972_sr2(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK | PHY_STAT_LINK | PHY_STAT_ANC);

	if (mii_reg & 0x0400) {
		fep->link = 1;
		*s |= PHY_STAT_LINK;
	} else {
		fep->link = 0;
	}
	if (mii_reg & 0x0080)
		*s |= PHY_STAT_ANC;
	if (mii_reg & 0x4000) {
		if (mii_reg & 0x0200)
			*s |= PHY_STAT_100FDX;
		else
			*s |= PHY_STAT_100HDX;
	} else {
		if (mii_reg & 0x0200)
			*s |= PHY_STAT_10FDX;
		else
			*s |= PHY_STAT_10HDX;
	}
	if (mii_reg & 0x0008)
		*s |= PHY_STAT_FAULT;
}

static phy_info_t phy_info_lxt972 = {
	0x0001378e,
	"LXT972",
	
	(const phy_cmd_t []) {  /* config */  
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_read(MII_LXT972_SR2), mii_parse_lxt972_sr2 },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		{ mk_mii_write(MII_LXT972_IER, 0x00f2), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		/* Somehow does the 972 tell me that the link is down
		 * the first read after power-up.
		 * read here to get a valid value in ack_int */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr }, 
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		/* find out the current status */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_LXT972_SR2), mii_parse_lxt972_sr2 },
		/* we only need to read ISR to acknowledge */
		{ mk_mii_read(MII_LXT972_ISR), NULL },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_write(MII_LXT972_IER, 0x0000), NULL },
		{ mk_mii_end, }
	},
};

#endif /* CONFIG_FEC_LXT972 */


/* ------------------------------------------------------------------------- */
/* The National DP83847 is used on the Future/Netburner Badge Board          */

#if defined(CONFIG_FEC_DP83847)

/* register definitions for the DP83847 */

#define MII_DP83847_PHYSTST       16  /* PHY Status Register */

static void mii_parse_dp83847_sr2(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK | PHY_STAT_LINK | PHY_STAT_ANC);

  // Link up?
	if (mii_reg & 0x0001) {
		fep->link = 1;
		*s |= PHY_STAT_LINK;
	} else {
		fep->link = 0;
	}
	// Status of link
	if (mii_reg & 0x0010)     // Autonegotioation complete?
		*s |= PHY_STAT_ANC;
	if (mii_reg & 0x0002) {   // 10MBps?
		if (mii_reg & 0x0004)   // Full Duplex?
			*s |= PHY_STAT_10FDX;
		else
			*s |= PHY_STAT_10HDX;
	} else {                  // 100 Mbps
		if (mii_reg & 0x0004)   // Full Duplex?
			*s |= PHY_STAT_100FDX;
		else
			*s |= PHY_STAT_100HDX;
	}
	if (mii_reg & 0x0008)
		*s |= PHY_STAT_FAULT;
}

static phy_info_t phy_info_dp83847= {
	0x020005c3, // oder 0x80017?
	"DP83847",
	
	(const phy_cmd_t []) {  /* config */  
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_read(MII_DP83847_PHYSTST), mii_parse_dp83847_sr2 },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		// { mk_mii_write(MII_LXT972_IER, 0x00f2), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr }, 
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int - never happens, we don't have an interrupt */
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown */
		{ mk_mii_end, }
	},
};

#endif /* CONFIG_FEC_DP83847 */


/* ------------------------------------------------------------------------- */
/* The Quality Semiconductor QS6612 is used on the RPX CLLF                  */

#if defined(CONFIG_FEC_QS6612)

/* register definitions */

#define MII_QS6612_MCR       17  /* Mode Control Register      */
#define MII_QS6612_FTR       27  /* Factory Test Register      */
#define MII_QS6612_MCO       28  /* Misc. Control Register     */
#define MII_QS6612_ISR       29  /* Interrupt Source Register  */
#define MII_QS6612_IMR       30  /* Interrupt Mask Register    */
#define MII_QS6612_PCR       31  /* 100BaseTx PHY Control Reg. */

static void mii_parse_qs6612_pcr(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK);

	switch((mii_reg >> 2) & 7) {
	case 1: *s |= PHY_STAT_10HDX; break;
	case 2: *s |= PHY_STAT_100HDX; break;
	case 5: *s |= PHY_STAT_10FDX; break;
	case 6: *s |= PHY_STAT_100FDX; break;
	}
}

static phy_info_t phy_info_qs6612 = {
	0x00181440, 
	"QS6612",
	
	(const phy_cmd_t []) {  /* config */  
//	{ mk_mii_write(MII_REG_ANAR, 0x061), NULL }, /* 10 MBit */

		/* The PHY powers up isolated on the RPX, 
		 * so send a command to allow operation.
		 */

		{ mk_mii_write(MII_QS6612_PCR, 0x0dc0), NULL },

		/* parse cr and anar to get some info */

		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		{ mk_mii_write(MII_QS6612_IMR, 0x003a), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		
		/* we need to read ISR, SR and ANER to acknowledge */
		
		{ mk_mii_read(MII_QS6612_ISR), NULL },
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_REG_ANER), NULL },

		/* read pcr to get info */

		{ mk_mii_read(MII_QS6612_PCR), mii_parse_qs6612_pcr },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_write(MII_QS6612_IMR, 0x0000), NULL },
		{ mk_mii_end, }
	},
};

#endif /* CONFIG_FEC_QS6612 */


/* ------------------------------------------------------------------------- */
/* AMD AM79C874 phy                                                          */

#if defined(CONFIG_FEC_AM79C874)

/* register definitions for the 874 */

#define MII_AM79C874_MFR       16  /* Miscellaneous Feature Register */
#define MII_AM79C874_ICSR      17  /* Interrupt/Status Register      */
#define MII_AM79C874_DR        18  /* Diagnostic Register            */
#define MII_AM79C874_PMLR      19  /* Power and Loopback Register    */
#define MII_AM79C874_MCR       21  /* ModeControl Register           */
#define MII_AM79C874_DC        23  /* Disconnect Counter             */
#define MII_AM79C874_REC       24  /* Recieve Error Counter          */

static void mii_parse_am79c874_dr(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK | PHY_STAT_ANC);

	if (mii_reg & 0x0080)
		*s |= PHY_STAT_ANC;
	if (mii_reg & 0x0400)
		*s |= ((mii_reg & 0x0800) ? PHY_STAT_100FDX : PHY_STAT_100HDX);
	else
		*s |= ((mii_reg & 0x0800) ? PHY_STAT_10FDX : PHY_STAT_10HDX);
}

static phy_info_t phy_info_am79c874 = {
	0x00022561, 
	"AM79C874",
	
	(const phy_cmd_t []) {  /* config */  
		/* limit to 10MBit because my protorype board 
		 * doesn't work with 100. */
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_read(MII_AM79C874_DR), mii_parse_am79c874_dr },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		{ mk_mii_write(MII_AM79C874_ICSR, 0xff00), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr }, 
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		/* find out the current status */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_AM79C874_DR), mii_parse_am79c874_dr },
		/* we only need to read ISR to acknowledge */
		{ mk_mii_read(MII_AM79C874_ICSR), NULL },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_write(MII_AM79C874_ICSR, 0x0000), NULL },
		{ mk_mii_end, }
	},
};

#endif /* CONFIG_FEC_AM79C874 */

/* ------------------------------------------------------------------------- */
/* Kendin KS8995M Phy5 Mode                                                  */

#ifdef CONFIG_FEC_KS8995M_P5

static phy_info_t phy_info_ks8995m = {
	0x00022145, 
	"KS8995M P5",
	
	(const phy_cmd_t []) {  /* config */
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup */
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr }, 
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int - never happens, we don't have an interrupt */
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown */
		{ mk_mii_end, }
	},
};

#endif /* CONFIG_FEC_KS8995M_P5 */

/*
 * RTL8201BL (on DNP5280)
 */
#if defined(CONFIG_FEC_RTL8201BL)

static phy_info_t phy_info_rtl8201bl = {
   0x00000820,
   "RTL8201BL",
	(const phy_cmd_t []) {  /* config */
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup */
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int - never happens, we don't have
									  an interrupt */
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown */
		{ mk_mii_end, }
	},
};


#endif /* CONFIG_FEC_RTL8201BL */


static phy_info_t *phy_info[] = {

#if defined(CONFIG_FEC_LXT970)
	&phy_info_lxt970,
#endif /* CONFIG_FEC_LXT970 */

#if defined(CONFIG_FEC_LXT971)
	&phy_info_lxt971,
#endif /* CONFIG_FEC_LXT971 */

#if defined(CONFIG_FEC_LXT972)
	&phy_info_lxt972,
#endif /* CONFIG_FEC_LXT972 */

#if defined(CONFIG_FEC_QS6612)
	&phy_info_qs6612,
#endif /* CONFIG_FEC_QS6612 */

#if defined(CONFIG_FEC_AM79C874)
	&phy_info_am79c874,
#endif /* CONFIG_FEC_AM79C874 */

#if defined(CONFIG_FEC_KS8995M_P5)
	&phy_info_ks8995m,
#endif /* CONFIG_FEC_AM79C874 */

#if defined(CONFIG_FEC_RTL8201BL)
	&phy_info_rtl8201bl,
#endif /* CONFIG_FEC_RTL8201BL */

#if defined(CONFIG_FEC_DP83847)
	&phy_info_dp83847,
#endif /* CONFIG_FEC_DP83847 */

	NULL
};

static void mii_display_status(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	if (!fep->link && !fep->old_link) {
		/* Link is still down - don't print anything */
		return;
	}

	printk("%s: status: ", dev->name);

	if (!fep->link) {
		printk("link down");
	} else {
		printk("link up");

		switch(*s & PHY_STAT_SPMASK) {
		case PHY_STAT_100FDX: printk(", 100MBit Full Duplex"); break;
		case PHY_STAT_100HDX: printk(", 100MBit Half Duplex"); break;
		case PHY_STAT_10FDX: printk(", 10MBit Full Duplex"); break;
		case PHY_STAT_10HDX: printk(", 10MBit Half Duplex"); break;
		default:
			printk(", Unknown speed/duplex");
		}

		if (*s & PHY_STAT_ANC)
			printk(", auto-negotiation complete");
	}

	if (*s & PHY_STAT_FAULT)
		printk(", remote fault");

	printk(".\n");
}

static void mii_display_config(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	printk("%s: config: auto-negotiation ", dev->name);

	if (*s & PHY_CONF_ANE)
		printk("on");
	else
		printk("off");

	if (*s & PHY_CONF_100FDX)
		printk(", 100FDX");
	if (*s & PHY_CONF_100HDX)
		printk(", 100HDX");
	if (*s & PHY_CONF_10FDX)
		printk(", 10FDX");
	if (*s & PHY_CONF_10HDX)
		printk(", 10HDX");
	if (!(*s & PHY_CONF_SPMASK))
		printk(", No speed/duplex selected?");

	if (*s & PHY_CONF_LOOP)
		printk(", loopback enabled");
	
	printk(".\n");

	fep->sequence_done = 1;
}

static void mii_relink(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	int duplex;

	fep->link = (fep->phy_status & PHY_STAT_LINK) ? 1 : 0;
	mii_display_status(dev);
	fep->old_link = fep->link;

	if (fep->link) {
		duplex = 0;
		if (fep->phy_status & (PHY_STAT_100FDX | PHY_STAT_10FDX))
			duplex = 1;
		fec_restart(dev, duplex);
		if (netif_queue_stopped(dev))
		    netif_wake_queue(dev);

	} else {
		fec_stop(dev);
	}
}

static void mii_queue_relink(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;

	fep->phy_task.routine = (void *)mii_relink;
	fep->phy_task.data = dev;
	schedule_task(&fep->phy_task);
}

static void mii_queue_config(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;

	fep->phy_task.routine = (void *)mii_display_config;
	fep->phy_task.data = dev;
	schedule_task(&fep->phy_task);
}



phy_cmd_t phy_cmd_relink[] = { { mk_mii_read(MII_REG_CR), mii_queue_relink },
			       { mk_mii_end, } };
phy_cmd_t phy_cmd_config[] = { { mk_mii_read(MII_REG_CR), mii_queue_config },
			       { mk_mii_end, } };



#if defined(M5272_PHY_STAT_INT) || !defined( CONFIG_M5272)
static void
#if defined(CONFIG_RPXCLASSIC)
mii_link_interrupt(void *dev_id)
#else
mii_link_interrupt(int irq, void * dev_id, struct pt_regs * regs)
#endif
{
	struct	net_device *dev = dev_id;
	struct fec_enet_private *fep = dev->priv;

#if defined(M5272_PHY_STAT_INT)
	{
		/* Acknowledge the interrupt */
		volatile unsigned long  *icrp;

		icrp = (volatile unsigned long *) (ICR_PHY_REG);
		*icrp = (*icrp & ICR_PHY_MASK_IPL) | ICR_MASK_IP;
	}
#endif

	if (fep->phy) {
		mii_do_cmd(dev, fep->phy->ack_int);
		/* restart and display status */
		mii_do_cmd(dev, phy_cmd_relink);
	}
}
#endif

/* Read remainder of PHY ID.
*/
static void
mii_discover_phy3(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep;
	int	i;

	fep = dev->priv;
	fep->phy_id |= (mii_reg & 0xffff);
	printk("fec: PHY @ 0x%x, ID 0x%08x", fep->phy_addr, fep->phy_id);

	for(i = 0; phy_info[i]; i++) {
		if(phy_info[i]->id == (fep->phy_id >> 4))
			break;
	}

	if (phy_info[i])
	{
		printk(" -- %s\n", phy_info[i]->name);
#if defined(M5272_PHY_STAT_INT)
		if (request_irq(PHY_INT, mii_link_interrupt, 0, 
						"fec(MII)", dev) != 0)
			printk("FEC: Could not allocate MII IRQ(%d)!\n", PHY_INT);
		{
		/* Unmask interrupts at ColdFire 5272 SIM */
			volatile unsigned long  *icrp;

			icrp = (volatile unsigned long *) (ICR_PHY_REG);
			*icrp = (*icrp & ICR_PHY_MASK) | ICR_PHY_SETUP;
		}
#endif
	}
	else {
	    if (fep->phy_addr++ < 32) {
		mii_queue(dev, mk_mii_read(MII_REG_PHYIR1),
				  mii_discover_phy);
		return;
		}
		else
		    printk(" -- unknown PHY!\n");
	}

	fep->phy = phy_info[i];
	fep->phy_id_done = 1;
}

/* Scan all of the MII PHY addresses looking for someone to respond
 * with a valid ID.  This usually happens quickly.
 */
static void
mii_discover_phy(uint mii_reg, struct net_device *dev)
{
	struct fec_enet_private *fep;
	volatile fec_t *fecp;
	uint phytype;

	fep = dev->priv;
	fecp = fec_hwp;

	if (fep->phy_addr < 32) {
#if defined(CONFIG_FEC_RTL8201BL)
		if ((phytype = (mii_reg & 0xffff)) != 0xffff) {
#else
		/* For all other cases we check for a valid phytype not 0x0000 or 0xffff */
		if ((phytype = (mii_reg & 0xffff)) != 0xffff && phytype != 0) {
#endif
			
			/* Got first part of ID, now get remainder.
			*/
			fep->phy_id = phytype << 16;
			mii_queue(dev, mk_mii_read(MII_REG_PHYIR2),
							mii_discover_phy3);
		}
		else {
			fep->phy_addr++;
			mii_queue(dev, mk_mii_read(MII_REG_PHYIR1),
							mii_discover_phy);
		}
	} else {
		printk("FEC: No PHY device found.\n");
		/* Disable external MII interface */
		fecp->fec_mii_speed = fep->phy_speed = 0;
	}
}

static int
fec_enet_open(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;

	/* I should reset the ring buffers here, but it's done in
	 * the close routine because init already set them up
	 */

	fep->sequence_done = 0;
	fep->link = 0;

	if (fep->phy) {
		mii_do_cmd(dev, fep->phy->ack_int);
		mii_do_cmd(dev, fep->phy->config);
		mii_do_cmd(dev, phy_cmd_config);  /* display configuration */

		while(!fep->sequence_done)
			schedule();

		mii_do_cmd(dev, fep->phy->startup);
#if defined( CONFIG_FEC_KS8995M_P5)
		mii_do_cmd(dev, phy_cmd_relink);
#endif
#if !defined( M5272_PHY_STAT_INT)
		fep->link = 1; /* Always assume we are connected (no interrupt
						  to turn on/off link) */
#endif
		fec_enet_setup_hw_p1( dev);
		netif_start_queue(dev);
		fec_enet_setup_hw_p2( dev);
	} else {
		fep->link = 1; /* lets just try it and see */
		/* no phy,  go full duplex,  it's most likely a hub chip */
		fec_restart(dev, 1);
		netif_start_queue(dev);
	}

	opened = 1;
	return 0;		/* Success */
}

static int
fec_enet_close(struct net_device *dev)
{
    int i;
	volatile cbd_t *bdp;
	volatile fec_t *fecp = fec_hwp;
	struct fec_enet_private *fep = dev->priv;

	/* open/close now works - but see notes below
	 */
  
	opened = 0;
	netif_stop_queue(dev);
	fec_stop(dev);

	/**********************************************
	 * Have to do all this after operations stopped
	 **********************************************/
	/* Set maximum receive buffer size.
	*/
	fecp->fec_r_buff_size = PKT_MAXBLR_SIZE;
#if (!defined(CONFIG_M5272) && !defined(CONFIG_M5282) && !defined(CONFIG_M5280))
	fecp->fec_r_hash = PKT_MAXBUF_SIZE;
#endif /* CONFIG_M5272 */

	/* Set receive and transmit descriptor base.
	*/
	fecp->fec_r_des_start = __pa((uint)(fep->rx_bd_base));
	fecp->fec_x_des_start = __pa((uint)(fep->tx_bd_base));

	fep->dirty_tx = fep->cur_tx = fep->tx_bd_base;
	fep->cur_rx = fep->rx_bd_base;

  /******************************************
   * Well, first let's free all the tx skb's 
   * so we don't have a memory leak.
   * -Bring ring back to init state (NOTE we
   * should do this in the open, but then I'd
   * have to shift code from the open AND we
   * are NOT running as a module ...MaTed
   ******************************************/
  bdp = fep->tx_bd_base;
  for (i = 0; i < TX_RING_SIZE; i++)
  {
	if (fep->tx_skbuff[i] != NULL)
	{
	  dev_kfree_skb_any(fep->tx_skbuff[i]);
	  fep->tx_skbuff[i] = NULL;
	}
  }
  /******************************************
   * -Bring ring back to init state (NOTE we
   * should do this in the open, but then I'd
   * have to shift code from the open AND we
   * are NOT running as a module ...MaTed
   ******************************************/
  bdp = fep->rx_bd_base;
  for (i = 0; i < RX_RING_SIZE; i++)
  {
	/* Initialize the BD for every fragment in the page.
	 */
	bdp->cbd_sc = BD_ENET_RX_EMPTY;
	bdp++;
  }
	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	/* ...and the same for transmmit.
	*/
	bdp = fep->tx_bd_base;
	for (i=0; i<TX_RING_SIZE; i++) {

		/* Initialize the BD for every fragment in the page.
		*/
		bdp->cbd_sc = 0;
		bdp->cbd_bufaddr = 0;
		bdp++;
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	return 0;
}

static struct net_device_stats *fec_enet_get_stats(struct net_device *dev)
{
	struct fec_enet_private *fep = (struct fec_enet_private *)dev->priv;

	return &fep->stats;
}

/* Set or clear the multicast filter for this adaptor.
 * Skeleton taken from sunlance driver.
 * The CPM Ethernet implementation allows Multicast as well as individual
 * MAC address filtering.  Some of the drivers check to make sure it is
 * a group multicast address, and discard those that are not.  I guess I
 * will do the same for now, but just remove the test if you want
 * individual filtering as well (do the upper net layers want or support
 * this kind of feature?).
 */

#define HASH_BITS	6		/* #bits in hash */
#define CRC32_POLY	0xEDB88320

static void set_multicast_list(struct net_device *dev)
{
	struct fec_enet_private *fep;
	volatile fec_t *ep;
	struct dev_mc_list *dmi;
	unsigned int i, j, bit, data, crc;
	unsigned char hash;

	fep = (struct fec_enet_private *)dev->priv;
	ep = fec_hwp;

	if (dev->flags&IFF_PROMISC) {
		/* Log any net taps. */
		printk("%s: Promiscuous mode enabled.\n", dev->name);
		ep->fec_r_cntrl |= 0x0008;
	} else {

		ep->fec_r_cntrl &= ~0x0008;

		if (dev->flags & IFF_ALLMULTI) {
			/* Catch all multicast addresses, so set the
			 * filter to all 1's.
			 */
			ep->fec_hash_table_high = 0xffffffff;
			ep->fec_hash_table_low = 0xffffffff;
		} else {
			/* Clear filter and add the addresses in hash register.
			*/
			ep->fec_hash_table_high = 0;
			ep->fec_hash_table_low = 0;
            
			dmi = dev->mc_list;

			for (j = 0; j < dev->mc_count; j++, dmi = dmi->next)
			{
				/* Only support group multicast for now.
				*/
				if (!(dmi->dmi_addr[0] & 1))
					continue;
			
				/* calculate crc32 value of mac address
				*/
				crc = 0xffffffff;

				for (i = 0; i < dmi->dmi_addrlen; i++)
				{
					data = dmi->dmi_addr[i];
					for (bit = 0; bit < 8; bit++, data >>= 1)
					{
						crc = (crc >> 1) ^
						(((crc ^ data) & 1) ? CRC32_POLY : 0);
					}
				}

				/* only upper 6 bits (HASH_BITS) are used
				   which point to specific bit in he hash registers
				*/
				hash = (crc >> (32 - HASH_BITS)) & 0x3f;
			
				if (hash > 31)
					ep->fec_hash_table_high |= 1 << (hash - 32);
				else
					ep->fec_hash_table_low |= 1 << hash;
			}
		}
	}
}

/* Set a MAC change in hardware.
 */
static int
fec_set_mac_address(struct net_device *dev, void *p)
{
	int i;
	struct sockaddr *addr=p;
	volatile fec_t *fecp;

	fecp = fec_hwp;

	if (netif_running(dev))
		return -EBUSY;

	/* Set the device copy of the Ethernet address
	*/
	memcpy(dev->dev_addr, addr->sa_data,dev->addr_len);
      
	/* Set our copy of the Ethernet address 
	*/
	for (i = 0; i < (ETH_ALEN / 2); i++)
	{
		my_enet_addr[i] = (dev->dev_addr[i*2] << 8) | dev->dev_addr[i*2 + 1];
	}

	/* Set station address.
	*/
	fecp->fec_addr_low = (my_enet_addr[0] << 16) | my_enet_addr[1];
	fecp->fec_addr_high = my_enet_addr[2] << 16;
  
	return 0;
}

/* Initialize the FEC Ethernet on 860T.
 */
int __init fec_enet_init(struct net_device *dev)
{
	struct fec_enet_private *fep;
	int		i, j;
	unsigned long	mem_addr;
	volatile cbd_t	*bdp;
	cbd_t		*cbd_base;
	volatile fec_t	*fecp;
	unsigned char	*eap, *iap;
	static int      probes = 0;

#if defined(CONFIG_RPXCLASSIC)
	unsigned char	tmpaddr[6];
#endif
#if (defined(CONFIG_M5272) || defined(CONFIG_M5282) || defined(CONFIG_M5280))
#if !defined(CONFIG_NETtel) && !defined(CONFIG_GILBARCONAP) || !defined(CONFIG_HW_FEITH)
	unsigned char	tmpaddr[6];
#endif /* !CONFIG_NETtel && !CONFIG_GILBARCONAP */
#else
	pte_t		*pte;
	extern		uint	_get_IMMR(void);
	bd_t		*bd;
	volatile	immap_t	*immap;

	immap = (immap_t *)IMAP_ADDR;	/* pointer to internal registers */
	bd = (bd_t *)__res;
#endif /* CONFIG_M5272 */

	/* Only allow us to be probed once. */
	if (dev->base_addr == 0xffe0)
		return(-ENXIO);

	printk ("fec.c: Probe number %d with 0x%04lx\n",
		probes, dev->base_addr);

	if (probes)
	  return(-ENXIO);
	else
	  probes++;


	/* Allocate some private information.
	*/
	fep = (struct fec_enet_private *)kmalloc(sizeof(*fep), GFP_KERNEL);
	__clear_user(fep,sizeof(*fep));

	/* Create an Ethernet device instance.
	*/
	fecp = fec_hwp;

	/* Whack a reset.  We should wait for this.
	*/
	fecp->fec_ecntrl = FEC_ENET_RESET;
	udelay(10);

	/* Clear and enable interrupts */
	fecp->fec_ievent = 0xffc0;
	fecp->fec_imask = (FEC_ENET_TXF | FEC_ENET_TXB |
		FEC_ENET_RXF | FEC_ENET_RXB | FEC_ENET_MII);
	fecp->fec_hash_table_high = 0;
	fecp->fec_hash_table_low = 0;
	fecp->fec_r_buff_size = PKT_MAXBLR_SIZE;
        //fecp->fec_ecntrl = 2; // this is too early PSW
        fecp->fec_r_des_active = 0x01000000;

	/*
	 * Set the Ethernet address. If using multiple Enets on the 8xx,
	 * this needs some work to get unique addresses.
	 */
	eap = (unsigned char *)my_enet_addr;
#if (defined(CONFIG_M5272) || defined(CONFIG_M5282) || defined(CONFIG_M5280))
#if defined(CONFIG_NETtel) || defined(CONFIG_GILBARCONAP) || defined(CONFIG_SE1100) || defined(CONFIG_HW_FEITH)
	/*
	 * If the MAC is all 1's or 0's, used the default coded one.
	 */
#if defined(CONFIG_GILBARCONAP) || defined(CONFIG_SE1100) || defined(CONFIG_SCALES)
	iap = (unsigned char *) 0xf0006000;
#elif defined(CONFIG_CANCam)
	iap = (unsigned char *) 0xf0020000;
#else
	iap = (unsigned char *) 0xf0006006;
#endif
	if (!iap[0] && !iap[1] && !iap[2] && !iap[3] && !iap[4] && !iap[5])
		iap = eap;
	if (iap[0] == 0xff && iap[1] == 0xff && iap[2] == 0xff &&
	    iap[3] == 0xff && iap[4] == 0xff && iap[5] == 0xff)
		iap = eap;
#elif defined (CONFIG_MTD_KeyTechnology)
	iap = (unsigned char *)0xffe04000;
	if (!iap[0] && !iap[1] && !iap[2] && !iap[3] && !iap[4] && !iap[5])
		iap = eap;
	if (iap[0] == 0xff && iap[1] == 0xff && iap[2] == 0xff &&
	    iap[3] == 0xff && iap[4] == 0xff && iap[5] == 0xff)
		iap = eap;
#elif defined(CONFIG_BOARD_UC5272)
	memcpy(&tmpaddr, &fec_hwaddr, 6);
	iap = &tmpaddr[0];
#else
	/*
	 *	We will assume something else has written a valid MAC
	 *	address into the FEC registers. Only problem is if no
	 *	one did we might have an invalid address, so we force
	 *	the first broadcast and multicast bits to 0.
	 */
	*((unsigned long *) &tmpaddr[0]) = fecp->fec_addr_low;
	*((unsigned short *) &tmpaddr[4]) = (fecp->fec_addr_high >> 16);
	tmpaddr[0] &= 0xf8;
	iap = tmpaddr;
#endif /* CONFIG_NETtel || CONFIG_GILBARCONAP*/
#else /* not defined(CONFIG_M5272) */
	iap = bd->bi_enetaddr;
#endif /* CONFIG_M5272 || CONFIG_M5282 || CONFIG_M5280 */

#if defined(CONFIG_RPXCLASSIC)
	/* The Embedded Planet boards have only one MAC address in
	 * the EEPROM, but can have two Ethernet ports.  For the
	 * FEC port, we create another address by setting one of
	 * the address bits above something that would have (up to
	 * now) been allocated.
	 */
	for (i=0; i<6; i++)
		tmpaddr[i] = *iap++;
	tmpaddr[3] |= 0x80;
	iap = tmpaddr;
#endif

	for (i=0; i<6; i++)
		dev->dev_addr[i] = *eap++ = *iap++;

	/* Allocate memory for buffer descriptors.
	*/
	if (((RX_RING_SIZE + TX_RING_SIZE) * sizeof(cbd_t)) > PAGE_SIZE) {
		printk("FEC init error.  Need more space.\n");
		printk("FEC initialization failed.\n");
		return 1;
	}
	mem_addr = __get_free_page(GFP_KERNEL);
	cbd_base = (cbd_t *)mem_addr;

#if !defined(CONFIG_UCLINUX)
	/* Make it uncached.
	*/
	pte = va_to_pte(mem_addr);
	pte_val(*pte) |= _PAGE_NO_CACHE;
	flush_tlb_page(init_mm.mmap, mem_addr);
#endif /* CONFIG_UCLINUX */

	/* Set receive and transmit descriptor base.
	*/
	fep->rx_bd_base = cbd_base;
	fep->tx_bd_base = cbd_base + RX_RING_SIZE;

	fep->dirty_tx = fep->cur_tx = fep->tx_bd_base;
	fep->cur_rx = fep->rx_bd_base;

	fep->skb_cur = fep->skb_dirty = 0;

	/* Initialize the receive buffer descriptors.
	*/
	bdp = fep->rx_bd_base;
	for (i=0; i<FEC_ENET_RX_PAGES; i++) {

		/* Allocate a page.
		*/
		mem_addr = __get_free_page(GFP_KERNEL);

#if !defined(CONFIG_UCLINUX)
		/* Make it uncached.
		*/
		pte = va_to_pte(mem_addr);
		pte_val(*pte) |= _PAGE_NO_CACHE;
		flush_tlb_page(init_mm.mmap, mem_addr);
#endif /* CONFIG_UCLINUX */

		/* Initialize the BD for every fragment in the page.
		*/
		for (j=0; j<FEC_ENET_RX_FRPPG; j++) {
			bdp->cbd_sc = BD_ENET_RX_EMPTY;
			bdp->cbd_bufaddr = __pa(mem_addr);
			mem_addr += FEC_ENET_RX_FRSIZE;
			bdp++;
		}
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	/* ...and the same for transmmit.
	*/
	bdp = fep->tx_bd_base;
	for (i=0; i<TX_RING_SIZE; i++) {

		/* Initialize the BD for every fragment in the page.
		*/
		bdp->cbd_sc = 0;
		bdp->cbd_bufaddr = 0;
		bdp++;
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	/* Set receive and transmit descriptor base.
	*/
	fecp->fec_r_des_start = __pa((uint)(fep->rx_bd_base));
	fecp->fec_x_des_start = __pa((uint)(fep->tx_bd_base));

	/* Install our interrupt handler.
	*/
#if defined(CONFIG_M5272)
	/* Setup interrupt handlers. */
	if (request_irq(86, fec_enet_interrupt, 0, "fec(RX)", dev) != 0)
		printk("FEC: Could not allocate FEC(RX) IRQ(86)!\n");
	if (request_irq(87, fec_enet_interrupt, 0, "fec(TX)", dev) != 0)
		printk("FEC: Could not allocate FEC(TX) IRQ(87)!\n");
	if (request_irq(88, fec_enet_interrupt, 0, "fec(OTHER)", dev) != 0)
		printk("FEC: Could not allocate FEC(OTHER) IRQ(88)!\n");

	/* Unmask interrupts at ColdFire 5272 SIM */
	{
		volatile unsigned long  *icrp;

		icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR3);
		*icrp = 0x00000ddd;
	}
#elif (defined(CONFIG_M5282) || defined(CONFIG_M5280))
	/* Setup interrupt handlers. */
	if (request_irq(64+23, fec_enet_interrupt, 0, "fec(TXF)", dev) != 0)
		printk("FEC: Could not allocate FEC(TXF) IRQ(64+23)!\n");
	if (request_irq(64+24, fec_enet_interrupt, 0, "fec(TXB)", dev) != 0)
		printk("FEC: Could not allocate FEC(TXB) IRQ(64+24)!\n");
	if (request_irq(64+25, fec_enet_interrupt, 0, "fec(TXFIFO)", dev) != 0)
		printk("FEC: Could not allocate FEC(TXFIFO) IRQ(64+25)!\n");
	if (request_irq(64+26, fec_enet_interrupt, 0, "fec(TXCR)", dev) != 0)
		printk("FEC: Could not allocate FEC(TXCR) IRQ(64+26)!\n");

	if (request_irq(64+27, fec_enet_interrupt, 0, "fec(RXF)", dev) != 0)
		printk("FEC: Could not allocate FEC(RXF) IRQ(64+27)!\n");
	if (request_irq(64+28, fec_enet_interrupt, 0, "fec(RXB)", dev) != 0)
		printk("FEC: Could not allocate FEC(RXB) IRQ(64+28)!\n");

	if (request_irq(64+29, fec_enet_interrupt, 0, "fec(MII)", dev) != 0)
		printk("FEC: Could not allocate FEC(MII) IRQ(64+29)!\n");
	if (request_irq(64+30, fec_enet_interrupt, 0, "fec(LC)", dev) != 0)
		printk("FEC: Could not allocate FEC(LC) IRQ(64+30)!\n");
	if (request_irq(64+31, fec_enet_interrupt, 0, "fec(HBERR)", dev) != 0)
		printk("FEC: Could not allocate FEC(HBERR) IRQ(64+31)!\n");
	if (request_irq(64+32, fec_enet_interrupt, 0, "fec(GRA)", dev) != 0)
		printk("FEC: Could not allocate FEC(GRA) IRQ(64+32)!\n");
	if (request_irq(64+33, fec_enet_interrupt, 0, "fec(EBERR)", dev) != 0)
		printk("FEC: Could not allocate FEC(EBERR) IRQ(64+33)!\n");
	if (request_irq(64+34, fec_enet_interrupt, 0, "fec(BABT)", dev) != 0)
		printk("FEC: Could not allocate FEC(BABT) IRQ(64+34)!\n");
	if (request_irq(64+35, fec_enet_interrupt, 0, "fec(BABR)", dev) != 0)
		printk("FEC: Could not allocate FEC(BABR) IRQ(64+35)!\n");

	/* Unmask interrupts at ColdFire 5282 interrupt controller */
	{
		volatile unsigned char  *icrp;
		volatile unsigned long  *imrp;

		icrp = (volatile unsigned char *) (MCF_IPSBAR + MCFICM_INTC0 +
			MCFINTC_ICR0);
		for (i = 23; (i < 36); i++)
			icrp[i] = 0x23;

		imrp = (volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 +
			MCFINTC_IMRH);
		*imrp &= ~0x0000000f;
		imrp = (volatile unsigned long *) (MCF_IPSBAR + MCFICM_INTC0 +
			MCFINTC_IMRL);
		*imrp &= ~0xff800001;
	}

	/* Set up gpio outputs for MII lines */
	{
	  volatile unsigned short *gpio_paspar;
	  gpio_paspar = (volatile unsigned short *) (MCF_IPSBAR +
												 0x100056);
	  *gpio_paspar = 0x0f00;
	}

#else
	if (request_8xxirq(FEC_INTERRUPT, fec_enet_interrupt, 0, "fec", dev) != 0)
		panic("Could not allocate FEC IRQ!");
#endif

#if defined(CONFIG_RPXCLASSIC)
	/* Make Port C, bit 15 an input that causes interrupts.
	*/
	immap->im_ioport.iop_pcpar &= ~0x0001;
	immap->im_ioport.iop_pcdir &= ~0x0001;
	immap->im_ioport.iop_pcso &= ~0x0001;
	immap->im_ioport.iop_pcint |= 0x0001;
	cpm_install_handler(CPMVEC_PIO_PC15, mii_link_interrupt, dev);

	/* Make LEDS reflect Link status.
	*/
	*((uint *) RPX_CSR_ADDR) &= ~BCSR2_FETHLEDMODE;
#endif
#if defined(CONFIG_FADS)
	if (request_8xxirq(SIU_IRQ2, mii_link_interrupt, 0, "mii", dev) != 0)
		panic("Could not allocate MII IRQ!");
#endif

	dev->base_addr = (unsigned long)fecp;
	dev->priv = fep;
        /* enable the device at this location to stop boot hangs PSW */
        fecp->fec_ecntrl = 2; 

	ether_setup(dev);

	/* The FEC Ethernet specific entries in the device structure. */
	dev->open = fec_enet_open;
	dev->hard_start_xmit = fec_enet_start_xmit;
	dev->tx_timeout = fec_timeout;
	dev->watchdog_timeo = TX_TIMEOUT;
	dev->stop = fec_enet_close;
	dev->get_stats = fec_enet_get_stats;
	dev->set_multicast_list = set_multicast_list;
	dev->set_mac_address = fec_set_mac_address;
#if defined(CONFIG_FEC_IOCTL)
	dev->do_ioctl = &fec_ioctl;
#endif
	for (i=0; i<NMII-1; i++)
		mii_cmds[i].mii_next = &mii_cmds[i+1];
	mii_free = mii_cmds;

#if defined(CONFIG_M5272) || defined(CONFIG_M5282) || defined(CONFIG_M5280)
	fecp->fec_r_cntrl = 0x04;
	fecp->fec_x_cntrl = 0x00;

	/* Set MII speed to 2.5 MHz
	*/
#if defined (CONFIG_M5272)
	fep->phy_speed = (((MCF_CLK / 4) / (2500000 / 10)) + 5) / 10;
	fep->phy_speed *= 2; /* see 5272 manual sec 11.5.8: MSCR */
#elif (defined (CONFIG_M5282) || defined(CONFIG_M5280))
	fep->phy_speed = (((MCF_CLK / 2) / (2500000 / 10)) + 5) / 10;
	fep->phy_speed *= 2; /* see 5282 manual sec 17.5.4.7: MSCR */
#endif	
	fecp->fec_mii_speed = fep->phy_speed;

	fec_restart(dev, 0);
#else
	/* Configure all of port D for MII.
	*/
	immap->im_ioport.iop_pdpar = 0x1fff;

	/* Bits moved from Rev. D onward.
	*/
	if ((_get_IMMR() & 0xffff) < 0x0501)
		immap->im_ioport.iop_pddir = 0x1c58;	/* Pre rev. D */
	else
		immap->im_ioport.iop_pddir = 0x1fff;	/* Rev. D and later */
	
	/* Set MII speed to 2.5 MHz
	*/
	fecp->fec_mii_speed = fep->phy_speed = 
		((bd->bi_busfreq * 1000000) / 2500000) & 0x7e;
#endif /* CONFIG_M5272 || CONFIG_M5282 || defined(CONFIG_M5280) */
	printk("%s: FEC ENET Version 0.2, ", dev->name);
	for (i=0; i<5; i++)
		printk("%02x:", dev->dev_addr[i]);
	printk("%02x\n", dev->dev_addr[5]);

	/* Queue up command to detect the PHY and initialize the
	 * remainder of the interface.
	 */
	fep->phy_id_done = 0;
#if defined(CONFIG_M5272)
	fep->phy_addr = PHY_START_ADDR;
#endif /* CONFIG_M5272 */
	mii_queue(dev, mk_mii_read(MII_REG_PHYIR1), mii_discover_phy);

	return 0;
}

/* This function is called to start or restart the FEC during a link
 * change.  This only happens when switching between half and full
 * duplex.
 */
static void
fec_restart(struct net_device *dev, int duplex)
{
	struct fec_enet_private *fep;
	volatile cbd_t *bdp;
	volatile fec_t *fecp;
	int i;

	fecp = fec_hwp;

	fep = dev->priv;

	/* Whack a reset.  We should wait for this.
	*/
	fecp->fec_ecntrl = 1;
	udelay(10);

	fec_enet_setup_hw_p1( dev);

	/* Set receive and transmit descriptor base.
	*/
	fecp->fec_r_des_start = __pa((uint)(fep->rx_bd_base));
	fecp->fec_x_des_start = __pa((uint)(fep->tx_bd_base));

	fep->dirty_tx = fep->cur_tx = fep->tx_bd_base;
	fep->cur_rx = fep->rx_bd_base;

	/* Reset SKB transmit buffers.
	*/
	fep->skb_cur = fep->skb_dirty = 0;
	for (i=0; i<=TX_RING_MOD_MASK; i++) {
		if (fep->tx_skbuff[i] != NULL) {
			dev_kfree_skb_any(fep->tx_skbuff[i]);
			fep->tx_skbuff[i] = NULL;
		}
	}

	/* Initialize the receive buffer descriptors.
	*/
	bdp = fep->rx_bd_base;
	for (i=0; i<RX_RING_SIZE; i++) {

		/* Initialize the BD for every fragment in the page.
		*/
		bdp->cbd_sc = BD_ENET_RX_EMPTY;
		bdp++;
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	/* ...and the same for transmmit.
	*/
	bdp = fep->tx_bd_base;
	for (i=0; i<TX_RING_SIZE; i++) {

		/* Initialize the BD for every fragment in the page.
		*/
		bdp->cbd_sc = 0;
		bdp->cbd_bufaddr = 0;
		bdp++;
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	fep->full_duplex = duplex;
	fec_enet_setup_hw_p2( dev );
}

static void
fec_stop(struct net_device *dev)
{
	volatile fec_t *fecp;
	struct fec_enet_private *fep;

	fecp = fec_hwp;
	fep = dev->priv;

	fecp->fec_x_cntrl = FEC_ENET_GTS;	/* Graceful transmit stop */

	while(!(fecp->fec_ievent & FEC_ENET_GRA));

	/* Whack a reset.  We should wait for this.
	*/
	fecp->fec_ecntrl = FEC_ENET_RESET;
	udelay(10);

	/* Clear outstanding MII command interrupts.
	*/
	fecp->fec_ievent = FEC_ENET_MII;

#if (!defined(CONFIG_M5272) && !defined(CONFIG_M5282) && !defined(CONFIG_M5280))
	/* Enable MII command finished interrupt 
	*/
	fecp->fec_ivec = (FEC_INTERRUPT/2) << 29;
#endif /* !CONFIG_M5272 && !CONFIG_M5282 && !CONFIG_M5280 */

	fecp->fec_imask = FEC_ENET_MII;
	fecp->fec_mii_speed = fep->phy_speed;
}

static void	fec_enet_setup_hw_p1( struct net_device *dev )
{
	unsigned char *eap;
	volatile fec_t *fecp = fec_hwp;
	int i;

	/* Enable interrupts we wish to service.
	*/
	fecp->fec_imask = (FEC_ENET_TXF | FEC_ENET_TXB |
			   FEC_ENET_RXF | FEC_ENET_RXB | FEC_ENET_MII);

	/* Clear any outstanding interrupt.
	*/
	fecp->fec_ievent = FEC_ENET_ALLINT;

#if (!defined(CONFIG_M5272) && !defined(CONFIG_M5282) && !defined(CONFIG_M5280))
	fecp->fec_ivec = (FEC_INTERRUPT/2) << 29;
#endif /* !CONFIG_M5272 && !CONFIG_M5282 && !CONFIG_M5280 */

	/* Set station address.
	*/
	fecp->fec_addr_low = (my_enet_addr[0] << 16) | my_enet_addr[1];
	fecp->fec_addr_high = (my_enet_addr[2] << 16);

	eap = (unsigned char *)&my_enet_addr[0];
	for (i=0; i<6; i++)
		dev->dev_addr[i] = *eap++;

	/* Reset all multicast.
	*/
	fecp->fec_hash_table_high = 0;
	fecp->fec_hash_table_low = 0;

	/* Set maximum receive buffer size.
	*/
	fecp->fec_r_buff_size = PKT_MAXBLR_SIZE;
#if (!defined(CONFIG_M5272) && !defined(CONFIG_M5282) && !defined(CONFIG_M5280))
	fecp->fec_r_hash = PKT_MAXBUF_SIZE;
#endif /* !CONFIG_M5272 && !CONFIG_M5282 && !CONFIG_M5280 */

    return;
}

static void 
fec_enet_setup_hw_p2( struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile fec_t *fecp = fec_hwp;

	/* Enable MII mode.
	*/
	if (fep->full_duplex) {
		fecp->fec_r_cntrl = 0x04;	/* MII enable */
		fecp->fec_x_cntrl = 0x04;	/* FD enable */
	}
	else {
		fecp->fec_r_cntrl = 0x06;	/* MII enable|No Rcv on Xmit */
		fecp->fec_x_cntrl = 0x00;
	}

#if (!defined(CONFIG_M5272) && !defined(CONFIG_M5282) && !defined(CONFIG_M5280))
	/* Enable big endian and don't care about SDMA FC.
	*/
	fecp->fec_fun_code = 0x78000000;
#endif /* !CONFIG_M5272 && !CONFIG_M5282 && !CONFIG_M5280 */

	/* Set MII speed.
	*/
	fecp->fec_mii_speed = fep->phy_speed;

	/* And last, enable the transmit and receive processing.
	*/
	fecp->fec_ecntrl = 2;
	fecp->fec_r_des_active = 0x01000000;
    return;
}

#if defined(CONFIG_FEC_IOCTL)
#define FEC_IOCTL_LXT972_STAT2	17		// lxt972 status 2 register

/*******************************************************************
 *  fec_ioctl_save_reg
 * Description:
 *	This routine saves the register read for fec_ioctl for subsequent
 * retrieval by another user ioctl call (see below)
 *
 * Parameters:
 *	uint mii_reg
 *		- # of the register requested
 *	struct net_device *dev
 *		- dev entry
 *
 *	Returns:
 *		0 -> SUCCESS
 *	   -1 -> already an outstanding queued get request
 *******************************************************************/
static void fec_ioctl_save_reg( uint mii_reg, struct net_device *dev)
{
  struct fec_enet_private 	*fep 	= (struct fec_enet_private *)dev->priv;
  fep->fec_ioctl_regval = mii_reg;	// save the value
  fep->fec_ioctl_wait = 1;			// indicate value is saved
  return;
}
/********************************************************************
 *	FEC_IOCTL
 * Description:
 *	This routine implements Donald Becker's MII IOCTL interface. It 
 * has a couple of minor extensions. At the present time only the
 * extension has been implemented and tested. Portions of the other
 * calls have implemented and not tested.  
 *
 * Parameters
 *	- dev		- pointer to the device table
 *	- rq		- pointer to the interface Request Structure which contains
 *				a pointer to the ioctl data transfer area (see fec_ioctl.h)
 */
static int fec_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
  struct fec_enet_private 	*fep 	= (struct fec_enet_private *) dev->priv;

  struct fec_ioctl_data 	*data	= (struct fec_ioctl_data *) &rq->ifr_data;

  int						 retval	= 0;	// default is good return
  int						 link_stat;	    // link status		

  if (FEC_DEBUG > 5)
	printk(" FEC_IOCTL**: entered: %s (dev-%p, rq-%p, cmd-%d) data-%p\n ",
		   dev->name, dev, rq, cmd, data);

  switch(cmd) 
  {
  case SIOCGMIIPHY:               /* Get address of MII PHY in use. */
  case SIOCDEVPRIVATE:            /* for binary compat, remove in 2.5 */

	if (FEC_DEBUG > 5)
	  printk(" FEC_IOCTL**: SIOCGMIIPHY: %s returning data->phy_id %d\n ",
			 dev->name, fep->phy_addr);

	data->phy_id = fep->phy_addr;
	break;
	/* NO fall through - just do what was requested */
  case SIOCGMIIREG:               /* Read MII PHY register. */
  case SIOCDEVPRIVATE+1:          /* for binary compat, remove in 2.5 */
	data->phy_id = fep->phy_addr;	// Setup - just because

	if (FEC_DEBUG > 5)
	  printk(" FEC_IOCTL**: SIOCGMIIREG: %s data->in %d, fec_ioctl_wait-%d\n ",
			 dev->name, data->value_in, fep->fec_ioctl_wait);

	if (data->value_in == SIOC_MIIREG_GET)
	{
	  // check if already have an outstanding request (queued or waiting 
	  //  to be read
	  if (fep->fec_ioctl_wait !=0)
	  {
		retval = -EALREADY;
		data->link_speed = data->duplex = SIOC_FEC_RC_NOT_COMP;
		break;
	  }
	  // queue the command to be issued
	  fep->fec_ioctl_reg = data->reg;
	  fep->fec_ioctl_wait = 2;	// waiting for queued command to complete
	  
	  mii_queue( dev, mk_mii_read( data->reg), fec_ioctl_save_reg);
	  break;
	}

	if (data->value_in == SIOC_MIIREG_QUERY)
	{
	  // check if we are still waiting for queued command to complete
	  if (fep->fec_ioctl_wait == 2)
	  {
		retval = -EAGAIN;		// try again (just the query
		data->link_speed = data->duplex = SIOC_FEC_RC_NOT_COMP;
		break;
	  }
	  // check if data available
	  if (fep->fec_ioctl_wait == 0)
	  {
		// not request outstanding and no data available
		retval = -ENODATA;	// mismatch on the query
		data->link_speed = data->duplex = SIOC_FEC_RC_NO_REQUEST;
		break;
	  }
	  // We have valid data
	  fep->fec_ioctl_wait = 0;	// mark as read
	  data->value_out = fep->fec_ioctl_regval;	 // set read register

	  if (FEC_DEBUG > 5)
      {
		printk(" FEC_IOCTL**: VALID DATA: %s internal (reg[%d] = 0x%x)\n ",
			   dev->name, data->reg, data->value_out);
		printk("         L**: VALID DATA: %s mii-tool (reg[%d] = 0x%x)\n ",
			   dev->name, fep->fec_ioctl_reg, fep->fec_ioctl_regval);
	  }

	  // check if the register value passed as one we read
	  if (fep->fec_ioctl_reg != data->reg)
	  {
		retval = -EINVAL;	// mismatch on the query invalid register passed
		data->link_speed = data->duplex = SIOC_FEC_RC_WRONG_REG;
		break;
	  }

	  // check if for the STAT2 register
	  if (fep->fec_ioctl_reg == FEC_IOCTL_LXT972_STAT2)
	  {
		link_stat = (fep->fec_ioctl_regval &
					 PHY_STATUS_REG_TWO_LINK) ? 1 : 0 ;
		if (FEC_DEBUG > 4)
		{
		  printk("         **: STAT2 READ: %s link_stat/fep=%d/%d reg=0x%x\n ",
				 dev->name, link_stat, fep->link, fep->fec_ioctl_regval);
		}
		if (link_stat)
		{
		  // need to translate (only available values)
		  data->link_speed = (fep->fec_ioctl_regval & 
							  PHY_STATUS_REG_TWO_MODE_100) ? 2 : 1 ;
		  data->duplex = (fep->fec_ioctl_regval &
						  PHY_STATUS_REG_TWO_DUPLEX_MODE) ? 1 : 0;
		}
		else
		{	// link is down
		  data->link_speed = data->duplex = SIOC_FEC_RC_NO_LINK;
		  // no error return - is valid
		  // retval = -EAGAIN;
		}
		if (FEC_DEBUG > 4)
		{
		  printk("          **: STAT2 DATA: %s link=%d duplex=%d\n ",
				 dev->name, data->link_speed, data->duplex);
		}
	  }
	  else
	  {
		// returning non STAT2 register value
		// just return the values
		// register already set, ret code is OK
		// we don't modify passed the end of the 4 half words (original 
		//  interface
		//data->link_speed = data->duplex = SIOC_FEC_RC_WRONG_REG;
	  }
	}	
	else
	{
	  data->link_speed = data->duplex = SIOC_FEC_RC_WRONG_REG;
	  retval = -ENOSYS;
	}
	break;
  case SIOCSMIIREG:               /* Write MII PHY register. */
  case SIOCDEVPRIVATE+2:          /* for binary compat, remove in 2.5 */
	// queue the command to be issued - no function follow up
	mii_queue( dev, mk_mii_write( data->reg, data->value_in), NULL);
	break;
  case SIOCETHTOOL:
	// Not implemented yet :-( ...MaTed---
	//	return fec_ethtool_ioctl( dev, (void *) rq->ifr_data);
  default:
	retval = -ENOSYS;
	break;
  }

  return retval;
}
#endif // CONFIG_FEC_IOCTL
