/*
 * adapter.c
 *
 * Xilinx Ethernet Adapter component to interface XEmac component to Linux
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under the terms
 * of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

/*
 * This driver is a bit unusual in that it is composed of two logical
 * parts where one part is the OS independent code and the other part is
 * the OS dependent code.  Xilinx provides their drivers split in this
 * fashion.  This file represents the Linux OS dependent part known as
 * the Linux adapter.  The other files in this directory are the OS
 * independent files as provided by Xilinx with no changes made to them.
 * The names exported by those files begin with XEmac_.  All functions
 * in this file that are called by Linux have names that begin with
 * xenet_.  The functions in this file that have Handler in their name
 * are registered as callbacks with the underlying Xilinx OS independent
 * layer.  Any other functions are static helper functions.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mii.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <xbasic_types.h>
#include "xemac.h"
#include "xemac_i.h"

MODULE_AUTHOR("MontaVista Software, Inc. <source@mvista.com>");
MODULE_DESCRIPTION("Xilinx Ethernet Media Access Controller driver");
MODULE_LICENSE("GPL");

/* FIXME hardcoded MAC address */
static unsigned char MAC_ADDR[6]={0x00, 0x00, 0xC0, 0xA3, 0xE5, 0x44};

#define TX_TIMEOUT (60*HZ)	/* Transmission timeout is 60 seconds. */

/*
 * Our private per device data.  When a net_device is allocated we will
 * ask for enough extra space for this.
 */
struct net_local {
	struct net_device_stats stats;	/* Statistics for this device */
	struct net_device *next_dev;	/* The next device in dev_list */
	struct timer_list phy_timer;	/* PHY monitoring timer */
	u32 index;		/* Which interface is this */
	u32 save_BaseAddress;	/* Saved physical base address */
	XInterruptHandler Isr;	/* Pointer to the XEmac ISR routine */
	struct sk_buff *saved_skb;	/* skb being transmitted */
	spinlock_t skb_lock;	/* For atomic access to saved_skb */
	u8 mii_addr;		/* The MII address of the PHY */
	/*
	 * The underlying OS independent code needs space as well.  A
	 * pointer to the following XEmac structure will be passed to
	 * any XEmac_ function that requires it.  However, we treat the
	 * data as an opaque object in this file (meaning that we never
	 * reference any of the fields inside of the structure).
	 */
	XEmac Emac;
};

/* List of devices we're handling and a lock to give us atomic access. */
static struct net_device *dev_list = NULL;
static spinlock_t dev_lock = SPIN_LOCK_UNLOCKED;

/* SAATODO: This function will be moved into the Xilinx code. */
/*****************************************************************************/
/**
*
* Lookup the device configuration based on the emac instance.  The table
* EmacConfigTable contains the configuration info for each device in the system.
*
* @param Instance is the index of the emac being looked up.
*
* @return
*
* A pointer to the configuration table entry corresponding to the given
* device ID, or NULL if no match is found.
*
* @note
*
* None.
*
******************************************************************************/
XEmac_Config *XEmac_GetConfig(int Instance)
{
	if (Instance < 0 || Instance >= XPAR_XEMAC_NUM_INSTANCES)
	{
		return NULL;
	}

	return &XEmac_ConfigTable[Instance];
}

/*
 * The following are notes regarding the critical sections in this
 * driver and how they are protected.
 *
 * saved_skb
 * The pointer to the skb that is currently being transmitted is
 * protected by a spinlock.  It is conceivable that the transmit timeout
 * could fire in the middle of a transmit completion.  The spinlock
 * protects the critical section of getting the pointer and NULLing it
 * out.
 *
 * dev_list
 * There is a spinlock protecting the device list.  It isn't really
 * necessary yet because the list is only manipulated at init and
 * cleanup, but it's there because it is basically free and if we start
 * doing hot add and removal of ethernet devices when the FPGA is
 * reprogrammed while the system is up, we'll need to protect the list.
 *
 * XEmac_Start, XEmac_Stop and XEmac_SetOptions are not thread safe.
 * These functions are called from xenet_open(), xenet_close(), reset(),
 * and xenet_set_multicast_list().  xenet_open() and xenet_close()
 * should be safe because when they do start and stop, they don't have
 * interrupts or timers enabled.  The other side is that they won't be
 * called while a timer or interrupt is being handled.  Next covered is
 * the interaction between reset() and xenet_set_multicast_list().  This
 * is taken care of by disabling the ethernet IRQ and bottom half when
 * the multicast list is being set.  The inverse case is covered because
 * a call to xenet_set_multicast_list() will not happen in the middle of
 * a timer or interrupt being serviced.  Finally, the interaction
 * between reset() being called from various places needs to be
 * considered.  reset() is called from poll_mii() and xenet_tx_timeout()
 * which are timer bottom halves as well as FifoRecvHandler() and
 * ErrorHandler() which are interrupt handlers.  The timer bottom halves
 * won't interrupt an interrupt handler (or each other) so that is
 * covered.  The interrupt handlers won't interrupt each other either.
 * That leaves the case of interrupt handlers interrupting timer bottom
 * halves.  This is handled simply by disabling the interrupt when the
 * bottom half calls reset().
 *
 * XEmac_PhyRead and XEmac_PhyWrite are not thread safe.
 * These functions are called from get_phy_status(), xenet_ioctl() and
 * probe().  probe() is only called from xenet_init() so it is not an
 * issue (nothing is really up and running yet).  get_phy_status() is
 * called from both poll_mii() (a timer bottom half) and xenet_open().
 * These shouldn't interfere with each other because xenet_open() is
 * what starts the poll_mii() timer.  xenet_open() and xenet_ioctl()
 * should be safe as well because they will be sequential.  That leaves
 * the interaction between poll_mii() and xenet_ioctl().  While the
 * timer bottom half is executing, a new ioctl won't come in so that is
 * taken care of.  That leaves the one case of the poll_mii timer
 * popping while handling an ioctl.  To take care of that case, the
 * timer is deleted when the ioctl comes in and then added back in after
 * the ioctl is finished.
 */

/*
 * Helper function to reset the underlying hardware.  This is called
 * when we get into such deep trouble that we don't know how to handle
 * otherwise.
 */
typedef enum DUPLEX { UNKNOWN_DUPLEX, HALF_DUPLEX, FULL_DUPLEX } DUPLEX;
static void
reset(struct net_device *dev, DUPLEX duplex)
{
	struct net_local *lp = (struct net_local *) dev->priv;
	struct sk_buff *tskb;
	u32 Options;
	u8 IfgPart1;
	u8 IfgPart2;
	u8 SendThreshold;
	u32 SendWaitBound;
	u8 RecvThreshold;
	u32 RecvWaitBound;
	unsigned int flags;

	/* Shouldn't really be necessary, but shouldn't hurt. */
	netif_stop_queue(dev);

	/*
	 * XEmac_Reset puts the device back to the default state.  We need
	 * to save all the settings we don't already know, reset, restore
	 * the settings, and then restart the emac.
	 */
	XEmac_GetInterframeGap(&lp->Emac, &IfgPart1, &IfgPart2);
	Options = XEmac_GetOptions(&lp->Emac);
	switch (duplex) {
	case HALF_DUPLEX:
		Options &= ~XEM_FDUPLEX_OPTION;
		break;
	case FULL_DUPLEX:
		Options |= XEM_FDUPLEX_OPTION;
		break;
	case UNKNOWN_DUPLEX:
		break;
	}

	if (XEmac_mIsSgDma(&lp->Emac)) {
		/*
		 * The following four functions will return an error if we are
		 * not doing scatter-gather DMA.  We just checked that so we
		 * can safely ignore the return values.  We cast them to void
		 * to make that explicit.
		 */
		(void) XEmac_GetPktThreshold(&lp->Emac, XEM_SEND,
					     &SendThreshold);
		(void) XEmac_GetPktWaitBound(&lp->Emac, XEM_SEND,
					     &SendWaitBound);
		(void) XEmac_GetPktThreshold(&lp->Emac, XEM_RECV,
					     &RecvThreshold);
		(void) XEmac_GetPktWaitBound(&lp->Emac, XEM_RECV,
					     &RecvWaitBound);
	}

	XEmac_Reset(&lp->Emac);

	/*
	 * The following three functions will return an error if the
	 * EMAC is already started.  We just stopped it by calling
	 * XEmac_Reset() so we can safely ignore the return values.
	 * We cast them to void to make that explicit.
	 */
	(void) XEmac_SetMacAddress(&lp->Emac, dev->dev_addr);
	(void) XEmac_SetInterframeGap(&lp->Emac, IfgPart1, IfgPart2);
	(void) XEmac_SetOptions(&lp->Emac, Options);
	if (XEmac_mIsSgDma(&lp->Emac)) {
		/*
		 * The following four functions will return an error if
		 * we are not doing scatter-gather DMA or if the EMAC is
		 * already started.  We just checked that we are indeed
		 * doing scatter-gather and we just stopped the EMAC so
		 * we can safely ignore the return values.  We cast them
		 * to void to make that explicit.
		 */
		(void) XEmac_SetPktThreshold(&lp->Emac, XEM_SEND,
					     SendThreshold);
		(void) XEmac_SetPktWaitBound(&lp->Emac, XEM_SEND,
					     SendWaitBound);
		(void) XEmac_SetPktThreshold(&lp->Emac, XEM_RECV,
					     RecvThreshold);
		(void) XEmac_SetPktWaitBound(&lp->Emac, XEM_RECV,
					     RecvWaitBound);
	}

	/*
	 * XEmac_Start returns an error when: it is already started, the send
	 * and receive handlers are not set, or a scatter-gather DMA list is
	 * missing.  None of these can happen at this point, so we cast the
	 * return to void to make that explicit.
	 */
	(void) XEmac_Start(&lp->Emac);

	/* Make sure that the send handler and we don't both free the skb. */
	spin_lock_irqsave(&lp->skb_lock,flags);
	tskb = lp->saved_skb;
	lp->saved_skb = NULL;
	spin_unlock_irqrestore(&lp->skb_lock,flags);
	if (tskb)
		dev_kfree_skb(tskb);

	/* We're all ready to go.  Start the queue in case it was stopped. */
	netif_wake_queue(dev);
}

static int
get_phy_status(struct net_device *dev, DUPLEX * duplex, int *linkup)
{
	struct net_local *lp = (struct net_local *) dev->priv;
	u16 reg;
	XStatus xs;

	xs = XEmac_PhyRead(&lp->Emac, lp->mii_addr, MII_BMCR, &reg);
	if (xs != XST_SUCCESS) {
		printk(KERN_ERR
		       "%s: Could not read PHY control register; error %d\n",
		       dev->name, xs);
		return -1;
	}

	if (!(reg & BMCR_ANENABLE)) {
		/*
		 * Auto-negotiation is disabled so the full duplex bit in
		 * the control register tells us if the PHY is running
		 * half or full duplex.
		 */
		*duplex = (reg & BMCR_FULLDPLX) ? FULL_DUPLEX : HALF_DUPLEX;
	} else {
		/*
		 * Auto-negotiation is enabled.  Figure out what was
		 * negotiated by looking for the best mode in the union
		 * of what we and our partner advertise.
		 */
		u16 advertise, partner, negotiated;

		xs = XEmac_PhyRead(&lp->Emac, lp->mii_addr,
				   MII_ADVERTISE, &advertise);
		if (xs != XST_SUCCESS) {
			printk(KERN_ERR
			       "%s: Could not read PHY advertisement; error %d\n",
			       dev->name, xs);
			return -1;
		}
		xs = XEmac_PhyRead(&lp->Emac, lp->mii_addr, MII_LPA, &partner);
		if (xs != XST_SUCCESS) {
			printk(KERN_ERR
			       "%s: Could not read PHY LPA; error %d\n",
			       dev->name, xs);
			return -1;
		}

		negotiated = advertise & partner & ADVERTISE_ALL;
		if (negotiated & ADVERTISE_100FULL)
			*duplex = FULL_DUPLEX;
		else if (negotiated & ADVERTISE_100HALF)
			*duplex = HALF_DUPLEX;
		else if (negotiated & ADVERTISE_10FULL)
			*duplex = FULL_DUPLEX;
		else
			*duplex = HALF_DUPLEX;
	}

	xs = XEmac_PhyRead(&lp->Emac, lp->mii_addr, MII_BMSR, &reg);
	if (xs != XST_SUCCESS) {
		printk(KERN_ERR
		       "%s: Could not read PHY status register; error %d\n",
		       dev->name, xs);
		return -1;
	}

	*linkup = (reg & BMSR_LSTATUS) != 0;

	return 0;
}

/*
 * This routine is used for two purposes.  The first is to keep the
 * EMAC's duplex setting in sync with the PHY's.  The second is to keep
 * the system apprised of the state of the link.  Note that this driver
 * does not configure the PHY.  Either the PHY should be configured for
 * auto-negotiation or it should be handled by something like mii-tool.
 */
static void
poll_mii(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	struct net_local *lp = (struct net_local *) dev->priv;
	u32 Options;
	DUPLEX phy_duplex, mac_duplex;
	int phy_carrier, netif_carrier;

	/* First, find out what's going on with the PHY. */
	if (get_phy_status(dev, &phy_duplex, &phy_carrier)) {
		printk(KERN_ERR "%s: Terminating link monitoring.\n",
		       dev->name);
		return;
	}

	/* Second, figure out if we have the EMAC in half or full duplex. */
	Options = XEmac_GetOptions(&lp->Emac);
	mac_duplex = (Options & XEM_FDUPLEX_OPTION) ? FULL_DUPLEX : HALF_DUPLEX;

	/* Now see if there is a mismatch. */
	if (mac_duplex != phy_duplex) {
		/*
		 * Make sure that no interrupts come in that could cause
		 * reentrancy problems in reset.
		 */
		disable_irq(dev->irq);
		reset(dev, phy_duplex);
		enable_irq(dev->irq);
	}

	netif_carrier = netif_carrier_ok(dev) != 0;

	if (phy_carrier != netif_carrier) {
		if (phy_carrier) {
			printk(KERN_INFO "%s: Link carrier restored.\n",
			       dev->name);
			netif_carrier_on(dev);
		} else {
			printk(KERN_INFO "%s: Link carrier lost.\n", dev->name);
			netif_carrier_off(dev);
		}
	}

	/* Set up the timer so we'll get called again in 2 seconds. */
	lp->phy_timer.expires = jiffies + 2 * HZ;
	add_timer(&lp->phy_timer);
}

/*
 * This routine is registered with the OS as the function to call when
 * the EMAC interrupts.  It in turn, calls the Xilinx OS independent
 * interrupt function.  There are different interrupt functions for FIFO
 * and scatter-gather so we just set a pointer (Isr) into our private
 * data so we don't have to figure it out here.  The Xilinx OS
 * independent interrupt function will in turn call any callbacks that
 * we have registered for various conditions.
 */
static void
xenet_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	struct net_local *lp = (struct net_local *) dev->priv;

	/* Call it. */
	(*(lp->Isr)) (&lp->Emac);
}

static int
xenet_open(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *) dev->priv;
	u32 Options;
	DUPLEX phy_duplex, mac_duplex;
	int phy_carrier;

	/*
	 * Just to be safe, stop the device first.  If the device is already
	 * stopped, an error will be returned.  In this case, we don't really
	 * care, so cast it to void to make it explicit.
	 */
	(void) XEmac_Stop(&lp->Emac);

	/* Set the MAC address each time opened. */
	if (XEmac_SetMacAddress(&lp->Emac, dev->dev_addr) != XST_SUCCESS) {
		printk(KERN_ERR "%s: Could not set MAC address.\n", dev->name);
		return -EIO;
	}

	/*
	 * If the device is not configured for polled mode, connect to the
	 * interrupt controller and enable interrupts.  Currently, there
	 * isn't any code to set polled mode, so this check is probably
	 * superfluous.
	 */
	Options = XEmac_GetOptions(&lp->Emac);
	if ((Options & XEM_POLLED_OPTION) == 0) {
		int retval;
		/* Grab the IRQ */
		retval =
		    /* request_irq(dev->irq, &xenet_interrupt, SA_INTERRUPT, dev->name, dev); */
		    request_irq(dev->irq, &xenet_interrupt, 0, dev->name, dev);
		if (retval) {
			printk(KERN_ERR
			       "%s: Could not allocate interrupt %d.\n",
			       dev->name, dev->irq);
			return retval;
		}
	}

	/* Set the EMAC's duplex setting based upon what the PHY says. */
	if (!get_phy_status(dev, &phy_duplex, &phy_carrier)) {
		/* We successfully got the PHY status. */
		mac_duplex = ((Options & XEM_FDUPLEX_OPTION)
			      ? FULL_DUPLEX : HALF_DUPLEX);
		if (mac_duplex != phy_duplex) {
			switch (phy_duplex) {
			case HALF_DUPLEX: Options &= ~XEM_FDUPLEX_OPTION; break;
			case FULL_DUPLEX: Options |=  XEM_FDUPLEX_OPTION; break;
			case UNKNOWN_DUPLEX: break;
			}
			/*
			 * The following function will return an error
			 * if the EMAC is already started.  We know it
			 * isn't started so we can safely ignore the
			 * return value.  We cast it to void to make
			 * that explicit.
			 */
			(void) XEmac_SetOptions(&lp->Emac, Options);
		}
	}

	if (XEmac_Start(&lp->Emac) != XST_SUCCESS) {
		printk(KERN_ERR "%s: Could not start device.\n", dev->name);
		free_irq(dev->irq, dev);
		return -EBUSY;
	}

	/* We're ready to go. */
	MOD_INC_USE_COUNT;
	netif_start_queue(dev);

	/* Set up the PHY monitoring timer. */
	lp->phy_timer.expires = jiffies + 2*HZ;
	lp->phy_timer.data = (unsigned long)dev;
	lp->phy_timer.function = &poll_mii;
	add_timer(&lp->phy_timer);

	return 0;
}
static int
xenet_close(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *) dev->priv;

	/* Shut down the PHY monitoring timer. */
	del_timer_sync(&lp->phy_timer);

	netif_stop_queue(dev);

	/*
	 * If not in polled mode, free the interrupt.  Currently, there
	 * isn't any code to set polled mode, so this check is probably
	 * superfluous.
	 */
	if ((XEmac_GetOptions(&lp->Emac) & XEM_POLLED_OPTION) == 0)
		free_irq(dev->irq, dev);

	if (XEmac_Stop(&lp->Emac) != XST_SUCCESS) {
		printk(KERN_ERR "%s: Could not stop device.\n", dev->name);
		return -EBUSY;
	}

	MOD_DEC_USE_COUNT;
	return 0;
}
static struct net_device_stats *
xenet_get_stats(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *) dev->priv;
	return &lp->stats;
}

/* Helper function to determine if a given XEmac error warrants a reset. */
extern inline int
status_requires_reset(XStatus s)
{
	return (s == XST_DMA_ERROR || s == XST_FIFO_ERROR
		|| s == XST_RESET_ERROR);
}

static int
xenet_FifoSend(struct sk_buff *orig_skb, struct net_device *dev)
{
	struct net_local *lp = (struct net_local *) dev->priv;
	struct sk_buff *new_skb;
	unsigned int len, align;
	unsigned int flags;

	/*
	 * The FIFO takes a single request at a time.  Stop the queue to
	 * accomplish this.  We'll wake the queue in FifoSendHandler once
	 * the skb has been sent or in xenet_tx_timeout if something goes
	 * horribly wrong.
	 */
	netif_stop_queue(dev);

	len = orig_skb->len;
	/*
	 * The packet FIFO requires the buffers to be 32 bit aligned.
	 * The sk_buff data is not 32 bit aligned, so we have to do this
	 * copy.  As you probably well know, this is not optimal.
	 */
	if (!(new_skb = dev_alloc_skb(len + 4))) {
		/* We couldn't get another skb. */
		dev_kfree_skb(orig_skb);
		lp->stats.tx_dropped++;
		printk(KERN_ERR "%s: Could not allocate transmit buffer.\n",
		       dev->name);
		netif_wake_queue(dev);
		return -EBUSY;
	}

	/*
	 * A new skb should have the data word aligned, but this code is
	 * here just in case that isn't true...  Calculate how many
	 * bytes we should reserve to get the data to start on a word
	 * boundary.  */
	align = 4 - ((unsigned long) new_skb->data & 3);
	if (align != 4)
		skb_reserve(new_skb, align);

	/* Copy the data from the original skb to the new one. */
	skb_put(new_skb, len);
	memcpy(new_skb->data, orig_skb->data, len);

	/* Get rid of the original skb. */
	dev_kfree_skb(orig_skb);

	lp->saved_skb = new_skb;
	if (XEmac_FifoSend(&lp->Emac, (u8 *) new_skb->data, len) != XST_SUCCESS) {
		/*
		 * I don't think that we will be fighting FifoSendHandler or
		 * xenet_tx_timeout, but it's cheap to guarantee it won't be a
		 * problem.
		 */
		spin_lock_irqsave(&lp->skb_lock,flags);
		new_skb = lp->saved_skb;
		lp->saved_skb = NULL;
		spin_unlock_irqrestore(&lp->skb_lock,flags);

		dev_kfree_skb(new_skb);
		lp->stats.tx_errors++;
		printk(KERN_ERR "%s: Could not transmit buffer.\n", dev->name);
		netif_wake_queue(dev);
		return -EIO;
	}
	return 0;
}

/* The callback function for completed frames sent in FIFO mode. */
static void
FifoSendHandler(void *CallbackRef)
{
	struct net_device *dev = (struct net_device *) CallbackRef;
	struct net_local *lp = (struct net_local *) dev->priv;
	struct sk_buff *tskb;
	unsigned int flags;

	lp->stats.tx_bytes += lp->saved_skb->len;
	lp->stats.tx_packets++;

	/* Make sure that the timeout handler and we don't both free the skb. */
	spin_lock_irqsave(&lp->skb_lock,flags);
	tskb = lp->saved_skb;
	lp->saved_skb = NULL;
	spin_unlock_irqrestore(&lp->skb_lock,flags);
	if (tskb)
		dev_kfree_skb(tskb);

	/* Start the queue back up to allow next request. */
	netif_wake_queue(dev);
}
static void
xenet_tx_timeout(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *) dev->priv;
	printk("%s: Exceeded transmit timeout of %lu ms.  Resetting emac.\n",
	       dev->name, TX_TIMEOUT * 1000UL / HZ);

	lp->stats.tx_errors++;

	/*
	 * Make sure that no interrupts come in that could cause reentrancy
	 * problems in reset.
	 */
	disable_irq(dev->irq);
	reset(dev, UNKNOWN_DUPLEX);
	enable_irq(dev->irq);
}

/* The callback function for frames received when in FIFO mode. */
static void
FifoRecvHandler(void *CallbackRef)
{
	struct net_device *dev = (struct net_device *) CallbackRef;
	struct net_local *lp = (struct net_local *) dev->priv;
	struct sk_buff *skb;
	unsigned int align;
	u32 len;
	XStatus Result;

	/*
	 * The OS independent Xilinx EMAC code does not provide a
	 * function to get the length of an incoming packet and a
	 * separate call to actually get the packet data.  It does this
	 * because they didn't add any code to keep the hardware's
	 * receive length and data FIFOs in sync.  Instead, they require
	 * that you send a maximal length buffer so that they can read
	 * the length and data FIFOs in a single chunk of code so that
	 * they can't get out of sync.  So, we need to allocate an skb
	 * that can hold a maximal sized packet.  The OS independent
	 * code needs to see the data 32-bit aligned, so we tack on an
	 * extra four just in case we need to do an skb_reserve to get
	 * it that way.
	 */
	len = XEM_MAX_FRAME_SIZE;
	if (!(skb = dev_alloc_skb(len + 4))) {
		/* Couldn't get memory. */
		lp->stats.rx_dropped++;
		printk(KERN_ERR "%s: Could not allocate receive buffer.\n",
		       dev->name);
		return;
	}

	/*
	 * A new skb should have the data word aligned, but this code is
	 * here just in case that isn't true...  Calculate how many
	 * bytes we should reserve to get the data to start on a word
	 * boundary.  */
	align = 4 - ((unsigned long) skb->data & 3);
	if (align != 4)
		skb_reserve(skb, align);

	/* Do a shift of 2 to force halfword align of ethernet header
	   and hence word alignment of ip header */
/* 
	skb_reserve(skb,2);
*/

	Result = XEmac_FifoRecv(&lp->Emac, (u8 *) skb->data, &len);
	if (Result != XST_SUCCESS) {
		int need_reset = status_requires_reset(Result);

		lp->stats.rx_errors++;
		dev_kfree_skb(skb);

		printk(KERN_ERR "%s: Could not receive buffer, error=%d%s.\n",
		       dev->name, Result,
		       need_reset ? ", resetting device." : "");
		if (need_reset)
			reset(dev, UNKNOWN_DUPLEX);
		return;
	}

	skb_put(skb, len);	/* Tell the skb how much data we got. */
	skb->dev = dev;		/* Fill out required meta-data. */
	skb->protocol = eth_type_trans(skb, dev);

	/* FIXME FIXME!! */
	/* Microblaze can't do unaligned struct accesses, so after stripping
	   ethernet header (22 octets), ip packet is unaligned.  We do a copy
	   now to put it back on wrd alignment.  This violates every principle
	   of linux networking efficiency, but until either mb-gcc or 
	   microblaze itself is fixed, we're stuck with it */

	memcpy(skb->head, skb->data, skb->len);

	/* And fix up skbuff ptrs */
	skb->tail -= (skb->data-skb->head);
	skb->data=skb->head;

	lp->stats.rx_packets++;
	lp->stats.rx_bytes += len;

	netif_rx(skb);		/* Send the packet upstream. */
}

/* The callback function for errors. */
static void
ErrorHandler(void *CallbackRef, XStatus Code)
{
	struct net_device *dev = (struct net_device *) CallbackRef;
	int need_reset;
	need_reset = status_requires_reset(Code);

	printk(KERN_ERR "%s: device error %d%s\n",
	       dev->name, Code, need_reset ? ", resetting device." : "");
	if (need_reset)
		reset(dev, UNKNOWN_DUPLEX);
}

static void
xenet_set_multicast_list(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *) dev->priv;
	u32 Options;

	/*
	 * XEmac_Start, XEmac_Stop and XEmac_SetOptions are supposed to
	 * be protected by a semaphore.  This Linux adapter doesn't have
	 * it as bad as the VxWorks adapter because the sequence of
	 * requests to us is much more sequential.  However, we do have
	 * one area in which this is a problem.
	 *
	 * xenet_set_multicast_list() is called while the link is up and
	 * interrupts are enabled, so at any point in time we could get
	 * an error that causes our reset() to be called.  reset() calls
	 * the aforementioned functions, and we need to call them from
	 * here as well.
	 *
	 * The solution is to make sure that we don't get interrupts or
	 * timers popping while we are in this function.
	 */
	disable_irq(dev->irq);
	local_bh_disable();

	/*
	 * The dev's set_multicast_list function is only called when
	 * the device is up.  So, without checking, we know we need to
	 * Stop and Start the XEmac because it has already been
	 * started.  XEmac_Stop() will return an error if it is already
	 * stopped, but in this case we don't care so cast it to void
	 * to make it explicit
	 */
	(void) XEmac_Stop(&lp->Emac);

	Options = XEmac_GetOptions(&lp->Emac);

	/* Clear out the bits we may set. */
	Options &= ~(XEM_PROMISC_OPTION | XEM_MULTICAST_OPTION);

	if (dev->flags & IFF_PROMISC)
		Options |= XEM_PROMISC_OPTION;
#if 0
	else {
		/*
		 * SAATODO: Xilinx is going to add multicast support to their
		 * VxWorks adapter and OS independent layer.  After that is
		 * done, this skeleton code should be fleshed out.  Note that
		 * IFF_MULTICAST is being masked out from dev->flags in probe,
		 * so that will need to be removed to actually do multidrop.
		 */
		if ((dev->flags & IFF_ALLMULTI)
		    || dev->mc_count > MAX_MULTICAST ? ? ?) {
			xemac_get_all_multicast ? ? ? ();
			Options |= XEM_MULTICAST_OPTION;
		} else if (dev->mc_count != 0) {
			struct dev_mc_list *mc;

			XEmac_MulticastClear(&lp->Emac);
			for (mc = dev->mc_list; mc; mc = mc->next)
				XEmac_MulticastAdd(&lp->Emac, mc->dmi_addr);
			Options |= XEM_MULTICAST_OPTION;
		}
	}
#endif

	/*
	 * The following function will return an error if the EMAC is already
	 * started.  We know it isn't started so we can safely ignore the
	 * return value.  We cast it to void to make that explicit.
	 */
	(void) XEmac_SetOptions(&lp->Emac, Options);

	/*
	 * XEmac_Start returns an error when: it is already started, the send
	 * and receive handlers are not set, or a scatter-gather DMA list is
	 * missing.  None of these can happen at this point, so we cast the
	 * return to void to make that explicit.
	 */
	(void) XEmac_Start(&lp->Emac);

	/* All done, get those interrupts and timers going again. */
	local_bh_enable();
	enable_irq(dev->irq);
}

static int
xenet_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_local *lp = (struct net_local *) dev->priv;
	/* mii_ioctl_data has 4 u16 fields: phy_id, reg_num, val_in & val_out */
	struct mii_ioctl_data *data = (struct mii_ioctl_data *) &rq->ifr_data;

	XStatus Result;

	switch (cmd) {
	case SIOCGMIIPHY:	/* Get address of MII PHY in use. */
	case SIOCDEVPRIVATE:	/* for binary compat, remove in 2.5 */
		data->phy_id = lp->mii_addr;
		/* Fall Through */

	case SIOCGMIIREG:	/* Read MII PHY register. */
	case SIOCDEVPRIVATE + 1:	/* for binary compat, remove in 2.5 */
		if (data->phy_id > 31 || data->reg_num > 31)
			return -ENXIO;

		/* Stop the PHY timer to prevent reentrancy. */
		del_timer_sync(&lp->phy_timer);
		Result = XEmac_PhyRead(&lp->Emac, data->phy_id,
				       data->reg_num, &data->val_out);
		/* Start the PHY timer up again. */
		lp->phy_timer.expires = jiffies + 2*HZ;
		add_timer(&lp->phy_timer);

		if (Result != XST_SUCCESS) {
			printk(KERN_ERR
			       "%s: Could not read from PHY, error=%d.\n",
			       dev->name, Result);
			return (Result == XST_EMAC_MII_BUSY) ? -EBUSY : -EIO;
		}
		return 0;

	case SIOCSMIIREG:	/* Write MII PHY register. */
	case SIOCDEVPRIVATE + 2:	/* for binary compat, remove in 2.5 */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (data->phy_id > 31 || data->reg_num > 31)
			return -ENXIO;

		/* Stop the PHY timer to prevent reentrancy. */
		del_timer_sync(&lp->phy_timer);
		Result = XEmac_PhyWrite(&lp->Emac, data->phy_id,
					data->reg_num, data->val_in);
		/* Start the PHY timer up again. */
		lp->phy_timer.expires = jiffies + 2*HZ;
		add_timer(&lp->phy_timer);

		if (Result != XST_SUCCESS) {
			printk(KERN_ERR
			       "%s: Could not write to PHY, error=%d.\n",
			       dev->name, Result);
			return (Result == XST_EMAC_MII_BUSY) ? -EBUSY : -EIO;
		}
		return 0;

	default:
		return -EOPNOTSUPP;
	}
}

static void
remove_head_dev(void)
{
	struct net_local *lp;
	struct net_device *dev;
	XEmac_Config *cfg;

	/* Pull the head off of dev_list. */
	spin_lock(&dev_lock);
	dev = dev_list;
	lp = (struct net_local *) dev->priv;
	dev_list = lp->next_dev;
	spin_unlock(&dev_lock);

	/* Put the base address back to the physical address. */
	cfg = XEmac_GetConfig(lp->index);
	iounmap((void *) cfg->BaseAddress);
	cfg->BaseAddress = lp->save_BaseAddress;

	/* Free up the memory. */
	if (lp->saved_skb)
		dev_kfree_skb(lp->saved_skb);
	kfree(lp);

	unregister_netdev(dev);
	kfree(dev);
}

static int __init
probe(int index)
{
	static const unsigned long remap_size
	    = XPAR_EMAC_0_HIGHADDR - XPAR_EMAC_0_BASEADDR + 1;
	struct net_device *dev;
	struct net_local *lp;
	XEmac_Config *cfg;
	unsigned int irq;
	u32 maddr;

/* FIXME hardcoded IRQ for emac at IRQ 3 */
#if 1
	irq=3+index;
#else
	switch (index) {
#if defined(XPAR_INTC_0_EMAC_0_VEC_ID)
	case 0:
		irq = 31 - XPAR_INTC_0_EMAC_0_VEC_ID;
		break;
#if defined(XPAR_INTC_0_EMAC_1_VEC_ID)
	case 1:
		irq = 31 - XPAR_INTC_0_EMAC_1_VEC_ID;
		break;
#if defined(XPAR_INTC_0_EMAC_2_VEC_ID)
	case 2:
		irq = 31 - XPAR_INTC_0_EMAC_2_VEC_ID;
		break;
#if defined(XPAR_INTC_0_EMAC_3_VEC_ID)
#error Edit this file to add more devices.
#endif				/* 3 */
#endif				/* 2 */
#endif				/* 1 */
#endif				/* 0 */
	default:
		return -ENODEV;
	}
#endif

	/* Find the config for our device. */
	cfg = XEmac_GetConfig(index);
	if (!cfg)
		return -ENODEV;

	dev = init_etherdev(0, sizeof (struct net_local));
	if (!dev) {
		printk(KERN_ERR "Could not allocate Xilinx enet device %d.\n",
		       index);
		return -ENOMEM;
	}
	SET_MODULE_OWNER(dev);

	ether_setup(dev);
	dev->irq = irq;

	/* Initialize our private data. */
	lp = (struct net_local *) dev->priv;
	memset(lp, 0, sizeof (struct net_local));
	lp->index = index;
	spin_lock_init(&lp->skb_lock);

	/* Make it the head of dev_list. */
	spin_lock(&dev_lock);
	lp->next_dev = dev_list;
	dev_list = dev;
	spin_unlock(&dev_lock);

	/* Change the addresses to be virtual; save the old ones to restore. */
	lp->save_BaseAddress = cfg->BaseAddress;
	cfg->BaseAddress = (u32) ioremap(lp->save_BaseAddress, remap_size);

	if (XEmac_Initialize(&lp->Emac, cfg->DeviceId) != XST_SUCCESS) {
		printk(KERN_ERR "%s: Could not initialize device.\n",
		       dev->name);
		remove_head_dev();
		return -ENODEV;
	}


/* FIXME this is where the MAC address gets set - 
originally copied from PPCBOOT data structure but we don't have that */
#if 0
	memcpy(dev->dev_addr, ((bd_t *) __res)->bi_enetaddr, 6);
#else
	/* Use MAC_ADDR #defined at tope of adapter.c */
	memcpy(dev->dev_addr, MAC_ADDR,6);
#endif

	if (XEmac_SetMacAddress(&lp->Emac, dev->dev_addr) != XST_SUCCESS) {
		/* should not fail right after an initialize */
		printk(KERN_ERR "%s: Could not set MAC address.\n", dev->name);
		remove_head_dev();
		return -EIO;
	}

	if (XEmac_mIsSgDma(&lp->Emac)) {
		/*
		 * SAATODO: Currently scatter-gather DMA does not work.  At
		 * some point Xilinx is going to get that working in their
		 * code and then this driver should be enhanced in a similar
		 * fashion.
		 */
		printk(KERN_ERR "%s: Scatter gather not supported yet.\n",
		       dev->name);
		remove_head_dev();
		return -EIO;
		/* lp->Isr = XEmac_IntrHandlerDma; */
	} else {
		XEmac_SetFifoRecvHandler(&lp->Emac, dev, FifoRecvHandler);
		XEmac_SetFifoSendHandler(&lp->Emac, dev, FifoSendHandler);
		dev->hard_start_xmit = xenet_FifoSend;
		lp->Isr = XEmac_IntrHandlerFifo;
	}
	XEmac_SetErrorHandler(&lp->Emac, dev, ErrorHandler);


	/* Scan to find the PHY. */
	lp->mii_addr = 0xFF;
	for (maddr = 0; maddr < 31; maddr++) {
		XStatus Result;
		u16 reg;

		Result = XEmac_PhyRead(&lp->Emac, maddr, MII_BMCR, &reg);
		/*
		 * XEmac_PhyRead is currently returning XST_SUCCESS even
		 * when reading from non-existent addresses.  Work
		 * around this by doing a primitive validation on the
		 * control word we get back.
		 */
		if (Result == XST_SUCCESS && (reg & BMCR_RESV) == 0) {
			lp->mii_addr = maddr;
			break;
		}
	}
	if (lp->mii_addr == 0xFF) {
		lp->mii_addr = 0;
		printk(KERN_WARNING
		       "%s: No PHY detected.  Assuming a PHY at address %d.\n",
		       dev->name, lp->mii_addr);
	}

	dev->open = xenet_open;
	dev->stop = xenet_close;
	dev->get_stats = xenet_get_stats;
	dev->flags &= ~IFF_MULTICAST;
	dev->set_multicast_list = xenet_set_multicast_list;
	dev->do_ioctl = xenet_ioctl;
	dev->tx_timeout = xenet_tx_timeout;
	dev->watchdog_timeo = TX_TIMEOUT;

	printk(KERN_INFO
	       "%s: Xilinx EMAC #%d at 0x%08X mapped to 0x%08X, irq=%d\n",
	       dev->name, index,
	       lp->save_BaseAddress, cfg->BaseAddress, dev->irq);
	return 0;
}
static int __init
xenet_init(void)
{
	int index = 0;

	while (probe(index++) == 0) ;
	/* If we found at least one, report success. */
	return (index > 1) ? 0 : -ENODEV;
}

static void __exit
xenet_cleanup(void)
{
	while (dev_list)
		remove_head_dev();
}

EXPORT_NO_SYMBOLS;

module_init(xenet_init);
module_exit(xenet_cleanup);
