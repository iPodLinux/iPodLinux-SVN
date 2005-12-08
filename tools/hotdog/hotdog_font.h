#ifndef _HOTDOG_FONT_H_
#define _HOTDOG_FONT_H_

#include "hotdog.h"

hd_object *HD_Font_Create(char *font,uint32 height,char *text);
void       HD_Font_Destroy(hd_object *obj);
void       HD_Font_Render(hd_engine *eng,hd_object *obj);

#endif
