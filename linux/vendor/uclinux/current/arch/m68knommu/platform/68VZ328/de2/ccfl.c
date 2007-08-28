/*
 *  linux/arch/m68knommu/platform/MC68VZ328/de2/ccfl.c
 *
 *  Copyright (C) 2003 Georges Menie, Kurt Stremerch
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/init.h>
#include <asm/MC68VZ328.h>

#define SmbSCL 0x10
#define SmbSDA 0x20
#define SmbDirReg PKDIR
#define SmbDataReg PKDATA

#define setbb(d,r) (r) |= (d)
#define clrbb(d,r) (r) &= ~(d)
#define readb(r) (r)

static void __init delay(int d)
{
	int i;

	for (i = 0; i < d * 20; i++);
}

static void __init smb_scl_high(void)
{
	setbb(SmbSCL, SmbDataReg);
}

static void __init smb_scl_low(void)
{
	clrbb(SmbSCL, SmbDataReg);
}

static void __init smb_sda_high(void)
{
	setbb(SmbSDA, SmbDataReg);
}

static void __init smb_sda_low(void)
{
	clrbb(SmbSDA, SmbDataReg);
}

static void __init smb_scl_out(void)
{
	setbb(SmbSCL, SmbDirReg);
}

static void __init smb_sda_out(void)
{
	setbb(SmbSDA, SmbDirReg);
}

static void __init smb_sda_in(void)
{
	clrbb(SmbSDA, SmbDirReg);
}

static int __init smb_sda_read(void)
{
	return (readb(SmbDataReg) & SmbSDA);
}

static void __init smb_start(void)
{
	smb_scl_out();
	smb_sda_out();
	smb_scl_high();
	smb_sda_high();
	delay(10);
	smb_sda_low();
	delay(3);
	smb_scl_low();
	delay(7);
}

static void __init smb_stop(void)
{
	smb_scl_high();
	delay(3);
	smb_sda_high();
	delay(7);
}

static int __init smb_out(unsigned char byte)
{
	int i, ret;

	for (i = 0; i < 8; i++) {
		if (byte & (1 << (7 - i)))
			smb_sda_high();
		else
			smb_sda_low();
		smb_scl_high();
		delay(10);
		smb_scl_low();
		delay(10);
	}
	smb_sda_in();
	smb_scl_high();
	delay(10);
	ret = smb_sda_read();
	smb_scl_low();
	delay(10);
	smb_sda_out();
	smb_sda_low();

	return ret;
}

static int __init smb_write(unsigned char adr, unsigned char reg,
							unsigned char val)
{
	int ret;

	adr &= 0xfe;
	smb_start();
	ret = smb_out(adr);
	if (ret == 0) {
		ret = smb_out(reg);
		if (ret == 0) {
			ret = smb_out(val);
		}
	}
	smb_stop();

	return ret;
}

int __init SetCCFLCurrent(int percent)
{
	unsigned char Address = 0x2C;	/* 7-bit wide */
	unsigned char Command = 0x7F;	/* byte wide */
	unsigned char Data;			/* byte wide */

	if (percent <= 0)
		percent = 0;
	if (percent > 100)
		percent = 100;
	Data = (percent * 100) / 109;

	return smb_write(Address << 1, Command, Data);
}
