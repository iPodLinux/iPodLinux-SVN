/***********************************************************************
 * linux/arch/armnommu/mach-c5471/mm.c
 *
 * Based on linux/drivers/net/isa-skeleton.c from the linux-2.4.17
 * and on the linux/drivers/net/skeleton.c from the linux 2.0.38
 * distribution:
 *
 *  Copyright (C) 1998-1999 Russell King
 *  Copyright (C) 1999 Hugo Fiennes
 *  Copyright (C) 2001 RidgeRun, Inc
 *  Copyright (C) 2003 Cadenux, LLC.
 *
 *  Extra MM routines for the SA1100 architecture
 *
 *  1999/12/04 Nicolas Pitre <nico@cam.org>
 *	Converted memory definition for struct meminfo initialisations.
 *	Memory is listed physically now.
 *
 *  2000/04/07 Nicolas Pitre <nico@cam.org>
 *	Reworked for run-time selection of memory definitions
 *
 *  2001/02/19 Gordon McNutt <gmcnutt@ridgerun.com>
 *      Modified mm-sa1100.c to make this, mm-dsc21.c.
 *
 *  2002 Gregory Nutt <greg.nutt@cadenux.com>
 *      Modified mm-dsc21.c for the c5471
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***********************************************************************/

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/bootmem.h>

#include <asm/hardware.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mach-types.h>

#include <asm/mach/map.h>
 
#ifdef CONFIG_DISCONTIGMEM

/*
 * Our node_data structure for discontigous memory.
 * There is 4 possible nodes i.e. the 4 SA1100 RAM banks.
 */

static bootmem_data_t node_bootmem_data[4];

pg_data_t c5471_node_data[4] =
{ { bdata: &node_bootmem_data[0] },
  { bdata: &node_bootmem_data[1] },
  { bdata: &node_bootmem_data[2] },
  { bdata: &node_bootmem_data[3] } };

#endif
