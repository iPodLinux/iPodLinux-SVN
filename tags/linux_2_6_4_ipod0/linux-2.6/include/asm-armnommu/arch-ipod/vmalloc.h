/*
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_VMALLOC_H__
#define __ASM_ARCH_VMALLOC_H__

/*
 * All 32bit addresses are effectively valid for vmalloc...
 * Sort of meaningless for non-VM targets.
 */
#define	VMALLOC_START	0
#define	VMALLOC_END	0xffffffff

#endif

