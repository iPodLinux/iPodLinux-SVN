#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hotdog.h"

void HD_Canvas_CreateAtFrom (hd_object *ret, hd_surface srf) {
  HD_NewObjectAt (ret);
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
}

hd_object *HD_Canvas_Create(uint32 width,uint32 height) {
  return HD_Canvas_CreateFrom (HD_NewSurface (width, height));
}

void HD_Canvas_CreateAt (hd_object *obj, uint32 width, uint32 height) 
{
    HD_Canvas_CreateAtFrom (obj, HD_NewSurface (width, height));
}

hd_object *HD_Canvas_CreateFrom (hd_surface srf) 
{
    hd_object *ret = malloc (sizeof(hd_object));
    HD_Canvas_CreateAtFrom (ret, srf);
    return ret;
}

void     HD_Canvas_Destroy(hd_object *obj) {
  if (obj->canvas != NULL) free (obj->canvas);
}

void HD_Canvas_Render (hd_object *obj, hd_surface srf, int dxoff, int dyoff) {
    if (!obj->opacity) return;
    
    HD_ScaleBlendClip (obj->canvas, 0, 0, obj->natw, obj->nath,
                       srf, obj->x + dxoff, obj->y + dyoff, obj->w, obj->h,
                       obj->speed, obj->opacity);
}

void HD_Canvas_RenderPart (hd_object *obj, hd_surface srf, int psx, int psy, int psw, int psh,
                           int dxoff, int dyoff) 
{
    if (!obj->opacity) return;

    int32 fx, fy;
    fx = (obj->w == obj->natw)? 0x10000 : ((obj->natw << 16) / obj->w);
    fy = (obj->h == obj->nath)? 0x10000 : ((obj->nath << 16) / obj->h);
    HD_ScaleBlendClip (obj->canvas, (psx * fx) >> 16, (psy * fy) >> 16, (psw * fx) >> 16, (psh * fy) >> 16,
                       srf, obj->x + dxoff + psx, obj->y + dyoff + psy, psw, psh,
                       obj->speed, obj->opacity);
}
