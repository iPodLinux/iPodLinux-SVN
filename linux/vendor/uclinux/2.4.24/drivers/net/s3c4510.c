/*
 * linux/deriver/net/s3c4510.c
 * Ethernet driver for Samsung 4510B
 * Copyright (C) 2002 Mac Wang <mac@os.nctu.edu.tw>
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>       // printk()
#include <linux/slab.h>		// kmalloc()
#include <linux/errno.h>	// error codes
#include <linux/types.h>	// size_t
#include <linux/interrupt.h>	// mark_bh

#include <linux/in.h>
#include <linux/netdevice.h>    // net_device
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>

#include <asm/irq.h>
#include "s3c4510.h"

#undef DEBUG
#ifdef	DEBUG
#define TRACE(str, args...)	printk("S3C4510 eth: " str, ## args)
#else
#define TRACE(str, args...)
#endif


static int timeout = 100;	// tx watchdog ticks 100 = 1s
static char *version = "Samsung S3C4510 Ethernet driver version 0.1 (2002-02-20) <mac@os.nctu.edu.tw>\n";

/*
 * This structure is private to each device. It is used to pass
 * packets in and out, so there is place for a packet
 */
struct s3c4510_priv {
	struct net_device_stats stats;

	unsigned long tx_ptr;
	unsigned long gtx_ptr;
	unsigned long rx_ptr;

	// Frame Descriptor linked list for S3C4510 MAC, initialized by FD_init()
	volatile struct FrameDesc rx_fd[RX_FRAME_SIZE];
	volatile struct FrameDesc tx_fd[TX_FRAME_SIZE];

	// Frame Buffer for S3C4510 MAC
	volatile struct ethframe  rx_buf[RX_FRAME_SIZE];
	volatile struct ethframe  tx_buf[TX_FRAME_SIZE];
	
	spinlock_t lock;

	struct sk_buff *skb;
};

/* --------------------- *
 * MII support functions *
 * --------------------- */
void MIIWrite(unsigned long PhyInAddr, unsigned long PhyAddr, unsigned long PhyData)
{
	CSR_WRITE(STADATA, PhyData);
	CSR_WRITE(STACON, PhyInAddr | PhyAddr | MiiBusy | PHYREGWRITE);
	while(CSR_READ(STACON) & MiiBusy);
}

unsigned long MIIRead(unsigned long PhyInAddr, unsigned long PhyAddr)
{
	CSR_WRITE(STACON, PhyInAddr | PhyAddr | MiiBusy);
	while(CSR_READ(STACON) & MiiBusy);
	return CSR_READ(STADATA);
}

/*
 *
 */
void FD_Init(struct net_device *dev)
{
	struct s3c4510_priv *priv = (struct s3c4510_priv *) dev->priv;

	struct FrameDesc *FD_ptr;
	struct FrameDesc *FD_start_ptr;
	struct FrameDesc *FD_last_ptr = NULL;
	unsigned long FB_base;
	unsigned long i;

	// TxFDInitialize() in diag code
	// Get Frame descriptor's base address
	// +0x4000000 is for setting this area to non-cacheable area.
	CSR_WRITE(BDMATXPTR, (unsigned long) priv->tx_fd + 0x4000000);
	priv->gtx_ptr = priv->tx_ptr = CSR_READ(BDMATXPTR);

	// Get Transmit buffer base address.
	FB_base = (unsigned long) priv->tx_buf + 0x4000000;

	// Generate linked list.
	FD_start_ptr = FD_ptr = (struct FrameDesc *)priv->tx_ptr;
	FD_last_ptr = NULL;

	for(i = 0; i < TX_FRAME_SIZE; i++) {
		if(FD_last_ptr == NULL)
			FD_last_ptr = FD_ptr;
		else
			FD_last_ptr -> NextFrameDescriptor = (unsigned long)FD_ptr;

		FD_ptr->FrameDataPtr = (unsigned long)(FB_base & CPU_owner);
		FD_ptr->Reserved = 0;
		FD_ptr->StatusAndFrameLength = (unsigned long)0x0;
		FD_last_ptr = FD_ptr;
		FD_ptr++;
		FB_base += sizeof(struct ethframe);
	}

	//Make Frame descriptor to ring buffer type.
	FD_last_ptr->NextFrameDescriptor = (unsigned long)FD_start_ptr;

	// RxFDInitialize() in diag code
	// Get Frame descriptor's base address
	CSR_WRITE(BDMARXPTR, (unsigned long) priv->rx_fd + 0x4000000);
	priv->rx_ptr = CSR_READ(BDMARXPTR);

	// Get Transmit buffer base address
	FB_base = (unsigned long)priv->rx_buf + 0x4000000;

	// Generate linked list
	FD_start_ptr = FD_ptr = (struct FrameDesc *)priv->rx_ptr;
	FD_last_ptr = NULL;

	for(i = 0; i < RX_FRAME_SIZE; i++) {
		if(FD_last_ptr == NULL )
			FD_last_ptr = FD_ptr;
		else
			FD_last_ptr -> NextFrameDescriptor = (unsigned long)FD_ptr;

		FD_ptr->FrameDataPtr = (unsigned long)(FB_base | BDMA_owner);
		FD_ptr->Reserved = 0;
		FD_ptr->StatusAndFrameLength = (unsigned long)0x0;
		FD_ptr->NextFrameDescriptor = 0x0;

		FD_last_ptr = FD_ptr;
		FD_ptr++;
		FB_base += sizeof(struct ethframe);
	}   

	// Make Frame descriptor to ring buffer type.
	FD_last_ptr->NextFrameDescriptor = (unsigned long)FD_start_ptr;
}


/*
 * rx
 */
void s3c4510_rx(int irq, void *dev_id, struct pt_regs *regs)
{
//	int i;
	int len;
	unsigned char *data;
	struct sk_buff *skb;
	struct FrameDesc *FD_ptr;
	unsigned long CFD_ptr;
	unsigned long RxStat;
	unsigned long BDMAStat;
	struct net_device *dev = (struct net_device *) dev_id;
	struct s3c4510_priv *priv = (struct s3c4510_priv *) dev->priv;
	TRACE("rx\n");

	spin_lock(&priv->lock);

	// 1. Get current frame descriptor and status
	CFD_ptr = CSR_READ(BDMARXPTR);
	BDMAStat = CSR_READ(BDMASTAT);

	// 2. Clear BDMA status register bit by write 1
	CSR_WRITE(BDMASTAT, BDMAStat | S_BRxRDF);

	do {
		// 3. Check Null List Interrupt
		/*
		if(CSR_READ(BDMASTAT) & BRxNL) {
			CSR_WRITE(BDMASTAT, );
		}
		*/

		// 4. Get Rx Frame Descriptor
		FD_ptr = (struct FrameDesc *) priv->rx_ptr;
		RxStat = (FD_ptr->StatusAndFrameLength >> 16) & 0xffff;
		
		// 5. If Rx frame is good, then process received frame
		if(RxStat & Good){
			len = (FD_ptr->StatusAndFrameLength & 0xffff) - 4;
			data = (unsigned char *) FD_ptr->FrameDataPtr + 2;

			// 6. Get received frame to memory buffer
			skb = dev_alloc_skb(len+2);
			if(!skb) {
				printk("S3C4510 eth: low on mem - packet dropped\n");
				priv->stats.rx_dropped++;
				return;
			}
//			memcpy(skb_put(skb, len), data, len);
/*
			printk("len: %d\n", len);
			for(i = 0; i < len; i++)
			{
				printk("%3x", data[i]);
				if((i+1)%16==0)
					printk("\n");
			}
			printk("\n");
*/
			skb->dev = dev;
			skb_reserve(skb, 2);
			skb_put(skb, len);
			eth_copy_and_sum(skb, data, len, 0);
			skb->protocol = eth_type_trans(skb, dev);
			priv->stats.rx_packets++;
			priv->stats.rx_bytes += len;
			netif_rx(skb);
		}
		else {
			// 7. If Rx frame has error, then process err frame
			priv->stats.rx_errors++;
			if(RxStat & LongErr)
				priv->stats.rx_length_errors++;
			if(RxStat & OvMax)
				priv->stats.rx_over_errors++;
			if(RxStat & CRCErr)
				priv->stats.rx_crc_errors++;
			if(RxStat & AlignErr)
				priv->stats.rx_frame_errors++;
			if(RxStat & Overflow)
				priv->stats.rx_fifo_errors++;
			
		}
		// 8. Change ownership to BDMA for next use
		FD_ptr -> FrameDataPtr |= BDMA_owner;

		// Save Current Status and Frame Length field, and clear
		FD_ptr -> StatusAndFrameLength = 0x0;

		// 9. Get Next Frame Descriptor pointer to process
		priv->rx_ptr = (unsigned long)(FD_ptr -> NextFrameDescriptor);
	}while(CFD_ptr != priv->rx_ptr);

	// 10. Check Notowner status
	if(CSR_READ(BDMASTAT) & S_BRxNO) {
		CSR_WRITE(BDMASTAT, S_BRxNO);
	}
		
	spin_unlock(&priv->lock);
}

/*
 * tx
 */
void s3c4510_tx(int irq, void *dev_id, struct pt_regs *regs)
{
	struct FrameDesc *FD_ptr;
	unsigned long CFD_ptr;
	unsigned long *FB_ptr;
	unsigned long status;
	struct net_device *dev = (struct net_device *) dev_id;
	struct s3c4510_priv *priv = (struct s3c4510_priv *) dev->priv;
	TRACE("tx\n");

	spin_lock(&priv->lock);

	CFD_ptr = CSR_READ(BDMATXPTR);
	while(priv->gtx_ptr != CFD_ptr)
	{
		FD_ptr = (struct FrameDesc *) priv->gtx_ptr;
		FB_ptr = (unsigned long *) &FD_ptr->FrameDataPtr;

		if(!(*FB_ptr & BDMA_owner))
		{
			status = (FD_ptr->StatusAndFrameLength >> 16) & 0xffff;
			if(status & Comp) {
				priv->stats.tx_packets++;
			}
			else {
				priv->stats.tx_errors++;
				if(status & TxPar)
					priv->stats.tx_aborted_errors++;
				if(status & NCarr)
					priv->stats.tx_carrier_errors++;
				if(status & Under)
					priv->stats.tx_fifo_errors++;
				if(status & LateColl)
					priv->stats.tx_window_errors++;
				if(status & ExColl)
					priv->stats.collisions++;
			}
		}
	
		// Clear Framedata pointer already used
		FD_ptr->StatusAndFrameLength = 0x0;
		priv->gtx_ptr = (unsigned long) FD_ptr->NextFrameDescriptor;
	}
	
	spin_unlock(&priv->lock);
	return;
}

/*
 * Open and Close
 */
int s3c4510_open(struct net_device *dev)
{
	int i;
	unsigned long status;
	MOD_INC_USE_COUNT;

	TRACE("open\n");

	// Disable irqs
	disable_irq(INT_BDMATX);
	disable_irq(INT_BDMARX);
	disable_irq(INT_MACTX);
	disable_irq(INT_MACRX);

	// register rx isr
	if(request_irq(INT_BDMARX, &s3c4510_rx, SA_INTERRUPT, "eth rx isr", dev)) {
		printk(KERN_ERR "s3c4510: Can't get irq %d\n", INT_BDMARX);
		return -EAGAIN;
	}

	// register tx isr
	if(request_irq(INT_MACTX, &s3c4510_tx, SA_INTERRUPT, "eth tx isr", dev)) {
		printk(KERN_ERR "s3c4510: Can't get irq %d\n", INT_MACTX);
		return -EAGAIN;
	}
#if 0
	printk("Reseting PHY\n");
	MIIWrite(0x0, 0x0, 1<<15);
	while((MIIRead(0x0, 0x0) & (1<<15)));
	
	
	printk("Perform AN again\n");
	MIIWrite(0x0, 0x0, MIIRead(0x0, 0x0) & ~(1<<12));
//	while(!(MIIRead(0x11, 0x0) & (1<<4)));

	MIIWrite(0x0, 0x0, 1<<13|1<<8);
	
	status = MIIRead(0x11, 0x0);
	if(status & 1<<15)
		printk("10M/b  ");
	else
		printk("100M/b  ");
	if(status & 1<<14)
		printk("Full duplex\n");
	else	printk("Half duplex\n");
#endif	
	// reset BDMA and MAC
	CSR_WRITE(BDMARXCON, BRxRS);
	CSR_WRITE(BDMATXCON, BTxRS);
	CSR_WRITE(MACON, Reset);
	CSR_WRITE(BDMARXLSZ, sizeof(struct ethframe));
	CSR_WRITE(MACON, gMACCON);

	FD_Init(dev);

	for(i = 0; i < (int)dev->addr_len-2; i++)
		CAM_Reg(0) = (CAM_Reg(0) << 8) | dev->dev_addr[i];
	for(i = (int)dev->addr_len-2; i < (int)dev->addr_len; i++)
		CAM_Reg(1) = (CAM_Reg(1) << 8) | dev->dev_addr[i];
	CAM_Reg(1) = (CAM_Reg(1) << 16);

	CSR_WRITE(CAMEN, 0x0001);
	CSR_WRITE(CAMCON, gCAMCON);

	enable_irq(INT_BDMARX);
	enable_irq(INT_MACTX);

	// ReadyMACTx();
	CSR_WRITE(BDMATXCON, gBDMATXCON);
	CSR_WRITE(MACTXCON, gMACTXCON);
	// ReadyMACRx();
	CSR_WRITE(BDMARXCON, gBDMARXCON);
	CSR_WRITE(MACRXCON, gMACRXCON);

	// Start the transmit queue
	netif_start_queue(dev);
	
	return 0;
}

int s3c4510_stop(struct net_device *dev)
{
	TRACE("stop\n");
	CSR_WRITE(BDMATXCON, 0);
	CSR_WRITE(BDMARXCON, 0);
	CSR_WRITE(MACTXCON, 0);
	CSR_WRITE(MACRXCON, 0);

	free_irq(INT_BDMARX, dev);
	free_irq(INT_MACTX, dev);

	netif_stop_queue(dev);
	MOD_DEC_USE_COUNT;
	
	return 0;
}

int s3c4510_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
//	int i;
	int len;
	char *data;
	struct s3c4510_priv *priv = (struct s3c4510_priv *) dev->priv;

	struct FrameDesc	*FD_ptr;	// frame descriptor pointer
	volatile unsigned long	*FB_ptr;	// frame data pointer
	unsigned char		*FB_data;	// frame data

	TRACE("start_xmit\n");
/*
	printk("sk_buff->len: %d\n", skb->len);
	printk("sk_buff->data_len: %d\n", skb->data_len);
	for(i = 0; i < skb->len; i++) {
		printk("%4x", skb->data[i]);
		if((i+1)%16 ==0)
			printk("\n");
	}
	printk("\n");
*/
	len = skb->len < ETH_ZLEN ? ETH_ZLEN : skb->len;
	data = skb->data;
	dev->trans_start = jiffies;
	
	// 1. Get Tx frame descriptor & data pointer
	FD_ptr = (struct FrameDesc *) priv->tx_ptr;
	FB_ptr = (unsigned long *) &FD_ptr->FrameDataPtr;
	FB_data = (unsigned char *) FD_ptr->FrameDataPtr;

	// 2. Check BDMA ownership
	if(*FB_ptr & BDMA_owner) {
		printk("no tx buffer\n");
//		check it later
//		netif_stop_queue(dev);
		return 1;
	}
	
	// 3. Prepare Tx Frame data to Frame buffer
	memcpy(FB_data, data, len);
	
	// 4. Set Tx Frame flag & Length Field
	FD_ptr->Reserved = (Padding | CRCMode | FrameDataPtrInc | LittleEndian | WA00 | MACTxIntEn);
	FD_ptr->StatusAndFrameLength = (len & 0xFFFF);
	
	// 5. Change ownership to BDMA
	FD_ptr->FrameDataPtr |= BDMA_owner;

	// 6. Enable MAC and BDMA Tx control register
	CSR_WRITE(BDMATXCON, gBDMATXCON);
	CSR_WRITE(MACTXCON, gMACTXCON);

	// 7. Change the Tx frame descriptor for next use
	priv->tx_ptr = (unsigned long)(FD_ptr->NextFrameDescriptor);

	dev_kfree_skb(skb);
	
	return 0;
}

struct net_device_stats *s3c4510_get_stats(struct net_device *dev)
{
	struct s3c4510_priv *priv = (struct s3c4510_priv *) dev->priv;
	TRACE("get_stats\n");
	return &priv->stats;
}

/*
 * The init function, invoked by register_netdev()
 */
int s3c4510_init(struct net_device *dev)
{
	int i;
	TRACE("init\n");
	ether_setup(dev);	// Assign some of the fields

	// set net_device methods
	dev->open = s3c4510_open;
	dev->stop = s3c4510_stop;
//	dev->ioctl = s3c4510_ioctl;
	dev->get_stats = s3c4510_get_stats;
//	dev->tx_timeout = s3c4510_tx_timeout;
	dev->hard_start_xmit = s3c4510_start_xmit;

	// set net_device data members
	dev->watchdog_timeo = timeout;
	dev->irq = 17;
	dev->dma = 0;

	// set MAC address manually
	dev->dev_addr[0] = 0x00;
	dev->dev_addr[1] = 0x40;
	dev->dev_addr[2] = 0x95;
	dev->dev_addr[3] = 0x36;
	dev->dev_addr[4] = 0x35;
	dev->dev_addr[5] = 0x34;

	printk(KERN_INFO "%s: ", dev->name);
	for(i = 0; i < 6; i++)
		printk("%2.2x%c", dev->dev_addr[i], (i==5) ? ' ' : ':');
	printk("\n");

	SET_MODULE_OWNER(dev);

	dev->priv = kmalloc(sizeof(struct s3c4510_priv), GFP_KERNEL);
	if(dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0, sizeof(struct s3c4510_priv));
	spin_lock_init(&((struct s3c4510_priv *) dev->priv)->lock);
	return 0;
}

struct net_device s3c4510_netdevs = {
	init: s3c4510_init,
};

/*
 * Finally, the module stuff
 */
int __init s3c4510_init_module(void)
{
	int result;
	TRACE("init_module\n");

	//Print version information
	printk(KERN_INFO "%s", version);

	//register_netdev will call s3c4510_init()
	if((result = register_netdev(&s3c4510_netdevs)))
		printk("S3C4510 eth: Error %i registering device \"%s\"\n", result, s3c4510_netdevs.name);
	return result ? 0 : -ENODEV;
}

void __exit s3c4510_cleanup(void)
{
	TRACE("cleanup\n");
	kfree(s3c4510_netdevs.priv);
	unregister_netdev(&s3c4510_netdevs);
	return;
}

module_init(s3c4510_init_module);
module_exit(s3c4510_cleanup);

MODULE_DESCRIPTION("Samsung 4510B ethernet driver");
MODULE_AUTHOR("Mac Wang <mac@os.nctu.edu.tw>");
MODULE_LICENSE("GPL");
