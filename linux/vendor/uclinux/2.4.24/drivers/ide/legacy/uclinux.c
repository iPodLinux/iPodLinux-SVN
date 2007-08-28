/****************************************************************************/
/*
 *  linux/drivers/ide/legacy/uclinux.c -- A simple driver that is easy to
 *                                        adjust for a large number of
 *                                        embedded targets
 *
 *     Copyright (C) 2001-2003 by David McCullough <davidm@snapgear.com>
 *     Copyright (C) 2002 by Greg Ungerer <gerg@snapgear.com>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 *
 *  ex: set ts=4 sw=4
 */
/****************************************************************************/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/delay.h>
#include <linux/ide.h>

/****************************************************************************/

#ifdef CONFIG_COLDFIRE
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#endif

/****************************************************************************/
/*
 *	Our list of ports/irq's for different boards.
 */

static struct ide_defaults {
	ide_ioreg_t	base;
	int			irq;
} ide_defaults[] = {
#if defined(CONFIG_SECUREEDGEMP3)
	{ ((ide_ioreg_t) 0x30800000), 29 },
#elif defined(CONFIG_eLIA) || defined(CONFIG_DISKtel)
	{ ((ide_ioreg_t) 0x30c00000), 29 },
#elif defined(CONFIG_M5249C3)
	{ ((ide_ioreg_t) 0x50000020), 165 },
#else
	#error "No IDE setup defined"
#endif
	{ ((ide_ioreg_t) -1), -1 }
};

/****************************************************************************/
/*
 * the register offsets from the base address
 */

#define IDE_DATA	0x00
#define IDE_ERROR	0x01
#define IDE_NSECTOR	0x02
#define IDE_SECTOR	0x03
#define IDE_LCYL	0x04
#define IDE_HCYL	0x05
#define IDE_SELECT	0x06
#define IDE_STATUS	0x07
#define IDE_CONTROL	0x0e /* this one may change on some platforms */
#define IDE_IRQ		-1 /* not used */

static int ide_offsets[IDE_NR_PORTS] = {
    IDE_DATA, IDE_ERROR,  IDE_NSECTOR, IDE_SECTOR, IDE_LCYL,
    IDE_HCYL, IDE_SELECT, IDE_STATUS,  IDE_CONTROL, IDE_IRQ
};

#define	DBGIDE(fmt,a...)
//#define	DBGIDE(fmt,a...) printk(fmt, a)

/****************************************************************************/

static void	OUTB(u8 addr, unsigned long port);
static void	OUTBSYNC(ide_drive_t *drive, u8 addr, unsigned long port);
static void	OUTW(u16 addr, unsigned long port);
static void	OUTL(u32 addr, unsigned long port);
static void	OUTSW(unsigned long port, void *addr, u32 count);
static void	OUTSL(unsigned long port, void *addr, u32 count);
static u8	INB(unsigned long port);
static u16	INW(unsigned long port);
static u32	INL(unsigned long port);
static void	INSW(unsigned long port, void *addr, u32 count);
static void	INSL(unsigned long port, void *addr, u32 count);

/****************************************************************************/
/*
 * if needed, this function returns 1 if an interrupt was acked
 */

#if defined(CONFIG_M5249C3)
static int ack_intr(ide_hwif_t* hwif)
{
	/* Clear interrupts for GPIO5 */
	*((volatile unsigned long *) 0x800000c0) = 0x00002020;
	return 1;
}
#else
#define ack_intr NULL
#endif

/****************************************************************************/

void uclinux_ide_init(void)
{
	hw_regs_t hw;
	ide_ioreg_t base;
	int index, i;
	ide_hwif_t* hwif;

	for (i = 0; ide_defaults[i].base != (ide_ioreg_t) -1; i++) {
		base = ide_defaults[i].base;
		memset(&hw, 0, sizeof(hw));
#ifdef CONFIG_COLDFIRE
		mcf_autovector(hw.irq);
#endif
		hw.irq = ide_defaults[i].irq;
		ide_setup_ports(&hw, base, ide_offsets, 0, 0, ack_intr,
				/* ide_iops, */
				ide_defaults[i].irq);
		index = ide_register_hw(&hw, &hwif);
		if (index != -1) {
			hwif->mmio  = 1; /* not io(0) or mmio(1) */
#ifdef CONFIG_COLDFIRE
			hwif->OUTB  = OUTB;
			hwif->OUTBSYNC = OUTBSYNC;
			hwif->OUTW  = OUTW;
			hwif->OUTL  = OUTL;
			hwif->OUTSW = OUTSW;
			hwif->OUTSL = OUTSL;
			hwif->INB   = INB;
			hwif->INW   = INW;
			hwif->INL   = INL;
			hwif->INSW  = INSW;
			hwif->INSL  = INSL;
#endif
		}
		disable_irq(ide_defaults[i].irq);
	}
}

/****************************************************************************/
/*
 * this is called after the irq handler has been installed,  so if you need
 * to fix something do it here
 */

void
uclinux_irq_fixup()
{
#ifdef CONFIG_M5249C3
	/* Enable interrupts for GPIO5 */
	*((volatile unsigned long *) 0x800000c4) |= 0x00000020;

	/* Enable interrupt level for GPIO5 - VEC37 */
	*((volatile unsigned long *) 0x80000150) |= 0x00200000;
#endif
}

/****************************************************************************/
/*
 * the custom IDE IO access for platforms that need it
 */

#ifdef CONFIG_COLDFIRE

#if defined(CONFIG_eLIA) || defined(CONFIG_DISKtel)
 /* 8/16 bit acesses are controlled by flicking bits in the CS register */
 #define	MODE_16BIT()	\
	*((volatile unsigned short *) (MCF_MBAR + MCFSIM_CSCR6)) = 0x0080
 #define	MODE_8BIT()	\
	*((volatile unsigned short *) (MCF_MBAR + MCFSIM_CSCR6)) = 0x0040

 #define	ENABLE16()		MODE_16BIT()
 #define	ENABLE32()		MODE_16BIT()
 #define	DISABLE16()		MODE_8BIT()
 #define	DISABLE32()		MODE_8BIT()
#endif /* defined(CONFIG_eLIA) || defined(CONFIG_DISKtel) */

#ifdef CONFIG_SECUREEDGEMP3
 #define	ADDR8(addr)		(((addr) & 0x1) ? (0x8000+(addr)-1) : (addr))
 #define	ADDR16(addr)	(addr)
 #define	ADDR32(addr)	(addr)
 #define	SWAP8(w)		((((w) & 0xffff) << 8) | (((w) & 0xffff) >> 8))
 #define	SWAP16(w)		(w)
 #define	SWAP32(w)		(w)

 #define	io8_t	unsigned short
#endif CONFIG_SECUREEDGEMP3

#ifdef CONFIG_M5249C3
 #define	ADDR(a)			(((a) & 0xfffffff0) + (((a) & 0xf) << 1))
 #define	ADDR8(addr)		ADDR(addr)
 #define	ADDR16(addr)	ADDR(addr)
 #define	ADDR32(addr)	ADDR(addr)

 #define	SWAP8(w)		(w)
 #define	SWAP16(w)		((((w)&0xffff) << 8)   | (((w)&0xffff) >> 8))
 #define	SWAP32(w)		((((w)&0xff) << 24)    | (((w)&0xff00) << 8) | \
							 (((w)&0xff0000) >> 8) | (((w)&0xff000000) >> 24))

 #define io8_t	unsigned short
#endif

/****************************************************************************/
/*
 * default anything that wasn't set
 */

#ifndef io8_t
#define io8_t	unsigned char
#endif

#ifndef io16_t
#define io16_t	unsigned short
#endif

#ifndef io32_t
#define io32_t	unsigned long
#endif

#ifndef ENABLE8
#define ENABLE8()
#endif

#ifndef ENABLE16
#define ENABLE16()
#endif

#ifndef ENABLE32
#define ENABLE32()
#endif

#ifndef DISABLE8
#define DISABLE8()
#endif

#ifndef DISABLE16
#define DISABLE16()
#endif

#ifndef DISABLE32
#define DISABLE32()
#endif

#ifndef SWAP8
#define SWAP8(x) (x)
#endif

#ifndef SWAP16
#define SWAP16(x) (x)
#endif

#ifndef SWAP16
#define SWAP16(x) (x)
#endif

#ifndef ADDR8
#define ADDR8(x) (x)
#endif

#ifndef ADDR16
#define ADDR16(x) (x)
#endif

#ifndef ADDR32
#define ADDR32(x) (x)
#endif

#ifndef SWAP8
#define SWAP8(x) (x)
#endif

#ifndef SWAP16
#define SWAP16(x) (x)
#endif

#ifndef SWAP32
#define SWAP32(x) (x)
#endif

/****************************************************************************/

static void
OUTB(u8 val, unsigned long addr)
{
	volatile io8_t	*rp;

	DBGIDE("%s(val=%x,addr=%x)\n", __FUNCTION__, val, addr);
	rp = (volatile unsigned short *) ADDR(addr);
	ENABLE8();
	*rp = SWAP8(val);
	DISABLE8();
}

static void
OUTBSYNC(ide_drive_t *drive, u8 val, unsigned long addr)
{
	volatile io8_t	*rp;

	DBGIDE("%s(val=%x,addr=%x)\n", __FUNCTION__, val, addr);
	rp = (volatile unsigned short *) ADDR(addr);
	ENABLE8();
	*rp = SWAP8(val);
	DISABLE8();
}

static u8
INB(unsigned long addr)
{
	volatile io8_t	*rp, val;

	rp = (volatile unsigned short *) ADDR(addr);
	ENABLE8();
	val = *rp;
	DISABLE8();
	DBGIDE("%s(addr=%x) = 0x%x\n", __FUNCTION__, addr, SWAP8(val));
	return(SWAP8(val));
}

static void
OUTW(u16 val, unsigned long addr)
{
	volatile io16_t	*rp;

	DBGIDE("%s(val=%x,addr=%x)\n", __FUNCTION__, val, addr);
	rp = (volatile io16_t *) ADDR(addr);
	ENABLE16();
	*rp = SWAP16(val);
	DISABLE16();
}

static void
OUTSW(unsigned long addr, void *vbuf, u32 len)
{
	volatile io16_t	*rp, val;
	unsigned short   	*buf;

	DBGIDE("%s(addr=%x,vbuf=%p,len=%x)\n", __FUNCTION__, addr, vbuf, len);
	buf = (unsigned short *) vbuf;
	rp = (volatile io16_t *) ADDR(addr);
	ENABLE16();
	for (; (len > 0); len--) {
		val = *buf++;
		*rp = SWAP16(val);
	}
	DISABLE16();
}

static u16
INW(unsigned long addr)
{
	volatile io16_t *rp, val;

	rp = (volatile io16_t *) ADDR(addr);
	ENABLE16();
	val = *rp;
	DISABLE16();
	DBGIDE("%s(addr=%x) = 0x%x\n", __FUNCTION__, addr, SWAP16(val));
	return(SWAP16(val));
}

static void
INSW(unsigned long addr, void *vbuf, u32 len)
{
	volatile io16_t *rp;
	unsigned short          w, *buf;

	DBGIDE("%s(addr=%x,vbuf=%p,len=%x)\n", __FUNCTION__, addr, vbuf, len);
	buf = (unsigned short *) vbuf;
	rp = (volatile io16_t *) ADDR(addr);
	ENABLE16();
	for (; (len > 0); len--) {
		w = *rp;
		*buf++ = SWAP16(w);
	}
	DISABLE16();
}

static u32
INL(unsigned long addr)
{
	volatile io32_t *rp, val;

	rp = (volatile io32_t *) ADDR(addr);
	ENABLE32();
	val = *rp;
	DISABLE32();
	DBGIDE("%s(addr=%x) = 0x%x\n", __FUNCTION__, addr, SWAP32(val));
	return(SWAP32(val));
}

static void
INSL(unsigned long addr, void *vbuf, u32 len)
{
	volatile io32_t *rp;
	unsigned long          w, *buf;

	DBGIDE("%s(addr=%x,vbuf=%p,len=%x)\n", __FUNCTION__, addr, vbuf, len);
	buf = (unsigned long *) vbuf;
	rp = (volatile io32_t *) ADDR(addr);
	ENABLE32();
	for (; (len > 0); len--) {
		w = *rp;
		*buf++ = SWAP32(w);
	}
	DISABLE32();
}

static void
OUTL(u32 val, unsigned long addr)
{
	volatile io32_t	*rp;

	DBGIDE("%s(val=%x,addr=%x)\n", __FUNCTION__, val, addr);
	rp = (volatile io32_t *) ADDR(addr);
	ENABLE32();
	*rp = SWAP32(val);
	DISABLE32();
}

static void
OUTSL(unsigned long addr, void *vbuf, u32 len)
{
	volatile io32_t	*rp, val;
	unsigned long   	*buf;

	DBGIDE("%s(addr=%x,vbuf=%p,len=%x)\n", __FUNCTION__, addr, vbuf, len);
	buf = (unsigned long *) vbuf;
	rp = (volatile io32_t *) ADDR(addr);
	ENABLE32();
	for (; (len > 0); len--) {
		val = *buf++;
		*rp = SWAP32(val);
	}
	DISABLE32();
}

#endif /* CONFIG_COLDFIRE */

/****************************************************************************/
