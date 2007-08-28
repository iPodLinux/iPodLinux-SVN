/* uCcs89x0.c: A Crystal Semiconductor CS89[02]0 driver for linux. */
/* 

	Port for uCsimm 1999 D. Jeff Dionne, Rt-Control Inc.(Arcturus Networks Inc.)
	Port for arm7 platforms 2001 Oleksandr Zhadan, Arcturus Networks Inc.

	Written 1996 by Russell Nelson, with reference to skeleton.c
	written 1993-1994 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

	The author may be reached at nelson@crynwr.com, Crynwr
	Software, 11 Grant St., Potsdam, NY 13676

  Changelog:

  Mike Cruse        : mcruse@cti-ltd.com
                    : Changes for Linux 2.0 compatibility. 
                    : Added dev_id parameter in net_interrupt(),
                    : request_irq() and free_irq(). Just NULL for now.

  Mike Cruse        : Added MOD_INC_USE_COUNT and MOD_DEC_USE_COUNT macros
                    : in net_open() and net_close() so kerneld would know
                    : that the module is in use and wouldn't eject the 
                    : driver prematurely.

  Mike Cruse        : Rewrote init_module() and cleanup_module using 8390.c
                    : as an example. Disabled autoprobing in init_module(),
                    : not a good thing to do to other devices while Linux
                    : is running from all accounts.
*/


/* ======================= configure the driver here ======================= */

/* use 0 for production, 1 for verification, >2 for debug */
#ifndef NET_DEBUG
#define NET_DEBUG 1
//#define NET_DEBUG 77
#endif

/* ======================= end of configuration ======================= */


/* Always include 'config.h' first in case the user wants to turn on
   or override something. */
#include <linux/config.h>

#define PRINTK(x) printk x

/*
  Sources:

	Crynwr packet driver epktisa.

	Crystal Semiconductor data sheets.

*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <asm/system.h>
#include <asm/bitops.h>

#ifndef CONFIG_UCCS89x0_HW_SWAP
#include <asm/io.h>
#else
#include <asm/io_hw_swap.h>
#endif

#include <asm/irq.h>
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include "uCcs8900.h"

#ifdef CONFIG_M68328
#include <asm/MC68328.h>
#endif

#ifdef CONFIG_M68EZ328
#include <asm/MC68EZ328.h>
#endif

#ifdef CONFIG_M68VZ328
#include <asm/MC68VZ328.h>
#endif

#ifdef CONFIG_M5272
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#endif

#if defined(CONFIG_UCBOOTSTRAP)
extern unsigned char cs8900a_hwaddr[6];
#endif

static char version[] __initdata=
"cs89x0.c:v1.02 11/26/96 Russell Nelson <nelson@crynwr.com>
cs89x0.c:68EZ328 support D. Jeff Dionne <jeff@arcturusnetworks.com> 1999
cs89x0.c:S3C4530 support O. Zhadan <oleks@arcturusnetworks.com> 2001
";

static unsigned int net_debug = NET_DEBUG;

#if defined(CONFIG_UCSIMM) || defined(CONFIG_UCDIMM)
static unsigned int netcard_portlist[] __initdata = { 0x10000300,0,0,0,0,0,0,0};
#elif defined (CONFIG_BOARD_EVS3C4530HEI)
static unsigned int netcard_portlist[] __initdata = { 0x05400000,0,0,0,0,0,0,0};
#elif defined(CONFIG_BOARD_UC5272)
static unsigned int netcard_portlist[] __initdata = { 0x30000000, 0 };
#else
static unsigned int netcard_portlist[] __initdata =
   { 0x300, 0x320, 0x340, 0x360, 0x200, 0x220, 0x240, 0x260, 0x280, 0x2a0, 0x2c0, 0x2e0, 0};
static unsigned int cs8900_irq_map[] = {10,11,12,5};
#endif


/* The number of low I/O ports used by the ethercard. */
#define NETCARD_IO_EXTENT	16

/* void *irq2dev_map[1];  FIXME:  This does NOT go here */

/* Information that need to be kept for each board. */
struct net_local {
	struct net_device_stats stats;
	int chip_type;		/* one of: CS8900, CS8920, CS8920M */
	char chip_revision;	/* revision letter of the chip ('A'...) */
	int send_cmd;		/* the propercommand used to send a packet. */
	int auto_neg_cnf;
	int adapter_cnf;
	int isa_config;
	int irq_map;
	int rx_mode;
	int curr_rx_cfg;
        int linectl;
	int force;		/* force various values; see FORCE* above. */
	spinlock_t lock;
        int send_underrun;      /* keep track of how many underruns in a row we get */
};


/* Index to functions, as function prototypes. */

unsigned char *get_MAC_address(char *devname);
extern int cs89x0_probe(struct net_device *dev);

static int cs89x0_probe1(struct net_device *dev, int ioaddr);
static int net_open(struct net_device *dev);
static int net_send_packet(struct sk_buff *skb, struct net_device *dev);
static void cs8900_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void set_multicast_list(struct net_device *dev);
static void net_rx(struct net_device *dev);
static int net_close(struct net_device *dev);
static struct net_device_stats *net_get_stats(struct net_device *dev);
static void reset_chip(struct net_device *dev);

static int set_mac_address(struct net_device *dev, void *addr);


/* Example routines you must write ;->. */
#define tx_done(dev) 1


/* Check for a network adaptor of this type, and return '0' iff one exists.
   If dev->base_addr == 0, probe all likely locations.
   If dev->base_addr == 1, always return failure.
   If dev->base_addr == 2, allocate space for the device and return success
   (detachable devices only).
   */

int __init cs89x0_probe(struct net_device *dev)
{
    static int i = 0; /* don't probe twice */
    if	 ( netcard_portlist[i] )
	 return(cs89x0_probe1(dev, netcard_portlist[i++]));

    else return -ENXIO;
}

int inline
readreg(struct net_device *dev, int portno)
{
	outw(portno, dev->base_addr + ADD_PORT);
	return inw(dev->base_addr + DATA_PORT);
}

void inline
writereg(struct net_device *dev, int portno, int value)
{
	outw(portno, dev->base_addr + ADD_PORT);
	outw(value,  dev->base_addr + DATA_PORT);
}

int inline
readword(struct net_device *dev, int portno)
{
	return inw(dev->base_addr + portno);
}

void inline
writeword(struct net_device *dev, int portno, int value)
{
	outw(value, dev->base_addr + portno);
}

/* This is the real probe routine.  */

static int __init 
cs89x0_probe1(struct net_device *dev, int ioaddr)
{
	struct net_local *lp;
	static unsigned version_printed = 0;
	int i;
	unsigned rev_type = 0;

/*	irq2dev_map[0] = dev;  */

#if defined(CONFIG_UCSIMM) || defined(CONFIG_UCDIMM)
	/* set up the chip select */
	*(volatile unsigned  char *)PFSEL_ADDR |=   0x01;  /* output /sleep */
	*(volatile unsigned short *)PFDIR_ADDR |= 0x0101;  /* not sleeping */

	*(volatile unsigned  char *)PFSEL_ADDR &=   ~0x02; /* input irq5 */
	*(volatile unsigned short *)PFDIR_ADDR &= ~0x0202; /* irq5 fcn on */
	
	*(volatile unsigned short *)CSGBB_ADDR  = 0x8000;  /* 0x10000000 */
	*(volatile unsigned short *)CSB_ADDR    = 0x01e1;  /* 128k, 2ws, FLASH, en */
#endif

#ifdef CONFIG_ARCH_ATMEL
	*(volatile unsigned int *) AIC_IDCR = AIC_IRQ1; /* disable interrupt IRQ1 */
#ifdef	CONFIG_EB40LS	 
	*(volatile unsigned int *) PIO_DISABLE_REGISTER = (1 << 10); /* disable PIO for IRQ1 */
#endif	
	*(volatile unsigned int *) AIC_ICCR = AIC_IRQ1; /* clear interrupt IRQ1 */
	*(volatile unsigned int *) AIC_IECR = AIC_IRQ1; /* enable interrupt IRQ1 */
#endif


#ifdef CONFIG_ALMA_ANS
        /* 
         * Make sure the chip select (CSA1) is enabled 
	 * Note, that we don't have to program the base address, since
         * it is programmed once for both CSA0 and CSA1 in *-head.S
	 */
        PFSEL &= ~PF_CSA1;
        PFDIR |= PF_CSA1;
 
        /* Make sure that interrupt line (irq3) is enabled too */
        PDSEL  &= ~PD_IRQ3;
        PDDIR  &= ~PD_IRQ3;
        PDKBEN |= PD_IRQ3;                                                       
#endif

	/* Initialize the device structure. */
	if (dev->priv == NULL) {
		dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
                memset(dev->priv, 0, sizeof(struct net_local));
        }
	dev->base_addr = ioaddr;
	lp = (struct net_local *)dev->priv;

	if (readreg(dev, PP_ChipID) != CHIP_EISA_ID_SIG) {
	  printk("cs89x0.c: No CrystalLan device found.\n");
		return ENODEV;
	}

	/* get the chip type */
	printk ("CrystalLAN EISA ID: 0x%04x\n", readreg(dev, 0));

	rev_type = readreg(dev, PRODUCT_ID_ADD);
	lp->chip_type = rev_type &~ REVISON_BITS;
	lp->chip_revision = ((rev_type & REVISON_BITS) >> 8) + 'A';

	/* Check the chip type and revision in order to set the correct send command
	CS8920 revision C and CS8900 revision F can use the faster send. */
	lp->send_cmd = TX_AFTER_ALL;
#if 0
	if (lp->chip_type == CS8900 && lp->chip_revision >= 'F')
		lp->send_cmd = TX_NOW;
	if (lp->chip_type != CS8900 && lp->chip_revision >= 'C')
		lp->send_cmd = TX_NOW;
#endif
	if (net_debug  &&  version_printed++ == 0)
		printk(version);

	printk("%s: cs89%c0%s rev %c found at 0x%.8lx %s",
	       dev->name,
	       lp->chip_type==CS8900?'0':'2',
	       lp->chip_type==CS8920M?"M":"",
	       lp->chip_revision,
	       dev->base_addr,
	       readreg(dev, PP_SelfST) & ACTIVE_33V ? "3.3Volts" : "5Volts");

	reset_chip(dev);

	/* Fill this in, we don't have an EEPROM */
	lp->adapter_cnf = A_CNF_10B_T | A_CNF_MEDIA_10B_T;
	lp->auto_neg_cnf = EE_AUTO_NEG_ENABLE | IMM_BIT;

	printk(" media %s%s%s",
	       (lp->adapter_cnf & A_CNF_10B_T)?"RJ-45,":"",
	       (lp->adapter_cnf & A_CNF_AUI)?"AUI,":"",
	       (lp->adapter_cnf & A_CNF_10B_2)?"BNC,":"");

	lp->irq_map = 0xffff;

	/* dev->dev_addr[0] through dev->dev_addr[6] holds the mac address
	 * of this ethernet device.  This can be set to anything we want it
	 * to be.  But care should be taken to make this number unique... */

#if   defined(CONFIG_UCSIMM) || defined(CONFIG_EB40LS) || \
      defined(CONFIG_BLIP)   || defined(CONFIG_UCLINKII) || \
      defined(CONFIG_UCDIMM)
 		memcpy(dev->dev_addr, cs8900a_hwaddr, 6);
	
#elif defined (CONFIG_BOARD_UCLINKII) || \
      defined (CONFIG_BOARD_EVS3C4530LII) || \
      defined (CONFIG_BOARD_EVS3C4530HEI)
      memcpy(dev->dev_addr, get_MAC_address("dev1"), 6);
#else
#error	    MAC address is not defined
#endif

	/* print the ethernet address. */
	for (i = 0; i < ETH_ALEN; i++)
		printk(" %2.2x", dev->dev_addr[i]);

#ifdef FIXME
	/* Grab the region so we can find another board if autoIRQ fails. */
	request_region(ioaddr, NETCARD_IO_EXTENT,"cs89x0");
#endif

	dev->open		= net_open;
	dev->stop		= net_close;
	dev->hard_start_xmit 	= net_send_packet;
	dev->get_stats		= net_get_stats;
	dev->set_multicast_list = &set_multicast_list;
	dev->set_mac_address 	= &set_mac_address;

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);

	printk("\n");
	return 0;
}



void __init
reset_chip(struct net_device *dev)
{
	int reset_start_time;

	writereg(dev, PP_SelfCTL, readreg(dev, PP_SelfCTL) | POWER_ON_RESET);

	current->state = TASK_INTERRUPTIBLE;
	/* wait 30 ms */
	schedule_timeout(30*HZ/1000);
	
	/* Wait until the chip is reset */
	reset_start_time = jiffies;
	while( (readreg(dev, PP_SelfST) & INIT_DONE) == 0 && jiffies - reset_start_time < 2)
		;
}

static int
detect_tp(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int timenow = jiffies;

	if (net_debug > 1) printk("%s: Attempting TP\n", dev->name);

        /* If connected to another full duplex capable 10-Base-T card the link pulses
           seem to be lost when the auto detect bit in the LineCTL is set.
           To overcome this the auto detect bit will be cleared whilst testing the
           10-Base-T interface.  This would not be necessary for the sparrow chip but
           is simpler to do it anyway. */
	writereg(dev, PP_LineCTL, lp->linectl &~ AUI_ONLY);

        /* Delay for the hardware to work out if the TP cable is present - 150ms */
	for (timenow = jiffies; jiffies - timenow < 15; )
                ;
	if ((readreg(dev, PP_LineST) & LINK_OK) == 0)
		return 0;

	return A_CNF_MEDIA_10B_T;
}

/* send a test packet - return true if carrier bits are ok */
int
send_test_pkt(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	char test_packet[] = { 0,0,0,0,0,0, 0,0,0,0,0,0,
				 0, 46, /* A 46 in network order */
				 0, 0, /* DSAP=0 & SSAP=0 fields */
				 0xf3, 0 /* Control (Test Req + P bit set) */ };
	long timenow = jiffies;
	unsigned short event;

	writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) | SERIAL_TX_ON);

	memcpy(test_packet,          dev->dev_addr, ETH_ALEN);
	memcpy(test_packet+ETH_ALEN, dev->dev_addr, ETH_ALEN);
        outw(TX_AFTER_ALL, ioaddr + TX_CMD_PORT);
        outw(ETH_ZLEN, ioaddr + TX_LEN_PORT);

	/* Test to see if the chip has allocated memory for the packet */
	while (jiffies - timenow < 5)
		if (readreg(dev, PP_BusST) & READY_FOR_TX_NOW)
			break;
	if (jiffies - timenow >= 5)
		return 0;	/* this shouldn't happen */

	/* Write the contents of the packet */
	outsw(ioaddr + TX_FRAME_PORT,test_packet,(ETH_ZLEN+1) >>1);

	if (net_debug > 1) printk("Sending test packet ");
	/* wait a couple of jiffies for packet to be received */
	for (timenow = jiffies; jiffies - timenow < 60; )
                ;
        if (((event = readreg(dev, PP_TxEvent)) & TX_SEND_OK_BITS) == TX_OK) {
                if (net_debug > 1) printk("succeeded\n");
                return 1;
        }
	if (net_debug > 1) printk("failed TxEvent 0x%.4x\n",event);
	return 0;
}


void
write_irq(struct net_device *dev, int chip_type, int irq)
{
  /* we only hooked up 0 :-) */
	writereg(dev, PP_CS8900_ISAINT, 0);
}

/* Open/initialize the board.  This is called (in the current kernel)
   sometime after booting when the 'ifconfig' program is run.

   This routine should set everything up anew at each open, even
   registers that "should" only need to be set once at boot, so that
   there is non-reboot way to recover if something goes wrong.
   */
static int
net_open(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int result = 0;

	write_irq(dev, lp->chip_type, 0);

/*	irq2dev_map[0] = dev;   */
	writereg(dev, PP_BusCTL, 0); /* ints off! */

#if defined(CONFIG_UCSIMM) || defined(CONFIG_UCDIMM)
	*(volatile unsigned short *)0xfffff302 |= 0x0080; /* +ve pol irq */
	dev->irq = IRQ5_IRQ_NUM;
        if (request_irq(dev->irq, cs8900_interrupt, IRQ_FLG_STD,
                        "CrystalLAN_cs8900a", dev))
                panic("Unable to attach cs8900 intr\n");
#endif

#ifdef CONFIG_ARCH_ATMEL
	/* Set IRQ1 with high level sensitive & priority level 6. */
	*(volatile unsigned int *) AIC_SMR(IRQ_IRQ1) = 0x46;

	/* We use IRQ_IRQ1 for the network interrupt */
	dev->irq = IRQ_IRQ1;
        if (request_irq(dev->irq,
                        cs8900_interrupt,
                        IRQ_FLG_STD,
                        "CrystalLAN_cs8900a", NULL))
                panic("Unable to attach cs8900 intr\n");                        
#endif

#if defined (CONFIG_BOARD_UCLINKII)     || \
    defined (CONFIG_BOARD_EVS3C4530LII) || \
    defined (CONFIG_BOARD_EVS3C4530HEI)
	
	*(volatile unsigned int *)IOPCON0 |= (xINTREQ0_ENABLE | xINTREQ0_ACT_HI);
	dev->irq = _IRQ0;
	if (request_irq (dev->irq, cs8900_interrupt, 0,
			 "Crystal_CS8900", dev))
 	  panic("Unable to attach cs8900 intr\n");
#endif

#ifdef CONFIG_ALMA_ANS
	/* We use positive polarity IRQ3 as a network interrupt */
	ICR |= ICR_POL3;
	dev->irq = IRQ3_IRQ_NUM;
	if (request_irq(dev->irq, cs8900_interrupt,
                        IRQ_FLG_STD,
                        "CrystalLAN_cs8900a", NULL))
                panic("Unable to attach cs8900 intr\n");                        
#endif

#if defined (CONFIG_BOARD_UC5272)
	dev->irq = 65; /* FIXME: These should be enumerated! */
	/* do some nonsense that should be taken care of by
	 * request_irq:
	 * enable INT1 and give it priority 4 */

	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	*icrp = (*icrp & 0x07777777) | 0xC0000000;

	*(volatile unsigned long *) (MCF_MBAR + MCFSIM_PITR) |= 0x80000000;

        if (request_irq(dev->irq, cs8900_interrupt, 0, "cs8900a", dev))
	  panic("Unable to attach cs8900 intr\n");
#endif

	/* set the Ethernet address */
	set_mac_address(dev, dev->dev_addr);

	/* Set the LineCTL */
	lp->linectl = 0;

        /* check to make sure that they have the "right" hardware available */
	switch(lp->adapter_cnf & A_CNF_MEDIA_TYPE) {
	case A_CNF_MEDIA_10B_T:
		result = lp->adapter_cnf & A_CNF_10B_T;
		break;
	case A_CNF_MEDIA_AUI:
		result = lp->adapter_cnf & A_CNF_AUI;
		break;
	case A_CNF_MEDIA_10B_2:
		result = lp->adapter_cnf & A_CNF_10B_2;
		break;
        default:
		result = lp->adapter_cnf & 
		         (A_CNF_10B_T | A_CNF_AUI | A_CNF_10B_2);
        }
        if (!result) {
                printk("%s: EEPROM is configured for unavailable media\n", dev->name);
        release_irq:
                writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) & ~(SERIAL_TX_ON | SERIAL_RX_ON));
		/* so subsequent opens don't fail we release the IRQ ...MaTed--- */
		free_irq( dev->irq, dev);
/*                irq2dev_map[dev->irq] = 0;    */
		return -EAGAIN;
	}

        /* set the hardware to the configured choice */
	switch(lp->adapter_cnf & A_CNF_MEDIA_TYPE) {
	case A_CNF_MEDIA_10B_T:
                result = detect_tp(dev);
                if (!result) { 
		  printk("%s: 10Base-T (RJ-45) has no cable\n", dev->name);
		  if (lp->auto_neg_cnf & IMM_BIT) { /* check "ignore missing media" bit */
		    printk("%s: but ignore the fact.\n", dev->name);
		    result = A_CNF_MEDIA_10B_T; /* Yes! I don't care if I see a link pulse */
		  }
                }
		break;
	case A_CNF_MEDIA_AUI:
	  printk("AUI is not supported by uCcs8900\n");
		break;
	case A_CNF_MEDIA_10B_2:
	  printk("10Base2 is not supported by uCcs8900\n");
		break;
	case A_CNF_MEDIA_AUTO:
		writereg(dev, PP_LineCTL, lp->linectl | AUTO_AUI_10BASET);
		if (lp->adapter_cnf & A_CNF_10B_T)
			if ((result = detect_tp(dev)) != 0)
				break;

		printk("%s: no media detected\n", dev->name);
                goto release_irq;
	}
	switch(result) {
	case 0: printk("%s: no network cable attached to configured media\n", dev->name);
                goto release_irq;
	case A_CNF_MEDIA_10B_T: printk("%s: using 10Base-T (RJ-45)\n", dev->name);break;
	case A_CNF_MEDIA_AUI:   printk("%s: using 10Base-5 (AUI)\n", dev->name);break;
	case A_CNF_MEDIA_10B_2: printk("%s: using 10Base-2 (BNC)\n", dev->name);break;
	default: printk("%s: unexpected result was %x\n", dev->name, result); goto release_irq;
	}

	/* Turn on both receive and transmit operations */
	writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) | SERIAL_RX_ON | SERIAL_TX_ON);

	/* Receive only error free packets addressed to this card */
	lp->rx_mode = 0;
	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT);

	lp->curr_rx_cfg = RX_OK_ENBL | RX_CRC_ERROR_ENBL;
	if (lp->isa_config & STREAM_TRANSFER)
		lp->curr_rx_cfg |= RX_STREAM_ENBL;

	writereg(dev, PP_RxCFG, lp->curr_rx_cfg);

	writereg(dev, PP_TxCFG, TX_LOST_CRS_ENBL | TX_SQE_ERROR_ENBL | TX_OK_ENBL |
	       TX_LATE_COL_ENBL | TX_JBR_ENBL | TX_ANY_COL_ENBL | TX_16_COL_ENBL);

	writereg(dev, PP_BufCFG, READY_FOR_TX_ENBL | RX_MISS_COUNT_OVRFLOW_ENBL |
		 TX_COL_COUNT_OVRFLOW_ENBL | TX_UNDERRUN_ENBL);

	/* now that we've got our act together, enable everything */
	writereg(dev, PP_BusCTL, ENABLE_IRQ );
	
	netif_start_queue(dev);
	
	return 0;
}


static int net_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

#if 1
	if (net_debug > 3) {  /*OZH*/
		printk("%s: sent %d byte packet of type %x\n",
			dev->name, skb->len,
			(skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);
	}
#endif
	/* keep the upload from being interrupted, since we
                  ask the chip to start transmitting before the
                  whole packet has been completely uploaded. */

	spin_lock_irq(&lp->lock);
	netif_stop_queue(dev);

	/* initiate a transmit sequence */
	writeword(dev, TX_CMD_PORT, lp->send_cmd);
	writeword(dev, TX_LEN_PORT, skb->len);

	/* Test to see if the chip has allocated memory for the packet */
	if ((readreg(dev, PP_BusST) & READY_FOR_TX_NOW) == 0) {
		/*
		 * Gasp!  It hasn't.  But that shouldn't happen since
		 * we're waiting for TxOk, so return 1 and requeue this packet.
		 */
		
		spin_unlock_irq(&lp->lock);
		if (net_debug) printk("cs89x0: Tx buffer not free!\n");
		return 1;
	}
	/* Write the contents of the packet */
	outsw(dev->base_addr + TX_FRAME_PORT,skb->data,(skb->len+1) >>1);
	spin_unlock_irq(&lp->lock);
	dev->trans_start = jiffies;
	dev_kfree_skb (skb);

	/*
	 * We DO NOT call netif_wake_queue() here.
	 * We also DO NOT call netif_start_queue().
	 *
	 * Either of these would cause another bottom half run through
	 * net_send_packet() before this packet has fully gone out.  That causes
	 * us to hit the "Gasp!" above and the send is rescheduled.  it runs like
	 * a dog.  We just return and wait for the Tx completion interrupt handler
	 * to restart the netdevice layer
	 */

	return 0;
}


/* The typical workload of the driver:
   Handle the network interface interrupts. */
void
cs8900_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
        struct net_device *dev = dev_id;
	struct net_local *lp;
	int ioaddr, status;
	volatile unsigned long  *icrp;

#if defined (CONFIG_BOARD_UC5272)
	/* clear INT1  */
	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	*icrp = (*icrp & 0x77777777) | 0x80000000;
#endif

	ioaddr = dev->base_addr;
	lp = (struct net_local *)dev->priv;

	/* we MUST read all the events out of the ISQ, otherwise we'll never
           get interrupted again.  As a consequence, we can't have any limit
           on the number of times we loop in the interrupt handler.  The
           hardware guarantees that eventually we'll run out of events.  Of
           course, if you're on a slow machine, and packets are arriving
           faster than you can read them off, you're screwed.  Hasta la
           vista, baby!  */
	while ((status = readword(dev, ISQ_PORT))) {
#if 1
		/* OZH */	
		if (net_debug > 4)printk("%s: event=%04x\n", dev->name, status);
#endif
		switch(status & ISQ_EVENT_MASK) {
		case ISQ_RECEIVER_EVENT:
			/* Got a packet(s). */
			net_rx(dev);
			break;
		case ISQ_TRANSMITTER_EVENT:
			lp->stats.tx_packets++;

			netif_wake_queue(dev);
			
			if ((status & TX_OK) == 0)	lp->stats.tx_errors++;
			if (status & TX_LOST_CRS) 	lp->stats.tx_carrier_errors++;
			if (status & TX_SQE_ERROR) 	lp->stats.tx_heartbeat_errors++;
			if (status & TX_LATE_COL) 	lp->stats.tx_window_errors++;
			if (status & TX_16_COL) 	lp->stats.tx_aborted_errors++;
			break;
		case ISQ_BUFFER_EVENT:
			if (status & READY_FOR_TX) {
				/* we tried to transmit a packet earlier,
                                   but inexplicably ran out of buffers.
                                   That shouldn't happen since we only ever
                                   load one packet.  Shrug.  Do the right
                                   thing anyway. */
				netif_wake_queue(dev);   
			}
			if (status & TX_UNDERRUN) {
				if (net_debug > 0) printk("%s: transmit underrun\n", dev->name);
                                lp->send_underrun++;
                                if (lp->send_underrun > 3) lp->send_cmd = TX_AFTER_ALL;
                        }
			break;
		case ISQ_RX_MISS_EVENT:
			lp->stats.rx_missed_errors += (status >>6);
			break;
		case ISQ_TX_COL_EVENT:
			lp->stats.collisions += (status >>6);
			break;
		}
	}
}

/* We have a good packet(s), get it/them out of the buffers. */
static void
net_rx(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	struct sk_buff *skb;
	int status, length;

	int ioaddr = dev->base_addr;
	status = inw(ioaddr + RX_FRAME_PORT);
	length = inw(ioaddr + RX_FRAME_PORT);

	if ((status & RX_OK) == 0) {
		lp->stats.rx_errors++;
		if (status & RX_RUNT) 		lp->stats.rx_length_errors++;
		if (status & RX_EXTRA_DATA) 	lp->stats.rx_length_errors++;
		if (status & RX_CRC_ERROR) if (!(status & (RX_EXTRA_DATA|RX_RUNT)))
			/* per str 172 */
			lp->stats.rx_crc_errors++;
		if (status & RX_DRIBBLE) 	lp->stats.rx_frame_errors++;
		return;
	}

	/* Malloc up new buffer. */
	skb = alloc_skb(length+2, GFP_ATOMIC);	/* Fixed 32bit alignment problem. OZH */
	if (skb == NULL) {
		lp->stats.rx_dropped++;
		return;
	}
	skb_reserve(skb, 2);	/* longword align L3 header */

	skb->dev = dev;
	insw(ioaddr + RX_FRAME_PORT, skb_put(skb, length), length >> 1);
	if (length & 1)
		skb->data[length-1] = inw(ioaddr + RX_FRAME_PORT);
#if 1
	if (net_debug > 3) {	/* OZH */
		printk(	"%s: received %d byte packet of type %x\n",
			dev->name, length,
			(skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);
	}
	if (net_debug == 77) {
	    int i;
	    for (i=0; i< length; i++)
	        printk ("%2.2x:", skb->data[i]);
    	    printk("\n");
	    }
#endif
        skb->protocol=eth_type_trans(skb,dev);
	netif_rx(skb);
	dev->last_rx = jiffies;
	lp->stats.rx_packets++;
	lp->stats.rx_bytes += length;
	return;
}

/* The inverse routine to net_open(). */
static int
net_close(struct net_device *dev)
{
	netif_stop_queue (dev);
	
	writereg(dev, PP_RxCFG, 0);
	writereg(dev, PP_TxCFG, 0);
	writereg(dev, PP_BufCFG, 0);
	writereg(dev, PP_BusCTL, 0);

	free_irq(dev->irq, dev);

	return 0;
}

/* Get the current statistics.	This may be called with the card open or
   closed. */
   
   
static  struct net_device_stats *
net_get_stats(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

	cli();
	/* Update the statistics from the device registers. */
	lp->stats.rx_missed_errors += (readreg(dev, PP_RxMiss) >> 6);
	lp->stats.collisions += (readreg(dev, PP_TxCol) >> 6);
	sti();

	return &lp->stats;
}

static void set_multicast_list(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

	if(dev->flags&IFF_PROMISC)
	{
		lp->rx_mode = RX_ALL_ACCEPT;
	}
	else if((dev->flags&IFF_ALLMULTI)||dev->mc_list)
	{
		/* The multicast-accept list is initialized to accept-all, and we
		   rely on higher-level filtering for now. */
		lp->rx_mode = RX_MULTCAST_ACCEPT;
	} 
	else
		lp->rx_mode = 0;

	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT | lp->rx_mode);

	/* in promiscuous mode, we accept errored packets, so we have to enable interrupts on them also */
	writereg(dev, PP_RxCFG, lp->curr_rx_cfg |
	     (lp->rx_mode == RX_ALL_ACCEPT? (RX_CRC_ERROR_ENBL|RX_RUNT_ENBL|RX_EXTRA_DATA_ENBL) : 0));
}

static int
set_mac_address(struct net_device *dev, void *addr)
{
	int i;
/* 	if (netif_running(dev)) */
/* 		return -EBUSY; */
	printk("%s: Setting MAC address to ", dev->name);
	for (i = 0; i < 6; i++)
		printk(" %2.2x", dev->dev_addr[i] = ((unsigned char *)addr)[i]);
	printk(".\n");
	/* set the Ethernet address */
	for (i=0; i < ETH_ALEN/2; i++)
		writereg(dev, PP_IA+i*2, dev->dev_addr[i*2] | (dev->dev_addr[i*2+1] << 8));

	return 0;
}

/*
 * Local variables:
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 4
 *  tab-width: 4
 * End:
 */
