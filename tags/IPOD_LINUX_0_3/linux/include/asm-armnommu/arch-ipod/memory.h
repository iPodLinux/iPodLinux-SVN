/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define PHYS_OFFSET	(DRAM_BASE)

#define PAGE_OFFSET	PHYS_OFFSET

/* user space process size - 16MB */
#define TASK_SIZE	(0x01000000UL)

/* arch/armnommu/kernel/setup.c */
#define END_MEM		(DRAM_BASE + DRAM_SIZE)

#endif

