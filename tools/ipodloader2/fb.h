#ifndef _FB_H_
#define _FB_H_

#include "bootloader.h"

// commonly used colors:
#define BLACK 0xFFFF
#define WHITE 0x0000
#define GREEN fb_rgb(0,255,0)
#define BLUEISH fb_rgb(0,0,192)
#define MENU_HILIGHT fb_rgb(0,128,128)

void fb_init(void);

void fb_update(uint16 *x);
void fb_cls(uint16 *x,uint16 val);
uint16 fb_rgb(uint8 r, uint8 g, uint8 b); // takes values between 0 and 255

#endif
