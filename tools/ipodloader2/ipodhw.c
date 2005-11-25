#include "bootloader.h"
#include "minilibc.h"

#define IPOD_PP5020_RTC 0x60005010

#define SYSINFO_TAG     (unsigned char *)0x40017f18
#define SYSINFO_PTR     (struct sysinfo_t **)0x40017f1c

#define SYSINFO_TAG_PP5022      (unsigned char *)0x4001ff18
#define SYSINFO_PTR_PP5022      (struct sysinfo_t **)0x4001ff1c


static uint32 ipod_rtc_reg = IPOD_PP5020_RTC;
static struct sysinfo_t ipod_sys_info;
static int ipod_sys_info_set = 0;

uint32 system_rev;

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

void ipod_wait_usec(uint32 usecs) {
  uint32 start;
  
  start = inl(ipod_rtc_reg);
  
  while( inl(ipod_rtc_reg) <= (start+usecs) );
}

int ipod_is_pp5022(void) {
  return (inl(0x70000000) << 8) >> 24 == '2';
}

void ipod_set_sys_info(void) {
  if (!ipod_sys_info_set) {
    unsigned sysinfo_tag = (unsigned)SYSINFO_TAG;
    struct sysinfo_t ** sysinfo_ptr = SYSINFO_PTR;
    
    if (ipod_is_pp5022()) {
      sysinfo_tag = (unsigned)SYSINFO_TAG_PP5022;
      sysinfo_ptr = SYSINFO_PTR_PP5022;
    }
    
    if (*(unsigned *)sysinfo_tag == *(unsigned *)"IsyS" 
	&& (*(struct sysinfo_t **)sysinfo_ptr)->IsyS ==  *
	(unsigned *)"IsyS" ) {
      mlc_memcpy(&ipod_sys_info, *sysinfo_ptr, sizeof(struct sysinfo_t));
      ipod_sys_info_set = 1;
      /* magic length based on newer ipod nano sysinfo */
      if (ipod_sys_info.len == 0xf8) {
	system_rev = ipod_sys_info.sdram_zero2;
      } else {
	system_rev = ipod_sys_info.boardHwSwInterfaceRev;
      }
    }
    else {
      ipod_sys_info_set = -1;
    }
  }
}

uint32 ipod_get_hwrev(void) {
  if (!ipod_sys_info_set)
    ipod_set_sys_info();

  return( system_rev );
}


/* get current usec counter */
int timer_get_current() {
  return inl(ipod_rtc_reg);
}

/* check if number of seconds has past */
int timer_check(int clock_start, int usecs) {
  if ((inl(ipod_rtc_reg) - clock_start) >= usecs) {
    return 1;
  } else {
    return 0;
  }
}
