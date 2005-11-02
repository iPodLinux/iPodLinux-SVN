#ifndef _HOTDOG_CANVAS_H_
#define _HOTDOG_CANVAS_H_

#include "hotdog.h"

hd_canvas *HD_Canvas_Create(uint32 width,uint32 height);
void       HD_Canvas_Destroy(hd_canvas *canvas);
void       HD_Canvas_Render(hd_engine *eng,hd_object *obj);

#endif
