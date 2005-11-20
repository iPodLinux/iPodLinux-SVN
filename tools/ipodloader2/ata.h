#ifndef _ATA_H_
#define _ATA_H_

#include "bootloader.h"

#define CB_DATA  0
#define CB_ERR   1
#define CB_FR    1
#define CB_SC    2
#define CB_SN    3
#define CB_CL    4
#define CB_CH    5
#define CB_DH    6
#define CB_STAT  7
#define CB_CMD   7
#define CB_ASTAT 8
#define CB_DC    8
#define CB_DA    9

#define CB_DH_DEV0 0xa0
#define CB_DH_DEV1 0xb0

#define CB_STAT_BSY 0x80
#define CB_STAT_RDY 0x40
#define CB_STAT_DRQ 0x08
#define CB_STAT_ERR 0x01


#define CB_DC_HD15 0x08
#define CB_DC_SRST 0x04
#define CB_DC_NIEN 0x02

#define CMD_DEVICE_RESET    0x08
#define CMD_IDENTIFY_DEVICE 0xEC
#define CMD_READ_BUFFER     0xE4
#define CMD_READ_SECTORS    0x20
#define CMD_READ_MULTIPLE   0xC4

typedef struct {
  uint32 sectors;
} blockdev_t;

uint32 ata_init(uint32 base);
int blockdev_read(void *dst,uint32 sector);

#endif
