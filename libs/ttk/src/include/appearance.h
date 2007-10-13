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

#ifndef _TTK_APPEARANCE_H_
#define _TTK_APPEARANCE_H_

#define TTK_AP_COLOR        01
#define TTK_AP_IMAGE        02
#define TTK_AP_SPACING      04
#define TTK_AP_RRECT        010
#define TTK_AP_GRADIENT     020
#define TTK_AP_GRAD_MID     040
#define TTK_AP_GRAD_HORIZ   0100
#define TTK_AP_GRAD_BAR     01000
#define TTK_AP_GRAD_TPERC   010000
#define TTK_AP_GRAD_RPERC   020000
#define TTK_AP_GRAD_BPERC   040000
#define TTK_AP_GRAD_LPERC   0100000
#define TTK_AP_ROUNDING     0200000

#define TTK_AP_IMG_HLEFT    0000
#define TTK_AP_IMG_HCENTER  0200
#define TTK_AP_IMG_HRIGHT   0400
#define TTK_AP_IMG_HAMASK   0600

#define TTK_AP_IMG_VTOP     00000
#define TTK_AP_IMG_VCENTER  02000
#define TTK_AP_IMG_VBOTTOM  04000
#define TTK_AP_IMG_VAMASK   06000

typedef struct TApItem
{
    char *name;
    int type; // bitwise OR of TTK_AP_* values
    ttk_color color;
    ttk_surface img;
    int spacing;
    int rounding;
    int rx, ry, rw, rh;
    ttk_color gradstart, gradend, gradmid, gradwith;
    int gbt, gbr, gbb, gbl; // grad bar top/right/bottom/left
    struct TApItem *next; // used in some cases
} TApItem;

void ttk_ap_load (const char *filename);

TApItem *ttk_ap_get (const char *prop);  /* get a property, null if fail */
TApItem *ttk_ap_getx (const char *prop); /* get a property, black if fail */

/* get primary, fall back to secondary, fall back to def_color */
/* returns a valid TApItem */
TApItem *ttk_ap_getx_fb_dc(     const char *primary,
                                const char *secondary,
                                ttk_color def_color );

/* get primary, fall back to secondary, fall back to def_color */
/* returns a valid ttk_color */
ttk_color ttk_ap_getx_color_fb(	const char *primary,
                                const char *secondary,
                                ttk_color def_color );

/* macros to help out for simple fallback-to-color*/
#define ttk_ap_getx_dc( p, c )		ttk_ap_getx_fb_dc( (p), NULL, (c) )
#define ttk_ap_getx_color( p, c )	ttk_ap_getx_color_fb( (p), NULL, (c) )


/* and graphics primitives */
void ttk_ap_hline (ttk_surface srf, TApItem *ap, int x1, int x2, int y);
void ttk_ap_vline (ttk_surface srf, TApItem *ap, int x, int y1, int y2);
void ttk_ap_dorect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2, int filled);
void ttk_ap_rect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2);
void ttk_ap_fillrect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2);

#endif
