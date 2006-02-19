#ifndef _IPODHW_H_
#define _IPODHW_H_

#include "bootloader.h"

#define IPOD_PP5002_RTC 0xCF001110
#define IPOD_PP5020_RTC 0x60005010
#define IPOD_PP5002_LCD_BASE    0xC0001000
#define IPOD_PP5020_LCD_BASE    0x70003000

#define SYSINFO_TAG     (unsigned char *)0x40017f18
#define SYSINFO_PTR     (struct sysinfo_t **)0x40017f1c
#define SYSINFO_TAG_PP5022      (unsigned char *)0x4001ff18
#define SYSINFO_PTR_PP5022      (struct sysinfo_t **)0x4001ff1c

#define IPOD_PP5002_IDE_PRIMARY_BASE         0xC00031E0
#define IPOD_PP5002_IDE_PRIMARY_CONTROL      0xC00033F8
#define IPOD_PP5020_IDE_PRIMARY_BASE         0xC30001E0
#define IPOD_PP5020_IDE_PRIMARY_CONTROL      0xC30003F8

#define IPOD_LCD_FORMAT_2BPP   0x00
#define IPOD_LCD_FORMAT_RGB565 0x01

/* bitmasks for 4g+; SCRL and SCRR are my own invention. */
#define IPOD_KEYPAD_SCRL   0x80
#define IPOD_KEYPAD_SCRR   0x40
#define IPOD_KEYPAD_MENU   0x10
#define IPOD_KEYPAD_PLAY   0x08
#define IPOD_KEYPAD_PREV   0x04
#define IPOD_KEYPAD_NEXT   0x02
#define IPOD_KEYPAD_ACTION 0x01

/* buttons returned by keypad_getkey() */
#define IPOD_KEY_NONE    0
#define IPOD_KEY_UP      1
#define IPOD_KEY_DOWN    2
#define IPOD_KEY_SELECT  3

typedef struct {
  uint32 hw_rev;
  uint32 lcd_base,lcd_height,lcd_width,lcd_type,lcd_busy_mask,lcd_format;
  uint32 rtc;
  uint32 ide_base,ide_control;

  uint32 mem_base,mem_size;
} ipod_t;

void    ipod_init_hardware(void);
ipod_t *ipod_get_hwinfo(void);

#endif
