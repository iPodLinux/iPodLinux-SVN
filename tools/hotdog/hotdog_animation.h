#ifndef _HOTDOG_ANIMATION_H_
#define _HOTDOG_ANIMATION_H_

#include "hotdog.h"

void HD_AnimateLinear (hd_object *obj, int sx, int sy, int sw, int sh,
                       int dx, int dy, int dw, int dh, int frames, void (*done)(hd_object *));

#endif
