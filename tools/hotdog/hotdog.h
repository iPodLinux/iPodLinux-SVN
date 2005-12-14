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

#define HD_TYPE_PRIMITIVE 0x00
#define HD_TYPE_PNG       0x01
#define HD_TYPE_FONT      0x02
#define HD_TYPE_CANVAS    0x03
#define HD_TYPE_USERDEF   0x100

#define HD_PRIM_RECTANGLE 0x00

typedef unsigned int   uint32;
typedef   signed int    int32;
typedef unsigned short uint16;
typedef   signed short  int16;
typedef unsigned char  uint8;
typedef   signed char   int8;

typedef struct hd_primitive {
    uint32 type;
    uint32 color; // ARGB
} hd_primitive;

typedef struct hd_font {
    int32   w,h;
    uint32 *argb;
} hd_font;

typedef struct hd_canvas {
    int32   w,h;
    uint32 *argb;
} hd_canvas;

typedef struct hd_png {
    int32   w,h;
    uint32 *argb;
} hd_png;

struct hd_engine;
struct hd_object;

typedef struct hd_rect {
    int x, y, w, h, z;
    struct hd_rect *next; /* used only sometimes */
} hd_rect;

typedef struct hd_object {
    int32 x, y, w, h, z;
    int32 natw, nath; /* natural width and height */
    uint8 opacity;
    uint32 type;
    int animating;
    /* private */ int dirty;
    
    union {
        hd_png       *png;
        hd_primitive *prim;
        hd_font      *font;
        hd_canvas    *canvas;
        void       *data;
    } sub;
    
    struct hd_engine *eng;
    
    void (*render)(struct hd_engine *eng, struct hd_object *obj, int x, int y, int w, int h);
    void (*destroy)(struct hd_object *obj);
    
    void (*animate)(struct hd_object *obj);
    void *animdata;

    /* private */ hd_rect last;
} hd_object;

typedef struct hd_obj_list {
    hd_object *obj;
    
    struct hd_obj_list *next;
} hd_obj_list;

typedef struct hd_engine {
    struct {
        int32  width,height;
        uint16 *framebuffer;
        uint8  *fb2bpp;
        void (*update)(struct hd_engine *, int, int, int, int);
    } screen;
    
    uint32  currentTime;
    uint32 *buffer;
    
    hd_obj_list *list;
} hd_engine;

#define BLEND_ARGB8888_ON_ARGB8888(dst_argb, src_argb)         \
{                                                              \
 uint32 alpha,dst[2];                                          \
 uint32 idst = (dst_argb);                                      \
 uint32 isrc = (src_argb);                                      \
 alpha = (uint32)(255 - (int32)(isrc >> 24));      \
 dst[0] = ((idst & 0x00FF00FF) * alpha + 0x00800080) & 0xFF00FF00;          \
 dst[1] = (((idst>>8) & 0x00FF00FF) * alpha + 0x00800080) & 0xFF00FF00;          \
 (dst_argb) = (dst[0]>>8) + dst[1] + isrc;                      \
}



hd_engine *HD_Initialize(uint32 width,uint32 height,uint8 bpp, void *framebuffer, void (*update)(struct hd_engine*, int, int, int, int));
void HD_Register(hd_engine *eng,hd_object *obj);
void HD_Render(hd_engine *eng);
void HD_Animate(hd_engine *eng);
void HD_Destroy(hd_object *obj);
void HD_ScaleBlendClip (uint32 *sbuf, int stw, int sth, int sx, int sy, int sw, int sh,
                        uint32 *dbuf, int dtw, int dth, int dx, int dy, int dw, int dh);
hd_object *HD_New_Object();
hd_obj_list *HD_StackObjects (hd_obj_list *head);

#endif
