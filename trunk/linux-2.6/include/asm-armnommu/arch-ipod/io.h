/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_IO_H
#define __ASM_ARCH_IO_H

/* kernel/resource.c
 * used to initialize the global ioport_resource struct
 * which is used in all calls to request_resource(),
 * allocate_resource(), etc.
 */
#define IO_SPACE_LIMIT 0xffffffff

/*
 * If we define __io then asm/io.h will take care of most of the
 * inb & friends macros.
 */
#define PCI_IO_VADDR      (0x0)
#define PCI_MEMORY_VADDR  (0x0)

#define __io(a)			(PCI_IO_VADDR + (a))
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		(PCI_MEMORY_VADDR + (unsigned long)(a))

/*
 * Validate the pci memory address for ioremap.
 */
#define iomem_valid_addr(iomem,size)	(1)


/*
 * Convert PCI memory space to a CPU physical address
 */
#define iomem_to_phys(iomem)	(iomem)

#endif

