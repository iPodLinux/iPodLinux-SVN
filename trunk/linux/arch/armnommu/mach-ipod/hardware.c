/*
 * hardware.c - special hardware routines for iPod
 *
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/string.h>
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

EXPORT_SYMBOL(ipod_get_hw_version);
EXPORT_SYMBOL(ipod_get_sysinfo);

