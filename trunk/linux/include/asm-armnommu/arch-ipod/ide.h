/*
 * Copyright (c) 2003-2005, Bernard Leach (leachbj@bouncycastle.org)
 */


#ifndef __ASM_ARCH_IDE_H
#define __ASM_ARCH_IDE_H

static __inline__ void ide_init_hwif_ports(
	hw_regs_t *hw,
	ide_ioreg_t data_port,
	ide_ioreg_t ctrl_port,
	int *irq)
{
}

static __inline__ void ide_init_default_hwifs(void)
{
}

#endif
