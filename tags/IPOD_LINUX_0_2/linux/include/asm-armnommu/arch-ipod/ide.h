/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_IDE_H
#define __ASM_ARCH_IDE_H

/* we have one controller */
#undef MAX_HWIFS
#define MAX_HWIFS	1

#define IDE_INLINE __inline__

#define IDE_PRIMARY_BASE	0xc00031e0
#define IDE_PRIMARY_CONTROL	0xc00033f8

static struct ipod_ide_defaults {
	ide_ioreg_t	base;
	int		irq;
} ipod_ide_defaults[MAX_HWIFS] = {
	{ (ide_ioreg_t)IDE_PRIMARY_BASE, 1 },
};

static IDE_INLINE int ide_default_irq(ide_ioreg_t base)
{
	int i;

	for ( i = 0; i < MAX_HWIFS; i++ )
		if ( ipod_ide_defaults[i].base == base )
			return ipod_ide_defaults[i].irq;
	return 0;
}

static IDE_INLINE ide_ioreg_t ide_default_io_base(int index)
{
	if ( index >= 0 && index < MAX_HWIFS )
		return ipod_ide_defaults[index].base;
	return 0;
}

/*
 * Set up a hw structure for a specified data port, control port and IRQ.
 * This should follow whatever the default interface uses.
 */
static IDE_INLINE void ide_init_hwif_ports(
	hw_regs_t *hw,
	ide_ioreg_t data_port,
	ide_ioreg_t ctrl_port,
	int *irq)
{
	ide_ioreg_t reg = data_port;
	int i;

	for ( i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++ ) {
		hw->io_ports[i] = reg;
		reg += 4;	/* our registers are on word boundaries */
	}

	if (ctrl_port) {
		hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
	}
	else {
		hw->io_ports[IDE_CONTROL_OFFSET] = data_port + 0x218;
	}
}

/*
 * This registers the standard ports for this architecture with the IDE
 * driver.
 */
static IDE_INLINE void ide_init_default_hwifs(void)
{
	hw_regs_t hw;
	ide_ioreg_t base;
	int index;

	outl(inl(0xc0003024) | (1<< 7), 0xc0003024);
	outl(inl(0xc0003024) & ~(1<<2), 0xc0003024);

	outl(0x10, 0xc0003000);
	outl(0x80002150, 0xc0003004);

	for (index = 0; index < MAX_HWIFS; index++) {

		base = ide_default_io_base(index);
		if (!base) continue;

		memset(&hw, 0, sizeof(hw));
		ide_init_hwif_ports(&hw, base, IDE_PRIMARY_CONTROL, NULL);
		hw.irq = ide_default_irq(base);
		ide_register_hw(&hw, NULL);
	}
}

#define ide_request_irq(irq,hand,flg,dev,id)	request_irq((irq),(hand),(flg),(dev),(id))
#define ide_free_irq(irq,dev_id)		free_irq((irq), (dev_id))
#define ide_check_region(from,extent)		check_region((from), (extent))
#define ide_request_region(from,extent,name)	request_region((from), (extent), (name))
#define ide_release_region(from,extent)		release_region((from), (extent))

#define ide_ack_intr(hwif)              (1)
#define ide_fix_driveid(id)             do {} while (0)
#define ide_release_lock(lock)          do {} while (0)
#define ide_get_lock(lock, hdlr, data)  do {} while (0)

#endif

