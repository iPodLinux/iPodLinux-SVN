#ifndef _HOTDOG_FONT_H_
#define _HOTDOG_FONT_H_

#include "hotdog.h"

hd_font *HD_Font_LoadHDF (const char *filename);
int      HD_Font_SaveHDF (hd_font *font, const char *filename);
hd_font *HD_Font_LoadFNT (const char *filename);

#endif
