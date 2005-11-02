#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hotdog.h"
#include "hotdog_animation.h"

void HD_Animate(hd_engine *eng,hd_object *obj,uint32 property,int32 dest,uint32 time) {
  assert(obj != NULL);
  
  if( (time <= eng->currentTime) ) {
    switch(property) {
    case HD_ANIM_X:
      obj->x = dest;
      break;
    case HD_ANIM_Y:
      obj->y = dest;
      break;
    case HD_ANIM_WIDTH:
      obj->w = dest;
      break;
    case HD_ANIM_HEIGHT:
      obj->h = dest;
      break;
    }
    return;
  }

  
}
