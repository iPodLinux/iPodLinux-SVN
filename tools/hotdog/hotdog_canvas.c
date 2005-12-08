#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hotdog.h"
#include "hotdog_canvas.h"

hd_object *HD_Canvas_Create(uint32 width,uint32 height) {
  hd_object *ret;

  ret = (hd_object *)malloc( sizeof(hd_object) );
  assert(ret != NULL);
  ret->type = HD_TYPE_CANVAS;
  ret->sub.canvas = (hd_canvas *)malloc( sizeof(hd_canvas) );
  assert(ret->sub.canvas != NULL);

  ret->sub.canvas->argb = (uint32 *)malloc( width * height * 4 );
  assert(ret->sub.canvas->argb != NULL);

  ret->sub.canvas->w = width;
  ret->sub.canvas->h = height;
  ret->render = HD_Canvas_Render;
  ret->destroy = HD_Canvas_Destroy;

  return(ret);
}

void     HD_Canvas_Destroy(hd_object *obj) {
  if( obj->sub.canvas->argb != NULL) free(obj->sub.canvas->argb);

  free(obj->sub.canvas);
}

void HD_Canvas_Render(hd_engine *eng,hd_object *obj) {
    HD_ScaleBlendClip (obj->sub.canvas->argb, obj->sub.canvas->w, obj->sub.canvas->h,
                       eng->buffer, eng->screen.width, eng->screen.height,
                       obj->x, obj->y, obj->w, obj->h);
}
