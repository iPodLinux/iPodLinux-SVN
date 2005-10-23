/*
 * hardware.c - special hardware routines for iPod
 *
 * Copyright (c) 2003-2005 Bernard Leach <leachbj@bouncycastle.org>
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/system.h>

static struct sysinfo_t ipod_sys_info;
static int ipod_sys_info_set;

void ipod_set_sys_info(void);

/*
 * nano  0x000C0005
 * mini2 0x00070002
 * ipod photo 0x60000
 * 4th generation 0x50000
 * ipod mini 0x40000
 * 3rd generation (docking) 0x30000, 0x30001
 * 2nd generation (touch wheel) 0x20000, 0x20001
 * 1st generation (scroll wheel) iPods 0x10000, 0x10001, 0x10002
 * failed 0x0
 */
unsigned ipod_get_hw_version(void)
{
	if (!ipod_sys_info_set) {
		ipod_set_sys_info();
	}

	if (ipod_sys_info_set > 0) {
		return system_rev;
	}

	return 0x0;
}

struct sysinfo_t *ipod_get_sysinfo(void)
{
	if (!ipod_sys_info_set) {
		ipod_set_sys_info();
	}

	if (ipod_sys_info_set > 0) {
		return &ipod_sys_info;
	}

	return 0x0;
}

int ipod_is_pp5022(void) {
	return (inl(0x70000000) << 8) >> 24 == '2';
}

void ipod_set_sys_info(void)
{
	if (!ipod_sys_info_set) {
		unsigned sysinfo_tag = SYSINFO_TAG;
		struct sysinfo_t ** sysinfo_ptr = SYSINFO_PTR;

		if (ipod_is_pp5022()) {
			sysinfo_tag = SYSINFO_TAG_PP5022;
			sysinfo_ptr = SYSINFO_PTR_PP5022;
		}

		if (*(unsigned *)sysinfo_tag == *(unsigned *)"IsyS" 
				&& (*(struct sysinfo_t **)sysinfo_ptr)->IsyS ==  *(unsigned *)"IsyS" ) {
			memcpy(&ipod_sys_info, *sysinfo_ptr, sizeof(struct sysinfo_t));
			ipod_sys_info_set = 1;
			/* magic length based on newer ipod nano sysinfo */
			if (ipod_sys_info.len == 0xf8) {
				system_rev = ipod_sys_info.sdram_zero2;
			} else {
				system_rev = ipod_sys_info.boardHwSwInterfaceRev;
			}
		}
		else {
			ipod_sys_info_set = -1;
		}
	}
}

void ipod_hard_reset(void)
{
	int hw_ver = ipod_get_hw_version() >> 16;
	if (hw_ver > 0x3) {
		outl(inl(0x60006004) | 0x4, 0x60006004);
	}
	else {
		outl(inl(0xcf005030) | 0x4, 0xcf005030);
	}
}

void
ipod_init_cache(void)
{
	unsigned i;
	int hw_ver;

	hw_ver = ipod_get_hw_version() >> 16;
	if (hw_ver > 0x3) {
		/* cache init mode? */
		outl(0x4, 0x6000C000);

		/* PP5002 has 8KB cache */
		for (i = 0xf0004000; i < 0xf0006000; i += 16) {
			outl(0x0, i);
		}

		outl(0x0, 0xf000f040);
		outl(0x3fc0, 0xf000f044);

		/* enable cache */
		outl(0x1, 0x6000C000);

		for (i = 0x10000000; i < 0x10002000; i += 16) {
			inb(i);
		}
	}
	else {
		outl(inl(0xcf004050) & ~0x700, 0xcf004050);
		outl(0x4000, 0xcf004020);

		outl(0x2, 0xcf004024);

		/* PP5002 has 8KB cache */
		for (i = 0xf0004000; i < 0xf0006000; i += 16) {
			outl(0x0, i);
		}

		outl(0x0, 0xf000f020);
		outl(0x3fc0, 0xf000f024);

		outl(0x3, 0xcf004024);
	}
}

void
ipod_set_cpu_speed(void)
{
	if ((ipod_get_hw_version() >> 16) > 0x3) {
		outl(inl(0x70000020) | (1<<30), 0x70000020);

		/* Set run state to 24MHz */
		outl((inl(0x60006020) & 0x0fffff0f) | 0x20000020, 0x60006020);

		/* 75 MHz (24/8)*25 */
		outl(0xaa021908, 0x60006034);
		udelay(2000);

		outl((inl(0x60006020) & 0x0fffff0f) | 0x20000070, 0x60006020);
	} else {
		outl(0x02, 0xcf005008);

		outl(0x55, 0xcf00500c);
		outl(0x6000, 0xcf005010);

#if 1
		// 75  MHz (24/24 * 75) (default)
		outl(24, 0xcf005018);
		outl(75, 0xcf00501c);
#endif

#if 0
		// 66 MHz (24/3 * 8)
		outl(3, 0xcf005018);
		outl(8, 0xcf00501c);
#endif

		outl(0xe000, 0xcf005010);

		udelay(2000);

		outl(0xa8, 0xcf00500c);
	}
}

#define IPOD_I2C_CTRL	(ipod_i2c_base+0x00)
#define IPOD_I2C_ADDR	(ipod_i2c_base+0x04)
#define IPOD_I2C_DATA0	(ipod_i2c_base+0x0c)
#define IPOD_I2C_DATA1	(ipod_i2c_base+0x10)
#define IPOD_I2C_DATA2	(ipod_i2c_base+0x14)
#define IPOD_I2C_DATA3	(ipod_i2c_base+0x18)
#define IPOD_I2C_STATUS	(ipod_i2c_base+0x1c)

/* IPOD_I2C_CTRL bit definitions */
#define IPOD_I2C_SEND	0x80

/* IPOD_I2C_STATUS bit definitions */
#define IPOD_I2C_BUSY	(1<<6)

#define POLL_TIMEOUT (HZ)

static unsigned ipod_i2c_base;

static int
ipod_i2c_wait_not_busy(void)
{
	unsigned long timeout;

	timeout = jiffies + POLL_TIMEOUT;
	while (time_before(jiffies, timeout)) {
		if (!(inb(IPOD_I2C_STATUS) & IPOD_I2C_BUSY)) {
			return 0;
		}
		yield();
	}

	return -ETIMEDOUT;
}

void
ipod_i2c_init(void)
{
	static int i2c_init = 0;

	if (i2c_init == 0) {
		int ipod_hw_ver;

		i2c_init = 1;

		ipod_hw_ver = ipod_get_hw_version() >> 16;

		/* reset I2C */
		if (ipod_hw_ver > 0x3) {
			ipod_i2c_base = 0x7000c000;

			if (ipod_hw_ver == 0x4) {
				/* GPIO port C disable port 0x10 */
				outl(inl(0x6000d008) & ~0x10, 0x6000d008);

				/* GPIO port C disable port 0x20 */
				outl(inl(0x6000d008) & ~0x20, 0x6000d008);
			}

			outl(inl(0x6000600c) | 0x1000, 0x6000600c);	/* enable 12 */

			outl(inl(0x60006004) | 0x1000, 0x60006004);	/* start reset 12 */
			outl(inl(0x60006004) & ~0x1000, 0x60006004);	/* end reset 12 */

			outl(0x0, 0x600060a4);
			outl(0x80 | (0 << 8), 0x600060a4);

				i2c_readbyte(0x8, 0);
		} else {
			ipod_i2c_base = 0xc0008000;

			outl(inl(0xcf005000) | 0x2, 0xcf005000);

			outl(inl(0xcf005030) | (1<<8), 0xcf005030);
			outl(inl(0xcf005030) & ~(1<<8), 0xcf005030);
		}
	}
}

int
ipod_i2c_read_byte(unsigned int addr, unsigned int *data)
{
	if (ipod_i2c_wait_not_busy() < 0) {
		return -ETIMEDOUT;
	}

	// clear top 15 bits, left shift 1, or in 0x1 for a read
	outb(((addr << 17) >> 16) | 0x1, IPOD_I2C_ADDR);

	outb(inb(IPOD_I2C_CTRL) | 0x20, IPOD_I2C_CTRL);

	outb(inb(IPOD_I2C_CTRL) | IPOD_I2C_SEND, IPOD_I2C_CTRL);

	if (ipod_i2c_wait_not_busy() < 0) {
		return -ETIMEDOUT;
	}

	if (data) {
		*data = inb(IPOD_I2C_DATA0);
	}

	return 0;
}

int
ipod_i2c_send_bytes(unsigned int addr, unsigned int len, unsigned char *data)
{
	int data_addr;
	int i;

	if (len < 1 || len > 4) {
		return -EINVAL;
	}

	if (ipod_i2c_wait_not_busy() < 0) {
		return -ETIMEDOUT;
	}

	// clear top 15 bits, left shift 1
	outb((addr << 17) >> 16, IPOD_I2C_ADDR);

	outb(inb(IPOD_I2C_CTRL) & ~0x20, IPOD_I2C_CTRL);

	data_addr = IPOD_I2C_DATA0;
	for ( i = 0; i < len; i++ ) {
		outb(*data++, data_addr);

		data_addr += 4;
	}

	outb((inb(IPOD_I2C_CTRL) & ~0x26) | ((len-1) << 1), IPOD_I2C_CTRL);

	outb(inb(IPOD_I2C_CTRL) | IPOD_I2C_SEND, IPOD_I2C_CTRL);

	return 0x0;
}

int
ipod_i2c_send(unsigned int addr, int data0, int data1)
{
	unsigned char data[2];
											
	data[0] = data0;
	data[1] = data1;

	return ipod_i2c_send_bytes(addr, 2, data);
}

int
ipod_i2c_send_byte(unsigned int addr, int data0)
{
	unsigned char data[1];

	data[0] = data0;

	return ipod_i2c_send_bytes(addr, 1, data);
}

int
i2c_readbyte(unsigned int dev_addr, int addr)
{
	int data;

	ipod_i2c_send_byte(dev_addr, addr);
	ipod_i2c_read_byte(dev_addr, &data);

	return data;
}

void
ipod_serial_init(void)
{
	int hw_ver = ipod_get_hw_version() >> 16;

	/* enable ttyS1 (piezo) */
	if (hw_ver <= 0x3) {
		outl(inl(0xcf004040) & ~(1<<12), 0xcf004040);
	}
	if (hw_ver == 0x3) {
		/* port c, bit 0 and 3 disabled */
		outl(inl(0xcf00000c) & ~((1<<0)|(1<<3)), 0xcf00000c);
	}

	if (hw_ver == 0x3) {
		/* 100100010001, 11,8,4,0 */
		// outl(0x911, 0xcf004040);
		/* 1000000000011100, 16,4,3,2 */
		// outl(0x801c, 0xcf004048);

		outl(inl(0xcf004040) | (1<<8) | (1<<4), 0xcf004040);
		outl(0x801c, 0xcf004048);
	}
	else {
		/* 011110010001, 10,9,8,7,4,0 */
		// outl(0x791, 0xcf004040);
	}

	/* enable ttyS0 (remote) */
	if (hw_ver < 0x3) {
		outl(inl(0xcf00000c) & ~0x8, 0xcf00000c);
	}

	if (hw_ver >= 0x3) {
		ipod_i2c_init();

		/* send some commands to the PCF */
		/* to power up the 3.3V pin + ?? */

		/* 4g diag this this (mini diag doesnt) */
		/* ipod_i2c_send(0x8, 0x38, 0x0); */

		ipod_i2c_send(0x8, 0x24, 0xf5);
		ipod_i2c_send(0x8, 0x25, 0xf8);

		/* 4g diag doesnt have this (mini diag does) */
		ipod_i2c_send(0x8, 0x26, 0xf5);
	}

	/* 3g ttyS0 init */
	if (hw_ver == 0x3) {
		ipod_i2c_send(0x8, 0x34, 0x02);
		ipod_i2c_send(0x8, 0x1b, 0xf9);

		outl(inl(0xcf004044) | (1<<4), 0xcf004044);
		outl(inl(0xcf004048) & ~(1<<4), 0xcf004048);

		/* port c bit 3 output 1 */
		outl(inl(0xcf000008) | (1<<3), 0xcf000008);
		outl(inl(0xcf000018) | (1<<3), 0xcf000018);
		outl(inl(0xcf000028) | (1<<3), 0xcf000028);

		// rmt_uart_init
		outl(inl(0xcf00000c) & ~0x1, 0xcf00000c);
		outl(inl(0xcf00000c) & ~0x8, 0xcf00000c);
	}

	if (hw_ver > 0x3) {
		/* 4g diag for ser1 */
		outl(inl(0x70000014) & ~0xF00, 0x70000014);
		outl(inl(0x70000014) | 0xA00, 0x70000014);

		/* 4g diag for ser0 */
		outl(inl(0x70000018) & ~0xC00, 0x70000018);

		/* where did this come from? */
		// outl(inl(0x70000020) & ~0x200, 0x70000020);

		/* port c bit 3 output 1 */
		outl(inl(0x6000d008) | (1<<3), 0x6000d008);
		outl(inl(0x6000d018) | (1<<3), 0x6000d018);
		outl(inl(0x6000d028) | (1<<3), 0x6000d028);

		/* enable ser0 */
		outl(inl(0x6000600c) | 0x40, 0x6000600c);

		/* reset ser0 */
		outl(inl(0x60006004) | 0x40, 0x60006004);
		outl(inl(0x60006004) & ~0x40, 0x60006004);

		/* enable ser1 */
		outl(inl(0x6000600c) | 0x80, 0x6000600c);

		/* reset ser1 */
		outl(inl(0x60006004) | 0x80, 0x60006004);
		outl(inl(0x60006004) & ~0x80, 0x60006004);
	}
}

/* put our ptr in on chip ram to avoid caching problems */
static ipod_dma_handler_t * ipod_dma_handler = DMA_HANDLER;

static ipod_cop_handler_t * ipod_cop_handler = COP_HANDLER;


void ipod_handle_cop(void)
{
	if (*ipod_cop_handler != 0) {
		(*ipod_cop_handler)();
	}
}

void ipod_set_handle_cop(ipod_cop_handler_t new_handler)
{
	*ipod_cop_handler = new_handler;
}


void ipod_set_process_dma(ipod_dma_handler_t new_handler)
{
	*ipod_dma_handler = new_handler;
}

void ipod_handle_dma(void)
{
	if (*ipod_dma_handler != 0) {
		(*ipod_dma_handler)();
	}
}

EXPORT_SYMBOL(ipod_get_hw_version);
EXPORT_SYMBOL(ipod_get_sysinfo);
EXPORT_SYMBOL(ipod_is_pp5022);
EXPORT_SYMBOL(ipod_i2c_init);
EXPORT_SYMBOL(ipod_i2c_send_bytes);
EXPORT_SYMBOL(ipod_i2c_send);
EXPORT_SYMBOL(ipod_i2c_send_byte);
EXPORT_SYMBOL(ipod_serial_init);
EXPORT_SYMBOL(ipod_set_process_dma);
EXPORT_SYMBOL(ipod_set_handle_cop);

