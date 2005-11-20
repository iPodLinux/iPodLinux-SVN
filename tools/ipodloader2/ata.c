#include "bootloader.h"
#include "console.h"
#include "minilibc.h"

#include "ata.h"

unsigned int pio_base_addr1,pio_base_addr2;
unsigned int pio_reg_addrs[10];

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
 pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
 pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
 pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
 pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
 pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
 pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
 pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
 pio_inbyte(CB_ASTAT); pio_inbyte(CB_ASTAT); \
}

blockdev_t blockdev;

void reg_reset(void) {
  unsigned char devCtrl;

  devCtrl = CB_DC_HD15 | CB_DC_NIEN;
  
  pio_outbyte( CB_DC, devCtrl | CB_DC_SRST );
  DELAY400NS;
  pio_outbyte( CB_DC, devCtrl );
  DELAY400NS;

  //printf("Waiting for BSY to lower\n");
  while( pio_inbyte(CB_STAT) & CB_STAT_BSY );
}

uint32 ata_init(uint32 base) {
  unsigned int devCtrl;
  unsigned char sc,sn,status;

  //printf("ATA Init\n");

  pio_base_addr1 = base;
  pio_base_addr2 = pio_base_addr1 + 0x200;

  pio_reg_addrs[ CB_DATA ] = pio_base_addr1 + 0 * 4;
  pio_reg_addrs[ CB_FR   ] = pio_base_addr1 + 1 * 4;
  pio_reg_addrs[ CB_SC   ] = pio_base_addr1 + 2 * 4;
  pio_reg_addrs[ CB_SN   ] = pio_base_addr1 + 3 * 4;
  pio_reg_addrs[ CB_CL   ] = pio_base_addr1 + 4 * 4;
  pio_reg_addrs[ CB_CH   ] = pio_base_addr1 + 5 * 4;
  pio_reg_addrs[ CB_DH   ] = pio_base_addr1 + 6 * 4;
  pio_reg_addrs[ CB_CMD  ] = pio_base_addr1 + 7 * 4;
  pio_reg_addrs[ CB_DC   ] = pio_base_addr2 + 6 * 4;
  pio_reg_addrs[ CB_DA   ] = pio_base_addr2 + 7 * 4;

  // Check for existance of DEV0
  pio_outbyte( CB_DH,CB_DH_DEV0 );
  DELAY400NS;
  pio_outbyte( CB_SC, 0x55 );
  pio_outbyte( CB_SN, 0xAA );
  pio_outbyte( CB_SC, 0xAA );
  pio_outbyte( CB_SN, 0x55 );
  pio_outbyte( CB_SC, 0x55 );
  pio_outbyte( CB_SN, 0xAA );
  sc = pio_inbyte( CB_SC );
  sn = pio_inbyte( CB_SN );
  if( (sc==0x55) && (sn==0xAA) ) {  // There is probably a device
    //printf("Found something at DEV0\n");
  } else {
    return(0x01);
    //printf("No such ATA..\n");
    //return;
  }

  // Do a device identify
  devCtrl = CB_DC_HD15 | CB_DC_NIEN;
  
  pio_outbyte( CB_DC, devCtrl );
  pio_outbyte( CB_FR, 0 );
  pio_outbyte( CB_SC, 0 );
  pio_outbyte( CB_SN, 0 );
  pio_outbyte( CB_CL, 0 );
  pio_outbyte( CB_CH, 0 );
  pio_outbyte( CB_DH, CB_DH_DEV0 );

  pio_outbyte( CB_CMD, CMD_IDENTIFY_DEVICE ); // Execute. Drive sets BSY
  DELAY400NS;

  // !!! Do some polling here for the BSY flag without reading the primary status register
  while( pio_inbyte( CB_ASTAT ) & CB_STAT_BSY );

  status = pio_inbyte( CB_STAT );
  if( status & CB_STAT_DRQ ) {
    unsigned short buffer[512];
    unsigned char *cbuff;
    unsigned int  drqByte,c;
    
    for(drqByte=0;drqByte<256;drqByte++) {
      buffer[ drqByte] = pio_inword( CB_DATA );
    }

    console_putchar('M');
    console_putchar('i');
    console_putchar('d');
    console_putchar(':');

    cbuff = (unsigned char *)buffer;
    
    for(c=27;c<47;c++) {
      if( buffer[c] != ((' ' << 8) + ' ') ) {
	console_putchar(buffer[c]>>8);
	console_putchar(buffer[c]&0xFF);
      }
    }
    console_putchar('\n');

    status = pio_inbyte( CB_STAT );
    console_putchar('D');
    console_putchar('R');
    console_putchar('Q');
    console_putchar(':');
    if(status & CB_STAT_DRQ)
      console_putchar('1');
    else
      console_putchar('0');
    console_putchar('\n');

    if( buffer[49] & (1<<9) ) { // Bit 9, endian fixed
    } else {
      console_putchar('n');
      console_putchar('o');
      console_putchar(' ');
    }
    console_putchar('L');
    console_putchar('B');
    console_putchar('A');
    console_putchar('\n');
    
    blockdev.sectors = (buffer[61] << 16) + buffer[60];

#if 0
    //console_puts("Secto");
    if( sectors > 78125000) {
      console_putchar('q');
    } else {
      console_putchar('w');

      //sectors = 1234567;

      i = 1000000000;
      while(i>=1) {
	v = sectors / i;
	console_putchar('0' + v);
	sectors %= i;
	i = i / 10;
      }


    }

    console_putchar('\n');
#endif
  }

  if( pio_inbyte( CB_ASTAT ) & CB_STAT_DRQ ) {
    console_putchar('D');
    console_putchar('r');
    console_putchar('Q');
    console_putchar('!');
    console_putchar('!');
    console_putchar('\n');
  }

  return(0x00);
}

int blockdev_read(void *dst,uint32 sector) {
  unsigned int    devCtrl;
  unsigned int    drqByte,read;
  unsigned short *buffer;
  unsigned char   cl,ch,sn,dh,status;

  // Make sure the drive isn't already busy
  if( pio_inbyte( CB_ASTAT ) & CB_STAT_BSY ) return(-1);

  // Wait until it's ready
  while( !(pio_inbyte( CB_ASTAT ) & CB_STAT_RDY) );

  sn =  sector & 0xFF;
  cl = (sector & 0xFF00) >> 8; 
  ch = (sector & 0xFF0000) >> 16;
  dh = (sector & 0x0F000000) >> 24;

  sn = 0;
  cl = 0;
  ch = 0;
  dh = 0;

  devCtrl = CB_DC_HD15 | CB_DC_NIEN;

  read    = 0;
  //  while(!read) {

    // Maybe a reset here?
    mlc_printf(".");
  
    pio_outbyte( CB_DC, devCtrl );
    pio_outbyte( CB_FR, 0  );
    pio_outbyte( CB_SC, 1  );  // One sector only  (SC/SN flipped?)
    pio_outbyte( CB_SN, sn );
    pio_outbyte( CB_CL, cl );
    pio_outbyte( CB_CH, ch );
    pio_outbyte( CB_DH, CB_DH_DEV0 | dh );
    
    pio_outbyte( CB_CMD, CMD_READ_MULTIPLE ); // Execute..
    DELAY400NS; // Delay a bit to allow drive to set BSY
    
    while( pio_inbyte( CB_ASTAT ) & CB_STAT_BSY );
    
    drqByte = 0;
    
    status = pio_inbyte( CB_STAT );
    if( status & CB_STAT_DRQ ) {  
      
      read = 1;
      
      buffer = (unsigned short *)dst;
      
      for(drqByte=0;drqByte<256;drqByte++) {
	buffer[ drqByte] = pio_inword( CB_DATA );
      }
    }
    //  }

  if(!read) return(-2);

  return(drqByte);
}
