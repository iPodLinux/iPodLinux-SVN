/*
 * drivers/net/netarmeth.c
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
 *             Rolf Peukert (Kernel 2.4.x adaption in progress,
 *                           added support for AMD 79C901 HomePHY)
 */

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
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>

/* Include hardware definitions */
#define OLD_STYLE_NETARM_DMA_CONFIGURATION
#include <asm/arch/netarm_registers.h>
#include "netarmeth.h"

/* Function Prototypes (used internally in this module) */

static void na_disable_eth_irqs(void);
static void na_enable_eth_irqs(void);
static void na_get_mac_addr(struct net_device *dev);
static void na_rx_fifo_reset(unsigned long ldev);
static void na_enable_timer(struct net_device *dev);
static void na_disable_timer(struct net_device *dev);
static void na_reset_rx_fifo(struct netarmeth_dev *na_dev);
static void na_load_mca_hash_table(struct dev_mc_list *mc_list);

/* Static Global Variables */

static const char *cardname = "netarmeth"; // Interface Name


/*--------------------------------------------------------------------*
	Utility Functions
 *--------------------------------------------------------------------*/

/* Enable and disable functions for DMA interrupts.
   The ethernet service interrupts are not needed in DMA mode. */

static void
na_disable_eth_irqs(void)
{
  NA_PRINTK("na_disable_eth_irqs\n");

  disable_irq(IRQ_DMA01);
  disable_irq(IRQ_DMA02);
}

static void
na_enable_eth_irqs(void)
{
  NA_PRINTK("na_enable_eth_irqs\n");

  enable_irq(IRQ_DMA01);
  enable_irq(IRQ_DMA02);
}


// The Ethernet controller contains three station address registers.  Each
// 32 bit register contains 16 bits of the 48 bit MAC address. Ignore the high
// 16 bits of each register.

static void
na_get_mac_addr(struct net_device *dev)
{
  unsigned short *p;

  NA_PRINTK("na_get_mac_addr\n");

  p = (unsigned short *) dev->dev_addr;
  p[0] = (unsigned short) inl_t(get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_1));
  p[1] = (unsigned short) inl_t(get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_2));
  p[2] = (unsigned short) inl_t(get_eth_reg_addr(NETARM_ETH_SAL_STATION_ADDR_3));

  NA_PRINTK2("  %X %X %X %X %X %X\n",
    dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
    dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);
}


static void
na_rx_fifo_reset(unsigned long ldev)
{
  struct net_device *dev = (struct net_device *)ldev;
  struct netarmeth_dev *na_dev = dev->priv;

  /* If we're still receiving packets, keep going. */
  if (na_dev->stats.rx_packets != na_dev->rx_fifo_packets) {
    na_dev->rx_fifo_packets = na_dev->stats.rx_packets;
    NA_PRINTK2("na_rx_fifo_reset 1\n");
  }
  else if (inl_t(get_eth_reg_addr(NETARM_ETH_GEN_CTRL)) & NETARM_ETH_GCR_ERXDMA
	   && !(inl_t(get_dma_reg_addr(NETARM_DMA1A_STATUS)) & 0xdf000000)
	   && ((inl_t(get_eth_reg_addr(NETARM_ETH_GEN_STAT)) & 0x0a000000)
	       == 0x02000000)) { 
      na_reset_rx_fifo(na_dev);	    
  }
  na_enable_timer(dev);
}

static void
na_enable_timer(struct net_device *dev)
{
  struct netarmeth_dev *na_dev;

  na_dev = dev->priv;
    
  na_dev->rx_fifo_packets = na_dev->stats.rx_packets;
  init_timer(&na_dev->rx_fifo_timer);
  na_dev->rx_fifo_timer.function = na_rx_fifo_reset;
  na_dev->rx_fifo_timer.data = (unsigned long) dev;
  na_dev->rx_fifo_timer.expires = jiffies + (HZ >> 3);
  add_timer(&na_dev->rx_fifo_timer);
}

static void
na_disable_timer(struct net_device *dev)
{
  struct netarmeth_dev *na_dev;

  na_dev = dev->priv;    
  del_timer(&na_dev->rx_fifo_timer);
}

static void
na_reset_rx_fifo(struct netarmeth_dev *na_dev)
{
  unsigned long reg;
  int           i;

  NA_PRINTK("na_reset_rx_fifo\n");

  // Disable receive DMA.

  outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_GEN_CTRL))
	  & ~NETARM_ETH_GCR_ERXDMA),
	 get_eth_reg_addr(NETARM_ETH_GEN_CTRL));

  // Disable receive FIFO.

  outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_GEN_CTRL)) & ~NETARM_ETH_GCR_ERX),
	 get_eth_reg_addr(NETARM_ETH_GEN_CTRL));

  // Disable DMA

  outl_t((inl_t(get_dma_reg_addr(NETARM_DMA1A_CONTROL)) & 0x7ffffc00),
	 get_dma_reg_addr(NETARM_DMA1A_CONTROL));

  // Delay 20 microsconds.

  udelay(20);

  // If NRIP or CAIP is set, reset them.
  
  reg = inl_t(get_dma_reg_addr(NETARM_DMA1A_CONTROL));
  if (reg & 0x20000000) {
    reg &= 0x20f00000;
    outl_t(reg, get_dma_reg_addr(NETARM_DMA1A_CONTROL));
  }

  // Reset DMA descriptors

  for (i = 0; i < NA_MAX_RX_BUF; i++) {
    na_dev->rx_dma_desc[i].src_buf_ptr  = (unsigned long) virt_to_bus(na_dev->rx_skb[i]->data);
    na_dev->rx_dma_desc[i].src_buf_ptr &= ~(NETARM_DMA_BD0_LAST
					    | NETARM_DMA_BD0_IDONE);
    na_dev->rx_dma_desc[i].buf_len      = NA_MAX_ETH_FRAME;
    na_dev->rx_dma_desc[i].stat         = 0;
    // Clear the F bit.
    na_dev->rx_dma_desc[i].buf_len     &= ~NETARM_DMA_BD1_FULL;
  }
  na_dev->rx_dma_desc[NA_MAX_RX_BUF-1].src_buf_ptr |= NETARM_DMA_BD0_WRAP;
  na_dev->rx_sw_index = 0;

  // Reenable DMA

  outl_t((inl_t(get_dma_reg_addr(NETARM_DMA1A_CONTROL)) | 0x80000000),
	 get_dma_reg_addr(NETARM_DMA1A_CONTROL));

  // Reenable receive FIFO.

  outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_GEN_CTRL)) | NETARM_ETH_GCR_ERX),
	 get_eth_reg_addr(NETARM_ETH_GEN_CTRL));

  // Reenable receive DMA.

  outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_GEN_CTRL))
	  | NETARM_ETH_GCR_ERXDMA),
	 get_eth_reg_addr(NETARM_ETH_GEN_CTRL));
}

static inline void
na_start_xmit(struct netarmeth_dev *na_dev)
{
  unsigned long reg;
  struct sk_buff *skb = skb_dequeue(&na_dev->tx_skb_queue);
    
  if (skb == NULL) return;

  // The skb will be free'd in na_tx_interrupt.

  na_dev->tx_skb = skb;

  // Set up the transmit DMA buffer descriptor.

  na_dev->tx_dma_desc.src_buf_ptr  = (unsigned long) virt_to_bus(skb->data);
  na_dev->tx_dma_desc.src_buf_ptr |= (NETARM_DMA_BD0_WRAP
				      | NETARM_DMA_BD0_IDONE
				      | NETARM_DMA_BD0_LAST);
  na_dev->tx_dma_desc.buf_len      = skb->len;
  na_dev->tx_dma_desc.stat         = 0;
  na_dev->tx_dma_desc.buf_len     |= NETARM_DMA_BD1_FULL;

  // Clear possible Not Ready interrupt and reactivate DMA channel.

  reg = (inl_t(get_dma_reg_addr(NETARM_DMA2_STATUS))
	 & NETARM_DMA_STAT_INT_EN_MASK) | NETARM_DMA_STAT_NR_INTPEN;
  outl_t(reg, get_dma_reg_addr(NETARM_DMA2_STATUS));
}


/*--------------------------------------------------------------------*
  Functions for the Media Independent Interface (MII)

  These functions assume the current Ethernet controller configuration
  uses an ENABLE or Home PHY through the MII interface.
 *--------------------------------------------------------------------*/

static unsigned long
na_mii_poll_busy(void)
{
	unsigned long start_time, current_time;

	// Poll until the busy bit is clear or one second has elapsed.
	// (this is PHY-type-independent)

	start_time = current_time = jiffies;
	while ((current_time < start_time + NA_MII_POLL_BUSY_DELAY) &&
	       (inl_t(get_eth_reg_addr(NETARM_ETH_MII_IND)) &
	        NETARM_ETH_MIII_BUSY)) {
		current_time = jiffies;
	}
	if (current_time < start_time + NA_MII_POLL_BUSY_DELAY) {
		return (1);
	} else {
		NA_PRINTK("na_mii_poll_busy - one second elapsed.\n");
		return (0);
	}
}

// na_mii_identify_phy
//
// This function tries to identify the PHY chip in the current hardware
// configuration.
//
// Return Values:
//   NA_ENABLE_PHY (0)
//   NA_LEVEL1_PHY (1)
//   NA_HOME_PHY (2)	-> declared in netarmeth.h

static na_phy_type
na_mii_identify_phy(void)
{
	int id_reg_a, id_reg_b;

	NA_PRINTK("na_mii_identify_phy\n");

	/* check for Enable PHY */
	outl_t(0x402, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
	outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_MII_CMD)) | NETARM_ETH_MIIC_RSTAT),
	       get_eth_reg_addr(NETARM_ETH_MII_CMD));
	na_mii_poll_busy();
	id_reg_a = inl_t(get_eth_reg_addr(NETARM_ETH_MII_READ));

	if (id_reg_a == 0x0043) {
		/* This must be an Enable or a Lucent LU3X31 PHY chip */
		NA_PRINTK("  Enable PHY\n");
		return (NA_ENABLE_PHY);
	}

	/* check for AMD 79C901 HomePHY */
	outl_t(0x102, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
	outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_MII_CMD)) | NETARM_ETH_MIIC_RSTAT),
	       get_eth_reg_addr(NETARM_ETH_MII_CMD));
	na_mii_poll_busy();
	id_reg_a = inl_t(get_eth_reg_addr(NETARM_ETH_MII_READ));

	outl_t(0x103, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
	outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_MII_CMD)) | NETARM_ETH_MIIC_RSTAT),
	       get_eth_reg_addr(NETARM_ETH_MII_CMD));
	na_mii_poll_busy();
	id_reg_b = inl_t(get_eth_reg_addr(NETARM_ETH_MII_READ));

	if ((id_reg_a == 0x0000) && (id_reg_b == 0x6b71)) {
		/* This must be an AMD Home PHY chip */
		NA_PRINTK("  Home PHY\n");
		return (NA_HOME_PHY);
	}

	/* else, i.e. default */
	NA_PRINTK("  Level 1 PHY\n");
	return (NA_LEVEL1_PHY);
}

static void
na_mii_reset(struct net_device *dev)
{
	struct netarmeth_dev *na_dev;

	NA_PRINTK("na_mii_reset\n");

	if (dev == NULL) {
		NA_PRINTK("na_mii_reset - NULL parameter\n");
		return;
	}
	na_dev = dev->priv;

	na_dev->phy_type = na_mii_identify_phy();
	if ((na_dev->phy_type != NA_ENABLE_PHY) &&
	    (na_dev->phy_type != NA_HOME_PHY)) {
		printk(KERN_WARNING "%s:  Found other than Enable, Lucent or Home PHY.\n", dev->name);
	}

	/* select appropriate Control register */
	if (na_dev->phy_type == NA_HOME_PHY)
		outl_t(0x100, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
	else
		outl_t(0x400, get_eth_reg_addr(NETARM_ETH_MII_ADDR));

	outl_t(0x8000, get_eth_reg_addr(NETARM_ETH_MII_WRITE));
	na_mii_poll_busy();

	outl_t(0, get_eth_reg_addr(NETARM_ETH_MII_WRITE));
	na_mii_poll_busy();
}

static unsigned long
na_mii_negotiate(struct netarmeth_dev *na_dev)
{
	unsigned long start_time, current_time;
	unsigned long read_data;

	NA_PRINTK("na_mii_negotiate\n");

	/* Select Autonegotiation Advertisement register */
	if (na_dev->phy_type == NA_HOME_PHY)
		outl_t(0x101, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
	else
		outl_t(0x404, get_eth_reg_addr(NETARM_ETH_MII_ADDR));

	/* Enable auto-negotiation */
	outl_t(0x01E1, get_eth_reg_addr(NETARM_ETH_MII_WRITE));
	na_mii_poll_busy();

	/* Select Control register */
	if (na_dev->phy_type == NA_HOME_PHY)
		outl_t(0x100, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
	else
		outl_t(0x400, get_eth_reg_addr(NETARM_ETH_MII_ADDR));

	/* Restart auto-negotiation */
	outl_t(0x1200, get_eth_reg_addr(NETARM_ETH_MII_WRITE));
	na_mii_poll_busy();

	// Wait for auto-negotiation to complete or for predefined delay to elapse.

	start_time = current_time = jiffies;
	do {
		/* Select Status register */
		if (na_dev->phy_type == NA_HOME_PHY)
			outl_t(0x101, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
		else
			outl_t(0x401, get_eth_reg_addr(NETARM_ETH_MII_ADDR));

		outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_MII_CMD))
		        | NETARM_ETH_MIIC_RSTAT),
		       get_eth_reg_addr(NETARM_ETH_MII_CMD));
		na_mii_poll_busy();

		read_data = inl_t(get_eth_reg_addr(NETARM_ETH_MII_READ));
		if ((read_data & 0x0024) == 0x0024) {
			return (0);
		}
		current_time = jiffies;
	} while (current_time < start_time + NA_MII_NEGOTIATE_DELAY);

	NA_PRINTK("  MII negotiation delay elapsed\n");
	return (1);
}

// na_mii_check_speed
//
// Determine the operating speed of the Ethernet interface.
//
// Return Values:
//   NA_10MBPS  (0):   10 Mbps
//   NA_100MBPS (1):  100 Mbps

static unsigned long
na_mii_check_speed(struct netarmeth_dev *na_dev)
{
	// Check link status.  If 0, default to 100 Mbps.
	NA_PRINTK("na_mii_check_speed\n");

	if (na_dev->phy_type == NA_HOME_PHY) {
		/* The AMD Home PHY only supports 10 Mbps anyway */
		NA_PRINTK("  returning NA_10MBPS (HomePHY)\n");
		na_dev->media_100BaseT = NA_10MBPS;
	} else {
		/* Read Status register */
		outl_t(0x401, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
		outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_MII_CMD)) | NETARM_ETH_MIIC_RSTAT),
		       get_eth_reg_addr(NETARM_ETH_MII_CMD));
		na_mii_poll_busy();

		if ((inl_t(get_eth_reg_addr(NETARM_ETH_MII_READ)) & 0x0004) == 0) {
			NA_PRINTK("  returning NA_100MBPS (link is down)\n");
			na_dev->media_100BaseT = NA_100MBPS;
		} else {
			/* Now it's OK to check the normal speed status */
			outl_t(0x417, get_eth_reg_addr(NETARM_ETH_MII_ADDR));
			outl_t((inl_t(get_eth_reg_addr(NETARM_ETH_MII_CMD)) | NETARM_ETH_MIIC_RSTAT),
			       get_eth_reg_addr(NETARM_ETH_MII_CMD));
			na_mii_poll_busy();

			if ((inl_t(get_eth_reg_addr(NETARM_ETH_MII_READ)) & 0x0200) != 0) {
				NA_PRINTK("  returning NA_100MBPS (b)\n");
				na_dev->media_100BaseT = NA_100MBPS;
			} else {
				NA_PRINTK("  returning NA_10MBPS\n");
				na_dev->media_100BaseT = NA_10MBPS;
			}
		}
	}
	return(na_dev->media_100BaseT);
}

/*--------------------------------------------------------------------*
	MULTICAST ADDRESS HASH TABLE FUNCTIONS
 *--------------------------------------------------------------------*/

static void
na_set_hash_bit(unsigned char *hash_table, int bit)
{
  int byte_index, bit_index;

  NA_PRINTK3("na_set_hash_bit\n");

  byte_index = bit >> 3;
  bit_index = bit & 7;
  hash_table[byte_index] |= (1 << bit_index);
}

// na_rotate
//
// Input arguments:
//   reg         value to rotate
//   direction   LEFT or RIGHT
//   bits        # of bits to rotate

static unsigned short
na_rotate (unsigned short reg, int direction, int bits)
{
  int index;

  NA_PRINTK3("na_rotate:  %X %s %d.\n",
    reg, ((direction == NA_LEFT) ? "LEFT" : "RIGHT"), bits);

  if (direction == NA_LEFT) {
    for (index = 0; index < bits; index++) {
      if (reg & 0x8000) {           // if high bit set
        reg = (reg << 1) | 1;       // shift left and or in 1
      } else {
        reg <<= 1;
      }
    }
  } else {                          // if ror
    for (index = 0; index < bits; index++) {
      if (reg & 1) {                // if low bit set
        reg = (reg >> 1) | 0x8000;  // shift right and or in hi bit
      } else {
        reg >>= 1;
      }
    }
  }
  NA_PRINTK3("  returning %X\n", reg);
  return (reg);
}

// na_calculate_hash_bit
//
// This routine calculates the bit to set in the CRC hash table
// to enable receipt of the specified multicast address.
//
// Return Value:  bit position to set in hash table

static int
na_calculate_hash_bit(unsigned char *mca)
{
  unsigned short  copy_mca[3];
  unsigned short *mcap, bp, bx;
  unsigned long   crc;
  int             result, mca_word, bit_index;

  NA_PRINTK3("na_calculate_hash_bit\n");

  memcpy (copy_mca, mca, sizeof(copy_mca));

  mcap = copy_mca;
  crc = 0xffffffffL;

  for (mca_word = 0; mca_word < 3; mca_word++) {
    bp = *mcap;                              // get word of address
    mcap++;

    for (bit_index = 0; bit_index < 16; bit_index++) {
      bx = (unsigned short) (crc >> 16);     // get high word of crc
      bx = na_rotate(bx, NA_LEFT, 1);        // bit 31 to lsb
      bx ^= bp;                              // combine with incoming
      crc <<= 1;                             // shift crc left 1 bit
      bx &= 1;                               // get control bit
      if (bx) {                              // if bit set
        crc ^= NA_POLYNOMIAL;                // xero crc with polynomial
      }
      crc |= bx;                             // or in control bit
      bp = na_rotate(bp, NA_RIGHT, 1);
    }
  }

  // CRC calculation done.  The 6-bit result resides in bits locations 28:23.

  result = (crc >> 23) & 0x3f;

  return (result);
}

// na_load_mca_hash_table
//
// This function generates the multicast address hash table from the
// multicast addresses contained in the provided linked list.  This function
// loads this table into the Ethernet controller.

static void
na_load_mca_hash_table(struct dev_mc_list *mc_list)
{
  unsigned short      hash_table[4];
  struct dev_mc_list *mc_list_entry;
  int                 bit_number;

  NA_PRINTK("na_load_mca_hash_table\n");

  // Generate the hash table.

  memset(hash_table, 0, sizeof(hash_table));

  for (mc_list_entry = mc_list;
       mc_list_entry != NULL;
       mc_list_entry = mc_list_entry->next)
  {
    NA_PRINTK("  multicast address:  %X %X %X %X %X %X\n",
      mc_list_entry->dmi_addr[0], mc_list_entry->dmi_addr[1],
      mc_list_entry->dmi_addr[2], mc_list_entry->dmi_addr[3],
      mc_list_entry->dmi_addr[4], mc_list_entry->dmi_addr[5]);

    bit_number = na_calculate_hash_bit(mc_list_entry->dmi_addr);
    na_set_hash_bit((unsigned char *) hash_table, bit_number);
  }
  NA_PRINTK("  hash table:  %X %X %X %X\n",
    hash_table[0], hash_table[1], hash_table[2], hash_table[3]);    

  // Load the hash table into the Ethernet controller.

  outl_t(hash_table[0], get_eth_reg_addr(NETARM_ETH_SAL_HASH_TBL_1));
  outl_t(hash_table[1], get_eth_reg_addr(NETARM_ETH_SAL_HASH_TBL_2));
  outl_t(hash_table[2], get_eth_reg_addr(NETARM_ETH_SAL_HASH_TBL_3));
  outl_t(hash_table[3], get_eth_reg_addr(NETARM_ETH_SAL_HASH_TBL_4));
}


/*--------------------------------------------------------------------*
	Interrupt Functions
 *--------------------------------------------------------------------*/

static void
netarmeth_rx_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
  struct net_device    *dev;
  struct netarmeth_dev *na_dev;
  unsigned long         status_reg, reg;
  struct sk_buff       *current_skb, *new_skb;
  int                   hw_index;

  NA_PRINTK2("netarmeth_rx_interrupt\n");

  dev = dev_id;
  if (dev == NULL) {
    printk(KERN_WARNING "Ethernet receive interrupt, null device structure.\n");

    status_reg = inl_t(get_dma_reg_addr(NETARM_DMA1A_STATUS));
    status_reg &= (NETARM_DMA_STAT_INTPEN_MASK | NETARM_DMA_STAT_INT_EN_MASK);
    // Acknowledge the pending interrupts.
    outl_t(status_reg, get_dma_reg_addr(NETARM_DMA1A_STATUS));
    return;
  }
  na_dev = dev->priv;
/*
  if (na_dev->rx_interrupt) {
    // Normally this can not happen, but I want to make sure.
    NA_PRINTK("rx irq recursion\n");
    return;
  }
*/
  na_dev->rx_interrupt = 1;

  // Determine which receive interrupts are pending.  Acknowledge the
  // pending interrupts by setting the interrupt pending bits in the
  // DMA Status Register.  If a Normal Completion or Buffer Not Ready
  // interrupt is pending process the received buffer(s) from the
  // receive DMA descriptor ring.

  status_reg = inl_t(get_dma_reg_addr(NETARM_DMA1A_STATUS));
  NA_PRINTK2("  DMA 1A status: %lX\n", status_reg);

  status_reg &= (NETARM_DMA_STAT_INTPEN_MASK | NETARM_DMA_STAT_INT_EN_MASK);
  // Acknowledge the pending interrupts.
  outl_t(status_reg, get_dma_reg_addr(NETARM_DMA1A_STATUS));

  if (status_reg & (NETARM_DMA_STAT_NC_INTPEN | NETARM_DMA_STAT_NR_INTPEN))
  {
    if (status_reg & NETARM_DMA_STAT_NR_INTPEN)
    {
	/* "F" bit in DMA buffer descriptor in incorrect state */
	NA_PRINTK("  *** NR interrupt ***\n");
    }

    // Process all the receive DMA buffer descriptors that have the
    // "full" bit set until the descriptor indicated by the hardware
    // DMA descriptor index is processed.

    reg = inl_t(get_dma_reg_addr(NETARM_DMA1A_CONTROL));
    hw_index = NETARM_DMA_CTL_INDEX(reg) >> 3; // Bytes / 8
    NA_PRINTK2("  hw_index: %lX, %d.\n", reg, hw_index);

    do
    {
      NA_PRINTK2("  sw_index: %d.\n", na_dev->rx_sw_index);

      if (na_dev->rx_dma_desc[na_dev->rx_sw_index].buf_len
	  & NETARM_DMA_BD1_FULL) {

        reg = na_dev->rx_dma_desc[na_dev->rx_sw_index].stat;
        NA_PRINTK2(" RX desc[%d] stat: %lX\n", na_dev->rx_sw_index, reg);

        if ((reg & NA_RXSTAT_ERR_MASK) || ((reg & NA_RXSTAT_RXOK) == 0))
	{
          // Receive Error.
          NA_PRINTK("  Receive error.\n");
          na_dev->stats.rx_errors++;

          // If an overrun has occurred, the receive FIFO must be reset.
          if (reg & NA_RXSTAT_ROVER)
	  {
            NA_PRINTK("  Reset receive FIFO.\n");
            na_dev->stats.rx_fifo_errors++;
            na_reset_rx_fifo(na_dev);
          }
          new_skb = na_dev->rx_skb[na_dev->rx_sw_index];
        }
	else if (reg & NA_RXSTAT_RXOK)
	{
          // Good Frame.
          NA_PRINTK2("  Good Frame.\n");

          // Save the current skb (receive buffer).
          current_skb = na_dev->rx_skb[na_dev->rx_sw_index];

          // Allocate a new skb to replace the one consumed by the current
          // frame.  If a new new skb cannot be allocated, ignore (discard)
          // this frame and reuse the original skb for a future frame.

          new_skb = dev_alloc_skb(NA_MAX_ETH_FRAME + 2);
          if (new_skb == NULL)
	  {
            NA_PRINTK("  dev_alloc_skb failure.\n");
            na_dev->stats.rx_dropped++;
            new_skb = na_dev->rx_skb[na_dev->rx_sw_index];
	  } 
	  else
	  {
	    // Pass the frame up.
	    na_dev->stats.rx_packets++;

	    reg = na_dev->rx_dma_desc[na_dev->rx_sw_index].buf_len
		& ~NETARM_DMA_BD1_FULL;
            NA_PRINTK3("  buf_len: %ld.\n", reg);
            NA_PRINTK3("  %X %X %X %X %X %X %X %X %X %X %X %X %X %X\n",
              current_skb->data[0], current_skb->data[1],
              current_skb->data[2], current_skb->data[3],
              current_skb->data[4], current_skb->data[5],
              current_skb->data[6], current_skb->data[7],
              current_skb->data[8], current_skb->data[9],
              current_skb->data[10], current_skb->data[11],
              current_skb->data[12], current_skb->data[13]);

            skb_put(current_skb, reg);
            current_skb->dev       = dev;
            current_skb->protocol  = eth_type_trans(current_skb, dev);
//            current_skb->ip_summed = CHECKSUM_UNNECESSARY;
	    skb_pull(current_skb, 2);

            netif_rx(current_skb);
	  }
        } 
	else
	{
          // Discard frame.
          NA_PRINTK("  RX unexpected receive status\n");
          na_dev->stats.rx_errors++;
          new_skb = na_dev->rx_skb[na_dev->rx_sw_index];
	}

        // Re-initialize the receive DMA descriptor.
        na_dev->rx_skb[na_dev->rx_sw_index] = new_skb;
        na_dev->rx_dma_desc[na_dev->rx_sw_index].src_buf_ptr =
          (unsigned long) virt_to_bus(new_skb->data);

        if (na_dev->rx_sw_index == NA_MAX_RX_BUF-1) {
          na_dev->rx_dma_desc[NA_MAX_RX_BUF-1].src_buf_ptr
	      |= NETARM_DMA_BD0_WRAP;
        }
        na_dev->rx_dma_desc[na_dev->rx_sw_index].buf_len  = NA_MAX_ETH_FRAME;
        na_dev->rx_dma_desc[na_dev->rx_sw_index].stat     = 0;
        na_dev->rx_dma_desc[na_dev->rx_sw_index].buf_len &= ~NETARM_DMA_BD1_FULL;

        // Clear possible Not Ready interrupt and reactivate the DMA channel.
	status_reg &= (NETARM_DMA_STAT_INTPEN_MASK
		       | NETARM_DMA_STAT_INT_EN_MASK);
	status_reg |= NETARM_DMA_STAT_NR_INTPEN;
	// Acknowledge the pending interrupts.
	outl_t(status_reg, get_dma_reg_addr(NETARM_DMA1A_STATUS));
      }
      else
      {
        // F bit is not set.
        NA_PRINTK("  RX skipping descriptor.\n");
      }

      // Increment the receive DMA descriptor index.
      na_dev->rx_sw_index++;
      if (na_dev->rx_sw_index >= NA_MAX_RX_BUF) {
        na_dev->rx_sw_index = 0;
      }
    } while (na_dev->rx_sw_index != hw_index);
  }
  else
  {
    NA_PRINTK("  RX Error Completion Interrupt\n");
    na_dev->stats.rx_errors++;
  }
  na_dev->rx_interrupt = 0;
}


static void
netarmeth_tx_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
  struct net_device    *dev;
  struct netarmeth_dev *na_dev;
  unsigned long         status_reg;

  NA_PRINTK2("netarmeth_tx_interrupt\n");

  dev = dev_id;
  if (dev == NULL) {
    printk(KERN_WARNING "Ethernet transmit interrupt, null device structure.\n");

    status_reg = inl_t(get_dma_reg_addr(NETARM_DMA2_STATUS));
    status_reg &= (NETARM_DMA_STAT_INTPEN_MASK | NETARM_DMA_STAT_INT_EN_MASK);
    // Acknowledge the pending interrupts.
    outl_t(status_reg, get_dma_reg_addr(NETARM_DMA2_STATUS));

    return;
  }

  na_dev = dev->priv;
/*
  if (na_dev->tx_interrupt) {
    // Normally this can not happen, but I want to make sure.
    NA_PRINTK("tx irq recursion\n");
    return;
  }
*/
  na_dev->tx_interrupt = 1;

  // Determine which receive interrupts are pending.  Acknowledge the pending
  // interrupts by setting the interrupt pending bits in the DMA Status
  // Register.

  status_reg = inl_t(get_dma_reg_addr(NETARM_DMA2_STATUS));
  NA_PRINTK2("  DMA 2 status: %lX\n", status_reg);

  status_reg &= (NETARM_DMA_STAT_INTPEN_MASK | NETARM_DMA_STAT_INT_EN_MASK);
  // Acknowledge the pending interrupts.
  outl_t(status_reg, get_dma_reg_addr(NETARM_DMA2_STATUS));

  if (status_reg & NETARM_DMA_STAT_NC_INTPEN) {
    na_dev->stats.tx_packets++;
  } else {
    na_dev->stats.tx_errors++;
  }
  if (na_dev->tx_skb) {
    dev_kfree_skb_irq(na_dev->tx_skb); /* was FREE_WRITE */
    /* fixed? calling dev_kfree_skb() from an interrupt caused a warning */
    na_dev->tx_skb = NULL;
  }
  if (! skb_queue_empty(&na_dev->tx_skb_queue)) {
    na_start_xmit(na_dev);  
  }
  else {
    na_dev->tbusy = 0;
  }
  // mark_bh(NET_BH); not longer available, instead using
  netif_wake_queue(dev);
  na_dev->tx_interrupt = 0;
}


/*--------------------------------------------------------------------*
	Hardware Initialization Functions
 *--------------------------------------------------------------------*/

static void
na_reset_dma(void)
{
  NA_PRINTK("na_reset_dma\n");

  /* Disable each channel, clear buffer pointer and interrupt bits */
  outl_t((inl_t(get_dma_reg_addr(NETARM_DMA1A_CONTROL)) &
	  ~NETARM_DMA_CTL_ENABLE), get_dma_reg_addr(NETARM_DMA1A_CONTROL));
  outl_t(0, get_dma_reg_addr(NETARM_DMA1A_BFR_DESCRPTOR_PTR));
  outl_t(0, get_dma_reg_addr(NETARM_DMA1A_STATUS));

  outl_t((inl_t(get_dma_reg_addr(NETARM_DMA1B_CONTROL)) &
	  ~NETARM_DMA_CTL_ENABLE), get_dma_reg_addr(NETARM_DMA1B_CONTROL));
  outl_t(0, get_dma_reg_addr(NETARM_DMA1B_BFR_DESCRPTOR_PTR));
  outl_t(0, get_dma_reg_addr(NETARM_DMA1B_STATUS));

  outl_t((inl_t(get_dma_reg_addr(NETARM_DMA1C_CONTROL)) &
	  ~NETARM_DMA_CTL_ENABLE), get_dma_reg_addr(NETARM_DMA1C_CONTROL));
  outl_t(0, get_dma_reg_addr(NETARM_DMA1C_BFR_DESCRPTOR_PTR));
  outl_t(0, get_dma_reg_addr(NETARM_DMA1C_STATUS));

  outl_t((inl_t(get_dma_reg_addr(NETARM_DMA1D_CONTROL)) &
	  ~NETARM_DMA_CTL_ENABLE), get_dma_reg_addr(NETARM_DMA1D_CONTROL));
  outl_t(0, get_dma_reg_addr(NETARM_DMA1D_BFR_DESCRPTOR_PTR));
  outl_t(0, get_dma_reg_addr(NETARM_DMA1D_STATUS));

  outl_t((inl_t(get_dma_reg_addr(NETARM_DMA2_CONTROL)) &
	  ~NETARM_DMA_CTL_ENABLE), get_dma_reg_addr(NETARM_DMA2_CONTROL));
  outl_t(0, get_dma_reg_addr(NETARM_DMA2_BFR_DESCRPTOR_PTR));
  outl_t(0, get_dma_reg_addr(NETARM_DMA2_STATUS));
}

// na_enable_dma

static void
na_enable_dma(struct net_device *dev)
{
  struct netarmeth_dev *na_dev;
  unsigned long         reg;

  NA_PRINTK("na_enable_dma\n");

  na_dev = dev->priv;  

  // Configure the DMA channels for transmit and receive.

  outl_t((unsigned long) virt_to_bus(na_dev->rx_dma_desc),
	 get_dma_reg_addr(NETARM_DMA1A_BFR_DESCRPTOR_PTR));
  outl_t((NETARM_DMA_STAT_NC_INT_EN | NETARM_DMA_STAT_EC_INT_EN
	  | NETARM_DMA_STAT_NR_INT_EN | NETARM_DMA_STAT_CA_INT_EN),
	 get_dma_reg_addr(NETARM_DMA1A_STATUS));

  outl_t((unsigned long) virt_to_bus(&na_dev->tx_dma_desc),
	 get_dma_reg_addr(NETARM_DMA2_BFR_DESCRPTOR_PTR));
  outl_t((NETARM_DMA_STAT_NC_INT_EN | NETARM_DMA_STAT_EC_INT_EN),
	 get_dma_reg_addr(NETARM_DMA2_STATUS));

  // Turn on each channel.

  reg = NETARM_DMA_CTL_ENABLE | NETARM_DMA_CTL_BUS_100_PERCENT |
        NETARM_DMA_CTL_MODE_FB_TO_MEM | NETARM_DMA_CTL_BURST_16_BYTE;
  outl_t(reg, get_dma_reg_addr(NETARM_DMA1A_CONTROL));

  if (na_dev->media_100BaseT) {
    reg = NETARM_DMA_CTL_ENABLE | NETARM_DMA_CTL_BUS_100_PERCENT |
          NETARM_DMA_CTL_MODE_FB_FROM_MEM | NETARM_DMA_CTL_BURST_16_BYTE;
  } else {
    reg = NETARM_DMA_CTL_ENABLE | NETARM_DMA_CTL_BUS_100_PERCENT |
	  NETARM_DMA_CTL_MODE_FB_FROM_MEM;
  }
  outl_t(reg, get_dma_reg_addr(NETARM_DMA2_CONTROL));
}


// This function assumes the current Ethernet controller configuration uses
// the ENABLE PHY through the MII interface.  The original vxWorks NET+ARM
// Ethernet driver, upon which this driver initialization code is modeled,
// also only supports the MII PHY but describes the initialization of the
// ENDEC PHY.  (See eth_chip_reset() in NccEthCtrl.c from the NET+ARM vxWorks
// source.)

static void
na_reset_eth(struct net_device *dev)
{
  struct netarmeth_dev *na_dev;

  NA_PRINTK("na_reset_eth\n");

  na_dev = dev->priv;

  na_mii_reset(dev);

  // Since this driver is designed for the NET+ARM-40, assume the NET+ARM clock
  // is set to 25Mhz.

  outl_t((NETARM_ETH_PCSC_CLKS_25M | NETARM_ETH_PCSC_ENJAB |
	  NETARM_ETH_PCSC_NOCFR), get_eth_reg_addr(NETARM_ETH_PCS_CFG));

  na_mii_negotiate(na_dev);
  na_mii_check_speed(na_dev);

  // Delay after resetting the chip before attempting to read/clear the
  // statistics counters.

  udelay(1000); // Delay 1 millisecond.  (Maybe this should be 1 second.)

  // Turn receive on.
  // Enable statistics register autozero on read.
  // Do not insert MAC address on transmit.
  // Do not enable special test modes.

  outl_t((NETARM_ETH_STLC_AUTOZ | NETARM_ETH_STLC_RXEN),
	 get_eth_reg_addr(NETARM_ETH_STL_CFG));

  // Read MIB statistics counters to zero them.

  inl_t(get_eth_reg_addr(NETARM_ETH_MIB_LCC));
  inl_t(get_eth_reg_addr(NETARM_ETH_MIB_EDC));
  inl_t(get_eth_reg_addr(NETARM_ETH_MIB_MCC));
  inl_t(get_eth_reg_addr(NETARM_ETH_MIB_CRCEC));
  inl_t(get_eth_reg_addr(NETARM_ETH_MIB_AEC));
  inl_t(get_eth_reg_addr(NETARM_ETH_MIB_CEC));
  inl_t(get_eth_reg_addr(NETARM_ETH_MIB_LFC));
  inl_t(get_eth_reg_addr(NETARM_ETH_MIB_SFC));
}

// na_enable_eth

static void
na_enable_eth(struct net_device *dev)
{
  unsigned long reg;

  NA_PRINTK("na_enable_eth\n");

  // Set the inter-packet gap delay to 0.96us for MII.
  // The NET+ARM H/W Reference Guide indicates that the Back-to-back IPG
  // Gap Timer Register should be set to 0x15 and the Non Back-to-back IPG
  // Gap Timer Register should be set to 0x00000C12 for the MII PHY.

  outl_t(0x15, get_eth_reg_addr(NETARM_ETH_B2B_IPG_GAP_TMR));
  outl_t(0x00000C12, get_eth_reg_addr(NETARM_ETH_NB2B_IPG_GAP_TMR));

  // Add CRC to end of packets.
  // Pad packets to minimum length of 64 bytes.
  // Allow unlimited length transmit packets.
  // Receive all broadcast packets.
  //
  // NOTE:  Multicast addressing is NOT enabled here currently.

  outl_t((NETARM_ETH_MACC_CRCEN | NETARM_ETH_MACC_PADEN
	  | NETARM_ETH_MACC_HUGEN), get_eth_reg_addr(NETARM_ETH_MAC_CFG));
  outl_t(NETARM_ETH_SALF_BROAD, get_eth_reg_addr(NETARM_ETH_SAL_FILTER));
  
  // JSC - Determine how ifconfig passes in a special MAC address.
  // Set it here.
  na_get_mac_addr(dev); // JSC !!!

  NA_PRINTK("  MCA Hash Table: %lX %lX %lX %lX\n",
	    inl_t(get_eth_reg_addr(NETARM_ETH_SAL_HASH_TBL_1)),
	    inl_t(get_eth_reg_addr(NETARM_ETH_SAL_HASH_TBL_2)),
	    inl_t(get_eth_reg_addr(NETARM_ETH_SAL_HASH_TBL_3)),
	    inl_t(get_eth_reg_addr(NETARM_ETH_SAL_HASH_TBL_4)));

  // Clear any outstanding Ethernet DMA interrupts.

  // Receive
  outl_t(NETARM_DMA_STAT_INTPEN_MASK, get_dma_reg_addr(NETARM_DMA1A_STATUS));
  // Transmit
  outl_t(NETARM_DMA_STAT_INTPEN_MASK, get_dma_reg_addr(NETARM_DMA2_STATUS));

  // Configure the Ethernet DMA channels.

  na_enable_dma(dev);

  // Associate the Ethernet DMA IRQs with the appropriate interrupt service
  // routines.  No need to call request_dma.  The DMA controller assignments
  // are hardwired.

  if (request_irq(IRQ_DMA01, netarmeth_rx_interrupt, 0, cardname, dev)) {
    printk(KERN_WARNING "%s:  request_irq(IRQ_DMA01) failure.\n", dev->name);
  }
  if (request_irq(IRQ_DMA02, netarmeth_tx_interrupt, 0, cardname, dev)) {
    printk(KERN_WARNING "%s:  request_irq(IRQ_DMA02) failure.\n", dev->name);
  }

  // Autozero statistics registers after reading.
  // Enable packet receiver.
  // (This was already set in na_reset_eth.)

  outl_t((NETARM_ETH_STLC_AUTOZ | NETARM_ETH_STLC_RXEN),
	 get_eth_reg_addr(NETARM_ETH_STL_CFG));

  // Enable receive FIFO.  Enable receive DMA.
  // Enable transmit FIFO. Enable transmit DMA.

  reg = NETARM_ETH_GCR_ERX | NETARM_ETH_GCR_ERXDMA | NETARM_ETH_GCR_ETX
      | NETARM_ETH_GCR_ETXDMA | NETARM_ETH_GCR_PNA;
  outl_t(reg, get_eth_reg_addr(NETARM_ETH_GEN_CTRL));
}


/*--------------------------------------------------------------------*
	Exported Functions
 *--------------------------------------------------------------------*/

// netarmeth_open
//
// This function is typically called by ifconfig during system startup.
// The Ethernet and DMA controllers are initialized and enabled here.

static int
netarmeth_open(struct net_device *dev)
{
  struct netarmeth_dev *na_dev;

  NA_PRINTK("netarmeth_open\n");

  na_dev = dev->priv;

  // Disable interrupts from the Ethernet DMA channels.

  na_disable_eth_irqs();

  // Reset the Ethernet DMA channels.

  na_reset_dma();

  // Reset the Ethernet controller, and determine the line speed.

  na_reset_eth(dev);

  // Enable the Ethernet controller and the DMA channels.
  // na_enable_eth calls na_enable_dma.

  na_enable_eth(dev);

  // Enable Ethernet and DMA interrupts.

  na_dev->tbusy = 0;
  na_dev->tx_interrupt = 0;
  na_dev->rx_interrupt = 0;
  na_enable_eth_irqs();

  // Enable timer
  na_enable_timer(dev);

  na_dev->start = 1;

  return (0);
}


static int
netarmeth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
  struct netarmeth_dev *na_dev;

  NA_PRINTK2("netarmeth_start_xmit\n");

  na_dev = dev->priv;

  // Put skb on the queue
  skb_queue_tail(&na_dev->tx_skb_queue, skb);
  
  if (test_and_set_bit(0, (void *) &na_dev->tbusy) == 0) {
    NA_PRINTK2("%s:  Transmit idle, sending packet.\n", dev->name);
    na_start_xmit(na_dev);
  }
  return (0);
}


static int
netarmeth_close(struct net_device *dev)
{
  struct netarmeth_dev *na_dev;
  struct sk_buff *skb;

  NA_PRINTK("netarmeth_close\n");

  na_dev = dev->priv;
  na_dev->tbusy = 1;
  na_dev->start = 0;

  // Disable interrupts from the Ethernet DMA channels.

  na_disable_eth_irqs();

  // Reset the Ethernet DMA channels.

  na_reset_dma();

  // Clear any outstanding Ethernet DMA interrupts.

  // Receive
  outl_t(NETARM_DMA_STAT_INTPEN_MASK, get_dma_reg_addr(NETARM_DMA1A_STATUS));
  // Transmit
  outl_t(NETARM_DMA_STAT_INTPEN_MASK, get_dma_reg_addr(NETARM_DMA2_STATUS));

  // Release the Ethernet DMA IRQs.

  free_irq(IRQ_DMA01, dev);
  free_irq(IRQ_DMA02, dev);

  // Disable the timer.

  na_disable_timer(dev);

  // Free any buffers left in the hardware transmit queue

  while ((skb = skb_dequeue(&na_dev->tx_skb_queue))) {
    dev_kfree_skb(skb);  /* was FREE_WRITE */
  }
  return (0);
}


static struct net_device_stats *
netarmeth_get_stats(struct net_device *dev)
{
  struct netarmeth_dev *na_dev;
  unsigned long         flags;

  NA_PRINTK("netarmeth_get_stats\n");

  na_dev = dev->priv;

  save_flags_cli(flags);

  // CRC Errors
  na_dev->stats.rx_crc_errors += inl_t(get_eth_reg_addr(NETARM_ETH_MIB_CRCEC));
  // Alignment Errors
  na_dev->stats.rx_frame_errors += inl_t(get_eth_reg_addr(NETARM_ETH_MIB_AEC));
  // Long Frames
  na_dev->stats.rx_length_errors
      += inl_t(get_eth_reg_addr(NETARM_ETH_MIB_LFC));
  // Short Frames
  na_dev->stats.rx_length_errors
      += inl_t(get_eth_reg_addr(NETARM_ETH_MIB_SFC));
  // Late Collisions
  na_dev->stats.tx_aborted_errors
      += inl_t(get_eth_reg_addr(NETARM_ETH_MIB_LCC));
  // Excessive Deferrals
  na_dev->stats.tx_aborted_errors
      += inl_t(get_eth_reg_addr(NETARM_ETH_MIB_EDC));
  // Maximum Collisions
  na_dev->stats.tx_aborted_errors
      += inl_t(get_eth_reg_addr(NETARM_ETH_MIB_MCC));

  restore_flags(flags);
  return (&na_dev->stats);
}

// netarmeth_set_multicast_list
//
// Always accept all broadcast frames.

static void
netarmeth_set_multicast_list(struct net_device *dev)
{
  NA_PRINTK("netarmeth_set_multicast_list\n");

  if (dev->flags & IFF_PROMISC)
  {
    // Enable promiscuous mode.
    NA_PRINTK("  Enable promiscuous mode.\n");
    outl_t(NETARM_ETH_SALF_PRO, get_eth_reg_addr(NETARM_ETH_SAL_FILTER));
  }
  else if ((dev->flags & IFF_ALLMULTI) || dev->mc_count > NA_MAX_MCA)
  {
    // Accept all multicast packets.
    NA_PRINTK("  Accept all multicast packets.\n");
    outl_t((NETARM_ETH_SALF_PRM | NETARM_ETH_SALF_BROAD),
	   get_eth_reg_addr(NETARM_ETH_SAL_FILTER));
  }
  else if (dev->mc_count)
  {
    // Accept multicast packets using the hash table.
    NA_PRINTK("  Accept multicast packets.  Load hash table.\n");
    na_load_mca_hash_table(dev->mc_list);
    outl_t((NETARM_ETH_SALF_PRA | NETARM_ETH_SALF_BROAD),
	   get_eth_reg_addr(NETARM_ETH_SAL_FILTER));
  }
  else
  {
    // Accept frames addressed to this station.
    NA_PRINTK("  Accept frames addressed to this station.\n");
    outl_t(NETARM_ETH_SALF_BROAD, get_eth_reg_addr(NETARM_ETH_SAL_FILTER));
  }
  NA_PRINTK("  returning.\n");
}

// netarmeth_timeout
//
// copied from the am79c961 driver

static void netarmeth_timeout(struct net_device *dev)
{
	NA_PRINTK("%s: transmit timed out, network cable problem?\n", dev->name);

	/* FIXME: shouldn't we shutdown and restart the transmitter? */

	netif_wake_queue(dev);
}


// netarmeth_init
//
// The NET+ARM Ethernet controller is an integral part of the NET+ARM chip.
// The CSRs are located at fixed addresses.  The Ethernet controller always
// uses DMA channels 1 and 2.  Therefore, this probe routine does not need
// to locate the Ethernet controller.  This routine simply initializes the
// device structure and the DMA buffer descriptors and registers the interrupt
// service routines with LINUX.

static int __init
netarmeth_init(void)
{
  struct net_device    *dev;
  struct netarmeth_dev *na_dev;
  int                   i, j;

  
  NA_PRINTK("netarmeth_init() entered\n");

  // Initialize the Device Structure.

  // Allocate a device structure.  Device structures are allocated
  // in Space.c for "built in" drivers.  Do not allocate the private data
  // area at this time. (why not? --rp)

  dev  = init_etherdev( NULL, sizeof(struct netarmeth_dev));
  if (dev == NULL) {
      return (-ENOMEM);
  }
  NA_PRINTK("  dev: %X, dev->name: %s\n", (unsigned int) dev, dev->name);

  SET_MODULE_OWNER(dev);

  // Initialize the private data area.

  na_dev = dev->priv;
  memset(na_dev, 0, sizeof(struct netarmeth_dev));

  // ether_setup initializes the device structure with standard
  // Ethernet values.

  ether_setup(dev);

  // No need to call request_region.  The Ethernet (and DMA) controller CSR
  // addresses are hardwired.

  dev->base_addr = NETARM_ETH_MODULE_BASE;

  // Get the MAC address from the Ethernet controller.

  na_get_mac_addr(dev); // this sets dev->dev_addr

  // Specify the exported driver functions.

  dev->open               = netarmeth_open;
  dev->stop               = netarmeth_close;
  dev->hard_start_xmit    = netarmeth_start_xmit;
  dev->get_stats          = netarmeth_get_stats;
  dev->set_multicast_list = netarmeth_set_multicast_list;
  dev->tx_timeout         = netarmeth_timeout;

  // Initialize the transmit DMA buffer descriptor.

  NA_PRINTK2("  tx_dma_desc: %X\n", (unsigned int)&na_dev->tx_dma_desc);

  na_dev->tx_dma_desc.src_buf_ptr = 0;
  na_dev->tx_dma_desc.buf_len = 0;
  na_dev->tx_dma_desc.stat = 0;

  skb_queue_head_init(&na_dev->tx_skb_queue);

  // Initialize the receive DMA buffer descriptors.  Allocate an skb
  // (receive buffer) for each descriptor.

  for (i = 0; i < NA_MAX_RX_BUF; i++) {
    na_dev->rx_skb[i] = dev_alloc_skb(NA_MAX_ETH_FRAME + 2);
    if (na_dev->rx_skb[i] == NULL) {
      for (j = 0; j < i; j++) {
        dev_kfree_skb(na_dev->rx_skb[j]); /* was FREE_READ */
        na_dev->rx_skb[j] = NULL;
      }
      return (-ENOMEM);
    }    
  }

  NA_PRINTK2("  rx_dma_desc[0]: %X\n", (unsigned int)&na_dev->rx_dma_desc[0]);

  for (i = 0; i < NA_MAX_RX_BUF; i++) {
    na_dev->rx_dma_desc[i].src_buf_ptr = 
      (unsigned long) virt_to_bus(na_dev->rx_skb[i]->data);
    na_dev->rx_dma_desc[i].buf_len     = NA_MAX_ETH_FRAME + 2;
    na_dev->rx_dma_desc[i].stat        = 0;
    // Clear the F bit.
    na_dev->rx_dma_desc[i].buf_len     &= ~NETARM_DMA_BD1_FULL;
  }
  na_dev->rx_dma_desc[NA_MAX_RX_BUF-1].src_buf_ptr |= NETARM_DMA_BD0_WRAP;
  na_dev->rx_sw_index = 0;

  NA_PRINTK("netarmeth_init: returning 0\n");
  return (0);
}

module_init(netarmeth_init);
MODULE_DESCRIPTION("NET+ARM ethernet driver");
MODULE_AUTHOR("Rolf Peukert <peukert@imms.de>");

