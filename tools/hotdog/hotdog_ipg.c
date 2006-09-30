/*
 * Copyright (c) 2005, James Jacobsson
 * Copyright (c) 2006, Felix Bruns
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
#include <errno.h>

#include "hotdog.h"

#define RGB565toARGB8888(pix) ((255<<24)|((((pix&0xF800)>>11)*8)<<16)|((((pix&0x07E0)>>5)*4)<<8)|((pix&0x001F)*8))
#define RGBAtoARGB8888(pix) (((pix&0x000000FF)<<24)|(((pix&0xFF000000)>>24)<<16)|(((pix&0x00FF0000)>>16)<<8)|((pix&0x0000FF00)>>8))
#define SWITCH_16(s) (((s & 0xff) << 8) | ((s & 0xff00) >> 8))
#define SWITCH_32(l) (((l & 0xff) << 24) | ((l & 0xff00) << 8) | ((l & 0xff0000) >> 8) | ((l & 0xff000000) >> 24))

int big_endian()
{
    char ch[4] = { '\0', '\1', '\2', '\3' };
    unsigned i = 0x00010203;

    if (*((int *)ch) == i) 
	return 1;
    else
	return 0;
}

hd_surface HD_RAWLCD5_Load (const char *fname, int *retw, int *reth)
{
	FILE *in;
	char buffer[5];
	unsigned short *rgb565;
	unsigned long width, height, id;
        hd_surface srf;
        int i, x, y, be = big_endian();
        
        in = NULL; srf = NULL; rgb565 = NULL;
	
	/* Open the file and read header info */
	in = fopen(fname,"rb");
	if(in == NULL) {
		fprintf(stderr, "Unable to open %s: %s\n", fname, strerror(errno));
                goto done;
	}
	fread(&width , 1, 4, in);
	fread(&height, 1, 4, in);
	fread(&id    , 1, 4, in);
	fread(buffer , 1, 4, in);
	buffer[4] = '\0';
	
	if(be){
		width  = SWITCH_32(width);
		height = SWITCH_32(height);
		id     = SWITCH_32(id);
	}
	
	/* Check if file is a IPD 565 (ID=4097) or RAWLCD5 (ID=640) */
	if( id != 4097 && ( id != 640 || strcmp(buffer, "565L") != 0 ) ) {
		fprintf(stderr, "File is not a RAWLCD5\n");
                goto done;
	}

	/* Allocate space for RGB565 pixel data and read it */
	rgb565 = calloc(width*height*2, sizeof(char));
	fread(rgb565, 1, width*height*2, in);
        
        /* Allocate our surface. */
        srf = HD_NewSurface (width, height);
        if (!srf) {
            fprintf (stderr, "Out of memory reading RAWLCD5 image.\n");
            goto done;
        }

	/* Set pixels */
	i = 0;
	for(y=0; y<height; y++){
		for(x=0; x<width; x++){
			if(be){
				HD_SRF_PIXF(srf, x, y) = RGB565toARGB8888( SWITCH_16(rgb565[i]) );
			}
			else{
				HD_SRF_PIXF(srf, x, y) = RGB565toARGB8888( rgb565[i] );
			}
			i++;
  		}
  	}

        /* Premultiply the alpha, if we need to. */
        HD_PremultiplyAlpha (srf);
	
 done:
        if (in)
            fclose(in);
        if (rgb565)
            free (rgb565);

        *retw = width;
        *reth = height;
	return(srf);
}

hd_object *HD_RAWLCD5_Create(const char *fname) {
        hd_object *ret;
        int w, h;

        ret = HD_New_Object();
        ret->type = HD_TYPE_CANVAS;
        ret->canvas = HD_RAWLCD5_Load (fname, &w, &h);
        ret->natw = ret->w = w;
        ret->nath = ret->h = h;

        /* Check alpha values, set speed appropriately. */
        ret->speed = HD_SPEED_NOALPHA | HD_SPEED_BINALPHA;
        uint32 *pp = HD_SRF_PIXELS (ret->canvas);
        while (pp < HD_SRF_END (ret->canvas)) {
            if ((*pp >> 24) != 0xff) {
                ret->speed &= ~HD_SPEED_NOALPHA;
                if ((*pp >> 24) != 0) ret->speed &= ~HD_SPEED_BINALPHA;
            }
            pp++;
        }

	ret->render = HD_Canvas_Render;
        ret->destroy = HD_Canvas_Destroy;

        return ret;
}

hd_surface HD_IPD_ANM_Load (const char *fname, int *retw, int *reth)
{
	FILE *in;
	char buffer[5];
	unsigned int  *palette;
	unsigned char *cindex;
	unsigned long width, height, id;
        hd_surface srf;
        int i, x, y, be = big_endian();
        
        in = NULL; srf = NULL; palette = NULL; cindex = NULL;
	
	/* Open the file and read header info */
	in = fopen(fname,"rb");
	if(in == NULL) {
		fprintf(stderr, "Unable to open %s: %s\n", fname, strerror(errno));
                goto done;
	}
	fread(&width , 1, 4, in);
	fread(&height, 1, 4, in);
	fread(&id    , 1, 4, in);
	fread(buffer , 1, 4, in);
	buffer[4] = '\0';
	
	if(be){
		width  = SWITCH_32(width);
		height = SWITCH_32(height);
		id     = SWITCH_32(id);
	}

	/* Check if file is a IPD 565 (ID=4097) or IPD Palette image (ID=2057) */
	if( id != 2057 && id != 4097 ) {
		fprintf(stderr, "File is not a IPD 565 or IPD Palette image\n");
                goto done;
	}
	/* If it is a IPD 565 (ID=4097) image, pass it to HD_RAWLCD5_Load */
	else if( id == 4097 ){
		int w, h;
		srf = HD_RAWLCD5_Load (fname, &w, &h);
		width  = w;
		height = h;
		goto done;
	}

	/* Allocate space for RGBA palette and color index, read it */
	palette = calloc(1024        , sizeof(int));
	cindex  = calloc(width*height, sizeof(char));
	fread(palette, 1, 1024        , in);
	fread(cindex , 1, width*height, in);
        
        /* Allocate our surface. */
        srf = HD_NewSurface (width, height);
        if (!srf) {
            fprintf (stderr, "Out of memory reading RAWLCD5 image.\n");
            goto done;
        }

	/* Set pixels */
	i = 0;
	for(y=0; y<height; y++){
		for(x=0; x<width; x++){
			if(be){
				HD_SRF_PIXF(srf, x, height-y-1) = RGBAtoARGB8888( palette[cindex[i]] );
			}
			else{
				HD_SRF_PIXF(srf, x, height-y-1) = RGBAtoARGB8888( SWITCH_32(palette[cindex[i]]) );
			}
			i++;
  		}
  	}

        /* Premultiply the alpha, if we need to. */
        HD_PremultiplyAlpha (srf);
	
 done:
        if (in)
            fclose(in);
        if (palette)
            free (palette);
        if (cindex)
            free (cindex);

        *retw = width;
        *reth = height;
	return(srf);
}

hd_object *HD_IPD_ANM_Create(const char *fname) {
        hd_object *ret;
        int w, h;

        ret = HD_New_Object();
        ret->type = HD_TYPE_CANVAS;
        ret->canvas = HD_IPD_ANM_Load (fname, &w, &h);
        ret->natw = ret->w = w;
        ret->nath = ret->h = h;

        /* Check alpha values, set speed appropriately. */
        ret->speed = HD_SPEED_NOALPHA | HD_SPEED_BINALPHA;
        uint32 *pp = HD_SRF_PIXELS (ret->canvas);
        while (pp < HD_SRF_END (ret->canvas)) {
            if ((*pp >> 24) != 0xff) {
                ret->speed &= ~HD_SPEED_NOALPHA;
                if ((*pp >> 24) != 0) ret->speed &= ~HD_SPEED_BINALPHA;
            }
            pp++;
        }

	ret->render = HD_Canvas_Render;
        ret->destroy = HD_Canvas_Destroy;

        return ret;
}
