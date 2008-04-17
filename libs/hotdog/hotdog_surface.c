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
    hd_surface ret = xmalloc ((2 + height + (width * height)) * 4);
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

void HD_SurfaceFlipVertical (hd_surface srf)
{
    unsigned int x, y, w, h, *p, *q, t;

    /* Width and height of surface. */
    w = HD_SRF_WIDTH(srf);
    h = HD_SRF_HEIGHT(srf);

    /* Flip it. */
    for (y = 0; y < (h >> 1); y++) {
        p = HD_SRF_PIXELS(srf) + (y * w);
        q = HD_SRF_PIXELS(srf) + ((h - 1 - y) * w);

        for (x = 0; x < w; x++) {
            t = *p; *p++ = *q; *q++ = t;
        }
    }
}

void HD_SurfaceFlipHorizontal (hd_surface srf)
{
    unsigned int x, y, w, h, *p, *q, t;

    /* Width and height of surface. */
    w = HD_SRF_WIDTH(srf);
    h = HD_SRF_HEIGHT(srf);

    /* Flipping. */
    for (y = 0; y < h; y++) {
        p = HD_SRF_PIXELS(srf) + (y * w);
        q = HD_SRF_PIXELS(srf) + ((y + 1) * w) - 1;

        for (x = 0; x < (w >> 1); x++) {
            t = *p; *p++ = *q; *q-- = t;
        }
    }
}

hd_surface HD_SurfaceFlipDiagonal (hd_surface srf, int direction)
{
    unsigned int x, y, w, h, nw, nh, *p, *q;
    hd_surface flp;

    /* Width and height of surface. */
    w = HD_SRF_WIDTH(srf);
    h = HD_SRF_HEIGHT(srf);

    /* 
     * Width and height of resulting surface.
     * Not for ul-lr and ur-ll flipping, but
     * also needed for that. 
     */
    nw = h;
    nh = w;

    /* Surface to contain flipped image. */
    flp = HD_NewSurface(h, w);

    /* Source and destination pointers. */
    p = HD_SRF_PIXELS(srf);
    q = HD_SRF_PIXELS(flp);

    /* Do the flipping. */
    switch (direction) {
        default:
        case HD_FLIP_DIAGONAL_UL_LR:
            for (x = 0; x < w; x++) {
                for (y = 0; y < h; y++) {
                    q[y+x*nw] = p[x+y*w];
                }
            }
            break;
        case HD_FLIP_DIAGONAL_UR_LL:
            for (x = 0; x < w; x++) {
                for (y = 0; y < h; y++) {
                    q[h-2-y+x*nw] = p[w-2-x+y*w];
                }
            }
            break;
        case HD_FLIP_ROTATE_90_CW:
            for (x = 0; x < w; x++) {
                for (y = 0; y < h; y++) {
                    q[h-1-y+x*nw] = p[x+y*w];
                }
            }
            break;
        case HD_FLIP_ROTATE_90_CCW:
            for (x = 0; x < w; x++) {
                for (y = 0; y < h; y++) {
                    q[y+x*nw] = p[w-1-x+y*w];
                }
            }
            break;
    }

    /* Free old surface and return flipped one */
    HD_FreeSurface (srf);
    return flp;
}

hd_surface HD_SurfaceRotate (hd_surface srf, int degrees)
{
    int x, y, w, h;
    int xraxis, yraxis;
    int xp, yp, xpr, ypr, xo, yo;
    int cosine, sine;
    hd_surface rtd;
    uint32 *rowr, *rowo;

    /* Width and height of surface */
    w = HD_SRF_WIDTH(srf);
    h = HD_SRF_HEIGHT(srf);


    /* Prepare for switch statemant. */
    degrees =  degrees % 360;

    /* If it's a 90°/180°/270°/360° rotation. */
    switch (degrees) {
        case 90:
        case -270:
            return HD_SurfaceFlipDiagonal (srf, HD_FLIP_ROTATE_90_CW);
            break;
        case 180:
        case -180:
            rtd = HD_NewSurface (w, h);
            memcpy(rtd, srf, (h+2+w*h)*sizeof(uint32));
            HD_SurfaceFlipVertical (rtd);
            HD_SurfaceFlipHorizontal (rtd);
            HD_FreeSurface (srf);
            return rtd;
            break;
        case -90:
        case 270:
            return HD_SurfaceFlipDiagonal (srf, HD_FLIP_ROTATE_90_CCW);
            break;
        case 0:
            return srf;
            break;
        default:
            break;
    }

    /* 
     * Surface which contains rotated image.
     * Corners will be cut off. Calculation of
     * new width and height to be added later
     * when automatic resizing of an objects
     * surface can be stopped on desktop builds.
     */
    rtd = HD_NewSurface (w, h);

    /* Rotate around this point (center of image). */
    xraxis = w / 2;
    yraxis = h / 2;

    /* Cosine and sine 16.16 fixed point. */
    cosine  = fcos(-degrees * 2048 / 180);
    sine    = fsin(-degrees * 2048 / 180);

    /* Now rotate. */
    for (y = 0; y < h; y++) {
        rowr = HD_SRF_ROWF(rtd, y);
        yp   = 2 * (y - yraxis) + 1;

        for (x = 0; x < w; x++) {
            xp = 2 * (x - xraxis) + 1;

            xpr = (int)(xp * cosine - yp * sine)/(1<<16);
            ypr = (int)(xp * sine + yp * cosine)/(1<<16);

            xo = (xpr - 1) / 2 + xraxis;
            yo = (ypr - 1) / 2 + yraxis;

            if (xo >= 0 && xo < w && yo >= 0 && yo < h) {
                rowo    = HD_SRF_ROWF(srf, yo);
                rowr[x] = rowo[xo];
            }
            else {
                rowr[x] = HD_RGBA(0, 0, 0, 0);
            }
        }
    }

    /* Free old surface and return rotated surface. */
    HD_FreeSurface (srf);
    return rtd;
}
