#ifndef _HOTDOG_ANIMATION_H_
#define _HOTDOG_ANIMATION_H_

#include "hotdog.h"

#define HD_ANIM_X      0x00
#define HD_ANIM_Y      0x01
#define HD_ANIM_WIDTH  0x02
#define HD_ANIM_HEIGHT 0x03

void HD_Animate(hd_engine *eng,hd_object *obj,uint32 property,int32 dest,uint32 time);

#endif
