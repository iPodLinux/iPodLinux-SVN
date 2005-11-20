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

uint32 ata_init(uint32 base) {
  uint8 tmp[2];

  pio_base_addr1 = base;
  pio_base_addr2 = pio_base_addr1 + 0x200;

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


  outl(inl(0xc3000028) | (1 << 5), 0xc3000028);
  outl(inl(0xc3000028) & ~0x10000000, 0xc3000028);
  
  outl(0x10, 0xc3000000);
  outl(0x80002150, 0xc3000004);


  /* 1st things first, check if there is an ATA controller here
   * We do this by writing values to two GP registers, and expect
   * to be able to read them back
   */
  pio_outbyte( REG_DEVICEHEAD, DEVICE_0 ); // Device 0
  ipod_wait_usec(500);
  pio_outbyte( REG_SECT_COUNT, 0x55 );
  pio_outbyte( REG_SECT      , 0xAA );
  pio_outbyte( REG_SECT_COUNT, 0xAA );
  pio_outbyte( REG_SECT      , 0x55 );
  pio_outbyte( REG_SECT_COUNT, 0x55 );
  pio_outbyte( REG_SECT      , 0xAA );
  tmp[0] = pio_inbyte( REG_SECT_COUNT );
  tmp[1] = pio_inbyte( REG_SECT );
  if( (tmp[0] != 0x55) || (tmp[1] != 0xAA) ) return(1);

  return(0);
}

static void ata_transfer_block(void *ptr) {
  uint32  wordsRead;
  uint16 *dst;

  dst = (uint16*)ptr;

  wordsRead = 0;
  while(wordsRead < 256) {
    dst[wordsRead] = pio_inword( REG_DATA );
    //tmp = pio_inword( REG_DATA );

    wordsRead++;
  }
}

void ata_identify(void) {
  uint8  status,c;
  //uint16 buff[256];
  uint16 *buff = (uint16*)0x11100000;

  pio_outbyte( REG_DEVICEHEAD, DEVICE_0 );
  pio_outbyte( REG_FEATURES  , 0 );
  pio_outbyte( REG_CONTROL   , CONTROL_NIEN );
  pio_outbyte( REG_SECT_COUNT, 0 );
  pio_outbyte( REG_SECT      , 0 );
  pio_outbyte( REG_CYL_LOW   , 0 );
  pio_outbyte( REG_CYL_HIGH  , 0 );

  pio_outbyte( REG_COMMAND, COMMAND_IDENTIFY_DEVICE );
  DELAY400NS;

  while( pio_inbyte( REG_ALTSTATUS) & STATUS_BSY ); // Spin until drive is not busy

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

int ata_readblock(void *dst, uint32 sector) {
  uint8   status;

  //sector++;

  pio_outbyte( REG_DEVICEHEAD, (1<<6) | DEVICE_0 | ((sector & 0xF000000) >> 24) );
  DELAY400NS;
  pio_outbyte( REG_FEATURES  , 0 );
  pio_outbyte( REG_CONTROL   , CONTROL_NIEN | 0x08); // 8 = HD15
  pio_outbyte( REG_SECT_COUNT, 1 );
  pio_outbyte( REG_SECT      ,  sector & 0xFF );
  pio_outbyte( REG_CYL_LOW   , (sector & 0xFF00) >> 8 );
  pio_outbyte( REG_CYL_HIGH  , (sector & 0xFF0000) >> 16 );

  pio_outbyte( REG_COMMAND, COMMAND_READ_SECTORS_VRFY );
  DELAY400NS;  DELAY400NS;

  while( pio_inbyte( REG_ALTSTATUS) & STATUS_BSY ); // Spin until drive is not busy
  DELAY400NS;  DELAY400NS;

  status = pio_inbyte( REG_STATUS );
  if( (status & (STATUS_BSY | STATUS_DRQ)) == STATUS_DRQ) {
    ata_transfer_block(dst);
  }  else {
    mlc_printf("IO Error\n");
    status = pio_inbyte( REG_ERROR );
    mlc_printf("Error reg: %u\n",status);
    for(;;);
  }

  return(0);
}
