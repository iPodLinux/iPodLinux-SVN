/*
 * hardware.c - special hardware routines for iPod
 *
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
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

#define SYSINFO_TAG     (unsigned char *)0x40017f18
#define SYSINFO_PTR     (struct sysinfo_t **)0x40017f1c

static struct sysinfo_t ipod_sys_info;
static int ipod_sys_info_set;

void ipod_set_sys_info(void);

/*
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
		return ipod_sys_info.boardHwSwInterfaceRev;
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

void ipod_set_sys_info(void)
{
	if (!ipod_sys_info_set) {
		if ( *(unsigned *)SYSINFO_TAG == *(unsigned *)"IsyS" 
				&& (*SYSINFO_PTR)->IsyS ==  *(unsigned *)"IsyS" ) {
			memcpy(&ipod_sys_info, *SYSINFO_PTR, sizeof(struct sysinfo_t));
			ipod_sys_info_set = 1;
		}
		else {
			ipod_sys_info_set = -1;
		}
	}
}

void ipod_hard_reset(void)
{
	outl(inl(0xcf005030) | 0x4, 0xcf005030);
}

/* make startup piezo noise */
void ipod_startup_noise(void)
{
	int i1, i2;

	outl(0x80, 0xc000604c);

	/* set divisor low word */
	outl(0xd, 0xc0006040);

	outl(0x3, 0xc000604c);

	outl(0x3, 0xc0006050);
	outl(0x7, 0xc0006048);

	for ( i1 = 0; i1 < 500; i1++ ) {
		outl(0x0, 0xc0006040);

		/* delay for a bit */
		for ( i2 = 0; i2 < i1; i2++ ) {
			/* empty */
		}
	}
}

/* wait for action button to be pressed and then released */
void
wait_for_action(void)
{
	/* wait for press */
	do {
		inl(0xcf000030);
	} while ( (inl(0xcf000030) & 0x2) != 0 );

	/* wait for release */
	do {
		inl(0xcf000030);
	} while ( (inl(0xcf000030) & 0x2) == 0 );
}

void
ipod_init_cache(void)
{
	unsigned i;

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

void
ipod_set_cpu_speed(void)
{
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

static void
set_boot_variable(unsigned *dest, char *src, unsigned len)
{
	if ( !(((unsigned)dest | (unsigned)src) & 0x3 ) ) {

		unsigned lr = 0x01010101;

		unsigned ip;
		unsigned t;

		do {
			if ( len < 4 ) break;

			ip = *(unsigned *)src;
			*dest = ip;

			dest ++;
			src += 4;
			len -= 4;

			/* this magic checks for embedded nulls */
			t = ip - lr;
			t = t & ~ip;
		} while ( (t & (lr << 7)) == 0 );
	}

	if ( len == 0 ) return;

	{
		char ip;

		do {
			ip = *src++;
			*((char *)dest)++ = ip;
			if ( ip == 0 ) break;
			len--;
		} while ( len != 0 );

		if ( len <= 1 ) return;
	}

	len --;

	do {
		*((unsigned char *)dest)++ = '\0';
		len--;
	} while ( len != 0 );
}

void
ipod_reboot_to_diskmode(void)
{
	/* select new mode and icon to show during boot */
	set_boot_variable((unsigned *)0x40017f00, "diskmode", 8);
	set_boot_variable((unsigned *)0x40017f08, "hotstuff", 8);

	/* do special startup? */
	outl(0x1, 0x40017f10);

	/* reset */
	outl(inl(0xcf005030) | 0x4, 0xcf005030);
}

#define IPOD_I2C_CTRL	0xc0008000
#define IPOD_I2C_ADDR	0xc0008004
#define IPOD_I2C_DATA0	0xc000800c
#define IPOD_I2C_DATA1	0xc0008010
#define IPOD_I2C_DATA2	0xc0008014
#define IPOD_I2C_DATA3	0xc0008018
#define IPOD_I2C_STATUS	0xc000801c

/* IPOD_I2C_CTRL bit definitions */
#define IPOD_I2C_SEND	0x80

/* IPOD_I2C_STATUS bit definitions */
#define IPOD_I2C_BUSY	(1<<6)

#define POLL_TIMEOUT (HZ)

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

void
ipod_serial_init(void)
{
	int hw_ver = ipod_get_hw_version() >> 16;

	/* enable ttyS1 (piezo) */
	outl(inl(0xcf004040) & ~0x1000, 0xcf004040);
	if (hw_ver == 0x3) {
		/* port c, bit 0 and 3 disabled */
		outl(inl(0xcf00000c) & ~((1<<0)|(1<<3)), 0xcf00000c);
	}

	if (hw_ver == 0x3) {
		/* 100100010001, 11,8,4,0 */
		// outl(0x911, 0xcf004040);
		/* 1000000000011100, 16,4,3,2 */
		// outl(0x801c, 0xcf004048);

		outl((inl(0xcf004040) & ~(1<<10)) | (1<<11) | (1<<8) | (1<<4), 0xcf004040);
		outl(0x801c, 0xcf004048);
	}
	else {
		/* 011110010001, 10,9,8,7,4,0 */
		// outl(0x791, 0xcf004040);
	}

	/* enable ttyS0 (remote) */
	if (hw_ver == 0x3) {
#if 0
		outl(inl(0xcf00000c) | 0x10, 0xcf00000c);
		outl(inl(0xcf00001c) & ~0x10, 0xcf00001c);

		// sub_0_28001CE4(0)
		outl(inl(0xcf004048) & ~0x8, 0xcf004048);

		// sub_0_28001C9C(0)
		outl(inl(0xcf00000c) | 0x20, 0xcf00000c);
		outl(inl(0xcf00001c) | 0x20, 0xcf00001c);
		outl(inl(0xcf00002c) & ~0x20, 0xcf00001c);

		// sub_0_28001C58(0)
		outl(inl(0xcf004040) | 0x400, 0xcf004040);
		outl(inl(0xcf004040) & ~0x800, 0xcf004040);
		outl(inl(0xcf004048) & ~0x2, 0xcf004048);

		// sub_0_28001C58(1)
		outl(inl(0xcf004040) | 0x400, 0xcf004040);
		outl(inl(0xcf004040) & ~0x800, 0xcf004040);
		outl(inl(0xcf004048) | 0x2, 0xcf004048);

		// sub_0_28001C10(0)
		outl(inl(0xcf000008) | 0x40, 0xcf000008);
		outl(inl(0xcf000018) | 0x40, 0xcf000018);
		outl(inl(0xcf000028) & ~0x40, 0xcf000028);

		udelay(1); /* Hold reset for at least 1000ns */

		// sub_0_28001C10(1)
		outl(inl(0xcf000008) | 0x40, 0xcf000008);
		outl(inl(0xcf000018) | 0x40, 0xcf000018);
		outl(inl(0xcf000028) | 0x40, 0xcf000028);

		// sub_0_2800AB2C
		outl(inl(0xcf005030) | 0x100, 0xcf005030);
		outl(inl(0xcf005030) & ~0x100, 0xcf005030);

		outl(0x1, 0xc0008020);
		outl(0x0, 0xc0008030);
		outl(0x51, 0xc000802c);
		outl(0x0, 0xc000803c);

		outl(inl(0xcf005000) | 0x2, 0xcf005000);

		outl(inl(0xc000251c) | 0x10000, 0xc000251c);
		outl(inl(0xc000251c) & ~0x10000, 0xc000251c);
		outl(inl(0xc000251c) | 0x30000, 0xc000251c);
		outl(0x15, 0xc0002500);

		outl(inl(0xcf00000c) | 0x40, 0xcf00000c);
		outl(inl(0xcf00001c) & ~0x40, 0xcf00000c);

		outl(0x7, 0xc0008034);
		outl(0x0, 0xc0008038);
		// end sub_0_2800AB2C
#endif
	}
	else {
		outl(inl(0xcf00000c) & ~0x8, 0xcf00000c);
	}

	/* 3g ttyS0 init */
	if (hw_ver == 0x3) {
		ipod_i2c_send(0x8, 0x24, 0xf5);
		ipod_i2c_send(0x8, 0x25, 0xf8);
		ipod_i2c_send(0x8, 0x26, 0xf5);
		ipod_i2c_send(0x8, 0x34, 0x02);
		ipod_i2c_send(0x8, 0x1b, 0xf9);

		// sub_0_2800147C(0)
		outl(inl(0xcf004044) | (1<<4), 0xcf004044);
		outl(inl(0xcf004048) & ~(1<<4), 0xcf004048);

		/* port c bit 3 output 1 */
		// sub_0_28001434(1)
		outl(inl(0xcf000008) | (1<<3), 0xcf000008);
		outl(inl(0xcf000018) | (1<<3), 0xcf000018);
		outl(inl(0xcf000028) | (1<<3), 0xcf000028);

		// rmt_uart_init
		outl(inl(0xcf00000c) & ~0x1, 0xcf00000c);
		outl(inl(0xcf00000c) & ~0x8, 0xcf00000c);
	}
}

EXPORT_SYMBOL(ipod_get_hw_version);
EXPORT_SYMBOL(ipod_get_sysinfo);
EXPORT_SYMBOL(ipod_i2c_send_bytes);
EXPORT_SYMBOL(ipod_i2c_send);
EXPORT_SYMBOL(ipod_serial_init);

