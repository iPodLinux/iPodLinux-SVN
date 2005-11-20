#ifndef _FB_H_
#define _FB_H_

#include "bootloader.h"

void fb_init(int hw_ver);
void fb_bitblt(unsigned short * x, int sx, int sy, int mx, int my);

#endif
