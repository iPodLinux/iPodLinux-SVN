/*
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
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
#define __io(a)			(a)

/*
 * If we define __mem_pci,__mem_isa then asm/io.h will take care of
 * readsw & friends macros.
 */
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		((unsigned long)(a))

#endif

