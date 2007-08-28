/*
 * Copyright (c) 2005 Joshua Oreman
 *
 * This file is a part of TTK.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ttk.h"
#include <stdlib.h>

#define _MAKETHIS slider_data *data = (slider_data *)this->data
extern ttk_screeninfo *ttk_screen;

typedef struct _slider_data
{
    int w; // used only in menu thing
    int min, max;
    int lastval, *val;
    void (*callback)(int cdata, int val);
    int cdata;
    ttk_surface empty;
    ttk_surface full;
    const char *label;
    int epoch;
} slider_data;

static int min_spacing()
{
#define SMIN(y,z) y < z ? y : z
    int s;

    s = SMIN(ttk_ap_getx("slider.border")->spacing, 0);
    s = SMIN(ttk_ap_getx("slider.bg")->spacing, s);
    s = SMIN(ttk_ap_getx("slider.full")->spacing, s);
    return s;
}

static void ttk_slider_setup_surfaces(slider_data *data, int w, int h, int s)
{
    TApItem *ap;

    if (data->empty) ttk_free_surface(data->empty);
    if (data->full) ttk_free_surface(data->full);
    data->empty = ttk_new_surface(w-s*2, h-s*2, ttk_screen->bpp);
    data->full = ttk_new_surface(w-s*2, h-s*2, ttk_screen->bpp);

    if ((ap = ttk_ap_get("window.bg"))) {
    	ttk_fillrect(data->empty, 0, 0, w-s*2, h-s*2, ap->color);
    	ttk_fillrect(data->full, 0, 0, w-s*2, h-s*2, ap->color);
    }
    if ((ap = ttk_ap_get("slider.bg")))
	ttk_ap_fillrect(data->empty, ap, -s+1, -s+1, w-1-s, h-1-s);
    if ((ap = ttk_ap_get("slider.border"))) {
	ttk_ap_rect(data->empty, ap, -s, -s, w-s, h-s);
	ttk_ap_rect(data->full, ap, -s, -s, w-s, h-s);
    }
    if ((ap = ttk_ap_get("slider.full")))
	ttk_ap_fillrect(data->full, ap, -s+1, -s+1, w-1-s, h-1-s);
}

// *val must stay allocated until you either set a callback or destroy the widget.
TWidget *ttk_new_special_slider_widget (int x, int y, int w, int h, int minval,
		int maxval, int *val, const char *title)
{
    TWidget *ret = ttk_new_widget (x, y);
    slider_data *data = calloc (sizeof(slider_data), 1);
    int s = min_spacing();
    
    ret->w = w-s*2;
    ret->h = (h-s*2) + (15*!!title);
    ret->data = data;
    ret->focusable = 1;
    ret->draw = ttk_slider_draw;
    ret->down = ttk_slider_down;
    ret->scroll = ttk_slider_scroll;
    ret->destroy = ttk_slider_free;

    data->label = title;
    data->min = minval;
    data->max = maxval;
    data->val = val;
    data->lastval = *val;
    data->callback = 0;
    data->epoch = ttk_epoch;

    ttk_slider_setup_surfaces(data, w, h, s);

    ret->dirty = 1;
    return ret;
}

TWidget *ttk_new_slider_widget (int x, int y, int w, int minval, int maxval, int *val, const char *title)
{ return ttk_new_special_slider_widget(x,y, w,11, minval,maxval, val, title); }

void ttk_slider_set_callback (TWidget *this, void (*callback)(int,int), int cdata) 
{
    _MAKETHIS;
    data->callback = callback;
    data->cdata = cdata;
}

void ttk_slider_set_bar (TWidget *this, ttk_surface empty, ttk_surface full) 
{
    _MAKETHIS;

    if (data->empty) ttk_free_surface (data->empty);
    if (data->full) ttk_free_surface (data->full);

    data->empty = empty;
    data->full = full;
}

void ttk_slider_set_val(TWidget * this, int newval)
{
    slider_data * sdata=this->data;
    sdata->lastval=newval;
    if (sdata->val) *(sdata->val) = newval;
}

void ttk_slider_draw (TWidget *this, ttk_surface srf)
{
    _MAKETHIS;
    int y = this->y;
    int X;
    int h = this->h - (15*!!data->label);

    if (data->epoch < ttk_epoch) {
	ttk_slider_setup_surfaces(data, this->w, h, min_spacing());
    }

    if (data->label) {
	ttk_text (srf, ttk_menufont, this->x + ((this->w + ttk_text_width (ttk_menufont, data->label)) / 2),
		  this->y, ttk_makecol (BLACK), data->label);
	y += 15;
    }

    X = (data->lastval - data->min) * this->w / (data->max - data->min);
    if (!X) X = 1; // show left edge of the "full" graphic

    ttk_blit_image_ex (data->full, 0, 0, X, h, srf, this->x, y);
    ttk_blit_image_ex (data->empty, this->w - (this->w - X), 0,
		       this->w - X, h, srf, this->x + X, y);
}

int ttk_slider_scroll (TWidget *this, int dir)
{
    _MAKETHIS;
    int oldval;
    
#ifdef IPOD
    static int sofar = 0;
    sofar += dir;
    if (!(sofar * (data->max - data->min) / 96))
	return 0;
    dir = sofar;
    sofar = 0;
    dir = dir * (data->max - data->min) / 96;
#endif

    oldval = data->lastval;

    data->lastval += dir;
    if (data->lastval > data->max) data->lastval = data->max;
    if (data->lastval < data->min) data->lastval = data->min;
    
    if (data->callback)
	data->callback (data->cdata, data->lastval);
    else if (data->val)
	*data->val = data->lastval;

    if (oldval != data->lastval) {
	this->dirty++;
	return TTK_EV_CLICK;
    }

    return 0;
}

int ttk_slider_down (TWidget *this, int button) 
{
    switch (button) {
    case TTK_BUTTON_MENU:
	ttk_hide_window (this->win);
	break;
    default:
	return TTK_EV_UNUSED;
    }
    return 0;
}

void ttk_slider_free (TWidget *this) 
{
    _MAKETHIS;
    ttk_free_surface (data->empty);
    ttk_free_surface (data->full);
    free (data);
}

TWindow *ttk_mh_slider (struct ttk_menu_item *this) 
{
    _MAKETHIS;
    
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, ttk_new_slider_widget (0, 0, data->w,
						data->min, data->max, data->val, 0));
    ttk_window_title (ret, this->name);
    ttk_set_popup (ret);
    ret->widgets->v->draw (ret->widgets->v, ret->srf);
    free (data);
    return ret;
}

void *ttk_md_slider (int w, int min, int max, int *val) 
{
    slider_data *data = calloc (sizeof(slider_data), 1);
    data->w = w;
    data->min = min;
    data->max = max;
    data->val = val;
    return (void *)data;
}
