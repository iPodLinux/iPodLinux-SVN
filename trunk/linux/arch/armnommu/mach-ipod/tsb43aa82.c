/*
 * tsb43aa82.c - ieee1394 driver for iPod
 *
 * Copyright (c) 2003,2004 Bernard Leach <leachbj@bouncycastle.org>
 *
 * IEEE1394 controller is the TSB43AA82 (iSphyx II) from Texas Instruments.
 *
 * Derived from ohci1394.c the driver for OHCI 1394 boards
 * Copyright (C)1999,2000 Sebastien Rougeaux <sebastien.rougeaux@anu.edu.au>
 *                        Gord Peters <GordPeters@smarttech.com>
 *              2001      Ben Collins <bcollins@debian.org>
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>

#include "../../../drivers/ieee1394/ieee1394.h"
#include "../../../drivers/ieee1394/ieee1394_types.h"
#include "../../../drivers/ieee1394/hosts.h"
#include "../../../drivers/ieee1394/ieee1394_core.h"

#include "tsb43aa82.h"

/***************************************************************
 * FIFO size definition
 * Async Command FIFOs = 378 quads - MTQ(3) - MRF(15) = 360 quads
 * ATF+ARF+CTQ+CRF <= 360 quads
 ***************************************************************/
#if 0
#define COMMAND_TX_QUE_SIZE   16
#define COMMAND_RX_FIFO_SIZE  64
#define ASYNC_TX_FIFO_SIZE    80
#endif
#define COMMAND_TX_QUE_SIZE   0
#define COMMAND_RX_FIFO_SIZE  0
#define ASYNC_TX_FIFO_SIZE    180
#define ASYNC_RX_FIFO_SIZE    (360 - COMMAND_TX_QUE_SIZE - \
                              COMMAND_RX_FIFO_SIZE - ASYNC_TX_FIFO_SIZE)
/* DxF Size Register 0x98 */
/* DTF+DRF <= (1182 quads / 4) units (ie 295) */
#define DMA_DXF_TOTAL_QUADS   (1180ul / 4) /*In units of 4 quads */
#define DMA_DXF_MAX_SIZE      (1052ul / 4)
#define DMA_DXF_MIN_SIZE      (DMA_DXF_TOTAL_QUADS - DMA_DXF_MAX_SIZE)
#define DMA_READ_FIFO_SIZE    (DMA_DXF_TOTAL_QUADS / 2)
#define DMA_WRITE_FIFO_SIZE   (DMA_DXF_TOTAL_QUADS - DMA_READ_FIFO_SIZE)

#define DXF_PG_ENTRY   4      /* # of Page Table Elements to retrieve */

static struct hpsb_host *ipod_host;

struct ti_ipod {
	/* buffer for csr config rom */
	quadlet_t *csr_config_rom_cpu;
	dma_addr_t csr_config_rom_bus;
	int csr_config_rom_length;

	/* protect access to the PHY register */
	spinlock_t phy_reg_lock;

	struct tasklet_struct tx_tasklet;

	struct list_head tx_list;
	spinlock_t tx_list_lock;

	spinlock_t tx_lock;
#define TX_READY	0
#define TX_BUSY		1
#define TX_END		2
	int tx_state;
#define RX_READY	0
#define RX_BUSY		1
	int rx_state;

	struct hpsb_packet *packet;

	/* IEEE-1394 part follows */
	struct hpsb_host *host;
};

static u16 ohci_crc16(u32 *ptr, int length)
{
	int shift;
	u32 crc, sum, data;

	crc = 0;
	for (; length > 0; length--) {
		data = be32_to_cpu(*ptr++);
		for (shift = 28; shift >= 0; shift -= 4) {
			sum = ((crc >> 12) ^ (data >> shift)) & 0x000f;
			crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ sum;
		}
		crc &= 0xffff;
	}
	return crc;
}

/* Config ROM macro implementation influenced by NetBSD OHCI driver */
/* taken from the OHCI driver */

struct config_rom_unit {
	u32 *start;
	u32 *refer;
	int length;
	int refunit;
};

struct config_rom_ptr {
	u32 *data;
	int unitnum;
	struct config_rom_unit unitdir[10];
};

#define cf_put_1quad(cr, q) (((cr)->data++)[0] = cpu_to_be32(q))

#define cf_put_4bytes(cr, b1, b2, b3, b4) \
	(((cr)->data++)[0] = cpu_to_be32(((b1) << 24) | ((b2) << 16) | ((b3) << 8) | (b4)))

#define cf_put_keyval(cr, key, val) (((cr)->data++)[0] = cpu_to_be32(((key) << 24) | (val)))

static inline void cf_put_str(struct config_rom_ptr *cr, const char *str)
{
	int t;
	char fourb[4];

	while (str[0]) {
		memset(fourb, 0, 4);
		for (t = 0; t < 4 && str[t]; t++) {
			fourb[t] = str[t];
		}
		cf_put_4bytes(cr, fourb[0], fourb[1], fourb[2], fourb[3]);
		str += strlen(str) < 4 ? strlen(str) : 4;
	}
	return;
}

static inline void cf_put_crc16(struct config_rom_ptr *cr, int unit)
{
	*cr->unitdir[unit].start =
		cpu_to_be32((cr->unitdir[unit].length << 16) |
			    ohci_crc16(cr->unitdir[unit].start + 1,
				       cr->unitdir[unit].length));
}

static inline void cf_unit_begin(struct config_rom_ptr *cr, int unit)
{
	if (cr->unitdir[unit].refer != NULL) {
		*cr->unitdir[unit].refer |=
			cpu_to_be32(cr->data - cr->unitdir[unit].refer);
		cf_put_crc16(cr, cr->unitdir[unit].refunit);
	}
	cr->unitnum = unit;
	cr->unitdir[unit].start = cr->data++;
}

static inline void cf_put_refer(struct config_rom_ptr *cr, char key, int unit)
{
	cr->unitdir[unit].refer = cr->data;
	cr->unitdir[unit].refunit = cr->unitnum;
	(cr->data++)[0] = cpu_to_be32(key << 24);
}

static inline void cf_unit_end(struct config_rom_ptr *cr)
{
	cr->unitdir[cr->unitnum].length = cr->data -
		(cr->unitdir[cr->unitnum].start + 1);
	cf_put_crc16(cr, cr->unitnum);
}

/* End of NetBSD derived code.  */

/* read 32 bit value from firewire chip */
static unsigned fw_reg_read(int addr)
{
	/* read from little endian controller */
	return (inw(0x30000000 + (addr * 2) + 0) & 0xff)
		| ((inw(0x30000000 + (addr * 2) + 2) & 0xff) << 8)
		| ((inw(0x30000000 + (addr * 2) + 4) & 0xff) << 16)
		| ((inw(0x30000000 + (addr * 2) + 6) & 0xff) << 24);
}

/* write 32 bit value to firewire chip */
static void fw_reg_write(int addr, unsigned val)
{
	/* write to little endian controller */
	outw(val, 0x30000000 + (addr * 2) + 0);
	outw(val >> 8, 0x30000000 + (addr * 2) + 2);
	outw(val >> 16, 0x30000000 + (addr * 2) + 4);
	outw(val >> 24, 0x30000000 + (addr * 2) + 6);
}

#define IPOD_LOOP_COUNT	100	/* Number of loops for reg read waits */

static u8 get_phy_reg(struct ti_ipod *ipod, u8 addr)
{
	int i;
	unsigned long flags;
	quadlet_t r;

	spin_lock_irqsave(&ipod->phy_reg_lock, flags);

	fw_reg_write(LYNX_PHYACCESS, addr << 24 | 0x80000000);

	for (i = 0; i < IPOD_LOOP_COUNT; i++) {
		if ( !(fw_reg_read(LYNX_PHYACCESS) & 0x80000000) )
			break;

		mdelay(1);
	}

	r = fw_reg_read(LYNX_PHYACCESS);

	if ( i == IPOD_LOOP_COUNT ) {
		printk(KERN_ERR "Get PHY Reg timeout [0x%08x]\n", r);
	}

	spin_unlock_irqrestore(&ipod->phy_reg_lock, flags);

	return r & 0xff;
}

static void set_phy_reg(struct ti_ipod *ipod, u8 addr, u8 data)
{
	int i;
	unsigned long flags;
	u32 r = 0;

	spin_lock_irqsave(&ipod->phy_reg_lock, flags);

	fw_reg_write(LYNX_PHYACCESS, (addr << 24) | (data << 16) | 0x40000000);

	for (i = 0; i < IPOD_LOOP_COUNT; i++) {
		r = fw_reg_read(LYNX_PHYACCESS);
		if (!(r & 0x40000000))
			break;

		mdelay(1);
	}

	if (i == IPOD_LOOP_COUNT) {
		printk(KERN_ERR "Set PHY Reg timeout [0x%08x]\n", r);
	}

	spin_unlock_irqrestore(&ipod->phy_reg_lock, flags);

	return;
}

static void set_phy_reg_mask(struct ti_ipod *ipod, u8 addr, u8 data)
{
	u8 old;

	old = get_phy_reg(ipod, addr);
	old |= data;
	set_phy_reg(ipod, addr, old);

	return;
}

/* this is the maximum length */
#define CONFIG_ROM_LEN	1024

static void init_config_rom(struct ti_ipod *ipod)
{
	struct config_rom_ptr cr;
	int i;

	memset(&cr, 0, sizeof(cr));
	memset(ipod->csr_config_rom_cpu, 0, sizeof (ipod->csr_config_rom_cpu));

	cr.data = ipod->csr_config_rom_cpu;

	/* Bus info block */
	cf_unit_begin(&cr, 0);
	cf_put_1quad(&cr, 0x31333934);	/* Bus ID == "1394" */
	cf_put_1quad(&cr, 0x40ff6002);	/* Bus options (based iPod f/w) */
					/* 6 -> 2^7 == max_rec */
	cf_put_1quad(&cr, 0x00000000);	/* vendor id, chip id high */
	cf_put_1quad(&cr, 0x024f3638);	/* chip id low */
	cf_unit_end(&cr);

	/* IEEE P1212 suggests the initial ROM header CRC should only
	 * cover the header itself (and not the entire ROM). Since we do
	 * this, then we can make our bus_info_len the same as the CRC
	 * length.  */
	ipod->csr_config_rom_cpu[0] |= cpu_to_be32(
		(be32_to_cpu(ipod->csr_config_rom_cpu[0]) & 0x00ff0000) << 8);

	/* Root directory */
	cf_unit_begin(&cr, 1);
	/* Vendor ID */
	cf_put_keyval(&cr, 0x03, 0x0);		/* Vendor ID == 0x0 */
	cf_put_refer(&cr, 0x81, 2);		/* Textual description unit */
	cf_put_keyval(&cr, 0x0c, 0x0083c0);	/* Node capabilities */
	/* NOTE: Add other unit referers here, and append at bottom */
	cf_unit_end(&cr);

	/* Textual description - "Linux iPod-1394" */
	cf_unit_begin(&cr, 2);
	cf_put_keyval(&cr, 0, 0);
	cf_put_1quad(&cr, 0);
	cf_put_str(&cr, "Linux iPod-1394");
	cf_unit_end(&cr);

	ipod->csr_config_rom_length = cr.data - ipod->csr_config_rom_cpu;

	/*
         * set AR_CSR_Size and CSR_Size (multiplies by 4 to get bytes)
	 */
	fw_reg_write(LYNX_CFR_CTRL, ipod->csr_config_rom_length << 18 | ipod->csr_config_rom_length << 2);

	/*
	 * Setup write to CSR
	 */
	fw_reg_write(LYNX_LOG_ROM_CTRL, LYNX_ROM_CTRL_XLOG|LYNX_ROM_CTRL_ROM_ADDR);

	for (i = 0; i < ipod->csr_config_rom_length; i++) {
		/* log data register, write the ROM data */
		fw_reg_write(LYNX_LOG_ROM_DATA, be32_to_cpu(ipod->csr_config_rom_cpu[i]));
	}

	/* Set XLOG & ROMvalid to Valid Config-ROM data */
	fw_reg_write(LYNX_LOG_ROM_CTRL, (fw_reg_read(LYNX_LOG_ROM_CTRL) & 0xFFFF7FFF)|LYNX_ROM_CTRL_VALID);
}

/* This function must store a pointer to the configuration ROM into the
 * location referenced to by pointer and return the size of the ROM. It
 * may not fail.  If any allocation is required, it must be done
 * earlier.
 */
static size_t ipod_1394_get_rom(struct hpsb_host *host, quadlet_t **ptr)
{
	struct ti_ipod *ipod = host->hostdata;

	*ptr = ipod->csr_config_rom_cpu;

	return ipod->csr_config_rom_length * 4;
}

static int tx_packet(struct ti_ipod *ipod, struct hpsb_packet *packet)
{
	/* write to ATF (& hold timer) */
	/* TODO: this doesnt seem to set the hold tr 0x4 like it should? */
	fw_reg_write(LYNX_TXTIMER_CTRL, 0x20);

	if (packet->tcode == TCODE_WRITEQ || packet->tcode == TCODE_READQ_RESPONSE) {
		cpu_to_be32s(&packet->header[3]);
	}

	fw_reg_write(LYNX_WRITE_FIRST, (packet->speed_code << 16) | (packet->header[0] & 0xffff));
	fw_reg_write(LYNX_WRITE_CONTINUE, (packet->header[0] & 0xffff0000) | (packet->header[1] & 0xffff));

	if (packet->data_size) {
		size_t data_len;
		quadlet_t *q;

		fw_reg_write(LYNX_WRITE_CONTINUE, packet->header[2]);
		if (packet->header_size == 16) {
			fw_reg_write(LYNX_WRITE_CONTINUE, packet->header[3]);
		}

		data_len = packet->data_size / 4;
		q = packet->data;
		while (data_len-- > 1) {
			fw_reg_write(LYNX_WRITE_CONTINUE, cpu_to_be32(*q));
			q++;
		}
		fw_reg_write(LYNX_WRITE_UPDATE, cpu_to_be32(*q));
	}
	else {
		if (packet->header_size == 12) {
			fw_reg_write(LYNX_WRITE_UPDATE, packet->header[2]);
		}
		else {
			fw_reg_write(LYNX_WRITE_CONTINUE, packet->header[2]);
			fw_reg_write(LYNX_WRITE_UPDATE, packet->header[3]);
		}
	}

	/* write to ATF & release timer */
	/* TODO: is this not required? */
	/* fw_reg_write(LYNX_TXTIMER_CTRL,  (fw_reg_read(LYNX_TXTIMER_CTRL) & ~0xff) | 0x26); */

	return 1;
}

static int tx_raw_packet(struct ti_ipod *ipod, struct hpsb_packet *packet)
{
	/* write to ATF (& hold timer) */
	/* TODO: this doesnt seem to set the hold tr 0x4 like it should? */
	fw_reg_write(LYNX_TXTIMER_CTRL, 0x20);

	/* bits 24,25,26 are always 1 in both quads */
	fw_reg_write(LYNX_WRITE_FIRST, packet->header[0] | 0x00e0);
	fw_reg_write(LYNX_WRITE_UPDATE, packet->header[1] | 0x00e0);

	/* write to ATF & release timer */
	/* TODO: is this not required? */
	/* fw_reg_write(LYNX_TXTIMER_CTRL,  (fw_reg_read(LYNX_TXTIMER_CTRL) & ~0xff) | 0x26); */

	return 1;
}

static int rx_packet(struct ti_ipod *ipod)
{
	quadlet_t data = 0;
	static quadlet_t pkt[1024];
	unsigned status;
	int packet_acked;
	int tcode;
	int i;

	status = fw_reg_read(LYNX_ARF_STATUS);
	if ((status & LYNX_XXFSTAT_EMPTY) == LYNX_XXFSTAT_EMPTY) {
		printk(KERN_ERR "rx_packet ARF Empty\n");
		return 0;
	}

	/* ensure at the beginning of a packet */
	while ((status & LYNX_XRFSTAT_PKT_CD) != LYNX_XRFSTAT_PKT_CD) {
		data = fw_reg_read(LYNX_ARF_DATA);
		status = fw_reg_read(LYNX_ARF_STATUS);

		if ((status & LYNX_XXFSTAT_EMPTY) == LYNX_XXFSTAT_EMPTY) {
			printk(KERN_ERR "rx_packet cant find pkt start\n");
			return 0;
		}
	}

	/* ensure next quad is not bogus */
	while ((status & LYNX_XRFSTAT_PKT_CD) == LYNX_XRFSTAT_PKT_CD) {
		data = fw_reg_read(LYNX_ARF_DATA);
		status = fw_reg_read(LYNX_ARF_STATUS);

		if ((status & LYNX_XXFSTAT_EMPTY) == LYNX_XXFSTAT_EMPTY) {
			printk(KERN_ERR "rx_packet skipped crap\n");
			return 0;
		}
	}

	packet_acked = ((data & 0x0f) == ACK_COMPLETE) ? 1 : 0;

	for (i = 0; i < 3; i++) {
		pkt[i] = fw_reg_read(LYNX_ARF_DATA);
	}

	tcode = (pkt[0] >> 4) & 0xf;
	if (tcode == TCODE_WRITEQ || tcode == TCODE_READQ_RESPONSE) {
		pkt[3] = cpu_to_be32(fw_reg_read(LYNX_ARF_DATA));
		hpsb_packet_received(ipod->host, pkt, 16, packet_acked);
	}
	else if (tcode == TCODE_READB) {
		pkt[3] = fw_reg_read(LYNX_ARF_DATA);
		hpsb_packet_received(ipod->host, pkt, 16, packet_acked);
	}
	else if (tcode == TCODE_READQ || tcode == TCODE_WRITE_RESPONSE) {
		hpsb_packet_received(ipod->host, pkt, 12, packet_acked);
	}
	else if (tcode == TCODE_WRITEB || tcode == TCODE_READB_RESPONSE)
	{
		int block_len;

		pkt[3] = fw_reg_read(LYNX_ARF_DATA);
		block_len = pkt[3] >> 16;

		for (i = 0; i < (block_len + 3) / 4; i++) {
			if (fw_reg_read(LYNX_ARF_STATUS) & LYNX_XXFSTAT_EMPTY) {
				printk(KERN_ERR "ARF empty\n");
				return 0;
			}
			pkt[4+i] = cpu_to_be32(fw_reg_read(LYNX_ARF_DATA));
		}

		hpsb_packet_received(ipod->host, pkt, 16 + block_len, packet_acked);
	}
	else {
		printk(KERN_ERR "Unknown tcode 0x%x\n", tcode);
	}

	status = fw_reg_read(LYNX_ARF_STATUS);
	return (status & LYNX_XXFSTAT_EMPTY) != LYNX_XXFSTAT_EMPTY;
}

static void tx_packet_end(struct ti_ipod *ipod)
{
	unsigned ack;

	ack = (fw_reg_read(LYNX_ACK) & 0xf0) >> 4;

	if (fw_reg_read(LYNX_TXTIMER_CTRL) & LYNX_TXTIMER_ATERR) {
		printk(KERN_ERR "LYNX_TXTIMER_ATERR\n");
		hpsb_packet_sent(ipod->host, ipod->packet, ACKX_SEND_ERROR);

		return;
	}

	/* broadcast messages don't get a response */
	if ((ipod->packet->node_id & NODE_MASK) == NODE_MASK) {
		if (ack != ACK_COMPLETE) {
			/* TODO: check this */
			printk(KERN_ERR "broadcast without ACK_COMPLETE\n");
		}
		hpsb_packet_sent(ipod->host, ipod->packet, ACK_COMPLETE);

		return;
	}

	/* mark the message as sent */
	hpsb_packet_sent(ipod->host, ipod->packet, ack);

	/* are we expecting a reponse and is it there? */
	if (ipod->packet->expect_response && ack == ACK_PENDING) {
		/* read response packet */
		rx_packet(ipod);
	}
}

static void tx_tasklet(unsigned long data)
{
	struct ti_ipod *ipod = (struct ti_ipod *)data;
	unsigned long flags;

	spin_lock_irqsave(&ipod->tx_lock, flags);

	if (ipod->tx_state == TX_END) {
		ipod->tx_state = TX_READY;
		tasklet_schedule(&ipod->tx_tasklet);

		tx_packet_end(ipod);
	}

	if (ipod->rx_state == RX_BUSY) {
		ipod->rx_state = RX_READY;

		while (rx_packet(ipod)) ;
	}

	if (ipod->tx_state == TX_READY) {
		struct hpsb_packet *packet = 0;

		spin_lock(&ipod->tx_list_lock);
		if (!list_empty(&ipod->tx_list)) {
			packet = driver_packet(ipod->tx_list.next);
			list_del(&packet->driver_list);
		}
		spin_unlock(&ipod->tx_list_lock);

		if (packet) {
			ipod->packet = packet;
			ipod->tx_state = TX_BUSY;

			if (packet->type == hpsb_async) {
				tx_packet(ipod, packet);
			}
			else if (packet->type == hpsb_raw) {
				tx_raw_packet(ipod, packet);
			}
		}
	}

	spin_unlock_irqrestore(&ipod->tx_lock, flags);
}

/* This function shall implement packet transmission based on
 * packet->type.  It shall CRC both parts of the packet (unless
 * packet->type == raw) and do byte-swapping as necessary or instruct
 * the hardware to do so.  It can return immediately after the packet
 * was queued for sending.  After sending, hpsb_sent_packet() has to be
 * called.  Return 0 for failure.
 * NOTE: The function must be callable in interrupt context.
 */
static int ipod_1394_transmit_packet(struct hpsb_host *host, struct hpsb_packet *packet)
{
	struct ti_ipod *ipod = host->hostdata;

	switch (packet->type) {
	case hpsb_async:
	case hpsb_raw:
		break;

	case hpsb_iso:
	default:
		printk(KERN_ERR "cannot do pkt type 0x%x\n", packet->type);
		return 0;
	}

	if (packet->data_size > (fw_reg_read(LYNX_ATF_STATUS)&0x1ff)) {
		printk(KERN_ERR "Transmit packet size %d is too large",
			packet->data_size);
		return 0;
	}

	spin_lock(&ipod->tx_list_lock);
	list_add_tail(&packet->driver_list, &ipod->tx_list);
	tasklet_schedule(&ipod->tx_tasklet);
	spin_unlock(&ipod->tx_list_lock);

	return 1;
}

/* This function requests miscellaneous services from the driver, see
 * above for command codes and expected actions.  Return -1 for unknown
 * command, though that should never happen.
 */
static int ipod_1394_devctl(struct hpsb_host *host, enum devctl_cmd command, int arg)
{
	struct ti_ipod *ipod = host->hostdata;
	int retval = 0;
	int phy_reg;

	switch (command) {
	case RESET_BUS:
		switch (arg)
		{
		case SHORT_RESET:
			set_phy_reg_mask(ipod, 5, 0x40); /* set ISBR */
			break;
		case LONG_RESET:
			set_phy_reg_mask(ipod, 1, 0x40); /* set IBR */
			break;

		case SHORT_RESET_NO_FORCE_ROOT:
			phy_reg = get_phy_reg(ipod, 1);
			if (phy_reg & 0x80) {
				phy_reg &= ~0x80;
				set_phy_reg(ipod, 1, phy_reg); /* clear RHB */
			}

			phy_reg = get_phy_reg(ipod, 5);
			phy_reg |= 0x40;
			set_phy_reg(ipod, 5, phy_reg); /* set ISBR */
			break;

		case LONG_RESET_NO_FORCE_ROOT:
			phy_reg = get_phy_reg(ipod, 1);
			phy_reg &= ~0x80;
			phy_reg |= 0x40;
			set_phy_reg(ipod, 1, phy_reg); /* clear RHB, set IBR */
			break;

		case SHORT_RESET_FORCE_ROOT:
			phy_reg = get_phy_reg(ipod, 1);
			if (!(phy_reg & 0x80)) {
				phy_reg |= 0x80;
				set_phy_reg(ipod, 1, phy_reg); /* set RHB */
			}

			phy_reg = get_phy_reg(ipod, 5);
			phy_reg |= 0x40;
			set_phy_reg(ipod, 5, phy_reg); /* set ISBR */
			break;

		case LONG_RESET_FORCE_ROOT:
			phy_reg = get_phy_reg(ipod, 1);
			phy_reg |= 0xc0;
			set_phy_reg(ipod, 1, phy_reg); /* set RHB and IBR */
			break;

		default:
			set_phy_reg_mask(ipod, 1, 0x40);
			break;
		}

		break;

	case GET_CYCLE_COUNTER:
		retval = fw_reg_read(LYNX_CYCLE_TIMER);
		break;

	case SET_CYCLE_COUNTER:
		fw_reg_write(LYNX_CYCLE_TIMER, arg);
		break;

	case SET_BUS_ID:
		printk(KERN_ERR "devctl command SET_BUS_ID err\n");
		break;

	case ACT_CYCLE_MASTER:
		if (arg) {
			/* enable cyclemaster */
			fw_reg_write(LYNX_CTRL, fw_reg_read(LYNX_CTRL)|LYNX_CTRL_CYCLE_MASTER);
		}
		else {
			/* disable cyclemaster */
			fw_reg_write(LYNX_CTRL, fw_reg_read(LYNX_CTRL)&~LYNX_CTRL_CYCLE_MASTER);
		}
		break;

	case CANCEL_REQUESTS:
		/* TODO: implement this */
		break;

	case MODIFY_USAGE:
		if (arg) {
			MOD_INC_USE_COUNT;
		} else {
			MOD_DEC_USE_COUNT;
		}
		retval = 1;
		break;

	case ISO_LISTEN_CHANNEL:
	case ISO_UNLISTEN_CHANNEL:
	default:
		printk(KERN_ERR "unknown devctl cmd %d\n", command);
		break;
	}

	return retval;
}


#define IPOD_1394_DRIVER_NAME	"ipod1394"

static struct hpsb_host_driver ipod_1394_driver = {
	.name = IPOD_1394_DRIVER_NAME,
	.get_rom = ipod_1394_get_rom,
	.transmit_packet = ipod_1394_transmit_packet,
	.devctl = ipod_1394_devctl,
};

#define SELF_ID_BEGIN        0x000000e0ul
#define SELF_ID_END          0x00000001ul
#define SELF_ID_VALID        0x80000000ul

/*
 * The self-id packet stream looks like:
 * 0xe0
 * 0x80xxxxxx
 * 0x81xxxxxx
 * 0x1
 * 0xe0
 * 0xe0
 * 0xe0
 */
static void handle_selfid(struct ti_ipod *ipod, struct hpsb_host *host)
{
	quadlet_t q0 = 0;
	size_t size;
	int num_nodes;
	int phyid;
	int isroot;

	/* Check the DRF for available data in FIFO */
	size = fw_reg_read(LYNX_DXF_AVAIL) & 0x7ff;
	if (size == 0) {
		printk(KERN_ERR "No self-id data in FIFO\n");
		/* TODO, need to initiate bus reset */
		return;
	}

	while (size > 0) {
		/* scan through DRF data for start of self-id data */
		do {
			q0 = fw_reg_read(LYNX_DRF_DATA);	/* read from DRF */
			size = fw_reg_read(LYNX_DXF_AVAIL) & 0x7ff;
		} while (q0 == SELF_ID_BEGIN && size > 0);

		if (size == 0) {
			/* TODO, this is an error if we havent seen any nodes */
			if (host->selfid_count == 0) {
				printk(KERN_ERR "No self-ids seen\n");
			}
			break;
		}

		if (fw_reg_read(LYNX_INTERRUPT) & LYNX_INTERRUPT_BusRst ) {
			printk(KERN_ERR "Bus reset pending\n");
			return;
		}

		num_nodes = (fw_reg_read(LYNX_BUS_RESET) >> 6) & 0x003f;
		host->selfid_count = 0;	/* reset if we are doing this again */

		while (num_nodes > 0) {
			if ((q0 & 0xc0000000) == SELF_ID_VALID) {
				printk(KERN_DEBUG "SelfID packet 0x%x received\n", q0);

				hpsb_selfid_received(host, cpu_to_be32(q0));

				if (!(q0 & 0x1)) {
					num_nodes--;
				}
			}
			else if (q0 == SELF_ID_END) {
				break;
			}
			q0 = fw_reg_read(LYNX_DRF_DATA);	/* read from DRF */
			size = fw_reg_read(LYNX_DXF_AVAIL) & 0x7ff;
		}
	}

	phyid = (fw_reg_read(LYNX_BUS_RESET) >> 16) & 0x3f;
	isroot = (fw_reg_read(LYNX_ACK) & 0x8000) != 0;

	printk(KERN_DEBUG "SelfID complete %x(%d)\n", phyid, (isroot?1:0));

	hpsb_selfid_complete(host, phyid, isroot);
}

static void ipod_1394_fw_int(int irq, struct ti_ipod *ipod)
{
	struct hpsb_host *host = ipod->host;
	unsigned fw_src;

	/* interrupt source */
	fw_src = fw_reg_read(LYNX_INTERRUPT);
	if (fw_src) {
		/* ack the interrupt */
		fw_reg_write(LYNX_INTERRUPT, fw_src);

		if (fw_src & LYNX_INTERRUPT_BusRst) {
			if (!host->in_bus_reset) {
				printk(KERN_ERR "irq: bus reset requested\n");
				hpsb_bus_reset(host);
			}
		}
		if (fw_src & LYNX_INTERRUPT_SelfIDEnd) {
			if (host->in_bus_reset) {
				quadlet_t bus_reset;
				int BRFErr_Code;

				bus_reset = (fw_reg_read(LYNX_BUS_RESET) & LYNX_BUSRESET_ERRCODE) >> 12;
				BRFErr_Code = (bus_reset & LYNX_BUSRESET_ERRCODE) >> 12;
				/* ignore gap count errors */
				if (BRFErr_Code == 0x0 || BRFErr_Code == 0xf)
				{
					/* TODO: this should be outside isr! */
					handle_selfid(ipod, host);
				}
				else
				{
					printk(KERN_ERR "Bus reset err 0x%x\n", BRFErr_Code);
					/* TODO, initiate bus reset */
				}
			}
			else {
				printk(KERN_ERR "irq: selfid outside bus reset\n");
			}
		}

		if (fw_src & LYNX_INTERRUPT_ATFEnd) {
			unsigned long flags;

			spin_lock_irqsave(&ipod->tx_lock, flags);
			ipod->tx_state = TX_END;
			tasklet_schedule(&ipod->tx_tasklet);
			spin_unlock_irqrestore(&ipod->tx_lock, flags);
		}

		if (fw_src & LYNX_INTERRUPT_RxARF) {
			unsigned long flags;

			spin_lock_irqsave(&ipod->tx_lock, flags);
			ipod->rx_state = RX_BUSY;
			tasklet_schedule(&ipod->tx_tasklet);
			spin_unlock_irqrestore(&ipod->tx_lock, flags);
		}

#if 0
		// we handle these
		fw_src &= ~(LYNX_INTERRUPT_BusRst|LYNX_INTERRUPT_SelfIDEnd|LYNX_INTERRUPT_ATFEnd|LYNX_INTERRUPT_RxARF);

		fw_src &= ~( LYNX_INTERRUPT_Int | LYNX_INTERRUPT_FairGap | LYNX_INTERRUPT_TxRdy | LYNX_INTERRUPT_CySec| LYNX_INTERRUPT_CyStart| LYNX_INTERRUPT_CyDne| LYNX_INTERRUPT_CyPending| LYNX_INTERRUPT_CyLost| LYNX_INTERRUPT_CyArbFail );
		if (host->in_bus_reset) {
			fw_src &= ~( LYNX_INTERRUPT_PhyPkt | LYNX_INTERRUPT_RxPhyReg | LYNX_INTERRUPT_UpdDRFHdr | LYNX_INTERRUPT_DRFEnd );
		}
		if (fw_src) {
			printk("fw_src %04x\n", fw_src);
		}
#endif
	}
}

static void ipod_1394_interrupt(int irq, void *dev_id, struct pt_regs *regs_are_unused)
{
	struct ti_ipod *ipod = (struct ti_ipod *)dev_id;
	unsigned source;

	/* get source of interupts */
	source = inb(0xcf000048);
	if (source) {
		outb(source, 0xcf000078);

		if (source & (1<<4) ) {
			/* get current status port c */
			unsigned state = inb(0xcf000038);

			if (state & (1<<4)) {
				printk(KERN_ERR "fw pwr +\n");
			}
			else {
				printk(KERN_ERR "fw pwr -\n");
			}

			outb(~state, 0xcf000068);
		}

		if (source & (1<<5)) {
			ipod_1394_fw_int(irq, ipod);
		}

		if (source & ~(1<<4 | 1<<5)) {
			printk(KERN_ERR "src=0x%x\n", source);
		}
	}
}

static __devinit void ipod_1394_hw_init(void)
{
	unsigned ipod_hw_ver;

	ipod_hw_ver = ipod_get_hw_version() >> 16;

	/* MIO setup? */
	outl((inl(0xcf004040) & ~(1<<6)) | (1<<7), 0xcf004040);
	outl(0x00001f1f, 0xcf00401c);
	outl(0xffff, 0xcf004020);

	/* this is the 1g and 2g init */
	if (ipod_hw_ver == 0x1 || ipod_hw_ver == 0x2)
	{
		/* port D enable, PD(4) power down and LPS(5) link power staus, and(7?) */
		outl(inl(0xcf00000c) | (1<<4)|(1<<5)|(1<<7), 0xcf00000c);

		/* port D output enable */
		outl(inl(0xcf00001c) | (1<<4)|(1<<5)|(1<<7), 0xcf00001c);

		/* port D output value ~10000 | 100000 */
		/* set PD (power down) to low and LPS to high (see 12.4) */
		outl((inl(0xcf00002c) & ~(1<<4)) | (1<<5), 0xcf00002c);

		udelay(20);
	}

	/* this is 1g only */
	if (ipod_hw_ver == 0x1)
	{
		/* Reset TSB43AA82 link and PHY (see section 12.5) */
		outl(inl(0xcf00000c) | (1<<1) | (1<<2) , 0xcf00000c);
		outl(inl(0xcf00001c) | (1<<1) | (1<<2), 0xcf00001c);
		outl(inl(0xcf00002c) | (1<<1) | (1<<2), 0xcf00002c);

		udelay(0x14);

		/* XRESETL */
		outl(inl(0xcf00002c) & ~(1<<1), 0xcf00002c);
		/* XRESETP */
		outl(inl(0xcf00002c) & ~(1<<2), 0xcf00002c);

		udelay(0x14);

		outl(inl(0xcf00002c) | (1<<1), 0xcf00002c);
		udelay(0x1);
		outl(inl(0xcf00002c) | (1<<2), 0xcf00002c);

		mdelay(10);
	}

	/* this is 2g only */
	if (ipod_hw_ver == 0x2)
	{
		unsigned r2;

		/* Port E Bit 2 Enable */
		outl(inl(0xcf004048) | (1<<2), 0xcf004048);

		r2 = inl(0xcf004044);

		/* Port E Bit 4 output high */
		outl(inl(0xcf004048) | (1<<4), 0xcf004048);

		udelay(0x14);

		/* Port E Bit 2 output low */
		outl(inl(0xcf004048) & ~(1<<2), 0xcf004048);
		/* Port E Bit 4 output low */
		outl(inl(0xcf004048) & ~(1<<4), 0xcf004048);

		udelay(0x14);

		/* Port E Bit 2 output high */
		outl(inl(0xcf004048) | (1<<2), 0xcf004048);
		udelay(0x1);
		/* Port E Bit 4 output high */
		outl(inl(0xcf004048) | (1<<4), 0xcf004048);

		udelay(0x14);

		outl(r2 | (1<<4), 0xcf004044);
	}

	/* this is 3g */
	if (ipod_hw_ver == 0x3)
	{
		/* reset device i2c */
		ipod_i2c_init();

		/* some i2c magic - this is the PCF address */
		ipod_i2c_send(0x8, 0x39, 0);
		ipod_i2c_send(0x8, 0x3a, 0);
		ipod_i2c_send(0x8, 0x3b, 0);
		ipod_i2c_send(0x8, 0x3c, 0);
		ipod_i2c_send(0x8, 0x39, 7);
		ipod_i2c_send(0x8, 0x3a, 0);
		ipod_i2c_send(0x8, 0x3b, 7);
		ipod_i2c_send(0x8, 0x3c, 7);
		ipod_i2c_send(0x8, 0x3b, 0);
		ipod_i2c_send(0x8, 0x3c, 0);

		/* don't enable GPIO port D bit 7 & set it to low
		 * this seems to enable firewire DMA which messes
		 * with the ide driver
		 */
		/*
		outl(inl(0xcf00000c) | (1<<7), 0xcf00000c);
		outl(inl(0xcf00001c) | (1<<7), 0xcf00001c);
		outl(inl(0xcf00002c) & ~(1<<7), 0xcf00002c);
		*/
	}

	if (fw_reg_read(LYNX_VERSION) != LYNX_VERSION_CHIP_ID) {
		printk(KERN_ERR "ipod_1394: invalid chip revsion 0x%x\n", fw_reg_read(LYNX_VERSION));
	}
}

static int __devinit ipod_1394_init(void)
{
	struct ti_ipod *ipod;

	printk("ipod_1394: $Id: tsb43aa82.c,v 1.6 2004/12/18 14:17:31 leachbj Exp $\n");

	ipod_host = hpsb_alloc_host(&ipod_1394_driver, sizeof(struct ti_ipod));
	if (!ipod_host) {
		printk(KERN_ERR "ipod_1394: failed to allocate control structure\n");
		return -ENOMEM;
	}

	ipod = ipod_host->hostdata;
	ipod->host = ipod_host;
	ipod->csr_config_rom_cpu = kmalloc(CONFIG_ROM_LEN, GFP_KERNEL);
	if (ipod->csr_config_rom_cpu == NULL) {
		printk(KERN_ERR "ipod_1394: Failed to allocate buffer config rom");
	}

	spin_lock_init(&ipod->phy_reg_lock);
	spin_lock_init(&ipod->tx_lock);
	spin_lock_init(&ipod->tx_list_lock);
	INIT_LIST_HEAD(&ipod->tx_list);

	tasklet_init(&ipod->tx_tasklet, tx_tasklet, (unsigned long)ipod);

	if (request_irq(PP5002_GPIO_IRQ, ipod_1394_interrupt, SA_SHIRQ, IPOD_1394_DRIVER_NAME, ipod)) {
		printk(KERN_ERR "ipod_1394: IRQ %d failed\n", PP5002_GPIO_IRQ);
	}

	ipod_1394_hw_init();


	/* Busy off until ready */
	fw_reg_write(LYNX_CTRL, LYNX_CTRL_ACK_BUSY_X|LYNX_CTRL_CYCLE_TIMER_ENABLE);

	/* Set FIFO sizes to inital values */
	fw_reg_write(LYNX_DXF_SIZE,
		  ((DMA_WRITE_FIFO_SIZE /* Set DTF size */
		| (LYNX_DTFDRF_PTBUF_SIZE(DXF_PG_ENTRY)*2) << 10) << 16)  /* Set DTF PgTbl Quads */
		| DMA_READ_FIFO_SIZE /* Set DRF size */
		| (LYNX_DTFDRF_PTBUF_SIZE(DXF_PG_ENTRY)*2) << 10);  /*Set DRF PgTbl Quads */

	fw_reg_write(LYNX_DMA_CTRL,
		 DMA_CTRL_DRF_PAGE_FETCH_04     /* Set PgTbl Fetch size */
		|DMA_CTRL_DTF_PAGE_FETCH_04);
	fw_reg_write(LYNX_ATF_STATUS, ASYNC_TX_FIFO_SIZE);
	fw_reg_write(LYNX_ARF_STATUS, ASYNC_RX_FIFO_SIZE);
	fw_reg_write(LYNX_CTQ_STATUS, COMMAND_TX_QUE_SIZE);
	fw_reg_write(LYNX_CRF_STATUS, COMMAND_RX_FIFO_SIZE);
                                                                                
	init_config_rom(ipod);

	/* Initialize ASYNC FIFO's */
	fw_reg_write(LYNX_ATF_STATUS, 0x1000|ASYNC_TX_FIFO_SIZE);
	fw_reg_write(LYNX_ARF_STATUS, 0x1000|ASYNC_RX_FIFO_SIZE);
	fw_reg_write(LYNX_MTQ_STATUS, 0x1000);
	fw_reg_write(LYNX_MRF_STATUS, 0x1000);
	fw_reg_write(LYNX_CTQ_STATUS, 0x1000|COMMAND_TX_QUE_SIZE);
	fw_reg_write(LYNX_CRF_STATUS, 0x1000|COMMAND_RX_FIFO_SIZE);

	/* Set Split Timeout, Retry Interval and Retry limit */
	fw_reg_write(LYNX_TIMELIMIT, LYNX_TIMELIMIT_INIT_VALUE);

	/* Initialize DMA FIFO's */
	fw_reg_write(LYNX_DMA_CTRL, fw_reg_read(LYNX_DMA_CTRL)|DMA_CTRL_DRF_CLEAR|DMA_CTRL_DTF_CLEAR);
	/* Ensure DMA FIFOs have cleared */
	while ( fw_reg_read(LYNX_DMA_CTRL) & (DMA_CTRL_DRF_CLEAR|DMA_CTRL_DTF_CLEAR) );

	fw_reg_write(LYNX_DMA_CTRL, (fw_reg_read(LYNX_DMA_CTRL) & 0xfc00)
		| DMA_CTRL_DRF_ENABLE | DMA_CTRL_DTF_ENABLE
		/* | DMA_CTRL_DRF_PKTZ_ENABLE | DMA_CTRL_DTF_PKTZ_ENABLE */
		| DMA_CTRL_CONF_SINGL_PKT );
		/* | DMA_CTRL_CHECK_PAGE_TABLE | DMA_CTRL_AUTO_PAGING */

	/* Initialize Bulky IF Control register, Baseline */
	fw_reg_write(LYNX_BI_CTRL, BDI_CTRL_AUTO_PAD_ENABL|BDI_CTRL_RCV_PAD_ENABLE );

	fw_reg_write(LYNX_CTRL,
		 LYNX_CTRL_ACK_BUSY_X	/* still busy */
		|LYNX_CTRL_SELFID_RX_ENABLE
		|LYNX_CTRL_SELFID_TO_DRF
		|LYNX_CTRL_TRANS_ENABLE 
		|LYNX_CTRL_ACCELARATED_ARB
		|LYNX_CTRL_RESET_TRANS
		|LYNX_CTRL_SPLIT_TRANS_ENABLE
		|LYNX_CTRL_RETRY_ENABLE
		|LYNX_CTRL_ACK_PENDING
		|LYNX_CTRL_CYCLE_MASTER
		|LYNX_CTRL_CYCLE_TIMER_ENABLE
		|LYNX_CTRL_DMA_CLEAR	/* ensure DMA state machines ready */
		|LYNX_CTRL_RCV_UNEXPECT_PKT);
		// LYNX_CTRL_UNEXPECT_PKT_TO_DRF // receive packets to DRF not ATF

	/* Clear DMAClear (this is a bug in ES1, it should automatically clear) */
	fw_reg_write(LYNX_CTRL, fw_reg_read(LYNX_CTRL) & ~LYNX_CTRL_DMA_CLEAR);

	/* Disable interrupt mask bits */
	fw_reg_write(LYNX_IMASK, 0);

	/* Clear interrupts */
	fw_reg_write(LYNX_INTERRUPT, LYNX_INTERRUPT_ALL_CLEAR);

	/*
	 * disable the dynamic power consumption, this fixes a bug with
	 * receiving broadcast messages.
	 */
	fw_reg_write(LYNX_RESERVED, POWER_CONSUMPTION_DIS);

	/* setup firewire power (4) and firewire (5) interrupts */
	{
		/* enable port c bits 4 & 5 */
		outb(inb(0xcf000008) | (1<<4) | (1<<5), 0xcf000008);

		/* pin 4 & 5 is input */
		outb(inl(0xcf000018) & ~((1<<4) | (1<<5)), 0xcf000018);

		outb(~inb(0xcf000038), 0xcf000068);
		outb(inb(0xcf000068) & ~(1<<5), 0xcf000068);

		outb(inb(0xcf000048), 0xcf000078);
		outb((1<<4)|(1<<5), 0xcf000058);
	}

	/* Disable interrupt mask bits */
	fw_reg_write(LYNX_IMASK, LYNX_INTERRUPT_BusRst|LYNX_INTERRUPT_SelfIDEnd|LYNX_INTERRUPT_ATFEnd|LYNX_INTERRUPT_RxARF);

	/* ready to process packets now */
	fw_reg_write(LYNX_CTRL, fw_reg_read(LYNX_CTRL) & ~LYNX_CTRL_ACK_BUSY_X);

	hpsb_add_host(ipod_host);

	return 0;
}

static void __exit ipod_1394_exit(void)
{
	struct ti_ipod *ipod = ipod_host->hostdata;

	hpsb_remove_host(ipod->host);

	/* free the IRQ */
	free_irq(PP5002_GPIO_IRQ, ipod);

        /* Wait and kill tasklet */
        tasklet_kill(&ipod->tx_tasklet);

	kfree(ipod->csr_config_rom_cpu);

	hpsb_unref_host(ipod_host);
	ipod_host = 0;

	/* XXX power down the TI chip */
}

module_init(ipod_1394_init);
module_exit(ipod_1394_exit);

MODULE_AUTHOR("Bernard Leach <leachbj@bouncycastle.org>");
MODULE_DESCRIPTION("driver for Texas Instruments IEEE-1394 controller in the iPod");
MODULE_LICENSE("GPL");

