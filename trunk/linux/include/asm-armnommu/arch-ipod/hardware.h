/*
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

/* this is called from drivers/block/blkmem.c */
#define HARD_RESET_NOW() ipod_hard_reset()

#ifndef __ASSEMBLY__
struct sysinfo_t {
	unsigned IsyS;  /* == "IsyS" */
	unsigned len;

	char BoardHwName[16];
	char pszSerialNumber[32];
	char pu8FirewireGuid[16];

	unsigned boardHwRev;
	unsigned bootLoaderImageRev;
	unsigned diskModeImageRev;
	unsigned diagImageRev;
	unsigned osImageRev;

	unsigned iram_perhaps;

	unsigned Flsh;
	unsigned flash_zero;
	unsigned flash_base;
	unsigned flash_size;
	unsigned flash_zero2;
								
	unsigned Sdrm;
	unsigned sdram_zero;
	unsigned sdram_base;
	unsigned sdram_size;
	unsigned sdram_zero2;
								
	unsigned Frwr;
	unsigned frwr_zero;
	unsigned frwr_base;
	unsigned frwr_size;
	unsigned frwr_zero2;
								
	unsigned Iram;
	unsigned iram_zero;
	unsigned iram_base;
	unsigned iram_size;
	unsigned iram_zero2;
								
	char pad7[120];

	unsigned boardHwSwInterfaceRev;

	/* added in V3 */
	char HddFirmwareRev[10];

	unsigned short RegionCode;

	unsigned PolicyFlags;

	char ModelNumStr[16];
};

extern unsigned ipod_get_hw_version(void);
extern struct sysinfo_t *ipod_get_sysinfo(void);
#endif

#endif

