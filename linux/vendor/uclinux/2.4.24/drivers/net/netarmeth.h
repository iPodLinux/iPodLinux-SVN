/*
 * drivers/net/netarmeth.h
 *
 * Copyright (C) 2000 NETsilicon, Inc.
 * Copyright (C) 2000 WireSpeed Communications Corporation
 * Copyright (C) 2001 IMMS gGmbH
 *
 * This software is copyrighted by WireSpeed. LICENSEE agrees that
 * it will not delete this copyright notice, trademarks or protective
 * notices from any copy made by LICENSEE.
 *
 * This software is provided "AS-IS" and any express or implied 
 * warranties or conditions, including but not limited to any
 * implied warranties of merchantability and fitness for a particular
 * purpose regarding this software. In no event shall WireSpeed
 * be liable for any indirect, consequential, or incidental damages,
 * loss of profits or revenue, loss of use or data, or interruption
 * of business, whether the alleged damages are labeled in contract,
 * tort, or indemnity.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * author(s) : Jackie Smith Cashion and David Smith
 *             Rolf Peukert (Linux Kernel 2.4.x adaption)
 */

#undef NA_DEBUG
// #define NA_DEBUG 1

#ifdef NA_DEBUG
#define NA_PRINTK(fmt, arg...) printk(KERN_INFO fmt, ##arg)
#else
#define NA_PRINTK(fmt, arg...)
#endif

#if (NA_DEBUG > 1)
#define NA_PRINTK2(fmt, arg...) printk(KERN_INFO fmt, ##arg)
#else
#define NA_PRINTK2(fmt, arg...)
#endif

#if (NA_DEBUG > 2)
#define NA_PRINTK3(fmt, arg...) printk(KERN_INFO fmt, ##arg)
#else
#define NA_PRINTK3(fmt, arg...)
#endif

// DMA Buffer Descriptor

struct na_dma_buf_desc {
  unsigned long  src_buf_ptr;  // Source Buffer Pointer (actually bits 0:28)
  unsigned short buf_len;      // Buffer Length (actually bits 0:14)
  unsigned short stat;         // Status (bits 16:31)
};

// Ethernet Receive Status Register in DMA Buffer Descriptor Status Field
#define NA_RXSTAT_ERR_MASK 0x07E0 // Error Mask
#define NA_RXSTAT_RXOK     0x2000 // Receive Packet OK
#define NA_RXSTAT_ROVER    0x0020 // Receive Overflow (Overrun)

// General Purpose Definitions

#define NA_TRUE       1
#define NA_FALSE      0

#define NA_10MBPS     0
#define NA_100MBPS    1

#define NA_LEFT  0
#define NA_RIGHT 1

#define NA_POLYNOMIAL 0x4c11db6L

#define NA_MAX_ETH_FRAME 1526
#define NA_MAX_ETH_FRAME_ALIGN (NA_MAX_ETH_FRAME + (NA_MAX_ETH_FRAME % 32))
#define NA_MAX_TX_BUF 1
//#define NA_MAX_RX_BUF 8
#define NA_MAX_RX_BUF 24

#define NA_MAX_MCA 64 // Maximum number of multicast addresses in hash table

// MII polling timeout value
// jiffies = number of 100 Hz ticks (1/100 seconds) since system boot
#define NA_MII_POLL_BUSY_DELAY 100    // 100 jiffies = 1 second

// MII negotiation timeout value
#define NA_MII_NEGOTIATE_DELAY 500    // 500 jiffies = 5 seconds

/* Type of the PHY chip attached to the NetARM */
typedef enum {NA_ENABLE_PHY, NA_LEVEL1_PHY, NA_HOME_PHY} na_phy_type;

// NET+ARM Ethernet Driver Private Data

struct netarmeth_dev {
  unsigned long           media_100BaseT;             // NA_10MBPS or NA_100MBPS
  struct na_dma_buf_desc  tx_dma_desc;
  struct na_dma_buf_desc  rx_dma_desc[NA_MAX_RX_BUF];
  int                     rx_sw_index;
  struct sk_buff         *tx_skb;
  struct sk_buff_head	  tx_skb_queue;
  struct sk_buff         *rx_skb[NA_MAX_RX_BUF];
  struct net_device_stats stats;
  struct timer_list       rx_fifo_timer;
  int                     rx_fifo_packets;
  /* Low-level status flags: */
  unsigned long		  tbusy; /* transmitter busy (must be long for bitops) */
  volatile unsigned char  start,		/* start an operation	*/
                          rx_interrupt,		/* receive interrupt arrived */
                          tx_interrupt;		/* transmit interrupt arrived */
  na_phy_type             phy_type; /* identified PHY chip */
};

/*
 * Translated address IO functions
 *
 * IO address has already been translated to a virtual address
 */
#define outb_t(v,p)	(*(volatile unsigned char *)(p) = (v))
#define outw_t(v,p)	(*(volatile unsigned short *)(p) = (v))
#define outl_t(v,p)	(*(volatile unsigned long *)(p) = (v))

#define inb_t(p) 	 (*(volatile unsigned char *)(p))
#define inw_t(p) 	(*(volatile unsigned short *)(p))
#define inl_t(p)	(*(volatile unsigned long *)(p))

