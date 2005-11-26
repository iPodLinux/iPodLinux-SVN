#include "bootloader.h"
#include "ipodhw.h"

static ipod_t *ipod;

int keypad_getstate_opto(void) {
  uint32 in,st,button;

begin:
  //ipod_wait_usec(15*1000);

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

int keypad_getstate(void) {
  uint8 state;

  if( (ipod->hw_rev>>16) >= 5 ) return( keypad_getstate_opto() );
  
  // REW, Menu, PLay, FF

  if( (ipod->hw_rev>>16) < 4 ) {
    state = inb(0xCF000030);
    if(     (state & 0x10) == 0) return(IPOD_KEYPAD_UP);
    else if((state & 0x04) == 0) return(IPOD_KEYPAD_DOWN);
    else if((state & 0x02) == 0) return(IPOD_KEYPAD_OK);
  } else {  // 0x4
    state = inb(0x6000d030);
    if(     (state & 0x02) == 0) return(IPOD_KEYPAD_UP);
    else if((state & 0x04) == 0) return(IPOD_KEYPAD_DOWN);
    else if((state & 0x01) == 0) return(IPOD_KEYPAD_OK);
  }
  return(0);
}

void keypad_init(void) {
  ipod = ipod_get_hwinfo();

  if( (ipod->hw_rev>>16) < 4 ) {
    /* 1G -> 3G Keyboard init */
    outb(~inb(0xcf000030), 0xcf000060);
    outb(inb(0xcf000040), 0xcf000070);
    
    outb(inb(0xcf000004) | 0x1, 0xcf000004);
    outb(inb(0xcf000014) | 0x1, 0xcf000014);
    outb(inb(0xcf000024) | 0x1, 0xcf000024);
    
    outb(0xff, 0xcf000050);
  } else if( (ipod->hw_rev>>16) == 4 ) {
    /* mini keyboard init */
    outl(inl(0x6000d000) | 0x3f, 0x6000d000);
    outl(inl(0x6000d010) & ~0x3f, 0x6000d010);
  } else if( (ipod->hw_rev>>16) >= 5 ) {
    // IAK
    outl(0,0x7000C180);
    
    outl( inl(0x7000C104) | 0xC0000000, 0x7000C104 );
    outl(0x400A1F00, 0x7000C100);
    
    outl( inl(0x6000D024) | 0x10, 0x6000D024 );
  }
}
