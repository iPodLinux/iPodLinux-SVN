#ifndef _HOTDOG_ANIMATION_H_
#define _HOTDOG_ANIMATION_H_

#include "hotdog.h"

void HD_AnimateLinear (hd_object *obj, int sx, int sy, int sw, int sh,
                       int dx, int dy, int dw, int dh, int frames, void (*done)(hd_object *));
void HD_AnimateCircle (hd_object *obj, int x, int y, int r, int32 fbot, int32 ftop,
                       int astart, int adist, int frames);
int32 fsin (int32 angle); // angle is in units of 1024 per pi/2 radians - that is, rad*2048/pi
                          // ret is a 16.16 fixpt - 0x10000 is 1, 0x0000 is 0
int32 fcos (int32 angle); // same

#endif
