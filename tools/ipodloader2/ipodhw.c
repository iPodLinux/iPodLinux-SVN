#include "bootloader.h"

#define IPOD_PP5020_RTC 0x60005010

static uint32 ipod_rtc_reg = IPOD_PP5020_RTC;

void ipod_wait_usec(uint32 usecs) {
  uint32 start;
  
  start = inl(ipod_rtc_reg);
  
  while( inl(ipod_rtc_reg) <= (start+usecs) );
}

uint32 ipod_get_hwrev(void) {
  uint32 rev;

  if( inl(0x2000) == (uint32)"gfCS" ) {
    rev = inl(0x2084);
  } else {
    rev = inl(0x405C);
  }

  return(rev);
}
