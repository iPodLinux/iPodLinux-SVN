#include "ttk.h"
#include "menu.h"
#include "imgview.h"
#include <stdlib.h>

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif

#define _MAKETHIS imgview_data *data = (imgview_data *)this->data
extern ttk_screeninfo *ttk_screen;

#define MAGSTEP 1.414

#define SDIR_X 0
#define SDIR_Y 1

typedef struct _imgview_data
{
    int ow, oh; // orig width + height
    int sx, sy; // (x,y) of UR corner of viewable part on cur
    int cw, ch; // cur width + height
    ttk_surface src;
    ttk_surface cur;
    float zoom, ozoom;
    int scrolldir;
} imgview_data;


#ifdef SDL
// Floyd-Steinberg dithering algorithm shamelessly modified from netpbm. :-)

#define FS_SCALE 1024

struct fserr 
{
    long **thiserr;
    long **nexterr;
    int fsForward;
};


void freeFserr (struct fserr *const fserr) 
{
    free (fserr->thiserr[0]);
    free (fserr->nexterr[0]);
    free (fserr->thiserr);
    free (fserr->nexterr);
}

static int initFserr (SDL_Surface *srf, struct fserr *const fserr) 
{
    unsigned int const fserrSize = srf->w + 2;

    fserr->thiserr = malloc (sizeof(long*) * srf->format->BytesPerPixel);
    fserr->nexterr = malloc (sizeof(long*) * srf->format->BytesPerPixel);
    fserr->thiserr[0] = malloc (sizeof(long) * fserrSize);
    fserr->nexterr[0] = malloc (sizeof(long) * fserrSize);

    if (!fserr->thiserr || !fserr->nexterr || !fserr->thiserr[0] || !fserr->nexterr[0]) {
	fprintf (stderr, "Error allocating Floyd-Steinberg structures");
	return 0;
    }
    
    srand ((int)(time (0) ^ getpid()));
    {
	int col;
	for (col = 0; col < fserrSize; ++col)
	    fserr->thiserr[0][col] = rand() % (FS_SCALE * 2) - FS_SCALE;
    }
    fserr->fsForward = 1;

    return 1;
}

static void floydInitRow (SDL_Surface *srf, struct fserr *const fserr) 
{
    int col;
    for (col = 0; col < srf->w + 2; ++col) {
	fserr->nexterr[0][col] = 0;
    }
}

static void floydAdjustColor (SDL_Surface *srf, Uint8 *color, struct fserr *const fserr, int col) 
{
    long int const s = *color + fserr->thiserr[0][col+1] / FS_SCALE;
    *color = MIN (255, MAX (0, s));
}

static void floydPropagateErr (SDL_Surface *in, struct fserr *const fserr, int col,
			       Uint8 oldcolor, SDL_Surface *out, Uint8 newcolor) 
{
    long const err = (oldcolor - newcolor) * FS_SCALE;
    if (fserr->fsForward) {
	fserr->thiserr[0][col + 2] += (err * 7) >> 4;
	fserr->nexterr[0][col    ] += (err * 3) >> 4;
	fserr->nexterr[0][col + 1] += (err * 5) >> 4;
	fserr->nexterr[0][col + 2] += (err    ) >> 4;
    } else {
	fserr->thiserr[0][col    ] += (err * 7) >> 4;
	fserr->nexterr[0][col + 2] += (err * 3) >> 4;
	fserr->nexterr[0][col + 1] += (err * 5) >> 4;
	fserr->nexterr[0][col    ] += (err    ) >> 4;
    }
}

static void floydSwitchDir (SDL_Surface *srf, struct fserr *const fserr) 
{
    long *const temperr = fserr->thiserr[0];
    fserr->thiserr[0] = fserr->nexterr[0];
    fserr->nexterr[0] = temperr;
    //    fserr->fsForward = !fserr->fsForward;
}

// My function:
static void do_dither (SDL_Surface *src, SDL_Surface *dst) 
{
    struct fserr fserr;
    int dstcolors[] = { 255, 160, 80, 0 };
    int i, y, x, ex, dx;
    Uint8 *p, *pline, *q, *qline;

    p = pline = src->pixels; q = qline = dst->pixels;
    
    initFserr (src, &fserr);

    for (y = 0; y < src->h; y++) {
	floydInitRow (src, &fserr);

	if (fserr.fsForward)
	    x = 0, ex = src->w, dx = 1;
	else
	    x = src->w - 1, p += src->pitch - 1, q += dst->pitch - 1, ex = -1, dx = -1;

	for (; x != ex; x += dx) {
	    int closest = -1, closest_distance = 256;
	    int origcol;
	    
	    *q = *p;
	    floydAdjustColor (src, q, &fserr, x);
	    origcol = *q;

	    for (i = 0; i < 4; i++) {
		if (abs (origcol - dstcolors[i]) < closest_distance) {
		    closest_distance = abs (origcol - dstcolors[i]);
		    closest = i;
		}
	    }
	    *q = closest;

	    floydPropagateErr (src, &fserr, x, origcol, dst, dstcolors[*q]);

	    p += dx;
	    q += dx;
	}

	floydSwitchDir (src, &fserr);

	pline += src->pitch;
	qline += dst->pitch;
	p = pline;
	q = qline;
    }
}
#endif


static ttk_surface floyd_steinberg_dither (ttk_surface srfc) 
{
#ifdef SDL  /* Relatively high-speed SDL version */
    
    SDL_Surface
	*srf = SDL_CreateRGBSurface (0, srfc->w, srfc->h, 8, 0, 0, 0, 0),
	*ret = ttk_new_surface (srfc->w, srfc->h, 2);
    
    Uint8  *pline, *p, *qline, *q;
    Uint16 *p16;
    Uint32 *p32;
    int x, y, i;
    
    for (i = 0; i < 256; i++) {
	srf->format->palette->colors[i].r = i;
	srf->format->palette->colors[i].g = i;
	srf->format->palette->colors[i].b = i;
    }
    
    if (SDL_MUSTLOCK (srfc))
	SDL_LockSurface (srfc);

    p16 = srfc->pixels; p32 = srfc->pixels; p = pline = srfc->pixels; q = qline = srf->pixels;
    
    for (y = 0; y < srfc->h; y++) {
	for (x = 0; x < srfc->w; x++) {
	    Uint8 r, g, b;
	    if (srfc->format->BitsPerPixel == 8) {
		r = srfc->format->palette->colors[*p].r;
		g = srfc->format->palette->colors[*p].g;
		b = srfc->format->palette->colors[*p].b;
		p++;
	    } else if (srfc->format->BitsPerPixel == 16) {
		SDL_GetRGB (*p16++, srfc->format, &r, &g, &b);
	    } else if (srfc->format->BitsPerPixel == 32) {
		SDL_GetRGB (*p32++, srfc->format, &r, &g, &b);
	    } else {
		fprintf (stderr, "unrecognized surface format %d!\n", srfc->format->BitsPerPixel);
		r = g = b = 160;
	    }
	    
	    *q++ = (r*30/100) + (g*59/100) + (b*11/100);
	}

	pline += srfc->pitch;
	p   = pline;
	p16 = (Uint16 *)pline;
	p32 = (Uint32 *)pline;

	qline += srf->pitch;
	q = qline;
    }

    p = pline = srf->pixels; q = qline = ret->pixels;
    
    do_dither (srf, ret);

#if 0
    for (y = 0; y < srf->h; y++) {
	for (x = 0; x < srf->w; x++) {
	    const int A = 7, B = 3, C = 5, D = 1;
	    int val, tval, err;

	    val = *p++;

	    if (val < 40) err = val, tval = 3;
	    else if (val < 120) err = val - 80, tval = 2;
	    else if (val < 204) err = val - 160, tval = 1;
	    else err = 255 - val, tval = 0;

	    *q++ = tval;

	    if (                     y + 1 < srf->h)  *(p + srf->pitch    ) += (A * err) >> 4;
	    if ((x + 1 < srf->w) && (y > 0))          *(p - srf->pitch + 1) += (B * err) >> 4;
	    if ( x + 1 < srf->w)                      *(p              + 1) += (C * err) >> 4;
	    if ((x + 1 < srf->w) && (y + 1 < srf->h)) *(p + srf->pitch + 1) += (D * err) >> 4;
	}

	pline += srf->pitch;
	p = pline;

	qline += ret->pitch;
	q = qline;
    }
#endif

    if (SDL_MUSTLOCK (srfc))
	SDL_UnlockSurface (srfc);

    SDL_FreeSurface (srfc);
    SDL_FreeSurface (srf);
    return ret;
#else
    return srfc; // MW should do auto dithering...
#endif
}

TWidget *ttk_new_imgview_widget (int w, int h, ttk_surface img) 
{
    TWidget *ret = ttk_new_widget (0, 0);
    imgview_data *data = calloc (sizeof(imgview_data), 1);
    int maxdimen, W, H;
    float magW, magH;
    
    ret->w = w;
    ret->h = h;
    ret->data = data;
    ret->focusable = 1;
    ret->draw = ttk_imgview_draw;
    ret->down = ttk_imgview_down;
    ret->scroll = ttk_imgview_scroll;
    ret->destroy = ttk_imgview_free;

    ttk_surface_get_dimen (img, &data->ow, &data->oh);
    data->sx = data->sy = 0;
    data->src = img;
    data->scrolldir = SDIR_X;
    data->ozoom = 0.0;

    if (data->ow > ret->w)
	magW = (float)ret->w / (float)data->ow;
    else
	magW = 1.0;

    if (data->oh > ret->h)
	magH = (float)ret->h / (float)data->oh;
    else
	magH = 1.0;

    if (magW > magH)
	data->zoom = magW;
    else
	data->zoom = magH;

    data->cur = ttk_scale_surface (data->src, data->zoom);
    if (ttk_screen->bpp == 2)
	data->cur = floyd_steinberg_dither (data->cur);

    ttk_surface_get_dimen (data->cur, &data->cw, &data->ch);

    ret->dirty = 1;
    return ret;
}

void ttk_imgview_draw (TWidget *this, ttk_surface srf) 
{
    int dx, dy;
    _MAKETHIS;
    
    dx = dy = 0;
    
    if (data->cw < this->w)
	dx = (this->w - data->cw) / 2;
    if (data->ch < this->h)
	dy = (this->h - data->ch) / 2;
    
    ttk_blit_image_ex (data->cur, data->sx, data->sy, data->cw, data->ch,
		       srf, dx, dy);
}

int ttk_imgview_scroll (TWidget *this, int dir) 
{
    _MAKETHIS;
    int oldsx = data->sx, oldsy = data->sy;
    
    dir *= 20;

    if (data->scrolldir == SDIR_X) {
	if (data->sx + this->w < data->cw) {
	    data->sx += dir;
	    if (data->sx + this->w >= data->cw)
		data->sx = data->cw - this->w - 1;
	    if (data->sx < 0)
		data->sx = 0;
	}
    }
    else if (data->scrolldir == SDIR_Y) {
	if (data->sy + this->h < data->ch) {
	    data->sy += dir;
	    if (data->sy + this->h >= data->ch)
		data->sy = data->ch - this->h - 1;
	    if (data->sy < 0)
		data->sy = 0;
	}
    }

    if ((oldsx != data->sx) || (oldsy != data->sy))
	this->dirty++;

    return 0;
}

int ttk_imgview_down (TWidget *this, int button) 
{
    _MAKETHIS;
    float oldzoom = data->zoom;

    switch (button) {
    case TTK_BUTTON_PREVIOUS:
	data->zoom /= 1.5;
	data->sx = data->sx * 2 / 3;
	data->sy = data->sy * 2 / 3;
	break;
    case TTK_BUTTON_NEXT:
	if (data->cw * 2 / 3 < 2000) {
	    data->zoom *= 1.5;
	    data->sx = data->sx * 3 / 2;
	    data->sy = data->sy * 3 / 2;
	}
	break;
    case TTK_BUTTON_ACTION:
	data->scrolldir = !data->scrolldir;
	break;
    case TTK_BUTTON_PLAY:
	if (data->ozoom > 0.01) {
	    data->zoom = data->ozoom;
	    data->ozoom = 0.0;
	} else {
	    data->ozoom = data->zoom;
	    data->zoom = 1.0;
	}
	break;
    case TTK_BUTTON_MENU:
	if (ttk_hide_window (this->win) == -1) {
	    ttk_quit();
	    exit (0);
	}
	break;
    default:
	return TTK_EV_UNUSED;
    }

    if (oldzoom != data->zoom) {
	ttk_free_surface (data->cur);

	data->cur = ttk_scale_surface (data->src, data->zoom);
	if (ttk_screen->bpp <= 8)
	    data->cur = floyd_steinberg_dither (data->cur);
	
	ttk_surface_get_dimen (data->cur, &data->cw, &data->ch);

	if (data->sx + this->w > data->cw)
	    data->sx = data->cw - this->w;
	if (data->sy + this->h > data->ch)
	    data->sy = data->ch - this->h;
	if (data->sx < 0) data->sx = 0;
	if (data->sy < 0) data->sy = 0;
	
	this->dirty++;
    }

    return 0;
}

void ttk_imgview_free (TWidget *this)
{
    _MAKETHIS;
    ttk_free_surface (data->cur);
    free (data);
}

TWindow *ttk_mh_imgview (struct ttk_menu_item *item)
{
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, ttk_new_imgview_widget (item->menuwidth, item->menuheight, (ttk_surface)item->data));
    ttk_window_title (ret, item->name);
    ret->widgets->v->draw (ret->widgets->v, ret->srf);
    return ret;
}

void *ttk_md_imgview (ttk_surface srf) 
{
    return (void *)srf;
}
