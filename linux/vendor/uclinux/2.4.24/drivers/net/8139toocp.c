/*

	8139too.c: A RealTek RTL-8139 Fast Ethernet driver for Linux.

	Maintained by Jeff Garzik <jgarzik@mandrakesoft.com>
	Copyright 2000,2001 Jeff Garzik

	Much code comes from Donald Becker's rtl8139.c driver,
	versions 1.13 and older.  This driver was originally based
	on rtl8139.c version 1.07.  Header of rtl8139.c version 1.13:

	-----<snip>-----

        	Written 1997-2001 by Donald Becker.
		This software may be used and distributed according to the
		terms of the GNU General Public License (GPL), incorporated
		herein by reference.  Drivers based on or derived from this
		code fall under the GPL and must retain the authorship,
		copyright and license notice.  This file is not a complete
		program and may only be used when the entire operating
		system is licensed under the GPL.

		This driver is for boards based on the RTL8129 and RTL8139
		PCI ethernet chips.

		The author may be reached as becker@scyld.com, or C/O Scyld
		Computing Corporation 410 Severn Ave., Suite 210 Annapolis
		MD 21403

		Support and updates available at
		http://www.scyld.com/network/rtl8139.html

		Twister-tuning table provided by Kinston
		<shangh@realtek.com.tw>.

	-----<snip>-----

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	Contributors:

		Donald Becker - he wrote the original driver, kudos to him!
		(but please don't e-mail him for support, this isn't his driver)

		Tigran Aivazian - bug fixes, skbuff free cleanup

		Martin Mares - suggestions for PCI cleanup

		David S. Miller - PCI DMA and softnet updates

		Ernst Gill - fixes ported from BSD driver

		Daniel Kobras - identified specific locations of
			posted MMIO write bugginess

		Gerard Sharp - bug fix, testing and feedback

		David Ford - Rx ring wrap fix

		Dan DeMaggio - swapped RTL8139 cards with me, and allowed me
		to find and fix a crucial bug on older chipsets.

		Donald Becker/Chris Butterworth/Marcus Westergren -
		Noticed various Rx packet size-related buglets.

		Santiago Garcia Mantinan - testing and feedback

		Jens David - 2.2.x kernel backports

		Martin Dennett - incredibly helpful insight on undocumented
		features of the 8139 chips

		Jean-Jacques Michel - bug fix
		
		Tobias Ringström - Rx interrupt status checking suggestion

		Andrew Morton - Clear blocked signals, avoid
		buffer overrun setting current->comm.

	Submitting bug reports:

		"rtl8139-diag -mmmaaavvveefN" output
		enable RTL8139_DEBUG below, and look at 'dmesg' or kernel log

		See 8139too.txt for more details.

-----------------------------------------------------------------------------
2001/09/21
Comments by ShuChen Shao

1.This driver is originally based on 8139too.c version "0.9.15".

2.It has been enhanced to support RTL8139C+ PCI ethernet chips and tested in 2.4.2 kernel.

3.RTL8139C+ PCI ethernet chips is set to support C+ mode by default.
  If FORCE_C_Mode below is enable, the RTL8139C+ chip will be forced to support C mode
  after reboot.

4.This program can be compiled at /usr/src/linux-2.4.2/drivers/net/ using the attached Makefile.
  And the object file named 8139too.o should be moved to the directory 
  /lib/modules/2.4.2-2/kernel/drivers/net/

*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/wireless.h>
#ifdef CONFIG_LEDMAN
#include <linux/ledman.h>
#endif
#include <asm/uaccess.h>	//for copy_from_user
#include <asm/io.h>

#if LINUX_VERSION_CODE < 0x20407
#else
#include <linux/mii.h>
#include <linux/completion.h>
#endif

#define RTL8139CP_VERSION "1.5.0"
#define MODNAME "8139too"
#define RTL8139_DRIVER_NAME   MODNAME " Fast Ethernet driver " RTL8139CP_VERSION
#define PFX MODNAME ": "

#undef RTL8139_PROC_DEBUG

#undef FORCE_CPlus_Mode
#undef FORCE_C_Mode

/* enable PIO instead of MMIO, if CONFIG_8139TOO_PIO is selected */
#ifdef CONFIG_8139TOO_PIO
#define USE_IO_OPS 1
#endif

/* define to 1 to enable copious debugging info */
#undef RTL8139_DEBUG

/* define to 1 to disable lightweight runtime debugging checks */
#undef RTL8139_NDEBUG


#ifdef RTL8139_DEBUG
/* note: prints function name for you */
#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

#ifdef RTL8139_NDEBUG
#  define assert(expr) do {} while (0)
#else
#  define assert(expr) \
        if(!(expr)) {					\
        printk( "Assertion failed! %s,%s,%s,line=%d\n",	\
        #expr,__FILE__,__FUNCTION__,__LINE__);		\
        }
#endif


/* A few user-configurable values. */
/* media options */
#define MAX_UNITS 8
static int media[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int full_duplex[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC.  */
static int multicast_filter_limit = 32;

/* Size of the in-memory receive ring. */
#define RX_BUF_LEN_IDX	2	/* 0==8K, 1==16K, 2==32K, 3==64K */
#define RX_BUF_LEN (8192 << RX_BUF_LEN_IDX)
#define RX_BUF_PAD 16
#define RX_BUF_WRAP_PAD 2048 /* spare padding to handle lack of packet wrap */
#define RX_BUF_TOT_LEN (RX_BUF_LEN + RX_BUF_PAD + RX_BUF_WRAP_PAD)

/* Number of Tx descriptor registers. */
#define NUM_TX_DESC	4

/* max supported ethernet frame size -- must be at least (dev->mtu+14+4).*/
#define MIN_ETH_FRAME_SIZE	60
#define MAX_ETH_FRAME_SIZE	1536

/* Size of the Tx bounce buffers -- must be at least (dev->mtu+14+4). */
#define TX_BUF_SIZE	MAX_ETH_FRAME_SIZE
#define TX_BUF_TOT_LEN	(TX_BUF_SIZE * NUM_TX_DESC)

/* PCI Tuning Parameters
   Threshold is bytes transferred to chip before transmission starts. */
#define TX_FIFO_THRESH 256	/* In bytes, rounded down to 32 byte units. */

/* The following settings are log_2(bytes)-4:  0 == 16 bytes .. 6==1024, 7==end of packet. */
#define RX_FIFO_THRESH	0	/* Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST	7 	/* Maximum PCI burst, '6' is 1024 */
#define TX_DMA_BURST	6	/* Maximum PCI burst, '6' is 1024 */

/* Operational parameters that usually are not changed. */
/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT  (6*HZ)

////////////////////////////////////////////////////////////////////
//for 8139C+ only
#define NUM_CP_TX_DESC	64			/* Number of Tx descriptor registers for C+*/
#define NUM_CP_RX_DESC	64			/* Number of Rx descriptor registers for C+*/
#define CP_RX_BUF_SIZE	2000
#define CPlusEarlyTxThld 0x06
#define CPlusChecksumOffload 0x00070000
////////////////////////////////////////////////////////////////////

enum {
	HAS_MII_XCVR = 0x010000,
	HAS_CHIP_XCVR = 0x020000,
	HAS_LNK_CHNG = 0x040000,
};

#define RTL_MIN_IO_SIZE 0x80
#define RTL8139B_IO_SIZE 256

#define RTL8129_CAPS	HAS_MII_XCVR
#define RTL8139_CAPS	HAS_CHIP_XCVR|HAS_LNK_CHNG

typedef enum {
	RTL8139 = 0,
	RTL8139_CB,
	SMC1211TX,
	/*MPX5030,*/
	DELTA8139,
	ADDTRON8139,
	DFE538TX,
	RTL8129,
} board_t;


/* indexed by board_t, above */
static struct {
	const char *name;
	u32 hw_flags;
} board_info[] __devinitdata = {
	{ "RealTek RTL8139CP Fast Ethernet", RTL8139_CAPS },
	{ "RealTek RTL8139B PCI/CardBus", RTL8139_CAPS },
	{ "SMC1211TX EZCard 10/100 (RealTek RTL8139)", RTL8139_CAPS },
/*	{ MPX5030, "Accton MPX5030 (RealTek RTL8139)", RTL8139_CAPS },*/
	{ "Delta Electronics 8139 10/100BaseTX", RTL8139_CAPS },
	{ "Addtron Technolgy 8139 10/100BaseTX", RTL8139_CAPS },
	{ "D-Link DFE-538TX (RealTek RTL8139)", RTL8139_CAPS },
	{ "RealTek RTL8129", RTL8129_CAPS },
};


static struct pci_device_id rtl8139_pci_tbl[] __devinitdata = {
	{0x10ec, 0x8139, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RTL8139 },
	{0x10ec, 0x8138, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RTL8139_CB },
	{0x1113, 0x1211, PCI_ANY_ID, PCI_ANY_ID, 0, 0, SMC1211TX },
/*	{0x1113, 0x1211, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MPX5030 },*/
	{0x1500, 0x1360, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DELTA8139 },
	{0x4033, 0x1360, PCI_ANY_ID, PCI_ANY_ID, 0, 0, ADDTRON8139 },
	{0x1186, 0x1300, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DFE538TX },
#if defined(CONFIG_MTD_NETtel) || defined(CONFIG_SH_SECUREEDGE5410)
	{0x10ec, 0x8129, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RTL8139 },
#else
	{0x10ec, 0x8129, PCI_ANY_ID, PCI_ANY_ID, 0, 0, RTL8129 },
#endif

	/* some crazy cards report invalid vendor ids like
	 * 0x0001 here.  The other ids are valid and constant,
	 * so we simply don't match on the main vendor id.
	 */
	{PCI_ANY_ID, 0x8139, 0x10ec, 0x8139, 0, 0, RTL8139 },

	{0,}
};
MODULE_DEVICE_TABLE (pci, rtl8139_pci_tbl);


/* The rest of these values should never change. */

/* Symbolic offsets to registers. */
enum RTL8139_registers {
	MAC0 = 0,		/* Ethernet hardware address. */
	MAR0 = 8,		/* Multicast filter. */
	TxStatus0 = 0x10,	/* Transmit status (Four 32bit registers). */
	TxAddr0 = 0x20,		/* Tx descriptors (also four 32bit). */
	RxBuf = 0x30,
	RxEarlyCnt = 0x34,
	RxEarlyStatus = 0x36,
	ChipCmd = 0x37,
	RxBufPtr = 0x38,
	RxBufAddr = 0x3A,
	IntrMask = 0x3C,
	IntrStatus = 0x3E,
	TxConfig = 0x40,
	ChipVersion = 0x43,
	RxConfig = 0x44,
	Timer = 0x48,		/* A general-purpose counter. */
	RxMissed = 0x4C,	/* 24 bits valid, write clears. */
	Cfg9346 = 0x50,
	Config0 = 0x51,
	Config1 = 0x52,
	FlashReg = 0x54,
	MediaStatus = 0x58,
	Config3 = 0x59,
	Config4 = 0x5A,		/* absent on RTL-8139A */
	HltClk = 0x5B,
	MultiIntr = 0x5C,
	TxSummary = 0x60,
	BasicModeCtrl = 0x62,
	BasicModeStatus = 0x64,
	NWayAdvert = 0x66,
	NWayLPAR = 0x68,
	NWayExpansion = 0x6A,
	/* Undocumented registers, but required for proper operation. */
	FIFOTMS = 0x70,		/* FIFO Control and test. */
	CSCR = 0x74,		/* Chip Status and Configuration Register. */
	PARA78 = 0x78,
	PARA7c = 0x7c,		/* Magic transceiver parameter register. */
	Config5 = 0xD8,		/* absent on RTL-8139A */
//  For C+ Registers
	CPlusTxPoll=0xD9, CPlusCmd=0xE0, CPlusRxStartAddr=0xE4,
	CPlusTxStartAddr=0x20, CPlusEarlyTxThldReg=0xEC,
};

enum ClearBitMasks {
	MultiIntrClear = 0xF000,
	ChipCmdClear = 0xE2,
	Config1Clear = (1<<7)|(1<<6)|(1<<3)|(1<<2)|(1<<1),
};

enum ChipCmdBits {
	CmdReset = 0x10,
	CmdRxEnb = 0x08,
	CmdTxEnb = 0x04,
	RxBufEmpty = 0x01,
};

enum CPlusCmdBits {
	CPlusTxEnb = 0x01,
	CPlusRxEnb = 0x02,
	CPlusCheckSumEnb = 0x20,
};

enum CPlusRxStatusDesc {
	CPlusRxRES = 0x00100000,
	CPlusRxCRC = 0x00040000,
	CPlusRxRUNT= 0x00080000,
	CPlusRxRWT = 0x00200000,
	CPlusRxFAE = 0x08000000,
	CPlusRxIPchecksumBIT  = 0x00008000,
	CPlusRxUDPIPchecksumBIT = 0x0000C000,
	CPlusRxTCPIPchecksumBIT = 0x0000A000,
	ProtocolType_NonIP	  = 0,
	ProtocolType_TCPIP	  = 1,
	ProtocolType_UDPIP	  = 2,
	ProtocolType_IP	  	  = 3,
};

enum CPlusTxChecksumOffload {
	CPlusTxIPchecksumOffload  = 0x00040000,
	CPlusTxUDPchecksumOffload = 0x00020000,
	CPlusTxTCPchecksumOffload = 0x00010000,
};

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatusBits {
	PCIErr = 0x8000,
	PCSTimeout = 0x4000,
	RxFIFOOver = 0x40,
	RxUnderrun = 0x20,
	RxOverflow = 0x10,
	TxErr = 0x08,
	TxOK = 0x04,
	RxErr = 0x02,
	RxOK = 0x01,
};
enum TxStatusBits {
	TxHostOwns = 0x2000,
	TxUnderrun = 0x4000,
	TxStatOK = 0x8000,
	TxOutOfWindow = 0x20000000,
	TxAborted = 0x40000000,
	TxCarrierLost = 0x80000000,
};
enum RxStatusBits {
	RxMulticast = 0x8000,
	RxPhysical = 0x4000,
	RxBroadcast = 0x2000,
	RxBadSymbol = 0x0020,
	RxRunt = 0x0010,
	RxTooLong = 0x0008,
	RxCRCErr = 0x0004,
	RxBadAlign = 0x0002,
	RxStatusOK = 0x0001,
};

/* Bits in RxConfig. */
enum rx_mode_bits {
	AcceptErr = 0x20,
	AcceptRunt = 0x10,
	AcceptBroadcast = 0x08,
	AcceptMulticast = 0x04,
	AcceptMyPhys = 0x02,
	AcceptAllPhys = 0x01,
};

/* Bits in TxConfig. */
enum tx_config_bits {
	TxIFG1 = (1 << 25),	/* Interframe Gap Time */
	TxIFG0 = (1 << 24),	/* Enabling these bits violates IEEE 802.3 */
	TxLoopBack = (1 << 18) | (1 << 17), /* enable loopback test mode */
	TxCRC = (1 << 16),	/* DISABLE appending CRC to end of Tx packets */
	TxClearAbt = (1 << 0),	/* Clear abort (WO) */
	TxDMAShift = 8,		/* DMA burst value (0-7) is shift this many bits */

	TxVersionMask = 0x7C800000, /* mask out version bits 30-26, 23 */
};

/* Bits in Config1 */
enum Config1Bits {
	Cfg1_PM_Enable = 0x01,
	Cfg1_VPD_Enable = 0x02,
	Cfg1_PIO = 0x04,
	Cfg1_MMIO = 0x08,
	Cfg1_LWAKE = 0x10,
	Cfg1_Driver_Load = 0x20,
	Cfg1_LED0 = 0x40,
	Cfg1_LED1 = 0x80,
};

enum RxConfigBits {
	/* Early Rx threshold, none or X/16 */
	RxCfgEarlyRxNone = 0,
	RxCfgEarlyRxShift = 24,

	/* rx fifo threshold */
	RxCfgFIFOShift = 13,
	RxCfgFIFONone = (7 << RxCfgFIFOShift),

	/* Max DMA burst */
	RxCfgDMAShift = 8,
	RxCfgDMAUnlimited = (7 << RxCfgDMAShift),

	/* rx ring buffer length */
	RxCfgRcv8K = 0,
	RxCfgRcv16K = (1 << 11),
	RxCfgRcv32K = (1 << 12),
	RxCfgRcv64K = (1 << 11) | (1 << 12),

	/* Disable packet wrap at end of Rx buffer */
	RxNoWrap = (1 << 7),
};


/* Twister tuning parameters from RealTek.
   Completely undocumented, but required to tune bad links. */
enum CSCRBits {
	CSCR_LinkOKBit = 0x0400,
	CSCR_LinkChangeBit = 0x0800,
	CSCR_LinkStatusBits = 0x0f000,
	CSCR_LinkDownOffCmd = 0x003c0,
	CSCR_LinkDownCmd = 0x0f3c0,
};


enum Cfg9346Bits {
	Cfg9346_Lock = 0x00,
	Cfg9346_Unlock = 0xC0,
};

enum NegotiationBits {
	AutoNegotiationEnable  = 0x1000,
	AutoNegotiationRestart = 0x0200,
	AutoNegoAbility10half  = 0x21,
	AutoNegoAbility10full  = 0x41,
	AutoNegoAbility100half  = 0x81,
	AutoNegoAbility100full  = 0x101,
};

enum MediaStatusBits {
	DuplexMode = 0x0100,	//in BasicModeControlRegister
	Speed_10 = 0x08,		//in Media Status Register
};

#define PARA78_default	0x78fa8388
#define PARA7c_default	0xcb38de43	/* param[0][3] */
#define PARA7c_xxx		0xcb38de43
static const unsigned long param[4][4] = {
	{0xcb39de43, 0xcb39ce43, 0xfb38de03, 0xcb38de43},
	{0xcb39de43, 0xcb39ce43, 0xcb39ce83, 0xcb39ce83},
	{0xcb39de43, 0xcb39ce43, 0xcb39ce83, 0xcb39ce83},
	{0xbb39de43, 0xbb39ce43, 0xbb39ce83, 0xbb39ce83}
};

struct ring_info {
	struct sk_buff *skb;
	dma_addr_t mapping;
};


typedef enum {
	CH_8139 = 0,
	CH_8139_K,
	CH_8139A,
	CH_8139B,
	CH_8130,
	CH_8139C,
	CH_8139CP,
} chip_t;


/* directly indexed by chip_t, above */
const static struct {
	const char *name;
	u8 version; /* from RTL8139C docs */
	u32 RxConfigMask; /* should clear the bits supported by this chip */
} rtl_chip_info[] = {
	{ "RTL-8139",
	  0x40,
	  0xf0fe0040, /* XXX copied from RTL8139A, verify */
	},

	{ "RTL-8139 rev K",
	  0x60,
	  0xf0fe0040,
	},

	{ "RTL-8139A",
	  0x70,
	  0xf0fe0040,
	},

	{ "RTL-8139B",
	  0x78,
	  0xf0fc0040
	},

	{ "RTL-8130",
	  0x7C,
	  0xf0fe0040, /* XXX copied from RTL8139A, verify */
	},

	{ "RTL-8139C",
	  0x74,
	  0xf0fc0040, /* XXX copied from RTL8139B, verify */
	},

	{ "RTL-8139CP",
	  0x76,
	  0xf0fc0040, /* XXX copied from RTL8139B, verify */
	},
};

struct CPlusTxDesc {
	u32		status;
	u32		vlan_tag;
	u64		buf_addr;
};

struct CPlusRxDesc {
	u32		status;
	u32		vlan_tag;
	u64		buf_addr;
};
/********************************************************************** */
#define IP_PROTOCOL            0x0008   /* Network Byte order */
#define TCP_PROTOCOL    6
#define UDP_PROTOCOL    17
#define ADDRESS_LEN             6

struct ethernet_ii_header {
    uint8_t DestAddr[ADDRESS_LEN];
    uint8_t SourceAddr[ADDRESS_LEN];
    uint16_t TypeLength;
};
/********************************************************************** */
/* Internet Protocol Version 4 (IPV4) Header Definition */
/********************************************************************** */

typedef struct _ip_v4_header_ {
    uint32_t HdrLength:4,           /* Network Byte order */
        HdrVersion:4,              /* Network Byte order */
        TOS:8,                       /* Network Byte order */
        Length:16;                 /* Network Byte order */

    union {
        uint32_t IdThruOffset;

        struct {
            uint32_t DatagramId:16, FragOffset1:5,   /* Network Byte order */
                MoreFragments:1, NoFragment:1, Reserved2:1, FragOffset:8;
        } Bits;

        struct {
            uint32_t DatagramId:16, FragsNFlags:16;   /* Network Byte order */
        } Fields;

    } FragmentArea;

    uint8_t TimeToLive;
    uint8_t ProtocolCarried;
    uint16_t Checksum;

    union {
        uint32_t SourceIPAddress;

        struct {
            uint32_t SrcAddrLo:16,     /* Network Byte order */
	             SrcAddrHi:16;
        } Fields;

        struct {
            uint32_t OctA:8, OctB:8, OctC:8, OctD:8;
        } SFields;

    } SrcAddr;

    union {
        uint32_t TargetIPAddress;

        struct {
            uint32_t DestAddrLo:16,     /* Network Byte order */
                DestAddrHi:16;
        } Fields;

        struct {
            uint32_t OctA:8, OctB:8, OctC:8, OctD:8;
        } DFields;

    } DestAddr;

    uint16_t IpOptions[20];   /* Maximum of 40 bytes (20 words) of IP options  */

} ip_v4_header;
/********************************************************************** */

struct rtl8139_private {
	void *mmio_addr;
	int drv_flags;
	struct pci_dev *pci_dev;
	struct net_device_stats stats;
	unsigned char *rx_ring;
	unsigned int cur_rx;	/* Index into the Rx buffer of next Rx pkt. */
	unsigned int tx_flag;
	unsigned long cur_tx;
	unsigned long dirty_tx;
	/* The saved address of a sent-in-place packet/buffer, for skfree(). */
	struct ring_info tx_info[NUM_TX_DESC];
	unsigned char *tx_buf[NUM_TX_DESC];	/* Tx bounce buffers */
	unsigned char *tx_bufs;	/* Tx bounce buffer region. */
	dma_addr_t rx_ring_dma;
	dma_addr_t tx_bufs_dma;
	signed char phys[4];		/* MII device addresses. */
	u16 advertising;		/* NWay media advertisement */
	char twistie, twist_row, twist_col;	/* Twister tune state. */
	unsigned int full_duplex:1;	/* Full-duplex operation requested. */
	unsigned int duplex_lock:1;
	unsigned int default_port:4;	/* Last dev->if_port value. */
	unsigned int media2:4;	/* Secondary monitored media port. */
	unsigned int medialock:1;	/* Don't sense media type. */
	unsigned int mediasense:1;	/* Media sensing in progress. */
	spinlock_t lock;
	chip_t chipset;
	pid_t thr_pid;
	wait_queue_head_t thr_wait;

#if LINUX_VERSION_CODE < 0x20407
	struct semaphore thr_exited;
#else
	struct completion thr_exited;
#endif


  // For 8139C+
	int AutoNegoAbility;
	unsigned long CP_cur_tx;
	unsigned long CP_dirty_tx;
	unsigned long CP_cur_rx;
	unsigned char *TxDescArrays;
	dma_addr_t TxDescArraysDMA;
	unsigned char *RxDescArrays;
	dma_addr_t RxDescArraysDMA;
	struct CPlusTxDesc *TxDescArray;
	struct CPlusRxDesc *RxDescArray;
	unsigned char *RxBufferRings;
	dma_addr_t RxBufferRingsDMA;
	unsigned char *RxBufferRing[NUM_CP_RX_DESC];
	struct ring_info TxRingInfo[NUM_CP_TX_DESC];

	atomic_t CRC32DoubleCheck_ERR;
};

MODULE_AUTHOR ("Jeff Garzik <jgarzik@mandrakesoft.com>");
MODULE_DESCRIPTION ("RealTek RTL-8139CP Fast Ethernet driver");
MODULE_LICENSE("GPL");
MODULE_PARM (multicast_filter_limit, "i");
MODULE_PARM (max_interrupt_work, "i");
MODULE_PARM (media, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM (full_duplex, "1-" __MODULE_STRING(MAX_UNITS) "i");

static int read_eeprom (void *ioaddr, int location, int addr_len);

static int rtl8139CP_open (struct net_device *dev);
static int rtl8139CP_start_xmit (struct sk_buff *skb, struct net_device *dev);
static void rtl8139CP_interrupt (int irq, void *dev_instance, struct pt_regs *regs);
static void rtl8139CP_init_ring (struct net_device *dev);
static void rtl8139CP_hw_start (struct net_device *dev);
static int rtl8139CP_close (struct net_device *dev);	       
static void rtl8139CP_tx_timeout (struct net_device *dev);

static int rtl8139_open (struct net_device *dev);
static int rtl8139_start_xmit (struct sk_buff *skb, struct net_device *dev);
static void rtl8139_interrupt (int irq, void *dev_instance, struct pt_regs *regs);
static void rtl8139_init_ring (struct net_device *dev);
static void rtl8139_hw_start (struct net_device *dev);
static int rtl8139_close (struct net_device *dev);
static void rtl8139_tx_timeout (struct net_device *dev);

static int mdio_read (struct net_device *dev, int phy_id, int location);
static void mdio_write (struct net_device *dev, int phy_id, int location, int val);
static int mii_ioctl (struct net_device *dev, struct ifreq *rq, int cmd);

static struct net_device_stats *rtl8139_get_stats (struct net_device *dev);
static inline u32 ether_crc (int length, unsigned char *data);
static void rtl8139_set_rx_mode (struct net_device *dev);

#ifdef USE_IO_OPS

#define RTL_R8(reg)		inb (((unsigned long)ioaddr) + (reg))
#define RTL_R16(reg)		inw (((unsigned long)ioaddr) + (reg))
#define RTL_R32(reg)		((unsigned long) inl (((unsigned long)ioaddr) + (reg)))
#define RTL_W8(reg, val8)	outb ((val8), ((unsigned long)ioaddr) + (reg))
#define RTL_W16(reg, val16)	outw ((val16), ((unsigned long)ioaddr) + (reg))
#define RTL_W32(reg, val32)	outl ((val32), ((unsigned long)ioaddr) + (reg))
#define RTL_W8_F		RTL_W8
#define RTL_W16_F		RTL_W16
#define RTL_W32_F		RTL_W32
#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb(addr) inb((unsigned long)(addr))
#define readw(addr) inw((unsigned long)(addr))
#define readl(addr) inl((unsigned long)(addr))
#define writeb(val,addr) outb((val),(unsigned long)(addr))
#define writew(val,addr) outw((val),(unsigned long)(addr))
#define writel(val,addr) outl((val),(unsigned long)(addr))

#else

/* write MMIO register, with flush */
/* Flush avoids rtl8139 bug w/ posted MMIO writes */
#define RTL_W8_F(reg, val8)	do { writeb ((val8), ioaddr + (reg)); readb (ioaddr + (reg)); } while (0)
#define RTL_W16_F(reg, val16)	do { writew ((val16), ioaddr + (reg)); readw (ioaddr + (reg)); } while (0)
#define RTL_W32_F(reg, val32)	do { writel ((val32), ioaddr + (reg)); readl (ioaddr + (reg)); } while (0)


#if MMIO_FLUSH_AUDIT_COMPLETE

/* write MMIO register */
#define RTL_W8(reg, val8)	writeb ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16)	writew ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32)	writel ((val32), ioaddr + (reg))

#else

/* write MMIO register, then flush */
#define RTL_W8		RTL_W8_F
#define RTL_W16		RTL_W16_F
#define RTL_W32		RTL_W32_F

#endif /* MMIO_FLUSH_AUDIT_COMPLETE */

/* read MMIO register */
#define RTL_R8(reg)		readb (ioaddr + (reg))
#define RTL_R16(reg)		readw (ioaddr + (reg))
#define RTL_R32(reg)		((unsigned long) readl (ioaddr + (reg)))

#endif /* USE_IO_OPS */


static const u16 rtl8139_intr_mask =
	PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver |
	TxErr | TxOK | RxErr | RxOK;

static const unsigned int rtl8139_rx_config =
	  RxCfgEarlyRxNone | RxCfgRcv32K | RxNoWrap |
	  (RX_FIFO_THRESH << RxCfgFIFOShift) |
	  (RX_DMA_BURST << RxCfgDMAShift);



//==========================================================================================
unsigned long crc32_table[256];
#define CRC32_POLY 0x04c11db7
#define ReverseBit(data) ( ((data<<7)&0x80) | ((data<<5)&0x40) | ((data<<3)&0x20) | ((data<<1)&0x10) |\
                	   ((data>>1)&0x08) | ((data>>3)&0x04) | ((data>>5)&0x02) | ((data>>7)&0x01)	)
//==========================================================================================
void init_crc32( struct net_device *dev )
{
        int i, j;
        unsigned long c;
        unsigned char *p=(unsigned char *)&c, *p1;
        unsigned char k;
	struct rtl8139_private *tp = dev->priv;

	;
	atomic_set( &tp->CRC32DoubleCheck_ERR, 0 );

        c = 0x12340000;

        for (i = 0; i < 256; ++i) 
        {
			k = ReverseBit((unsigned char)i);
			for (c = ((unsigned long)k) << 24, j = 8; j > 0; --j)
				c = c & 0x80000000 ? (c << 1) ^ CRC32_POLY : (c << 1);
			p1 = (unsigned char *)(&crc32_table[i]);

			p1[0] = ReverseBit(p[3]);
			p1[1] = ReverseBit(p[2]);
			p1[2] = ReverseBit(p[1]);
			p1[3] = ReverseBit(p[0]);
        }
}

unsigned long crc32(unsigned char *buf, int len)
{
        unsigned char *p;
        unsigned long  crc;

        crc = 0xffffffff;       /* preload shift register, per CRC-32 spec */

        for (p = buf; len > 0; ++p, --len)
        {
			crc = crc32_table[ (crc ^ *p) & 0xff] ^ (crc >> 8);
        }
        return ~crc;    /* transmit complement, per CRC-32 spec */
}


//int DoubleCheck( &rx_ring[ring_offset + 4], pkt_size )
//----------------------------------------------------------------------
// rx_data_ptr	:	the start pointer of the received data
// pkt_size	:	the received data length excluding 4-bytes CRC
//----------------------------------------------------------------------
static inline int CRC32DoubleCheck( unsigned char *rx_data_ptr, unsigned int pkt_size )
{
	unsigned short	EtherType = *( (unsigned short *)(rx_data_ptr + 12) );
	unsigned char	IPType = *(rx_data_ptr + 23);
	unsigned long	SWCRC = 0xffffffff;
	unsigned long	HWCRC;

	//----------------------------------------------------------------------
	// If this is the packet of type TCP/IP, skip CRC32 double-check.
	//----------------------------------------------------------------------
	if( (EtherType == 0x0008) && (IPType == 0x06) ){
		return 0;
	}

	HWCRC = *( (unsigned long *)(rx_data_ptr + pkt_size) );

	SWCRC = crc32( rx_data_ptr, pkt_size );

//	printk(KERN_INFO "RTL8139: %s( pkt_size = %d, EtherType = 0x%4.4x, IPType = 0x%2.2x). HWCRC = 0x%8.8lx, SWCRC = 0x%8.8lx\n",
//				__FUNCTION__, pkt_size, EtherType, IPType, HWCRC, SWCRC );

	return ( (SWCRC == HWCRC) ? 0 : -1 );
}




#ifdef RTL8139_PROC_DEBUG
//==========================================================================================
struct proc_dir_entry *rtl8139_Proc_root;
struct proc_dir_entry *rtl8139_Proc;


static int rtl8139_proc_status(char *buffer,
		  	char **buffer_location,
		  	off_t offset,
		  	int buffer_length,
		  	int *eof,
		  	void *data)
{
	char *buf = buffer;
	struct net_device *dev = (struct net_device *)data;

	buf += sprintf( buf, " ************ Current driver status ************\n\n\n" );
	buf += sprintf( buf, " MAC address = %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
				dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2], dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5] );

	buf += sprintf( buf, " ioaddr = 0x%8.8lx\n",  (unsigned long)dev->base_addr );
	buf += sprintf( buf, " irq = %d\n",  dev->irq );

	return (buf - buffer);
}


static void rtl8139_init_proc(struct net_device *dev)
{
	if(rtl8139_Proc_root == NULL)
		rtl8139_Proc_root = proc_mkdir("rtl8139", NULL);
	if(rtl8139_Proc_root != NULL){
		rtl8139_Proc = create_proc_read_entry(
				"status",0644, rtl8139_Proc_root, rtl8139_proc_status, (void *)dev);
	}
}

static void rtl8139_remove_proc(struct net_device *dev)
{
	if(rtl8139_Proc_root != NULL){
		remove_proc_entry("status", rtl8139_Proc_root );
		remove_proc_entry("rtl8139", NULL );
	}
}
//==========================================================================================
#endif	//#ifdef RTL8139_PROC_DEBUG





//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------
static int __devinit rtl8139_init_board (struct pci_dev *pdev, struct net_device **dev_out, void **ioaddr_out)
{
	void *ioaddr = NULL;
	struct net_device *dev;
	struct rtl8139_private *tp;
	u8 tmp8;
	int rc, i;
	u32 pio_start, pio_end, pio_flags, pio_len;
	unsigned long mmio_start, mmio_end, mmio_flags, mmio_len;
	u32 tmp;


	DPRINTK ("ENTER\n");

	assert (pdev != NULL);
	assert (ioaddr_out != NULL);

	*ioaddr_out = NULL;
	*dev_out = NULL;

	/* dev zeroed in init_etherdev */
	dev = init_etherdev (NULL, sizeof (*tp));
	if (dev == NULL) {
		printk (KERN_ERR PFX "unable to alloc new ethernet\n");
		DPRINTK ("EXIT, returning -ENOMEM\n");
		return -ENOMEM;
	}
	SET_MODULE_OWNER(dev);
	tp = dev->priv;

	/* enable device (incl. PCI PM wakeup and hotplug setup) */
	rc = pci_enable_device (pdev);
	if (rc)
		goto err_out;

	pio_start = pci_resource_start (pdev, 0);
	pio_end = pci_resource_end (pdev, 0);
	pio_flags = pci_resource_flags (pdev, 0);
	pio_len = pci_resource_len (pdev, 0);

	mmio_start = pci_resource_start (pdev, 1);
	mmio_end = pci_resource_end (pdev, 1);
	mmio_flags = pci_resource_flags (pdev, 1);
	mmio_len = pci_resource_len (pdev, 1);

	/* set this immediately, we need to know before
	 * we talk to the chip directly */
	DPRINTK("PIO region size == 0x%02X\n", pio_len);
	DPRINTK("MMIO region size == 0x%02lX\n", mmio_len);
	if (pio_len == RTL8139B_IO_SIZE)
		tp->chipset = CH_8139B;

	/* make sure PCI base addr 0 is PIO */
	if (!(pio_flags & IORESOURCE_IO)) {
		printk (KERN_ERR PFX "region #0 not a PIO resource, aborting\n");
		rc = -ENODEV;
		goto err_out;
	}

	/* make sure PCI base addr 1 is MMIO */
	if (!(mmio_flags & IORESOURCE_MEM)) {
		printk (KERN_ERR PFX "region #1 not an MMIO resource, aborting\n");
		rc = -ENODEV;
		goto err_out;
	}

	/* check for weird/broken PCI region reporting */
	if ((pio_len < RTL_MIN_IO_SIZE) ||
	    (mmio_len < RTL_MIN_IO_SIZE)) {
		printk (KERN_ERR PFX "Invalid PCI region size(s), aborting\n");
		rc = -ENODEV;
		goto err_out;
	}


#ifndef USE_IO_OPS
	rc = pci_request_regions (pdev, dev->name);
	if (rc)
		goto err_out;
#endif /* !USE_IO_OPS */



	/* enable PCI bus-mastering */
	pci_set_master (pdev);

#ifdef USE_IO_OPS
	ioaddr = (void *) pio_start;
	printk ("%s: use pci IO map. ioaddr=0x%08lx\n", dev->name, (unsigned long)ioaddr);
#else
	/* ioremap MMIO region */
	ioaddr = ioremap (mmio_start, mmio_len);
	if (ioaddr == NULL) {
		printk (KERN_ERR PFX "cannot remap MMIO, aborting\n");
		rc = -EIO;
		goto err_out_free_res;
	}
	printk ("%s: use memory map. ioaddr=0x%08lx\n", dev->name, (unsigned long)ioaddr);
#endif /* USE_IO_OPS */

	/* Soft reset the chip. */
	RTL_W8 (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) | CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--)
		if ((RTL_R8 (ChipCmd) & CmdReset) == 0)
			break;
		else
			udelay (10);

	/* Bring the chip out of low-power mode. */
	if (tp->chipset == CH_8139B) {
		RTL_W8 (Config1, RTL_R8 (Config1) & ~(1<<4));
		RTL_W8 (Config4, RTL_R8 (Config4) & ~(1<<2));
	} else {
		/* handle RTL8139A and RTL8139 cases */
		/* XXX from becker driver. is this right?? */
		RTL_W8 (Config1, 0);
	}

	/* make sure chip thinks PIO and MMIO are enabled */
	tmp8 = RTL_R8 (Config1);
	if ((tmp8 & Cfg1_PIO) == 0) {
		printk (KERN_ERR PFX "PIO not enabled, Cfg1=%02X, aborting\n", tmp8);
		rc = -EIO;
		goto err_out_iounmap;
	}
	if ((tmp8 & Cfg1_MMIO) == 0) {
		printk (KERN_ERR PFX "MMIO not enabled, Cfg1=%02X, aborting\n", tmp8);
		rc = -EIO;
		goto err_out_iounmap;
	}

	/* identify chip attached to board */
//	tmp = RTL_R8 (ChipVersion);
	tmp = RTL_R32 (TxConfig);
	tmp = ( (tmp&0x7c000000) + ( (tmp&0x00800000)<<2 ) )>>24;

	for (i = ARRAY_SIZE (rtl_chip_info) - 1; i >= 0; i--)
		if (tmp == rtl_chip_info[i].version) {
			tp->chipset = i;
			goto match;
		}

	/* if unknown chip, assume array element #0, original RTL-8139 in this case */
	printk (KERN_DEBUG PFX "PCI device %s: unknown chip version, assuming RTL-8139\n",
		pdev->slot_name);
	printk (KERN_DEBUG PFX "PCI device %s: TxConfig = 0x%lx\n", pdev->slot_name, RTL_R32 (TxConfig));
	tp->chipset = 0;

match:
#ifdef FORCE_CPlus_Mode
	if(tp->chipset >= (ARRAY_SIZE (rtl_chip_info) - 2))	
		tp->chipset = ARRAY_SIZE (rtl_chip_info) - 1;
#endif
#ifdef FORCE_C_Mode
	if(tp->chipset > (ARRAY_SIZE (rtl_chip_info) - 2))
		tp->chipset = ARRAY_SIZE (rtl_chip_info) - 2;
#endif

	DPRINTK ("chipset id (%d) == index %d, '%s'\n",
		tmp,
		tp->chipset,
		rtl_chip_info[tp->chipset].name);

	DPRINTK ("EXIT, returning 0\n");
	*ioaddr_out = ioaddr;
	*dev_out = dev;
	return 0;

err_out_iounmap:
	assert (ioaddr > 0);
#ifndef USE_IO_OPS
	iounmap (ioaddr);
err_out_free_res:
	pci_release_regions (pdev);
#endif /* !USE_IO_OPS */
err_out:
	unregister_netdev (dev);
	kfree (dev);
	DPRINTK ("EXIT, returning %d\n", rc);
	return rc;
}


static int __devinit rtl8139_init_one (struct pci_dev *pdev,
				       const struct pci_device_id *ent)
{
	struct net_device *dev = NULL;
	struct rtl8139_private *tp;
	int i, addr_len, option;
	void *ioaddr = NULL;
	static int board_idx = -1;
	static int printed_version;
	u8 tmp;

	
	DPRINTK ("ENTER\n");

	assert (pdev != NULL);
	assert (ent != NULL);

	board_idx++;

	if (!printed_version) {
		printk (KERN_INFO RTL8139_DRIVER_NAME " loaded\n");
		printed_version = 1;
	}

	i = rtl8139_init_board (pdev, &dev, &ioaddr);
	if (i < 0) {
		DPRINTK ("EXIT, returning %d\n", i);
		return i;
	}

	tp = dev->priv;

	assert (ioaddr != NULL);
	assert (dev != NULL);
	assert (tp != NULL);

	init_crc32( dev );

#if defined(CONFIG_MTD_NETtel) || defined(CONFIG_SH_SECUREEDGE5410)
	/* Don't rely on the eeprom, get MAC from chip. */
	for (i = 0; (i < 6); i++)
		dev->dev_addr[i] = readb(ioaddr + MAC0 + i);
#else
	addr_len = read_eeprom (ioaddr, 0, 8) == 0x8129 ? 8 : 6;
	for (i = 0; i < 3; i++)
		((u16 *) (dev->dev_addr))[i] =
		    le16_to_cpu (read_eeprom (ioaddr, i + 7, addr_len));
#endif

	/* The Rtl8139-specific entries in the device structure. */
	if( rtl_chip_info[tp->chipset].name == "RTL-8139CP" ){
		dev->open = rtl8139CP_open;
		dev->hard_start_xmit = rtl8139CP_start_xmit;
		dev->stop = rtl8139CP_close;
		dev->tx_timeout = rtl8139CP_tx_timeout;
		dev->features |= NETIF_F_IP_CSUM;	/* Can checksum only TCP/UDP over IPv4. */
		printk (KERN_DEBUG "%s: Identified 8139CP chip type '%s'. Support TCP/UDP checksumOffload\n",dev->name,rtl_chip_info[tp->chipset].name);

	}
	else{
		dev->open = rtl8139_open;
		dev->hard_start_xmit = rtl8139_start_xmit;
		dev->stop = rtl8139_close;
		dev->tx_timeout = rtl8139_tx_timeout;
		printk (KERN_DEBUG "%s:  Identified 8139CP chip type '%s'\n", dev->name, rtl_chip_info[tp->chipset].name);
	}


	dev->get_stats = rtl8139_get_stats;
	dev->set_multicast_list = rtl8139_set_rx_mode;
	dev->do_ioctl = mii_ioctl;

	dev->watchdog_timeo = TX_TIMEOUT;
	dev->irq = pdev->irq;
	dev->base_addr = (unsigned long) ioaddr;
	/* dev->priv/tp zeroed and aligned in init_etherdev */
	tp = dev->priv;
	/* note: tp->chipset set in rtl8139_init_board */
	tp->drv_flags = board_info[ent->driver_data].hw_flags;
	tp->pci_dev = pdev;
	tp->mmio_addr = ioaddr;
	spin_lock_init (&tp->lock);
	init_waitqueue_head (&tp->thr_wait);

#if LINUX_VERSION_CODE < 0x20407
	init_MUTEX_LOCKED (&tp->thr_exited);
#else
	init_completion (&tp->thr_exited);
#endif

	pdev->driver_data = dev;

	printk (KERN_INFO "%s: %s at 0x%lx, "
		"%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
		"IRQ %d\n",
		dev->name,
		board_info[ent->driver_data].name,
		dev->base_addr,
		dev->dev_addr[0], dev->dev_addr[1],
		dev->dev_addr[2], dev->dev_addr[3],
		dev->dev_addr[4], dev->dev_addr[5],
		dev->irq);

//	dev->tx_queue_len=500;

	/* Find the connected MII xcvrs.
	   Doing this in open() would allow detecting external xcvrs later, but
	   takes too much time. */
	if (tp->drv_flags & HAS_MII_XCVR) {
		int phy, phy_idx = 0;
		for (phy = 0; phy < 32 && phy_idx < sizeof(tp->phys); phy++) {
			int mii_status = mdio_read(dev, phy, 1);
			if (mii_status != 0xffff  &&  mii_status != 0x0000) {
				tp->phys[phy_idx++] = phy;
				tp->advertising = mdio_read(dev, phy, 4);
				printk(KERN_INFO "%s: MII transceiver %d status 0x%4.4x "
					   "advertising %4.4x.\n",
					   dev->name, phy, mii_status, tp->advertising);
			}
		}
		if (phy_idx == 0) {
			printk(KERN_INFO "%s: No MII transceivers found!  Assuming SYM "
				   "transceiver.\n",
				   dev->name);
			tp->phys[0] = 32;
		}
	} else
		tp->phys[0] = 32;

	/* Put the chip into low-power mode. */
	RTL_W8_F (Cfg9346, Cfg9346_Unlock);

	tmp = RTL_R8 (Config1) & Config1Clear;
	tmp |= (tp->chipset == CH_8139B) ? 3 : 1; /* Enable PM/VPD */
	RTL_W8_F (Config1, tmp);

	RTL_W8_F (HltClk, 'H');	/* 'R' would leave the clock running. */

	/* The lower four bits are the media type. */
	option = (board_idx >= MAX_UNITS) ? 0 : media[board_idx];
	tp->AutoNegoAbility = option&0xF;

	if (option > 0) {
		tp->full_duplex = (option & 0x210) ? 1 : 0;
		tp->default_port = option & 0xFF;
		if (tp->default_port)
			tp->medialock = 1;
	}
	if (board_idx < MAX_UNITS  &&  full_duplex[board_idx] > 0)
		tp->full_duplex = full_duplex[board_idx];
	if (tp->full_duplex) {
		printk(KERN_INFO "%s: Media type forced to Full Duplex.\n", dev->name);
		/* Changing the MII-advertised media because might prevent
		   re-connection. */
		tp->duplex_lock = 1;
	}

	if (tp->default_port) {
		printk(KERN_INFO "%s: Forcing %dMbs %s-duplex operation.\n", dev->name,
			   (option & 0x0C ? 100 : 10),
			   (option & 0x0A ? "full" : "half"));
		mdio_write(dev, tp->phys[0], 0,
				   ((option & 0x20) ? 0x2000 : 0) | 	/* 100mbps? */
				   ((option & 0x10) ? 0x0100 : 0)); /* Full duplex? */
	}


#ifdef RTL8139_PROC_DEBUG
	//-----------------------------------------------------------------------------
	// Add proc file system
	//-----------------------------------------------------------------------------
	rtl8139_init_proc(dev);
#endif	//#ifdef RTL8139_PROC_DEBUG


	DPRINTK ("EXIT - returning 0\n");
	return 0;
}


static void __devexit rtl8139_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pdev->driver_data;
	struct rtl8139_private *np;


	DPRINTK ("ENTER\n");

	assert (dev != NULL);

	np = (struct rtl8139_private *) (dev->priv);
	assert (np != NULL);


#ifndef USE_IO_OPS
	iounmap (np->mmio_addr);
	pci_release_regions (pdev);
#endif /* !USE_IO_OPS */

#ifdef RTL8139_PROC_DEBUG
	//-----------------------------------------------------------------------------
	// Remove proc file system
	//-----------------------------------------------------------------------------
	rtl8139_remove_proc(dev);
#endif	//#ifdef RTL8139_PROC_DEBUG


	unregister_netdev (dev);

	pdev->driver_data = NULL;

	pci_disable_device (pdev);

	DPRINTK ("EXIT\n");
}


/* Serial EEPROM section. */

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x04	/* EEPROM shift clock. */
#define EE_CS			0x08	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x02	/* EEPROM chip data in. */
#define EE_WRITE_0		0x00
#define EE_WRITE_1		0x02
#define EE_DATA_READ	0x01	/* EEPROM chip data out. */
#define EE_ENB			(0x80 | EE_CS)

/* Delay between EEPROM clock transitions.
   No extra delay is needed with 33Mhz PCI, but 66Mhz may change this.
 */

#define eeprom_delay()	readl(ee_addr)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5)
#define EE_READ_CMD		(6)
#define EE_ERASE_CMD	(7)

static int __devinit read_eeprom (void *ioaddr, int location, int addr_len)
{
	int i;
	unsigned retval = 0;
	void *ee_addr = ioaddr + Cfg9346;
	int read_cmd = location | (EE_READ_CMD << addr_len);

	DPRINTK ("ENTER\n");

	writeb (EE_ENB & ~EE_CS, ee_addr);
	writeb (EE_ENB, ee_addr);
	eeprom_delay ();

	/* Shift the read command bits out. */
	for (i = 4 + addr_len; i >= 0; i--) {
		int dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		writeb (EE_ENB | dataval, ee_addr);
		eeprom_delay ();
		writeb (EE_ENB | dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay ();
	}
	writeb (EE_ENB, ee_addr);
	eeprom_delay ();

	for (i = 16; i > 0; i--) {
		writeb (EE_ENB | EE_SHIFT_CLK, ee_addr);
		eeprom_delay ();
		retval =
		    (retval << 1) | ((readb (ee_addr) & EE_DATA_READ) ? 1 :
				     0);
		writeb (EE_ENB, ee_addr);
		eeprom_delay ();
	}

	/* Terminate the EEPROM access. */
	writeb (~EE_CS, ee_addr);
	eeprom_delay ();

	DPRINTK ("EXIT - returning %d\n", retval);
	return retval;
}

/* MII serial management: mostly bogus for now. */
/* Read and write the MII management registers using software-generated
   serial MDIO protocol.
   The maximum data clock rate is 2.5 Mhz.  The minimum timing is usually
   met by back-to-back PCI I/O cycles, but we insert a delay to avoid
   "overclocking" issues. */
#define MDIO_DIR		0x80
#define MDIO_DATA_OUT	0x04
#define MDIO_DATA_IN	0x02
#define MDIO_CLK		0x01
#define MDIO_WRITE0 (MDIO_DIR)
#define MDIO_WRITE1 (MDIO_DIR | MDIO_DATA_OUT)

#define mdio_delay(mdio_addr)	readb(mdio_addr)


static char mii_2_8139_map[8] = {
	BasicModeCtrl,
	BasicModeStatus,
	0,
	0,
	NWayAdvert,
	NWayLPAR,
	NWayExpansion,
	0
};


/* Syncronize the MII management interface by shifting 32 one bits out. */
static void mdio_sync (void *mdio_addr)
{
	int i;

	DPRINTK ("ENTER\n");

	for (i = 32; i >= 0; i--) {
		writeb (MDIO_WRITE1, mdio_addr);
		mdio_delay (mdio_addr);
		writeb (MDIO_WRITE1 | MDIO_CLK, mdio_addr);
		mdio_delay (mdio_addr);
	}

	DPRINTK ("EXIT\n");
}


static int mdio_read (struct net_device *dev, int phy_id, int location)
{
	struct rtl8139_private *tp = dev->priv;
	void *mdio_addr = tp->mmio_addr + Config4;
	int mii_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	int retval = 0;
	int i;

	DPRINTK ("ENTER\n");

	if (phy_id > 31) {	/* Really a 8139.  Use internal registers. */
		DPRINTK ("EXIT after directly using 8139 internal regs\n");
		return location < 8 && mii_2_8139_map[location] ?
		    readw (tp->mmio_addr + mii_2_8139_map[location]) : 0;
	}
	mdio_sync (mdio_addr);
	/* Shift the read command bits out. */
	for (i = 15; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDIO_DATA_OUT : 0;

		writeb (MDIO_DIR | dataval, mdio_addr);
		mdio_delay (mdio_addr);
		writeb (MDIO_DIR | dataval | MDIO_CLK, mdio_addr);
		mdio_delay (mdio_addr);
	}

	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; i--) {
		writeb (0, mdio_addr);
		mdio_delay (mdio_addr);
		retval = (retval << 1) | ((readb (mdio_addr) & MDIO_DATA_IN) ? 1 : 0);
		writeb (MDIO_CLK, mdio_addr);
		mdio_delay (mdio_addr);
	}

	DPRINTK ("EXIT, returning %d\n", (retval >> 1) & 0xffff);
	return (retval >> 1) & 0xffff;
}


static void mdio_write (struct net_device *dev, int phy_id, int location,
			int value)
{
	struct rtl8139_private *tp = dev->priv;
	void *mdio_addr = tp->mmio_addr + Config4;
	int mii_cmd = (0x5002 << 16) | (phy_id << 23) | (location << 18) | value;
	int i;

	DPRINTK ("ENTER\n");

	if (phy_id > 31) {	/* Really a 8139.  Use internal registers. */
		void *ioaddr = tp->mmio_addr;
		if (location == 0) {
			RTL_W8_F (Cfg9346, Cfg9346_Unlock);
			//////////////////////////////////////////////////////////////
			switch(tp->AutoNegoAbility){
			case 1: RTL_W16 (NWayAdvert, AutoNegoAbility10half); break;
			case 2: RTL_W16 (NWayAdvert, AutoNegoAbility10full); break;
			case 4: RTL_W16 (NWayAdvert, AutoNegoAbility100half); break;
			case 8: RTL_W16 (NWayAdvert, AutoNegoAbility100full); break;
			default: break;
			}
			RTL_W16_F (BasicModeCtrl, AutoNegotiationEnable|AutoNegotiationRestart);
			//////////////////////////////////////////////////////////////
//			RTL_W16_F (BasicModeCtrl, value);
			RTL_W8_F (Cfg9346, Cfg9346_Lock);
		} else if (location < 8 && mii_2_8139_map[location])
			RTL_W16_F (mii_2_8139_map[location], value);

		return;
	}
	mdio_sync (mdio_addr);

	/* Shift the command bits out. */
	for (i = 31; i >= 0; i--) {
		int dataval =
		    (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;
		writeb (dataval, mdio_addr);
		mdio_delay (mdio_addr);
		writeb (dataval | MDIO_CLK, mdio_addr);
		mdio_delay (mdio_addr);
	}
	/* Clear out extra bits. */
	for (i = 2; i > 0; i--) {
		writeb (0, mdio_addr);
		mdio_delay (mdio_addr);
		writeb (MDIO_CLK, mdio_addr);
		mdio_delay (mdio_addr);
	}
	return;
}

static int rtl8139_open (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	int retval;
#ifdef RTL8139_DEBUG
	void *ioaddr = tp->mmio_addr;
#endif

	DPRINTK ("ENTER\n");

	retval = request_irq (dev->irq, rtl8139_interrupt, SA_SHIRQ, dev->name, dev);
	if (retval) {
		DPRINTK ("EXIT, returning %d\n", retval);
		return retval;
	}

	tp->tx_bufs = pci_alloc_consistent(tp->pci_dev, TX_BUF_TOT_LEN,
					   &tp->tx_bufs_dma);
	tp->rx_ring = pci_alloc_consistent(tp->pci_dev, RX_BUF_TOT_LEN,
					   &tp->rx_ring_dma);
	if (tp->tx_bufs == NULL || tp->rx_ring == NULL) {
		free_irq(dev->irq, dev);

		if (tp->tx_bufs)
			pci_free_consistent(tp->pci_dev, TX_BUF_TOT_LEN,
					    tp->tx_bufs, tp->tx_bufs_dma);
		if (tp->rx_ring)
			pci_free_consistent(tp->pci_dev, RX_BUF_TOT_LEN,
					    tp->rx_ring, tp->rx_ring_dma);

		DPRINTK ("EXIT, returning -ENOMEM\n");
		return -ENOMEM;

	}

	tp->full_duplex = tp->duplex_lock;
	tp->tx_flag = (TX_FIFO_THRESH << 11) & 0x003f0000;
	tp->twistie = 1;

	rtl8139_init_ring (dev);
	rtl8139_hw_start (dev);

	DPRINTK ("%s: rtl8139_open() ioaddr %#lx IRQ %d"
			" GP Pins %2.2x %s-duplex.\n",
			dev->name, pci_resource_start (tp->pci_dev, 1),
			dev->irq, RTL_R8 (MediaStatus),
			tp->full_duplex ? "full" : "half");


	DPRINTK ("EXIT, returning 0\n");
	return 0;
}


/* Start the hardware at open or resume. */
static void rtl8139_hw_start (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	u32 i;
	u8 tmp;

	DPRINTK ("ENTER\n");

	/* Soft reset the chip. */
	RTL_W8 (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) | CmdReset);
	udelay (100);

	/* Check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--)
		if ((RTL_R8 (ChipCmd) & CmdReset) == 0)
			break;

	/* unlock Config[01234] and BMCR register writes */
	RTL_W8_F (Cfg9346, Cfg9346_Unlock);
	/* Restore our idea of the MAC address. */
	RTL_W32_F (MAC0 + 0, cpu_to_le32 (*(u32 *) (dev->dev_addr + 0)));
	RTL_W32_F (MAC0 + 4, cpu_to_le32 (*(u32 *) (dev->dev_addr + 4)));

	/* Must enable Tx/Rx before setting transfer thresholds! */
	RTL_W8_F (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) |
			   CmdRxEnb | CmdTxEnb);

	i = rtl8139_rx_config |
	    (RTL_R32 (RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);
	RTL_W32_F (RxConfig, i);

	/* Check this value: the documentation for IFG contradicts ifself. */
	RTL_W32 (TxConfig, (TX_DMA_BURST << TxDMAShift));

	tp->cur_rx = 0;

	/* This is check_duplex() */
	if (tp->phys[0] >= 0  ||  (tp->drv_flags & HAS_MII_XCVR)) {
		u16 mii_reg5 = mdio_read(dev, tp->phys[0], 5);
		if (mii_reg5 == 0xffff)
			;					/* Not there */
		else if ((mii_reg5 & 0x0100) == 0x0100
				 || (mii_reg5 & 0x00C0) == 0x0040)
			tp->full_duplex = 1;
		printk(KERN_INFO"%s: Setting %s%s-duplex based on"
			   " auto-negotiated partner ability %4.4x.\n", dev->name,
			   mii_reg5 == 0 ? "" :
			   (mii_reg5 & 0x0180) ? "100mbps " : "10mbps ",
			   tp->full_duplex ? "full" : "half", mii_reg5);
	}

	if (tp->chipset >= CH_8139A) {
		tmp = RTL_R8 (Config1) & Config1Clear;
		tmp |= Cfg1_Driver_Load;
		tmp |= (tp->chipset == CH_8139B) ? 3 : 1; /* Enable PM/VPD */
		RTL_W8_F (Config1, tmp);
	} else {
		u8 foo = RTL_R8 (Config1) & Config1Clear;
		RTL_W8 (Config1, tp->full_duplex ? (foo|0x60) : (foo|0x20));
	}

	if (tp->chipset >= CH_8139B) {
		tmp = RTL_R8 (Config4) & ~(1<<2);
		/* chip will clear Rx FIFO overflow automatically */
		tmp |= (1<<7);
		RTL_W8 (Config4, tmp);

		/* disable magic packet scanning, which is enabled
		 * when PM is enabled above (Config1) */
		RTL_W8 (Config3, RTL_R8 (Config3) & ~(1<<5));
	}

	/* Lock Config[01234] and BMCR register writes */
	RTL_W8_F (Cfg9346, Cfg9346_Lock);
	udelay (10);

	/* init Rx ring buffer DMA address */
	RTL_W32_F (RxBuf, tp->rx_ring_dma);

	/* init Tx buffer DMA addresses */
	for (i = 0; i < NUM_TX_DESC; i++)
		RTL_W32_F (TxAddr0 + (i * 4), tp->tx_bufs_dma + (tp->tx_buf[i] - tp->tx_bufs));

	RTL_W32_F (RxMissed, 0);

	rtl8139_set_rx_mode (dev);

	/* no early-rx interrupts */
	RTL_W16 (MultiIntr, RTL_R16 (MultiIntr) & MultiIntrClear);

	/* make sure RxTx has started */
	RTL_W8_F (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) |
			   CmdRxEnb | CmdTxEnb);

	/* Enable all known interrupts by setting the interrupt mask. */
	RTL_W16_F (IntrMask, rtl8139_intr_mask);

	netif_start_queue (dev);

	DPRINTK ("EXIT\n");
}


/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void rtl8139_init_ring (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	int i;

	DPRINTK ("ENTER\n");

	tp->cur_rx = 0;
	tp->cur_tx = 0;
	tp->dirty_tx = 0;

	for (i = 0; i < NUM_TX_DESC; i++) {
		tp->tx_info[i].skb = NULL;
		tp->tx_info[i].mapping = 0;
		tp->tx_buf[i] = &tp->tx_bufs[i * TX_BUF_SIZE];
	}

	DPRINTK ("EXIT\n");
}


static int rtl8139CP_open (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	int retval;
	u8 diff;			//for 8139C+
	u32 txPhyAddr, rxPhyAddr;	//for 8139C+

#ifdef RTL8139_DEBUG
	void *ioaddr = tp->mmio_addr;
#endif

	DPRINTK ("ENTER\n");

	retval = request_irq (dev->irq, rtl8139CP_interrupt, SA_SHIRQ, dev->name, dev);
	if (retval) {
		DPRINTK ("EXIT, returning %d\n", retval);
		return retval;
	}

	//////////////////////////////////////////////////////////////////////////////
	tp->TxDescArrays = pci_alloc_consistent(tp->pci_dev, NUM_CP_TX_DESC*sizeof(struct CPlusTxDesc)+256 ,
					   &tp->TxDescArraysDMA);
	// Tx Desscriptor needs 256 bytes alignment;
	txPhyAddr=tp->TxDescArraysDMA;
	diff=txPhyAddr-((txPhyAddr >> 8)<< 8);
	diff=256-diff;
	txPhyAddr +=diff;
	tp->TxDescArray = (struct CPlusTxDesc *)(tp->TxDescArrays + diff);

	tp->RxDescArrays = pci_alloc_consistent(tp->pci_dev, NUM_CP_RX_DESC*sizeof(struct CPlusRxDesc)+256 ,
					   &tp->RxDescArraysDMA);
	// Rx Desscriptor needs 256 bytes alignment;
	rxPhyAddr=tp->RxDescArraysDMA;
	diff=rxPhyAddr-((rxPhyAddr >> 8)<< 8);
	diff=256-diff;
	rxPhyAddr +=diff;
	tp->RxDescArray = (struct CPlusRxDesc *)(tp->RxDescArrays + diff);
	
	if (tp->TxDescArrays == NULL || tp->RxDescArrays == NULL) {
		printk(KERN_INFO"Allocate RxDescArray or TxDescArray failed\n");
		free_irq(dev->irq, dev);
		if (tp->TxDescArrays)
			pci_free_consistent(tp->pci_dev, NUM_CP_TX_DESC*sizeof(struct CPlusTxDesc)+256,
					   tp->TxDescArrays, tp->TxDescArraysDMA);
		if (tp->RxDescArrays)
			pci_free_consistent(tp->pci_dev, NUM_CP_RX_DESC*sizeof(struct CPlusRxDesc)+256,
					   tp->RxDescArrays, tp->RxDescArraysDMA);
		DPRINTK ("EXIT, returning -ENOMEM\n");
		return -ENOMEM;
	}
	tp->RxBufferRings=pci_alloc_consistent(tp->pci_dev, CP_RX_BUF_SIZE*NUM_CP_RX_DESC, &tp->RxBufferRingsDMA);
	if(tp->RxBufferRings==NULL){
		printk(KERN_INFO"Allocate RxBufferRing failed\n");
	}
	//////////////////////////////////////////////////////////////////////////////
	tp->full_duplex = tp->duplex_lock;
	tp->tx_flag = (TX_FIFO_THRESH << 11) & 0x003f0000;
	tp->twistie = 1;

	rtl8139CP_init_ring (dev);
	rtl8139CP_hw_start (dev);

	DPRINTK ("%s: rtl8139_open() ioaddr %#lx IRQ %d"
			" GP Pins %2.2x %s-duplex.\n",
			dev->name, pci_resource_start (tp->pci_dev, 1),
			dev->irq, RTL_R8 (MediaStatus),
			tp->full_duplex ? "full" : "half");


	DPRINTK ("EXIT, returning 0\n");
	return 0;
}


/* Start the hardware at open or resume. */
static void rtl8139CP_hw_start (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	u32 i;
	u8 tmp;
	u16 BasicModeCtrlReg;
	u8 MediaStatusReg;

	DPRINTK ("ENTER\n");

	/* Soft reset the chip. */
	RTL_W8 (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear) | CmdReset);
	udelay (100);

	/* Check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--)
		if ((RTL_R8 (ChipCmd) & CmdReset) == 0)
			break;

	/* unlock Config[01234] and BMCR register writes */
	RTL_W8_F (Cfg9346, Cfg9346_Unlock);

	RTL_W16 (CPlusCmd, CPlusTxEnb | CPlusRxEnb | CPlusCheckSumEnb);	//C+ mode only

	/* Restore our idea of the MAC address. */
	RTL_W32_F (MAC0 + 0, cpu_to_le32 (*(u32 *) (dev->dev_addr + 0)));
	RTL_W32_F (MAC0 + 4, cpu_to_le32 (*(u32 *) (dev->dev_addr + 4)));

	/* Must enable Tx/Rx before setting transfer thresholds! */
	RTL_W8 (ChipCmd,CmdRxEnb | CmdTxEnb);					//C+ mode only
	RTL_W8 (CPlusEarlyTxThldReg, CPlusEarlyTxThld);			//C+ mode only


	i = rtl8139_rx_config | (RTL_R32 (RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);
	RTL_W32_F (RxConfig, i);

	/* Check this value: the documentation for IFG contradicts ifself. */
	RTL_W32 (TxConfig, (TX_DMA_BURST << TxDMAShift)|0x03000000);

	tp->CP_cur_rx = 0;

/*
	//////////////////////////////////////////////////////////////
	RTL_W8_F (Cfg9346, Cfg9346_Unlock);
	RTL_W16 (NWayAdvert, AutoNegoAbility10half | AutoNegoAbility10full | AutoNegoAbility100half | AutoNegoAbility100full);
	RTL_W16_F (BasicModeCtrl, AutoNegotiationEnable|AutoNegotiationRestart);
	RTL_W8_F (Cfg9346, Cfg9346_Lock);
	//////////////////////////////////////////////////////////////
*/
	MediaStatusReg = RTL_R8(MediaStatus);
	BasicModeCtrlReg = RTL_R16(BasicModeCtrl);

	/* This is check_duplex() */
	if (tp->phys[0] >= 0  ||  (tp->drv_flags & HAS_MII_XCVR)) {
		u16 mii_reg5 = mdio_read(dev, tp->phys[0], 5);
		if (mii_reg5 == 0xffff)
			;					/* Not there */
		else if ((mii_reg5 & 0x0100) == 0x0100
				 || (mii_reg5 & 0x00C0) == 0x0040)
			tp->full_duplex = 1;
/*
		printk(KERN_INFO"%s: Setting %s%s-duplex based on"
			   " auto-negotiated partner ability %4.4x.\n", dev->name,
			   mii_reg5 == 0 ? "" :
			   (mii_reg5 & 0x0180) ? "100mbps " : "10mbps ",
			   tp->full_duplex ? "full" : "half", mii_reg5);
*/
		printk(KERN_INFO"%s: Setting %s%s-duplex based on"
			   " auto-negotiated partner ability %4.4x.\n", dev->name,
			   (MediaStatusReg & Speed_10) ? "10mbps " : "100mbps ",
			   (BasicModeCtrlReg & DuplexMode) ? "full" : "half", mii_reg5);
	}

	if (tp->chipset >= CH_8139A) {
		tmp = RTL_R8 (Config1) & Config1Clear;
		tmp |= Cfg1_Driver_Load;
		tmp |= (tp->chipset == CH_8139B) ? 3 : 1; /* Enable PM/VPD */
		RTL_W8_F (Config1, tmp);
	} else {
		u8 foo = RTL_R8 (Config1) & Config1Clear;
		RTL_W8 (Config1, tp->full_duplex ? (foo|0x60) : (foo|0x20));
	}

	if (tp->chipset >= CH_8139B) {
		tmp = RTL_R8 (Config4) & ~(1<<2);
		/* chip will clear Rx FIFO overflow automatically */
		tmp |= (1<<7);
		RTL_W8 (Config4, tmp);

		/* disable magic packet scanning, which is enabled
		 * when PM is enabled above (Config1) */
		RTL_W8 (Config3, RTL_R8 (Config3) & ~(1<<5));
	}

	RTL_W32 ( CPlusTxStartAddr, (unsigned char *)tp->TxDescArray - tp->TxDescArrays + tp->TxDescArraysDMA);	//C+ mode only
	RTL_W32 ( CPlusRxStartAddr, (unsigned char *)tp->RxDescArray - tp->RxDescArrays + tp->RxDescArraysDMA);	//C+ mode only	

	/* Lock Config[01234] and BMCR register writes */
	RTL_W8_F (Cfg9346, Cfg9346_Lock);
	udelay (10);

	RTL_W32_F (RxMissed, 0);

	rtl8139_set_rx_mode (dev);

	/* no early-rx interrupts */
	RTL_W16 (MultiIntr, RTL_R16 (MultiIntr) & MultiIntrClear);

	/* Enable all known interrupts by setting the interrupt mask. */
	RTL_W16_F (IntrMask, rtl8139_intr_mask);

	netif_start_queue (dev);

	DPRINTK ("EXIT\n");
}


/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void rtl8139CP_init_ring (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	int i;


	DPRINTK ("ENTER\n");

	//////////////////////////////////////////////////////////////////////////////
	tp->CP_cur_rx = 0;
	tp->CP_cur_tx = 0;
	tp->CP_dirty_tx = 0;
	memset(tp->TxDescArray, 0x0, NUM_CP_TX_DESC*sizeof(struct CPlusTxDesc));
	memset(tp->RxDescArray, 0x0, NUM_CP_RX_DESC*sizeof(struct CPlusRxDesc));

	for (i=0 ; i<NUM_CP_TX_DESC ; i++){
		tp->TxRingInfo[i].skb=NULL;
		tp->TxRingInfo[i].mapping = 0;
	}
	for (i=0; i <NUM_CP_RX_DESC; i++) {
		if(i==(NUM_CP_RX_DESC-1))
			tp->RxDescArray[i].status = cpu_to_le32(0xC0000000+CP_RX_BUF_SIZE);
		else
			tp->RxDescArray[i].status = cpu_to_le32(0x80000000+CP_RX_BUF_SIZE);

		tp->RxBufferRing[i]= &(tp->RxBufferRings[i*CP_RX_BUF_SIZE]);
		tp->RxDescArray[i].buf_addr=cpu_to_le64(tp->RxBufferRing[i] - tp->RxBufferRings + tp->RxBufferRingsDMA);
	}
	//////////////////////////////////////////////////////////////////////////////
	DPRINTK ("EXIT\n");
}





static void rtl8139_tx_clear (struct rtl8139_private *tp)
{
	int i;

	tp->cur_tx = 0;
	tp->dirty_tx = 0;

	/* Dump the unsent Tx packets. */
	for (i = 0; i < NUM_TX_DESC; i++) {
		struct ring_info *rp = &tp->tx_info[i];
		if (rp->mapping != 0) {
			pci_unmap_single (tp->pci_dev, rp->mapping,
					  rp->skb->len, PCI_DMA_TODEVICE);
			rp->mapping = 0;
		}
		if (rp->skb) {
			dev_kfree_skb (rp->skb);
			rp->skb = NULL;
			tp->stats.tx_dropped++;
		}
	}
}


static void rtl8139CP_tx_clear (struct rtl8139_private *tp)
{
	int i;

	tp->CP_cur_tx=0;
	for (i=0 ; i<NUM_CP_TX_DESC ; i++){
		struct ring_info *rp = &tp->TxRingInfo[i];
		if (rp->mapping != 0) {
			pci_unmap_single (tp->pci_dev, rp->mapping,
					  rp->skb->len, PCI_DMA_TODEVICE);
			rp->mapping = 0;
		}
		if (rp->skb) {
			dev_kfree_skb (rp->skb);
			rp->skb = NULL;
			tp->stats.tx_dropped++;
		}
	}
}


static void rtl8139_tx_timeout (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	u8 tmp8;

	DPRINTK ("%s: Transmit timeout, status %2.2x %4.4x "
		 "media %2.2x.\n", dev->name,
		 RTL_R8 (ChipCmd),
		 RTL_R16 (IntrStatus),
		 RTL_R8 (MediaStatus));

	/* disable Tx ASAP, if not already */
	tmp8 = RTL_R8 (ChipCmd);
	if (tmp8 & CmdTxEnb)
		RTL_W8 (ChipCmd, tmp8 & ~CmdTxEnb);

	/* Disable interrupts by clearing the interrupt mask. */
	RTL_W16 (IntrMask, 0x0000);

	/* Stop a shared interrupt from scavenging while we are. */
	spin_lock_irq (&tp->lock);

	rtl8139_tx_clear (tp);

	spin_unlock_irq (&tp->lock);

	/* ...and finally, reset everything */
	rtl8139_hw_start (dev);	

	netif_wake_queue (dev);
}

static void rtl8139CP_tx_timeout (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	u8 tmp8;

	printk ("%s: %s - Transmit timeout, status %2.2x %4.4x "
		 "media %2.2x.\n", dev->name, __FUNCTION__,
		 RTL_R8 (ChipCmd),
		 RTL_R16 (IntrStatus),
		 RTL_R8 (MediaStatus));

	/* disable Tx ASAP, if not already */
	tmp8 = RTL_R8 (ChipCmd);
	if (tmp8 & CmdTxEnb)
		RTL_W8 (ChipCmd, tmp8 & ~CmdTxEnb);

	/* Disable interrupts by clearing the interrupt mask. */
	RTL_W16 (IntrMask, 0x0000);

	/* Stop a shared interrupt from scavenging while we are. */
	spin_lock_irq (&tp->lock);

	rtl8139CP_tx_clear (tp);

	spin_unlock_irq (&tp->lock);

	/* ...and finally, reset everything */
	rtl8139CP_hw_start (dev);

	netif_wake_queue (dev);
}


static int rtl8139_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	int entry;

	/* Calculate the next Tx descriptor entry. */
	entry = tp->cur_tx % NUM_TX_DESC;

	assert (tp->tx_info[entry].skb == NULL);
	assert (tp->tx_info[entry].mapping == 0);


	// ----------------------------------------------
	// For skb->len < 60, padding payload with 0x20.
	// ----------------------------------------------
	if( skb->len < ETH_ZLEN ){							//if data_len < 60
		if( (skb->data + ETH_ZLEN) <= skb->end ){				//	if data_buf is greater than 60 bytes
			//printk("%s:memset( skb->data + %d, 0x20, %d )\n", __FUNCTION__, skb->len, ETH_ZLEN - skb->len);
			memset( skb->data + skb->len, 0x20, (ETH_ZLEN - skb->len) );	//		padding data payload with 0x20
			skb->len = (skb->len >= ETH_ZLEN) ? skb->len : ETH_ZLEN;	//		data_len = 60
		}
		else{
			printk("%s:(skb->data+ETH_ZLEN) > skb->end\n",__FUNCTION__);	//	data_buf is less than or equal to 60 bytes
		}																//		do nothing
	}


	tp->tx_info[entry].skb = skb;
	if ((long) skb->data & 3) {	/* Must use alignment buffer. */
		/* tp->tx_info[entry].mapping = 0; */
		memcpy (tp->tx_buf[entry], skb->data, skb->len);
		RTL_W32 (TxAddr0 + (entry * 4),
			 tp->tx_bufs_dma + (tp->tx_buf[entry] - tp->tx_bufs));
	} else {
		tp->tx_info[entry].mapping =
		    pci_map_single (tp->pci_dev, skb->data, skb->len,
				    PCI_DMA_TODEVICE);
		RTL_W32 (TxAddr0 + (entry * 4), tp->tx_info[entry].mapping);
	}

	/* Note: the chip doesn't have auto-pad! */
	RTL_W32 (TxStatus0 + (entry * sizeof (u32)),
		 tp->tx_flag | (skb->len >= ETH_ZLEN ? skb->len : ETH_ZLEN));

	dev->trans_start = jiffies;

	spin_lock_irq (&tp->lock);

	tp->cur_tx++;
	if ((tp->cur_tx - NUM_TX_DESC) == tp->dirty_tx)
		netif_stop_queue (dev);

	spin_unlock_irq (&tp->lock);

	DPRINTK ("%s: Queued Tx packet at %p size %u to slot %d.\n",
		 dev->name, skb->data, skb->len, entry);

	return 0;
}


static int rtl8139CP_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	int CP_entry;
	struct ethernet_ii_header *eIIHeader;
	ip_v4_header   *IPPacket = NULL;
	u32 tx_status;

	mb();

	DPRINTK("%s: %s!!!\n", dev->name, __FUNCTION__ );


	CP_entry = tp->CP_cur_tx % NUM_CP_TX_DESC;			//C+ mode only

	spin_lock_irq (&tp->lock);

	tx_status = le32_to_cpu(tp->TxDescArray[CP_entry].status);
	if( (tx_status & 0x80000000)==0 ){

		tp->TxRingInfo[CP_entry].skb = skb;
		tp->TxRingInfo[CP_entry].mapping =
		    pci_map_single (tp->pci_dev, skb->data, skb->len,
				    PCI_DMA_TODEVICE);
		tp->TxDescArray[CP_entry].buf_addr=cpu_to_le64(tp->TxRingInfo[CP_entry].mapping);	//C+ mode only
		if( CP_entry != (NUM_CP_TX_DESC-1) )
		  tp->TxDescArray[CP_entry].status= cpu_to_le32(0xB0000000 |( (skb->len > ETH_ZLEN) ? skb->len : ETH_ZLEN));
	        else
		  tp->TxDescArray[CP_entry].status= cpu_to_le32(0xF0000000 |( (skb->len > ETH_ZLEN) ? skb->len : ETH_ZLEN));
	
		//////////////////////////////////////////////////////////////////////////////
		if( skb->ip_summed == CHECKSUM_HW ){
			eIIHeader = (struct ethernet_ii_header *) skb->data;
    			// Check for Ethernet_II IP encapsulation //
    			if (eIIHeader->TypeLength == IP_PROTOCOL) {
        			// We need to point the IPPacket past the Ethernet_II header. //
        			IPPacket = (ip_v4_header *) ((uint8_t *) eIIHeader + sizeof(struct ethernet_ii_header));
		
        			if (IPPacket->ProtocolCarried == TCP_PROTOCOL) {
					tp->TxDescArray[CP_entry].status |= cpu_to_le32(CPlusTxIPchecksumOffload | CPlusTxTCPchecksumOffload);
        			}
        			else if (IPPacket->ProtocolCarried == UDP_PROTOCOL) {
					tp->TxDescArray[CP_entry].status |= cpu_to_le32(CPlusTxIPchecksumOffload | CPlusTxUDPchecksumOffload);
        			}
    			}
		}//end of if( skb->ip_summed == CHECKSUM_HW )
		//////////////////////////////////////////////////////////////////////////////

		RTL_W8 (CPlusTxPoll, 0x40);		//set polling bit

		dev->trans_start = jiffies;

		tp->CP_cur_tx++;						//C+ mode only
		if ( (tp->CP_cur_tx - NUM_CP_TX_DESC) == tp->CP_dirty_tx )
			netif_stop_queue (dev);
	}//end of if( (tp->TxDescArray[CP_entry].status & 0x80000000)==0 )

	spin_unlock_irq (&tp->lock);

	mb();

//	if ( (tp->CP_cur_tx - NUM_CP_TX_DESC) == tp->CP_dirty_tx )
//		netif_stop_queue (dev);

	return 0;
}


static void rtl8139_tx_interrupt (struct net_device *dev,
				  struct rtl8139_private *tp,
				  void *ioaddr)
{
	unsigned long dirty_tx, tx_left;

	assert (dev != NULL);
	assert (tp != NULL);
	assert (ioaddr != NULL);

	dirty_tx = tp->dirty_tx;
	tx_left = tp->cur_tx - dirty_tx;
	while (tx_left > 0) {
		int entry = dirty_tx % NUM_TX_DESC;
		int txstatus;

		txstatus = RTL_R32 (TxStatus0 + (entry * sizeof (u32)));

		if (!(txstatus & (TxStatOK | TxUnderrun | TxAborted)))
			break;	/* It still hasn't been Txed */

		/* Note: TxCarrierLost is always asserted at 100mbps. */
		if (txstatus & (TxOutOfWindow | TxAborted)) {
			/* There was an major error, log it. */
			DPRINTK ("%s: Transmit error, Tx status %8.8x.\n",
				 dev->name, txstatus);
			tp->stats.tx_errors++;
			if (txstatus & TxAborted) {
				tp->stats.tx_aborted_errors++;
				RTL_W32 (TxConfig, TxClearAbt | (TX_DMA_BURST << TxDMAShift));
			}
			if (txstatus & TxCarrierLost)
				tp->stats.tx_carrier_errors++;
			if (txstatus & TxOutOfWindow)
				tp->stats.tx_window_errors++;
#ifdef ETHER_STATS
			if ((txstatus & 0x0f000000) == 0x0f000000)
				tp->stats.collisions16++;
#endif
		} else {
			if (txstatus & TxUnderrun) {
				/* Add 64 to the Tx FIFO threshold. */
				if (tp->tx_flag < 0x00300000)
					tp->tx_flag += 0x00020000;
				tp->stats.tx_fifo_errors++;
			}
			tp->stats.collisions += (txstatus >> 24) & 15;
			tp->stats.tx_bytes += txstatus & 0x7ff;
			tp->stats.tx_packets++;
		}

		/* Free the original skb. */
		if (tp->tx_info[entry].mapping != 0) {
			pci_unmap_single(tp->pci_dev,
					 tp->tx_info[entry].mapping,
					 tp->tx_info[entry].skb->len,
					 PCI_DMA_TODEVICE);
			tp->tx_info[entry].mapping = 0;
		}
		dev_kfree_skb_irq (tp->tx_info[entry].skb);
		tp->tx_info[entry].skb = NULL;

		dirty_tx++;
		tx_left--;
	}

#ifndef RTL8139_NDEBUG
	if (tp->cur_tx - dirty_tx > NUM_TX_DESC) {
		printk (KERN_ERR "%s: Out-of-sync dirty pointer, %ld vs. %ld.\n",
		        dev->name, dirty_tx, tp->cur_tx);
		dirty_tx += NUM_TX_DESC;
	}
#endif /* RTL8139_NDEBUG */

	/* only wake the queue if we did work, and the queue is stopped */
	if (tp->dirty_tx != dirty_tx) {
		tp->dirty_tx = dirty_tx;
		if (netif_queue_stopped (dev))
			netif_wake_queue (dev);
	}
}


/* TODO: clean this up!  Rx reset need not be this intensive */
static void rtl8139_rx_err (u32 rx_status, struct net_device *dev,
			    struct rtl8139_private *tp, void *ioaddr)
{
	u8 tmp8;
	int tmp_work = 1000;

	DPRINTK ("%s: Ethernet frame had errors, status %8.8x.\n",
	         dev->name, rx_status);
	if (rx_status & RxTooLong) {
		DPRINTK ("%s: Oversized Ethernet frame, status %4.4x!\n",
			 dev->name, rx_status);
		/* A.C.: The chip hangs here. */
	}
	tp->stats.rx_errors++;
	if (rx_status & (RxBadSymbol | RxBadAlign))
		tp->stats.rx_frame_errors++;
	if (rx_status & (RxRunt | RxTooLong))
		tp->stats.rx_length_errors++;
	if (rx_status & RxCRCErr)
		tp->stats.rx_crc_errors++;
	/* Reset the receiver, based on RealTek recommendation. (Bug?) */
	tp->cur_rx = 0;

	/* disable receive */
	tmp8 = RTL_R8 (ChipCmd) & ChipCmdClear;
	RTL_W8_F (ChipCmd, tmp8 | CmdTxEnb);

	/* A.C.: Reset the multicast list. */
	rtl8139_set_rx_mode (dev);

	/* XXX potentially temporary hack to
	 * restart hung receiver */
	while (--tmp_work > 0) {
		tmp8 = RTL_R8 (ChipCmd);
		if ((tmp8 & CmdRxEnb) && (tmp8 & CmdTxEnb))
			break;
		RTL_W8_F (ChipCmd,
			  (tmp8 & ChipCmdClear) | CmdRxEnb | CmdTxEnb);
	}

	/* G.S.: Re-enable receiver */
	/* XXX temporary hack to work around receiver hang */
	rtl8139_set_rx_mode (dev);

	if (tmp_work <= 0)
		printk (KERN_WARNING PFX "tx/rx enable wait too long\n");
}


/* The data sheet doesn't describe the Rx ring at all, so I'm guessing at the
   field alignments and semantics. */
static void rtl8139_rx_interrupt (struct net_device *dev,
				  struct rtl8139_private *tp, void *ioaddr)
{
	unsigned char *rx_ring;
	u16 cur_rx;

	assert (dev != NULL);
	assert (tp != NULL);
	assert (ioaddr != NULL);

	rx_ring = tp->rx_ring;
	cur_rx = tp->cur_rx;

	DPRINTK ("%s: In rtl8139_rx(), current %4.4x BufAddr %4.4x,"
		 " free to %4.4x, Cmd %2.2x.\n", dev->name, cur_rx,
		 RTL_R16 (RxBufAddr),
		 RTL_R16 (RxBufPtr), RTL_R8 (ChipCmd));

	while ((RTL_R8 (ChipCmd) & RxBufEmpty) == 0) {
		int ring_offset = cur_rx % RX_BUF_LEN;
		u32 rx_status;
		unsigned int rx_size;
		unsigned int pkt_size;
		struct sk_buff *skb;

		/* read size+status of next frame from DMA ring buffer */
		rx_status = le32_to_cpu (*(u32 *) (rx_ring + ring_offset));
		rx_size = rx_status >> 16;
		pkt_size = rx_size - 4;

		DPRINTK ("%s:  rtl8139_rx() status %4.4x, size %4.4x,"
			 " cur %4.4x.\n", dev->name, rx_status,
			 rx_size, cur_rx);
#if RTL8139_DEBUG > 2
		{
			int i;
			DPRINTK ("%s: Frame contents ", dev->name);
			for (i = 0; i < 70; i++)
				printk (" %2.2x",
					rx_ring[ring_offset + i]);
			printk (".\n");
		}
#endif

		/* E. Gill */
		/* Note from BSD driver:
		 * Here's a totally undocumented fact for you. When the
		 * RealTek chip is in the process of copying a packet into
		 * RAM for you, the length will be 0xfff0. If you spot a
		 * packet header with this value, you need to stop. The
		 * datasheet makes absolutely no mention of this and
		 * RealTek should be shot for this.
		 */
		if (rx_size == 0xfff0)
			break;

		/* If Rx err or invalid rx_size/rx_status received
		 * (which happens if we get lost in the ring),
		 * Rx process gets reset, so we abort any further
		 * Rx processing.
		 */
		if ((rx_size > (MAX_ETH_FRAME_SIZE+4)) ||
		    (!(rx_status & RxStatusOK))) {
			rtl8139_rx_err (rx_status, dev, tp, ioaddr);
			return;
		}

		/* Malloc up new buffer, compatible with net-2e. */
		/* Omit the four octet CRC from the length. */

		/* TODO: consider allocating skb's outside of
		 * interrupt context, both to speed interrupt processing,
		 * and also to reduce the chances of having to
		 * drop packets here under memory pressure.
		 */
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		if( CRC32DoubleCheck( &rx_ring[ring_offset + 4], pkt_size ) >= 0 )
		{
			skb = dev_alloc_skb (pkt_size + 2);
			if (skb) {
				skb->dev = dev;
				skb_reserve (skb, 2);	/* 16 byte align the IP fields. */

				eth_copy_and_sum (skb, &rx_ring[ring_offset + 4], pkt_size, 0);
				skb_put (skb, pkt_size);
				skb->protocol = eth_type_trans (skb, dev);
				netif_rx (skb);

				dev->last_rx = jiffies;
				tp->stats.rx_bytes += pkt_size;
				tp->stats.rx_packets++;
			} else {
				printk (KERN_WARNING
					"%s: Memory squeeze, dropping packet.\n",
					dev->name);
				tp->stats.rx_dropped++;
			}
		}
		else{
			atomic_inc( &tp->CRC32DoubleCheck_ERR );
			printk("%s: CRC double check failed.\n", dev->name);
		}
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

		cur_rx = (cur_rx + rx_size + 4 + 3) & ~3;
		RTL_W16_F (RxBufPtr, cur_rx - 16);
	}

	DPRINTK ("%s: Done rtl8139_rx(), current %4.4x BufAddr %4.4x,"
		 " free to %4.4x, Cmd %2.2x.\n", dev->name, cur_rx,
		 RTL_R16 (RxBufAddr),
		 RTL_R16 (RxBufPtr), RTL_R8 (ChipCmd));

	tp->cur_rx = cur_rx;
}


static void rtl8139_weird_interrupt (struct net_device *dev,
				     struct rtl8139_private *tp,
				     void *ioaddr,
				     int status, int link_changed)
{
	printk (KERN_DEBUG "%s: Abnormal interrupt, status %8.8x.\n",
		dev->name, status);

	assert (dev != NULL);
	assert (tp != NULL);
	assert (ioaddr != NULL);

	/* Update the error count. */
	tp->stats.rx_missed_errors += RTL_R32 (RxMissed);
	RTL_W32 (RxMissed, 0);

	if ((status & RxUnderrun) && link_changed &&
	    (tp->drv_flags & HAS_LNK_CHNG)) {
		/* Really link-change on new chips. */
		int lpar = RTL_R16 (NWayLPAR);
		int duplex = (lpar & 0x0100) || (lpar & 0x01C0) == 0x0040
				|| tp->duplex_lock;
		if (tp->full_duplex != duplex) {
			tp->full_duplex = duplex;
			RTL_W8 (Cfg9346, Cfg9346_Unlock);
			RTL_W8 (Config1, tp->full_duplex ? 0x60 : 0x20);
			RTL_W8 (Cfg9346, Cfg9346_Lock);
		}
		status &= ~RxUnderrun;
	}

	/* XXX along with rtl8139_rx_err, are we double-counting errors? */
	if (status &
	    (RxUnderrun | RxOverflow | RxErr | RxFIFOOver))
		tp->stats.rx_errors++;

	if (status & (PCSTimeout))
		tp->stats.rx_length_errors++;
	if (status & (RxUnderrun | RxFIFOOver))
		tp->stats.rx_fifo_errors++;
	if (status & RxOverflow) {
		tp->stats.rx_over_errors++;
		tp->cur_rx = RTL_R16 (RxBufAddr) % RX_BUF_LEN;
		RTL_W16_F (RxBufPtr, tp->cur_rx - 16);
	}
	if (status & PCIErr) {
		u16 pci_cmd_status;
		pci_read_config_word (tp->pci_dev, PCI_STATUS, &pci_cmd_status);

		printk (KERN_ERR "%s: PCI Bus error %4.4x.\n",
			dev->name, pci_cmd_status);
	}
}


/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void rtl8139_interrupt (int irq, void *dev_instance,
			       struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_instance;
	struct rtl8139_private *tp = dev->priv;
	int boguscnt = max_interrupt_work;
	void *ioaddr = tp->mmio_addr;
	int status = 0, link_changed = 0; /* avoid bogus "uninit" warning */

	do {
		status = RTL_R16 (IntrStatus);

		/* h/w no longer present (hotplug?) or major error, bail */
		if (status == 0xFFFF)
			break;

		/* Acknowledge all of the current interrupt sources ASAP, but
		   an first get an additional status bit from CSCR. */
		if (status & RxUnderrun){
			link_changed = RTL_R16 (CSCR) & CSCR_LinkChangeBit;
			printk("TEST Link change!\n");
		}

		/* E. Gill */
		/* In case of an RxFIFOOver we must also clear the RxOverflow
		   bit to avoid dropping frames for ever. Believe me, I got a
		   lot of troubles copying huge data (approximately 2 RxFIFOOver
		   errors per 1GB data transfer).
		   The following is written in the 'p-guide.pdf' file (RTL8139(A/B)
		   Programming guide V0.1, from 1999/1/15) on page 9 from REALTEC.
		   -----------------------------------------------------------
		   2. RxFIFOOvw handling:
		     When RxFIFOOvw occurs, all incoming packets are discarded.
		     Clear ISR(RxFIFOOvw) doesn't dismiss RxFIFOOvw event. To
		     dismiss RxFIFOOvw event, the ISR(RxBufOvw) must be written
		     with a '1'.
		   -----------------------------------------------------------
		   Unfortunately I was not able to find any reason for the
		   RxFIFOOver error (I got the feeling this depends on the
		   CPU speed, lower CPU speed --> more errors).
		   After clearing the RxOverflow bit the transfer of the
		   packet was repeated and all data are error free transferred */
		RTL_W16_F (IntrStatus, (status & RxFIFOOver) ? (status | RxOverflow) : status);

		DPRINTK ("%s: interrupt  status=%#4.4x new intstat=%#4.4x.\n",
				dev->name, status,
				RTL_R16 (IntrStatus));

		if ((status &
		     (PCIErr | PCSTimeout | RxUnderrun | RxOverflow |
		      RxFIFOOver | TxErr | TxOK | RxErr | RxOK)) == 0)
			break;

		/* Check uncommon events with one test. */
		if (status & (PCIErr | PCSTimeout | RxUnderrun | RxOverflow |
		  	      RxFIFOOver | TxErr | RxErr))
			rtl8139_weird_interrupt (dev, tp, ioaddr,
						 status, link_changed);

		if (status & (RxOK | RxUnderrun | RxOverflow | RxFIFOOver))	/* Rx interrupt */
			rtl8139_rx_interrupt (dev, tp, ioaddr);

		if (status & (TxOK | TxErr)) {
			spin_lock (&tp->lock);
			rtl8139_tx_interrupt (dev, tp, ioaddr);
			spin_unlock (&tp->lock);
		}

		boguscnt--;
	} while (boguscnt > 0);

	if (boguscnt <= 0) {
		printk (KERN_WARNING
			"%s: Too much work at interrupt, "
			"IntrStatus=0x%4.4x.\n", dev->name,
			status);

		/* Clear all interrupt sources. */
		RTL_W16 (IntrStatus, 0xffff);
	}

	DPRINTK ("%s: exiting interrupt, intr_status=%#4.4x.\n",
		 dev->name, RTL_R16 (IntrStatus));
}


static void rtl8139CP_tx_interrupt (struct net_device *dev,
				  struct rtl8139_private *tp,
				  void *ioaddr)
{
	unsigned long CP_dirty_tx, CP_tx_left;			


	assert (dev != NULL);
	assert (tp != NULL);
	assert (ioaddr != NULL);

#ifdef CONFIG_LEDMAN
	ledman_cmd(LEDMAN_CMD_SET, (dev->name[3] == '0') ?  LEDMAN_LAN1_TX :
			((dev->name[3] == '1') ?  LEDMAN_LAN2_TX : LEDMAN_LAN3_TX));
#endif

	CP_dirty_tx = tp->CP_dirty_tx;
	CP_tx_left = tp->CP_cur_tx - CP_dirty_tx;
	while (CP_tx_left > 0) {					
		unsigned long entry = CP_dirty_tx % NUM_CP_TX_DESC;
	
		if (tp->TxRingInfo[entry].mapping != 0) {
			pci_unmap_single(tp->pci_dev,
					 tp->TxRingInfo[entry].mapping,
					 tp->TxRingInfo[entry].skb->len,
					 PCI_DMA_TODEVICE);
			tp->TxRingInfo[entry].mapping = 0;
		}
		dev_kfree_skb_irq (tp->TxRingInfo[entry].skb);
		tp->TxRingInfo[entry].skb = NULL;
		tp->stats.tx_packets++;			
		CP_dirty_tx++;				
		CP_tx_left--;				
	}						

	if (tp->CP_dirty_tx != CP_dirty_tx) {			
		tp->CP_dirty_tx = CP_dirty_tx;			
		if (netif_queue_stopped (dev))			
			netif_wake_queue (dev);			
	}							
}


/* The data sheet doesn't describe the Rx ring at all, so I'm guessing at the
   field alignments and semantics. */
static void rtl8139CP_rx_interrupt (struct net_device *dev,
				  struct rtl8139_private *tp, void *ioaddr)
{
	int CP_cur_rx;						
	struct sk_buff *skb; 					
	int pkt_size=0 ;					
	int boguscnt = max_interrupt_work;
	u32 rx_status;


	assert (dev != NULL);
	assert (tp != NULL);
	assert (ioaddr != NULL);

#ifdef CONFIG_LEDMAN
	ledman_cmd(LEDMAN_CMD_SET, (dev->name[3] == '0') ?  LEDMAN_LAN1_RX :
			((dev->name[3] == '1') ?  LEDMAN_LAN2_RX : LEDMAN_LAN3_RX));
#endif

	CP_cur_rx = tp->CP_cur_rx;					

	while (boguscnt--) {
		rx_status = le32_to_cpu(tp->RxDescArray[CP_cur_rx].status);
		if (rx_status & 0x80000000) {
			if (boguscnt == max_interrupt_work-1)
				printk("no rx\n");
			break;
		}
		
	    if (rx_status & CPlusRxRES){
		printk(KERN_INFO"RTL8139: Rx ERROR!!!\n");
		tp->stats.rx_errors++;
		if (rx_status & CPlusRxFAE)
			tp->stats.rx_frame_errors++;
		if (rx_status & (CPlusRxRWT|CPlusRxRUNT) )
			tp->stats.rx_length_errors++;
		if (rx_status & CPlusRxCRC)
			tp->stats.rx_crc_errors++;
	    }
	    else{
	   	pkt_size=(int)(rx_status & 0x00001FFF)-4;
		skb = dev_alloc_skb(pkt_size + 2);
		if (skb != NULL) {
		  	skb->dev = dev;
			skb_reserve (skb, 2);	// 16 byte align the IP fields. //
			eth_copy_and_sum(skb, tp->RxBufferRing[CP_cur_rx], pkt_size, 0);

			////////For Checksum Offload///////////////////////////////////////
			switch( (rx_status>>16)&0x3 ){
				case ProtocolType_IP:
							//if the protocol type is IP and IP checksum is correct//
							if( (rx_status & CPlusRxIPchecksumBIT) == 0 )
									skb->ip_summed=CHECKSUM_UNNECESSARY;
							break;
				case ProtocolType_TCPIP:
							//if the protocol type is TCP/IP and TCP/IP checksum is correct//
							if( (rx_status & CPlusRxTCPIPchecksumBIT) == 0 )
									skb->ip_summed=CHECKSUM_UNNECESSARY;
							break;
				case ProtocolType_UDPIP:
							//if the protocol type is UDP/IP and UDP/IP checksum is correct//
							if( (rx_status & CPlusRxUDPIPchecksumBIT) == 0 )
									skb->ip_summed=CHECKSUM_UNNECESSARY;
							break;
				default:
							break;
			}

			///////////////////////////////////////////////////////////////////

			skb_put(skb, pkt_size);
			skb->protocol = eth_type_trans(skb, dev);

			netif_rx (skb);

			dev->last_rx = jiffies;
	    		tp->stats.rx_bytes += pkt_size;
	    		tp->stats.rx_packets++;
		}
		else{
			printk(KERN_WARNING"%s: Memory squeeze, deferring packet.\n",dev->name);
			/* We should check that some rx space is free.
		   	If not, free one and mark stats->rx_dropped++. */
			tp->stats.rx_dropped++;
              	}
	    }

	    if( CP_cur_rx== (NUM_CP_RX_DESC-1) )
		tp->RxDescArray[CP_cur_rx].status  = cpu_to_le32(0xC0000000 + CP_RX_BUF_SIZE);
	    else
		tp->RxDescArray[CP_cur_rx].status  = cpu_to_le32(0x80000000 + CP_RX_BUF_SIZE);

	    tp->RxDescArray[CP_cur_rx].buf_addr  = cpu_to_le64(tp->RxBufferRing[CP_cur_rx] - tp->RxBufferRings + tp->RxBufferRingsDMA);

	    CP_cur_rx = (CP_cur_rx +1) % NUM_CP_RX_DESC;
	}// end of while ( (tp->RxDescArray[CP_cur_rx].status & 0x80000000)== 0)

	tp->CP_cur_rx = CP_cur_rx;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void rtl8139CP_interrupt (int irq, void *dev_instance,
			       struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_instance;
	struct rtl8139_private *tp = dev->priv;
	int boguscnt = max_interrupt_work;
	void *ioaddr = tp->mmio_addr;
	int status = 0, link_changed = 0; /* avoid bogus "uninit" warning */

	do {
		status = RTL_R16 (IntrStatus);

		/* h/w no longer present (hotplug?) or major error, bail */
		if (status == 0xFFFF)
			break;

		/* Acknowledge all of the current interrupt sources ASAP, but
		   an first get an additional status bit from CSCR. */
		if (status & RxUnderrun){
			printk("%s: %s RxUnderrun/Linkchange!\n", dev->name, __FUNCTION__);
			rtl8139CP_init_ring (dev);
			rtl8139CP_hw_start (dev);
			link_changed = RTL_R16 (CSCR) & CSCR_LinkChangeBit;
		}

		/* E. Gill */
		/* In case of an RxFIFOOver we must also clear the RxOverflow
		   bit to avoid dropping frames for ever. Believe me, I got a
		   lot of troubles copying huge data (approximately 2 RxFIFOOver
		   errors per 1GB data transfer).
		   The following is written in the 'p-guide.pdf' file (RTL8139(A/B)
		   Programming guide V0.1, from 1999/1/15) on page 9 from REALTEC.
		   -----------------------------------------------------------
		   2. RxFIFOOvw handling:
		     When RxFIFOOvw occurs, all incoming packets are discarded.
		     Clear ISR(RxFIFOOvw) doesn't dismiss RxFIFOOvw event. To
		     dismiss RxFIFOOvw event, the ISR(RxBufOvw) must be written
		     with a '1'.
		   -----------------------------------------------------------
		   Unfortunately I was not able to find any reason for the
		   RxFIFOOver error (I got the feeling this depends on the
		   CPU speed, lower CPU speed --> more errors).
		   After clearing the RxOverflow bit the transfer of the
		   packet was repeated and all data are error free transferred */
		RTL_W16_F (IntrStatus, (status & RxFIFOOver) ? (status | RxOverflow) : status);

		DPRINTK ("%s: interrupt  status=%#4.4x new intstat=%#4.4x.\n",
				dev->name, status,
				RTL_R16 (IntrStatus));

		if ((status & (PCIErr | PCSTimeout | RxUnderrun | RxOverflow |
		      RxFIFOOver | TxErr | TxOK | RxErr | RxOK)) == 0)
			break;

		if (status & (RxOK | RxUnderrun | RxOverflow | RxFIFOOver))	/* Rx interrupt */
			rtl8139CP_rx_interrupt (dev, tp, ioaddr);

		if (status & (TxOK | TxErr)) {
			spin_lock (&tp->lock);
			rtl8139CP_tx_interrupt (dev, tp, ioaddr);
			spin_unlock (&tp->lock);
		}

		boguscnt--;
	} while (boguscnt > 0);

	if (boguscnt <= 0) {
		printk (KERN_WARNING "%s: Too much work at interrupt, "	"IntrStatus=0x%4.4x.\n", dev->name,	status);

		/* Clear all interrupt sources. */
		RTL_W16 (IntrStatus, 0xffff);
	}

	DPRINTK ("%s: exiting interrupt, intr_status=%#4.4x.\n", dev->name, RTL_R16 (IntrStatus));
}


static int rtl8139_close (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	int ret = 0;

	DPRINTK ("ENTER\n");

	netif_stop_queue (dev);

	if (tp->thr_pid >= 0) {
		ret = kill_proc (tp->thr_pid, SIGTERM, 1);
		if (ret) {
			printk (KERN_ERR "%s: unable to signal thread\n", dev->name);
			return ret;
		}
#if LINUX_VERSION_CODE < 0x20407
		down (&tp->thr_exited);
#else
		wait_for_completion (&tp->thr_exited);
#endif
	}

	DPRINTK ("%s: Shutting down ethercard, status was 0x%4.4x.\n", dev->name, RTL_R16 (IntrStatus));

	spin_lock_irq (&tp->lock);

	/* Stop the chip's Tx and Rx DMA processes. */
	RTL_W8 (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear));

	/* Disable interrupts by clearing the interrupt mask. */
	RTL_W16 (IntrMask, 0x0000);

	/* Update the error counts. */
	tp->stats.rx_missed_errors += RTL_R32 (RxMissed);
	RTL_W32 (RxMissed, 0);

	spin_unlock_irq (&tp->lock);

	synchronize_irq ();
	free_irq (dev->irq, dev);

	rtl8139_tx_clear (tp);
	pci_free_consistent(tp->pci_dev, RX_BUF_TOT_LEN,tp->rx_ring, tp->rx_ring_dma);
	pci_free_consistent(tp->pci_dev, TX_BUF_TOT_LEN,tp->tx_bufs, tp->tx_bufs_dma);
	tp->rx_ring = NULL;
	tp->tx_bufs = NULL;
	
	/* Green! Put the chip in low-power mode. */
	RTL_W8 (Cfg9346, Cfg9346_Unlock);
	RTL_W8 (Config1, 0x03);
	RTL_W8 (HltClk, 'H');	/* 'R' would leave the clock running. */

	DPRINTK ("EXIT\n");
	return 0;
}

static int rtl8139CP_close (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	int ret = 0;
	int i;

	DPRINTK ("ENTER\n");

	netif_stop_queue (dev);

	if (tp->thr_pid >= 0) {
		ret = kill_proc (tp->thr_pid, SIGTERM, 1);
		if (ret) {
			printk (KERN_ERR "%s: unable to signal thread\n", dev->name);
			return ret;
		}

#if LINUX_VERSION_CODE < 0x20407
		down (&tp->thr_exited);
#else
		wait_for_completion (&tp->thr_exited);
#endif
	}

	DPRINTK ("%s: Shutting down ethercard, status was 0x%4.4x.\n", dev->name, RTL_R16 (IntrStatus));

	spin_lock_irq (&tp->lock);

	/* Stop the chip's Tx and Rx DMA processes. */
	RTL_W8 (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear));

	/* Disable interrupts by clearing the interrupt mask. */
	RTL_W16 (IntrMask, 0x0000);

	/* Update the error counts. */
	tp->stats.rx_missed_errors += RTL_R32 (RxMissed);
	RTL_W32 (RxMissed, 0);

	spin_unlock_irq (&tp->lock);

	synchronize_irq ();
	free_irq (dev->irq, dev);

	rtl8139CP_tx_clear (tp);
	if (tp->TxDescArrays)
		pci_free_consistent(tp->pci_dev, NUM_CP_TX_DESC*sizeof(struct CPlusTxDesc)+256,
				   tp->TxDescArrays, tp->TxDescArraysDMA);
	if (tp->RxDescArrays)
		pci_free_consistent(tp->pci_dev, NUM_CP_RX_DESC*sizeof(struct CPlusRxDesc)+256,
				   tp->RxDescArrays, tp->RxDescArraysDMA);
	tp->TxDescArrays = NULL;
	tp->RxDescArrays = NULL;
	tp->TxDescArray = NULL;
	tp->RxDescArray = NULL;
	for (i=0; i <NUM_CP_RX_DESC; i++)tp->RxBufferRing[i]=NULL;
	pci_free_consistent(tp->pci_dev, CP_RX_BUF_SIZE*NUM_CP_RX_DESC,
			tp->RxBufferRings, tp->RxBufferRingsDMA);

	/* Green! Put the chip in low-power mode. */
	RTL_W8 (Cfg9346, Cfg9346_Unlock);
	RTL_W8 (Config1, 0x03);
	RTL_W8 (HltClk, 'H');	/* 'R' would leave the clock running. */

	DPRINTK ("EXIT\n");
	return 0;
}








static int mii_ioctl (struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct rtl8139_private *tp = dev->priv;
	u16 *data = (u16 *) & rq->ifr_data;
	int rc = 0;

	DPRINTK ("ENTER\n");

	switch (cmd) {
	case SIOCDEVPRIVATE:	/* Get the address of the PHY in use. */
		data[0] = tp->phys[0] & 0x3f;
		/* Fall Through */

	case SIOCDEVPRIVATE + 1:	/* Read the specified MII register. */
		data[3] = mdio_read (dev, data[0], data[1] & 0x1f);
		break;

	case SIOCDEVPRIVATE + 2:	/* Write the specified MII register */
		if (!capable (CAP_NET_ADMIN)) {
			rc = -EPERM;
			break;
		}

		if (data[0] == tp->phys[0]) {
			u16 value = data[2];
			switch (data[1]) {
			case 0:
				/* Check for autonegotiation on or reset. */
				tp->medialock = (value & 0x9000) ? 0 : 1;
				if (tp->medialock)
					tp->full_duplex = (value & 0x0100) ? 1 : 0;
				break;
			case 4: tp->advertising = value; break;
			}
		}
		mdio_write(dev, data[0], data[1] & 0x1f, data[2]);
		break;

	default:
		rc = -EOPNOTSUPP;
		break;
	}

	DPRINTK ("EXIT, returning %d\n", rc);
	return rc;
}


static struct net_device_stats *rtl8139_get_stats (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;

	DPRINTK ("ENTER\n");

	if (netif_running(dev)) {
		spin_lock_irq (&tp->lock);
		tp->stats.rx_missed_errors += RTL_R32 (RxMissed);
		RTL_W32 (RxMissed, 0);
		spin_unlock_irq (&tp->lock);
	}

	DPRINTK ("EXIT\n");
	return &tp->stats;
}

/* Set or clear the multicast filter for this adaptor.
   This routine is not state sensitive and need not be SMP locked. */

static unsigned const ethernet_polynomial = 0x04c11db7U;
static inline u32 ether_crc (int length, unsigned char *data)
{
	int crc = -1;

	DPRINTK ("ENTER\n");

	while (--length >= 0) {
		unsigned char current_octet = *data++;
		int bit;
		for (bit = 0; bit < 8; bit++, current_octet >>= 1)
			crc = (crc << 1) ^ ((crc < 0) ^ (current_octet & 1) ?
			     ethernet_polynomial : 0);
	}

	DPRINTK ("EXIT, returning %u\n", crc);
	return crc;
}


static void rtl8139_set_rx_mode (struct net_device *dev)
{
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	unsigned long flags;
	u32 mc_filter[2];	/* Multicast hash filter */
	int i, rx_mode;
	u32 tmp;

	DPRINTK ("ENTER\n");

	DPRINTK ("%s:   rtl8139_set_rx_mode(%4.4x) done -- Rx config %8.8lx.\n",
			dev->name, dev->flags, RTL_R32 (RxConfig));

	/* Note: do not reorder, GCC is clever about common statements. */
	if (dev->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		printk (KERN_NOTICE "%s: Promiscuous mode enabled.\n",
			dev->name);
		rx_mode =
		    AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
		    AcceptAllPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else if ((dev->mc_count > multicast_filter_limit)
		   || (dev->flags & IFF_ALLMULTI)) {
		/* Too many to filter perfectly -- accept all multicasts. */
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else {
		struct dev_mc_list *mclist;
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0;
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
		     i++, mclist = mclist->next)
			set_bit (ether_crc (ETH_ALEN, mclist->dmi_addr) >> 26,
				 mc_filter);
	}

	spin_lock_irqsave (&tp->lock, flags);

	/* We can safely update without stopping the chip. */
	tmp = rtl8139_rx_config | rx_mode |
		(RTL_R32 (RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);
	RTL_W32_F (RxConfig, tmp);
	RTL_W32_F (MAR0 + 0, mc_filter[0]);
	RTL_W32_F (MAR0 + 4, mc_filter[1]);

	spin_unlock_irqrestore (&tp->lock, flags);

	DPRINTK ("EXIT\n");
}

#ifdef CONFIG_PM

#if LINUX_VERSION_CODE < 0x20407
static void rtl8139_suspend (struct pci_dev *pdev)
#else
static int rtl8139_suspend (struct pci_dev *pdev, u32 state)
#endif
{
	struct net_device *dev = pdev->driver_data;
	struct rtl8139_private *tp = dev->priv;
	void *ioaddr = tp->mmio_addr;
	unsigned long flags;

	netif_device_detach (dev);

	spin_lock_irqsave (&tp->lock, flags);

	/* Disable interrupts, stop Tx and Rx. */
	RTL_W16 (IntrMask, 0x0000);
	RTL_W8 (ChipCmd, (RTL_R8 (ChipCmd) & ChipCmdClear));

	/* Update the error counts. */
	tp->stats.rx_missed_errors += RTL_R32 (RxMissed);
	RTL_W32 (RxMissed, 0);

	spin_unlock_irqrestore (&tp->lock, flags);

#if LINUX_VERSION_CODE < 0x20407
#else
	return 0;
#endif
}


#if LINUX_VERSION_CODE < 0x20407
static void rtl8139_resume (struct pci_dev *pdev)
#else
static int rtl8139_resume (struct pci_dev *pdev)
#endif
{
	struct net_device *dev = pdev->driver_data;
	struct rtl8139_private *tp = dev->priv;

	netif_device_attach (dev);
	if( rtl_chip_info[tp->chipset].name == "RTL-8139CP" )
		rtl8139CP_hw_start (dev);
	else
		rtl8139_hw_start (dev);

#if LINUX_VERSION_CODE < 0x20407
#else
	return 0;
#endif
}

#endif	//end of #ifdef CONFIG_PM

static struct pci_driver rtl8139_pci_driver = {
	name:		MODNAME,
	id_table:	rtl8139_pci_tbl,
	probe:		rtl8139_init_one,
	remove:		__devexit_p(rtl8139_remove_one),
#ifdef CONFIG_PM
	suspend:	rtl8139_suspend,
	resume:		rtl8139_resume,
#endif	//end of #ifdef CONFIG_PM	
};


static int __init rtl8139_init_module (void)
{
	return pci_module_init (&rtl8139_pci_driver);
}


static void __exit rtl8139_cleanup_module (void)
{
	pci_unregister_driver (&rtl8139_pci_driver);
}


module_init(rtl8139_init_module);
module_exit(rtl8139_cleanup_module);
