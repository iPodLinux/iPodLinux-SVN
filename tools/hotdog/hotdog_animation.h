#ifndef _HOTDOG_ANIMATION_H_
#define _HOTDOG_ANIMATION_H_

#include "hotdog.h"

void HD_AnimateLinear (hd_object *obj, int sx, int sy, int sw, int sh,
                       int dx, int dy, int dw, int dh, int frames, void (*done)(hd_object *));
void HD_AnimateCircle (hd_object *obj, int x, int y, int r, int32 fbot, int32 ftop,
                       int astart, int adist, int frames);

#endif
