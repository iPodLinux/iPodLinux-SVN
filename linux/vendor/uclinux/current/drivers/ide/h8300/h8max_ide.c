/****************************************************************************/
/*
 *  linux/drivers/ide/h8300/h8max_ide.c
 *  H8MAX IDE Interface support
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

/* IDE I/F configuration */
#define IDE_BASE 0x200000
#define IDE_CTRL 0x60000c
#define IDE_IRQ  EXT_IRQ5

#define IDE_DATA	0x00
#define IDE_ERROR	0x02
#define IDE_NSECTOR	0x04
#define IDE_SECTOR	0x06
#define IDE_LCYL	0x08
#define IDE_HCYL	0x0A
#define IDE_SELECT	0x0C
#define IDE_STATUS	0x0E

const static int offsets[IDE_NR_PORTS] = {
    IDE_DATA, IDE_ERROR,  IDE_NSECTOR, IDE_SECTOR, IDE_LCYL,
    IDE_HCYL, IDE_SELECT, IDE_STATUS,          -1,       -1
};

static void h8max_outb(u8 d, unsigned long a)
{
	*(unsigned short *)a = (d & 0xff);
}

static void h8max_outbsync(ide_drive_t *drive, u8 d, unsigned long a)
{
	*(unsigned short *)a = (d & 0xff);
}

static u8 h8max_inb(unsigned long a)
{
	return (*(unsigned short *)a) & 0xff;
}

static void h8max_outw(u16 d, unsigned long a)
{
	*(unsigned short *)a = ((d << 8) & 0xff00) | ((d >> 8) & 0x00ff);
}

static u16 h8max_inw(unsigned long a)
{
	unsigned short d;
	d = *(unsigned short *)a;
	return ((d << 8) & 0xff00) | ((d >> 8) & 0x00ff);
}

static void h8max_outl(u32 d, unsigned long a)
{
}

static u32 h8max_inl(unsigned long a)
{
	return 0xffffffffUL;
}

static void h8max_outsw(unsigned long addr, void *buf, u32 len)
{
	unsigned volatile short *ap = (unsigned volatile short *)addr;
	unsigned short *bp = (unsigned short *)buf;
	unsigned short d;
	while(len--) {
		d = *bp++;
		*ap = (d >> 8) | (d << 8);
	}
}

static void h8max_insw(unsigned long addr, void *buf, u32 len)
{
	unsigned volatile short *ap = (unsigned volatile short *)addr;
	unsigned short *bp = (unsigned short *)buf;
	unsigned short d;
	while(len--) {
		d = *ap;
		*bp++ = (d >> 8) | (d << 8);
	}
}

void __init h8300_ide_init(void)
{
	hw_regs_t hw;
	ide_hwif_t *hwif;

	memset(&hw,0,sizeof(hw));
	ide_setup_ports(&hw, IDE_BASE, (int *)offsets,
                             IDE_CTRL, 0, NULL,IDE_IRQ);
	if (ide_register_hw(&hw, &hwif) != -1) {
		hwif->mmio  = 0;
		hwif->OUTB  = h8max_outb;
		hwif->OUTBSYNC = h8max_outbsync;
		hwif->OUTW  = h8max_outw;
		hwif->OUTL  = h8max_outl;
		hwif->OUTSW = h8max_outsw;
		hwif->OUTSL = NULL;
		hwif->INB   = h8max_inb;
		hwif->INW   = h8max_inw;
		hwif->INL   = h8max_inl;
		hwif->INSW  = h8max_insw;
		hwif->INSL  = NULL;
	}
}

void __init ide_print_resource(char *name, hw_regs_t *hw)
{
	printk("%s at 0x%08x-0x%08x,0x%08x on irq %d", name,
		(unsigned int)hw->io_ports[IDE_DATA_OFFSET],
		(unsigned int)hw->io_ports[IDE_DATA_OFFSET]+15,
		(unsigned int)hw->io_ports[IDE_CONTROL_OFFSET],
		hw->irq);
}
