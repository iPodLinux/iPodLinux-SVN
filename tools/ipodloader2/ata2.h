#ifndef _ATA2_H_
#define _ATA2_H_

#include "bootloader.h"

uint32 ata_init(uint32 base);
void   ata_identify(void);
int    ata_readblock(void *dst, uint32 sector);

#endif
