/*
 * Basic ATA2 driver for the ipodlinux bootloader
 * 
 * Supports:
 *  PIO (Polling)
 *  Multiple block reads
 *
 * Author: James Jacobsson ( slowcoder@mac.com )
 */
#include "bootloader.h"
#include "console.h"
#include "ipodhw.h"
#include "minilibc.h"

#define REG_DATA       0x0
#define REG_ERROR      0x1
#define REG_FEATURES   0x1
#define REG_SECT_COUNT 0x2
#define REG_SECT       0x3
#define REG_CYL_LOW    0x4
#define REG_CYL_HIGH   0x5
#define REG_DEVICEHEAD 0x6
#define REG_STATUS     0x7
#define REG_COMMAND    0x7
#define REG_CONTROL    0x8
#define REG_ALTSTATUS  0x8 

#define REG_DA         0x9

#define CONTROL_NIEN   0x2
#define CONTROL_SRST   0x4

#define COMMAND_IDENTIFY_DEVICE 0xEC
#define COMMAND_READ_MULTIPLE 0xC4
#define COMMAND_READ_SECTORS  0x20
#define COMMAND_READ_SECTORS_VRFY  0x21

#define DEVICE_0       0xA0
#define DEVICE_1       0xB0

#define STATUS_BSY     0x80
#define STATUS_DRDY    0x40
#define STATUS_DF      0x20
#define STATUS_DSC     0x10
#define STATUS_DRQ     0x08
#define STATUS_CORR    0x04
#define STATUS_IDX     0x02
#define STATUS_ERR     0x01

unsigned int pio_base_addr1,pio_base_addr2;
unsigned int pio_reg_addrs[10];

#define CACHE_NUMBLOCKS 16
static uint8  *cachedata;
static uint32 *cacheaddr;
static uint32 *cachetick;
static uint32  cacheticks;

static struct {
  uint16 chs[3];
  uint32 sectors;
} ATAdev;

void pio_outbyte(unsigned int addr,unsigned char data) {
  outl( data, pio_reg_addrs[ addr ] );
}

volatile unsigned char pio_inbyte( unsigned int addr ) {
  return( inl( pio_reg_addrs[ addr ] ) );
}
volatile unsigned short pio_inword( unsigned int addr ) {
  return( inl( pio_reg_addrs[ addr ] ) );
}
volatile unsigned int pio_indword( unsigned int addr ) {
  return( inl( pio_reg_addrs[ addr ] ) );
}

#define DELAY400NS { \
 pio_inbyte(REG_ALTSTATUS); pio_inbyte(REG_ALTSTATUS); \
 pio_inbyte(REG_ALTSTATUS); pio_inbyte(REG_ALTSTATUS); \
 pio_inbyte(REG_ALTSTATUS); pio_inbyte(REG_ALTSTATUS); \
 pio_inbyte(REG_ALTSTATUS); pio_inbyte(REG_ALTSTATUS); \
 pio_inbyte(REG_ALTSTATUS); pio_inbyte(REG_ALTSTATUS); \
 pio_inbyte(REG_ALTSTATUS); pio_inbyte(REG_ALTSTATUS); \
 pio_inbyte(REG_ALTSTATUS); pio_inbyte(REG_ALTSTATUS); \
 pio_inbyte(REG_ALTSTATUS); pio_inbyte(REG_ALTSTATUS); \
}

uint32 ata_init(void) {
  uint8   tmp[2],i;
  ipod_t *ipod;

  ipod = ipod_get_hwinfo();

  pio_base_addr1 = ipod->ide_base;
  pio_base_addr2 = pio_base_addr1 + 0x200;

  /*
   * Sets up a number of "shortcuts" for us to use via the pio_ macros
   * Note: The PP chips have their IO regs 4 byte aligned
   */
  pio_reg_addrs[ REG_DATA       ] = pio_base_addr1 + 0 * 4;
  pio_reg_addrs[ REG_FEATURES   ] = pio_base_addr1 + 1 * 4;
  pio_reg_addrs[ REG_SECT_COUNT ] = pio_base_addr1 + 2 * 4;
  pio_reg_addrs[ REG_SECT       ] = pio_base_addr1 + 3 * 4;
  pio_reg_addrs[ REG_CYL_LOW    ] = pio_base_addr1 + 4 * 4;
  pio_reg_addrs[ REG_CYL_HIGH   ] = pio_base_addr1 + 5 * 4;
  pio_reg_addrs[ REG_DEVICEHEAD ] = pio_base_addr1 + 6 * 4;
  pio_reg_addrs[ REG_COMMAND    ] = pio_base_addr1 + 7 * 4;
  pio_reg_addrs[ REG_CONTROL    ] = pio_base_addr2 + 6 * 4;
  pio_reg_addrs[ REG_DA         ] = pio_base_addr2 + 7 * 4;

  /*
   * Black magic
   */
  if( (ipod->hw_rev >> 16) > 3 ) {
    /* PP502x */
    outl(inl(0xc3000028) | (1 << 5), 0xc3000028);
    outl(inl(0xc3000028) & ~0x10000000, 0xc3000028);
    
    outl(0x10, 0xc3000000);
    outl(0x80002150, 0xc3000004);
  } else {
    /* PP5002 */
    outl(inl(0xc0003024) | (1 << 7), 0xc0003024);
    outl(inl(0xc0003024) & ~(1<<2), 0xc0003024);
    
    outl(0x10, 0xc0003000);
    outl(0x80002150, 0xc0003004);
  }

  /* 1st things first, check if there is an ATA controller here
   * We do this by writing values to two GP registers, and expect
   * to be able to read them back
   */
  pio_outbyte( REG_DEVICEHEAD, DEVICE_0 ); /* Device 0 */
  DELAY400NS;
  pio_outbyte( REG_SECT_COUNT, 0x55 );
  pio_outbyte( REG_SECT      , 0xAA );
  pio_outbyte( REG_SECT_COUNT, 0xAA );
  pio_outbyte( REG_SECT      , 0x55 );
  pio_outbyte( REG_SECT_COUNT, 0x55 );
  pio_outbyte( REG_SECT      , 0xAA );
  tmp[0] = pio_inbyte( REG_SECT_COUNT );
  tmp[1] = pio_inbyte( REG_SECT );
  if( (tmp[0] != 0x55) || (tmp[1] != 0xAA) ) return(1);

  /*
   * Okay, we're sure there's an ATA2 controller and device, so
   * lets set up the caching
   */
  cachedata  = (uint8 *)mlc_malloc(CACHE_NUMBLOCKS * 512);
  cacheaddr  = (uint32*)mlc_malloc(CACHE_NUMBLOCKS * sizeof(uint32));
  cachetick  = (uint32*)mlc_malloc(CACHE_NUMBLOCKS * sizeof(uint32));
  cacheticks = 0;
  
  for(i=0;i<CACHE_NUMBLOCKS;i++) {
    cachetick[i] =  0;  /* Time is zero */
    cacheaddr[i] = -1;  /* Invalid sector number */
  }

  return(0);
}

/*
 * Copies one block of data (512bytes) from the device
 * to host memory
 */
static void ata_transfer_block(void *ptr) {
  uint32  wordsRead;
  uint16 *dst;

  dst = (uint16*)ptr;

  wordsRead = 0;
  while(wordsRead < 256) {
    dst[wordsRead] = pio_inword( REG_DATA );

    wordsRead++;
  }
}

/*
 * Does some extended identification of the ATA device
 */
void ata_identify(void) {
  uint8  status,c;
  uint16 *buff = (uint16*)mlc_malloc(512);

  pio_outbyte( REG_DEVICEHEAD, DEVICE_0 );
  pio_outbyte( REG_FEATURES  , 0 );
  pio_outbyte( REG_CONTROL   , CONTROL_NIEN );
  pio_outbyte( REG_SECT_COUNT, 0 );
  pio_outbyte( REG_SECT      , 0 );
  pio_outbyte( REG_CYL_LOW   , 0 );
  pio_outbyte( REG_CYL_HIGH  , 0 );

  pio_outbyte( REG_COMMAND, COMMAND_IDENTIFY_DEVICE );
  DELAY400NS;

  while( pio_inbyte( REG_ALTSTATUS) & STATUS_BSY ); /* Spin until drive is not busy */

  status = pio_inbyte( REG_STATUS );
  if( status & STATUS_DRQ ) {
    ata_transfer_block( buff );

    ATAdev.sectors = (buff[61] << 16) + buff[60];
    ATAdev.chs[0]  = buff[1];
    ATAdev.chs[1]  = buff[3];
    ATAdev.chs[2]  = buff[6];
    
    mlc_printf("ATA Device\n");
    mlc_printf("Size: %uMB (%u/%u/%u)\n",ATAdev.sectors/2048,ATAdev.chs[0],ATAdev.chs[1],ATAdev.chs[2]);

    mlc_printf("HDDid: ");
    for(c=27;c<47;c++) {
      if( buff[c] != ((' ' << 8) + ' ') ) {
	console_putchar(buff[c]>>8);
	console_putchar(buff[c]&0xFF);
      }
    }
    console_putchar('\n');
  } else {
    mlc_printf("DRQ not set..\n");
  }
}

/*
 * Sets up the transfer of one block of data
 */
int ata_readblock(void *dst, uint32 sector) {
  uint8   status,i,cacheindex;

  /*
   * Check if we have this block in cache first
   */
  for(i=0;i<CACHE_NUMBLOCKS;i++) {
    if( cacheaddr[i] == sector ) {
      mlc_memcpy(dst,cachedata + 512*i,512);  /* We did.. No need to bother the ATA controller */
      cacheticks++;
      cachetick[i] = cacheticks;
      return(0);
    }
  }
  /*
   * Okay, it wasnt in cache.. We need to figure out which block
   * to replace in the cache.  Lets use a simple LRU
   */
  cacheindex = 0;
  for(i=0;i<CACHE_NUMBLOCKS;i++) {
    if( cachetick[i] < cachetick[cacheindex] ) cacheindex = i;
  }
  cachetick[cacheindex] = cacheticks;

  pio_outbyte( REG_DEVICEHEAD, (1<<6) | DEVICE_0 | ((sector & 0xF000000) >> 24) );
  DELAY400NS;
  pio_outbyte( REG_FEATURES  , 0 );
  pio_outbyte( REG_CONTROL   , CONTROL_NIEN | 0x08); /* 8 = HD15 */
  pio_outbyte( REG_SECT_COUNT, 1 );
  pio_outbyte( REG_SECT      ,  sector & 0xFF );
  pio_outbyte( REG_CYL_LOW   , (sector & 0xFF00) >> 8 );
  pio_outbyte( REG_CYL_HIGH  , (sector & 0xFF0000) >> 16 );

  pio_outbyte( REG_COMMAND, COMMAND_READ_SECTORS_VRFY );
  DELAY400NS;  DELAY400NS;

  while( pio_inbyte( REG_ALTSTATUS) & STATUS_BSY ); /* Spin until drive is not busy */
  DELAY400NS;  DELAY400NS;

  status = pio_inbyte( REG_STATUS );
  if( (status & (STATUS_BSY | STATUS_DRQ)) == STATUS_DRQ) {
    /*ata_transfer_block(dst);*/
    cacheaddr[cacheindex] = sector;
    ata_transfer_block(cachedata + cacheindex * 512);
  }  else {
    mlc_printf("IO Error\n");
    status = pio_inbyte( REG_ERROR );
    mlc_printf("Error reg: %u\n",status);
    for(;;);
  }

  /*
   * We've read a block. Lets make sure it's getting cached as well
   */
  mlc_memcpy(dst,cachedata + cacheindex*512,512);

  cacheticks++;

  return(0);
}

/*
 * Replace this with COMMAND_READ_MULTIPLE for FAT32 speedups
 */
void ata_readblocks(void *dst,uint32 sector,uint32 count) {
  uint32 i;

  for(i=0;i<count;i++)
    ata_readblock((void*)((uint8*)dst + i*512),sector+i);

}
