/*
 * This file is "heavily based" ;) on physmap.c, which I copied
 * and changed to suit my needs! But to show who is responsible
 * for the file I included the new copyright. I don't know if that
 * is the correct way, so if this should be a problem, please
 * email me! 
 * 
 * (C) 2003 SSV Embedded Systems <support@ist1.de>
 * 
 * ----------------------------------------------------------------------
 * File:       dnp5280.c
 * Place:      uClinux-dist/linux-2.4.x/drivers/mtd/maps/
 * Author:     Heiko Degenhardt. For support please contact
 *             M. Hasewinkel" <support@ist1.de>
 * Contents:   Flash layout of the DNP5280 board.
 * Version:    v01.00
 * Date:       Thu Sep  4 13:52:19 CEST 2003
 * ______________________________________________________________________
 *
 * CHANGES
 * 030904   v01.00 Creation from physmap.c
 *          (hede)
 * ______________________________________________________________________
 *
 *  This program is free software; you can redistribute  it and/or
 *  modify it under  the terms of  the GNU General  Public License as
 *  published by the Free Software Foundation;  either version 2 of
 *  the  License, or (at your option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR
 *  IMPLIED WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE
 *  FOR ANY   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   TO,
 *  PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF USE, DATA,
 *  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 *  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License
 *  along with this program; if not, write  to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Mappings of chips in memory of the senTec COBRA5272 board.
 * 
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>


#define WINDOW_ADDR 0xff800000
#define WINDOW_SIZE 0x800000
#define BUSWIDTH 2

static struct mtd_info *mymtd;

__u8 dnp5280map_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 dnp5280map_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 dnp5280map_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void dnp5280map_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void dnp5280map_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void dnp5280map_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void dnp5280map_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void dnp5280map_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

struct map_info dnp5280map_map = {
	name: "Physically mapped flash of DNP5280",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8: dnp5280map_read8,
	read16: dnp5280map_read16,
	read32: dnp5280map_read32,
	copy_from: dnp5280map_copy_from,
	write8: dnp5280map_write8,
	write16: dnp5280map_write16,
	write32: dnp5280map_write32,
	copy_to: dnp5280map_copy_to
};

/*
 * MTD 'PARTITIONING' STUFF 
 *
 * ATTENTION! This is just an example!
 * 
 */
#define NUM_PARTITIONS 3

static struct mtd_partition dnp5280_partitions[NUM_PARTITIONS] = {
        {
                name: "dBUG",
                size: 0x50000,
                offset: 0
        },
        {
                name: "uClinux",
                size: 0x2B0000,
                offset: 0x50000
        },
        {
                name: "flash space",
                size: 0x500000,
                offset: 0x300000
        }
};

#if LINUX_VERSION_CODE < 0x20212 && defined(MODULE)
#define init_dnp5280map init_module
#define cleanup_dnp5280map cleanup_module
#endif

int __init init_dnp5280map(void)
{
  	printk(KERN_NOTICE "dnp5280map flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);
	dnp5280map_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!dnp5280map_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &dnp5280map_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;

		return add_mtd_partitions(mymtd, dnp5280_partitions, NUM_PARTITIONS);
	}

	iounmap((void *)dnp5280map_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_dnp5280map(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (dnp5280map_map.map_priv_1) {
		iounmap((void *)dnp5280map_map.map_priv_1);
		dnp5280map_map.map_priv_1 = 0;
	}
}

module_init(init_dnp5280map);
module_exit(cleanup_dnp5280map);

