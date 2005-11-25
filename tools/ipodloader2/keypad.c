#include "bootloader.h"
#include "ipodhw.h"

int keypad_getstate(void) {
  uint32 in,st,button;

begin:
  ipod_wait_usec(15*1000);

  //if( !(inl(0x6000d030)&0x20) ) return 'h';
  
  in = inl(0x7000C140);
  
  st = (in & 0xFF000000)>>24;
  if( (st != 0xC0) && (st != 0x80) ) goto begin;

  button = (in & 0x00001F00) >> 8;

  // IAK
  outl(0,0x7000C180);

  outl( inl(0x7000C104) | 0xC0000000, 0x7000C104 );
  outl(0x400A1F00, 0x7000C100);
  
  outl( inl(0x6000D024) | 0x10, 0x6000D024 );

  return(button);
}


void keypad_init(void) {
  // IAK
  outl(0,0x7000C180);

  outl( inl(0x7000C104) | 0xC0000000, 0x7000C104 );
  outl(0x400A1F00, 0x7000C100);
  
  outl( inl(0x6000D024) | 0x10, 0x6000D024 );
}
