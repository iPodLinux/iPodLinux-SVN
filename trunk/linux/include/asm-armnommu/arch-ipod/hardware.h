/*
 * Copyright (c) 2003-2005 Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

/* this is called from drivers/block/blkmem.c */
#define HARD_RESET_NOW() ipod_hard_reset()

/* PP5020,PP5002 register definitions */
#define PP5002_PROC_ID	0xc4000000
#define PP5002_COP_CTRL	0xcf004058
#define PP5020_PROC_ID	0x60000000
#define PP5020_COP_CTRL	0x60007004

/* special locations in fast ram */
#define PP_CPU_TYPE	0x40000000

#define DMA_READ_OFF	0x40000004
#define DMA_WRITE_OFF	0x40000008
#define DMA_ACTIVE	0x4000000c
#define DMA_STEREO	0x40000010
#define DMA_HANDLER	(ipod_dma_handler_t *)0x40000014
#define DMA_BASE	0x40000018

#define SYSINFO_TAG	(unsigned char *)0x40017f18
#define SYSINFO_PTR	(struct sysinfo_t **)0x40017f1c

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

extern void ipod_i2c_init(void);
extern int ipod_i2c_send_bytes(unsigned int addr, unsigned int len, unsigned char *data);
extern int ipod_i2c_send(unsigned int addr, int data0, int data1);
extern int ipod_i2c_send_byte(unsigned int addr, int data0);
extern int ipod_i2c_read_byte(unsigned int addr, unsigned int *data);

extern void ipod_serial_init(void);

typedef void (*ipod_dma_handler_t)(void);

extern void ipod_set_process_dma(ipod_dma_handler_t new_handler);

#endif

#endif

