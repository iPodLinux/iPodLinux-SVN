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
#define PCIO_BASE 0
#define __io(a) (PCIO_BASE + (a))

#define __arch_getw(a) (*(volatile unsigned short *)(a))
#define __arch_putw(v,a) (*(volatile unsigned short *)(a) = (v))

#endif

