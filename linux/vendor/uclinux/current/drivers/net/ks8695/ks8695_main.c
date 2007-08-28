/*
	Copyright (c) 2002, Micrel Kendin Operations

	Written 2002 by LIQUN RUAN

	This software may be used and distributed according to the terms of 
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice. This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as lruan@kendin.com
	Micrel Kendin Operations
	486 Mercury Dr.
	Sunnyvale, CA 94085

	This driver is for Kendin's KS8695 SOHO Router Chipset as ethernet driver.

	Support and updates available at
	www.kendin.com or www.micrel.com

*/
#define __KS8695_MAIN__
#include "ks8695_drv.h"
#include "ks8695_ioctrl.h"
#include "ks8695_cache.h"

/* define which external ports do what... */
#if CONFIG_MACH_LITE300
#define	LANPORT		0
#define	WANPORT		1
#define	HPNAPORT	2
#else
#define	WANPORT		0
#define	LANPORT		1
#define	HPNAPORT	2
#endif

/*#define	DEBUG_INT*/
#define	USE_TX_UNAVAIL
/*#define	PACKET_DUMP*/
#define	NO_SPINLOCK

#define	USE_FIQ

/* update dcache by ourself only if no PCI subsystem is used */
#ifndef	PCI_SUBSYSTEM
#endif

#define KS8695_MAX_INTLOOP			1
#define	WATCHDOG_TICK				3

char ks8695_driver_name[] = "ks8695 SOHO Router 10/100T Ethernet Dirver";
char ks8695_driver_string[]="Micrel KS8695 Ethernet Driver";
char ks8695_driver_version[] = "1.0.0.14";
char ks8695_copyright[] = "Copyright (c) 2002 Micrel INC.";

PADAPTER_STRUCT ks8695_adapter_list = NULL;

#ifndef	__KS8695_IOCTRL_H
/* for debugging, a build in packet checking mechanism */
enum _DEBUG_PACKET {
	DEBUG_PACKET_LEN	= 0x00000001,		/* debug packet length */
	DEBUG_PACKET_HEADER = 0x00000002,		/* debug packet header */
	DEBUG_PACKET_CONTENT= 0x00000004,		/* debug packet content */
	DEBUG_PACKET_OVSIZE = 0x00000008,		/* dump rx over sized packet content */
	DEBUG_PACKET_UNDERSIZE = 0x00000010,	/* prompt rx under sized packet */
};
#endif

static struct pci_device_id ks8695_pci_table[] = {
    /* Micrel Kendin KS8695 Gigabit Ethernet Network Driver */
	/* Generic */
    { PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    /* required last entry */
    { 0, }
};

MODULE_DEVICE_TABLE(pci, ks8695_pci_table);

#ifdef	PCI_SUBSYSTEM
static char *ks8695_strings[] = {
    "Micrel Kendin KS8695 10/100T Ethernet Network Driver",
};

static struct pci_driver ks8695_driver = {
    name	: ks8695_driver_name,
    id_table: ks8695_pci_table,
    probe	: ks8695_probe,
    remove	: ks8695_remove,
    /* Power Managment Hooks */
    suspend	: NULL,
    resume	: NULL
};
#endif

#define KS8695_MAX_NIC		3		/* 0 for WAN, 1 for LAN and 2 for HPHA */
#define	STAT_NET(x)			(Adapter->net_stats.x)

static struct pci_dev pci_dev_mimic[KS8695_MAX_NIC];
static int	pci_dev_index = 0;		/* max dev probe allowed */

#define KS8695_OPTION_INIT	{ [0 ... KS8695_MAX_NIC - 1] = OPTION_UNSET }

static int TxDescriptors[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int RxDescriptors[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int Speed[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int Duplex[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int FlowControl[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int RxChecksum[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int TxChecksum[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int TxPBL[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int RxPBL[KS8695_MAX_NIC] = KS8695_OPTION_INIT;
static int HPNA = OPTION_UNSET;
static int PowerSaving = 0;		/* default is off */
static int ICacheLockdown = 1;	/* default is on */
static int RoundBobin = 0;		/* default Icache is random */

/* if defined as module */
#ifdef MODULE

MODULE_AUTHOR("Micrel Kendin Operations, <lruan@kendin.com>");
MODULE_DESCRIPTION("Micrel Kendin KS8695 SOHO Router Ethernet Network Driver");
#ifdef	ARM_LINUX
MODULE_LICENSE("GPL");
#endif

MODULE_PARM(TxDescriptors,	"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(RxDescriptors,	"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(Speed,			"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(Duplex,			"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(FlowControl,	"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(RxChecksum,		"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(TxChecksum,		"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(TxPBL,			"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(RxPBL,			"1-" __MODULE_STRING(KS8695_MAX_NIC) "i");
MODULE_PARM(HPNA,			"1-i");
MODULE_PARM(PowerSaving,	"1-i");
MODULE_PARM(ICacheLockdown,	"1-i");
MODULE_PARM(RoundBobin,		"1-i");

EXPORT_SYMBOL(ks8695_init_module);
EXPORT_SYMBOL(ks8695_exit_module);
EXPORT_SYMBOL(ks8695_probe);
EXPORT_SYMBOL(ks8695_remove);
EXPORT_SYMBOL(ks8695_open);
EXPORT_SYMBOL(ks8695_close);
EXPORT_SYMBOL(ks8695_xmit_frame);
EXPORT_SYMBOL(ks8695_isr);
EXPORT_SYMBOL(ks8695_isr_link);
EXPORT_SYMBOL(ks8695_set_multi);
EXPORT_SYMBOL(ks8695_change_mtu);
EXPORT_SYMBOL(ks8695_set_mac);
EXPORT_SYMBOL(ks8695_get_stats);
EXPORT_SYMBOL(ks8695_watchdog);
EXPORT_SYMBOL(ks8695_ioctl);

/* for I-cache lockdown or FIQ purpose */
EXPORT_SYMBOL(ks8695_isre);

#endif

#ifndef	PCI_SUBSYSTEM
int ks8695_module_probe(void);
EXPORT_SYMBOL(ks8695_module_probe);
#endif

/*********************************************************************
 * Local Function Prototypes
 *********************************************************************/
static void CheckConfigurations(PADAPTER_STRUCT Adapter);
static int  SoftwareInit(PADAPTER_STRUCT Adapter);
static int  HardwareInit(PADAPTER_STRUCT Adapter);
static int  AllocateTxDescriptors(PADAPTER_STRUCT Adapter);
static int  AllocateRxDescriptors(PADAPTER_STRUCT Adapter);
static void FreeTxDescriptors(PADAPTER_STRUCT Adapter);
static void FreeRxDescriptors(PADAPTER_STRUCT Adapter);
static void UpdateStatsCounters(PADAPTER_STRUCT Adapter);
#if	0
static int	ProcessTxInterrupts(PADAPTER_STRUCT Adapter);
static int	ProcessRxInterrupts(PADAPTER_STRUCT Adapter);
#endif
static void ReceiveBufferFill(uintptr_t data);
static void CleanTxRing(PADAPTER_STRUCT Adapter);
static void CleanRxRing(PADAPTER_STRUCT Adapter);
static void InitTxRing(PADAPTER_STRUCT Adapter);
static void InitRxRing(PADAPTER_STRUCT Adapter);
/*static void ReceiveBufferFillEx(PADAPTER_STRUCT Adapter);*/
static int ks8695_icache_lock2(void *icache_start, void *icache_end);

/*
 * ResetDma
 *	This function is use to reset DMA in case Tx DMA was sucked due to 
 *	heavy traffic condition.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
static __inline void ResetDma(PADAPTER_STRUCT Adapter)
{
	struct net_device *netdev;
	/*BOOLEAN	bTxStarted, bRxStarted;*/
	UINT uRxReg;

#ifdef	DEBUG_THIS
	if (DMA_LAN == DI.usDMAId) {
		DRV_INFO("%s: LAN", __FUNCTION__);
	} else if (DMA_WAN == DI.usDMAId) {
		DRV_INFO("%s: WAN", __FUNCTION__);
	} else {
		DRV_INFO("%s: HPNA", __FUNCTION__);
	}
#endif

	if (!test_bit(KS8695_BOARD_OPEN, &Adapter->flags)) {
		DRV_INFO("%s: driver not opened yet", __FUNCTION__);
		return;
	}

	netdev = Adapter->netdev;
	/*bRxStarted = DI.bRxStarted;
	bTxStarted = DI.bTxStarted;
	*/
	tasklet_disable(&DI.rx_fill_tasklet);
	netif_stop_queue(netdev);

	macStopAll(Adapter);

	CleanRxRing(Adapter);
	InitRxRing(Adapter);
	CleanTxRing(Adapter);
	InitTxRing(Adapter);

	ks8695_ChipInit(Adapter, FALSE);

	KS8695_WRITE_REG(REG_INT_STATUS, DI.uIntMask);

	/* read RX mode register */
	uRxReg = KS8695_READ_REG(REG_RXCTRL + DI.nOffset);
	if (netdev->flags & IFF_PROMISC) {
		uRxReg |= DMA_PROMISCUOUS;
	}
	if (netdev->flags & (IFF_ALLMULTI | IFF_MULTICAST)) {
		uRxReg |= DMA_MULTICAST;
	}
	uRxReg |= DMA_BROADCAST;

	/* write RX mode register */
	KS8695_WRITE_REG(REG_RXCTRL + DI.nOffset, uRxReg);

	KS8695_WRITE_REG(REG_RXBASE + DI.nOffset, cpu_to_le32(DI.RxDescDMA));
	KS8695_WRITE_REG(REG_TXBASE + DI.nOffset, cpu_to_le32(DI.TxDescDMA));
	macEnableInterrupt(Adapter, TRUE);
	tasklet_enable(&DI.rx_fill_tasklet);
	tasklet_schedule(&DI.rx_fill_tasklet);
	netif_start_queue(netdev);

	/*if (bRxStarted)*/
		macStartRx(Adapter, TRUE);
	/*if (bTxStarted)*/
		macStartTx(Adapter, TRUE);
}

/*
 * ks8695_dump_packet
 *	This function is use to dump given packet for debugging.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *	data		pointer to the beginning of the packet to dump
 *	len			length of the packet
 *	flag		debug flag
 *
 * Return(s)
 *	NONE.
 */
static __inline void ks8695_dump_packet(PADAPTER_STRUCT Adapter, unsigned char *data, int len, UINT flag)
{
	/* we may need to have locking mechamism to use this function, since Rx call it within INT context
	   and Tx call it in normal context */

	if (flag && len >= 18) {
		if (flag & DEBUG_PACKET_LEN) {
			printk("Pkt Len=%d\n", len);
		}
		if (flag & DEBUG_PACKET_HEADER) {
			printk("DA=%02x:%02x:%02x:%02x:%02x:%02x\n", 
				*data, *(data + 1), *(data + 2), *(data + 3), *(data + 4), *(data + 5));
			printk("SA=%02x:%02x:%02x:%02x:%02x:%02x\n", 
				*(data + 6), *(data + 7), *(data + 8), *(data + 9), *(data + 10), *(data + 11));
			printk("Type=%04x (%d)\n", ntohs(*(unsigned short *)(data + 12)), ntohs(*(unsigned short *)(data + 12)));
		}
		if (flag & DEBUG_PACKET_CONTENT) {
			int	j = 0, k;

			/* skip DA/SA/TYPE, ETH_HLEN is defined in if_ether.h under include/linux dir */
			data += ETH_HLEN;
			/*len -= (ETH_HLEN + ETH_CRC_LENGTH);*/
			len -= ETH_HLEN;
			do {
				printk("\n %04d   ", j);
				for (k = 0; (k < 16 && len); k++, data++, len--) {
					printk("%02x  ", *data);
				}
				j += 16;
			} while (len > 0);
			/* last dump crc field */
			/*printk("\nCRC=%04x\n", ntohl(*(unsigned int *)data));*/
		}
	}
}

/*
 * ks8695_relink
 *	This function is use to setup link in case some dynamic configuration
 *	is applied via ifconfig! if driver is opened!
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	NONE.
 */
static void ks8695_relink(PADAPTER_STRUCT Adapter)
{
	if (test_bit(KS8695_BOARD_OPEN, &Adapter->flags)) {
		/* reset the flag even if it is auto nego is in progress
		   to make sure we don't miss it!!! */
		if (DMA_LAN != DI.usDMAId) {
			swDetectPhyConnection(Adapter, 0);
		}
		else {
			int	i;

			for (i = 0; i < SW_MAX_LAN_PORTS; i++) {
				swDetectPhyConnection(Adapter, i);
			}
		}
	}
}

/*
 * ks8695_report_carrier
 *	This function is use to report carrier status to net device
 *
 * Argument(s)
 *	netdev		pointer to net_device structure.
 *	carrier		carrier status (0 off, non zero on)
 *
 * Return(s)
 *	NONE.
 */
static void ks8695_report_carrier(struct net_device *netdev, int carrier)
{
#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* if link is on */
	if (carrier) {
		netif_carrier_on(netdev);
		netif_carrier_ok(netdev);
	}
	else {
		netif_carrier_off(netdev);
	}
}

#ifndef	PCI_SUBSYSTEM
/*
 * ks8695_module_probe
 *	This function is used to simulate pci's probe function.
 *
 * Argument(s)
 *	NONE.
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_module_probe(void)
{
	spinlock_t	eth_lock = SPIN_LOCK_UNLOCKED;
	unsigned long flags;
	int	nRet, nHPHA = 0;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

#ifdef	__KS8695_CACHE_H
	/*if (RoundBobin) {*/
		ks8695_icache_change_policy(RoundBobin);
	/*}*/
	if (ICacheLockdown)
		ks8695_icache_lock2(ks8695_isr, ks8695_isre);
#endif
	
	/* do we need to call ks8695_probe multiple times to register eth1 for LAN and eth2 for HPNA? */
	if (pci_dev_index >= KS8695_MAX_NIC)
		return -EINVAL;

	/* if user enabled HPNA */
	if (HPNA != OPTION_UNSET) {
		nHPHA = HPNA ? 1 : 0;
	}

	/* if allow power saving, default no power saving (wait for interrupt) */
	if (PowerSaving) {
		ks8695_enable_power_saving(PowerSaving);
	}

	nRet = 0;
	/* default WAN and LAN, plus HPNA if enabled by the user */
	for (pci_dev_index = 0; pci_dev_index < (2 + nHPHA); pci_dev_index++) {
		if (0 == pci_dev_index) {
			strcpy(pci_dev_mimic[pci_dev_index].name, "WAN Port");
			pci_dev_mimic[pci_dev_index].irq = 29;
		}
		else if (1 == pci_dev_index) {
			strcpy(pci_dev_mimic[pci_dev_index].name, "LAN Port");
			pci_dev_mimic[pci_dev_index].irq = 22;
		}
		else {
			strcpy(pci_dev_mimic[pci_dev_index].name, "HPNA Port");
			pci_dev_mimic[pci_dev_index].irq = 14;
		}
#ifdef	DEBUG_THIS
		DRV_INFO("%s: set ks8695_probe(%d)", __FUNCTION__, pci_dev_index);
#endif
		spin_lock_irqsave(&eth_lock, flags);
		/* we don't use pci id field, so set it to NULL */
		nRet = ks8695_probe(&pci_dev_mimic[pci_dev_index], NULL);
		spin_unlock_irqrestore(&eth_lock, flags);
		/* if error happened */
		if (nRet) {
			DRV_ERR("%s: ks8695_probe(%d) failed, error code = 0x%08x", __FUNCTION__, pci_dev_index, nRet);
			break;
		}
	}

	return nRet;
}

/*
 * hook_irqs
 *	This function is used to hook irqs associated to given DMA type
 *
 * Argument(s)
 *	netdev	pointer to netdev structure.
 *	req		request or free interrupts
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
static int hook_irqs(struct net_device *netdev, int req)
{
	PADAPTER_STRUCT Adapter = netdev->priv;
	int	i;

	switch (DI.usDMAId) {
	default:
	case DMA_HPNA:
	case DMA_LAN:
		break;

	case DMA_WAN:
		if (DI.uLinkIntMask & INT_WAN_LINK) {
			if (req) {
#ifndef	USE_FIQ
				if (request_irq(31, &ks8695_isr_link, SA_SHIRQ, pci_dev_mimic[Adapter->bd_number].name, netdev)) {
#else
				if (request_irq(31, &ks8695_isr_link, SA_SHIRQ | SA_INTERRUPT, pci_dev_mimic[Adapter->bd_number].name, netdev)) {
#endif
					return -EBUSY;
				}
			}
			else {
				free_irq(31, netdev);
			}
		}
		break;
	}

	/* each DMA has 6 interrupt bits associated, except WAN which has one extra, INT_WAN_LINK */
	for (i = 0; i < 6; i++) {
		if (DI.uIntMask & (1L << (DI.uIntShift + i))) {
			if (req) {
#ifndef	USE_FIQ
				if (request_irq(i + DI.uIntShift, &ks8695_isr, SA_SHIRQ, pci_dev_mimic[Adapter->bd_number].name, netdev)) {
#else
				if (request_irq(i + DI.uIntShift, &ks8695_isr, SA_SHIRQ | SA_INTERRUPT, pci_dev_mimic[Adapter->bd_number].name, netdev)) {
#endif
					return -EBUSY;
				}
			}
			else {
				free_irq(i + DI.uIntShift, netdev);
			}
		}
	}

	return 0;
}
#endif

/*
 *	Determine MAC addresses for ethernet ports.
 */
#ifdef CONFIG_MACH_LITE300
/*
 *	Ideally we want to use the MAC addresses stored in flash.
 *	But we do some sanity checks in case they are not present
 *	first.
 */
void ks8695_getmac(unsigned char *dst, int index)
{
	unsigned char dm[] = {
		0x00, 0xd0, 0xcf, 0x00, 0x00, 0x00,
	};
	unsigned char *src, *mp, *ep;
	int i;

	/* Construct a default MAC address just in case */
	dm[ETH_LENGTH_OF_ADDRESS-1] = index;
	src = &dm[0];

	ep = ioremap(0x02000000, 0x20000);
	if (ep) {
		/* Check if flash MAC is valid */
		mp = ep + 0x10000 + (index * ETH_LENGTH_OF_ADDRESS);
		for (i = 0; (i < ETH_LENGTH_OF_ADDRESS); i++) {
			if ((mp[i] != 0) && (mp[i] != 0xff)) {
				src = mp;
				break;
			}
		}
	}

	memcpy(dst, src, ETH_LENGTH_OF_ADDRESS);

	if (ep)
		iounmap(ep);
}
#else
void ks8695_getmac(unsigned char *dst, int index)
{
	static char *macs[] = {
		"\0LLAN1", "\0WWAN0", "\0HPNA2",
	};
	memcpy(dst, macs[index], ETH_LENGTH_OF_ADDRESS);
}
#endif

/*
 * ks8695_init_module
 *	This function is the first routine called when the driver is loaded.
 *
 * Argument(s)
 *	NONE.
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_init_module(void)
{
	int	nRet;

	/* Print the driver ID string and copyright notice */
	DRV_INFO("%s,  version %s, %s",	ks8695_driver_string, ks8695_driver_version, ks8695_copyright);

//	DRV_INFO(" IO Address=0x%x", IO_ADDRESS(REG_IO_BASE));

#ifdef	PCI_SUBSYSTEM
	/* register the driver with the PCI subsystem */
	nRet = pci_module_init(&ks8695_driver);
#else
	nRet = ks8695_module_probe();
#endif

	return nRet;
}

module_init(ks8695_init_module);

/*
 * ks8695_exit_module
 *	This function is called just before the driver is removed from memory.
 *
 * Argument(s)
 *	NONE.
 *
 * Return(s)
 *	NONE.
 */
void ks8695_exit_module(void)
{
#ifdef	DEBUG_THIS
#ifdef MODULE
	DRV_INFO("%s: pci_dev_index=%d", __FUNCTION__, pci_dev_index);
#else
	DRV_INFO("%s", __FUNCTION__);
#endif
#endif

#ifdef	PCI_SUBSYSTEM
	pci_unregister_driver(&ks8695_driver);
#else
	{	
		int i;

#ifdef	__KS8695_CACHE_H
		if (ICacheLockdown)
			ks8695_icache_unlock();
#endif
		for (i = pci_dev_index; i > 0; i--) {
			ks8695_remove(&pci_dev_mimic[i - 1]);
		}
		pci_dev_index = 0;
	}
#endif
}

module_exit(ks8695_exit_module);

/*
 * ks8695_probe
 *	This function initializes an adapter identified by a pci_dev
 *  structure. Note that KS8695 eval board doesn't have PCI bus at all,
 *	but the driver uses that since it was derived from a PCI based driver
 *	originally.
 *
 * Argument(s)
 *  pdev	pointer to PCI device information struct
 *  ent		pointer to PCI device ID structure (ks8695_pci_table)
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *netdev = NULL;
	PADAPTER_STRUCT Adapter;
	static int cards_found = 0;
	int	nRet;

	/* Register a new network interface and allocate private data
	   structure (ADAPTER_STRUCT) */
	netdev = init_etherdev(netdev, sizeof(ADAPTER_STRUCT));
	if (NULL == netdev) {
		DRV_ERR("init_etherdev failed");
		return -ENOMEM;
	}

	Adapter = (PADAPTER_STRUCT) netdev->priv;
	memset(Adapter, 0, sizeof(ADAPTER_STRUCT));
	Adapter->netdev = netdev;
	Adapter->pdev = pdev;

	/* chain the ADAPTER_STRUCT into the list */
	if (ks8695_adapter_list)
		ks8695_adapter_list->prev = Adapter;
	Adapter->next = ks8695_adapter_list;
	ks8695_adapter_list = Adapter;

	/* simply tell the network interface we are using this irq, but the driver
	   use more for each DMA, look for /proc/interrupts for details */
	netdev->irq = pdev->irq;

	Adapter->stDMAInfo.nBaseAddr = IO_ADDRESS(REG_IO_BASE);
    netdev->mem_start = IO_ADDRESS(REG_IO_BASE);
    netdev->mem_end = netdev->mem_start + 0xffff;

#ifdef	DEBUG_THIS
	DRV_INFO("VA = 0x%08x, PA=0x%08x", Adapter->stDMAInfo.nBaseAddr, REG_IO_BASE);
#endif

	/* set up function pointers to driver entry points */
	netdev->open               = &ks8695_open;
	netdev->stop               = &ks8695_close;
	netdev->hard_start_xmit    = &ks8695_xmit_frame;
	netdev->get_stats          = &ks8695_get_stats;
	netdev->set_multicast_list = &ks8695_set_multi;
	netdev->set_mac_address    = &ks8695_set_mac;
	netdev->change_mtu         = &ks8695_change_mtu;
	netdev->do_ioctl           = &ks8695_ioctl;
	if (DI.bTxChecksum) 
		netdev->features	   = NETIF_F_HW_CSUM;

	/* the card will tell which driver it will be */
	Adapter->bd_number = cards_found;

	if (WANPORT == cards_found) {
		/* for WAN */
		DI.usDMAId = DMA_WAN;
		DI.nOffset = 0x6000;
		DI.uIntMask = INT_WAN_MASK;
		DI.uLinkIntMask = INT_WAN_LINK;
#ifndef	USE_TX_UNAVAIL
		/* clear Tx buf unavail bit */
		DI.uIntMask &= ~BIT(28);
#else
		/* clear Tx Completed bit */
		/* if use Tx coalescing, don't disable Tx Complete bit */
		/*DI.uIntMask &= ~BIT(30);*/
#endif
		/* DMA's stop bit is a little bit different compared with KS9020, so disable them first */
		DI.uIntMask &= ~BIT(26);	
		DI.uIntMask &= ~BIT(25);
		DI.uIntShift = 25;
		/* set default mac address for WAN */
		ks8695_getmac(DI.stMacStation, cards_found);
	} else if (LANPORT == cards_found) {
		/* for LAN */
		DI.usDMAId = DMA_LAN;
		DI.nOffset = 0x8000;
		DI.uIntMask = INT_LAN_MASK;
#ifndef	USE_TX_UNAVAIL
		/* clear Tx buf unavail bit */
		DI.uIntMask &= ~BIT(15);
#else
		/* clear Tx Completed bit */
		/* if use Tx coalescing, don't disable Tx Complete bit */
		/*DI.uIntMask &= ~BIT(17);*/
#endif
		DI.uIntMask &= ~BIT(13);	
		DI.uIntMask &= ~BIT(12);

		DI.uIntShift = 12;
		ks8695_getmac(DI.stMacStation, cards_found);
	} else if (HPNAPORT == cards_found) {
		/* for HPNA */
		DI.usDMAId = DMA_HPNA;
		DI.nOffset = 0xa000;
		DI.uIntMask = INT_HPNA_MASK;
#ifndef	USE_TX_UNAVAIL
		/* clear Tx buf unavail bit */
		/* if use Tx coalescing, don't disable Tx Complete bit */
		/*DI.uIntMask &= ~BIT(21);*/
#else
		/* clear Tx Completed bit */
		DI.uIntMask &= ~BIT(23);
#endif
		DI.uIntMask &= ~BIT(19);	
		DI.uIntMask &= ~BIT(18);

		DI.uIntShift = 18;
		ks8695_getmac(DI.stMacStation, cards_found);
	} else {
		DRV_ERR("%s: card id out of range (%d)", __FUNCTION__, cards_found);
		return -ENODEV;
	}

	nRet = SoftwareInit(Adapter);
	if (nRet) {
		DRV_ERR("%s: SoftwareInit failed", __FUNCTION__);
#ifdef	PCI_SUBSYSTEM
		release_mem_region(pci_resource_start(pdev, BAR_0), pci_resource_len(pdev, BAR_0));
#endif
		ks8695_remove(pdev);
		return nRet;
	}
	CheckConfigurations(Adapter);

	/* reset spinlock */
	DI.lock = SPIN_LOCK_UNLOCKED;
	/* for debug only */
	DI.lock1 = SPIN_LOCK_UNLOCKED;
	DI.lock_refill = SPIN_LOCK_UNLOCKED;

	/* finally, we get around to setting up the hardware */
	if (HardwareInit(Adapter) < 0) {
		DRV_ERR("%s: HardwareInit failed", __FUNCTION__);
		ks8695_remove(pdev);
		return -ENODEV;
	}
	cards_found++;

	/* set LED 0 for speed */
	swSetLED(Adapter, FALSE, LED_SPEED);
	/* set LED 1 for link/activities */
	swSetLED(Adapter, TRUE, LED_LINK_ACTIVITY);

	return 0;
}

/*
 * ks8695_remove
 *	This function is called by the PCI subsystem to alert the driver
 *  that it should release a PCI device. It is called to clean up from
 *  a failure in ks8695_probe.
 *
 * Argument(s)
 *  pdev	pointer to PCI device information struct
 *
 * Return(s)
 *	NONE.
 */
void ks8695_remove(struct pci_dev *pdev)
{
	struct net_device *netdev;
	PADAPTER_STRUCT Adapter;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* find the Adapter struct that matches this PCI device */
	for (Adapter = ks8695_adapter_list; Adapter != NULL; Adapter = Adapter->next) {
		if (Adapter->pdev == pdev)
			break;
	}
	/* if no match is found */
	if (Adapter == NULL)
		return;

#ifdef	DEBUG_THIS
	DRV_INFO("%s: match found, bd_num = %d", __FUNCTION__, Adapter->bd_number);
#endif

	netdev = Adapter->netdev;

	if (test_bit(KS8695_BOARD_OPEN, &Adapter->flags))
		ks8695_close(netdev);

	/* remove from the adapter list */
	if (ks8695_adapter_list == Adapter)
		ks8695_adapter_list = Adapter->next;
	if (Adapter->next != NULL)
		Adapter->next->prev = Adapter->prev;
	if (Adapter->prev != NULL)
		Adapter->prev->next = Adapter->next;

	/* free the net_device _and_ ADAPTER_STRUCT memory */
	unregister_netdev(netdev);
	kfree(netdev);
}

/*
 * CheckConfigurations
 *	This function checks all command line paramters for valid user
 *  input. If an invalid value is given, or if no user specified
 *  value exists, a default value is used.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	NONE.
 */
static void CheckConfigurations(PADAPTER_STRUCT Adapter)
{
	int board = Adapter->bd_number, i;

#ifdef	DEBUG_THIS
	DRV_INFO("%s (board number = %d)", __FUNCTION__, board);
#endif

	/* Transmit Descriptor Count */
	if (TxDescriptors[board] == OPTION_UNSET) {
		DI.nTxDescTotal = TXDESC_DEFAULT;	/* 256 | 128 | 64(d) */
	} else if ((TxDescriptors[board] > TXDESC_MAX) &&
			   (TxDescriptors[board] < TXDESC_MIN)) {
		DRV_WARN("Invalid TxDescriptor specified (%d), using default %d",
			   TxDescriptors[board], TXDESC_DEFAULT);
		DI.nTxDescTotal = TXDESC_DEFAULT;
	} else {
		DRV_INFO("User specified TxDescriptors %d is used", TxDescriptors[board]);
		DI.nTxDescTotal = TxDescriptors[board];
	}
	/* Tx coalescing, currently can only be used if buffer unavailable bit is set */
	DI.nTransmitCoalescing = (DI.nTxDescTotal >> 3);

	/* Receive Descriptor Count */
	if (RxDescriptors[board] == OPTION_UNSET) {
		DI.nRxDescTotal = RXDESC_DEFAULT;		/* 256(d) | 128 | 64 */
	} else if ((RxDescriptors[board] > RXDESC_MAX) ||
			   (RxDescriptors[board] < RXDESC_MIN)) {
		DRV_WARN("Invalid RxDescriptor specified (%d), using default %d",
			   RxDescriptors[board], RXDESC_DEFAULT);
	} else {
		DRV_INFO("User specified RxDescriptors %d is used", RxDescriptors[board]);
		DI.nRxDescTotal = RxDescriptors[board];
	}

	/* Receive Checksum Offload Enable */
	if (RxChecksum[board] == OPTION_UNSET) {
		DI.bRxChecksum = RXCHECKSUM_DEFAULT;		/* enabled */
	} else if ((RxChecksum[board] != OPTION_ENABLED) && (RxChecksum[board] != OPTION_DISABLED)) {
		DRV_INFO("Invalid RxChecksum specified (%i), using default of %i",
			RxChecksum[board], RXCHECKSUM_DEFAULT);
		DI.bRxChecksum = RXCHECKSUM_DEFAULT;
	} else {
		DRV_INFO("Receive Checksum Offload %s",
			RxChecksum[board] == OPTION_ENABLED ? "Enabled" : "Disabled");
		DI.bRxChecksum = RxChecksum[board];
	}

	/* Transmit Checksum Offload Enable configuration */
	if (OPTION_UNSET == TxChecksum[board]) {
		DI.bTxChecksum = TXCHECKSUM_DEFAULT;		/* disabled */
	} else if ((OPTION_ENABLED != TxChecksum[board]) && (OPTION_DISABLED != TxChecksum[board])) {
		DRV_INFO("Invalid TxChecksum specified (%i), using default of %i",
			TxChecksum[board], TXCHECKSUM_DEFAULT);
		DI.bTxChecksum = TXCHECKSUM_DEFAULT;
	} else {
		DRV_INFO("Transmit Checksum Offload specified %s",
			TxChecksum[board] == OPTION_ENABLED ? "Enabled" : "Disabled");
		DI.bTxChecksum = TxChecksum[board];
	}

	/* Flow Control */
	if (FlowControl[board] == OPTION_UNSET) {
		DI.bRxFlowCtrl = FLOWCONTROL_DEFAULT;		/* enabled */
	} else if ((OPTION_ENABLED != FlowControl[board]) && (OPTION_DISABLED != FlowControl[board])) {
		DRV_INFO("Invalid FlowControl specified (%i), using default %i",
			   FlowControl[board], FLOWCONTROL_DEFAULT);
		DI.bRxFlowCtrl = FLOWCONTROL_DEFAULT;
	} else {
		DRV_INFO("Flow Control %s", FlowControl[board] == OPTION_ENABLED ?
			"Enabled" : "Disabled");
		DI.bRxFlowCtrl = FlowControl[board];
	}
	/* currently Tx control flow shares the setting of Rx control flow */
	DI.bTxFlowCtrl = DI.bRxFlowCtrl;

	/* Perform PHY PowerDown Reset instead of soft reset, the Option function can 
	   be overwritten by user later */
	DI.bPowerDownReset = TRUE;

	/* Programmable Burst Length */
	if (OPTION_UNSET == TxPBL[board]) {
		DI.byTxPBL = PBL_DEFAULT;		/* FIFO size */
	} else if ((0 != TxPBL[board]) && (1 != TxPBL[board]) && (2 != TxPBL[board]) && (4 != TxPBL[board]) &&
		(8 != TxPBL[board]) && (16 != TxPBL[board]) && (32 != TxPBL[board])) {
		DRV_INFO("Invalid TX Programmable Burst Length specified (%i), using default of %i",
			TxPBL[board], PBL_DEFAULT);
		DI.byTxPBL = PBL_DEFAULT;
	} else {
		DRV_INFO("Programmable Burst Length specified %d bytes", TxPBL[board]);
		DI.byTxPBL = TxPBL[board];
	}

	if (OPTION_UNSET == RxPBL[board]) {
		DI.byRxPBL = PBL_DEFAULT;		/* FIFO size */
	} else if ((0 != TxPBL[board]) && (1 != RxPBL[board]) && (2 != RxPBL[board]) && (4 != RxPBL[board]) &&
		(8 != RxPBL[board]) && (16 != RxPBL[board]) && (32 != RxPBL[board])) {
		DRV_INFO("Invalid TX Programmable Burst Length specified (%i), using default of %i",
			RxPBL[board], PBL_DEFAULT);
		DI.byRxPBL = PBL_DEFAULT;
	} else {
		DRV_INFO("Programmable Burst Length specified %d bytes", RxPBL[board]);
		DI.byRxPBL = RxPBL[board];
	}

	/* User speed and/or duplex options */
	if (Duplex[board] == OPTION_UNSET && Speed[board] == OPTION_UNSET) {
		DI.usCType[0] = SW_PHY_DEFAULT;
	}
	else {
		switch (Speed[board]) {
			case 10:
				if (Duplex[board])
					DI.usCType[0] = SW_PHY_10BASE_T_FD;		/* 10Base-TX Full Duplex */
				else {
					/* don't advertise flow control in half duplex case */
					if (DMA_WAN == DI.usDMAId) {
						DI.bRxFlowCtrl = FALSE;
						DI.bTxFlowCtrl = FALSE;
					}
					DI.usCType[0] = SW_PHY_10BASE_T;		/* 10Base-T Half Duplex */
				}
				break;

			case 100:
			default:
				if (Duplex[board])
					DI.usCType[0] = SW_PHY_100BASE_TX_FD;	/* 100Base-TX Full Duplex */
				else {
					/* don't advertise flow control in half duplex case */
					if (DMA_WAN == DI.usDMAId) {
						DI.bRxFlowCtrl = FALSE;
						DI.bTxFlowCtrl = FALSE;
					}
					DI.usCType[0] = SW_PHY_100BASE_TX;		/* 100Base-TX Half Duplex */
				}
				break;
		}
	}

	if (DMA_LAN == DI.usDMAId) {
		/*TEMP, currently assume all other ports share same configuration with
		  first one, will add more options for LAN ports later */
		for (i = 1; i < SW_MAX_LAN_PORTS; i++) {
			DI.usCType[i] = DI.usCType[0];
		}

		/* initialize some variables which do not have user configurable options */
		for (i = 0; i <= SW_MAX_LAN_PORTS; i++) {
			DPI[i].byCrossTalkMask = 0x1f;
			DPI[i].bySpanningTree = SW_SPANNINGTREE_ALL;
			DPI[i].byDisableSpanningTreeLearn = FALSE;
		}

		/* set default as direct mode for port 5, so no lookup table is checking */
		DI.bRxDirectMode = FALSE;
		DI.bTxRreTagMode = FALSE;

		DI.bPort5FlowCtrl = DI.bRxFlowCtrl;
		DI.bPortsFlowCtrl = DI.bRxFlowCtrl;
	}
}

/*
 * SoftwareInit
 *	This function initializes the Adapter private data structure.
 *
 * Argument(s)
 *  Adapter	pointer to ADAPTER_STRUCT structure
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
static int SoftwareInit(PADAPTER_STRUCT Adapter)
{
	struct net_device *netdev = Adapter->netdev;

	/* Initial Receive Buffer Length */
	if ((netdev->mtu + ENET_HEADER_SIZE + ETH_CRC_LENGTH) <= BUFFER_1568) {
		DI.uRxBufferLen = BUFFER_1568;	/* 0x620 */
	}
	else {
		DI.uRxBufferLen = BUFFER_2048;	/* 0x800 */
	}

	/* please update link status within watchdog routine */
	DI.bLinkChanged[0] = TRUE;
	if (DMA_LAN == DI.usDMAId) {	/* if LAN driver, 3 more ports */
		DI.bLinkChanged[1] = TRUE;
		DI.bLinkChanged[2] = TRUE;
		DI.bLinkChanged[3] = TRUE;
	}

	return 0;
}

/*
 * HardwareInit
 *	This function initializes the hardware to a configuration as specified by the
 *  Adapter structure, including mac I/F, switch engine, IRQ and others.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT structure
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
static int HardwareInit(PADAPTER_STRUCT Adapter)
{
#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* note that chip reset should only take once 
	   if three driver instances are used for WAN, LAN and HPNA respectively */
	if (!ks8695_ChipInit(Adapter, TRUE)) {
		DRV_ERR("Hardware Initialization Failed");
		return -1;
	}

	return 0;
}

/*
 * ks8695_open
 *	This function is called when a network interface is made
 *  active by the system (IFF_UP). At this point all resources needed
 *  for transmit and receive operations are allocated, the interrupt
 *  handler is registered with the OS, the watchdog timer is started,
 *  and the stack is notified when the interface is ready.
 *
 * Argument(s)
 *  netdev		pointer to net_device struct
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_open(struct net_device *netdev)
{
	PADAPTER_STRUCT Adapter = netdev->priv;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* prevent multiple opens on same driver instance */
	if (test_and_set_bit(KS8695_BOARD_OPEN, &Adapter->flags)) {
		return -EBUSY;
	}

	/* stop Tx/Rx and disable interrupt */
	macStopAll(Adapter);
	if (DMA_LAN == DI.usDMAId) {
		swEnableSwitch(Adapter, FALSE);
	}

	if (HardwareInit(Adapter) < 0) {
		clear_bit(KS8695_BOARD_OPEN, &Adapter->flags);
		return -EBUSY;
	}

	/* allocate transmit descriptors */
	if (AllocateTxDescriptors(Adapter) != 0) {
		clear_bit(KS8695_BOARD_OPEN, &Adapter->flags);
		return -ENOMEM;
	}
	/* set base address for Tx DMA */
	KS8695_WRITE_REG(REG_TXBASE + DI.nOffset, cpu_to_le32(DI.TxDescDMA));
	macStartTx(Adapter, TRUE);

	/* allocate receive descriptors and buffers */
	if (AllocateRxDescriptors(Adapter) != 0) {
		FreeTxDescriptors(Adapter);
		clear_bit(KS8695_BOARD_OPEN, &Adapter->flags);
		return -ENOMEM;
	}
	/* set base address for Rx DMA */
	KS8695_WRITE_REG(REG_RXBASE + DI.nOffset, cpu_to_le32(DI.RxDescDMA));
	macStartRx(Adapter, TRUE);

	/* hook the interrupt */
#ifdef	PCI_SUBSYSTEM
	if (request_irq(netdev->irq, &ks8695_isr, SA_SHIRQ, ks8695_driver_name, netdev) != 0) {
#else
	if (hook_irqs(netdev, TRUE)) {
#endif
		DRV_ERR("%s: hook_irqs failed", __FUNCTION__);
		clear_bit(KS8695_BOARD_OPEN, &Adapter->flags);
		FreeTxDescriptors(Adapter);
		FreeRxDescriptors(Adapter);
		return -EBUSY;
	}

	/* fill Rx ring with sk_buffs */
	tasklet_init(&DI.rx_fill_tasklet, ReceiveBufferFill, (unsigned long)Adapter);
	tasklet_schedule(&DI.rx_fill_tasklet);

	/* Set the watchdog timer for 2 seconds */
	init_timer(&Adapter->timer_id);

	Adapter->timer_id.function = &ks8695_watchdog;
	Adapter->timer_id.data = (unsigned long) netdev;
	mod_timer(&Adapter->timer_id, (jiffies + WATCHDOG_TICK * HZ));

	/* stats accumulated while down are dropped
	 * this does not clear the running total */
	swResetSNMPInfo(Adapter);

	if (DMA_LAN == DI.usDMAId) {
		swEnableSwitch(Adapter, TRUE);
	}
	macEnableInterrupt(Adapter, TRUE);

	/* clear tbusy bit */
	netif_start_queue(netdev);

	/* if defined as module */
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif

	return 0;
}

/*
 * ks8695_close
 *	This function is called when an interface is de-activated by the network 
 *	module (IFF_DOWN).
 *
 * Argument(s)
 *  netdev		pointer to net_device struct
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_close(struct net_device *netdev)
{
	PADAPTER_STRUCT Adapter = netdev->priv;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	if (!test_bit(KS8695_BOARD_OPEN, &Adapter->flags))
		return 0;

	/* stop all */
	macStopAll(Adapter);
	if (DMA_LAN == DI.usDMAId) {
		swEnableSwitch(Adapter, FALSE);
	}

	netif_stop_queue(netdev);
#ifdef	PCI_SUBSYSTEM
	free_irq(netdev->irq, netdev);
#else
	hook_irqs(netdev, FALSE);
#endif
	del_timer(&Adapter->timer_id);
	tasklet_disable(&DI.rx_fill_tasklet);

	FreeTxDescriptors(Adapter);
	FreeRxDescriptors(Adapter);

	/* if defined as module */
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif

	clear_bit(KS8695_BOARD_OPEN, &Adapter->flags);

	return 0;
}

/*
 * InitTxRing
 *	This function is used to initialize Tx descriptor ring.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	NONE
 */
void InitTxRing(PADAPTER_STRUCT Adapter)
{
	int i;
	TXDESC	*pTxDesc = DI.pTxDescriptors;
	UINT32	uPA = DI.TxDescDMA;

	for (i = 0; i < DI.nTxDescTotal - 1; i++, pTxDesc++) {
		uPA += sizeof(TXDESC);		/* pointer to next Tx Descriptor */
		pTxDesc->TxDMANextPtr = cpu_to_le32(uPA);
	}
	/* last descriptor should point back to the beginning */
	pTxDesc->TxDMANextPtr = cpu_to_le32(DI.TxDescDMA);
	pTxDesc->TxFrameControl |= cpu_to_le32(TFC_TER);
}

/*
 * AllocateTxDescriptors
 *	This function is used to allocate Tx descriptors, including allocate memory,
 *	alignment adjustment, variable initialization and so on.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
static int AllocateTxDescriptors(PADAPTER_STRUCT Adapter)
{
	int size;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* allocate data buffers for transmission */
	size = sizeof(struct ks8695_buffer) * DI.nTxDescTotal;
	DI.pTxSkb = kmalloc(size, GFP_KERNEL);
	if (DI.pTxSkb == NULL) {
		return -ENOMEM;
	}
	memset(DI.pTxSkb, 0, size);

	/* round up to nearest 4K */
	size = KS8695_ROUNDUP(DI.nTxDescTotal * sizeof(TXDESC) + DESC_ALIGNMENT, BUFFER_4K);
	DI.pTxDescriptors = consistent_alloc(GFP_KERNEL | GFP_DMA, size, &DI.TxDescDMA);
	if (NULL == DI.pTxDescriptors) {
		kfree(DI.pTxSkb);
		DI.pTxSkb = NULL;
		return -ENOMEM;
	}
#ifdef	DEBUG_THIS
    DRV_INFO("TXDESC> DataBuf=0x%08x, Descriptor=0x%08x, PA=0x%08x", (UINT)DI.pTxSkb, (UINT)DI.pTxDescriptors, (UINT)DI.TxDescDMA);
#endif
	memset(DI.pTxDescriptors, 0, size);

    atomic_set(&DI.nTxDescAvail, DI.nTxDescTotal);
    DI.nTxDescNextAvail = 0;
    DI.nTxDescUsed = 0;
	DI.nTransmitCount = 0;
	DI.nTxProcessedCount = 0;
	DI.bTxNoResource = 0;

	InitTxRing(Adapter);

    return 0;
}

/*
 * InitRxRing
 *	This function is used to initialize Rx descriptor ring.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	NONE
 */
void InitRxRing(PADAPTER_STRUCT Adapter)
{
	int i;
	RXDESC	*pRxDesc = DI.pRxDescriptors;
	UINT32	uPA = DI.RxDescDMA;

	for (i = 0; i < DI.nRxDescTotal - 1; i++, pRxDesc++) {
		uPA += sizeof(RXDESC);		/* pointer to next Rx Descriptor */
		pRxDesc->RxDMANextPtr = cpu_to_le32(uPA);
	}
	/* last descriptor should point back to the beginning */
	pRxDesc->RxDMANextPtr = cpu_to_le32(DI.RxDescDMA);
	/*pRxDesc->RxDMAFragLen |= cpu_to_le32(RFC_RER); */
	pRxDesc->RxDMAFragLen &= cpu_to_le32(~RFC_RBS_MASK);
}

/*
 * AllocateRxDescriptors
 *	This function is used to setup Rx descriptors, including allocate memory, receive SKBs
 *	alignment adjustment, variable initialization and so on.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
static int AllocateRxDescriptors(PADAPTER_STRUCT Adapter)
{
	int size;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	size = sizeof(struct ks8695_buffer) * DI.nRxDescTotal;
	DI.pRxSkb = kmalloc(size, GFP_KERNEL);
	if (DI.pRxSkb == NULL) {
		return -ENOMEM;
	}
	memset(DI.pRxSkb, 0, size);

	/* Round up to nearest 4K */
	size = KS8695_ROUNDUP(DI.nRxDescTotal * sizeof(RXDESC) + DESC_ALIGNMENT, BUFFER_4K);
	DI.pRxDescriptors = consistent_alloc(GFP_KERNEL | GFP_DMA, size, &DI.RxDescDMA);
	if (NULL == DI.pRxDescriptors) {
		kfree(DI.pRxSkb);
		DI.pRxSkb = NULL;
		return -ENOMEM;
	}

#ifdef	DEBUG_THIS
	DRV_INFO("RXDESC> DataBuf=0x%08x, Descriptor=0x%08x, PA=0x%08x", 
			(UINT)DI.pRxSkb, (UINT)DI.pRxDescriptors, (UINT)DI.RxDescDMA);
#endif

	memset(DI.pRxDescriptors, 0, size);

	DI.nRxDescNextAvail = 0;
	atomic_set(&DI.RxDescEmpty, DI.nRxDescTotal);
	DI.nRxDescNextToFill = 0;

	InitRxRing(Adapter);

    return 0;
}

/*
 * FreeTxDescriptors
 *	This function is used to free Tx resources.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	NONE.
 */
static void FreeTxDescriptors(PADAPTER_STRUCT Adapter)
{
	int size;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	CleanTxRing(Adapter);

	kfree(DI.pTxSkb);
	DI.pTxSkb = NULL;

	size = KS8695_ROUNDUP(DI.nTxDescTotal * sizeof(TXDESC) + DESC_ALIGNMENT, BUFFER_4K);
	consistent_free((void *) DI.pTxDescriptors, size, DI.TxDescDMA);
	DI.pTxDescriptors = NULL;
	DI.TxDescDMA = 0;
}

/*
 * CleanTxRing
 *	This function is used to go through Tx descriptor list and clean up any pending resources.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	NONE.
 */
static void CleanTxRing(PADAPTER_STRUCT Adapter)
{
	unsigned long size;
	TXDESC	*pTxDesc = DI.pTxDescriptors;
	int i;

	/* free pending sk_buffs if any */
	for (i = 0; i < DI.nTxDescTotal; i++, pTxDesc++) {
		if (NULL != DI.pTxSkb[i].skb) {
#ifdef	PCI_SUBSYSTEM
			pci_unmap_single(Adapter->pdev, DI.pTxSkb[i].dma,
							 DI.pTxSkb[i].length,
							 DI.pTxSkb[i].direction);
#else
			/* nothing needs to do */
#endif

			dev_kfree_skb(DI.pTxSkb[i].skb);
			DI.pTxSkb[i].skb = NULL;

			/* reset corresponding Tx Descriptor structure as well */
			pTxDesc->TxDMAFragAddr = 0;
			pTxDesc->TxOwnBit = 0;
			pTxDesc->TxFrameControl = 0;
		}
	}
	DI.nTransmitCount = 0;
	DI.nTxProcessedCount = 0;

	size = sizeof(struct ks8695_buffer) * DI.nTxDescTotal;
	memset(DI.pTxSkb, 0, size);

	size = KS8695_ROUNDUP(DI.nTxDescTotal * sizeof(TXDESC) + DESC_ALIGNMENT, BUFFER_4K);
	memset(DI.pTxDescriptors, 0, size);
	atomic_set(&DI.nTxDescAvail, DI.nTxDescTotal);
	DI.nTxDescNextAvail = 0;
	DI.nTxDescUsed = 0;

	/* for safety!!! */
	KS8695_WRITE_REG(REG_TXBASE + DI.nOffset, 0);
}

/*
 * FreeRxDescriptors
 *	This function is used to free Rx resources.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	NONE.
 */
static void FreeRxDescriptors(PADAPTER_STRUCT Adapter)
{
	int size;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	tasklet_disable(&DI.rx_fill_tasklet);

	CleanRxRing(Adapter);

	kfree(DI.pRxSkb);
	DI.pRxSkb = NULL;

	size = KS8695_ROUNDUP(DI.nRxDescTotal * sizeof(RXDESC) + DESC_ALIGNMENT, BUFFER_4K);
	consistent_free((void *) DI.pRxDescriptors, size, DI.RxDescDMA);
	DI.pRxDescriptors = NULL;
	DI.RxDescDMA = 0;
}

/*
 * CleanRxRing
 *	This function is used to go through Rx descriptor list and clean up any pending resources.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	NONE.
 */
static void CleanRxRing(PADAPTER_STRUCT Adapter)
{
	unsigned long size;
	RXDESC	*pRxDesc = DI.pRxDescriptors;
	int i;

	/* Free pending sk_buffs if any */
	for (i = 0; i < DI.nRxDescTotal; i++, pRxDesc++) {
		if (DI.pRxSkb[i].skb != NULL) {
#ifdef	PCI_SUBSYSTEM
			pci_unmap_single(Adapter->pdev, DI.pRxSkb[i].dma,
							 DI.pRxSkb[i].length,
							 DI.pRxSkb[i].direction);
#else
			/* nothing needs to do */
#endif

			dev_kfree_skb(DI.pRxSkb[i].skb);
			DI.pRxSkb[i].skb = NULL;

			/* reset corresponding Rx Descriptor structure as well */
			pRxDesc->RxFrameControl &= cpu_to_le32(~(RFC_FRAMECTRL_MASK | DESC_OWN_BIT));
			pRxDesc->RxDMAFragLen = 0;
			pRxDesc->RxDMAFragAddr = 0;
		}
	}

	size = sizeof(struct ks8695_buffer) * DI.nRxDescTotal;
	memset(DI.pRxSkb, 0, size);

	size = KS8695_ROUNDUP(DI.nRxDescTotal * sizeof(RXDESC) + DESC_ALIGNMENT, BUFFER_4K);
	memset(DI.pRxDescriptors, 0, size);
	atomic_set(&DI.RxDescEmpty, DI.nRxDescTotal);
	DI.nRxDescNextAvail = 0;
	DI.nRxDescNextToFill = 0;
	/*DI.uRx1518plus = 0;*/
	/*DI.uRxUnderSize = 0;*/

	/* for safety!!! */
	KS8695_WRITE_REG(REG_RXBASE + DI.nOffset, 0);
}

/*
 * ks8695_set_multi
 *	This function is used to set Multicast and Promiscuous mode. It is 
 *	called whenever the multicast address list or the network interface
 *	flags are updated. This routine is resposible for configuring the 
 *	hardware for proper multicast, promiscuous mode, and all-multi behavior.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *
 * Return(s)
 *	NONE.
 */
void ks8695_set_multi(struct net_device *netdev)
{
	PADAPTER_STRUCT Adapter = netdev->priv;
	uint32_t uReg;
#if	0
	uint32_t HwLowAddress, HwHighAddress;
	uint16_t HashValue, HashReg, HashBit;
	struct dev_mc_list *mc_ptr;
#endif

	BOOLEAN	bRxStarted;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	bRxStarted = DI.bRxStarted;
	if (bRxStarted)
		macStartRx(Adapter, FALSE);

	/* read RX mode register in order to set hardware filter mode */
	uReg = KS8695_READ_REG(REG_RXCTRL + DI.nOffset);
	uReg |= DMA_UNICAST | DMA_BROADCAST;
	uReg &= ~(DMA_PROMISCUOUS | DMA_MULTICAST);

	if (netdev->flags & IFF_PROMISC) {
		uReg |= DMA_PROMISCUOUS;
	}
	if (netdev->flags & (IFF_ALLMULTI | IFF_MULTICAST)) {
		uReg |= DMA_MULTICAST;
	}

	KS8695_WRITE_REG(REG_RXCTRL + DI.nOffset, uReg);

	if (bRxStarted)
		macStartRx(Adapter, TRUE);

	ks8695_relink(Adapter);
}

/*
 * ks8695_watchdog
 *	This function is a timer callback routine for updating statistics infomration.
 *
 * Argument(s)
 *  data		pointer to net_device struct
 *
 * Return(s)
 *	NONE.
 */
void ks8695_watchdog(unsigned long data)
{
	struct net_device *netdev = (struct net_device *)data;
	PADAPTER_STRUCT Adapter = netdev->priv;
	int	carrier;

	if (DMA_LAN == DI.usDMAId) {
		static int	nCheck = 0;

		if (nCheck++ > 6) {
			int	i;
			uint8_t	bLinkActive[SW_MAX_LAN_PORTS];

			nCheck = 0;
			for (i = 0; i < SW_MAX_LAN_PORTS; i++) {
				/* keep a track for previous link active state */
				bLinkActive[i] = DI.bLinkActive[i];

				carrier = swGetPhyStatus(Adapter, i);
				/* if current link state is not same as previous state, means link state changed */
				if (bLinkActive[i] != DI.bLinkActive[i]) {
					DI.bLinkChanged[i] = TRUE;
					ks8695_report_carrier(netdev, carrier);
				}
				/* note that since LAN doesn't have Interrupt bit for link status change */
				/* we have to check it to make sure if link is lost, restart it!!! */
				if (!DI.bLinkActive[i]) {
					swDetectPhyConnection(Adapter, i);
				}
			}
		}
	}
	else {
		if (!DI.bLinkActive[0]) {
			carrier = swGetPhyStatus(Adapter, 0);
			ks8695_report_carrier(netdev, carrier);
		}
		/* handling WAN DMA sucked case if any */
		/*if (DMA_WAN == DI.usDMAId) {*/
		{	/* all driver ? */
			static int	nCount = 0;

			/* if no tx resource is reported */
			if (DI.bTxNoResource) {
				nCount++;
				/* if happened 5 times (WATCHDOG_TICK seconds * 5), means most likely, the WAN Tx DMA is died,
				   reset it again */
				if (nCount > 5) {
					DI.nResetCount++;
					ResetDma(Adapter);
					DI.bTxNoResource = FALSE;
					/* wake queue will call mark_bh(NET_BH) to resume tx */
					netif_wake_queue(netdev);
					nCount = 0;
				}
			}
		}
	}
	UpdateStatsCounters(Adapter);

	/* Reset the timer */
	mod_timer(&Adapter->timer_id, jiffies + WATCHDOG_TICK * HZ);
}

/*
 * ks8695_xmit_frame
 *	This function is used to called by the stack to initiate a transmit.
 *  The out of resource condition is checked after each successful Tx
 *  so that the stack can be notified, preventing the driver from
 *  ever needing to drop a frame.  The atomic operations on
 *  nTxDescAvail are used to syncronize with the transmit
 *  interrupt processing code without the need for a spinlock.
 *
 * Argument(s)
 *  skb			buffer with frame data to transmit
 *  netdev		pointer to network interface device structure
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	PADAPTER_STRUCT Adapter = netdev->priv;
	TXDESC *pTxDesc;
	int i, len;
	char *data;
	unsigned long flags;

	len = skb->len;
	data = skb->data;
#ifdef	DEBUG_THIS
	/*DRV_INFO("%s> len=%d", __FUNCTION__, len); */
#endif

	i = DI.nTxDescNextAvail;
	pTxDesc = &DI.pTxDescriptors[i];

	DI.pTxSkb[i].skb = skb;
	DI.pTxSkb[i].length = len;
	DI.pTxSkb[i].direction = PCI_DMA_TODEVICE;
#ifdef	PCI_SUBSYSTEM
	DI.pTxSkb[i].dma = pci_map_single(Adapter->pdev, data, len, PCI_DMA_TODEVICE);
#else

	consistent_sync(data, DI.uRxBufferLen, PCI_DMA_TODEVICE);
	DI.pTxSkb[i].dma = virt_to_bus(data);

#endif	/* PCI_SUBSYSTEM */

#ifdef	PACKET_DUMP
	ks8695_dump_packet(Adapter, data, len, DI.uDebugDumpTxPkt);
#endif

	/* set DMA buffer address */
	pTxDesc->TxDMAFragAddr = cpu_to_le32(DI.pTxSkb[i].dma);

#if	0
	if (DMA_LAN == DI.usDMAId) {
		/* may need to set SPN for IGCP for LAN driver, but do it later; */
	}
#endif

	__save_flags_cli(flags);
	/* note that since we have set the last Tx descriptor back to the first to form */
	/* a ring, there is no need to keep ring end flag for performance sake */
	/* clear some bits operation for optimization!!! */
#ifndef	USE_TX_UNAVAIL
	pTxDesc->TxFrameControl = cpu_to_le32((TFC_FS | TFC_LS | TFC_IC) | (len & TFC_TBS_MASK));
#else
	if ((DI.nTransmitCount + 1) % DI.nTransmitCoalescing) {
		pTxDesc->TxFrameControl = cpu_to_le32((TFC_FS | TFC_LS) | (len & TFC_TBS_MASK));
	}
	else {
		pTxDesc->TxFrameControl = cpu_to_le32((TFC_FS | TFC_LS | TFC_IC) | (len & TFC_TBS_MASK));
	}
#endif

	/* set own bit */
	pTxDesc->TxOwnBit = cpu_to_le32(DESC_OWN_BIT);

	/* eanble read transfer for the packet!!! */
	KS8695_WRITE_REG(REG_TXSTART + DI.nOffset, 1);

	/*atomic_dec(&DI.nTxDescAvail);*/
	/*__save_flags_cli(flags);*/
	DI.nTxDescAvail.counter--;
	/* update pending transimt packet count */
	DI.nTransmitCount++;
	__restore_flags(flags);
	if (atomic_read(&DI.nTxDescAvail) <= 1) {
#ifdef	DEBUG_THIS
		if (DMA_WAN == DI.usDMAId)
			DRV_WARN("%s> no WAN tx descriptors available, tx suspended, nTransmitCount=%d", __FUNCTION__, DI.nTransmitCount);
		else if (DMA_LAN == DI.usDMAId)
			DRV_WARN("%s> no LAN tx descriptors available, tx suspended, nTransmitCount=%d", __FUNCTION__, DI.nTransmitCount);
		else
			DRV_WARN("%s> no HPNA tx descriptors available, tx suspended, nTransmitCount=%d", __FUNCTION__, DI.nTransmitCount);
#endif
		DI.bTxNoResource = TRUE;
		netif_stop_queue(netdev);
	}

	/* adv to next available descriptor */
	DI.nTxDescNextAvail = ++DI.nTxDescNextAvail % DI.nTxDescTotal;
	netdev->trans_start = jiffies;

#ifdef	DEBUG_INT
#if	0
	/* for debug only */
	if (atomic_read(&DI.nTxDescAvail) + DI.nTransmitCount != DI.nTxDescTotal) {
		DRV_WARN("%s> inconsistency, nTransmitCount=%d, nTxDescAvail=%d", __FUNCTION__, DI.nTransmitCount, atomic_read(&DI.nTxDescAvail));
	}
#endif
#endif

	return 0;
}

/*
 * ks8695_get_stats
 *	This function is used to get NIC's SNMP staticstics.
 *
 * Argument(s)
 *  netdev		network interface device structure
 *
 * Return(s)
 *	pointer to net_device_stats structure
 */
struct net_device_stats *ks8695_get_stats(struct net_device *netdev)
{
    PADAPTER_STRUCT Adapter = netdev->priv;

#ifdef	DEBUG_THIS
    DRV_INFO("ks8695_get_stats");
#endif

    return &Adapter->net_stats;
}

/*
 * ks8695_change_mtu
 *	This function is use to change the Maximum Transfer Unit.
 *
 * Argument(s)
 *	netdev		pointer to net_device structure.
 *	new_mtu		new_mtu new value for maximum frame size
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_change_mtu(struct net_device *netdev, int new_mtu)
{
	PADAPTER_STRUCT Adapter = netdev->priv;
	uint32_t old_mtu = DI.uRxBufferLen;

	DRV_INFO("%s", __FUNCTION__);

	if (new_mtu <= DI.uRxBufferLen) {
		netdev->mtu = new_mtu;
		return 0;
	}

	if ((new_mtu < MINIMUM_ETHERNET_PACKET_SIZE - ENET_HEADER_SIZE) ||
		new_mtu > BUFFER_2048 - ENET_HEADER_SIZE) {
		DRV_ERR("%s> Invalid MTU setting", __FUNCTION__);
		return -EINVAL;
	}

	if (new_mtu <= BUFFER_1568 - ENET_HEADER_SIZE) {
		DI.uRxBufferLen = BUFFER_1568;
	} else {
		DI.uRxBufferLen = BUFFER_2048;
	}

	if (old_mtu != DI.uRxBufferLen) {
		/* put DEBUG_THIS after verification please */
		DRV_INFO("%s, old=%d, new=%d", __FUNCTION__, old_mtu, DI.uRxBufferLen);
		ResetDma(Adapter);
	}

	netdev->mtu = new_mtu;
	ks8695_relink(Adapter);

	return 0;
}

/*
 * ks8695_set_mac
 *	This function is use to change Ethernet Address of the NIC.
 *
 * Argument(s)
 *	netdev		pointer to net_device structure.
 *	p			pointer to sockaddr structure
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_set_mac(struct net_device *netdev, void *p)
{
	PADAPTER_STRUCT Adapter = netdev->priv;
	struct sockaddr *addr = (struct sockaddr *)p;
	BOOLEAN	bRxStarted, bTxStarted;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	bRxStarted = DI.bRxStarted;
	bTxStarted = DI.bTxStarted;
	if (bRxStarted)
		macStartRx(Adapter, FALSE);
	if (bTxStarted)
		macStartTx(Adapter, FALSE);

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(DI.stMacCurrent, addr->sa_data, netdev->addr_len);
	macSetStationAddress(Adapter, DI.stMacCurrent);

	if (bRxStarted)
		macStartRx(Adapter, TRUE);
	if (bTxStarted)
		macStartTx(Adapter, TRUE);

	ks8695_relink(Adapter);

	return 0;
}

/*
 * UpdateStatsCounters
 *	This function is used to update the board statistics counters.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure
 *
 * Return(s)
 *	NONE
 */
static void UpdateStatsCounters(PADAPTER_STRUCT Adapter)
{
	struct net_device_stats *stats;
	
	stats= &Adapter->net_stats;
}

/*
 * CheckState
 *	This function is used to handle error conditions if any.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure
 *	uISR		bit values of ISR register
 *
 * Return(s)
 *	NONE.
 */
static __inline void CheckState(PADAPTER_STRUCT Adapter, UINT32 uISR)
{
	BOOLEAN	bTxStopped = FALSE, bRxStopped = FALSE;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* clear all bits other than stop */
	uISR &= (DI.uIntMask & INT_DMA_STOP_MASK);
	switch (DI.usDMAId) {
	case DMA_HPNA:
		if (uISR & INT_HPNA_TX_STOPPED)
			bTxStopped = TRUE;
		if (uISR & INT_HPNA_RX_STOPPED)
			bRxStopped = TRUE;
		break;

	case DMA_LAN:
		if (uISR & INT_LAN_TX_STOPPED)
			bTxStopped = TRUE;
		if (uISR & INT_LAN_RX_STOPPED)
			bRxStopped = TRUE;
		break;

	default:
	case DMA_WAN:
		if (uISR & INT_WAN_TX_STOPPED)
			bTxStopped = TRUE;
		if (uISR & INT_WAN_RX_STOPPED)
			bRxStopped = TRUE;
		break;
	}

	if (bRxStopped) {
		/* if Rx started already, then it is a problem! */
		if (DI.bRxStarted) {
			DRV_WARN("%s> RX stopped, ISR=0x%08x", __FUNCTION__, uISR);

			macStartRx(Adapter, FALSE);
			DelayInMilliseconds(2);
			macStartRx(Adapter, TRUE);
		}
		else {
			/* ACK and clear the bit */
			KS8695_WRITE_REG(REG_INT_STATUS, uISR);
		}
	}
	if (bTxStopped) {
		/* if Tx started already, then it is a problem! */
		if (DI.bTxStarted) {
			DRV_WARN("%s> TX stopped, ISR=0x%08x", __FUNCTION__, uISR);

			macStartTx(Adapter, FALSE);
			DelayInMilliseconds(2);
			macStartTx(Adapter, TRUE);
		}
		else {
			/* ACK and clear the bit */
			KS8695_WRITE_REG(REG_INT_STATUS, uISR);
		}
	}
}

/*
 * CheckLinkState
 *	This function is used to check link status to see whether link has changed or not.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *	uISR		ISR register (should be IMSR) to check
 *
 * Return(s)
 *	TRUE	if link change has detected
 *	FALSE	otherwise
 */
static __inline BOOLEAN CheckLinkState(PADAPTER_STRUCT Adapter, UINT uISR)
{
	BOOLEAN	bLinkChanged = FALSE;
	int	i;

	switch (DI.usDMAId) {
	case DMA_HPNA:
		/* what to do? */
		return FALSE;

	case DMA_WAN:
		if (uISR & INT_WAN_LINK) {
			bLinkChanged = TRUE;
			DI.bLinkChanged[0] = TRUE;
		}
		break;

	default:
	case DMA_LAN:
		for (i = 0; i < SW_MAX_LAN_PORTS; i++) {
			if (FALSE == DI.bLinkChanged[i]) {
				UINT	uReg = KS8695_READ_REG(REG_SWITCH_AUTO0 + (i >> 1));
				if (0 == (i % 2))
					uReg >>= 16;
				if (!(uReg & SW_AUTONEGO_STAT_LINK)) {
					bLinkChanged = TRUE;
					DI.bLinkChanged[i] = TRUE;
				}
			}
		}
		break;
	}

	return bLinkChanged;
}

/*
 * ProcessTxInterrupts
 *	This function is use to process Tx interrupt, reclaim resources after 
 *	transmit completes.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	how many number of Tx packets are not processed yet.
 */
static int __inline ProcessTxInterrupts(PADAPTER_STRUCT Adapter)
{
	int i;
	TXDESC *TransmitDescriptor;
	unsigned long flags;

#ifdef	DEBUG_INT
	/*DRV_INFO("> %d", DI.nTransmitCount);*/
#endif

	i = DI.nTxDescUsed;
	TransmitDescriptor = &DI.pTxDescriptors[i];
	while (!(le32_to_cpu(TransmitDescriptor->TxOwnBit) & DESC_OWN_BIT) && DI.nTransmitCount > 0) {
#ifdef	DEBUG_INT
		if (!(le32_to_cpu(TransmitDescriptor->TxFrameControl) & (TFC_FS | TFC_FS))) {
			DRV_WARN("%s> frame ctrl is not consistent (ctrl=%x, ISR=%08x, skb=%x)", __FUNCTION__, le32_to_cpu(TransmitDescriptor->TxFrameControl), KS8695_READ_REG(REG_INT_STATUS), (UINT)DI.pTxSkb[i].skb);
			DRV_WARN("nTransmitCount=%d, nTxDescAvail=%d, nTxDescNextAvail=%d, nTxDescUsed=%d, nTxProcessedCount=%u", 
				DI.nTransmitCount, atomic_read(&DI.nTxDescAvail), DI.nTxDescNextAvail, DI.nTxDescUsed, DI.nTxProcessedCount);
			break;
		}
#endif

#ifdef	PCI_SUBSYSTEM
		pci_unmap_single(Adapter->pdev, DI.pTxSkb[i].dma,
						 DI.pTxSkb[i].length,
						 DI.pTxSkb[i].direction);
#else
#endif

		/* note that WAN DMA doesn't have statistics counters associated with,
		   therefore use local variables to track them instead */
		STAT_NET(tx_packets)++;
		STAT_NET(tx_bytes) += DI.pTxSkb[i].length;

#ifdef	DEBUG_INT
		if (0 == DI.pTxSkb[i].skb) {
			DRV_WARN("%s> Oops, null skb detected, len=%d", __FUNCTION__, (int)DI.pTxSkb[i].length);
			DRV_WARN(">nTransmitCount=%d, nTxDescAvail=%d, nTxDescNextAvail=%d, nTxProcessedCount=%u", 
				DI.nTransmitCount, atomic_read(&DI.nTxDescAvail), DI.nTxDescNextAvail, DI.nTxProcessedCount);
		}
		else
#endif
			dev_kfree_skb_irq(DI.pTxSkb[i].skb);

		DI.pTxSkb[i].skb = NULL;

		/*atomic_inc(&DI.nTxDescAvail);*/
		/*DI.nTransmitCount--;*/
		__save_flags_cli(flags);
		DI.nTxDescAvail.counter++;
		DI.nTransmitCount--;
		__restore_flags(flags);
		
		/* clear corresponding fields */
		TransmitDescriptor->TxDMAFragAddr = 0;
		/* clear all related bits, including len field, control bits and port bits */
		/*TransmitDescriptor->TxFrameControl &= cpu_to_le32(~(TFC_FRAMECTRL_MASK));*/
		TransmitDescriptor->TxFrameControl = 0;

		/* to next Tx descriptor */
		i = (i+1) % DI.nTxDescTotal;
		TransmitDescriptor = &DI.pTxDescriptors[i];
		DI.nTxProcessedCount++;
	}
	DI.nTxDescUsed = i;

	if (DI.bTxNoResource && netif_queue_stopped(Adapter->netdev) &&
	   (atomic_read(&DI.nTxDescAvail) >	((DI.nTxDescTotal * 3) >> 2))) {	/* 3/4 */
		DI.bTxNoResource = FALSE;
		netif_wake_queue(Adapter->netdev);
#ifdef	DEBUG_THIS
		DRV_INFO("%s> Tx process resumed", __FUNCTION__);
#endif
	}

	return DI.nTransmitCount;
}

/*
 * ProcessRxInterrupts
 *	This function is use to process Rx interrupt, send received data up
 *	the network stack. 
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	how many Rx packets are processed.
 */
static int __inline ProcessRxInterrupts(PADAPTER_STRUCT Adapter)
{
	RXDESC *CurrentDescriptor;
	int i, nProcessed = 0;
	uint32_t Length;
	uint32_t uFrameCtrl;
	struct sk_buff *skb;
	BOOLEAN	bGood;

#ifdef	DEBUG_INT
		/*DRV_INFO("%s", __FUNCTION__); */
#endif

	i = DI.nRxDescNextAvail;
	CurrentDescriptor = &DI.pRxDescriptors[i];

	while (!((uFrameCtrl = le32_to_cpu(CurrentDescriptor->RxFrameControl)) & DESC_OWN_BIT)) {
#ifdef	DEBUG_INT
		/*DRV_INFO("%s", __FUNCTION__); */
		/* for debug only */
		if (atomic_read(&DI.RxDescEmpty) >= DI.nRxDescTotal) {
			DRV_WARN("NoRxBuffer");
			STAT_NET(rx_missed_errors)++;
			break;
		}
#endif

		skb = DI.pRxSkb[i].skb;
		/* if it is a valid Rx descriptor, the skb should point to a skb buffer!!!
		   otherwise, most likely Rx was stressed by application in small packet size, 64 e.g.,
		   like SmartBit and no buffer is available at the moment, it is OK, not an error */
		if (NULL == skb) {
			/*DRV_INFO("Rx null skb pointer");*/
			break;
		}

		/* length with CRC bytes included */
		Length = (uFrameCtrl & RFC_FL_MASK);
		/* test both bits to make sure single packet */
		if ((uFrameCtrl & (RFC_LS | RFC_FS)) != (RFC_LS | RFC_FS)) {
			DRV_INFO("%s> spanning packet detected (framectrl=0x%08x, rx desc index=%d)", __FUNCTION__, uFrameCtrl, i);
			if (uFrameCtrl & RFC_FS) {
				/* first segment */
				Length = DI.uRxBufferLen;
				DRV_INFO(" first segment, len=%d", Length);
				/* compensite offset CRC */
				Length += ETH_CRC_LENGTH;
			}
			else if (uFrameCtrl & RFC_LS) {
				/* last segment */
				if (Length > DI.uRxBufferLen + ETH_CRC_LENGTH) {
					Length -= DI.uRxBufferLen;
					DRV_INFO(" last segment, len=%d", Length);
				}
				else {
					DRV_WARN("%s> under size packet (len=%d, buffer=%d)", __FUNCTION__, Length, DI.uRxBufferLen);
					STAT_NET(rx_errors)++;
					bGood = FALSE;
					goto CLEAN_UP;
				}
			}
			else {
				if (0 == uFrameCtrl) {
					/* race condition ? */
					DRV_WARN("FragLen=0x%08x, FragAddr=0x%08x, RxNextPtr=0x%08x, RxDescEmpty=%d, pkt dropped", 
						CurrentDescriptor->RxDMAFragLen, CurrentDescriptor->RxDMAFragAddr, CurrentDescriptor->RxDMANextPtr, atomic_read(&DI.RxDescEmpty));
/*#ifdef	PACKET_DUMP*/
					ks8695_dump_packet(Adapter, skb->data, DI.uRxBufferLen, DEBUG_PACKET_LEN | DEBUG_PACKET_HEADER | DEBUG_PACKET_CONTENT);
/*#endif*/
				}
				else {
					DRV_WARN("%s> error spanning packet, dropped", __FUNCTION__);
				}
				STAT_NET(rx_errors)++;
				bGood = FALSE;
				goto CLEAN_UP;
			}
		}
		/* if error happened!!! */
		if (uFrameCtrl & (RFC_ES | RFC_RE)) {
			DRV_WARN("%s> error found (framectrl=0x%08x)", __FUNCTION__, uFrameCtrl);
			STAT_NET(rx_errors)++;
			if (uFrameCtrl & RFC_TL) {
				STAT_NET(rx_length_errors)++;
			}
			if (uFrameCtrl & RFC_CRC) {
				STAT_NET(rx_crc_errors)++;
			}
			if (uFrameCtrl & RFC_RF) {
				STAT_NET(rx_length_errors)++;
			}
			/* if errors other than ES happened!!! */
			if (uFrameCtrl & RFC_RE) {
				DRV_WARN("%s> RFC_RE (MII) (framectrl=0x%08x)", __FUNCTION__, uFrameCtrl);
				STAT_NET(rx_errors)++;
			}
			/* RLQ, 11/07/2002, added more check to IP/TCP/UDP checksum errors */
			if (uFrameCtrl | (RFC_IPE | RFC_TCPE | RFC_UDPE)) {
				STAT_NET(rx_errors)++;
			}
			bGood = FALSE;
			goto CLEAN_UP;
		}

		/* for debug purpose */
		if (Length > 1518) {
			DI.uRx1518plus++;
			/* note that printout affects the performance figure quite lots, so don't display
			   it when conducting performance test, like Chariot */
			if (DI.uDebugDumpRxPkt & DEBUG_PACKET_OVSIZE) {
				DRV_INFO("%s> oversize pkt, size=%d, RxDesc=%d", __FUNCTION__, Length, i);
			}
			/* do early drop */
			STAT_NET(rx_errors)++;
			bGood = FALSE;
			goto CLEAN_UP;
		}

		/* for debug purpose */
		if (Length < 64) {
			DI.uRxUnderSize++;
			/* note that printout affects the performance figure quite lots, so don't display
			   it when conducting performance test, like Chariot */
			if (DI.uDebugDumpRxPkt & DEBUG_PACKET_UNDERSIZE) {
				DRV_INFO("%s> under pkt, size=%d, RxDesc=%d", __FUNCTION__, Length, i);
			}
			/* do early drop */
			STAT_NET(rx_errors)++;
			bGood = FALSE;
			goto CLEAN_UP;
		}

		/* if we are here, means a valid packet received!!! Get length of the pacekt */
		bGood = TRUE;

		/* offset CRC bytes! */
		Length -= ETH_CRC_LENGTH;

		skb_put(skb, Length);

		/* check and set Rx Checksum Offload flag */
		if (DI.bRxChecksum) {
			/* tell upper edge that the driver handles it already! */
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		}
		else {
			skb->ip_summed = CHECKSUM_NONE;
		}
		
CLEAN_UP:
#ifdef	PCI_SUBSYSTEM
		pci_unmap_single(Adapter->pdev, DI.pRxSkb[i].dma,
						 DI.pRxSkb[i].length,
						 DI.pRxSkb[i].direction);
#else
		/* to do something ? */
		/*consistent_sync(skb->data, DI.uRxBufferLen, PCI_DMA_FROMDEVICE);*/
#endif

#ifdef	PACKET_DUMP
		/* build in debug mechanism */
		ks8695_dump_packet(Adapter, skb->data, Length, DI.uDebugDumpRxPkt);
#endif

		if (bGood) {
			skb->protocol = eth_type_trans(skb, Adapter->netdev);
			netif_rx(skb);
			nProcessed++;
			/* note that WAN DMA doesn't have statistics counters associated with,
			   therefore use local variables to track them instead */
			STAT_NET(rx_packets)++;
			STAT_NET(rx_bytes) += Length;
			if (uFrameCtrl & RFC_MF)
				STAT_NET(multicast)++;
		}
		else {
			DRV_WARN("%s> error pkt (framectrl=0x%08x, RxDescEmpty=%d)", __FUNCTION__, uFrameCtrl, atomic_read(&DI.RxDescEmpty));

			/* bad packet!!! Discard it!!! */
			dev_kfree_skb_irq(skb);
			Adapter->net_stats.rx_frame_errors++;
			Adapter->netdev->last_rx = jiffies;
		}

		DI.pRxSkb[i].skb = NULL;
		/*CurrentDescriptor->RxFrameControl &= cpu_to_le32(~RFC_FRAMECTRL_MASK);
		CurrentDescriptor->RxDMAFragLen &= cpu_to_le32(~RFC_RBS_MASK);*/
		CurrentDescriptor->RxFrameControl = 0;
		CurrentDescriptor->RxDMAFragLen = 0;
		CurrentDescriptor->RxDMAFragAddr = 0;

		atomic_inc(&DI.RxDescEmpty);

		i = ((i+1) % DI.nRxDescTotal);
		CurrentDescriptor = &DI.pRxDescriptors[i];
		/*RLQ, do it in sync manner, this takes more CPU time */
		/*ReceiveBufferFillEx(Adapter);*/
		if (nProcessed >= DI.nRxDescTotal >> 1) {
			break;
		}
	}

	/* if filled Rx descriptors are less than 3/4 full, refill more sk_buffs */
	if (atomic_read(&DI.RxDescEmpty) > (DI.nRxDescTotal >> 2)) {
#ifdef	DEBUG_INT
		/*DRV_INFO("< tasklet_schedule called");*/
#endif
		tasklet_schedule(&DI.rx_fill_tasklet);
	}

	DI.nRxDescNextAvail = i;

#ifdef	DEBUG_INT
	/*DRV_INFO("< %d", nProcessed);*/
	/* for performance tuning */
	if (nProcessed > DI.nMaxProcessedCount)
		DI.nMaxProcessedCount = nProcessed;
#endif

	return nProcessed;
}

/*
 * ks8695_isr
 *	This function is the Interrupt Service Routine.
 *
 * Argument(s)
 *	irq		interrupt number
 *	data	pointer to net_device structure
 *	regs	pointer to pt_regs structure
 *
 * Return(s)
 *	NONE.
 */
void ks8695_isr(int irq, void *data, struct pt_regs *regs)
{
	BOOLEAN bMoreData = TRUE;
	PADAPTER_STRUCT Adapter = ((struct net_device *)data)->priv;
	uint32_t uISR;
	uint32_t ProcessCount = KS8695_MAX_INTLOOP;

#ifndef	NO_SPINLOCK
	/* for debug only */
	spin_lock(&DI.lock1);
#endif

	KS8695_WRITE_REG(REG_INT_ENABLE, KS8695_READ_REG(REG_INT_ENABLE) & ~DI.uIntMask);

#ifdef	DEBUG_INT
	/*DRV_INFO("%d", irq);*/
#endif

	while (ProcessCount-- > 0 && bMoreData) {
		uISR = KS8695_READ_REG(REG_INT_STATUS) & DI.uIntMask;	/* take out others if any */
		if (0 == uISR) break;

		/* ACK, HW guy asked to put it in the front, but it will hang in speed 100 using SmartBit */
		/*KS8695_WRITE_REG(REG_INT_STATUS, uISR);*/

		/* call ProcessRxInterrupts only if an interrupt is generated */ 
		if ((uISR >> DI.uIntShift) & INT_RX_BIT)
			bMoreData = ProcessRxInterrupts(Adapter);
#ifndef	USE_TX_UNAVAIL
		if ((uISR >> DI.uIntShift) & INT_TX_BIT) {
#else
		/*if ((uISR >> DI.uIntShift) & INT_TX_UNAVAIL_BIT) {*/
		/*if ((uISR >> DI.uIntShift) & (INT_TX_BIT | INT_TX_UNAVAIL_BIT)) {*/
		if (DI.nTransmitCount) {
#endif
			if (ProcessTxInterrupts(Adapter)) {
				bMoreData = TRUE;
			}
		}
		/* ACK */
		KS8695_WRITE_REG(REG_INT_STATUS, uISR);
		if ((uISR >> DI.uIntShift) & INT_RX_UNAVAIL_BIT) {
			tasklet_schedule(&DI.rx_fill_tasklet);
			break;
		}
	}

	/*CheckState(Adapter, uISR);*/
	/* Restore Previous Interrupt Settings */
	KS8695_WRITE_REG(REG_INT_ENABLE, KS8695_READ_REG(REG_INT_ENABLE) | DI.uIntMask);
#ifdef	DEBUG_INT
#if	0
	if (DMA_WAN == DI.usDMAId) {
		printk("O ");
	} else {
		printk("o ");
	}
#endif
#endif
	
#ifndef	NO_SPINLOCK
	/* for debug only */
	spin_unlock(&DI.lock1);
#endif
}

/*
 * ks8695_isre
 * for I-cache lockdown or FIQ purpose. Make sure this function is after ks8695_isr immediately.
 *
 * Argument(s)
 *	NONE.
 *
 * Return(s)
 *	NONE.
 */
void ks8695_isre(void)
{
	/* just for the end of ks8695_isr routine, unless we find a way to define end of function
	   within ks8695_isr itself */
}

/*
 * ks8695_isr_link
 *	This function is to process WAN link change interrupt as a special case
 *
 * Argument(s)
 *	irq		interrupt number
 *	data	pointer to net_device structure
 *	regs	pointer to pt_regs structure
 *
 * Return(s)
 *	NONE.
 */
void ks8695_isr_link(int irq, void *data, struct pt_regs *regs)
{
	PADAPTER_STRUCT Adapter = ((struct net_device *)data)->priv;
	UINT	uIER;

#ifdef	DEBUG_INT
	/*DRV_INFO("WAN Link changed");*/
#endif

	spin_lock(&DI.lock);
    uIER = KS8695_READ_REG(REG_INT_ENABLE) & ~INT_WAN_LINK;
	KS8695_WRITE_REG(REG_INT_ENABLE, uIER);
	spin_unlock(&DI.lock);

	DI.nLinkChangeCount++;
	DI.bLinkChanged[0] = TRUE;

	/* start auto nego only when link is down */
	if (!swGetWANLinkStatus(Adapter)) {
		swPhyReset(Adapter, 0);
		swAutoNegoAdvertisement(Adapter, 0);
		swDetectPhyConnection(Adapter, 0);
	}

	/* ACK */
	KS8695_WRITE_REG(REG_INT_STATUS, INT_WAN_LINK);
	spin_lock(&DI.lock);
    uIER = KS8695_READ_REG(REG_INT_ENABLE) | INT_WAN_LINK;
	KS8695_WRITE_REG(REG_INT_ENABLE, uIER);
	spin_unlock(&DI.lock);
}

/*
 * ReceiveBufferFill
 *	This function is use to replace used receive buffers with new SBBs
 *	to Rx descriptors.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	NONE.
 */
static void ReceiveBufferFill(uintptr_t data)
{
	PADAPTER_STRUCT Adapter = (PADAPTER_STRUCT) data;
	RXDESC *CurrentDescriptor;
	struct sk_buff *skb;
	unsigned long flags;
	int i;
#ifdef	DEBUG_INT
	int	nFilled = 0;
#endif

#ifdef	DEBUG_INT
#if	0
	if (DMA_WAN == DI.usDMAId) {
		printk("F ");
	} else {
		printk("f ");
	}
#endif
#endif

	if (!test_bit(KS8695_BOARD_OPEN, &Adapter->flags))
		return;

#ifndef	NO_SPINLOCK
	spin_lock(&DI.lock_refill);
#endif

	__save_flags_cli(flags);
	i = DI.nRxDescNextToFill;
	/* do we need lock here as well ? */

	while (NULL == DI.pRxSkb[i].skb) {
		CurrentDescriptor = &DI.pRxDescriptors[i];
#ifdef	DEBUG_INT
		/* for debug only, remove it after verified!!! */
		if (le32_to_cpu(CurrentDescriptor->RxFrameControl) & DESC_OWN_BIT) {
			DRV_WARN("%s> Rx descriptor own bit is not reset", __FUNCTION__);
		}
#endif

		skb = alloc_skb(DI.uRxBufferLen, GFP_ATOMIC | GFP_DMA);
		if (NULL == skb) {
			/* keep re-trying when failures occur */
			tasklet_schedule(&DI.rx_fill_tasklet);
			/*DRV_WARN("%s> alloc_skb failed, refill rescheduled again", __FUNCTION__);*/
			break;
		}

		skb->dev = Adapter->netdev;
		/*RLQ, 08/21/2002, move it closer to descriptor assignment */
		/*DI.pRxSkb[i].skb = skb;*/
		DI.pRxSkb[i].length = DI.uRxBufferLen;
		DI.pRxSkb[i].direction = PCI_DMA_FROMDEVICE;
#ifdef	PCI_SUBSYSTEM
		DI.pRxSkb[i].dma = pci_map_single(Adapter->pdev, skb->data,
												DI.uRxBufferLen,
												PCI_DMA_FROMDEVICE);
#else
		consistent_sync(skb->data, DI.uRxBufferLen, PCI_DMA_FROMDEVICE);
		DI.pRxSkb[i].dma = virt_to_bus(skb->data);
#endif

		/* to avoid race problem, to make the change of these variables atomic */
		__save_flags_cli(flags);
		DI.pRxSkb[i].skb = skb;
		/* setup Rx descriptor!!! */
		CurrentDescriptor->RxDMAFragAddr = cpu_to_le32(DI.pRxSkb[i].dma);
		CurrentDescriptor->RxDMAFragLen = cpu_to_le32(DI.uRxBufferLen);
		CurrentDescriptor->RxFrameControl |= cpu_to_le32(DESC_OWN_BIT);

		/*atomic_dec(&DI.RxDescEmpty);*/
		DI.RxDescEmpty.counter--;
		/*__restore_flags(flags);*/

		i = (i+1) % DI.nRxDescTotal;
#ifdef	DEBUG_INT
		nFilled++;
		if (nFilled > DI.nMaxFilledCount) {
			DI.nMaxFilledCount = nFilled;
			/* need to reset it since first time, it could be max */
			if (DI.nMaxFilledCount == DI.nRxDescTotal) {
				DI.nMaxFilledCount = 0;
			}
		}
#endif
	}
	DI.nRxDescNextToFill = i;
	__restore_flags(flags);
	
#ifndef	NO_SPINLOCK
	spin_unlock(&DI.lock_refill);
#endif

#ifdef	DEBUG_INT
	/*DRV_INFO("filled< %d", nFilled);*/
#endif

	/* enable Rx engine!!! */
	KS8695_WRITE_REG(REG_RXSTART + DI.nOffset, 1);
}

#if	0
/*
 * ReceiveBufferFillEx
 *	This function is use to replace used receive buffers with new SBBs
 *	to Rx descriptors.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	NONE.
 */
static void ReceiveBufferFillEx(PADAPTER_STRUCT Adapter)
{
	RXDESC *CurrentDescriptor;
	struct sk_buff *skb;
	int i;
#ifdef	DEBUG_INT
	int	nFilled = 0;
#endif

	i = DI.nRxDescNextToFill;
	while (NULL == DI.pRxSkb[i].skb) {
		CurrentDescriptor = &DI.pRxDescriptors[i];
		skb = alloc_skb(DI.uRxBufferLen, GFP_ATOMIC | GFP_DMA);
		if (NULL == skb) {
			/* keep re-trying when failures occur */
			/*DRV_WARN("%s> alloc_skb failed", __FUNCTION__);*/
			break;
		}

		skb->dev = Adapter->netdev;
		DI.pRxSkb[i].skb = skb;
		DI.pRxSkb[i].length = DI.uRxBufferLen;
		DI.pRxSkb[i].direction = PCI_DMA_FROMDEVICE;
#ifdef	PCI_SUBSYSTEM
		DI.pRxSkb[i].dma = pci_map_single(Adapter->pdev, skb->data,
												DI.uRxBufferLen,
												PCI_DMA_FROMDEVICE);
#else
		consistent_sync(skb->data, DI.uRxBufferLen, PCI_DMA_FROMDEVICE);
		DI.pRxSkb[i].dma = virt_to_bus(skb->data);
#endif

		/* setup Rx descriptor!!! */
		CurrentDescriptor->RxDMAFragAddr = cpu_to_le32(DI.pRxSkb[i].dma);
		CurrentDescriptor->RxDMAFragLen = cpu_to_le32(DI.uRxBufferLen);
		CurrentDescriptor->RxFrameControl |= cpu_to_le32(DESC_OWN_BIT);

		atomic_dec(&DI.RxDescEmpty);

		i = ++i % DI.nRxDescTotal;
#ifdef	DEBUG_INT
		nFilled++;
#endif
	}
	DI.nRxDescNextToFill = i;

#ifdef	DEBUG_INT
	/*DRV_INFO("filled< %d", nFilled);*/
#endif

	/* enable Rx engine!!! */
	/*KS8695_WRITE_REG(REG_RXSTART + DI.nOffset, 1);*/
}
#endif

#if	0
enum {
	REG_DMA_DUMP,			/* dump all base DMA registers (based on current driver is for) */
	REG_DMA_STATION_DUMP,	/* dump all DMA extra station registers */

	REG_UART_DUMP,			/* dump all UART related registers */
	REG_INT_DUMP,			/* dump all Interrupt related registers */
	REG_TIMER_DUMP,			/* dump all Timer related registers */
	REG_GPIO_DUMP,			/* dump all GPIO related registers */

	REG_SWITCH_DUMP,		/* dump all Switch related registers */
	REG_MISC_DUMP,			/* dump all misc registers */

	REG_SNMP_DUMP,			/* dump all SNMP registers */

	DRV_VERSION,			/* get driver version, (we need this since proc is removed from driver) */

	SET_MAC_LOOPBACK,		/* to enable/disable mac loopback */
	SET_PHY_LOOPBACK,		/* to enable/disable phy loopback */

	MEMORY_DUMP,			/* to dump given memory */
	MEMORY_SEARCH,			/* to search for given data pattern */

	REG_WRITE,				/* to write IO register */

	DEBUG_DUMP_TX_PACKET,	/* to debug ethernet packet to transmit */
	DEBUG_DUMP_RX_PACKET,	/* to debug ethernet packet received */

	DEBUG_RESET_DESC,		/* to reset Rx descriptors */
	DEBUG_STATISTICS,		/* debug statistics */
	DEBUG_DESCRIPTORS,		/* debug descriptors */

	DEBUG_LINK_STATUS,		/* debug link status */

	CONFIG_LINK_TYPE,		/* configure link media type */
	CONFIG_STATION_EX,		/* configure additional station */

	/* switch configuration for web page */
	CONFIG_SWITCH_GET,		/* get switch configuration settings */
	CONFIG_SWITCH_SET,		/* set switch configuration settings */
};

/* defined configured SWITCH SUBID */
enum CONFIG_SWITCH_SUBID {
	/* configuration related to basic switch web page */
	CONFIG_SW_SUBID_ON,						/* turn on/off switch for LAN */
	CONFIG_SW_SUBID_PORT_VLAN,				/* configure port VLAN ID, and Engress mode */
	CONFIG_SW_SUBID_PRIORITY,				/* configure port priority */

	/* configuration related to advanced switch web page */
	CONFIG_SW_SUBID_ADV_LINK_SELECTION,		/* configure port link selection */
	CONFIG_SW_SUBID_ADV_CTRL,				/* configure switch control register */
	CONFIG_SW_SUBID_ADV_MIRRORING,			/* configure switch port mirroring */
	CONFIG_SW_SUBID_ADV_THRESHOLD,			/* configure threshold for both 802.1p and broadcast storm protection */
	CONFIG_SW_SUBID_ADV_DSCP,				/* configure switch DSCP priority */

	/* configuration related to Switch internal web page */
	CONFIG_SW_SUBID_INTERNAL_LED,			/* configure LED for all */
	CONFIG_SW_SUBID_INTERNAL_MISC,			/* configure misc. */
	CONFIG_SW_SUBID_INTERNAL_SPANNINGTREE,	/* configure spanning tree */
};

/* use __packed in armcc later */
typedef struct {
	uint8_t		byId;
	uint16_t	usLen;
	union {
		uint32_t	uData[0];
		uint16_t	usData[0];
		uint8_t		byData[0];
	} u;
} IOCTRL, *PIOCTRL;

typedef struct {
	uint8_t		byId;
	uint16_t	usLen;
	uint8_t		bySubId;
	union {
		uint32_t	uData[0];
		uint16_t	usData[0];
		uint8_t		byData[0];
	} u;
} IOCTRL_SWITCH, *PIOCTRL_SWITCH;

#define	REG_DMA_MAX			8
#define	REG_DMA_STATION_MAX	32
#define	REG_UART_MAX		9
#define	REG_INT_MAX			14
#define	REG_TIMER_MAX		5
#define	REG_GPIO_MAX		3
#define	REG_SWITCH_MAX		21
#define	REG_MISC_MAX		7
#define	REG_SNMP_MAX		138
#endif

static int	ks8695_ioctl_switch(PADAPTER_STRUCT Adapter, PIOCTRL_SWITCH pIoCtrl);

/*
 * ks8695_ioctl
 *	This function is the ioctl entry point. It handles some driver specific IO
 *	functions.
 *
 * Argument(s)
 *	netdev		pointer to net_device structure.
 *	ifr			pointer to ifreq structure
 *	cmd			io cmd 
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
    PADAPTER_STRUCT Adapter = netdev->priv;
	PIOCTRL	pIoCtrl;
	int	nRet = -1;

    /*DRV_INFO("%s> cmd = 0x%x", __FUNCTION__, cmd); */
	pIoCtrl = (PIOCTRL)ifr->ifr_data;

    switch(cmd) {
		case SIOC_KS8695_IOCTRL:
			if (ifr->ifr_data) {
				UINT32	*pReg, i;

				switch (pIoCtrl->byId)
				{
					case REG_DMA_DUMP:
						if (pIoCtrl->usLen >= (4 * (1 + REG_DMA_MAX) + 3)) { /* 1 + 2 + 4 + 8 * 4 */
							pReg = pIoCtrl->u.uData;
							/* tell debug application its offset */
							*pReg++ = DI.nOffset;
							for (i = REG_TXCTRL; i <= REG_STATION_HIGH ; i += 4, pReg++) {
								*pReg = (UINT32)KS8695_READ_REG(i + DI.nOffset);
							}
							nRet = 0;
						}
						break;

					case REG_DMA_STATION_DUMP:
						if (pIoCtrl->usLen >= (4 * REG_DMA_STATION_MAX + 3)) { /* 1 + 2 + 16 * 2 * 4 */
							pReg = pIoCtrl->u.uData;
							for (i = REG_MAC0_LOW; i <= REG_MAC15_HIGH ; i += 8) {
								*pReg++ = (UINT32)KS8695_READ_REG(i + DI.nOffset);
								*pReg++ = (UINT32)KS8695_READ_REG(i + DI.nOffset + 4);
							}
							nRet = 0;
						}
						break;

					case REG_UART_DUMP:
						if (pIoCtrl->usLen >= (4 * REG_UART_MAX + 3)) { /* 1 + 2 + 9 * 4 */
							pReg = pIoCtrl->u.uData;
							for (i = REG_UART_RX_BUFFER; i <= REG_UART_STATUS ; i += 4, pReg++) {
								*pReg = (UINT32)KS8695_READ_REG(i);
							}
							nRet = 0;
						}
						break;

					case REG_INT_DUMP:
						if (pIoCtrl->usLen >= (4 * REG_INT_MAX + 3)) { /* 1 + 2 + 14 * 4 */
							pReg = pIoCtrl->u.uData;
							for (i = REG_INT_CONTL; i <= REG_IRQ_PEND_PRIORITY ; i += 4, pReg++) {
								*pReg = (UINT32)KS8695_READ_REG(i);
							}
							nRet = 0;
						}
						break;

					/* Timer receive related */
					case REG_TIMER_DUMP:
						if (pIoCtrl->usLen >= (4 * REG_TIMER_MAX + 3)) { /* 1 + 2 + 5 * 4 */
							pReg = pIoCtrl->u.uData;
							for (i = REG_TIMER_CTRL; i <= REG_TIMER0_PCOUNT ; i += 4, pReg++) {
								*pReg = (UINT32)KS8695_READ_REG(i);
							}
							nRet = 0;
						}
						break;

					/* GPIO receive related */
					case REG_GPIO_DUMP:
						if (pIoCtrl->usLen >= (4 * REG_GPIO_MAX + 3)) { /* 1 + 2 + 3 * 4 */
							pReg = pIoCtrl->u.uData;
							for (i = REG_GPIO_MODE; i <= REG_GPIO_DATA ; i += 4, pReg++) {
								*pReg = (UINT32)KS8695_READ_REG(i);
							}
							nRet = 0;
						}
						break;

					case REG_SWITCH_DUMP:
						if (pIoCtrl->usLen >= (4 * REG_SWITCH_MAX + 3)) { /* 1 + 2 + 21 * 4 */
							pReg = pIoCtrl->u.uData;
							for (i = REG_SWITCH_CTRL0; i <= REG_LAN34_POWERMAGR ; i += 4, pReg++) {
								*pReg = (UINT32)KS8695_READ_REG(i);
							}
							nRet = 0;
						}
						break;

					case REG_MISC_DUMP:
						if (pIoCtrl->usLen >= (4 * REG_MISC_MAX + 3)) { /* 1 + 2 + 7 * 4 */
							pReg = pIoCtrl->u.uData;
							for (i = REG_DEVICE_ID; i <= REG_WAN_PHY_STATUS ; i += 4, pReg++) {
								*pReg = (UINT32)KS8695_READ_REG(i);
							}
							nRet = 0;
						}
						break;

					case REG_SNMP_DUMP:
						/* do we need to restrict its read to LAN driver only? May be later! */
						if (pIoCtrl->usLen >= (4 * REG_SNMP_MAX + 3)) { /* 1 + 2 + 138 * 4 */
							/* each port (1-4) takes 16 counters, and last 10 counters for port */
							pReg = pIoCtrl->u.uData;
							for (i = 0; i <= REG_SNMP_MAX ; i++, pReg++) {
								*pReg = swReadSNMPReg(Adapter, i);
								DelayInMicroseconds(10);
							}
							nRet = 0;
						}
						break;

					case DRV_VERSION:
						if (pIoCtrl->usLen >= 19) { /* 1 + 2 + 16 */
							strncpy(pIoCtrl->u.byData, ks8695_driver_version, 15);
							nRet = 0;
						}
						break;

					case SET_MAC_LOOPBACK:
						if (pIoCtrl->usLen >= sizeof(IOCTRL)) { /* 1 + 2 + 1 */
							macSetLoopback(Adapter, pIoCtrl->u.byData[0] ? TRUE : FALSE);
						}
						break;

					case SET_PHY_LOOPBACK:
						if (pIoCtrl->usLen >= sizeof(IOCTRL)) { /* 1 + 2 + 1 + 1 (optional) */
							if (4 == pIoCtrl->usLen) 
								swPhyLoopback(Adapter, pIoCtrl->u.byData[0] ? TRUE : FALSE, 0);
							else {
								swPhyLoopback(Adapter, pIoCtrl->u.byData[0] ? TRUE : FALSE, pIoCtrl->u.byData[1]);
							}
						}
						break;

					case MEMORY_DUMP:
						/* dump 32 dwords each time */
						if (pIoCtrl->usLen >= (4 * 32 + 3)) { /* 1 + 2 + 1 + 1 (optional) */
							UINT32	*pReg1;

							/* note that the address is in the first dword, when returned will contain data */
							pReg = pIoCtrl->u.uData;
							pReg1 = (UINT32 *)(*pReg);

#ifdef	DEBUG_THIS
							DRV_INFO("addr=0x%08x, 0x%0x8", pReg1, *pReg1);
#endif
							/* if no null pointer */
							if (pReg1 && (0xc0000000 == ((UINT)pReg1 & 0xc0000000))) {
								for (i = 0; i <= 32 ; i++, pReg++, pReg1++) {
									*pReg = *pReg1;
								}
								nRet = 0;
							}
							else {
								DRV_INFO("%s> invalid address: 0x%08x", __FUNCTION__, *pReg1);
								nRet = -EINVAL;
							}
						}
						break;

					case MEMORY_SEARCH:
						/* dump 32 dwords each time */
						if (pIoCtrl->usLen > 3 && pIoCtrl->usLen < 128) { /* 1 + 2 + optional length */
							DRV_INFO("%s> not implemented yet", __FUNCTION__);
							nRet = 0;
						}
						break;

					case REG_WRITE:
						/* write control register */
						if (pIoCtrl->usLen >= 7) { /* 1 + 2 + 1 * 4 */
							UINT	uReg;

							uReg = pIoCtrl->u.uData[0];
							if (uReg >= 0xffff) {
								return -EINVAL;
							}
							if (pIoCtrl->usLen < 11) {
								/* if no parameter is given, display register value instead */
								printk("Reg(0x%04x) = 0x%08x", uReg, KS8695_READ_REG(uReg));
							}
							else {
								KS8695_WRITE_REG(uReg, pIoCtrl->u.uData[1]);
							}
							nRet = 0;
						}
						break;

					case DEBUG_DUMP_TX_PACKET:
						/* set dump tx packet flag */
						if (pIoCtrl->usLen >= 7) { /* 1 + 2 + 4 */
							DI.uDebugDumpTxPkt = pIoCtrl->u.uData[0];
#ifndef	PACKET_DUMP
							DRV_INFO("%s> DumpTxPkt was disabled", __FUNCTION__);
#endif
							nRet = 0;
						}
						break;

					case DEBUG_DUMP_RX_PACKET:
						/* set dump rx packet flag */
						if (pIoCtrl->usLen >= 7) { /* 1 + 2 + 4 */
							DI.uDebugDumpRxPkt = pIoCtrl->u.uData[0];
#ifndef	PACKET_DUMP
							DRV_INFO("%s> DumpRxPkt was disabled", __FUNCTION__);
#endif
							nRet = 0;
						}
						break;

					case DEBUG_RESET_DESC:
						/* set dump rx packet flag */
						if (pIoCtrl->usLen == 3) { /* 1 + 2 */
							ResetDma(Adapter);
							nRet = 0;
						}
						break;

					case DEBUG_STATISTICS:
						/* printout statistical counters */
						if (pIoCtrl->usLen == 3) { /* 1 + 2 */
							printk("------- statistics TX -------\n");
							printk("tx_packets      = %12u\n", (UINT)STAT_NET(tx_packets));
							printk("tx_bytes        = %12u\n", (UINT)STAT_NET(tx_bytes));
							printk("tx_dropped      = %12u\n", (UINT)STAT_NET(tx_dropped));
							printk("tx_errors       = %12u\n", (UINT)STAT_NET(tx_errors));

							printk("------- statistics RX -------\n");
							printk("rx_packets      = %12u\n", (UINT)STAT_NET(rx_packets));
							printk("rx_bytes        = %12u\n", (UINT)STAT_NET(rx_bytes));
							printk("rx_dropped      = %12u\n", (UINT)STAT_NET(rx_dropped));
							printk("rx_errors       = %12u\n", (UINT)STAT_NET(rx_errors));
							printk("rx_length_errors= %12u\n", (UINT)STAT_NET(rx_length_errors));
							printk("rx_crc_errors   = %12u\n", (UINT)STAT_NET(rx_crc_errors));
							printk("collisions      = %12u\n", (UINT)STAT_NET(collisions));
							printk("multicast       = %12u\n", (UINT)STAT_NET(multicast));
							printk("rx_missed_errors= %12u\n", (UINT)STAT_NET(rx_missed_errors));
							printk("rx_length_errors= %12u\n", (UINT)STAT_NET(rx_length_errors));
							printk("over size pkts  = %12u\n", (UINT)DI.uRx1518plus);
							printk("under size pkts = %12u\n", (UINT)DI.uRxUnderSize);
							printk("TransmitCount   = %12u\n", DI.nTransmitCount);
#ifdef	DEBUG_INT	
							printk("ProcessedCount  = %12u\n", DI.nTxProcessedCount);
							printk("max refilled cnt= %12u\n", (UINT)DI.nMaxFilledCount);
							printk("max processed ct= %12u\n", (UINT)DI.nMaxProcessedCount);
#endif

							printk("------- Misc -------\n");
							printk("DMA reset count = %12d\n", DI.nResetCount);
							printk("Link change cnt = %12d\n", DI.nLinkChangeCount);
							nRet = 0;
						}
						break;

					case DEBUG_DESCRIPTORS:
						/* printout descriptors info */
						if (pIoCtrl->usLen == 3) { /* 1 + 2 */
							printk("------ TX Descriptors ------\n");
							printk("descriptor VA   = 0x%08x\n", (UINT)DI.pTxDescriptors);
							printk("total           = %10d\n", DI.nTxDescTotal);
							printk("available       = %10d\n", atomic_read(&DI.nTxDescAvail));
							printk("next available  = %10d\n", DI.nTxDescNextAvail);
							printk("no resource     = %10d\n", DI.bTxNoResource);
							printk("------ RX Descriptors ------\n");
							printk("descriptor VA   = 0x%08x\n", (UINT)DI.pRxDescriptors);
							printk("total           = %10d\n", DI.nRxDescTotal);
							printk("next to fill    = %10d\n", DI.nRxDescNextToFill);
							printk("next available  = %10d\n", DI.nRxDescNextAvail);
							printk("empty           = %10d\n", atomic_read(&DI.RxDescEmpty));
							nRet = 0;
						}
						break;

					case DEBUG_LINK_STATUS:
						/* printout link status */
						if (pIoCtrl->usLen >= 3) { /* 1 + 2 */
							int	i;

							if (DMA_LAN != DI.usDMAId) {
								/* RLQ, 1.0.0.14, modified for UPNP query */
								if (pIoCtrl->usLen == 15) {	/* 3 + 3 * 4 */
									UINT32	*pReg;

									/* note that the address is in the first dword, when returned will contain data */
									pReg = pIoCtrl->u.uData;
									i = 0;
									*pReg++ = DI.bLinkActive[i];	/* link active */
									if (!DI.bLinkActive[i]) {
										*pReg++ = 0;				/* speed */
										*pReg = 0;					/* duplex */
									}
									else {
										*pReg++ = (SPEED_100 == DI.usLinkSpeed[i]) ? 100000000 : 10000000;
										*pReg = DI.bHalfDuplex[i];
									}
								}
								else {	/* for back compatible with ks8695_debug utility */
									i = 0;
									if (!DI.bLinkActive[i]) {
										printk("Link = Down, Speed=Unknown, Duplex=Unknown\n");
									}
									else {
										if (SW_PHY_AUTO == DI.usCType[i]) {
											printk("Link=Up, Speed=%s, Duplex (read)=%s, Duplex (detected)=%s\n",
												SPEED_100 == DI.usLinkSpeed[i] ? "100" : "10", 
												DI.bHalfDuplex[i] ? "Full Duplex" : "Half Duplex",
												DI.bHalfDuplexDetected[i] ? "Full Duplex" : "Half Duplex");
										}
										else {
											printk("Link=Up, Speed=%s, Duplex=%s\n",
												SPEED_100 == DI.usLinkSpeed[i] ? "100" : "10", 
												DI.bHalfDuplex[i] ? "Full Duplex" : "Half Duplex");
										}
									}
								}
							}
							else {
								/* RLQ, 1.0.0.14, modified for UPNP query */
								if (pIoCtrl->usLen == 3 + 3 * 4 * SW_MAX_LAN_PORTS) {	/* 3 + 3 * 4 * 4 = 51 */
									UINT32	*pReg;

									/* note that the address is in the first dword, when returned will contain data */
									pReg = pIoCtrl->u.uData;
									for (i = 0; i < SW_MAX_LAN_PORTS; i++) {
										*pReg++ = DI.bLinkActive[i];	/* link active */
										if (!DI.bLinkActive[i]) {
											*pReg++ = 0;				/* speed */
											*pReg++ = 0;				/* duplex */
										}
										else {
											*pReg++ = (SPEED_100 == DI.usLinkSpeed[i]) ? 100000000 : 10000000;
											*pReg++ = DI.bHalfDuplex[i];
										}
									}
								}
								else {
									for (i = 0; i < SW_MAX_LAN_PORTS; i++) {
										if (DI.bLinkActive[i]) {
											printk("Port[%d]  Speed=%s, Duplex (read)=%s, Duplex (detected)=%s\n", (i + 1),
												SPEED_100 == DI.usLinkSpeed[i] ? "100" : "10", 
												DI.bHalfDuplex[i] ? "Full Duplex" : "Half Duplex",
											DI.bHalfDuplexDetected[i] ? "Full Duplex" : "Half Duplex");
										}
										else {
											printk("Port[%d]  Speed = Unknown, Duplex = Unknown\n", (i + 1));
										}
									}
								}
							}
							nRet = 0;
						}
						break;

					case CONFIG_LINK_TYPE:
						/* configure link media type (forced mode without auto nego) */
						if (pIoCtrl->usLen == 19) { /* 1 + 2 + 4 * 4*/
							uint32_t	uPort;
							uint32_t	uSpeed;
							uint32_t	uDuplex;

							pReg = pIoCtrl->u.uData;
							if (DMA_LAN != DI.usDMAId) {
								uPort = 0;
								pReg++;
							} else {
								uPort = *pReg++;
								if (uPort >= SW_MAX_LAN_PORTS) {
									DRV_WARN("%s> LAN port out of range (%d)", __FUNCTION__, uPort);
									break;
								}
							}
							DI.byDisableAutoNego[uPort] = *pReg++;
							uSpeed = *pReg++;
							uDuplex = *pReg;
							/*DRV_INFO("%s> port=%d, disable autonego=%d, 100=%d, duplex=%d", __FUNCTION__, uPort, DI.byDisableAutoNego[uPort], uSpeed, uDuplex);*/
							swConfigureMediaType(Adapter, uPort, uSpeed, uDuplex);
							nRet = 0;
						}
						break;

					case CONFIG_STATION_EX:
						/* configure link media type (forced mode without auto nego) */
						if (pIoCtrl->usLen == 13) { /* 1 + 2 + 4 + 6 */
							pReg = pIoCtrl->u.uData;

							/* uData[0] = set, byData[4-9] = mac address */
							if (pIoCtrl->u.uData[0]) {
								int	i;

								i = macGetIndexStationEx(Adapter);
								if (i >= 0) {
									macSetStationEx(Adapter, &pIoCtrl->u.byData[4], i);
									nRet = 0;
								}
							}
							else {
								macResetStationEx(Adapter, &pIoCtrl->u.byData[4]);
								nRet = 0;
							}
						}
						break;

					case CONFIG_SWITCH_GET:
						/* we don't really need it since the OS always boots at super mode */
						/* if that is not the case, then enable following line, and add header file if missing ? */
						/*if (!capable(CAP_NET_ADMIN)) return -EPERM;*/
						/* !!! fall over !!! */

					case CONFIG_SWITCH_SET:
						/* for LAN driver only */
						if (DMA_LAN == DI.usDMAId) {
							return ks8695_ioctl_switch(Adapter, (PIOCTRL_SWITCH)ifr->ifr_data);
						}
						else {
							if (CONFIG_SW_SUBID_ADV_LINK_SELECTION == ((PIOCTRL_SWITCH)ifr->ifr_data)->bySubId) {
								return ks8695_ioctl_switch(Adapter, (PIOCTRL_SWITCH)ifr->ifr_data);
							}
							else {
								/* filter out the IF supported for WAN */
								return -EPERM;
							}
						}
						break;

					default:
						DRV_INFO("%s> unsupported parameters: id=%d, len=%d", __FUNCTION__, pIoCtrl->byId, pIoCtrl->usLen);
						return -EOPNOTSUPP;
				}
				break;
			}

		default:
			return -EOPNOTSUPP;
    }

    return nRet;
}

/*
 * ks8695_ioctl_switch
 *	This function is used to configure CONFIG_SWITCH_SUBID related functions, for web page based
 *	configuration or for ks8695_debug tool.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *	pIoCtrl		pointer to IOCTRL_SWITCH structure.
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int ks8695_ioctl_switch(PADAPTER_STRUCT Adapter, PIOCTRL_SWITCH pIoCtrl)
{
	int	nRet = -1;
	uint32_t	uPort, uReg;

    switch(pIoCtrl->bySubId) {
		case CONFIG_SW_SUBID_ON:
			if (pIoCtrl->usLen == 8) { /* 1 + 2 + 1 + 4 */
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) 
					swEnableSwitch(Adapter, pIoCtrl->u.uData[0]);
				else {
					/* return current switch status */
					pIoCtrl->u.uData[0] = (KS8695_READ_REG(REG_SWITCH_CTRL0) & SW_CTRL0_SWITCH_ENABLE) ? TRUE : FALSE;
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_PORT_VLAN:
			if (pIoCtrl->usLen == 20) { /* 1 + 2 + 1 + 4 * 4 */
				uPort = pIoCtrl->u.uData[0];
				if (uPort >= SW_MAX_LAN_PORTS) {
					DRV_WARN("%s> LAN port out of range (%d)", __FUNCTION__, uPort);
					break;
				}
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					uint32_t	bInsertion;

					DPI[uPort].usTag = (uint16_t)pIoCtrl->u.uData[1];
					/* note that the driver doesn't use VLAN name, so the web page needs to remember it */
					bInsertion = pIoCtrl->u.uData[2];
					DPI[uPort].byCrossTalkMask = (uint8_t)(pIoCtrl->u.uData[3] & 0x1f);
#ifdef	DEBUG_THIS
					DRV_INFO("%s> port=%d, VID=%d, EgressMode=%s, crosstalk bit=0x%x", __FUNCTION__, uPort, DPI[uPort].usTag, bInsertion ? "tagged" : "untagged");
#endif
					swConfigurePort(Adapter, uPort);
					swConfigTagInsertion(Adapter, uPort, bInsertion);
				}
				else {
					pIoCtrl->u.uData[1] = (uint32_t)DPI[uPort].usTag;
					pIoCtrl->u.uData[2] = (KS8695_READ_REG(REG_SWITCH_ADVANCED) & (1L << (17 + uPort))) ? TRUE : FALSE;
					pIoCtrl->u.uData[3] = (uint32_t)DPI[uPort].byCrossTalkMask;
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_PRIORITY:
			if (pIoCtrl->usLen == 32) { /* 1 + 2 + 1 + 4 * 7 */
				uPort = pIoCtrl->u.uData[0];
				if (uPort >= SW_MAX_LAN_PORTS) {
					DRV_WARN("%s> LAN port out of range (%d)", __FUNCTION__, uPort);
					break;
				}
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					uint32_t	bRemoval;

					DPI[uPort].byIngressPriorityTOS		= (uint8_t)pIoCtrl->u.uData[1];
					DPI[uPort].byIngressPriority802_1P	= (uint8_t)pIoCtrl->u.uData[2];
					DPI[uPort].byIngressPriority		= (uint8_t)pIoCtrl->u.uData[3];
					DPI[uPort].byEgressPriority			= (uint8_t)pIoCtrl->u.uData[4];
					bRemoval = (uint8_t)pIoCtrl->u.uData[5];
					DPI[uPort].byStormProtection		= (uint8_t)pIoCtrl->u.uData[6];
#ifdef	DEBUG_THIS
					DRV_INFO("%s> port=%d, DSCPEnable=%d, IngressPriority802_1P=%d, IngressPriority=%d,\
						EgressPriority=%d, IngressTagRemoval=%d, StormProtection=%d", __FUNCTION__, uPort, 
						DPI[uPort].byIngressPriorityTOS, DPI[uPort].byIngressPriority802_1P, DPI[uPort].byIngressPriority,
						DPI[uPort].byEgressPriority, bRemoval, DPI[uPort].byStormProtection);
#endif
					swConfigurePort(Adapter, uPort);
					swConfigTagRemoval(Adapter, uPort, bRemoval);
				}
				else {
					pIoCtrl->u.uData[1]	= (uint32_t)DPI[uPort].byIngressPriorityTOS;
					pIoCtrl->u.uData[2]	= (uint32_t)DPI[uPort].byIngressPriority802_1P;
					pIoCtrl->u.uData[3]	= (uint32_t)DPI[uPort].byIngressPriority;
					pIoCtrl->u.uData[4]	= (uint32_t)DPI[uPort].byEgressPriority;
					pIoCtrl->u.uData[6]	= (uint32_t)DPI[uPort].byStormProtection;
					uReg = KS8695_READ_REG(REG_SWITCH_ADVANCED);
					pIoCtrl->u.uData[5] = uReg & (1L << (22 + uPort)) ? TRUE : FALSE;
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_ADV_LINK_SELECTION:
			if (pIoCtrl->usLen >= 16) { /* 1 + 2 + 1 + 4 * (3 | 4) */
				uPort = pIoCtrl->u.uData[0];
				if (uPort >= SW_MAX_LAN_PORTS) {
					DRV_WARN("%s> LAN port out of range (%d)", __FUNCTION__, uPort);
					break;
				}
				if (DMA_LAN != DI.usDMAId) {
					uPort = 0;
				}
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					uint32_t	uDuplex;

					if (pIoCtrl->usLen < 20) {
						uDuplex = 0;
					}
					else {
						uDuplex = pIoCtrl->u.uData[3];
					}
					/* auto nego or forced mode */
					DI.byDisableAutoNego[uPort] = pIoCtrl->u.uData[1];
					swConfigureMediaType(Adapter, uPort, pIoCtrl->u.uData[2], uDuplex);
				}
				else {
					pIoCtrl->u.uData[1] = DI.usCType[uPort];
					pIoCtrl->u.uData[2] = (uint32_t)DI.byDisableAutoNego[uPort];
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_ADV_CTRL:
			if (pIoCtrl->usLen == 24) { /* 1 + 2 + 1 + 4 * 5 */
				uReg = KS8695_READ_REG(REG_SWITCH_CTRL0);
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					if (pIoCtrl->u.uData[0]) {
						uReg |= SW_CTRL0_FLOWCTRL_FAIR;
					}
					else {
						uReg &= ~SW_CTRL0_FLOWCTRL_FAIR;
					}
					if (pIoCtrl->u.uData[1]) {
						uReg |= SW_CTRL0_LEN_CHECKING;
					}
					else {
						uReg &= ~SW_CTRL0_LEN_CHECKING;
					}
					if (pIoCtrl->u.uData[2]) {
						uReg |= SW_CTRL0_AUTO_FAST_AGING;
					}
					else {
						uReg &= ~SW_CTRL0_AUTO_FAST_AGING;
					}
					if (pIoCtrl->u.uData[3]) {
						uReg |= SW_CTRL0_NO_BCAST_STORM_PROT;
					}
					else {
						uReg &= ~SW_CTRL0_NO_BCAST_STORM_PROT;
					}
					uReg &= 0xfffffff3;	/* clear priority scheme fields, 3:2 */
					uReg |= (pIoCtrl->u.uData[4] & 0x3) << 2;
					KS8695_WRITE_REG(REG_SWITCH_CTRL0, uReg);
					/* need 20 cpu clock delay for switch related registers */
					DelayInMicroseconds(10);
				}
				else {
					pIoCtrl->u.uData[0] = uReg & SW_CTRL0_FLOWCTRL_FAIR ? TRUE : FALSE;
					pIoCtrl->u.uData[1] = uReg & SW_CTRL0_LEN_CHECKING ? TRUE : FALSE;
					pIoCtrl->u.uData[2] = uReg & SW_CTRL0_AUTO_FAST_AGING ? TRUE : FALSE;
					pIoCtrl->u.uData[3] = uReg & SW_CTRL0_NO_BCAST_STORM_PROT ? TRUE : FALSE;
					pIoCtrl->u.uData[4] = (uReg >> 2) & 0x3;
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_ADV_MIRRORING:
			if (pIoCtrl->usLen == 24) { /* 1 + 2 + 1 + 4 * 5 */
				uReg = KS8695_READ_REG(REG_SWITCH_ADVANCED);
				/* note that the port start from 1 - 5 instead of 0 - 5 used internally in the driver */
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					uReg &= 0xfffe0000;		/* currently, WEB page allows only on sniffer */
					/* sniffer port */
					if (pIoCtrl->u.uData[0] > 0 && pIoCtrl->u.uData[0] <= 5) {
						uReg |= (1L << (pIoCtrl->u.uData[0] - 1)) << 11;
					}
					/* Tx mirror port */
					if (pIoCtrl->u.uData[1] > 0 && pIoCtrl->u.uData[1] <= 5) {
						uReg |= (1L << (pIoCtrl->u.uData[1] - 1)) << 6;
					}
					/* Rx mirror port */
					if (pIoCtrl->u.uData[2] > 0 && pIoCtrl->u.uData[2] <= 5) {
						uReg |= (1L << (pIoCtrl->u.uData[2] - 1)) << 1;
					}
					/* sniffer mode, 1 for AND 0 for OR */
					if (pIoCtrl->u.uData[3]) {
						uReg |= 0x00010000;		/* bit 16 */
					}
					/* IGMP trap enable */
					if (pIoCtrl->u.uData[4]) {
						uReg |= 0x00000001;		/* bit 0 */
					}
					KS8695_WRITE_REG(REG_SWITCH_ADVANCED, uReg);
					/* need 20 cpu clock delay for switch related registers */
					DelayInMicroseconds(10);
				}
				else {
					pIoCtrl->u.uData[0] = (uReg >> 11) & 0x1f;
					pIoCtrl->u.uData[1] = (uReg >> 6) & 0x1f;
					pIoCtrl->u.uData[2] = (uReg >> 1) & 0x1f;
					pIoCtrl->u.uData[3] = (uReg & 0x00010000) ? TRUE : FALSE;
					pIoCtrl->u.uData[4] = (uReg & 0x00000001) ? TRUE : FALSE;
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_ADV_THRESHOLD:
			if (pIoCtrl->usLen == 12) { /* 1 + 2 + 1 + 4 * 2 */
				uReg = KS8695_READ_REG(REG_SWITCH_CTRL1);	/* 0xE804 */
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					uReg &= 0x00ffffff;	/* bit 31:24 */
					uReg |= (pIoCtrl->u.uData[0] & 0xff) << 24;
					KS8695_WRITE_REG(REG_SWITCH_CTRL1, uReg);
					DelayInMicroseconds(10);

					uReg = KS8695_READ_REG(REG_SWITCH_CTRL0);
					uReg &= 0x8fffffff;	/* bit 30:28 */
					uReg |= (pIoCtrl->u.uData[1] & 0x07) << 28;
					KS8695_WRITE_REG(REG_SWITCH_CTRL0, uReg);
					/* need 20 cpu clock delay for switch related registers */
					DelayInMicroseconds(10);
				}
				else {
					pIoCtrl->u.uData[0] = (uReg >> 24);
					uReg = KS8695_READ_REG(REG_SWITCH_CTRL0);
					pIoCtrl->u.uData[1] = (uReg >> 28) & 0x07;
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_ADV_DSCP:
			if (pIoCtrl->usLen == 12) { /* 1 + 2 + 1 + 4 * 2 */
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					/* DSCP high */
					KS8695_WRITE_REG(REG_DSCP_HIGH, pIoCtrl->u.uData[0]);
					DelayInMicroseconds(10);
					/* DSCP low */
					KS8695_WRITE_REG(REG_DSCP_LOW, pIoCtrl->u.uData[1]);
					DelayInMicroseconds(10);
				}
				else {
					pIoCtrl->u.uData[0] = KS8695_READ_REG(REG_DSCP_HIGH);
					pIoCtrl->u.uData[1] = KS8695_READ_REG(REG_DSCP_LOW);
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_INTERNAL_LED:
			if (pIoCtrl->usLen == 12) { /* 1 + 2 + 1 + 4 * 2 */
				uint8_t	byLed0, byLed1;

				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					byLed0 = (uint8_t)pIoCtrl->u.uData[0];
					byLed1 = (uint8_t)pIoCtrl->u.uData[1];
					if (byLed0 <= LED_LINK_ACTIVITY && byLed1 <= LED_LINK_ACTIVITY) {
						swSetLED(Adapter, FALSE, byLed0);
						swSetLED(Adapter, TRUE, byLed1);

						/* can we set WAN here as well or separate it to WAN driver ? */
						{
							uReg = KS8695_READ_REG(REG_WAN_CONTROL);
							uReg &= 0xffffff88;		/* 6:4, 2:0 */
							uReg |= (uint32_t)byLed1 << 4;
							uReg |= (uint32_t)byLed0;
#if 0
							KS8695_WRITE_REG(REG_WAN_CONTROL, uReg);
#endif
							/* need 20 cpu clock delay for switch related registers */
							DelayInMicroseconds(10);
						}
					}
					else {	/* out of range error */
						DRV_WARN("%s> LED setting out of range", __FUNCTION__);
						break;
					}
				}
				else {
					/* note that currently, all LED use same settings, so there is no
					   need to define port in the IF */
					/* LAN */
					uReg = KS8695_READ_REG(REG_SWITCH_CTRL0);

					pIoCtrl->u.uData[0] = (uint32_t)((uReg >> 22) & 0x7);
					pIoCtrl->u.uData[1] = (uint32_t)((uReg >> 25) & 0x7);
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_INTERNAL_MISC:
			if (pIoCtrl->usLen == 44) { /* 1 + 2 + 1 + 4 * 10 */
				uReg = KS8695_READ_REG(REG_SWITCH_CTRL0);
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					if (pIoCtrl->u.uData[0]) {
						uReg |= SW_CTRL0_ERROR_PKT;
					}
					else {
						uReg &= ~SW_CTRL0_ERROR_PKT;
					}
					if (pIoCtrl->u.uData[1]) {
						uReg |= SW_CTRL0_BUFFER_SHARE;
					}
					else {
						uReg &= ~SW_CTRL0_BUFFER_SHARE;
					}
					if (pIoCtrl->u.uData[2]) {
						uReg |= SW_CTRL0_AGING_ENABLE;
					}
					else {
						uReg &= ~SW_CTRL0_AGING_ENABLE;
					}
					if (pIoCtrl->u.uData[3]) {
						uReg |= SW_CTRL0_FAST_AGING;
					}
					else {
						uReg &= ~SW_CTRL0_FAST_AGING;
					}
					if (pIoCtrl->u.uData[4]) {
						uReg |= SW_CTRL0_FAST_BACKOFF;
					}
					else {
						uReg &= ~SW_CTRL0_FAST_BACKOFF;
					}
					if (pIoCtrl->u.uData[5]) {
						uReg |= SW_CTRL0_6K_BUFFER;
					}
					else {
						uReg &= ~SW_CTRL0_6K_BUFFER;
					}
					if (pIoCtrl->u.uData[6]) {
						uReg |= SW_CTRL0_MISMATCH_DISCARD;
					}
					else {
						uReg &= ~SW_CTRL0_MISMATCH_DISCARD;
					}
					if (pIoCtrl->u.uData[7]) {
						uReg |= SW_CTRL0_COLLISION_DROP;
					}
					else {
						uReg &= ~SW_CTRL0_COLLISION_DROP;
					}
					if (pIoCtrl->u.uData[8]) {
						uReg |= SW_CTRL0_BACK_PRESSURE;
					}
					else {
						uReg &= ~SW_CTRL0_BACK_PRESSURE;
					}
					if (pIoCtrl->u.uData[9]) {
						uReg |= SW_CTRL0_PREAMBLE_MODE;
					}
					else {
						uReg &= ~SW_CTRL0_PREAMBLE_MODE;
					}
					KS8695_WRITE_REG(REG_SWITCH_CTRL0, uReg);
					/* need 20 cpu clock delay for switch related registers */
					DelayInMicroseconds(10);
				}
				else {
					pIoCtrl->u.uData[0] = uReg & SW_CTRL0_ERROR_PKT ? TRUE : FALSE;
					pIoCtrl->u.uData[1] = uReg & SW_CTRL0_BUFFER_SHARE ? TRUE : FALSE;
					pIoCtrl->u.uData[2] = uReg & SW_CTRL0_AGING_ENABLE ? TRUE : FALSE;
					pIoCtrl->u.uData[3] = uReg & SW_CTRL0_FAST_AGING ? TRUE : FALSE;
					pIoCtrl->u.uData[4] = uReg & SW_CTRL0_FAST_BACKOFF ? TRUE : FALSE;
					pIoCtrl->u.uData[5] = uReg & SW_CTRL0_6K_BUFFER ? TRUE : FALSE;
					pIoCtrl->u.uData[6] = uReg & SW_CTRL0_MISMATCH_DISCARD ? TRUE : FALSE;
					pIoCtrl->u.uData[7] = uReg & SW_CTRL0_COLLISION_DROP ? TRUE : FALSE;
					pIoCtrl->u.uData[8] = uReg & SW_CTRL0_BACK_PRESSURE ? TRUE : FALSE;
					pIoCtrl->u.uData[9] = uReg & SW_CTRL0_PREAMBLE_MODE ? TRUE : FALSE;
				}
				nRet = 0;
			}
			break;

		case CONFIG_SW_SUBID_INTERNAL_SPANNINGTREE:
			if (pIoCtrl->usLen == 20) { /* 1 + 2 + 1 + 4 * 4 */
				uint32_t	uTxSpanning, uRxSpanning;

				uPort = pIoCtrl->u.uData[0];
				if (uPort >= SW_MAX_LAN_PORTS) {
					DRV_WARN("%s> LAN port out of range (%d)", __FUNCTION__, uPort);
					break;
				}
				if (CONFIG_SWITCH_SET == pIoCtrl->byId) {
					uTxSpanning = pIoCtrl->u.uData[1];
					uRxSpanning = pIoCtrl->u.uData[2];
					DPI[uPort].byDisableSpanningTreeLearn = pIoCtrl->u.uData[3];
					if (uTxSpanning) {
						if (uRxSpanning)
							DPI[uPort].bySpanningTree = SW_SPANNINGTREE_ALL;
						else
							DPI[uPort].bySpanningTree = SW_SPANNINGTREE_TX;
					}
					else {
						if (uRxSpanning)
							DPI[uPort].bySpanningTree = SW_SPANNINGTREE_RX;
						else
							DPI[uPort].bySpanningTree = SW_SPANNINGTREE_NONE;
					}
					swConfigurePort(Adapter, uPort);
				}
				else {
					uTxSpanning = uRxSpanning = FALSE;
					if (SW_SPANNINGTREE_ALL == DPI[uPort].bySpanningTree) {
						uTxSpanning = uRxSpanning = TRUE;
					}
					else if (SW_SPANNINGTREE_TX == DPI[uPort].bySpanningTree) {
						uTxSpanning = TRUE;
					}
					else if (SW_SPANNINGTREE_RX == DPI[uPort].bySpanningTree) {
						uRxSpanning = TRUE;
					}
					pIoCtrl->u.uData[1] = uTxSpanning;
					pIoCtrl->u.uData[2] = uRxSpanning;
					pIoCtrl->u.uData[3] = (uint32_t)DPI[uPort].byDisableSpanningTreeLearn;
				}
				nRet = 0;
			}
			break;
			
		default:
			DRV_INFO("%s> unsupported parameters: id=%d, len=%d", __FUNCTION__, pIoCtrl->byId, pIoCtrl->usLen);
			return -EOPNOTSUPP;
	}

    return nRet;
}

/*
 * ks8695_icache_lock
 *	This function is use to lock given icache
 *
 * Argument(s)
 *	icache_start	pointer to starting icache address
 *	icache_end		pointer to ending icache address
 *
 * Return(s)
 *	0		if success
 *	error	otherwise
 */
int ks8695_icache_lock2(void *icache_start, void *icache_end)
{
	uint32_t victim_base = ICACHE_VICTIM_BASE << ICACHE_VICTIM_INDEX;
	spinlock_t	lock = SPIN_LOCK_UNLOCKED;
	unsigned long flags;
#ifdef	DEBUG_THIS
	int	len;

	len = (int)(icache_end - icache_start);
	DRV_INFO("%s: start=%p, end=%p, len=%d", __FUNCTION__, icache_start, icache_end, len);
	/* if lockdown lines are more than half of max associtivity */
	if ((len / ICACHE_BYTES_PER_LINE) > (ICACHE_ASSOCITIVITY >> 1)) {
		DRV_WARN("%s: lockdown lines = %d is too many, (Assoc=%d)", __FUNCTION__, (len / ICACHE_BYTES_PER_LINE), ICACHE_ASSOCITIVITY);
		return -1;
	}
#endif

	spin_lock_irqsave(&lock, flags);
	
	__asm__(
		" \
		ADRL	r0, ks8695_isr		/* compile complains if icache_start is given instead */ \n\
		ADRL	r1, ks8695_isre		/* compile complains if icache_end is given instead */ \n\
		MOV	r2, %0 \n\
		MCR	p15, 0, r2, c9, c4, 1 \n\
 \n\
lock_loop: \n\
		MCR	p15, 0, r0, c7, c13, 1 \n\
		ADD	r0, r0, #32 \n\
 \n\
		AND	r3, r0, #0x60 \n\
		CMP	r3, #0x0 \n\
		ADDEQ	r2, r2, #0x1<<26		/* ICACHE_VICTIM_INDEX */ \n\
		MCREQ	p15, 0, r2, c9, c0, 1 \n\
		  \n\
		CMP	r0, r1 \n\
		BLE	lock_loop \n\
 \n\
		CMP	r3, #0x0 \n\
		ADDNE	r2, r2, #0x1<<26		/* ICACHE_VICTIM_INDEX */ \n\
		MCRNE	p15, 0, r2, c9, c0,	1 \n\
		"
		:
		: "r" (victim_base)
		: "r0", "r1", "r2", "r3"
	);

#ifdef	DEBUG_THIS
	ks8695_icache_read_c9();
#endif

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

/* ks8695_main.c */
