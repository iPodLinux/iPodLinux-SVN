/*
 * tsb43aa82.c - ieee1394 driver for iPod
 *
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
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

#define RX_SLD	(1<<(31-1))
#define RS_ISEL	(1<<(31-2))
#define CONTROL_REG	0x8

#define LOG_ROM_CONTROL	0xf8
#define LOG_CLR	(1<<(31-15))

#define BUS_RESET	(1<<(31-2))
#define END_SELF_ID	(1<<(31-4))

#define ATF_END		(1<<(31-24))	/* response to our message is avail */
#define ARF_RXD		(1<<(31-25))	/* request packet is waiting */

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
		for (t = 0; t < 4 && str[t]; t++)
			fourb[t] = str[t];
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

	fw_reg_write(0x20, addr << 24 | 0x80000000);

	for (i = 0; i < IPOD_LOOP_COUNT; i++) {
		if ( !(fw_reg_read(0x20) & 0x80000000) )
			break;

		mdelay(1);
	}

	r = fw_reg_read(0x20);

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

	fw_reg_write(0x20, (addr << 24) | (data << 16) | 0x40000000);

	for (i = 0; i < IPOD_LOOP_COUNT; i++) {
		r = fw_reg_read(0x20);
		if ( !(r & 0x40000000) )
			break;

		mdelay(1);
	}

	if ( i == IPOD_LOOP_COUNT ) {
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
	cf_put_1quad(&cr, 0x00ff6002);	/* Bus options (based iPod f/w) */
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

	/* Configuration ROM control register
         * set the rom size
	 * 31-2 == 29
	 * 31-18 == 13
	 * multiplies arg_r1 by 4 to get bytes
	 */
	fw_reg_write(0x8c, (ipod->csr_config_rom_length << (31-13)) | (ipod->csr_config_rom_length << (31-29)));

	/* Log/ROM control register
	 * 00000000000000001000010000000000 (1<<15) | (1<<10)
	 * Set XLOG=1
	 */
	fw_reg_write(0xf8, (1<<(31-16))|(1<<(31-21)));

	for (i = 0; i < ipod->csr_config_rom_length; i++) {
		/* log data register, write the ROM data */
		fw_reg_write(0xfc, be32_to_cpu(ipod->csr_config_rom_cpu[i]));
	}

	/* log/rom control register
 	 *  100000000000000 (1<<14) (31-14 -> 17 == ROMValid)
	 * 1000000000000000 (1<<15) (31-15 -> 16 == XLOG)
	 */
	fw_reg_write(0xf8, (fw_reg_read(0xf8) & ~(1<<(31-16))) | (1<<(31-17)));
}

/* This function must store a pointer to the configuration ROM into the
 * location referenced to by pointer and return the size of the ROM. It
 * may not fail.  If any allocation is required, it must be done
 * earlier.
 */
static size_t ipod_1394_get_rom(struct hpsb_host *host, quadlet_t **ptr)
{
	struct ti_ipod *ipod=host->hostdata;

	*ptr = ipod->csr_config_rom_cpu;

	return ipod->csr_config_rom_length * 4;
}

static int tx_packet(struct ti_ipod *ipod, struct hpsb_packet *packet)
{
	unsigned r = 0;

	/* write to ATF & hold timer */
	/* TXN_TIMER_CONTROL */
	r = fw_reg_read(0x60);
	fw_reg_write(0x60, (r & ~0xff) | 0x24 | (r & 0x4 ? 0 : 0x4));

#define WRITE_FIRST	0x70
#define WRITE_CONTINUE	0x74
#define WRITE_UPDATE	0x78

	if ( packet->tcode == TCODE_WRITEQ || packet->tcode == TCODE_READQ_RESPONSE ) {
		cpu_to_be32s(&packet->header[3]);
	}

	fw_reg_write(WRITE_FIRST, (packet->speed_code << 16) | (packet->header[0] & 0xffff));
	fw_reg_write(WRITE_CONTINUE, (packet->header[0] & 0xffff0000) | (packet->header[1] & 0xffff));

	if ( packet->data_size ) {
		size_t data_len;
		quadlet_t *q;

		fw_reg_write(WRITE_CONTINUE, packet->header[2]);
		if ( packet->header_size == 16 ) {
			fw_reg_write(WRITE_CONTINUE, packet->header[3]);
		}

		data_len = packet->data_size / 4;
		q = packet->data;
		while ( data_len-- > 1) {
			int to = 0;

			while ( fw_reg_read(0x2c) & (1<<(31-0)) ) {
				udelay(1);
				if ( to++ == 10 ) {
					printk(KERN_ERR "data_len=0x%x\n", data_len);
					panic("ATF full\n");
				}
			}

			fw_reg_write(WRITE_CONTINUE, cpu_to_be32(*q));
			q++;
		}
		fw_reg_write(WRITE_UPDATE, cpu_to_be32(*q));
	}
	else {
		if ( packet->header_size == 12 ) {
			fw_reg_write(WRITE_UPDATE, packet->header[2]);
		}
		else {
			fw_reg_write(WRITE_CONTINUE, packet->header[2]);
			fw_reg_write(WRITE_UPDATE, packet->header[3]);
		}
	}

	/* write to ATF & release timer */
	/* TXN_TIMER_CONTROL */
	fw_reg_write(0x60,  (fw_reg_read(0x60) & ~0xff) | 0x26);

	return 1;
}

static int tx_raw_packet(struct ti_ipod *ipod, struct hpsb_packet *packet)
{
	unsigned r = 0;

	/* write to ATF & hold timer */
	/* TXN_TIMER_CONTROL */
	r = fw_reg_read(0x60);
	fw_reg_write(0x60, (r & ~0xff) | 0x24 | (r & 0x4 ? 0 : 0x4));

	fw_reg_write(WRITE_FIRST, packet->header[0] | 0x00e0);
	fw_reg_write(WRITE_UPDATE, packet->header[1] | 0xffff);

	/* write to ATF & release timer */
	/* TXN_TIMER_CONTROL */
	fw_reg_write(0x60,  (fw_reg_read(0x60) & ~0xff) | 0x26);

	return 1;
}

static void rx_packet(struct hpsb_host *host)
{
	static quadlet_t data[1024], status;
	int i;
	int tcode;
	/* unsigned arf; */
	/* int avail; */

	if ( !(fw_reg_read(0x30) & 0x8000) ) {
		printk(KERN_ERR "ARF sts: 0x%04x\n", fw_reg_read(0x30));
		printk(KERN_ERR "ARF not at first quad\n");

		printk("0x%08x\n", status);
		for ( i = 0; i < 3; i++ ) {
			printk("0x%08x\n", data[i]);
		}
	}

	/* arf = fw_reg_read(0x30); */
	/* avail = (arf << 7) >> 23; */

	status = fw_reg_read(0x80);

	for ( i = 0; i < 3; i++ ) {
		data[i] = fw_reg_read(0x80);
	}

#if 0
	if ( avail > 0xb4 ) {
		printk("invalid avail 0x%x\n", avail);
		printk("arf 0x%08x\n", arf);

		printk("%08x ", status);
		for ( i = 0 ; i < 3; i++ ) {
			printk("%08x ", data[i]);
		}
		printk("\n");
	}
#endif

	tcode = (data[0] >> 4) & 0xf;
	if ( tcode == TCODE_WRITEQ || tcode == TCODE_READQ_RESPONSE ) {
		data[3] = cpu_to_be32(fw_reg_read(0x80));
		hpsb_packet_received(host, data, 16, 0);
	}
	else if ( tcode == TCODE_READB ) {
		data[3] = fw_reg_read(0x80);
		hpsb_packet_received(host, data, 16, 0);
	}
	else if ( tcode == TCODE_READQ || tcode == TCODE_WRITE_RESPONSE ) {
		hpsb_packet_received(host, data, 12, 0);
	}
	else if ( tcode == TCODE_WRITEB || tcode == TCODE_READB_RESPONSE )
{
		int block_len;

		data[3] = fw_reg_read(0x80);
		block_len = data[3] >> 16;

		/* if ( block_len > 500 ) printk("block_len %d arf 0x%08x\n", block_len, fw_reg_read(0x30)); */

		for (i = 0; i < (block_len + 3) / 4; i++) {
			int to = 0;

			while ( fw_reg_read(0x30) & (1<<(31-3)) ) {
				udelay(1);
				if ( to++ == 10 ) {
					printk(KERN_ERR "blocklen=0x%x\n", block_len);
					panic("ARF emtpy\n");
				}
			}

			data[4+i] = cpu_to_be32(fw_reg_read(0x80));
		}

		/* if ( block_len > 500 ) printk("done\n"); */

		hpsb_packet_received(host, data, 16 + block_len, 0);
	}
	else {
		printk(KERN_ERR "unknown tcode 0x%x\n", tcode);
		printk(KERN_ERR "0x%04x ", status);
		for ( i = 0; i < 3; i++ ) {
			printk("0x%04x ", data[i]);
		}
		printk("\n");
	}
}

static void tx_tasklet(unsigned long data)
{
	struct ti_ipod *ipod = (struct ti_ipod *)data;
	unsigned long flags;

	spin_lock_irqsave(&ipod->tx_lock, flags);

	if ( ipod->tx_state == TX_END ) {
		unsigned r;

		ipod->tx_state = TX_READY;
		tasklet_schedule(&ipod->tx_tasklet);

		if ( fw_reg_read(0x60) & 0x200000 ) {
			printk(KERN_ERR "ATErr\n");
			hpsb_packet_sent(ipod->host, ipod->packet, ACKX_SEND_ERROR);
		}
else {
		r = fw_reg_read(0x4);
		if ( r & (1<<(31-23)) ) {
			printk(KERN_ERR ">ARF sts: 0x%04x\n", fw_reg_read(0x30));
			printk(KERN_ERR "ack err\n");
			printk("ack 0x%04x\n", r);
			/* clear the ATF (only do this on timeout!) */
			/* fw_reg_write(0x2c, 0x10a0); */
			printk("tcode 0x%x tlabel 0x%x datalen %d\n", ipod->packet->tcode, ipod->packet->tlabel, ipod->packet->data_size);
			hpsb_packet_sent(ipod->host, ipod->packet, ACKX_SEND_ERROR);
		}
		else {
			if ( (ipod->packet->node_id & NODE_MASK) == NODE_MASK ) {
				hpsb_packet_sent(ipod->host, ipod->packet, 0x1);
			}
			else {
				int response = ipod->packet->expect_response;

				if ( ((r>>4) & 0xf) == 1 && response ) {
					printk(KERN_DEBUG "premature end? %d rx %d\n", ((fw_reg_read(0x30) << 7) >> 23), ipod->rx_state);

					/* TODO: we should really just read the
					 * reply and add to packet then
					 * send back the ACK_COMPLETE
					 */
					hpsb_packet_sent(ipod->host, ipod->packet, 0x2);
				}
				else {
					hpsb_packet_sent(ipod->host, ipod->packet, (r >> 4) & 0xf);
				}

				if ( response ) {
					if ( ((fw_reg_read(0x30) << 7) >> 23) == 0 ) {
						printk(KERN_ERR "no reply data ready\n");
					}
					else {
						rx_packet(ipod->host);

						/* if we have no more data ignore any interrupt */
						if ( ((fw_reg_read(0x30) << 7) >> 23) == 0 ) ipod->rx_state = RX_READY;
					}
				}
			}
		}
}
	}

	if ( ipod->rx_state == RX_BUSY ) {
		ipod->rx_state = RX_READY;

		rx_packet(ipod->host);
	}

	if ( ipod->tx_state == TX_READY ) {
		struct hpsb_packet *packet = 0;

		spin_lock(&ipod->tx_list_lock);
		if ( !list_empty(&ipod->tx_list) ) {
			packet = driver_packet(ipod->tx_list.next);
			list_del(&packet->driver_list);
		}
		spin_unlock(&ipod->tx_list_lock);

		if ( packet ) {
			ipod->packet = packet;
			ipod->tx_state = TX_BUSY;
			/* spin_unlock_irqrestore(&ipod->tx_lock, flags); */

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

	switch ( packet->type ) {
	case hpsb_async:
	case hpsb_raw:
		break;

	case hpsb_iso:
	default:
		printk(KERN_ERR "cannot do pkt type 0x%x\n", packet->type);
		return 0;
	}

	/* spin_lock_bh(&ipod->tx_list_lock); */
	spin_lock(&ipod->tx_list_lock);
	list_add_tail(&packet->driver_list, &ipod->tx_list);
	tasklet_schedule(&ipod->tx_tasklet);
	/* spin_unlock_bh(&ipod->tx_list_lock); */
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
		printk("devctl:RESET\n");
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
		retval = fw_reg_read(0x14);	/* Cycle Timer Register */
		printk("devctl:get_cycle=0x%x\n", retval);
		break;

	case SET_CYCLE_COUNTER:
		printk("devctl:set_cycle=0x%x\n", arg);
		fw_reg_write(0x14, arg);
		break;

	case SET_BUS_ID:
		printk(KERN_ERR "devctl command SET_BUS_ID err\n");
		break;

	case ACT_CYCLE_MASTER:
		printk("devctl:cycle_master=%d\n", arg);
		if ( arg ) {
			/* enable cycletimer and cyclemaster */
			fw_reg_write(0x8, fw_reg_read(0x8) | (1<<(31-20)) | (1<<(31-22)));
		}
		else {
			/* enable cycletimer and cyclemaster, cyclesource */
			fw_reg_write(0x8, fw_reg_read(0x8) & ~(1<<(31-20)));
		}
		break;

	case CANCEL_REQUESTS:
		printk(KERN_ERR "cancel requests\n");
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

static void handle_selfid(struct ti_ipod *ipod, struct hpsb_host *host,
				int phyid, int isroot)
{
	quadlet_t q0;
	size_t size;

/*	printk(KERN_ERR "phyid: 0x%x\n", phyid); */
/*	printk(KERN_ERR "isroot: %d\n", isroot); */

	size = fw_reg_read(0x9c) & 0x1ff;
/*	printk("size = %d\n", size); */

	while (size > 0) {
		q0 = fw_reg_read(0xac);		/* read from DRF */
		if ( (q0 & 0x80000000) == 0 ) {
/*			printk(KERN_ERR "ignored 0x%x received\n", q0); */
		}
		else {
			printk(KERN_ERR "SelfID packet 0x%x received\n", q0);
			hpsb_selfid_received(host, cpu_to_be32(q0));
			if (((q0 & 0x3f000000) >> 24) == phyid) {
				printk(KERN_ERR "SelfID for this node is 0x%08x\n", q0);
			}
		}

		size--;
	}

/*	printk(KERN_ERR "SelfID complete\n"); */

	hpsb_selfid_complete(host, phyid, isroot);
}

static void ipod_1394_fw_int(int irq, struct ti_ipod *ipod)
{
	struct hpsb_host *host = ipod->host;
	unsigned fw_src;

	/* interrupt source */
	fw_src = fw_reg_read(0xc);
	if ( fw_src ) {
		/* ack the interrupt */
		fw_reg_write(0xc, fw_src);

		if ( fw_src & BUS_RESET ) {
			if ( !host->in_bus_reset ) {
				printk(KERN_ERR "irq: bus reset requested\n");
				hpsb_bus_reset(host);
			}
		}
		if ( fw_src & END_SELF_ID ) {
			if ( host->in_bus_reset ) {
				quadlet_t bus_reset;

				if ( (fw_reg_read(0x8) & 0x80000000) == 0 ) {
					printk(KERN_ERR "ID not valid!\n");
					goto selfid_not_valid;
				}

				bus_reset = fw_reg_read(0x24);	/* Bus Reset Register */

				if ( bus_reset & 0xf000 ) {
					printk(KERN_ERR "bus reset err 0x%x\n", (bus_reset & 0xf000) >> 12);
				}
				else {
					int phyid = -1, isroot = 0;

					phyid =  (bus_reset & 0x3f0000) >> 16;

					isroot = (fw_reg_read(0x4) & 0x8000) != 0;	/* micellaneous register */

					handle_selfid(ipod, host, phyid, isroot);
				}
			}
			else {
				printk(KERN_ERR "irq: selfid outside bus reset\n");
			}
selfid_not_valid:
		}

		if ( fw_src & ATF_END ) {
			unsigned long flags;

			spin_lock_irqsave(&ipod->tx_lock, flags);
			ipod->tx_state = TX_END;
			tasklet_schedule(&ipod->tx_tasklet);
			spin_unlock_irqrestore(&ipod->tx_lock, flags);
		}

		if ( fw_src & ARF_RXD ) {
			unsigned long flags;

			spin_lock_irqsave(&ipod->tx_lock, flags);
			ipod->rx_state = RX_BUSY;
			tasklet_schedule(&ipod->tx_tasklet);
			spin_unlock_irqrestore(&ipod->tx_lock, flags);
		}
	}
}

static void ipod_1394_interrupt(int irq, void *dev_id, struct pt_regs *regs_are_unused)
{
	struct ti_ipod *ipod = (struct ti_ipod *)dev_id;
	unsigned source;

	/* get source of interupts */
	source = inb(0xcf000048);
	if ( source ) {
		outb(source, 0xcf000078);

		if ( source & (1<<4) ) {
			/* get current status port c */
			unsigned state = inb(0xcf000038);

			if ( state & (1<<4) ) {
				printk(KERN_ERR "fw pwr +\n");
			}
			else {
				printk(KERN_ERR "fw pwr -\n");
			}

			outb(~state, 0xcf000068);
		}

		if ( source & (1<<5) ) {
			ipod_1394_fw_int(irq, ipod);
		}

		if ( source & ~(1<<4 | 1<<5) ) {
			printk(KERN_ERR "src=0x%x\n", source);
		}
	}
}

/* needed for 3g only */
static __devinit void fw_i2c(int data0, int data1)
{
	outl(0x6a, 0xc0008000);
	mdelay(10);

	outl(0x0, 0xc0008000);
	outl(inl(0xc0008000) | (1<<1), 0xc0008000);

	outl(0x10, 0xc0008004);		/* address */

	outl(inl(0xc0008000) | (1<<5), 0xc0008000);

	outl(data0, 0xc000800c);
	outl(data1, 0xc0008010);

	outl(inl(0xc0008000) | (1<<7), 0xc0008000);	/* send cmd */
	mdelay(10);
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
	if ( ipod_hw_ver == 0x1 || ipod_hw_ver == 0x2 )
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
	if ( ipod_hw_ver == 0x1 )
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
	if ( ipod_hw_ver == 0x2 )
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
		/* Port E Bit 2 output high */
		outl(inl(0xcf004048) | (1<<4), 0xcf004048);

		udelay(0x14);

		outl(r2 | (1<<4), 0xcf004044);
	}

	/* this is 3g */
	if ( ipod_hw_ver == 0x3 )
	{
		/* reset device i2c */
		outl(inl(0xcf005030) | (1<<8), 0xcf005030);
		outl(inl(0xcf005030) & ~(1<<8), 0xcf005030);

		/* some i2c magic */
		fw_i2c(0x39, 0);
		fw_i2c(0x3a, 0);
		fw_i2c(0x3b, 0);
		fw_i2c(0x3c, 0);
		fw_i2c(0x39, 7);
		fw_i2c(0x3a, 0);
		fw_i2c(0x3b, 7);
		fw_i2c(0x3c, 7);
		fw_i2c(0x3b, 0);
		fw_i2c(0x3c, 0);

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

	if ( fw_reg_read(0x0) != 0x43008203 ) {
		printk(KERN_ERR "ipod_1394: invalid chip revsion 0x%x\n", fw_reg_read(0x0));
	}
}

static int __devinit ipod_1394_init(void)
{
	struct ti_ipod *ipod;

	printk("ipod_1394: $Id: tsb42aa82.c,v 1.6 2004/02/10 21:24:48 leachbj Exp $\n");

	ipod_host = hpsb_alloc_host(&ipod_1394_driver, sizeof(struct ti_ipod));
	if ( !ipod_host ) {
		printk(KERN_ERR "ipod_1394: failed to allocate control structure\n");
		return -ENOMEM;
	}

	ipod = ipod_host->hostdata;
	ipod->host = ipod_host;
	ipod->csr_config_rom_cpu = kmalloc(CONFIG_ROM_LEN, GFP_KERNEL);
	if ( ipod->csr_config_rom_cpu == NULL ) {
		printk(KERN_ERR "ipod_1394: Failed to allocate buffer config rom");
	}

	spin_lock_init(&ipod->phy_reg_lock);
	spin_lock_init(&ipod->tx_lock);
	spin_lock_init(&ipod->tx_list_lock);
	INIT_LIST_HEAD(&ipod->tx_list);

	tasklet_init(&ipod->tx_tasklet, tx_tasklet, (unsigned long)ipod);

	if ( request_irq(GPIO_IRQ, ipod_1394_interrupt, SA_SHIRQ, IPOD_1394_DRIVER_NAME, ipod) ) {
		printk(KERN_ERR "ipod_1394: IRQ %d failed\n", GPIO_IRQ);
	}

	ipod_1394_hw_init();

	/* setup firewire power (4) and firewire (5) interrupts */

	/* enable port c bits 4 & 5 */
	outb(inb(0xcf000008) | (1<<4) | (1<<5), 0xcf000008);

	/* pin 4 & 5 is input */
	outb(inl(0xcf000018) & ~((1<<4) | (1<<5)), 0xcf000018);

	outb(~inb(0xcf000038), 0xcf000068);
	outb(inb(0xcf000068) & ~(1<<5), 0xcf000068);

	outb(inb(0xcf000048), 0xcf000078);
	outb((1<<4)|(1<<5), 0xcf000058);

	/* set CRF size to 0x0 (we dont use it) */
	fw_reg_write(0x40, 0x1000);
	/* set CTQ size to 0x0 (we dont use it) */
	fw_reg_write(0x3c, 0x1000);
	/* set ARF size to 0xb4 */
	/* clear the ARF */
	fw_reg_write(0x30, 0x10b4);
	/* set ATF size to 0xb4 */
	/* clear the ATF */
	fw_reg_write(0x2c, 0x10b4);

	/*
	 * ATF default size is 0x80		(0x2c)
	 * ARF default size is 0x8e		(0x30)
	 * CTQ default size is 0xf		(0x3c)
	 * CRF default size is 0x4b (min 0x44) (0x40)
	 * MTQ fixed size 3
	 * MRF fixed size 15
	 */

	/* rxsld,rsisel,tren,reset tr,split transaction enable, */
	/* auto retry,cycle master,cyle timer enable,dma block clear*/
	fw_reg_write(CONTROL_REG, (0x6420cb00 | (1<<(31-18))|(1<<(31-24))) & ~(1<(31-25)));
	/* finish dma clear */
	fw_reg_write(CONTROL_REG, (0x6420ca00 | (1<<(31-18))|(1<<(31-24))) & ~(1<(31-25)));

	/* DMA control
	 * 00111100001000000110110010000011
	 * 2 3 4 5 10 17 18 20 21 24 30 31
	 * DRF enable
	 * DTF enable
	 * DRF packetizer enable
	 * DTF packetizer enable
	 * Recieve confirm for each packet
	 * Data read fetchsize == 011
	 * Data transmit fetchsize == 001
	 * DTF header insert
	 * DRF clear
	 * DTF clear
	 */
	fw_reg_write(0x90, 0x3c206c83);

	fw_reg_write(0xc0, 0x0);

	init_config_rom(ipod);

#define INTERRUPT_MASK_REG	0x10
	/* acknowledge all interrupts */
	fw_reg_write(0xc, fw_reg_read(0xc));

	/* unmask the interrupts */
	fw_reg_write(INTERRUPT_MASK_REG, BUS_RESET|END_SELF_ID|ATF_END|ARF_RXD);

	hpsb_add_host(ipod_host);

	return 0;
}

static void __exit ipod_1394_exit(void)
{
	struct ti_ipod *ipod = ipod_host->hostdata;

	hpsb_remove_host(ipod->host);

	/* free the IRQ */
	free_irq(GPIO_IRQ, ipod);

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

