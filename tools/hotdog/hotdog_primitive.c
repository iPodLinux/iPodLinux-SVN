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
#include <stdio.h>
#include <assert.h>

#include "hotdog.h"

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

void HD_Primitive_Render(hd_engine *eng,hd_object *obj) {
	int32 x,y;

	for(y=obj->y;y<(obj->y+obj->h);y++) {
		for(x=obj->x;x<(obj->x+obj->h);x++) {

			BLEND_ARGB8888_ON_ARGB8888( eng->buffer[ y * eng->screen.width + x ], obj->sub.prim->color );

		}
	}
}
