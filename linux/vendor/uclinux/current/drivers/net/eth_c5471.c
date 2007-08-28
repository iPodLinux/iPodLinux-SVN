/***********************************************************************
 * linux/drivers/net/eth_c5471.c
 *
 * Based on linux/drivers/net/isa-skeleton.c from the linux-2.4.17
 * and on the linux/drivers/net/skeleton.c from the linux 2.0.38
 * distribution:
 *
 *   Copyright (C) 2003 Cadenux, LLC. All rights reserved.
 *   todd.fischer@cadenux.com  <www.cadenux.com>
 *
 *   Copyright (C) 2001 RidgeRun, Inc. All rights reserved.
 *
 *   A network driver outline for linux written 1993-94 by Donald Becker.
 *
 *   Copyright 1993 United States Government as represented by the
 *   Director, National Security Agency.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***********************************************************************/

/***********************************************************************
 * Included Files
 ***********************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/crc32.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/arch/sysdep.h> /* LINUX_20 */

#ifdef LINUX_20
#undef dev_kfree_skb
#include <linux/malloc.h>    /* Deprecated */
#else
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#endif

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include "eth_c5471.h"

/***********************************************************************
 * Definitions
 ***********************************************************************/

#ifndef C5471_LAN_RATE /* Should be provided by config.in script */
# define C5471_LAN_RATE 0
#endif

/***********************************************************************
 * Private Type Declarations
 ***********************************************************************/

/***********************************************************************
 * Public Variables
 ***********************************************************************/

/* This is a parameter that can be overriden at the time this module
 * is loaded. It is used to control whether the board is set for
 * 10BaseT (10) 100BaseT (100), or autonegotiation (0) operation.
 * e.g. insmod LAN_Rate=10
 */

int LAN_Rate = C5471_LAN_RATE;

/***********************************************************************
 * Private Variables
 ***********************************************************************/

static const char cardname[] = "ethernet";

/* Information that needs to be kept for each each port. */

struct net_local
{
  struct NET_DEVICE_STATS stats;
  long open_time;                 /* Useless example local info. */

#ifndef LINUX_20
  /* Tx control lock.  This protects the transmit buffer ring
   * state along with the "tx full" state of the driver.  This
   * means all netif_queue flow control actions are protected
   * by this lock as well.
   */

  spinlock_t lock;
#endif /* LINUX_20 */
};

#ifndef LINUX_20
static int (*eth_set_mac_address)(struct NET_DEVICE *dev, void *addr) = NULL;
#endif /* LINUX_20 */

/***********************************************************************
 * Private Function Prototypes
 ***********************************************************************/

#ifdef MODULE
static int  c5471_net_probe(struct NET_DEVICE *dev);
#else /* MODULE */
extern int  c5471_net_probe(struct NET_DEVICE *dev);
#endif /* MODULE */
static int  net_open(struct NET_DEVICE *dev);
static int  net_send_packet(struct sk_buff *skb, struct NET_DEVICE *dev);
static void net_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void net_rx(struct NET_DEVICE *dev);
#if TX_EVENT
static void net_tx(struct NET_DEVICE *dev);
#endif /* TX_EVENT */
static int  net_close(struct NET_DEVICE *dev);
static struct NET_DEVICE_STATS *net_get_stats(struct NET_DEVICE *dev);
static void set_multicast_list(struct NET_DEVICE *dev);
#ifndef LINUX_20
static void net_tx_timeout(struct NET_DEVICE *dev);
#endif /* LINUX_20 */

static void net_get_mac_address(struct NET_DEVICE *dev);
static int  net_set_mac_address(struct NET_DEVICE *dev, void *addr);
static int  net_reset_mac_address(struct NET_DEVICE *dev);

/***********************************************************************
 * c5471_net_probe
 *
 * This function is called during the kernel initialization
 * for cases when this driver is linked with the kernel or
 * from within this driver module when loaded by insmod.
 *
 * Check for a network adaptor of this type, and return '0' iff one
 * exists.
 *
 * There is only one valid value for the base_address.  Normally,
 * the following apply:
 *
 * dev->base_addr == 0, probe all likely locations.
 * dev->base_addr == 1, always return failure.
 * dev->base_addr == 2, allocate space for the device and return
 *                      success (detachable devices only).
 *
 * However, we ignore these and slam-dunk the known C5471 base_addr.
 *
 * Similarly, we use the known C5471 IRQ.  We ignore the following
 * possbilities:
 *
 * dev->irq == -1, a user-level program will set the IRQ.
 * dev->irq <   2, do auto IRQ setupt
 * dev->irq >=  2, use this IRQ.
 *
 ***********************************************************************/

#ifdef MODULE
static
#endif /* MODULE */
int
#ifndef LINUX_20
__init
#endif
c5471_net_probe(struct NET_DEVICE *dev)
{
  SET_MODULE_OWNER(dev);

  dbg("%s/%s found at 0x%x, ", dev->name, cardname, ETHER_BASE);

  /* Allocate a new 'dev' if needed. */

#ifndef MODULE
#ifdef LINUX_20
  if (dev == NULL)
    {
      dev = init_etherdev(0, 0);
      if (dev == NULL)
	return -ENOMEM;
    }
#endif /* LINUX_20 */

  /* Fill in the 'dev' fields that will not be provided if we
   * are not a module.
   */

  dev->base_addr = ETHER_BASE;
  dev->irq       = ETHER_IRQ;
  dev->dma       = 0; /* no dma  */
#endif /* MODULE */

  /* Since C5471 has hard "jumpered" interrupts, allocate the interrupt
   * vector now. There is no point in waiting since no other device
   * can use the interrupt, and this marks the irq as busy.
   */

#if 0 /* Nah... We'll do this in net_open() */
  {
    int irqval;
    irqval = request_irq(dev->irq, net_interrupt, 0, cardname, dev);
    if (irqval)
      {
	warn("%s: unable to get IRQ %d (irqval=%d)",
	     dev->name, dev->irq, irqval);
	return -EAGAIN;
      }
  }
#endif /* Nah */

  /* Allocate private device data */

  if (dev->priv == NULL)
    {
      dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
      if (dev->priv == NULL)
	return -ENOMEM;
    }

  memset(dev->priv, 0, sizeof(struct net_local));

#ifndef LINUX_20
  {
    struct net_local *np;
    np = (struct net_local *)dev->priv;
    spin_lock_init(&np->lock);
  }
#endif

  /* Grab the region so that no one else tries to probe our ioports. */

  request_region(dev->base_addr, NETCARD_IO_EXTENT, cardname);

  /* Configure our driver routines to the kernel's INET subsystem. */

  dev->open               = net_open;
  dev->stop               = net_close;
  dev->hard_start_xmit    = net_send_packet;
  dev->get_stats          = net_get_stats;
  dev->set_multicast_list = set_multicast_list;
#ifdef LINUX_20
  dev->set_mac_address    = net_set_mac_address;
#else
  dev->tx_timeout         = net_tx_timeout;
  dev->watchdog_timeo     = MY_TX_TIMEOUT;
#endif

  /* Get the default MAC address */

  net_get_mac_address(dev);

  /* Fill in the fields of the device structure with ethernet values.
   * (see net_init.c).  */

  ether_setup(dev);

  /* Capture the attempts to set the MAC address */

#ifndef LINUX_20
  eth_set_mac_address  = dev->set_mac_address;
  dev->set_mac_address = net_set_mac_address;
#endif

  dbg("%s driver registered with kernel", cardname);
  return 0;
}

/***********************************************************************
 * net_open
 *
 * Open/initialize the board. This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is non-reboot way to recover if something goes wrong.
 *
 ***********************************************************************/

static int
net_open(struct NET_DEVICE *dev)
{
  struct net_local *np = (struct net_local *)dev->priv;
  int irqval;

  dbg("request_irq(%d, 0x%08lx, 0, %s, 0x%08lx)",
      (int)dev->irq, (long)net_interrupt, cardname, (long)dev);

  /* This is used if the interrupt line can turned off (shared).
   * See 3c503.c for an example of selecting the IRQ at config-time.
   */

#ifdef LINUX_20
  dev->tbusy     = 0;
  dev->start     = 1;
  dev->interrupt = 0;
#endif /* LINUX_20 */

  irqval = request_irq(dev->irq, net_interrupt, 0, cardname, dev);
  if (irqval)
    {
      warn("%s: unable to get IRQ %d (irqval=%d)",
	   dev->name, dev->irq, irqval);
      return -EAGAIN;
    }

  /* Always allocate the DMA channel after the IRQ,
   * and clean up on failure.
   * -- We have no DMA.
   */

  net_reset_mac_address(dev);

  /* Reset the hardware here. Don't forget to set the station address. */

  eth_turn_on_ethernet();
  np->open_time  = jiffies;

#ifdef LINUX_20
  dev->tbusy     = 0;
  dev->interrupt = 0;
  dev->start     = 1;

  MOD_INC_USE_COUNT;
#else
  /* We are now ready to accept transmit requeusts from
   * the queueing layer of the networking.
   */

  netif_start_queue(dev);
#endif

  return 0;
}

/***********************************************************************
 * net_send_packet
 *
 * LINUX_24:
 * This will only be invoked if your driver is _not_ in XOFF state.
 * What this means is that you need not check it, and that this
 * invariant will hold if you make sure that the netif_*_queue()
 * calls are done at the proper times.
 *
 ***********************************************************************/

static int
net_send_packet(struct sk_buff *skb, struct NET_DEVICE *dev)
{
  struct net_local *np;
  short             length;

  dbg("skb=0x%08lx, dev=0x%08lx", (long)skb, (long)dev);

  /* If some error occurs while trying to transmit this
   * packet, you should return '1' from this function.
   * In such a case you _may not_ do anything to the
   * SKB, it is still owned by the network queueing
   * layer when an error is returned.  This means you
   * may not modify any SKB fields, you may not free
   * the SKB, etc.
   */

#ifdef LINUX_20
  if (dev->tbusy)
    {
      /* If we get here, some higher level has decided we are broken.
       * They had been waiting for an indication that the prior xmit had
       * finished but never received it. Do what we can here to get the
       * h/w running again.
       */

      int tickssofar = jiffies - dev->trans_start;
      if (tickssofar < 5) return 1;

      warn("%s: suspect transmit timed out", dev->name);

      eth_turn_on_ethernet();

      dev->tbusy       = 0;
      dev->trans_start = jiffies;
    }
#endif

  if (!skb)
    {
      warn("%s: unexpected null skb", dev->name);
      return 1;
    }
  if (!dev)
    {
      warn("unexpected null device");
      return 1;
    }

  np     = (struct net_local *)dev->priv;
  length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;

  dbg("np=0x%08lx, length=%d", (long)np, length);

#if TX_RING

  /* This is the most common case for modern hardware.
   * The spinlock protects this code from the TX complete
   * hardware interrupt handler.  Queue flow control is
   * thus managed under this lock as well.
   */

  spin_lock_irq(&np->lock);

  add_to_tx_ring(np, skb, length);
  dev->trans_start = jiffies;

  /* If we just used up the very last entry in the
   * TX ring on this device, tell the queueing
   * layer to send no more.
   */

  if (tx_full(dev))
    {
      netif_stop_queue(dev);
    }

  /* When the TX completion hw interrupt arrives, this
   * is when the transmit statistics are updated.
   */

  spin_unlock_irq(&np->lock);

#else

  /* This is the case for older hardware which takes
   * a single transmit buffer at a time, and it is
   * just written to the device via PIO.
   *
   * No spin locking is needed since there is no TX complete
   * event.  If by chance your card does have a TX complete
   * hardware IRQ then you may need to utilize np->lock here.
   */

#ifdef LINUX_20
  /* Block a timer-based transmit from overlapping. This could better be
   * done with atomic_swap(1, dev->tbusy), but set_bit() works as well.
   */

  if (set_bit(0, (void*)&dev->tbusy) != 0)
    {
      warn("%s: Transmitter access conflict", dev->name);
    }
  else
    {
      eth_send_packet(skb->data, length);
      dev->trans_start = jiffies;
    }
  dev_kfree_skb(skb,FREE_WRITE);
#else /* LINUX_20 */
  eth_send_packet(skb->data, length);
  np->stats.tx_bytes += skb->len;

  dev->trans_start = jiffies;

  dev_kfree_skb(skb);
#endif /* LINUX_20 */
#endif

  return 0;
}

/***********************************************************************
 * net_interrupt.  Interrupt handler that is invoked whenever a
 * packet is received or transmitted to the network.
 ***********************************************************************/

static void
net_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  struct NET_DEVICE *dev = (struct NET_DEVICE *)dev_id;
  struct net_local  *np;

  verbose("");

  if (dev == NULL)
    {
      warn("%s: irq %d for unknown device", cardname, irq);
      return;
    }
#ifdef LINUX_20
  dev->interrupt = 1;
#endif /* LINUX_20 */

  np = (struct net_local *)dev->priv;

  if (eth_recv_int_active())
    {
      /* Got packet(s) */

      net_rx(dev);
    }

#if TX_EVENT
  if (eth_xmit_int_active())
    {
      /* Transmit complete. */

      net_tx(dev);

#if TX_RING
      np->stats.tx_packets++;
      netif_wake_queue(dev);
#endif /* TX_RING */

    }
#endif /* TX_EVENT */

#ifdef LINUX_20
  dev->interrupt = 0;
#endif /* LINUX_20 */
}

/***********************************************************************
 * net_rx.
 *
 * We have a good packet(s), get it/them out of the buffers.
 *
 ***********************************************************************/

static void
net_rx(struct NET_DEVICE *dev)
{
  struct net_local *lp = (struct net_local *)dev->priv;
  struct sk_buff *skb;
  int packet_len;

  /* Get the number of bytes in the first packet of the frame. */

  eth_get_received_packet_len(&packet_len);

  /* Loop until a zero-length packet is reported. */

  while (packet_len)
    {
      verbose("packet_len=%d", packet_len);

      /* Allocate memory for the packet data. */

#ifdef LINUX_20
      skb = alloc_skb(packet_len + 2, GFP_ATOMIC);
#else
      skb = dev_alloc_skb(packet_len + 2);
#endif /* LINUX_20 */
      if (skb == NULL)
	{
	  warn("%s: Memory squeeze, dropping packet", dev->name);
	  lp->stats.rx_dropped++;
	  dbg("rx_dropped=%ld", lp->stats.rx_dropped);
	  return;
	}

      skb_reserve(skb, 2);  /* align the buffer */
      skb->len = packet_len;
      skb->dev = dev;

      /* 'skb->data' points to the start of sk_buff data area. */

      eth_get_received_packet_data(skb->data,(int *)(&(skb->len)));

      if (eth_rx_errors())
	{
	  lp->stats.rx_errors++;
	  dbg("rx_errors=%ld", lp->stats.rx_errors);
	}
      if (eth_rx_length_errors())
	{
	  lp->stats.rx_length_errors++;
	  dbg("rx_length_errors=%ld", lp->stats.rx_length_errors);
	}
      if (eth_rx_over_errors())
	{
	  lp->stats.rx_over_errors++;
	  dbg("rx_over_errors=%ld", lp->stats.rx_over_errors);
	}
      if (eth_rx_crc_errors())
	{
	  lp->stats.rx_crc_errors++;
	  dbg("rx_crc_errors=%ld", lp->stats.rx_crc_errors);
	}
      if (eth_rx_frame_errors())
	{
	  lp->stats.rx_frame_errors++;
	  dbg("rx_frame_errors=%ld", lp->stats.rx_frame_errors);
	}
      if (eth_rx_fifo_errors())
	{
	  lp->stats.rx_fifo_errors++;
	  dbg("rx_fifo_errors=%ld", lp->stats.rx_fifo_errors);
	}
      if (eth_rx_missed_errors())
	{
	  lp->stats.rx_missed_errors++;
	  dbg("rx_missed_errors=%ld", lp->stats.rx_missed_errors);
	}

      /* This INET call also modifies skb->pkt_type, skb->mac.raw, etc */

      skb->protocol = eth_type_trans(skb, dev); 

      /* Inform Linux INET subsystem. */
#ifdef LINUX_20
      lp->stats.rx_packets++;
      netif_rx(skb);
#else /* LINUX_20 */
      netif_rx(skb); 

      dev->last_rx  = jiffies;
      lp->stats.rx_packets++;
      lp->stats.rx_bytes += packet_len;
#endif /* LINUX_20 */

      /* Get the length of the next packet. */

      eth_get_received_packet_len(&packet_len);
    }  
  return;
}

/***********************************************************************
 * net_tx
 ***********************************************************************/

#if TX_EVENT
static void
net_tx(struct NET_DEVICE *dev)
{
#if TX_RING
  struct net_local *np = (struct net_local *)dev->priv;
  int entry;

  verbose("");

  /* This protects us from concurrent execution of
   * our dev->hard_start_xmit function above.
   */
  spin_lock(&np->lock);

  entry = np->tx_old;
  while (tx_entry_is_sent(np, entry))
    {
      struct sk_buff *skb = np->skbs[entry];

      np->stats.tx_bytes += skb->len;
      dev_kfree_skb_irq (skb);

      entry = next_tx_entry(np, entry);
    }
  np->tx_old = entry;

  /* If we had stopped the queue due to a "tx full"
   * condition, and space has now been made available,
   * wake up the queue.
   */
  if (netif_queue_stopped(dev) && ! tx_full(dev))
    {
      netif_wake_queue(dev);
    }

  spin_unlock(&np->lock);
#else

  struct net_local *np = (struct net_local *)dev->priv;

#ifdef LINUX_20
  dev->tbusy = 0;
#endif /* LINUX_20 */

  /* Update statistics */

  np->stats.tx_packets++;
  verbose("tx_packets=%ld", np->stats.rx_errors);

  if (eth_tx_errors())
    {
      np->stats.tx_errors++;
      dbg("tx_errors=%ld", np->stats.tx_errors);
    }
  if (eth_tx_aborted_errors())
    {
      np->stats.tx_aborted_errors++;
      dbg("tx_aborted_errors=%ld", np->stats.tx_aborted_errors);
    }
  if (eth_tx_carrier_errors())
    {
      np->stats.tx_carrier_errors++;
      dbg("tx_carrier_errors=%ld", np->stats.tx_carrier_errors);
    }
  if (eth_tx_fifo_errors())
    {
      np->stats.tx_fifo_errors++;
      dbg("tx_fifo_errors=%ld", np->stats.tx_fifo_errors);
    }
  if (eth_tx_heartbeat_errors())
    {
      np->stats.tx_heartbeat_errors++;
      dbg("tx_heartbeat_errors=%ld", np->stats.tx_heartbeat_errors);
    }
  if (eth_tx_window_errors())
    {
      np->stats.tx_window_errors++;
      dbg("tx_window_errors=%ld", np->stats.tx_window_errors);
    }
#endif /* TX_RING */

#ifdef LINUX_20
  /* Driver now informs Linux INET subsystem. */

  mark_bh(NET_BH);
#endif /* LINUX_20 */
}
#endif /* TX_EVENT */

/***********************************************************************
 * net_close
 ***********************************************************************/

static int
net_close(struct NET_DEVICE *dev)
{
  struct net_local *np = (struct net_local *)dev->priv;

  dbg("");

#ifdef LINUX_20
  eth_turn_off_ethernet();
  free_irq(dev->irq, dev);

  np->open_time = 0;

  dev->tbusy    = 1;
  dev->start    = 0;

  MOD_DEC_USE_COUNT;
#else
  np->open_time = 0;

  netif_stop_queue(dev);

  eth_turn_off_ethernet();
  free_irq(dev->irq, dev);  
#endif

  /* Update statistics here */

  return 0;
}

/***********************************************************************
 * net_get_status
 ***********************************************************************/

static struct NET_DEVICE_STATS *net_get_stats(struct NET_DEVICE *dev)
{
  struct net_local *np = (struct net_local *)dev->priv;

  dbg("");

  /* Update statistics here -- with interrupts disabled */

  return &np->stats;
}

/***********************************************************************
 * set_multicast_list
 ***********************************************************************/

static void set_multicast_list(struct NET_DEVICE *dev)
{
  int eim_addr_mode;              /* ENET_ADR_ defines */
  int cpu_fltr_mode;              /* EIM_FILTER_ defines */

	if(dev->flags&IFF_PROMISC)
	{
    eim_addr_mode = ENET_ADR_PROMISCUOUS;
    cpu_fltr_mode = EIM_FILTER_MULTICAST | 
                    EIM_FILTER_BROADCAST | 
                    EIM_FILTER_UNICAST;
    eth_multicast_promiscuous();
	}
	else if(dev->flags&IFF_ALLMULTI)
	{
    eim_addr_mode = ENET_ADR_PROMISCUOUS;
    cpu_fltr_mode = EIM_FILTER_MULTICAST | 
                    EIM_FILTER_BROADCAST | 
                    EIM_FILTER_UNICAST;
    eth_multicast_accept_multicast();
	} 
	else if(dev->mc_count > 0)
	{
		/* Accept all multicast packets and
		   rely on higher-level filtering */
    eim_addr_mode = ENET_ADR_PROMISCUOUS;
    cpu_fltr_mode = EIM_FILTER_MULTICAST | 
                    EIM_FILTER_BROADCAST | 
                    EIM_FILTER_UNICAST;
    eth_multicast_accept_multicast();
	} 
	else {
    /* just accept broadcast packets and packets
       sent to this device */     
    eim_addr_mode = ENET_ADR_PROMISCUOUS;
    cpu_fltr_mode = EIM_FILTER_BROADCAST | 
                    EIM_FILTER_UNICAST;
  }

  eth_set_addressing(eim_addr_mode);
  eth_set_filter(cpu_fltr_mode);
}

/***********************************************************************
 * net_tx_timeout
 ***********************************************************************/

#ifndef LINUX_20
static void net_tx_timeout(struct NET_DEVICE *dev)
{
  struct net_local *np = (struct net_local *)dev->priv;

  warn("%s: transmit timed out", dev->name);

  /* Try to restart the adaptor. */

  eth_turn_on_ethernet();

  np->stats.tx_errors++;
  dbg("tx_errors=%ld", np->stats.tx_errors);

  /* If we have space available to accept new transmit
   * requests, wake up the queueing layer.  This would
   * be the case if the eth_turn_on_ethernet() call above just
   * flushes out the tx queue and empties it.
   *
   * If instead, the tx queue is retained then the
   * netif_wake_queue() call should be placed in the
   * TX completion interrupt handler of the driver instead
   * of here.
   */

  if (!tx_full(dev))
    {
      netif_wake_queue(dev);
    }
}
#endif /* LINUX_20 */

/***********************************************************************
 * net_get_mac_address
 ***********************************************************************/

static void net_get_mac_address(struct NET_DEVICE *dev)
{
  eth_get_default_MAC(dev->dev_addr);

  dbg("Default ether MAC address = %02x:%02x:%02x:%02x:%02x:%02x",
      dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2], 
      dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);
}

/***********************************************************************
 * net_set_mac_address
 ***********************************************************************/

static int net_set_mac_address(struct NET_DEVICE *dev, void *addr)
#ifndef LINUX_20
{
  int retval = 0;

  dbg("");

  /* Get the MAC address.  Do we have a pointer to the MAC
   * address getting function?  Did it successfully get the
   * MAC address? */

  if (eth_set_mac_address)
    {
      if (eth_set_mac_address(dev,addr) == 0)
	{
	  /* Yes, set the HW MAC address. */

	  retval = net_reset_mac_address(dev);
	}
    }
  else
    {
      /* No pointer, set the hardware MAC address. */

      retval = net_reset_mac_address(dev);
    }
  return retval;
}
#else /* LINUX_20 */
{
  /* Copy the address */

  memcpy(dev->dev_addr, addr, 6);

  /* Set the hardware MAC address. */

  return net_reset_mac_address(dev);
}
#endif /* LINUX_20 */

/***********************************************************************
 * net_reset_mac_address
 ***********************************************************************/

static int net_reset_mac_address(struct NET_DEVICE *dev)
{
  eth_set_MAC(dev->dev_addr);

  dbg("Set ether MAC address = %02x:%02x:%02x:%02x:%02x:%02x",
      dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2], 
      dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

  return 0;
}

/***********************************************************************
 * MODULE LOGIC
 ***********************************************************************/

#ifdef MODULE

/* The NET_DEVICE structure for this device. */

#ifdef LINUX_20
static char devicename[9] = { 0, };
static struct NET_DEVICE this_device =
{
  devicename,     /* will be inserted by linux/drivers/net/net_init.c */
  0,              /* rmem_end:   shmem "recv" end */
  0,              /* rmem_start: shmem "recv" start */
  0,              /* mem_end:    shared mem end */
  0,              /* mem_start:  shared mem start */
  ETHER_BASE,     /* base_addr:  device I/O address */
  ETHER_IRQ,      /* irq:        device IRQ number */
  0,              /* start:      start an operation */
  0,              /* interrupt:  interrupt arrived */
  0,              /* tbusy:      transmitter busy */
  NULL,           /* next:       list implementation */
  c5471_net_probe /* init:       initialization function */
                  /* (Rest implicitly initialized to zero) */
};
#else /* LINUX_20 */
static struct NET_DEVICE this_device;
#endif /* LINUX_20 */

/* Set some defaults which the insmod command line can override
 * (which, of course, is kind of silly because these are not optional
 * values). */

static int io  = ETHER_BASE;   /* Base address of registers */
static int irq = ETHER_IRQ;    /* IRQ of ethernet device */
static int dma = 0;            /* We don't support DMA. */
static int mem = 0;            /* We don't use this */

MODULE_PARM(io,  "i");
MODULE_PARM(irq, "i");
MODULE_PARM(dma, "i");
MODULE_PARM(mem, "i");
MODULE_PARM(LAN_Rate, "i");

MODULE_PARM_DESC(io,       "C5471 Ethernet I/O base address");
MODULE_PARM_DESC(irq,      "C5471 Ethernet IRQ number");
MODULE_PARM_DESC(LAN_Rate, "0=Autonegotiate, 10=10BaseT, 100=100BaseT");

MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

/***********************************************************************
 * net_init
 ***********************************************************************/

#ifdef LINUX_20
static int net_init(void)
#else
static int __init net_init(void)
#endif
{
  int result;

  printk("C5471 C5471 Ethernet Driver, LAN_Rate=%d\n", LAN_Rate);

  if (io == 0)
    {
      warn("%s: You shouldn't use auto-probing with insmod!", cardname);
    }

  /* Copy the parameters from insmod into the device structure. */

  this_device.name[0]   = '\0';            /* System will provide name */
  this_device.base_addr = io;              /* Base address of registers */
  this_device.irq       = irq;             /* IRQ of ethernet device */
  this_device.dma       = dma;             /* We don't support DMA. */
  this_device.mem_start = mem;             /* We don't use this */
#ifndef LINUX_20
  this_device.init      = c5471_net_probe; /* Address of init function */
#endif
	
  result = register_netdev(&this_device);
  if (result != 0)
    {
      err("Failed to register ethernet driver, result=%d", result);
      return result;
    }

  dbg("Successfully registered net device");
  return 0;
}

/***********************************************************************
 * net_cleanup
 ***********************************************************************/

#ifdef LINUX_20
static void net_cleanup(void)
#else
static void __exit net_cleanup(void)
#endif
{
  dbg("");

  /* No need to check MOD_IN_USE, as sys_delete_module() checks. */

  unregister_netdev(&this_device);

  /* If we don't do this, we can't re-insmod it later.
   * Release irq/dma here, when you have jumpered versions and
   * allocate them in c5471_net_probe().
   */

  if (this_device.irq)
    {
      free_irq(this_device.irq, &this_device);
    }

  if (this_device.dma)
    {
      free_dma(this_device.dma);
    }

  if (this_device.base_addr)
    {
      release_region(this_device.base_addr, NETCARD_IO_EXTENT);
    }

  if (this_device.priv)
    {
#ifdef LINUX_20
      kfree_s(this_device.priv, sizeof(struct net_local));
#else
      kfree(this_device.priv);
#endif
    }
}

module_init(net_init);
module_exit(net_cleanup);

#endif /* MODULE */
/*
 * Local variables:
 *  c-indent-level: 2
 *  tab-width: 2
 * End:
 */
