/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/*
 * These should be the same as the definitions in asm/memory.h
 * without the '__' at the beginning.
 */
#define __virt_to_bus(x) ((unsigned long) (x))
#define __bus_to_virt(x) ((void *) (x))
#define __virt_to_phys(x) ((unsigned long) (x))
#define __phys_to_virt(x) ((void *) (x))

#define PHYS_OFFSET	(DRAM_BASE)
#define PAGE_OFFSET	PHYS_OFFSET

/* user space process size - 16MB */
#define TASK_SIZE	(0x01000000UL)

/* arch/armnommu/kernel/setup.c */
#define END_MEM		(DRAM_BASE + DRAM_SIZE)

#endif

