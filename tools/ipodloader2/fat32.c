#include "bootloader.h"
#include "console.h"
#include "minilibc.h"
#include "ata2.h"
#include "fat32.h"

typedef struct {
  uint8  skip1[4];
  uint8  type;
  uint8  skip2[3];
  uint32 lba;
  uint8  skip3[4];
} part_entry;

void fat32_init(void) {
  uint8      *bootsect;
  part_entry  part;
  uint32      offset,i;

  bootsect = mlc_malloc(512);
  ata_readblock( bootsect, 0 );

  if( (bootsect[510] == 0x55) && (bootsect[511] == 0xAA) ) {
    mlc_printf("Valid bootsector\n");
  } else {
    mlc_printf("Invalid bootsector\n");
  }

  part.type = bootsect[446 + 4];
  part.lba  = (bootsect[446 + 11] << 24) | (bootsect[446 + 10] << 16)  | (bootsect[446 + 9] << 8) | (bootsect[446 + 8]);
  mlc_printf("p0 - T:%x LBA:%u\n",part.type,part.lba);
  part.type = bootsect[462 + 4];
  part.lba  = (bootsect[462 + 11] << 24) | (bootsect[462 + 10] << 16)  | (bootsect[462 + 9] << 8) | (bootsect[462 + 8]);
  mlc_printf("p1 - T:%x LBA:%u\n",part.type,part.lba);
  //  mlc_printf("p1 - T:%x\n",part.type);
  part.type = bootsect[478 + 4];
  part.lba  = (bootsect[478 + 11] << 24) | (bootsect[478 + 10] << 16)  | (bootsect[478 + 9] << 8) | (bootsect[478 + 8]);
  mlc_printf("p2 - T:%x LBA:%u\n",part.type,part.lba);
  //  mlc_printf("p2 - T:%x\n",part.type);

  part.lba  = (bootsect[462 + 11] << 24) | (bootsect[462 + 10] << 16)  | (bootsect[462 + 9] << 8) | (bootsect[462 + 8]);
  ata_readblock( bootsect, part.lba );

  if( (bootsect[510] == 0x55) && (bootsect[511] == 0xAA) ) {
    mlc_printf("Proper FAT32 sig\n");
  }
  offset = *(uint32*)(bootsect + 0x2C);
  mlc_printf("RootDir: %u\n",offset);
  
  ata_readblock( bootsect, part.lba+offset ); // Read first cluster of root
  for(i=0;i<11;i++) console_putchar(bootsect[i]);
  console_putchar('\n');


  //part = (part_entry *)(bootsect + 446);
  //mlc_printf("p0 - T:%u  LBA:%u\n",part->type,part->lba);
}
