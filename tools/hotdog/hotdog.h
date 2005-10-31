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

#include "main.h"


#define HD_TYPE_PRIMITIVE 0x00
#define HD_TYPE_PNG       0x01
#define HD_PRIM_RECTANGLE 0x00

typedef struct {
	uint32 type;
	uint32 color; // ARGB
} hd_primitive;

typedef struct {
	int32 w,h;

	uint32 *argb;
} hd_png;

typedef struct {
	 int32 x,y,w,h,depth;
	uint8  opacity;
	uint32 type;

	union {
		hd_png       *png;
		hd_primitive *prim;
	} sub;

} hd_object;

typedef struct hd_obj_list {
	hd_object *obj;

	struct hd_obj_list *next;
} hd_obj_list;

typedef struct {
	struct {
		uint32  width,height;
		uint16 *framebuffer;
	} screen;

	uint32 *buffer;

	hd_obj_list *list;
} hd_engine;

hd_engine *HD_Initialize(uint32 width,uint32 height,uint16 *framebuffer);
uint8 HD_Register(hd_engine *eng,hd_object *obj);
void HD_Render(hd_engine *eng);

#endif