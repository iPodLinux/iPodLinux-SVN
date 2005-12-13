#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hotdog.h"
#include "hotdog_font.h"

#include <ft2build.h>
#include <freetype/freetype.h>

static void HD_Font_RenderGlyph(hd_font *font, FT_Bitmap *ftmap, int32 x,int32 y, int32 w, int32 h ) {
  int32 fx,fy;
  uint8 luma;
  
  if(x<0)x=0; // Silly FT2...

  for(fy=0;fy<ftmap->rows;fy++) {
    for(fx=0;fx<ftmap->width;fx++) {
      luma = (*(uint8 *)(ftmap->buffer + (fy*ftmap->pitch + fx)))&0xFF;

      font->argb[ (fy+y) * font->w + (fx+x) ] = (luma<<24) | (luma<<16) | (luma<<8) | luma;
    }
  }

}

hd_object *HD_Font_Create(char *font,uint32 height,char *text) {
  hd_object *ret;
  hd_font *fsub;
  FT_Library library;
  FT_Face    face;
  int        error;
  int32      xsize,n,x;

  ret = HD_New_Object();
  ret->type = HD_TYPE_FONT;
  ret->sub.font = fsub = (hd_font *)malloc( sizeof(hd_font) );
  assert(font != NULL);

  error = FT_Init_FreeType( &library );
  assert(error == 0);

  error = FT_New_Face( library, font, 0, &face );
  assert(error == 0);

  error = FT_Set_Pixel_Sizes( face, 0, height );
  assert(error == 0);

  xsize = 0;
  for(n=0;n<strlen(text);n++) {
    FT_UInt glyph_index;

    glyph_index = FT_Get_Char_Index( face, text[n] );

    error = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
    assert( error == 0 );

    error = FT_Render_Glyph( face->glyph, ft_render_mode_normal );
    assert( error == 0 );

    xsize += face->glyph->advance.x >> 6;
  }
  xsize++;

  fsub->argb = (uint32 *)malloc( xsize * height * 4 );
  assert(fsub->argb != NULL);

  memset(fsub->argb,0,xsize * height * 4);
  ret->natw = fsub->w = xsize;
  ret->nath = fsub->h = height;

  x = 0;
  for(n=0;n<strlen(text);n++) {
    FT_UInt glyph_index;

    glyph_index = FT_Get_Char_Index( face, text[n] );

    error = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
    assert( error == 0 );

    error = FT_Render_Glyph( face->glyph, ft_render_mode_normal );
    assert( error == 0 );

    HD_Font_RenderGlyph( fsub, &face->glyph->bitmap,
			 x + face->glyph->bitmap_left,(height - face->glyph->bitmap_top),
			 face->glyph->advance.x >> 6,
			 height );

    x += face->glyph->advance.x >> 6;
  }

  ret->destroy = HD_Font_Destroy;
  ret->render = HD_Font_Render;

  return(ret);
}

void     HD_Font_Destroy(hd_object *obj) {
  if( obj->sub.font->argb != NULL) free(obj->sub.font->argb);
  free (obj->sub.font);
}

void HD_Font_Render (hd_engine *eng, hd_object *obj, int x, int y, int w, int h) 
{
    HD_ScaleBlendClip (obj->sub.font->argb, obj->sub.font->w, obj->sub.font->h,
                       x, y, w, h,
                       eng->buffer, eng->screen.width, eng->screen.height,
                       obj->x, obj->y, obj->w, obj->h);
}

