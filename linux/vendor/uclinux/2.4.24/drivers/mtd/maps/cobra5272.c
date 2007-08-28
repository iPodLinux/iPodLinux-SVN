/*
 * This file is "heavily based" ;) on physmap.c, which I copied
 * and changed to suit my needs! But to show who is responsible
 * for the file I included the new copyright. I don't know if that
 * is the correct way, so if this should be a problem, please
 * email me! 
 * 
 * (C) 2002 senTec Elektronik GmbH, Heiko Degenhardt
 * <linux@sentec-elektronik.de>
 * 
 * ----------------------------------------------------------------------
 * File:       cobra5272.c
 * Place:      uClinux-dist/linux-2.4.x/drivers/mtd/maps/
 * Author:     Heiko Degenhardt (linux@sentec-elektronik.de)
 * Contents:   Flash layout of the COBRA5272 board.
 * Version:    v01.00
 * Date:       Mon Mar 25 09:01:01 CET 2002
 * ______________________________________________________________________
 *
 * CHANGES
 * 020325   v01.00 Creation from physmap.c
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


#define WINDOW_ADDR 0xffe00000
#define WINDOW_SIZE 0x200000
#define BUSWIDTH 2

static struct mtd_info *mymtd;

__u8 cb5272map_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 cb5272map_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 cb5272map_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void cb5272map_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void cb5272map_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void cb5272map_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void cb5272map_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void cb5272map_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

struct map_info cb5272map_map = {
	name: "Physically mapped flash of COBRA5272",
	size: WINDOW_SIZE,
	buswidth: BUSWIDTH,
	read8: cb5272map_read8,
	read16: cb5272map_read16,
	read32: cb5272map_read32,
	copy_from: cb5272map_copy_from,
	write8: cb5272map_write8,
	write16: cb5272map_write16,
	write32: cb5272map_write32,
	copy_to: cb5272map_copy_to
};

/*
 * MTD 'PARTITIONING' STUFF 
 */
#define NUM_PARTITIONS 10
static struct mtd_partition nettel_partitions[NUM_PARTITIONS] = {
        {
                name: "boot (16K)",
                size: 0x4000,
                offset: 0
        },
        {
                name: "kernel (512K)",
                size: 0x80000,
                offset: 0x80000
        },
        {
                name: "rootfs (1024K)",
                size: 0x100000,
                offset: 0x100000
        },
	{
                name: "spare (8K)",
                size: 0x2000,
                offset: 0x4000
        },
        {
                name: "spare (8K)",
                size: 0x2000,
                offset: 0x6000
        },
        {
                name: "spare (256K)",
                size: 0x40000,
                offset: 0x40000
        },
        {
                name: "complete (2048K)",
                size: 0x200000,
                offset: 0x0
        },
        {
                name: "boot J13 (256K)",
                size: 0x40000,
                offset: 0x100000
        },
        {
                name: "kernel J13 (512K)",
                size: 0x80000,
                offset: 0x140000
        },
        {
                name: "rootfs J13 (256K)",
                size: 0x40000,
                offset: 0x1c0000
        }
};

#if LINUX_VERSION_CODE < 0x20212 && defined(MODULE)
#define init_cb5272map init_module
#define cleanup_cb5272map cleanup_module
#endif

int __init init_cb5272map(void)
{
  	printk(KERN_NOTICE "cb5272map flash device: %x at %x\n", WINDOW_SIZE, WINDOW_ADDR);
	cb5272map_map.map_priv_1 = (unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);

	if (!cb5272map_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &cb5272map_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;

		return add_mtd_partitions(mymtd, nettel_partitions, NUM_PARTITIONS);
	}

	iounmap((void *)cb5272map_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_cb5272map(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (cb5272map_map.map_priv_1) {
		iounmap((void *)cb5272map_map.map_priv_1);
		cb5272map_map.map_priv_1 = 0;
	}
}

module_init(init_cb5272map);
module_exit(cleanup_cb5272map);

