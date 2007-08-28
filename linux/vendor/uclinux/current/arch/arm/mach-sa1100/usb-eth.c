 /*
 * Ethernet driver for the SA1100 USB client function
 * Copyright (c) 2001 by Nicolas Pitre
 *
 * This code was loosely inspired by the original initial ethernet test driver
 * Copyright (c) Compaq Computer Corporation, 1999
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is still work in progress...
 *
 * 19/02/2001 - Now we are compatible with generic usbnet driver. green@iXcelerator.com
 * 09/03/2001 - Dropped 'framing' scheme, as it seems to cause a lot of problems with little benefit.
 *		Now, since we do not know what size of packet we are receiving
 *		last usb packet in sequence will always be less than max packet
 *		receive endpoint can accept.
 *		Now the only way to check correct start of frame is to compare
 *		MAC address. Also now we are stalling on each receive error.
 *
 * 15/03/2001 - Using buffer to get data from UDC. DMA needs to have 8 byte
 *		aligned buffer, but this breaks IP code (unaligned access).
 *
 * 01/04/2001 - stall endpoint operations appeared to be very unstable, so
 *              they are disabled now.
 *
 * 03/06/2001 - Readded "zerocopy" receive path (tunable).
 *
 */

// Define DMA_NO_COPY if you want data to arrive directly into the
// receive network buffers, instead of arriving into bounce buffer
// and then get copied to network buffer.
// This does not work correctly right now.
#undef DMA_NO_COPY

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/timer.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/random.h>

#include "sa1100_usb.h"


#define ETHERNET_VENDOR_ID 0x49f
#define ETHERNET_PRODUCT_ID 0x505A
#define MAX_PACKET 32768
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// Should be global, so that insmod can change these
int usb_rsize=64;
int usb_wsize=64;

static struct usbe_info_t {
  struct net_device *dev;
  u16 packet_id;
  struct net_device_stats stats;
} usbe_info;

static char usb_eth_name[16] = "usbf";
static struct net_device usb_eth_device;
static struct sk_buff *cur_tx_skb, *next_tx_skb;
static struct sk_buff *cur_rx_skb, *next_rx_skb;
static volatile int terminating;
#ifndef DMA_NO_COPY
static char *dmabuf; // we need that, as dma expect it's buffers to be aligned on 8 bytes boundary
#endif

static int usb_change_mtu (struct net_device *net, int new_mtu)
{
	if (new_mtu <= sizeof (struct ethhdr) || new_mtu > MAX_PACKET)
		return -EINVAL;
	// no second zero-length packet read wanted after mtu-sized packets
	if (((new_mtu + sizeof (struct ethhdr)) % usb_rsize) == 0)
		return -EDOM;

	net->mtu = new_mtu;
	return 0;
}

static struct sk_buff *
usb_new_recv_skb(void)
{
	struct sk_buff *skb = alloc_skb( 2 + sizeof (struct ethhdr) + usb_eth_device.mtu,GFP_ATOMIC);

	if (skb) {
		skb_reserve(skb, 2);
	}
	return skb;
}

static u8 bcast_hwaddr[ETH_ALEN]={0xff,0xff,0xff,0xff,0xff,0xff};
static void
usb_recv_callback(int flag, int size)
{
	struct sk_buff *skb;

	if (terminating)
		return;

	skb = cur_rx_skb;

	/* flag validation */
	if (flag == 0) {
		if ( skb_tailroom (skb) < size ) { // hey! we are overloaded!!!
			usbe_info.stats.rx_over_errors++;
			goto error;
		}
#ifndef DMA_NO_COPY
		memcpy(skb->tail,dmabuf,size);
#endif
		skb_put(skb, size);
	} else {
		if (flag == -EIO) {
			usbe_info.stats.rx_errors++;
		}
		goto error;
	}

	/* validate packet length */
	if (size == usb_rsize ) {
		/* packet not complete yet */
		skb = NULL;
	}

	/*
	 * At this point skb is non null if we have a complete packet.
	 * If so take a fresh skb right away and restart USB receive without
	 * further delays, then process the packet.  Otherwise resume USB
	 * receive on the current skb and exit.
	 */

	if (skb)
		cur_rx_skb = next_rx_skb;
#ifndef DMA_NO_COPY
	sa1100_usb_recv(dmabuf, usb_rsize,
			usb_recv_callback);
#else
	sa1100_usb_recv(cur_rx_skb->tail, MIN(usb_rsize, skb_tailroom (cur_rx_skb)),
			usb_recv_callback);
#endif
	if (!skb)
		return;

	next_rx_skb = usb_new_recv_skb();
	if (!next_rx_skb) {
		/*
		 * We can't aford loosing buffer space...
		 * So we drop the current packet and recycle its skb.
		 */
		printk("%s: can't allocate new skb\n", __FUNCTION__);
		usbe_info.stats.rx_dropped++;
		skb_trim(skb, 0);
		next_rx_skb = skb;
		return;
	}
	if ( skb->len >= sizeof(struct ethhdr)) {
		if (memcmp(skb->data,usb_eth_device.dev_addr,ETH_ALEN) && memcmp(skb->data,bcast_hwaddr,ETH_ALEN) ) {
			// This frame is not for us. nor it is broadcast
			usbe_info.stats.rx_frame_errors++;
			kfree_skb(skb);
			goto error;
		}
	}

	if (skb->len) {
		int     status;
// FIXME: eth_copy_and_csum "small" packets to new SKB (small < ~200 bytes) ?

		skb->dev = &usb_eth_device;
		skb->protocol = eth_type_trans (skb, &usb_eth_device);
		usbe_info.stats.rx_packets++;
		usbe_info.stats.rx_bytes += skb->len;
		skb->ip_summed = CHECKSUM_NONE;
		status = netif_rx (skb);
		if (status != NET_RX_SUCCESS)
			printk("netif_rx failed with code %d\n",status);
	} else {
error:
		/*
		 * Error due to HW addr mismatch, or IO error.
		 * Recycle the current skb and reset USB reception.
		 */
		skb_trim(cur_rx_skb, 0);
//		if ( flag == -EINTR || flag == -EAGAIN ) // only if we are coming out of stall
#ifndef DMA_NO_COPY
			sa1100_usb_recv(dmabuf, usb_rsize, usb_recv_callback);
#else
			sa1100_usb_recv(cur_rx_skb->tail, MIN(usb_rsize, skb_tailroom (cur_rx_skb)), usb_recv_callback);
#endif
	}
}


static void
usb_send_callback(int flag, int size)
{
	struct net_device *dev = usbe_info.dev;
	struct net_device_stats *stats;
	struct sk_buff *skb=cur_tx_skb;
	int ret;

	if (terminating)
		return;

	stats = &usbe_info.stats;
	switch (flag) {
	    case 0:
		stats->tx_packets++;
		stats->tx_bytes += size;
		break;
	    case -EIO:
		stats->tx_errors++;
		break;
	    default:
		stats->tx_dropped++;
		break;
	}

	cur_tx_skb = next_tx_skb;
	next_tx_skb = NULL;
	dev_kfree_skb_irq(skb);
	if (!cur_tx_skb)
		return;

	dev->trans_start = jiffies;
	ret = sa1100_usb_send(cur_tx_skb->data, cur_tx_skb->len, usb_send_callback);
	if (ret) {
		/* If the USB core can't accept the packet, we drop it. */
		dev_kfree_skb_irq(cur_tx_skb);
		cur_tx_skb = NULL;
		usbe_info.stats.tx_carrier_errors++;
	}
	netif_wake_queue(dev);
}

static int
usb_eth_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret;
	unsigned long flags;

	if (next_tx_skb) {
		printk("%s: called with next_tx_skb != NULL\n", __FUNCTION__);
		return 1;
	}

	if (skb_shared (skb)) {
		struct sk_buff  *skb2 = skb_unshare(skb, GFP_ATOMIC);
		if (!skb2) {
			usbe_info.stats.tx_dropped++;
			dev_kfree_skb(skb);
			return 1;
		}
		skb = skb2;
	}

	if ((skb->len % usb_wsize) == 0) {
		skb->len++; // other side will ignore this one, anyway.
	}

	local_irq_save(flags);
	if (cur_tx_skb) {
		next_tx_skb = skb;
		netif_stop_queue(dev);
	} else {
		cur_tx_skb = skb;
		dev->trans_start = jiffies;
		ret = sa1100_usb_send(skb->data, skb->len, usb_send_callback);
		if (ret) {
			/* If the USB core can't accept the packet, we drop it. */
			dev_kfree_skb(skb);
			cur_tx_skb = NULL;
			usbe_info.stats.tx_carrier_errors++;
		}
	}
	local_irq_restore(flags);
	return 0;
}

static void
usb_xmit_timeout(struct net_device *dev )
{
	sa1100_usb_send_reset();
	dev->trans_start = jiffies;
	netif_wake_queue(dev);
}


static int
usb_eth_open(struct net_device *dev)
{
	terminating = 0;
	cur_tx_skb = next_tx_skb = NULL;
	cur_rx_skb = usb_new_recv_skb();
	next_rx_skb = usb_new_recv_skb();
	if (!cur_rx_skb || !next_rx_skb) {
		printk("%s: can't allocate new skb\n", __FUNCTION__);
		if (cur_rx_skb)
			kfree_skb(cur_rx_skb);
		if (next_rx_skb)
			kfree_skb(next_rx_skb);
		return -ENOMEM;;
	}

	MOD_INC_USE_COUNT;
#ifndef DMA_NO_COPY
	sa1100_usb_recv(dmabuf, usb_rsize, usb_recv_callback);
#else
	sa1100_usb_recv(cur_rx_skb->tail, MIN(usb_rsize, skb_tailroom (cur_rx_skb)),
			usb_recv_callback);
#endif
	return 0;
}

static int
usb_eth_release(struct net_device *dev)
{
	terminating = 1;
	sa1100_usb_send_reset();
	sa1100_usb_recv_reset();
	if (cur_tx_skb)
		kfree_skb(cur_tx_skb);
	if (next_tx_skb)
		kfree_skb(next_tx_skb);
	if (cur_rx_skb)
		kfree_skb(cur_rx_skb);
	if (next_rx_skb)
		kfree_skb(next_rx_skb);
	MOD_DEC_USE_COUNT;
	return 0;
}

static struct net_device_stats *
usb_eth_stats(struct net_device *dev)
{
	struct usbe_info_t *priv =  (struct usbe_info_t*) dev->priv;
	struct net_device_stats *stats=NULL;

	if (priv)
		stats = &priv->stats;
	return stats;
}

static int
usb_eth_probe(struct net_device *dev)
{
	u8 node_id [ETH_ALEN];

	get_random_bytes (node_id, sizeof node_id);
	node_id [0] &= 0xfe;    // clear multicast bit

	/*
	 * Assign the hardware address of the board:
	 * generate it randomly, as there can be many such
	 * devices on the bus.
	 */
	memcpy (dev->dev_addr, node_id, sizeof node_id);

	dev->open = usb_eth_open;
	dev->change_mtu = usb_change_mtu;
	dev->stop = usb_eth_release;
	dev->hard_start_xmit = usb_eth_xmit;
	dev->get_stats = usb_eth_stats;
	dev->watchdog_timeo = 1*HZ;
	dev->tx_timeout = usb_xmit_timeout;
	dev->priv = &usbe_info;

	usbe_info.dev = dev;

	/* clear the statistics */
	memset(&usbe_info.stats, 0, sizeof(struct net_device_stats));

	ether_setup(dev);
	dev->flags &= ~IFF_MULTICAST;
	dev->flags &= ~IFF_BROADCAST;
	//dev->flags |= IFF_NOARP;

	return 0;
}

#ifdef MODULE
MODULE_PARM(usb_rsize, "1i");
MODULE_PARM_DESC(usb_rsize, "number of bytes in packets from host to sa1100");
MODULE_PARM(usb_wsize, "1i");
MODULE_PARM_DESC(usb_wsize, "number of bytes in packets from sa1100 to host");
#endif

static int __init
usb_eth_init(void)
{
	int rc;

#ifndef DMA_NO_COPY
	dmabuf = kmalloc( usb_rsize, GFP_KERNEL | GFP_DMA );
	if (!dmabuf)
		return -ENOMEM;
#endif
	strncpy(usb_eth_device.name, usb_eth_name, IFNAMSIZ);
	usb_eth_device.init = usb_eth_probe;
	if (register_netdev(&usb_eth_device) != 0)
		return -EIO;

	rc = sa1100_usb_open( "usb-eth" );
	if ( rc == 0 ) {
		 string_desc_t * pstr;
		 desc_t * pd = sa1100_usb_get_descriptor_ptr();

		 pd->b.ep1.wMaxPacketSize = make_word( usb_rsize );
		 pd->b.ep2.wMaxPacketSize = make_word( usb_wsize );
		 pd->dev.idVendor	  = ETHERNET_VENDOR_ID;
		 pd->dev.idProduct     = ETHERNET_PRODUCT_ID;
		 pstr = sa1100_usb_kmalloc_string_descriptor( "SA1100 USB NIC" );
		 if ( pstr ) {
			  sa1100_usb_set_string_descriptor( 1, pstr );
			  pd->dev.iProduct = 1;
		 }
		 rc = sa1100_usb_start();
	}
	return rc;
}

module_init(usb_eth_init);

static void __exit
usb_eth_cleanup(void)
{
	string_desc_t * pstr;
	sa1100_usb_stop();
	sa1100_usb_close();
	if ( (pstr = sa1100_usb_get_string_descriptor(1)) != NULL )
		kfree( pstr );
#ifndef DMA_NO_COPY
	kfree(dmabuf);
#endif
	unregister_netdev(&usb_eth_device);
}

module_exit(usb_eth_cleanup);
