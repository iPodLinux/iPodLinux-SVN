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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hotdog.h"
#include "hotdog_primitive.h"
#include "hotdog_png.h"
#include "hotdog_font.h"

hd_engine *HD_Initialize(uint32 width,uint32 height,uint16 *framebuffer) {
	hd_engine *eng;

	eng = (hd_engine *)malloc( sizeof(hd_engine) );
	assert(eng != NULL);

	eng->buffer = (uint32 *)malloc( width * height * 4 );
	assert(eng->buffer != NULL);

	eng->screen.width  = width;
	eng->screen.height = height;
	eng->screen.framebuffer = framebuffer;

	eng->list = NULL;

	return(eng);
}

uint8 HD_Register(hd_engine *eng,hd_object *obj) {
	hd_obj_list *curr;

	// Special case for first entry
	if( eng->list == NULL ) {
		eng->list = (hd_obj_list *)malloc( sizeof(hd_obj_list) );
		eng->list->obj = obj;
		eng->list->next = NULL;

		return(0);
	}

	curr = eng->list;
	while(curr->next != NULL) curr = curr->next;

	curr->next = (hd_obj_list *)malloc( sizeof(hd_obj_list) );
	assert(curr->next);

	curr->next->obj = obj;
	curr->next->next = NULL;

	return(0);
}

void HD_Render(hd_engine *eng) {
	hd_obj_list *curr;

	// Clear the internal buffer
	memset(eng->buffer,0,eng->screen.width*eng->screen.height*4);

	curr = eng->list;
	while(curr != NULL) {
		switch( curr->obj->type ) {
			case( HD_TYPE_PRIMITIVE ):
				HD_Primitive_Render(eng,curr->obj);
				break;
			case( HD_TYPE_PNG ):
				HD_PNG_Render(eng,curr->obj);
				break;
			case( HD_TYPE_FONT ):
				HD_Font_Render(eng,curr->obj);
				break;
			default:
				break;
		}
		curr = curr->next;
	}

	// Translate internal ARGB8888 to RGB565
	{
		uint32 off,sPix;
		uint16 dPix;

		for(off=0;off<(eng->screen.width*eng->screen.height);off++) {
			sPix = eng->buffer[off];

			dPix  = ((sPix & 0x00FF0000) >> (16+3)) << 11; // R
			dPix |= ((sPix & 0x0000FF00) >> (8+2)) << 5;  // G
			dPix |= ((sPix & 0x000000FF) >> (3));    // B

			eng->screen.framebuffer[off] = dPix;
		}

	}
}
