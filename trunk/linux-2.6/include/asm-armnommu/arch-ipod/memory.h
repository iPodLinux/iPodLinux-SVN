/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/*
 * These should be the same as the definitions in asm/memory.h
 * without the '__' at the beginning.
 */
#define __virt_to_phys(vpage) ((unsigned long) (vpage))
#define __phys_to_virt(ppage) ((unsigned long) (ppage))
#define __virt_to_bus(vpage) ((unsigned long) (vpage))
#define __bus_to_virt(ppage) ((unsigned long) (ppage))


#define PHYS_OFFSET	(CONFIG_DRAM_BASE)
#define PAGE_OFFSET	(PHYS_OFFSET)
#define END_MEM		(CONFIG_DRAM_BASE + CONFIG_DRAM_SIZE)

#define TASK_SIZE	(0xf0000000UL)

#define PHYS_TO_NID(addr)	(0)  /* node  0  */
#define NR_NODES		1

#endif

