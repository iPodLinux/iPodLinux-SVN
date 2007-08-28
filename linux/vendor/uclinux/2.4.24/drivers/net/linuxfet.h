#ifndef __KERNEL__
#define __KERNEL__
#endif
/* Include files, designed to support most kernel versions 2.0.0 and later. */
#include <linux/config.h>

#if defined(CONFIG_SMP) && ! defined(__SMP__)
#define __SMP__
#endif

#if defined(MODULE) && defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
#define MODVERSIONS
/* Older kernels do not include this automatically. */
#endif

#include <linux/version.h>
#if (LINUX_VERSION_CODE < 0x20341) &&  defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#include <linux/module.h>


#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#if LINUX_VERSION_CODE > 0x02032a
#include <linux/init.h>
#endif
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/if.h>
#include <asm/io.h>
#include <asm/processor.h>      /* Processor type for cache alignment. */
#include <asm/bitops.h>

#if (LINUX_VERSION_CODE < 0x20155)
#include <linux/bios32.h>
/* A minimal version of the 2.2.* PCI support that handles configuration
   space access.
   Drivers that actually use pci_dev fields must do explicit compatibility.
   Note that the struct pci_dev * "pointer" is actually a byte mapped integer!
*/
#if (LINUX_VERSION_CODE < 0x20020)
typedef struct _pci_dev {
    int not_used;
} pci_dev;
#endif

#define pci_find_slot(bus, devfn) (struct pci_dev*)((bus<<8) | devfn | 0xf0000)
#define bus_number(pci_dev) ((((int)(pci_dev)) >> 8) & 0xff)
#define devfn_number(pci_dev) (((int)(pci_dev)) & 0xff)

#ifndef CONFIG_PCI
extern inline int pci_present(void)
{ 
    return 0;
}
#else
#define pci_present pcibios_present
#endif

#define pci_read_config_byte(pdev, where, valp)\
    pcibios_read_config_byte(bus_number(pdev), devfn_number(pdev), where, valp)
#define pci_read_config_word(pdev, where, valp)\
    pcibios_read_config_word(bus_number(pdev), devfn_number(pdev), where, valp)
#define pci_read_config_dword(pdev, where, valp)\
    pcibios_read_config_dword(bus_number(pdev), devfn_number(pdev), where, valp)
#define pci_write_config_byte(pdev, where, val)\
    pcibios_write_config_byte(bus_number(pdev), devfn_number(pdev), where, val)
#define pci_write_config_word(pdev, where, val)\
    pcibios_write_config_word(bus_number(pdev), devfn_number(pdev), where, val)
#define pci_write_config_dword(pdev, where, val)\
    pcibios_write_config_dword(bus_number(pdev), devfn_number(pdev), where, val)
#endif

/* Keep the ring sizes a power of two for compile efficiency.
   The compiler will convert <unsigned>'%'<2^N> into a bit mask.
   Making the Tx ring too large decreases the effectiveness of channel
   bonding and packet priority.
   There are no ill effects from too-large receive rings. */
#define TX_RING_SIZE	16
#define TX_QUEUE_LEN	16      /* Limit ring entries actually used.  */
#define RX_RING_SIZE	64      /* for flow control, so wee need more RDs */
#define U_HEADER_LEN	14
#define U_CRC_LEN	4
#define PCI_REG_MODE3	0x53
#define MODE3_MIION	0x04    /* in PCI_REG_MOD3 OF PCI space */
#define PRIV_ALIGN	15  /* Required alignment mask */

/* Operational parameters that usually are not changed. */
/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT	(2*HZ)
#define PKT_BUF_SZ	1900                 /* Size of each temporary Rx buffer.*/

/* for checksum offload */
#define IPOK		0x20
#define TUOK		0x10
#define IPKT		0x08
#define TCPKT		0x04
#define UDPKT		0x02
#define TAG		0x01

/* DaviCom PHY ID */
#define CID_DAVICOM         0x0181B800    // OUI = 00-60-6E , 0x0181 B800
#define CID_DAVICOM_B       0x0181B802    //

/* These are used by the netdrivers to report values from the
   MII (Media Indpendent Interface) management registers.
*/
#ifndef SIOCGMIIPHY
//warning You are using an old kern_compat.h version.
#define SIOCGMIIPHY	(SIOCDEVPRIVATE)		/* Get the PHY in use. */
#define SIOCGMIIREG	(SIOCDEVPRIVATE + 1) 		/* Read a PHY register. */
#define SIOCSMIIREG	(SIOCDEVPRIVATE + 2) 		/* Write a PHY register. */
#define SIOCGPARAMS	(SIOCDEVPRIVATE + 3) 		/* Read operational parameters. */
#define SIOCSPARAMS	(SIOCDEVPRIVATE + 4) 		/* Set operational parameters. */
#endif
// for CAM Controller
#define CAMC_CAMEN 	0x01
#define CAMC_VCAMSL	0x02
#define CAMC_SELECT_MCAM 0x00
#define CAMC_SELECT_VCAM 0x01
#define CAMC_CAMRD	0x08
#define CAMC_CAMWR	0x04

/* Condensed bus+endian portability operations. */
#define virt_to_le32desc(addr)	cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)	bus_to_virt(le32_to_cpu(addr))

#define USE_IO_OPS
#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb	inb
#define readw	inw
#define readl	inl
#define writeb	outb
#define writew	outw
#define writel	outl

// max time out delay time
#define W_MAX_TIMEOUT	0x0FFFU
	
#define RHINE_IOTYPE	(FET_PCI_USES_IO  | FET_PCI_USES_MASTER | FET_PCI_ADDR0)
#define RHINEII_IOSIZE	256

#if (LINUX_VERSION_CODE < 0x20100)
#define PCI_CAPABILITY_LIST	0x34	/* Offset of first capability list entry */
#define PCI_STATUS_CAP_LIST	0x10	/* Support capability List */
#define PCI_CAP_ID_PM	0x01	/* Power Management */
#endif

/* SMP and better multiarchitecture support were added.
   Using an older kernel means we assume a little-endian uniprocessor.
*/
#if (LINUX_VERSION_CODE < 0x20123)
#define hard_smp_processor_id()	smp_processor_id()
#define test_and_set_bit(val, addr)	set_bit(val, addr)
#define le16_to_cpu(val)	(val)
#define cpu_to_le16(val)	(val)
#define le16_to_cpus(val)
#define le32_to_cpu(val)	(val)
#define cpu_to_le32(val)	(val)
typedef long	spinlock_t;
#define SPIN_LOCK_UNLOCKED	0
#define spin_lock(lock)
#define spin_unlock(lock)
#define spin_lock_irqsave(lock, flags)	save_flags(flags); cli();
#define spin_unlock_irqrestore(lock, flags)	restore_flags(flags);
#endif

#if (LINUX_VERSION_CODE <= 0x20139)
#define	net_device_stats	enet_statistics
#else
#define	NETSTATS_VER2
#endif

/* There was no support for PCI address space mapping in 2.0, but the
   Alpha needed it.  See the 2.2 documentation. */
#if (LINUX_VERSION_CODE < 0x20100) && (!defined(__alpha__))
#define ioremap(a,b)	(((unsigned long)(a) >= 0x100000) ? vremap(a,b) : (void*)(a))
#define iounmap(v)	do { if ((unsigned long)(v) >= 0x100000) vfree(v);} while (0)
#endif

/* The arg count changed, but function name did not.
   We cover that bad choice by defining a new name.
*/
#if (LINUX_VERSION_CODE > 0x020343)
#define dev_free_skb(skb)	dev_kfree_skb_irq(skb)
#elif (LINUX_VERSION_CODE < 0x020159)
#define dev_free_skb(skb)	dev_kfree_skb(skb, FREE_WRITE);
#else
#define dev_free_skb(skb)	dev_kfree_skb(skb);
#endif

/* The old 'struct device' used a too-generic name. */
#if (LINUX_VERSION_CODE < 0x2030d)
#define net_device	device
#endif

/* The 2.2 kernels added the start of capability-based security for operations
   that formerally could only be done by root.
*/
#if (!defined(CAP_NET_ADMIN))
#define capable(CAP_XXX)	(suser())
#endif

/* Support for adding info about the purpose of and parameters for kernel
   modules was added in 2.1. */
#if (LINUX_VERSION_CODE < 0x20115)
#define MODULE_AUTHOR(name)	extern int nonesuch
#define MODULE_DESCRIPTION(string)	extern int nonesuch
#define MODULE_PARM(varname, typestring)	extern int nonesuch
#endif

/*
  These are the structures in the table that drives the PCI probe routines.
  Note the matching code uses a bitmask: more specific table entries should
  be placed before "catch-all" entries.

  The table must be zero terminated.
*/
typedef enum _chip_capability_flags {
    FET_CanHaveMII = 1,
    FET_HasESIPhy = 2,
    FET_HasDavicomPhy = 4,
    FET_ReqTxAlign = 0x10,
    FET_HasWOL = 0x20,
} chip_capability_flags;

typedef enum _pci_id_flags_bits {
    /* Set PCI command register bits before calling probe1(). */
    FET_PCI_USES_IO = 1,
    FET_PCI_USES_MEM = 2,
    FET_PCI_USES_MASTER = 4,
    /* Read and map the single following PCI BAR. */
    FET_PCI_ADDR0 = 0 << 4,
    FET_PCI_ADDR1 = 1 << 4,
    FET_PCI_ADDR2 = 2 << 4,
    FET_PCI_ADDR3 = 3 << 4,
    FET_PCI_ADDR_64BITS = 0x100,
    FET_PCI_NO_ACPI_WAKE = 0x200,
    FET_PCI_NO_MIN_LATENCY = 0x400,
} pci_id_flags_bits;

typedef enum _drv_id_flags {
    FET_PCI_HOTSWAP = 1, /* Leave module loaded for Cardbus-like chips. */
} drv_id_flags;

typedef enum _drv_pwr_action {
    FET_DRV_NOOP,			/* No action. */
    FET_DRV_ATTACH,			/* The driver may expect power ops. */
    FET_DRV_SUSPEND,		/* Machine suspending, next event RESUME or DETACH. */
    FET_DRV_RESUME,			/* Resume from previous SUSPEND  */
    FET_DRV_DETACH,			/* Card will-be/is gone. Valid from SUSPEND! */
    FET_DRV_PWR_WakeOn,		/* Put device in e.g. Wake-On-LAN mode. */
    FET_DRV_PWR_DOWN,		/* Go to lowest power mode. */
    FET_DRV_PWR_UP,			/* Go to normal power mode. */
} drv_pwr_action;

typedef enum _register_offsets {
    FET_PAR = 0x00,
    FET_RCR = 0x06,
    FET_TCR = 0x07,
    FET_CR0 = 0x08,
    FET_CR1 = 0x09,
    FET_TXQWAK = 0x0A,
    FET_ISR0 = 0x0C,
    FET_IMR0 = 0x0E,
    FET_MAR0 = 0x10,     //Muilticast Hash Table/CAM content access register
    FET_MAR4 = 0x14,     //Muilticast Hash Table/CAM content access register
    FET_MCAMD0 = 0x10,
    FET_MCAMD4 = 0x14,
    FET_VCAMD0 = 0x16,
    FET_CURR_RX_DESC_ADDR = 0x18,
    FET_CURR_TX_DESC_ADDR = 0x1C,
    FET_GFTEST = 0x54,
    FET_MIICFG = 0x6C,
    FET_MIISR = 0x6D,
    FET_BCR0 = 0x6E,
    FET_BCR1 = 0x6F,
    FET_MIICR = 0x70,
    FET_MIIADR = 0x71,
    FET_MII_DATA_REG = 0x72,
    FET_EECSR = 0x74,
    FET_CFGA = 0x78,
    FET_CFGB = 0x79,
    FET_CFGC = 0x7A,
    FET_CFGD = 0x7B,
    FET_MPA_tally = 0x7C,
    FET_CRC_tally = 0x7E,
    FET_Micr0 = 0x80,
    FET_STICKHW = 0x83,
    FET_CAMMSK = 0x88,
    FET_CAMC = 0x92,
    FET_CAMADD =0x93,
    FET_PHYANAR = 0x95, /* for VT3106 */
    FET_FlowCR0 = 0x98, /* for VT3106 */
    FET_FlowCR1 = 0x99, /* for VT3106 */
    FET_TxPauseTimer = 0x9A,
    FET_WOLCR_CLR = 0xA4,
    FET_WOLCG_CLR = 0xA7,
    FET_PWRCSR_CLR = 0xAC
} register_offsets;

/* Bits in the interrupt status/mask registers. */
typedef enum _intr_status_bits {
    FET_ISR_PRX = 0x0001,
    FET_ISR_PTX = 0x0002,
    FET_ISR_RXE = 0x0004,
    FET_ISR_TXE = 0x0008,
    FET_ISR_TU = 0x0010,
    FET_ISR_RU = 0x0020,
    FET_ISR_BE = 0x0040,
    FET_ISR_CNT = 0x0080,
    FET_ISR_ERI = 0x0100,
    FET_ISR_UDFI = 0x0200,
    FET_ISR_OVFI = 0x0400,
    FET_ISR_PKTRACE = 0x0800,
    FET_ISR_NORBF = 0x1000,
    FET_ISR_ABTI = 0x2000,
    FET_ISR_SRCI = 0x4000,
    FET_ISR_GENI = 0x8000,
    FET_ISR_NormalSummary = 0x0003,
    FET_ISR_AbnormalSummary = 0xC260,
} intr_status_bits;

typedef enum _FET_CFGD_bits {
    FET_CRADOM = 0x08,
    FET_CAP = 0x04,
    FET_MBA = 0X02,
    FET_BAKOPT = 0x01
} FET_CFGD_bits;

typedef enum _FET_TCR_bits {
    FET_TCR_LB0 = 0x02,
    FET_TCR_LB1 = 0X04,
    FET_TCR_OFSET = 0x08,
    FET_TCR_RTFT0 = 0x20,
    FET_TCR_RTFT1 = 0x40,
    FET_TCR_RTSF = 0x80
} _FET_TCR_bits;

/* Bits in *_desc.status */
typedef enum _rx_status_bits {
    FET_RxOK = 0x8000,
    FET_RxWholePkt = 0x0300,
    FET_RxErr = 0x008F
} rx_status_bits;

typedef enum _desc_status_bits {
    FET_DescOwn = 0x80000000,
    FET_DescEndPacket = 0x4000,
    FET_DescIntr = 0x1000,
} desc_status_bits;

/* Bits in CR0 and CR1. */
typedef enum _chip_cmd_bits {
    FET_CmdInit = 0x0001,
    FET_CmdStart = 0x0002,
    FET_CmdStop = 0x0004,
    FET_CmdRxOn = 0x0008,
    FET_CmdTxOn = 0x0010,
    FET_CmdTxDemand1 = 0x20,
    FET_CmdRxDemand1 = 0x40,
    FET_CmdEarlyRx = 0x0100,
    FET_CmdEarlyTx = 0x0200,
    FET_CmdFDuplex = 0x0400,
    FET_CmdNoFDuplex = 0xFBFF,
    FET_CmdNoTxPoll = 0x0800,
    FET_CmdReset = 0x8000,
} chip_cmd_bits;

typedef struct _match_info {
    int	pci, pci_mask, subsystem, subsystem_mask;
    int	revision, revision_mask; 				/* Only 8 bits. */
} match_info;

typedef struct _pci_id_info {
    const char	*name;
    match_info	id;
    pci_id_flags_bits	pci_flags;
    int	io_size;				/* Needed for I/O region check or ioremap(). */
    int	drv_flags;				/* Driver use, intended as capability flags. */
} pci_id_info;

typedef struct _drv_id_info {
    const char	*name;			/* Single-word driver name. */
    int	flags;
    int	pci_class;				/* Typically PCI_CLASS_NETWORK_ETHERNET<<8. */
    pci_id_info	*pci_dev_tbl;
    void *(*probe1)(struct pci_dev *pdev, void *dev_ptr, unsigned long ioaddr, int irq, int table_idx, int fnd_cnt, u32 revision);
    /* Optional, called for suspend, resume and detach. */
    int (*pwr_event)(void *dev, int event);
} drv_id_info;


/* The Rx and Tx buffer descriptors. */
typedef struct _rx_desc {
    s32	rx_status;
    u32	desc_length;
    u32	addr;
    u32	next_desc;
} rx_desc;

typedef struct _tx_desc {
    s32	tx_status;
    u32	desc_length;
    u32	addr;
    u32	next_desc;
} tx_desc;

/* for VT3106 */
typedef struct _IEEE_8021Q_header {
    u16 tag_protocol;
    u16 tag_control;
} IEEE_8021Q_header;



typedef struct _netdev_private {
    rx_desc* rx_ring;
    tx_desc* tx_ring[8];

#if (LINUX_VERSION_CODE >= 0x20341)
    struct pci_dev *pdev;
    void *ring;
    dma_addr_t ring_dma;
    void *tx_bufs[8];
    dma_addr_t tx_bufs_dma[8];
#if (LINUX_VERSION_CODE >= 0x020400)
    dma_addr_t tx_skbuff_dma[8][TX_RING_SIZE];
    dma_addr_t rx_skbuff_dma[RX_RING_SIZE];
#endif //#if (LINUX_VERSION_CODE >= 0x020400)
#else //#if (LINUX_VERSION_CODE >= 0x20341)
    void	*priv_addr;                    /* Unaligned address for kfree */
    void        *priv_rd;               /* Unaligned address for rx descriptors */
    void        *priv_td;               /* Unaligned address for tx descriptors */    
#endif ////#if (LINUX_VERSION_CODE >= 0x20341)
    /* The addresses of receive-in-place skbuffs. */
    struct	sk_buff* rx_skbuff[RX_RING_SIZE];
    /* The saved address of a sent-in-place packet/buffer, for later free(). */
    struct	sk_buff* tx_skbuff[8][TX_RING_SIZE];
    unsigned char	*tx_buf[8][TX_RING_SIZE];    /* Tx bounce buffers */
    struct	net_device *next_module;     /* Link for devices of this type. */
    struct	net_device_stats stats;
    struct	timer_list timer;    /* Media monitoring timer. */
    /* Frequently used values: keep some adjacent for cache effect. */
    int	chip_idx, card_idx, drv_flags;
    struct	pci_dev *pci_dev;
    rx_desc	*rx_head_desc;
    unsigned int	cur_rx, dirty_rx;      /* Producer/consumer ring indices */
    unsigned int	cur_tx[8], dirty_tx[8];
    unsigned int	rx_buf_sz;             /* Based on MTU+slack. */
    u16	chip_cmd;                       /* Current setting for CR0 and CR1 */
    unsigned int	tx_full[8];             /* The Tx queue is full. */
    /* These values are keep track of the transceiver/media in use. */
    unsigned int	full_duplex;         /* Full-duplex operation requested. */
    unsigned int	auto_negotiation;    /* 1:auto mode, 0:force mode */
    u8	tx_thresh;
    u8	rx_thresh;
    u8  DMA_length;
    /* MII transceiver section. */
    int	mii_cnt;                        /* MII device addresses. */
    u16	advertising;                    /* NWay media advertisement */
    unsigned char	phy_addr;              /* MII device addresses. */
    u8	revision;
    /* Descriptor rings first for alignment. */
} netdev_private;

