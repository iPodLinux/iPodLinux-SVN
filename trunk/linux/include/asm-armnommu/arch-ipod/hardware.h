/*
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

/* this is called from drivers/block/blkmem.c */
#define HARD_RESET_NOW() ipod_hard_reset()

#ifndef __ASSEMBLY__
struct sysinfo_t {
	unsigned IsyS;
	unsigned pad[17];
	unsigned boardHwSwInterfaceRev; /* #72 */
};

extern unsigned ipod_get_hw_version(void);
extern struct sysinfo_t *ipod_get_sysinfo(void);
#endif

#endif

