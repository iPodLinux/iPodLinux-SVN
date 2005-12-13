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

#include "png.h"

#include "hotdog.h"
#include "hotdog_png.h"

void HD_PNG_Render(hd_engine *eng,hd_object *obj, int x, int y, int w, int h) {
    HD_ScaleBlendClip (obj->sub.png->argb, obj->sub.png->w, obj->sub.png->h,
                       x, y, w, h,
                       eng->buffer, eng->screen.width, eng->screen.height,
                       obj->x, obj->y, obj->w, obj->h);
}

#if YOU_WANT_TO_USE_THE_OLD_CODE
void HD_PNG_Render(hd_engine *eng,hd_object *obj) {
  int32 fp_step_x,fp_step_y,fp_ix,fp_iy,fp_initial_ix,fp_initial_iy;
  int32 x,y;
  int32 startx,starty,endx,endy;
  uint32 buffOff, imgOff;
  
  fp_step_x = (obj->sub.png->w << 16) / obj->w;
  fp_step_y = (obj->sub.png->h << 16) / obj->h;

  // 1st, check if we need to do anything at all
  if( (obj->x > eng->screen.width) || (obj->y > eng->screen.height) ||
	  ((obj->x+obj->w) < 0) || ((obj->y+obj->h) < 0) ) {

	// Nope, we're entirely off-screen, so there's nothing for us to do
	return;

  // 2nd, check if we're completely inside the screen
  } else if( (obj->x >= 0) && ((obj->x+obj->w) < eng->screen.width) &&
             (obj->y >= 0) && ((obj->y+obj->h) < eng->screen.height) ) {
	
    startx = obj->x;
    starty = obj->y;
    endx   = obj->x+obj->w;
    endy   = obj->y+obj->h;

    fp_initial_ix = 0;
    fp_initial_iy = 0;
  } else {
    starty = obj->y;
	startx = obj->x;
    endy   = obj->y+obj->h;
    endx   = obj->x+obj->w;
    fp_initial_ix = 0;
    fp_initial_iy = 0;

    // Let the clipping commence
    if( obj->x < 0 ) {
		startx = 0;
		endx   = obj->w + obj->x;
		fp_initial_ix = -(obj->x) * fp_step_x;
	}
	if( (obj->x+obj->w) > eng->screen.width ) {
		endx = eng->screen.width - 1;
	}
    if( obj->y < 0 ) {
		starty = 0;
		endy   = obj->h + obj->y;
		fp_initial_iy = -(obj->y) * fp_step_y;
	}
	if( (obj->y+obj->h) > eng->screen.height ) {
		endy = eng->screen.height - 1;
	}
	
  }
  
  buffOff = starty * eng->screen.width;// + startx;
  imgOff  = 0;
  
  fp_iy = fp_initial_iy;
  for(y=starty;y<endy;y++) {
    fp_ix = fp_initial_ix;
    imgOff = (fp_iy>>16) * obj->sub.png->w;
    
    for(x=startx;x<endx;x++) {
      
      BLEND_ARGB8888_ON_ARGB8888( eng->buffer[ buffOff + x ], obj->sub.png->argb[ imgOff + (fp_ix>>16) ] );
      
      fp_ix += fp_step_x;
    }
    buffOff += eng->screen.width;
    fp_iy += fp_step_y;
  }
}
#endif

#if YOU_WANT_TO_UNDERSTAND_THE_OPTIMIZED_VERSION_ABOVE
void HD_PNG_Render_DOUBLE(hd_engine *eng,hd_object *obj) {
	double step_x,step_y,ix,iy;
	int32 x,y;
	
	step_x = (float)obj->sub.png->w / (float)obj->w;
	step_y = (float)obj->sub.png->h / (float)obj->h;
	
	ix = iy = 0;
	for(y=0;y<obj->h;y++) {
		ix = 0;
		for(x=0;x<obj->w;x++) {
			
			BLEND_ARGB8888_ON_ARGB8888( eng->buffer[ (y+obj->y) * eng->screen.width + (x+obj->x) ], obj->sub.png->argb[ (uint32)iy * obj->sub.png->w + (uint32)ix ] );
			
			ix += step_x;
		}
		iy += step_y;
	}
}
#endif

hd_object *HD_PNG_Create(char *fname) {
	FILE *in;
	unsigned char header[8];
	uint32 y,off;
        hd_object *ret;
	hd_png *png;
	png_bytep *row_pointers;
	
	in = fopen(fname,"rb");
	if(in == NULL) {
		printf("Unable to open %s\n",fname);
		return(NULL);
	}
	fread(header, 1, 8, in);
	
	if( png_sig_cmp(header, 0, 8) != 0 ) {
		printf("File is not a PNG\n");
		return(NULL);
	}
	
        ret = HD_New_Object();
        ret->type = HD_TYPE_PNG;
        ret->sub.png = png = (hd_png *)malloc (sizeof(hd_png));
	
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr)
		return(NULL);
	
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr) {
		png_destroy_read_struct(&png_ptr,(png_infopp)NULL, (png_infopp)NULL);
		return (NULL);
	}
	
	png_infop end_info = png_create_info_struct(png_ptr);
	if(!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr,(png_infopp)NULL);
		return(NULL);
	}
	
	png_init_io(png_ptr, in);
	png_set_sig_bytes(png_ptr, 8);
	
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	
	ret->natw = png->w = png_get_image_width(png_ptr,info_ptr);
	ret->nath = png->h = png_get_image_height(png_ptr,info_ptr);
	
	row_pointers = png_get_rows(png_ptr, info_ptr);
	png->argb = (uint32*)malloc( png->w * png->h * 4 );
	
	//printf("Image dim: %ux%u\n",ret->w,ret->h);
	
	for(y=0;y<png->h;y++) {
		memcpy( png->argb + y*png->w, row_pointers[y], png->w * 4 );

		// Convert RGBA to ARGB8888 w/ premultiplied alpha
		for(off=0;off<png->w;off++) {
			uint16 argb[4];

#ifndef _BIG_ENDIAN		
			*(uint32*)(png->argb + y*png->w + off) = 
			  ((*(uint32*)(png->argb + y*png->w + off) & 0xFF000000) >> 24) |
			  ((*(uint32*)(png->argb + y*png->w + off) & 0x00FF0000) >>  8) |
			  ((*(uint32*)(png->argb + y*png->w + off) & 0x0000FF00) <<  8) |
			  ((*(uint32*)(png->argb + y*png->w + off) & 0x000000FF) << 24);
#endif
			argb[1] = (*(uint32*)(png->argb + y*png->w + off) & 0xFF000000) >> 24;
			argb[2] = (*(uint32*)(png->argb + y*png->w + off) & 0x00FF0000) >> 16;
			argb[3] = (*(uint32*)(png->argb + y*png->w + off) & 0x0000FF00) >> 8;
			argb[0] = (*(uint32*)(png->argb + y*png->w + off) & 0x000000FF);
		
			argb[1] = ((argb[1] * argb[0]) / 255) & 0xFF;
			argb[2] = ((argb[2] * argb[0]) / 255) & 0xFF;
			argb[3] = ((argb[3] * argb[0]) / 255) & 0xFF;
		
			*(uint32*)(png->argb + y*png->w + off) = (argb[0] << 24) | (argb[1] << 16) | (argb[2] << 8) | argb[3];
		}
	}
	
	if( png_get_bit_depth(png_ptr,info_ptr) != 8 ) {
		free(png->argb);
		free(png);
                free(ret);
		printf("Not a 32 bit image\n");
		return(NULL);
	}
	if( png_get_color_type(png_ptr,info_ptr) != PNG_COLOR_TYPE_RGB_ALPHA ) {
		free(png->argb);
		free(png);
                free(ret);
		printf("Not a RGBA image\n");
		return(NULL);
	}
	
	ret->render = HD_PNG_Render;
        ret->destroy = HD_PNG_Destroy;
	
	fclose(in);
	
	return(ret);
	
}

void HD_PNG_Destroy (hd_object *obj) 
{
    if (obj->sub.png->argb) free (obj->sub.png->argb);
    free (obj->sub.png);
}
