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

#include <stdlib.h>
#include <string.h>
#include "hotdog.h"

static void deconstruct_mask (uint32 mask, int *shiftp, int *lossp) 
{
    if (!mask) {
        *shiftp = 32;
        *lossp = 0;
        return;
    }

    int bit = 0;
    while (!(mask & (1 << bit)) && (bit < 32))
        bit++;

    *shiftp = bit;
    while ((mask & (1 << bit)) && (bit < 32))
        bit++;

    if (bit > (*shiftp + 8))
        bit = *shiftp + 8;

    *lossp = 8 - (bit - *shiftp);
}

static void deconstruct_masks (uint32 Rmask, uint32 Gmask, uint32 Bmask, uint32 Amask,
                        int *Rshift, int *Gshift, int *Bshift, int *Ashift,
                        int *Rloss, int *Gloss, int *Bloss, int *Aloss) 
{
    deconstruct_mask (Rmask, Rshift, Rloss);
    deconstruct_mask (Gmask, Gshift, Gloss);
    deconstruct_mask (Bmask, Bshift, Bloss);
    deconstruct_mask (Amask, Ashift, Aloss);
}

hd_surface HD_SurfaceFromARGB (uint32 *argb, int32 width, int32 height)
{
    hd_surface ret = HD_NewSurface (width, height);
    memcpy (HD_SRF_PIXELS (ret), argb, width * height * 4);
    return ret;
}

hd_surface HD_SurfaceFromPixels (void *pixels, int32 width, int32 height, int bpp,
                                 uint32 Rmask, uint32 Gmask, uint32 Bmask, uint32 Amask) 
{
    hd_surface ret = HD_NewSurface (width, height);
    void *pp = pixels;
    uint32 *dst = HD_SRF_PIXELS (ret);
    int Rshift, Gshift, Bshift, Ashift, Rloss, Gloss, Bloss, Aloss;

    deconstruct_masks (Rmask, Gmask, Bmask, Amask,
                       &Rshift, &Gshift, &Bshift, &Ashift,
                       &Rloss, &Gloss, &Bloss, &Aloss);

    while ((char *)pp < ((char *)pixels + width*height*(bpp >> 3))) {
        uint8 *p8 = (uint8 *)pp;
        uint16 *p16 = (uint16 *)pp;
        uint32 *p32 = (uint32 *)pp;
        uint8 pix8 = *p8;
        uint16 pix16 = *p16;
        uint32 pix32 = *p32;
        switch (bpp) {
        case 8:
            *dst = ((((pix8 >> Ashift) << Aloss) << 24) |
                    (((pix8 >> Rshift) << Rloss) << 16) |
                    (((pix8 >> Gshift) << Gloss) <<  8) |
                    (((pix8 >> Bshift) << Bloss) <<  0));
            break;
        case 16:
            *dst = ((((pix16 >> Ashift) << Aloss) << 24) |
                    (((pix16 >> Rshift) << Rloss) << 16) |
                    (((pix16 >> Gshift) << Gloss) <<  8) |
                    (((pix16 >> Bshift) << Bloss) <<  0));
            break;
        case 24:
#if __BYTE_ORDER__ == __BIG_ENDIAN__
            pix32 = ((p8[0] << 16) | (p8[1] << 8) | p8[2]);
#else
            pix32 = ((p8[2] << 16) | (p8[1] << 8) | p8[0]);
#endif
            /* FALLTHRU */
        case 32:
            *dst = ((((pix32 >> Ashift) << Aloss) << 24) |
                    (((pix32 >> Rshift) << Rloss) << 16) |
                    (((pix32 >> Gshift) << Gloss) <<  8) |
                    (((pix32 >> Bshift) << Bloss) <<  0));
            break;
        }

        if (!Amask) *dst |= 0xff000000;

        dst++;
        pp = (char *)pp + bpp;
    }

    return ret;
}

hd_surface HD_NewSurface (int32 width, int32 height) 
{
    hd_surface ret = malloc ((2 + height + (width * height)) * 4);
    int i;
    
    ret[0] = width;
    ret[1] = height;
    for (i = 0; i < height; i++) {
        ret[2+i] = 2 + height + i*width;
    }
    // solid black, transparent
    memset (ret + 2 + height, 0, width * height * 4);

    return ret;
}

void HD_FreeSurface (hd_surface srf) 
{
    free (srf);
}

void HD_PremultiplyAlpha (hd_surface srf) 
{
    uint32 *pp = HD_SRF_PIXELS (srf);
    uint32 *endp = HD_SRF_END (srf);
    while (pp < endp) {
        uint16 buf[4];
        
        buf[0] = (*pp & 0xFF000000) >> 24;
        buf[1] = (*pp & 0x00FF0000) >> 16;
        buf[2] = (*pp & 0x0000FF00) >> 8;
        buf[3] = (*pp & 0x000000FF);
        
        buf[1] = ((buf[1] * buf[0]) / 255) & 0xFF;
        buf[2] = ((buf[2] * buf[0]) / 255) & 0xFF;
        buf[3] = ((buf[3] * buf[0]) / 255) & 0xFF;
        
        *pp++ = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];        
    }
}
