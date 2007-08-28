/****************************************************************************/
/*
 *  linux/drivers/ide/h8300/aki3068_ide.c
 *  aki3068net IDE I/F support
 *  reference schematic is http://www.linet.gr.jp/mituiwa/h8/h8osv3/hddprog/hdd.png
 *
 *  Copyright (C) 2003 Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 *
 */
/****************************************************************************/


#include <linux/config.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <asm/irq.h>
#include <asm/regs306x.h>

/* IDE I/F configuration */
#define IDE_BASE 0x600000
#define IDE_CTRL 0x600206
#define IDE_IRQ  EXT_IRQ4

#define IDE_DATA	0x00
#define IDE_ERROR	0x01
#define IDE_NSECTOR	0x02
#define IDE_SECTOR	0x03
#define IDE_LCYL	0x04
#define IDE_HCYL	0x05
#define IDE_SELECT	0x06
#define IDE_STATUS	0x07

const static int offsets[IDE_NR_PORTS] = {
    IDE_DATA, IDE_ERROR,  IDE_NSECTOR, IDE_SECTOR, IDE_LCYL,
    IDE_HCYL, IDE_SELECT, IDE_STATUS,          -1,       -1
};

static void _outb(u8 d, unsigned long a)
{
	*(unsigned char *)a = (d & 0xff);
}

static void _outbsync(ide_drive_t *drive, u8 d, unsigned long a)
{
	*(unsigned char *)a = (d & 0xff);
}

static u8 _inb(unsigned long a)
{
	return *(unsigned char *)a;
}

static void _outw(u16 d, unsigned long a)
{
	*(unsigned short *)a = d;
}

static u16 _inw(unsigned long a)
{
	return *(unsigned short *)a;
}

static void _outl(u32 d, unsigned long a)
{
}

static u32 _inl(unsigned long a)
{
	return 0xffffffffUL;
}

static void _outsw(unsigned long addr, void *buf, u32 len)
{
	unsigned volatile short *ap = (unsigned volatile short *)addr;
	unsigned short *bp = (unsigned short *)buf;
	while(len--) {
		*ap = *bp++;
	}
}

static void _insw(unsigned long addr, void *buf, u32 len)
{
	unsigned volatile short *ap = (unsigned volatile short *)addr;
	unsigned short *bp = (unsigned short *)buf;
	while(len--) {
		*bp++ = *ap;
	}
}

void __init h8300_ide_init(void)
{
	hw_regs_t hw;
	ide_hwif_t *hwif;

	*(volatile unsigned char *)ABWCR |= (1<<3);
	memset(&hw,0,sizeof(hw));
	ide_setup_ports(&hw, IDE_BASE, (int *)offsets,
                             IDE_CTRL, 0, NULL,IDE_IRQ);
	if (ide_register_hw(&hw, &hwif) != -1) {
		hwif->mmio  = 0;
		hwif->OUTB  = _outb;
		hwif->OUTBSYNC = _outbsync;
		hwif->OUTW  = _outw;
		hwif->OUTL  = _outl;
		hwif->OUTSW = _outsw;
		hwif->OUTSL = NULL;
		hwif->INB   = _inb;
		hwif->INW   = _inw;
		hwif->INL   = _inl;
		hwif->INSW  = _insw;
		hwif->INSL  = NULL;
	}
}

void __init ide_print_resource(char *name, hw_regs_t *hw)
{
	printk("%s at 0x%08x-0x%08x,0x%08x on irq %d", name,
		(unsigned int)hw->io_ports[IDE_DATA_OFFSET],
		(unsigned int)hw->io_ports[IDE_DATA_OFFSET]+7,
		(unsigned int)hw->io_ports[IDE_CONTROL_OFFSET],
		hw->irq);
}
