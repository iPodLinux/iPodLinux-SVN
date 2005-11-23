#ifndef _FB_H_
#define _FB_H_

#include "bootloader.h"

void fb_init(int hw_ver);

void fb_update(uint16 *x);
void fb_cls(uint16 *x,uint16 val);

#endif
