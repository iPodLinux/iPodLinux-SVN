#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hotdog.h"

hd_object *HD_Canvas_Create(uint32 width,uint32 height) {
  return HD_Canvas_CreateFrom (HD_NewSurface (width, height));
}

hd_object *HD_Canvas_CreateFrom (hd_surface srf) {
  hd_object *ret;

  ret = HD_New_Object();
  ret->type = HD_TYPE_CANVAS;
  ret->canvas = srf;
  assert(ret->canvas != NULL);

  /* Check alpha values, set speed appropriately. */
  ret->speed = HD_SPEED_NOALPHA | HD_SPEED_BINALPHA;
  uint32 *pp = HD_SRF_PIXELS (ret->canvas);
  while (pp < HD_SRF_END (ret->canvas)) {
      if ((*pp >> 24) != 0xff) {
          ret->speed &= ~HD_SPEED_NOALPHA;
          if ((*pp >> 24) != 0) ret->speed &= ~HD_SPEED_BINALPHA;
      }
      pp++;
  }
  
  ret->natw = ret->w = HD_SRF_WIDTH (srf);
  ret->nath = ret->h = HD_SRF_HEIGHT (srf);
  ret->render = HD_Canvas_Render;
  ret->destroy = HD_Canvas_Destroy;

  return(ret);
}

void     HD_Canvas_Destroy(hd_object *obj) {
  if (obj->canvas != NULL) free (obj->canvas);
}

void HD_Canvas_Render(hd_engine *eng,hd_object *obj, int x, int y, int w, int h) {
    HD_ScaleBlendClip (obj->canvas, x, y, w, h,
                       eng->buffer, obj->x, obj->y, obj->w, obj->h, obj->speed);
}
