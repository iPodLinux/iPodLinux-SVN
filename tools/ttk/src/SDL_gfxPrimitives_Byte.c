/* 

 SDL_gfxPrimitives - Graphics primitives for SDL surfaces

 LGPL (c) A. Schiffler

*/

/* Modification by Joshua Oreman for non-alpha 1-byte very fast primitives. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "ttk/SDL_gfxPrimitives.h"
#include "ttk/SDL_gfxPrimitives_font.h"

/* -===================- */

/* ----- Defines for pixel clipping tests */

#define clip_xmin(surface) surface->clip_rect.x
#define clip_xmax(surface) surface->clip_rect.x+surface->clip_rect.w-1
#define clip_ymin(surface) surface->clip_rect.y
#define clip_ymax(surface) surface->clip_rect.y+surface->clip_rect.h-1

/* ----- Pixel - fast, no blending, no locking, clipping */

int fastPixelByteNolock(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    int bpp;
    Uint8 *p;

    /*
     * Honor clipping setup at pixel level 
     */
    if ((x >= clip_xmin(dst)) && (x <= clip_xmax(dst)) && (y >= clip_ymin(dst)) && (y <= clip_ymax(dst))) {

	/*
	 * Get destination format 
	 */
	bpp = dst->format->BytesPerPixel;
	p = (Uint8 *) dst->pixels + y * dst->pitch + x * bpp;
	switch (bpp) {
	case 1:
	    *p = color;
	    break;
	case 2:
	    *(Uint16 *) p = color;
	    break;
	case 3:
	    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		p[0] = (color >> 16) & 0xff;
		p[1] = (color >> 8) & 0xff;
		p[2] = color & 0xff;
	    } else {
		p[0] = color & 0xff;
		p[1] = (color >> 8) & 0xff;
		p[2] = (color >> 16) & 0xff;
	    }
	    break;
	case 4:
	    *(Uint32 *) p = color;
	    break;
	}			/* switch */


    }

    return (0);
}

/* ----- Pixel - fast, no blending, no locking, no clipping */

/* (faster but dangerous, make sure we stay in surface bounds) */

int fastPixelByteNolockNoclip(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    int bpp;
    Uint8 *p;

    /*
     * Get destination format 
     */
    bpp = dst->format->BytesPerPixel;
    p = (Uint8 *) dst->pixels + y * dst->pitch + x * bpp;
    switch (bpp) {
    case 1:
	*p = color;
	break;
    case 2:
	*(Uint16 *) p = color;
	break;
    case 3:
	if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
	    p[0] = (color >> 16) & 0xff;
	    p[1] = (color >> 8) & 0xff;
	    p[2] = color & 0xff;
	} else {
	    p[0] = color & 0xff;
	    p[1] = (color >> 8) & 0xff;
	    p[2] = (color >> 16) & 0xff;
	}
	break;
    case 4:
	*(Uint32 *) p = color;
	break;
    }				/* switch */

    return (0);
}

/* ----- Pixel - fast, no blending, locking, clipping */

int fastPixelByte(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    int result;

    /*
     * Lock the surface 
     */
    if (SDL_MUSTLOCK(dst)) {
	if (SDL_LockSurface(dst) < 0) {
	    return (-1);
	}
    }

    result = fastPixelByteNolock(dst, x, y, color);

    /*
     * Unlock surface 
     */
    if (SDL_MUSTLOCK(dst)) {
	SDL_UnlockSurface(dst);
    }

    return (result);
}

/* ----- Pixel - pixel draw with blending enabled if a<255 */

int pixelByte(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    return fastPixelByte (dst, x, y, color);
}

int pixelByteNolock(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
    return fastPixelByteNolock (dst, x, y, color);
}

int hlineByte(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color)
{
    Sint16 left, right, top, bottom;
    Uint8 *pixel, *pixellast;
    int dx;
    int pixx, pixy;
    Sint16 w;
    Sint16 xtmp;
    int result = -1;
    Uint8 *colorptr;

    /*
     * Get clipping boundary 
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    /*
     * Check visibility of hline 
     */
    if ((x1<left) && (x2<left)) {
     return(0);
    }
    if ((x1>right) && (x2>right)) {
     return(0);
    }
    if ((y<top) || (y>bottom)) {
     return (0);
    }

    /*
     * Clip x 
     */
    if (x1 < left) {
	x1 = left;
    }
    if (x2 > right) {
	x2 = right;
    }

    /*
     * Swap x1, x2 if required 
     */
    if (x1 > x2) {
	xtmp = x1;
	x1 = x2;
	x2 = xtmp;
    }

    /*
     * Calculate width 
     */
    w = x2 - x1;

    /*
     * Sanity check on width 
     */
    if (w < 0) {
	return (0);
    }

	/*
	 * No alpha-blending required 
	 */


	/*
	 * Lock surface 
	 */
	SDL_LockSurface(dst);

	/*
	 * More variable setup 
	 */
	dx = w;
	pixx = dst->format->BytesPerPixel;
	pixy = dst->pitch;
	pixel = ((Uint8 *) dst->pixels) + pixx * (int) x1 + pixy * (int) y;

	/*
	 * Draw 
	 */
	switch (dst->format->BytesPerPixel) {
	case 1:
	    memset(pixel, color, dx + 1);
	    break;
	case 2:
	    pixellast = pixel + dx + dx;
	    for (; pixel <= pixellast; pixel += pixx) {
		*(Uint16 *) pixel = color;
	    }
	    break;
	case 3:
	    pixellast = pixel + dx + dx + dx;
	    for (; pixel <= pixellast; pixel += pixx) {
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		    pixel[0] = (color >> 16) & 0xff;
		    pixel[1] = (color >> 8) & 0xff;
		    pixel[2] = color & 0xff;
		} else {
		    pixel[0] = color & 0xff;
		    pixel[1] = (color >> 8) & 0xff;
		    pixel[2] = (color >> 16) & 0xff;
		}
	    }
	    break;
	default:		/* case 4 */
	    dx = dx + dx;
	    pixellast = pixel + dx + dx;
	    for (; pixel <= pixellast; pixel += pixx) {
		*(Uint32 *) pixel = color;
	    }
	    break;
	}

	/*
	 * Unlock surface 
	 */
	SDL_UnlockSurface(dst);

	/*
	 * Set result code 
	 */
	result = 0;

    return (result);
}

int vlineByte(SDL_Surface * dst, Sint16 x, Sint16 y1, Sint16 y2, Uint32 color)
{
    Sint16 left, right, top, bottom;
    Uint8 *pixel, *pixellast;
    int dy;
    int pixx, pixy;
    Sint16 h;
    Sint16 ytmp;
    int result = -1;
    Uint8 *colorptr;

    /*
     * Get clipping boundary 
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    /*
     * Check visibility of vline 
     */
    if ((x<left) || (x>right)) {
     return (0);
    }
    if ((y1<top) && (y2<top)) {
     return(0);
    }
    if ((y1>bottom) && (y2>bottom)) {
     return(0);
    }

    /*
     * Clip y 
     */
    if (y1 < top) {
	y1 = top;
    }
    if (y2 > bottom) {
	y2 = bottom;
    }

    /*
     * Swap y1, y2 if required 
     */
    if (y1 > y2) {
	ytmp = y1;
	y1 = y2;
	y2 = ytmp;
    }

    /*
     * Calculate height 
     */
    h = y2 - y1;

    /*
     * Sanity check on height 
     */
    if (h < 0) {
	return (0);
    }

	/*
	 * No alpha-blending required 
	 */

	/*
	 * Lock surface 
	 */
	SDL_LockSurface(dst);

	/*
	 * More variable setup 
	 */
	dy = h;
	pixx = dst->format->BytesPerPixel;
	pixy = dst->pitch;
	pixel = ((Uint8 *) dst->pixels) + pixx * (int) x + pixy * (int) y1;
	pixellast = pixel + pixy * dy;

	/*
	 * Draw 
	 */
	switch (dst->format->BytesPerPixel) {
	case 1:
	    for (; pixel <= pixellast; pixel += pixy) {
		*(Uint8 *) pixel = color;
	    }
	    break;
	case 2:
	    for (; pixel <= pixellast; pixel += pixy) {
		*(Uint16 *) pixel = color;
	    }
	    break;
	case 3:
	    for (; pixel <= pixellast; pixel += pixy) {
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		    pixel[0] = (color >> 16) & 0xff;
		    pixel[1] = (color >> 8) & 0xff;
		    pixel[2] = color & 0xff;
		} else {
		    pixel[0] = color & 0xff;
		    pixel[1] = (color >> 8) & 0xff;
		    pixel[2] = (color >> 16) & 0xff;
		}
	    }
	    break;
	default:		/* case 4 */
	    for (; pixel <= pixellast; pixel += pixy) {
		*(Uint32 *) pixel = color;
	    }
	    break;
	}

	/*
	 * Unlock surface 
	 */
	SDL_UnlockSurface(dst);

	/*
	 * Set result code 
	 */
	result = 0;

    return (result);
}

int rectangleByte(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
    int result;
    Sint16 w, h, xtmp, ytmp;

    /*
     * Swap x1, x2 if required 
     */
    if (x1 > x2) {
	xtmp = x1;
	x1 = x2;
	x2 = xtmp;
    }

    /*
     * Swap y1, y2 if required 
     */
    if (y1 > y2) {
	ytmp = y1;
	y1 = y2;
	y2 = ytmp;
    }

    /*
     * Calculate width&height 
     */
    w = x2 - x1;
    h = y2 - y1;

    /*
     * Sanity check 
     */
    if ((w < 0) || (h < 0)) {
	return (0);
    }

    /*
     * Test for special cases of straight lines or single point 
     */
    if (x1 == x2) {
	if (y1 == y2) {
	    return (pixelByte(dst, x1, y1, color));
	} else {
	    return (vlineByte(dst, x1, y1, y2, color));
	}
    } else {
	if (y1 == y2) {
	    return (hlineByte(dst, x1, x2, y1, color));
	}
    }

    /*
     * Draw rectangle 
     */
    result = 0;
    result |= hlineByte(dst, x1, x2, y1, color);
    result |= hlineByte(dst, x1, x2, y2, color);
    y1 += 1;
    y2 -= 1;
    if (y1<=y2) {
     result |= vlineByte(dst, x1, y1, y2, color);
     result |= vlineByte(dst, x2, y1, y2, color);
    }
    return (result);

}

/* --------- Clipping routines for line */

/* Clipping based heavily on code from                       */
/* http://www.ncsa.uiuc.edu/Vis/Graphics/src/clipCohSuth.c   */

#define CLIP_LEFT_EDGE   0x1
#define CLIP_RIGHT_EDGE  0x2
#define CLIP_BOTTOM_EDGE 0x4
#define CLIP_TOP_EDGE    0x8
#define CLIP_INSIDE(a)   (!a)
#define CLIP_REJECT(a,b) (a&b)
#define CLIP_ACCEPT(a,b) (!(a|b))

static int clipEncode(Sint16 x, Sint16 y, Sint16 left, Sint16 top, Sint16 right, Sint16 bottom)
{
    int code = 0;

    if (x < left) {
	code |= CLIP_LEFT_EDGE;
    } else if (x > right) {
	code |= CLIP_RIGHT_EDGE;
    }
    if (y < top) {
	code |= CLIP_TOP_EDGE;
    } else if (y > bottom) {
	code |= CLIP_BOTTOM_EDGE;
    }
    return code;
}

static int clipLine(SDL_Surface * dst, Sint16 * x1, Sint16 * y1, Sint16 * x2, Sint16 * y2)
{
    Sint16 left, right, top, bottom;
    int code1, code2;
    int draw = 0;
    Sint16 swaptmp;
    float m;

    /*
     * Get clipping boundary 
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    while (1) {
	code1 = clipEncode(*x1, *y1, left, top, right, bottom);
	code2 = clipEncode(*x2, *y2, left, top, right, bottom);
	if (CLIP_ACCEPT(code1, code2)) {
	    draw = 1;
	    break;
	} else if (CLIP_REJECT(code1, code2))
	    break;
	else {
	    if (CLIP_INSIDE(code1)) {
		swaptmp = *x2;
		*x2 = *x1;
		*x1 = swaptmp;
		swaptmp = *y2;
		*y2 = *y1;
		*y1 = swaptmp;
		swaptmp = code2;
		code2 = code1;
		code1 = swaptmp;
	    }
	    if (*x2 != *x1) {
		m = (*y2 - *y1) / (float) (*x2 - *x1);
	    } else {
		m = 1.0f;
	    }
	    if (code1 & CLIP_LEFT_EDGE) {
		*y1 += (Sint16) ((left - *x1) * m);
		*x1 = left;
	    } else if (code1 & CLIP_RIGHT_EDGE) {
		*y1 += (Sint16) ((right - *x1) * m);
		*x1 = right;
	    } else if (code1 & CLIP_BOTTOM_EDGE) {
		if (*x2 != *x1) {
		    *x1 += (Sint16) ((bottom - *y1) / m);
		}
		*y1 = bottom;
	    } else if (code1 & CLIP_TOP_EDGE) {
		if (*x2 != *x1) {
		    *x1 += (Sint16) ((top - *y1) / m);
		}
		*y1 = top;
	    }
	}
    }

    return draw;
}

/* ----- Filled rectangle (Box) */

int boxByte(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
    Sint16 left, right, top, bottom;
    Uint8 *pixel, *pixellast;
    int x, dx;
    int dy;
    int pixx, pixy;
    Sint16 w, h, tmp;
    int result;
    Uint8 *colorptr;

    /*
     * Get clipping boundary 
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
    
    /* Check visibility */
    if ((x1<left) && (x2<left)) {
     return(0);
    }
    if ((x1>right) && (x2>right)) {
     return(0);
    }
    if ((y1<top) && (y2<top)) {
     return(0);
    }
    if ((y1>bottom) && (y2>bottom)) {
     return(0);
    }
     
    /* Clip all points */
    if (x1<left) { 
     x1=left; 
    } else if (x1>right) {
     x1=right;
    }
    if (x2<left) { 
     x2=left; 
    } else if (x2>right) {
     x2=right;
    }
    if (y1<top) { 
     y1=top; 
    } else if (y1>bottom) {
     y1=bottom;
    }
    if (y2<top) { 
     y2=top; 
    } else if (y2>bottom) {
     y2=bottom;
    }

    /*
     * Order coordinates 
     */
    if (x1 > x2) {
	tmp = x1;
	x1 = x2;
	x2 = tmp;
    }
    if (y1 > y2) {
	tmp = y1;
	y1 = y2;
	y2 = tmp;
    }

    /*
     * Test for special cases of straight line or single point 
     */
    if (x1 == x2) {
	if (y1 == y2) {
	    return (pixelByte(dst, x1, y1, color));
	} else { 
	    return (vlineByte(dst, x1, y1, y2, color));
	}
    }
    if (y1 == y2) {
	return (hlineByte(dst, x1, x2, y1, color));
    }


    /*
     * Calculate width&height 
     */
    w = x2 - x1;
    h = y2 - y1;

	/*
	 * No alpha-blending required 
	 */

	/*
	 * Lock surface 
	 */
	SDL_LockSurface(dst);

	/*
	 * More variable setup 
	 */
	dx = w;
	dy = h;
	pixx = dst->format->BytesPerPixel;
	pixy = dst->pitch;
	pixel = ((Uint8 *) dst->pixels) + pixx * (int) x1 + pixy * (int) y1;
	pixellast = pixel + pixx * dx + pixy * dy;
	dx++;
	
	/*
	 * Draw 
	 */
	switch (dst->format->BytesPerPixel) {
	case 1:
	    for (; pixel <= pixellast; pixel += pixy) {
		memset(pixel, (Uint8) color, dx);
	    }
	    break;
	case 2:
	    pixy -= (pixx * dx);
	    for (; pixel <= pixellast; pixel += pixy) {
		for (x = 0; x < dx; x++) {
		    *(Uint16 *) pixel = color;
		    pixel += pixx;
		}
	    }
	    break;
	case 3:
	    pixy -= (pixx * dx);
	    for (; pixel <= pixellast; pixel += pixy) {
		for (x = 0; x < dx; x++) {
		    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			pixel[0] = (color >> 16) & 0xff;
			pixel[1] = (color >> 8) & 0xff;
			pixel[2] = color & 0xff;
		    } else {
			pixel[0] = color & 0xff;
			pixel[1] = (color >> 8) & 0xff;
			pixel[2] = (color >> 16) & 0xff;
		    }
		    pixel += pixx;
		}
	    }
	    break;
	default:		/* case 4 */
	    pixy -= (pixx * dx);
	    for (; pixel <= pixellast; pixel += pixy) {
		for (x = 0; x < dx; x++) {
		    *(Uint32 *) pixel = color;
		    pixel += pixx;
		}
	    }
	    break;
	}

	/*
	 * Unlock surface 
	 */
	SDL_UnlockSurface(dst);

	result = 0;

    return (result);
}

/* ----- Line */

/* Non-alpha line drawing code adapted from routine          */
/* by Pete Shinners, pete@shinners.org                       */
/* Originally from pygame, http://pygame.seul.org            */

#define ABS(a) (((a)<0) ? -(a) : (a))

int lineByte(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
    int pixx, pixy;
    int x, y;
    int dx, dy;
    int ax, ay;
    int sx, sy;
    int swaptmp;
    Uint8 *pixel;
    Uint8 *colorptr;

    /*
     * Clip line and test if we have to draw 
     */
    if (!(clipLine(dst, &x1, &y1, &x2, &y2))) {
	return (0);
    }

    /*
     * Test for special cases of straight lines or single point 
     */
    if (x1 == x2) {
	if (y1 < y2) {
	    return (vlineByte(dst, x1, y1, y2, color));
	} else if (y1 > y2) {
	    return (vlineByte(dst, x1, y2, y1, color));
	} else {
	    return (pixelByte(dst, x1, y1, color));
	}
    }
    if (y1 == y2) {
	if (x1 < x2) {
	    return (hlineByte(dst, x1, x2, y1, color));
	} else if (x1 > x2) {
	    return (hlineByte(dst, x2, x1, y1, color));
	}
    }

    /*
     * Variable setup 
     */
    dx = x2 - x1;
    dy = y2 - y1;
    sx = (dx >= 0) ? 1 : -1;
    sy = (dy >= 0) ? 1 : -1;

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
	if (SDL_LockSurface(dst) < 0) {
	    return (-1);
	}
    }

	/*
	 * No alpha blending - use fast pixel routines 
	 */

	/*
	 * More variable setup 
	 */
	dx = sx * dx + 1;
	dy = sy * dy + 1;
	pixx = dst->format->BytesPerPixel;
	pixy = dst->pitch;
	pixel = ((Uint8 *) dst->pixels) + pixx * (int) x1 + pixy * (int) y1;
	pixx *= sx;
	pixy *= sy;
	if (dx < dy) {
	    swaptmp = dx;
	    dx = dy;
	    dy = swaptmp;
	    swaptmp = pixx;
	    pixx = pixy;
	    pixy = swaptmp;
	}

	/*
	 * Draw 
	 */
	x = 0;
	y = 0;
	switch (dst->format->BytesPerPixel) {
	case 1:
	    for (; x < dx; x++, pixel += pixx) {
		*pixel = color;
		y += dy;
		if (y >= dx) {
		    y -= dx;
		    pixel += pixy;
		}
	    }
	    break;
	case 2:
	    for (; x < dx; x++, pixel += pixx) {
		*(Uint16 *) pixel = color;
		y += dy;
		if (y >= dx) {
		    y -= dx;
		    pixel += pixy;
		}
	    }
	    break;
	case 3:
	    for (; x < dx; x++, pixel += pixx) {
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
		    pixel[0] = (color >> 16) & 0xff;
		    pixel[1] = (color >> 8) & 0xff;
		    pixel[2] = color & 0xff;
		} else {
		    pixel[0] = color & 0xff;
		    pixel[1] = (color >> 8) & 0xff;
		    pixel[2] = (color >> 16) & 0xff;
		}
		y += dy;
		if (y >= dx) {
		    y -= dx;
		    pixel += pixy;
		}
	    }
	    break;
	default:		/* case 4 */
	    for (; x < dx; x++, pixel += pixx) {
		*(Uint32 *) pixel = color;
		y += dy;
		if (y >= dx) {
		    y -= dx;
		    pixel += pixy;
		}
	    }
	    break;
	}

    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
	SDL_UnlockSurface(dst);
    }

    return (0);
}

/* AA Line */

int aalineByte(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
    return (lineByte(dst, x1, y1, x2, y2, color));
}


/* ----- Circle */

/* Note: Based on algorithm from sge library, modified by A. Schiffler */
/* with multiple pixel-draw removal and other minor speedup changes.   */

int circleByte(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 r, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 x1, y1, x2, y2;
    Sint16 cx = 0;
    Sint16 cy = r;
    Sint16 ocx = (Sint16) 0xffff;
    Sint16 ocy = (Sint16) 0xffff;
    Sint16 df = 1 - r;
    Sint16 d_e = 3;
    Sint16 d_se = -2 * r + 5;
    Sint16 xpcx, xmcx, xpcy, xmcy;
    Sint16 ypcy, ymcy, ypcx, ymcx;
    Uint8 *colorptr;

    /*
     * Sanity check radius 
     */
    if (r < 0) {
	return (-1);
    }

    /*
     * Special case for r=0 - draw a point 
     */
    if (r == 0) {
	return (pixelByte(dst, x, y, color));
    }

    /*
     * Get clipping boundary 
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    /*
     * Test if bounding box of circle is visible 
     */
    x1 = x - r;
    x2 = x + r;
    y1 = y - r;
    y2 = y + r;
    if ((x1<left) && (x2<left)) {
     return(0);
    } 
    if ((x1>right) && (x2>right)) {
     return(0);
    } 
    if ((y1<top) && (y2<top)) {
     return(0);
    } 
    if ((y1>bottom) && (y2>bottom)) {
     return(0);
    } 

    /*
     * Draw circle 
     */
    result = 0;

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
	if (SDL_LockSurface(dst) < 0) {
	    return (-1);
	}
    }

	/*
	 * No Alpha - direct memory writes 
	 */


	/*
	 * Draw 
	 */
	do {
	    if ((ocy != cy) || (ocx != cx)) {
		xpcx = x + cx;
		xmcx = x - cx;
		if (cy > 0) {
		    ypcy = y + cy;
		    ymcy = y - cy;
		    result |= fastPixelByteNolock(dst, xmcx, ypcy, color);
		    result |= fastPixelByteNolock(dst, xpcx, ypcy, color);
		    result |= fastPixelByteNolock(dst, xmcx, ymcy, color);
		    result |= fastPixelByteNolock(dst, xpcx, ymcy, color);
		} else {
		    result |= fastPixelByteNolock(dst, xmcx, y, color);
		    result |= fastPixelByteNolock(dst, xpcx, y, color);
		}
		ocy = cy;
		xpcy = x + cy;
		xmcy = x - cy;
		if (cx > 0) {
		    ypcx = y + cx;
		    ymcx = y - cx;
		    result |= fastPixelByteNolock(dst, xmcy, ypcx, color);
		    result |= fastPixelByteNolock(dst, xpcy, ypcx, color);
		    result |= fastPixelByteNolock(dst, xmcy, ymcx, color);
		    result |= fastPixelByteNolock(dst, xpcy, ymcx, color);
		} else {
		    result |= fastPixelByteNolock(dst, xmcy, y, color);
		    result |= fastPixelByteNolock(dst, xpcy, y, color);
		}
		ocx = cx;
	    }
	    /*
	     * Update 
	     */
	    if (df < 0) {
		df += d_e;
		d_e += 2;
		d_se += 2;
	    } else {
		df += d_se;
		d_e += 2;
		d_se += 4;
		cy--;
	    }
	    cx++;
	} while (cx <= cy);


    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
	SDL_UnlockSurface(dst);
    }

    return (result);
}

/* ----- AA Circle */

int aacircleByte(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 r, Uint32 color)
{
    return (circleByte(dst, x, y, r, color));
}

/* ----- Filled Circle */

/* Note: Based on algorithm from sge library with multiple-hline draw removal */

/* and other speedup changes. */

int filledCircleByte(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 r, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 x1, y1, x2, y2;
    Sint16 cx = 0;
    Sint16 cy = r;
    Sint16 ocx = (Sint16) 0xffff;
    Sint16 ocy = (Sint16) 0xffff;
    Sint16 df = 1 - r;
    Sint16 d_e = 3;
    Sint16 d_se = -2 * r + 5;
    Sint16 xpcx, xmcx, xpcy, xmcy;
    Sint16 ypcy, ymcy, ypcx, ymcx;

    /*
     * Sanity check radius 
     */
    if (r < 0) {
	return (-1);
    }

    /*
     * Special case for r=0 - draw a point 
     */
    if (r == 0) {
	return (pixelByte(dst, x, y, color));
    }

    /*
     * Get clipping boundary 
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    /*
     * Test if bounding box of circle is visible 
     */
    x1 = x - r;
    x2 = x + r;
    y1 = y - r;
    y2 = y + r;
    if ((x1<left) && (x2<left)) {
     return(0);
    } 
    if ((x1>right) && (x2>right)) {
     return(0);
    } 
    if ((y1<top) && (y2<top)) {
     return(0);
    } 
    if ((y1>bottom) && (y2>bottom)) {
     return(0);
    } 

    /*
     * Draw 
     */
    result = 0;
    do {
	xpcx = x + cx;
	xmcx = x - cx;
	xpcy = x + cy;
	xmcy = x - cy;
	if (ocy != cy) {
	    if (cy > 0) {
		ypcy = y + cy;
		ymcy = y - cy;
		result |= hlineByte(dst, xmcx, xpcx, ypcy, color);
		result |= hlineByte(dst, xmcx, xpcx, ymcy, color);
	    } else {
		result |= hlineByte(dst, xmcx, xpcx, y, color);
	    }
	    ocy = cy;
	}
	if (ocx != cx) {
	    if (cx != cy) {
		if (cx > 0) {
		    ypcx = y + cx;
		    ymcx = y - cx;
		    result |= hlineByte(dst, xmcy, xpcy, ymcx, color);
		    result |= hlineByte(dst, xmcy, xpcy, ypcx, color);
		} else {
		    result |= hlineByte(dst, xmcy, xpcy, y, color);
		}
	    }
	    ocx = cx;
	}
	/*
	 * Update 
	 */
	if (df < 0) {
	    df += d_e;
	    d_e += 2;
	    d_se += 2;
	} else {
	    df += d_se;
	    d_e += 2;
	    d_se += 4;
	    cy--;
	}
	cx++;
    } while (cx <= cy);

    return (result);
}


/* ----- Ellipse */

/* Note: Based on algorithm from sge library with multiple-hline draw removal */
/* and other speedup changes. */

int ellipseByte(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 x1, y1, x2, y2;
    int ix, iy;
    int h, i, j, k;
    int oh, oi, oj, ok;
    int xmh, xph, ypk, ymk;
    int xmi, xpi, ymj, ypj;
    int xmj, xpj, ymi, ypi;
    int xmk, xpk, ymh, yph;
    Uint8 *colorptr;

    /*
     * Sanity check radii 
     */
    if ((rx < 0) || (ry < 0)) {
	return (-1);
    }

    /*
     * Special case for rx=0 - draw a vline 
     */
    if (rx == 0) {
	return (vlineByte(dst, x, y - ry, y + ry, color));
    }
    /*
     * Special case for ry=0 - draw a hline 
     */
    if (ry == 0) {
	return (hlineByte(dst, x - rx, x + rx, y, color));
    }

    /*
     * Get clipping boundary 
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    /*
     * Test if bounding box of ellipse is visible 
     */
    x1 = x - rx;
    x2 = x + rx;
    y1 = y - ry;
    y2 = y + ry;
    if ((x1<left) && (x2<left)) {
     return(0);
    } 
    if ((x1>right) && (x2>right)) {
     return(0);
    } 
    if ((y1<top) && (y2<top)) {
     return(0);
    } 
    if ((y1>bottom) && (y2>bottom)) {
     return(0);
    } 

    /*
     * Init vars 
     */
    oh = oi = oj = ok = 0xFFFF;

    /*
     * Draw 
     */
    result = 0;

    /* Lock surface */
    if (SDL_MUSTLOCK(dst)) {
	if (SDL_LockSurface(dst) < 0) {
	    return (-1);
	}
    }

	/*
	 * No Alpha - direct memory writes 
	 */


	if (rx > ry) {
	    ix = 0;
	    iy = rx * 64;

	    do {
		h = (ix + 32) >> 6;
		i = (iy + 32) >> 6;
		j = (h * ry) / rx;
		k = (i * ry) / rx;

		if (((ok != k) && (oj != k)) || ((oj != j) && (ok != j)) || (k != j)) {
		    xph = x + h;
		    xmh = x - h;
		    if (k > 0) {
			ypk = y + k;
			ymk = y - k;
			result |= fastPixelByteNolock(dst, xmh, ypk, color);
			result |= fastPixelByteNolock(dst, xph, ypk, color);
			result |= fastPixelByteNolock(dst, xmh, ymk, color);
			result |= fastPixelByteNolock(dst, xph, ymk, color);
		    } else {
			result |= fastPixelByteNolock(dst, xmh, y, color);
			result |= fastPixelByteNolock(dst, xph, y, color);
		    }
		    ok = k;
		    xpi = x + i;
		    xmi = x - i;
		    if (j > 0) {
			ypj = y + j;
			ymj = y - j;
			result |= fastPixelByteNolock(dst, xmi, ypj, color);
			result |= fastPixelByteNolock(dst, xpi, ypj, color);
			result |= fastPixelByteNolock(dst, xmi, ymj, color);
			result |= fastPixelByteNolock(dst, xpi, ymj, color);
		    } else {
			result |= fastPixelByteNolock(dst, xmi, y, color);
			result |= fastPixelByteNolock(dst, xpi, y, color);
		    }
		    oj = j;
		}

		ix = ix + iy / rx;
		iy = iy - ix / rx;

	    } while (i > h);
	} else {
	    ix = 0;
	    iy = ry * 64;

	    do {
		h = (ix + 32) >> 6;
		i = (iy + 32) >> 6;
		j = (h * rx) / ry;
		k = (i * rx) / ry;

		if (((oi != i) && (oh != i)) || ((oh != h) && (oi != h) && (i != h))) {
		    xmj = x - j;
		    xpj = x + j;
		    if (i > 0) {
			ypi = y + i;
			ymi = y - i;
			result |= fastPixelByteNolock(dst, xmj, ypi, color);
			result |= fastPixelByteNolock(dst, xpj, ypi, color);
			result |= fastPixelByteNolock(dst, xmj, ymi, color);
			result |= fastPixelByteNolock(dst, xpj, ymi, color);
		    } else {
			result |= fastPixelByteNolock(dst, xmj, y, color);
			result |= fastPixelByteNolock(dst, xpj, y, color);
		    }
		    oi = i;
		    xmk = x - k;
		    xpk = x + k;
		    if (h > 0) {
			yph = y + h;
			ymh = y - h;
			result |= fastPixelByteNolock(dst, xmk, yph, color);
			result |= fastPixelByteNolock(dst, xpk, yph, color);
			result |= fastPixelByteNolock(dst, xmk, ymh, color);
			result |= fastPixelByteNolock(dst, xpk, ymh, color);
		    } else {
			result |= fastPixelByteNolock(dst, xmk, y, color);
			result |= fastPixelByteNolock(dst, xpk, y, color);
		    }
		    oh = h;
		}

		ix = ix + iy / ry;
		iy = iy - ix / ry;

	    } while (i > h);
	}

    /* Unlock surface */
    if (SDL_MUSTLOCK(dst)) {
	SDL_UnlockSurface(dst);
    }

    return (result);
}

/* ----- AA Ellipse */

/* Based on code from Anders Lindstroem, based on code from SGE, based on code from TwinLib */

int aaellipseByte(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
    /*
     * Draw 
     */
    return (ellipseByte (dst, x, y, rx, ry, color));
}

/* ---- Filled Ellipse */

/* Note: */
/* Based on algorithm from sge library with multiple-hline draw removal */
/* and other speedup changes. */

int filledEllipseByte(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
    Sint16 left, right, top, bottom;
    int result;
    Sint16 x1, y1, x2, y2;
    int ix, iy;
    int h, i, j, k;
    int oh, oi, oj, ok;
    int xmh, xph;
    int xmi, xpi;
    int xmj, xpj;
    int xmk, xpk;

    /*
     * Sanity check radii 
     */
    if ((rx < 0) || (ry < 0)) {
	return (-1);
    }

    /*
     * Special case for rx=0 - draw a vline 
     */
    if (rx == 0) {
	return (vlineByte(dst, x, y - ry, y + ry, color));
    }
    /*
     * Special case for ry=0 - draw a hline 
     */
    if (ry == 0) {
	return (hlineByte(dst, x - rx, x + rx, y, color));
    }

    /*
     * Get clipping boundary 
     */
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    /*
     * Test if bounding box of ellipse is visible 
     */
    x1 = x - rx;
    x2 = x + rx;
    y1 = y - ry;
    y2 = y + ry;
    if ((x1<left) && (x2<left)) {
     return(0);
    } 
    if ((x1>right) && (x2>right)) {
     return(0);
    } 
    if ((y1<top) && (y2<top)) {
     return(0);
    } 
    if ((y1>bottom) && (y2>bottom)) {
     return(0);
    } 

    /*
     * Init vars 
     */
    oh = oi = oj = ok = 0xFFFF;

    /*
     * Draw 
     */
    result = 0;
    if (rx > ry) {
	ix = 0;
	iy = rx * 64;

	do {
	    h = (ix + 32) >> 6;
	    i = (iy + 32) >> 6;
	    j = (h * ry) / rx;
	    k = (i * ry) / rx;

	    if ((ok != k) && (oj != k)) {
		xph = x + h;
		xmh = x - h;
		if (k > 0) {
		    result |= hlineByte(dst, xmh, xph, y + k, color);
		    result |= hlineByte(dst, xmh, xph, y - k, color);
		} else {
		    result |= hlineByte(dst, xmh, xph, y, color);
		}
		ok = k;
	    }
	    if ((oj != j) && (ok != j) && (k != j)) {
		xmi = x - i;
		xpi = x + i;
		if (j > 0) {
		    result |= hlineByte(dst, xmi, xpi, y + j, color);
		    result |= hlineByte(dst, xmi, xpi, y - j, color);
		} else {
		    result |= hlineByte(dst, xmi, xpi, y, color);
		}
		oj = j;
	    }

	    ix = ix + iy / rx;
	    iy = iy - ix / rx;

	} while (i > h);
    } else {
	ix = 0;
	iy = ry * 64;

	do {
	    h = (ix + 32) >> 6;
	    i = (iy + 32) >> 6;
	    j = (h * rx) / ry;
	    k = (i * rx) / ry;

	    if ((oi != i) && (oh != i)) {
		xmj = x - j;
		xpj = x + j;
		if (i > 0) {
		    result |= hlineByte(dst, xmj, xpj, y + i, color);
		    result |= hlineByte(dst, xmj, xpj, y - i, color);
		} else {
		    result |= hlineByte(dst, xmj, xpj, y, color);
		}
		oi = i;
	    }
	    if ((oh != h) && (oi != h) && (i != h)) {
		xmk = x - k;
		xpk = x + k;
		if (h > 0) {
		    result |= hlineByte(dst, xmk, xpk, y + h, color);
		    result |= hlineByte(dst, xmk, xpk, y - h, color);
		} else {
		    result |= hlineByte(dst, xmk, xpk, y, color);
		}
		oh = h;
	    }

	    ix = ix + iy / ry;
	    iy = iy - ix / ry;

	} while (i > h);
    }

    return (result);
}


/* ----- filled pie */

/* Low-speed float pie-calc implementation by drawing polygons/lines. */

int pieByte(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, 
		Sint16 start, Sint16 end, Uint32 color) 
{
    fprintf (stderr, "Can't do pie on mono 'Pods\n");
    return 0;

}

int filledPieByte(SDL_Surface * dst, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color)
{
    fprintf (stderr, "Can't do fpie on mono 'Pods\n");
    return 0;
}

/* Trigon */

int trigonByte(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
 Sint16 vx[3]; 
 Sint16 vy[3];
 
 vx[0]=x1;
 vx[1]=x2;
 vx[2]=x3;
 vy[0]=y1;
 vy[1]=y2;
 vy[2]=y3;
 
 return(polygonByte(dst,vx,vy,3,color));
}

/* AA-Trigon */

int aatrigonByte(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
 Sint16 vx[3]; 
 Sint16 vy[3];
 
 vx[0]=x1;
 vx[1]=x2;
 vx[2]=x3;
 vy[0]=y1;
 vy[1]=y2;
 vy[2]=y3;
 
 return(aapolygonByte(dst,vx,vy,3,color));
}

/* Filled Trigon */

int filledTrigonByte(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
 Sint16 vx[3]; 
 Sint16 vy[3];
 
 vx[0]=x1;
 vx[1]=x2;
 vx[2]=x3;
 vy[0]=y1;
 vy[1]=y2;
 vy[2]=y3;
 
 return(filledPolygonByte(dst,vx,vy,3,color));
}

/* ---- Polygon */

int polygonByte(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint32 color)
{
    int result;
    int i;
    const Sint16 *x1, *y1, *x2, *y2;

    /*
     * Sanity check 
     */
    if (n < 3) {
	return (-1);
    }

    /*
     * Pointer setup 
     */
    x1 = x2 = vx;
    y1 = y2 = vy;
    x2++;
    y2++;

    /*
     * Draw 
     */
    result = 0;
    for (i = 1; i < n; i++) {
	result |= lineByte(dst, *x1, *y1, *x2, *y2, color);
	x1 = x2;
	y1 = y2;
	x2++;
	y2++;
    }
    result |= lineByte(dst, *x1, *y1, *vx, *vy, color);

    return (result);
}

/* ---- AA-Polygon */

int aapolygonByte(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint32 color)
{
    return polygonByte (dst, vx, vy, n, color);
}

/* ---- Filled Polygon */

int gfxPrimitivesCompareInt(const void *a, const void *b);

static int *gfxPrimitivesPolyInts = NULL;
static int gfxPrimitivesPolyAllocated = 0;

int filledPolygonByte(SDL_Surface * dst, const Sint16 * vx, const Sint16 * vy, int n, Uint32 color)
{
    int result;
    int i;
    int x, y, xa, xb;
    int miny, maxy;
    int x1, y1;
    int x2, y2;
    int ind1, ind2;
    int ints;

    /*
     * Sanity check 
     */
    if (n < 3) {
	return -1;
    }

    /*
     * Allocate temp array, only grow array 
     */
    if (!gfxPrimitivesPolyAllocated) {
	gfxPrimitivesPolyInts = (int *) malloc(sizeof(int) * n);
	gfxPrimitivesPolyAllocated = n;
    } else {
	if (gfxPrimitivesPolyAllocated < n) {
	    gfxPrimitivesPolyInts = (int *) realloc(gfxPrimitivesPolyInts, sizeof(int) * n);
	    gfxPrimitivesPolyAllocated = n;
	}
    }

    /*
     * Determine Y maxima 
     */
    miny = vy[0];
    maxy = vy[0];
    for (i = 1; (i < n); i++) {
	if (vy[i] < miny) {
	    miny = vy[i];
	} else if (vy[i] > maxy) {
	    maxy = vy[i];
	}
    }

    /*
     * Draw, scanning y 
     */
    result = 0;
    for (y = miny; (y <= maxy); y++) {
	ints = 0;
	for (i = 0; (i < n); i++) {
	    if (!i) {
		ind1 = n - 1;
		ind2 = 0;
	    } else {
		ind1 = i - 1;
		ind2 = i;
	    }
	    y1 = vy[ind1];
	    y2 = vy[ind2];
	    if (y1 < y2) {
		x1 = vx[ind1];
		x2 = vx[ind2];
	    } else if (y1 > y2) {
		y2 = vy[ind1];
		y1 = vy[ind2];
		x2 = vx[ind1];
		x1 = vx[ind2];
	    } else {
		continue;
	    }
	    if ( ((y >= y1) && (y < y2)) || ((y == maxy) && (y > y1) && (y <= y2)) ) {
		gfxPrimitivesPolyInts[ints++] = ((65536 * (y - y1)) / (y2 - y1)) * (x2 - x1) + (65536 * x1);
	    } 
	    
	}
	
	qsort(gfxPrimitivesPolyInts, ints, sizeof(int), gfxPrimitivesCompareInt);

	for (i = 0; (i < ints); i += 2) {
	    xa = gfxPrimitivesPolyInts[i] + 1;
	    xa = (xa >> 16) + ((xa & 32768) >> 15);
	    xb = gfxPrimitivesPolyInts[i+1] - 1;
	    xb = (xb >> 16) + ((xb & 32768) >> 15);
	    result |= hlineByte(dst, xa, xb, y, color);
	}
    }

    return (result);
}

