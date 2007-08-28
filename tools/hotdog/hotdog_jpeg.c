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

#include "jpeglib.h"

#include "hotdog.h"

hd_surface HD_JPEG_Load (const char *fname, int *retw, int *reth)
{
        FILE *in;
        hd_surface srf;
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        unsigned char *ptr, *rgb;
        unsigned int i, ipos, w, h;
        int x, y;
        
        in = NULL; srf = NULL; ptr = NULL, rgb = NULL;
	w = h = 0;
        
        /* Open JPEG file */
        in = fopen(fname,"rb");
        if(in == NULL) {
                printf("Unable to open %s: %s\n", fname, strerror(errno));
                goto done;
        }
        
        /* Initialize */
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo, in);
        jpeg_read_header(&cinfo, TRUE);
        jpeg_start_decompress(&cinfo);
        
        w = cinfo.output_width;
        h = cinfo.output_height;
        
        /* Allocate our surface. */
        srf = HD_NewSurface (w, h);
        if (!srf) {
            fprintf (stderr, "Out of memory reading JPEG image.\n");
            goto done;
        }
        
        /* Allocate rgb buffer */
        rgb = malloc(w * h * 3 * sizeof(unsigned char));
        if (!rgb){
            fprintf (stderr, "Out of memory allocating buffer for JPEG image.\n");
            goto done;
        }
        
        /* Read JPEG image to RGB buffer */
        if(cinfo.output_components == 3){
                ptr = rgb;
                i = 0;
                while (cinfo.output_scanline < h){
                        jpeg_read_scanlines(&cinfo, &ptr, 1);
                        ptr += 3 * w;
                        i++;
                }
        }
        else if(cinfo.output_components == 1){
                ptr = malloc(w * sizeof(unsigned char));
                if (!ptr){
                    fprintf (stderr, "Out of memory allocating buffer for JPEG image.\n");
                    goto done;
                }
                
                ipos = 0;
                while (cinfo.output_scanline < h){
                        jpeg_read_scanlines(&cinfo, &ptr, 1);
                        for (i = 0; i < w; i++){
                                memset(rgb + ipos, ptr[i], 3);
                                ipos += 3;
                        }
                }
                
                if(ptr){
                        free(ptr);
                }
        }
        
        /* Assign RGB pixels to surface */
        i = 0;
        for (y = 0; y < (int)h; y++){
                for (x = 0; x < (int)w; x++){
                        HD_SRF_PIXF(srf, x, y) = HD_RGBA(rgb[i], rgb[i+1], rgb[i+2], 255);
                        i += 3;
                }
        }
        
        /* Finish */
        jpeg_finish_decompress(&cinfo);
        
 done:
        if (&cinfo)
            jpeg_destroy_decompress(&cinfo);
        if (in)
            fclose(in);
        if (rgb)
            free(rgb);
        
        *retw = w;
        *reth = h;
        return(srf);
}

hd_object *HD_JPEG_Create(const char *fname) {
        hd_object *ret;
        int w, h;

        ret = HD_New_Object();
        ret->type = HD_TYPE_CANVAS;
        ret->canvas = HD_JPEG_Load (fname, &w, &h);
        ret->natw = ret->w = w;
        ret->nath = ret->h = h;
        ret->speed = HD_SPEED_NOALPHA;
        ret->render = HD_Canvas_Render;
        ret->destroy = HD_Canvas_Destroy;

        return ret;
}
