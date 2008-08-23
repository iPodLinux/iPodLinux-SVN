/*
 * Copyright (c) 2005, James Jacobsson
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 * Neither the name of the organization nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior
 * written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#ifndef _HOTDOG_H_
#define _HOTDOG_H_

#define HD_TYPE_CANVAS    0x01
#define HD_TYPE_PRIMITIVE 0x02
#define HD_TYPE_BAG       0x03
#define HD_TYPE_OTHER     0xFF

#define HD_PRIM_RECTANGLE 0x00

/****** Basic typedefs ******/
typedef unsigned int   uint32;
typedef   signed int    int32;
typedef unsigned short uint16;
typedef   signed short  int16;
typedef unsigned char  uint8;
typedef   signed char   int8;

/****** Surfaces ******/

/* Surfaces are just things for drawing graphics.
 * However, the actual pixels don't start until (h+2)*4
 * bytes in. The first uint32 is the width, and the second
 * is the height. After that come <h> uint32's, each giving the
 * offset (in 4-byte blocks from the beginning of the surface)
 * of the beginning of the respective row. All pixels are stored
 * in ARGB8888 format. For instance, a 4x4 surface would look like
 * this.
 *
 * [ofs] value
 *   [0] 4
 *   [1] 4
 *   [2] 6   (beginning of row 0)
 *   [3] 10  (beginning of row 1)
 *   [4] 14  (beginning of row 2)
 *   [5] 18  (beginning of row 3)
 *   [6] pixel at (0, 0)
 *   [7] pixel at (0, 1)
 *   [8] pixel at (0, 2)
 *   [9] pixel at (0, 3)
 *  [10] pixel at (1, 0)
 *   ..
 *  [13] pixel at (1, 3)
 *  [14] pixel at (2, 0)
 *   ..
 *  [18] pixel at (3, 0)
 *   ..
 *  [21] pixel at (3, 3)
 */
typedef uint32 *hd_surface;

/* "F" variants are (F)ast and need to be pre-clipped. */
#define HD_SRF_WIDTH(srf) ((srf)[0])
#define HD_SRF_HEIGHT(srf) ((srf)[1])
#define HD_SRF_ROW(srf,y) (((y)<(int)((srf)[1]))?((srf) + ((srf)[2 + (y)])):0)
#define HD_SRF_ROWF(srf,y) ((srf) + ((srf)[2 + (y)]))
#define HD_SRF_PIXF(srf,x,y) ((srf)[((srf)[2 + (y)]) + (x)])
#define HD_SRF_SETPIX(srf,x,y,pix) (((x)>=0)&&((y)>=0)&&((x)<(int)((srf)[0]))&&((y)<(int)((srf)[1]))? (HD_SRF_PIXF(srf,x,y) = (pix)) : (pix))
#define HD_SRF_GETPIX(srf,x,y) (((x)<(int)((srf)[0]))&&((y)<(int)((srf)[1]))? HD_SRF_PIXF(srf,x,y) : 0)
#define HD_SRF_PIXPTR(srf,x,y) (((x)<(int)((srf)[0]))&&((y)<(int)((srf)[1]))? &HD_SRF_PIXF(srf,x,y) : 0)
#define HD_SRF_PIXELS(srf) ((srf) + 2 + ((srf)[1]))
#define HD_SRF_END(srf) ((srf) + 2 + ((srf)[1]) + (((srf)[0]) * ((srf)[1])))


/* HD_SurfaceFrom* will _copy_ the pixels, not just reference them.
 * Thus, you still need to free argb or pixels.
 */
hd_surface HD_SurfaceFromARGB (uint32 *argb, int32 width, int32 height);

/* If [pixels] contains an alpha channel and is not premultiplied,
 * it's YOUR RESPONSIBILITY to call PremultiplyAlpha on the returned
 * surface.
 */
hd_surface HD_SurfaceFromPixels (void *pixels, int32 width, int32 height, int bpp,
                                 uint32 Rmask, uint32 Gmask, uint32 Bmask, uint32 Amask);

/* NewSurface fills the surface with black. */
hd_surface HD_NewSurface (int32 width, int32 height);

void HD_FreeSurface (hd_surface srf); // you can just use free if you want

/* Premultiplies the alpha in [srf]. You must be careful to only call this once. */
void HD_PremultiplyAlpha (hd_surface srf);

/* Flipping and rotating of surfaces.
 * 
 * Constants to be passed to HD_FlipDiagonal:
 *  HD_FLIP_DIAGONAL_UL_LR => flip over ul-lr diagonal.
 *  HD_FLIP_DIAGONAL_UR_LL => flip over ur-ll diagonal.
 *  HD_FLIP_ROTATE_90_CW   => rotate 90° clockwise.
 *  HD_FLIP_ROTATE_90_CCW  => rotate 90° counter-clockwise.
 *
 */
#define HD_FLIP_DIAGONAL_UL_LR 0
#define HD_FLIP_DIAGONAL_UR_LL 1
#define HD_FLIP_ROTATE_90_CW   2
#define HD_FLIP_ROTATE_90_CCW  3

void HD_SurfaceFlipVertical (hd_surface srf);
void HD_SurfaceFlipHorizontal (hd_surface srf);
hd_surface HD_SurfaceFlipDiagonal (hd_surface srf, int direction);
hd_surface HD_SurfaceRotate (hd_surface srf, int degrees);


/****** Fonts ******/

/* There are two commonly-used ways to store font character
 * data in an array of pixels. One, which we'll call `pitched',
 * is to have the surface laid out like a graphics program would
 * if you just typed all the characters in a row; these surfaces
 * have a defined width, which we'll call the `pitch'.
 *
 * The other is to just clump all the pixels for a font right
 * next to each other, as if each character was its own mini-surface
 * and they were laid out right on top of each other or something.
 * This kind is called `clumped', and the pitch has a completely
 * different meaning: it is now the number of bytes a character's
 * in-memory width must be divisible by. For instance, for 32-bit
 * clumped chars this would be 4; for Microwindows FNT files with
 * their 16-bit bitmaps it would be 2; in most other cases it would be 1.
 *
 * SFonts are pitched.
 *
 * FNTs and PCFs are clumped.
 */

#define HD_FONT_CLUMPED  0
#define HD_FONT_PITCHED  1

typedef struct hd_font {
    // Pixel data:
    int     bitstype; // PITCHED or CLUMPED
    int32   pitch, h; // h = height of font; pitch = length of a scanline in bytes
    int32   pixbytes; // number of bytes in `pixels'
    void   *pixels;
    int     bpp; // 1 or 8 or 32

    // Char info:
    int     firstchar, nchars;
    uint32 *offset; // offset[N-firstchar] = X-coord of char N in pixels
    uint8  *width;  // width[N-firstchar]  = width   "    "  "  "    "
    int     defaultchar; // Char to use if req'd char doesn't exist
} hd_font;

hd_font *HD_Font_LoadHDF (const char *filename);
int      HD_Font_SaveHDF (hd_font *font, const char *filename);
hd_font *HD_Font_LoadFNT (const char *filename);
hd_font *HD_Font_LoadPCF (const char *filename);
hd_font *HD_Font_LoadSFont (const char *filename);
hd_font *HD_Font_LoadFFF (const char *filename);

void HD_Font_Free(hd_font *font);
int HD_Font_TextWidth (hd_font *font, const char *str); // UTF-8
int HD_Font_TextWidthLatin1 (hd_font *font, const char *str);
int HD_Font_TextWidthUnicode (hd_font *font, const uint16 *str);
/* TextHeight is just font->h */

/** Draws with the TOP LEFT CORNER at (x,y) */
void HD_Font_Draw (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str);
void HD_Font_DrawLatin1 (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str);
void HD_Font_DrawUnicode (hd_surface srf, hd_font *font, int x, int y, uint32 color, const uint16 *str);
/* Fast = no blending. May have unpredictable results for AA fonts on non-white surfaces.
   Only draws black or white for 8-bpp fonts. */
void HD_Font_DrawFast (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str);
void HD_Font_DrawFastLatin1 (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str);
void HD_Font_DrawFastUnicode (hd_surface srf, hd_font *font, int x, int y, uint32 color, const uint16 *str);

struct hd_object *HD_Font_MakeObject (hd_font *font, uint32 col, const char *str);

/****** Compositing ******/

struct hd_engine;
struct hd_object;

typedef struct hd_rect {
    int x, y, w, h, z;
    struct hd_rect *next; /* used only sometimes */
} hd_rect;

typedef struct hd_pending_animation {
    void (*setup)(struct hd_object *obj);
    void (*animate)(struct hd_object *obj);
    void *animdata;
    struct hd_pending_animation *next;
} hd_pending_animation;

typedef struct hd_object {
    int32 x, y, w, h, z;
    int32 natw, nath; /* natural width and height */
    uint8 opacity;
    uint32 type;
    int animating;
    /* private */ int dirty;
    
    hd_surface   canvas;
    void       *data;
    
    struct hd_engine *eng;
    
    void (*render)(struct hd_object *obj, hd_surface srf, int dxoff, int dyoff);
    void (*renderpart)(struct hd_object *obj, hd_surface srf, int psx, int psy, int psw, int psh, int dxoff, int dyoff); /* optional; psx/psy/etc. are post-scale coordinates */
    void (*destroy)(struct hd_object *obj);
    
    void (*animate)(struct hd_object *obj);
    void *animdata;
    hd_pending_animation *pending_animations;

    /* readonly */ hd_rect preanim, postanim;

    /* Only for simple "blit-it-and-forget-it" objects: */
#define HD_SPEED_NOALPHA   1   /* every pixel is opaque, alpha ignored - major speedup */
#define HD_SPEED_BINALPHA  2   /* any nonzero alpha is like 0xFF - medium speedup */
#define HD_SPEED_NOSCALE   4   /* it's always a 1:1 scale - minor speedup */
#define HD_SPEED_CSPRITE   8   /* compile the sprite - ignored currently, would be major speedup if it was impl */
    int speed;

    int dontfree; // Object isn't malloc()ed, don't free() it

    /* private */ hd_rect last;
} hd_object;

typedef struct hd_obj_list {
    hd_object *obj;
    
    struct hd_obj_list *next;
} hd_obj_list;

typedef struct hd_engine {
    struct {
        union {
            hd_rect rect;
            struct {
                int __r0, __r1;
                int width, height;
            };
        };
        uint16 *framebuffer;
        uint8  *fb2bpp;
        void (*update)(struct hd_engine *, int, int, int, int);
    } screen;
    
    uint32      currentTime;
    hd_surface  buffer;
    
    hd_obj_list *list;
    hd_rect *deregistered; // Rects on here are used for dirties once, then deleted.
} hd_engine;

/**** Optimized ops ****/

/* The "idst ^= 0xFF000000; isrc &= ~0xFF000000;" line makes the alphas composite correctly.
 * Witness:
 *                     |                             |
 * light, 100%   -->   |   --> 30% of original -->   |   --> 30%*40%=12% of original
 *                     |                             |
 *                  alpha=70%                     alpha=60%
 *                 transp=30%                    transp=40%
 *
 * It's the *transparencies* that multiply, not the alphas (opacities).
 * The XOR changes opacity to transparency for the calculation, and the mask-out prevents the
 * src alpha channel from getting in there twice.
 */
#define _HD_ALPHA_BLEND(dst_argb, src_argb, ret_argb, opac)         \
{                                                              \
 uint32 __alpha,__dst[2];                                          \
 uint32 __idst = (dst_argb);                                      \
 uint32 __isrc = (src_argb);                                      \
 if ((opac) != 0xff) { \
    __dst[0] = ((__isrc & 0x00FF00FF) * (opac) + 0x00FF00FF) & ~0x00FF00FF; \
    __dst[1] = (((__isrc >> 8) & 0x00FF00FF) * (opac) + 0x00FF00FF) & ~0x00FF00FF; \
    __isrc = (__dst[0]>>8) + __dst[1]; \
 } \
 __alpha = 255 - (__isrc >> 24); \
 __idst ^= 0xFF000000; __isrc &= ~0xFF000000; \
 __dst[0] = ((__idst & 0x00FF00FF) * __alpha + 0x00FF00FF) & ~0x00FF00FF;          \
 __dst[1] = (((__idst>>8) & 0x00FF00FF) * __alpha + 0x008000FF) & ~0x00FF00FF;          \
 __dst[1] ^= 0xFF000000; \
 (ret_argb) = (__dst[0]>>8) + __dst[1] + __isrc;                      \
}

#define BLEND_ARGB8888_ON_ARGB8888(dst_argb, src_argb, opac) _HD_ALPHA_BLEND(dst_argb, src_argb, dst_argb, opac)

#define HD_RGB(r,g,b) (0xff000000 | ((r) << 16) | ((g) << 8) | (b))
#define HD_RGBA(r,g,b,a) (((a) << 24) | \
                          (((((r)*(a)) + 0xFF) >> 8) << 16) | \
                          (((((g)*(a)) + 0xFF)     ) & 0xFF00) | \
                           ((((b)*(a)) + 0xFF) >> 8))
#define HD_RGBAM(mr,mg,mb,a) (((a) << 24) | ((mr) << 16) | ((mg) << 8) | (mb))
#define HD_MASKPIX(argb,a) ({ \
 uint32 __dst[2]; \
 uint32 __argb = (argb), __a = (a); \
 __dst[0] = ((__argb & 0x00FF00FF) * __a + 0x00800080) & 0xFF00FF00; \
 __dst[1] = (((__argb >> 8) & 0x00FF00FF) * __a + 0x00800080) & 0xFF00FF00; \
 ((__dst[0]>>8) + __dst[1]); \
})

#define HD_SRF_BLENDPIX(srf,x,y,pix) do \
{ \
 uint32 p; \
 _HD_ALPHA_BLEND (HD_SRF_GETPIX (srf,x,y), pix, p, 0xff); \
 HD_SRF_SETPIX (srf,x,y,p); \
} while(0)

/* Rects */
#define HD_SameRect(r1,r2) (memcmp(r1,r2, 5*sizeof(int)) == 0)
#define HD_CopyRect(dstr,srcr) memcpy(dstr, srcr, 5*sizeof(int))
void HD_ClipRect (hd_rect *rect, hd_rect *clip); // clips rect to clip
void HD_ExpandRect (hd_rect *rect, hd_rect *strut); // extends rect to strut

/* Basic funcs */
hd_engine *HD_Initialize(uint32 width,uint32 height,uint8 bpp, void *framebuffer, void (*update)(struct hd_engine*, int, int, int, int));
void HD_Deinitialize(hd_engine *eng);
#define HD_IsRegistered(obj) ((obj)->eng != 0)
void HD_Register(hd_engine *eng, hd_object *obj);
void HD_Deregister (hd_engine *eng, hd_object *obj);
void HD_Render(hd_engine *eng);
void HD_Animate(hd_engine *eng);
void HD_Destroy(hd_object *obj);
#define HD_FreeObject HD_Destroy
#define HD_DestroyObject HD_Destroy
void HD_ScaleBlendClip (hd_surface sbuf, int sx, int sy, int sw, int sh,
                        hd_surface dbuf, int dx, int dy, int dw, int dh,
                        int speed, uint8 opacity);

#define HD_New_Object HD_NewObject
hd_object *HD_NewObject();
void HD_NewObjectAt (hd_object *obj);
hd_obj_list *HD_StackObjects (hd_obj_list *head);

/****** Animation ******/
#define HD_IsAnimating(obj) ((obj)->animating != 0)
void HD_SetAnimation (hd_object *obj, void (*setup)(hd_object *),
                      void (*animate)(hd_object *), void *animdata);
void HD_StopAnimation (hd_object *obj);

#define HD_ANIM_ABSOLUTE  0x10000000 /* OR with frames to make the obj actually move to (sx,sy)swxsh */
#define HD_ANIM_RELATIVE  0x00000000
void HD_AnimateLinear (hd_object *obj, int sx, int sy, int sw, int sh,
                       int dx, int dy, int dw, int dh, int frames, void (*done)(hd_object *));
void HD_AnimateCircle (hd_object *obj, int x, int y, int r, int32 fbot, int32 ftop,
                       int astart, int adist, int frames);
void HD_XAnimateCircle (hd_object *obj, int x, int y, int xr, int yr, int32 fbot, int32 ftop,
                        int astart, int adist, int frames);
int32 fsin (int32 angle); // angle is in units of 1024 per pi/2 radians - that is, rad*2048/pi
                          // ret is a 16.16 fixpt - 0x10000 is 1, 0x0000 is 0
int32 fcos (int32 angle); // same

/****** Canvasses ******/
hd_object *HD_Canvas_Create (uint32 w, uint32 h);
void       HD_Canvas_CreateAt (hd_object *obj, uint32 w, uint32 h);
/* Canvas_CreateFrom() takes ownership of srf; it'll free it when the object is freed. */
hd_object *HD_Canvas_CreateFrom(hd_surface srf);
void       HD_Canvas_Destroy(hd_object *obj);
void       HD_Canvas_Render(hd_object *obj, hd_surface srf, int dxoff, int dyoff);
void       HD_Canvas_RenderPart (hd_object *obj, hd_surface srf, int psx, int psy, int psw, int psh, int dxoff, int dyoff);

/****** PNGs ******/
hd_object *HD_PNG_Create(const char *fname);
hd_surface HD_PNG_Load (const char *fname, int *w, int *h);

/****** JPEGs ******/
#define HD_JPG_Create  HD_JPEG_Create
#define HD_JPG_Load    HD_JPEG_Load
hd_object *HD_JPEG_Create(const char *fname);
hd_surface HD_JPEG_Load (const char *fname, int *w, int *h);

/****** RAWLCD5s ******/
hd_object *HD_RAWLCD5_Create(const char *fname);
hd_surface HD_RAWLCD5_Load (const char *fname, int *w, int *h);

/****** IPDs & ANMs ******/
hd_object *HD_IPD_ANM_Create(const char *fname);
hd_surface HD_IPD_ANM_Load (const char *fname, int *w, int *h);

/****** Primitives ******/
#define HD_CLEAR  0x00FFFFFF // magic value that says "clear this part to 0x00000000, no blending"
typedef struct _hd_point {
	int x, y;
} hd_point;
void HD_Pixel(hd_surface srf, int x, int y, uint32 col);
void HD_Line(hd_surface srf, int x0, int y0, int x1, int y1, uint32 col);
void HD_AALine(hd_surface srf, int x0, int y0, int x1, int y1, uint32 col);
void HD_Lines(hd_surface srf, hd_point *points, int n, uint32 col);
void HD_AALines(hd_surface srf, hd_point *points, int n, uint32 col);
void HD_Poly(hd_surface srf, hd_point *points, int n, uint32 col);
void HD_AAPoly(hd_surface srf, hd_point *points, int n, uint32 col);
void HD_FillPoly(hd_surface srf, hd_point *points, int n, uint32 col);
void HD_Bitmap(hd_surface srf, int x, int y, int w, int h,
		const unsigned short *bits, uint32 col);
void HD_FillRect(hd_surface srf, int x1, int y1, int x2, int y2, uint32 col);
void HD_Rect(hd_surface srf, int x1, int y1, int x2, int y2, uint32 col);
void HD_Circle(hd_surface srf, int x, int y, int r, uint32 col);
void HD_AACircle(hd_surface srf, int xc, int yc, int r, uint32 col);
void HD_FillCircle(hd_surface srf, int x, int y, int r, uint32 col);
void HD_AAFillCircle(hd_surface srf, int xc, int yc, int r, uint32 col);
void HD_AAEllipse(hd_surface srf, int xc, int yc, int rx, int ry, uint32 col);
void HD_Ellipse(hd_surface srf, int x, int y, int rx, int ry, uint32 col);
void HD_FillEllipse(hd_surface srf, int x, int y, int rx, int ry, uint32 col);
void HD_AAFillEllipse(hd_surface srf, int x, int y, int rx, int ry, uint32 col);
void HD_Bezier(hd_surface srf, int order, hd_point *points,
               int resolution, uint32 col);
void HD_AABezier(hd_surface srf, int order, hd_point *points,
                 int resolution, uint32 col);
void HD_Blur(hd_surface srf, int x, int y, int w, int h, int rad);
void HD_Chrome(hd_surface srf, int x, int y, int w, int h);

#ifdef IPOD
/****** LCD ******/
#define HD_LCD_TYPE_PHOTO  0
#define HD_LCD_TYPE_COLOR  1
#define HD_LCD_TYPE_MONO   2
#define HD_LCD_TYPE_MINI2G 3
#define HD_LCD_TYPE_VIDEO  5

#define HD_LCD_BPP(type) ((type == 2 || type == 3)? 2 : 16)

void HD_LCD_Init();
void HD_LCD_GetInfo (int *hw_ver, int *lcd_width, int *lcd_height, int *lcd_type);
void HD_LCD_Update (void *fb, int x, int y, int w, int h);
void HD_LCD_Quit(); // restore to text mode before quitting
#endif

/* safe allocation */
#include <sys/types.h>
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
void *xcalloc(size_t nmemb, size_t size);
char *xstrdup(const char *s);
void xfree(void *ptr); // doesn't do anything special, but feel -free- to use
#define safe_free(x) do { if (x) free(x), x = 0; } while (0)

#endif
