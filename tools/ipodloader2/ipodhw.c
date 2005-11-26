#include "bootloader.h"
#include "ipodhw.h"
#include "minilibc.h"

#define LCD_DATA 0x10
#define LCD_CMD  0x08

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

static ipod_t    ipod;
static struct sysinfo_t ipod_sys_info;


static int ipod_is_pp5022(void) {
  return (inl(0x70000000) << 8) >> 24 == '2';
}

static void ipod_set_sysinfo(void) {
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

    /* magic length based on newer ipod nano sysinfo */
    if (ipod_sys_info.len == 0xf8) {
      ipod.hw_rev = ipod_sys_info.sdram_zero2; 
    } else {
      ipod.hw_rev = ipod_sys_info.boardHwSwInterfaceRev; 
    }
  }
}

/* get current usec counter */
static int timer_get_current(void) {
  return inl(ipod.rtc);
}

/* check if number of useconds has past */
static int timer_check(int clock_start, int usecs) {
  unsigned long clock;
  clock = inl(ipod.rtc);
  
  if ( (clock - clock_start) >= usecs ) {
    return 1;
  } else {
    return 0;
  }
}


/* wait for LCD with timeout */
static void lcd_wait_write(void) {
  if ((inl(ipod.lcd_base) & ipod.lcd_busy_mask) != 0) {
    int start = timer_get_current();
    
    do {
      if ((inl(ipod.lcd_base) & ipod.lcd_busy_mask) == 0) break;
    } while (timer_check(start, 1000) == 0);
  }
}


/* send LCD data */
static void lcd_send_data(int data_lo, int data_hi) {
  lcd_wait_write();
  if( (ipod.hw_rev>>16) == 0x7 ) {
    outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
    outl(data_hi | (data_lo << 8) | 0x760000, 0x70003008);
  } else {
    outl(data_lo, ipod.lcd_base + LCD_DATA);
    lcd_wait_write();
    outl(data_hi, ipod.lcd_base + LCD_DATA);
  }
}


/* send LCD command */
static void lcd_prepare_cmd(int cmd) {
  lcd_wait_write();
  if( (ipod.hw_rev>>16) == 0x7) {
    outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
    outl(cmd | 0x740000, 0x70003008);
  } else {
    outl(0x0, ipod.lcd_base + LCD_CMD);
    lcd_wait_write();
    outl(cmd, ipod.lcd_base + LCD_CMD);
  }
}

static void lcd_cmd_and_data(int cmd, int data_lo, int data_hi) {
  lcd_prepare_cmd(cmd);
  lcd_send_data(data_lo, data_hi);
}


static void ipod_set_backlight(int on) {

  if((ipod.hw_rev>>16) >= 0x4) {
    if ((ipod.hw_rev>>16) == 0x5 || (ipod.hw_rev>>16) == 0x6) {
      if (on) {
	/* brightness full */
	outl(0x80000000 | (0xff << 16), 0x7000a010);
	
	/* set port B03 on */
	outl(((0x100 | 1) << 3), 0x6000d824);
      } else {
	/* fades backlght off on 4g */
	/* GPO D01 disable */
	outl(inl(0x70000084) & ~0x2000000, 0x70000084);
	outl(0x80000000, 0x7000a010);
      }
    } else if ((ipod.hw_rev>>16) == 0x04 || (ipod.hw_rev>>16) == 0x7) {
      /* set port B03 */
      outl(((0x100 | (on ? 1 : 0)) << 3), 0x6000d824);
    } else if ( ((ipod.hw_rev>>16) == 0xc) || ((ipod.hw_rev>>16) == 0xb) ) {
      /* set port B03 */
      outl(((0x100 | (on ? 1 : 0)) << 3), 0x6000d824);
      /* set port L07 */
      outl(((0x100 | (on ? 1 : 0)) << 7), 0x6000d12c);
    }
  } else {
    int lcd_state;
    
    lcd_state = inl(IPOD_PP5002_LCD_BASE);
    if (on) {
      lcd_state = lcd_state | 0x2;
    }
    else {
      lcd_state = lcd_state & ~0x2;
    }
    outl(lcd_state, IPOD_PP5002_LCD_BASE);
  }
  
  if ((ipod.hw_rev>>16) < 0x6 || (ipod.hw_rev>>16) == 0x7) {
    if (on) {
      /* display control (1 00 0 1) */
      /* GSH=01 -> 2/3 level grayscale control */
      /* GSL=00 -> 1/4 level grayscale control */
      /* REV=0 -> don't reverse */
      /* D=1 -> display on */
      if ((ipod.hw_rev>>16) < 3) {
	/* REV=1 */
	lcd_cmd_and_data(0x7, 0x0, 0x11 | 0x2);
      }
      else {
	lcd_cmd_and_data(0x7, 0x0, 0x11);
      }
    }
    else {
      /* display control (10 0 1) */
      /* GSL=10 -> 2/4 level grayscale control */
      /* REV=0 -> don't reverse */
      /* D=1 -> display on */
      lcd_cmd_and_data(0x7, 0x0, 0x9);
    }
  }
}


ipod_t *ipod_get_hwinfo(void) {
  return(&ipod);
}

void ipod_init_hardware(void) {
  ipod_set_sysinfo();

  if( (ipod.hw_rev>>16) == 0xC ) { // Nano
    ipod.lcd_base      = 0x70008A0C;
    ipod.lcd_busy_mask = 0x80000000;
    ipod.lcd_width     = 176;
    ipod.lcd_height    = 132;
    ipod.lcd_type      = 1;
    ipod.lcd_format    = IPOD_LCD_FORMAT_RGB565;
    ipod.rtc           = IPOD_PP5020_RTC;
    ipod.ide_base      = IPOD_PP5020_IDE_PRIMARY_BASE;
    ipod.ide_control   = IPOD_PP5020_IDE_PRIMARY_CONTROL;
    ipod.mem_base      = 0x10000000;
    ipod.mem_size      = 0x2000000;
  } else if( (ipod.hw_rev >> 16) == 0xB ) { // 5th Gen  (Video)
    ipod.lcd_base      = 0x70008A0C;
    ipod.lcd_busy_mask = 0x80000000;
    ipod.lcd_width     = 320;
    ipod.lcd_height    = 240;
    ipod.lcd_type      = 5;
    ipod.lcd_format    = IPOD_LCD_FORMAT_RGB565;
    ipod.rtc           = IPOD_PP5020_RTC;
    ipod.ide_base      = IPOD_PP5020_IDE_PRIMARY_BASE;
    ipod.ide_control   = IPOD_PP5020_IDE_PRIMARY_CONTROL;
    ipod.mem_base      = 0x10000000;
    ipod.mem_size      = 0x2000000;  // 0x4000000 for 60GB model.. Maybe a check?
  } else if( (ipod.hw_rev>>16) == 0x6 ) { // Photo / Color
    ipod.lcd_base      = 0x70008A0C;
    ipod.lcd_busy_mask = 0x80000000;
    ipod.lcd_width     = 220;
    ipod.lcd_height    = 176;
    ipod.lcd_format    = IPOD_LCD_FORMAT_RGB565;
    ipod.rtc           = IPOD_PP5020_RTC;
    ipod.ide_base      = IPOD_PP5020_IDE_PRIMARY_BASE;
    ipod.ide_control   = IPOD_PP5020_IDE_PRIMARY_CONTROL;
    ipod.mem_base      = 0x10000000;
    ipod.mem_size      = 0x2000000;

    if(ipod.hw_rev == 0x60000) ipod.lcd_type = 0;
    else { // Detect the LCD type
      int gpio_a01, gpio_a04;
	    
      /* A01 */
      gpio_a01 = (inl(0x6000D030) & 0x2) >> 1;
      /* A04 */
      gpio_a04 = (inl(0x6000D030) & 0x10) >> 4;
      
      if (((gpio_a01 << 1) | gpio_a04) == 0 || ((gpio_a01 << 1) | gpio_a04) == 2) {
        ipod.lcd_type = 0;
      } else {
        ipod.lcd_type = 1;
      }
    }
  } else if( (ipod.hw_rev>>16) == 0x5 ) { // 4G
    ipod.lcd_base      = IPOD_PP5020_LCD_BASE;
    ipod.lcd_busy_mask = 0x8000;
    ipod.lcd_width     = 160;
    ipod.lcd_height    = 128;
    ipod.lcd_type      = 0;
    ipod.lcd_format    = IPOD_LCD_FORMAT_2BPP;
    ipod.rtc           = IPOD_PP5020_RTC;
    ipod.ide_base      = IPOD_PP5020_IDE_PRIMARY_BASE;
    ipod.ide_control   = IPOD_PP5020_IDE_PRIMARY_CONTROL;
    ipod.mem_base      = 0x10000000;
    ipod.mem_size      = 0x2000000;
  } else if( ((ipod.hw_rev>>16) == 0x4) || 
	     ((ipod.hw_rev>>16) == 0x7) ) { // Mini / Mini 2G
    ipod.lcd_base      = IPOD_PP5020_LCD_BASE;
    ipod.lcd_busy_mask = 0x8000;
    ipod.lcd_width     = 138;
    ipod.lcd_height    = 110;
    ipod.lcd_type      = 0;
    ipod.lcd_format    = IPOD_LCD_FORMAT_2BPP;
    ipod.rtc           = IPOD_PP5020_RTC;
    ipod.ide_base      = IPOD_PP5020_IDE_PRIMARY_BASE;
    ipod.ide_control   = IPOD_PP5020_IDE_PRIMARY_CONTROL;
    ipod.mem_base      = 0x10000000;
    ipod.mem_size      = 0x2000000;
  } else if( (ipod.hw_rev>>16) < 0x4 ) { // 3G / 2G / 1G
    ipod.lcd_base      = IPOD_PP5002_LCD_BASE;
    ipod.lcd_busy_mask = 0x8000;
    ipod.lcd_width     = 160;
    ipod.lcd_height    = 128;
    ipod.lcd_type      = 0;
    ipod.lcd_format    = IPOD_LCD_FORMAT_2BPP;
    ipod.rtc           = IPOD_PP5002_RTC;
    ipod.ide_base      = IPOD_PP5002_IDE_PRIMARY_BASE;
    ipod.ide_control   = IPOD_PP5002_IDE_PRIMARY_CONTROL;
    ipod.mem_base      = 0x28000000;
    ipod.mem_size      = 0x2000000;
  }

  ipod_set_backlight(1);
}

/*uint32 ipod_get_hwrev(void) {
  return(ipod.hw_rev);
  }*/
