/*
 *  linux/arch/arm/mach-ks8695/arch.c
 *
 *  Copyright (C) 2002 Micrel Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/amba_kmi.h>
#include <asm/arch/platform.h>

extern void ks8695_map_io(void);
extern void ks8695_init_irq(void);

#ifdef CONFIG_KMI_KEYB
static struct kmi_info ks8695_keyboard __initdata = {
	base:	IO_ADDRESS(KMI0_BASE),
	irq:	IRQ_KMIINT0,
	divisor: 24 / 8 - 1,
	type:	KMI_KEYBOARD,
};

static struct kmi_info ks8695_mouse __initdata = {
	base:	IO_ADDRESS(KMI1_BASE),
	irq:	IRQ_KMIINT1,
	divisor: 24 / 8 - 1,
	type:	KMI_MOUSE,
};
#endif

static void __init
ks8695_fixup(struct machine_desc *desc, struct param_struct *unused,
		 char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_KMI_KEYB
	register_kmi(&ks8695_keyboard);
	register_kmi(&ks8695_mouse);
#endif
}

#ifdef CONFIG_MACH_LITE300
MACHINE_START(KS8695, "SnapGear-LITE300")
	MAINTAINER("SnapGear Inc.")
	BOOT_MEM(KS8695_MEM_START, KS8695_IO_BASE, 0xF0000000)
	BOOT_PARAMS(0x00000100)
	FIXUP(ks8695_fixup)
	MAPIO(ks8695_map_io)
	INITIRQ(ks8695_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_MACH_KS8695
MACHINE_START(KS8695, "Micrel-KS8695")
	MAINTAINER("Micrel Inc.")
	BOOT_MEM(KS8695_MEM_START, KS8695_IO_BASE, 0xF0000000)
	BOOT_PARAMS(0x00000100)
	FIXUP(ks8695_fixup)
	MAPIO(ks8695_map_io)
	INITIRQ(ks8695_init_irq)
MACHINE_END
#endif
