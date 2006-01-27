/*
 * Copyright (C) 2004 Bernard Leach, Scott Lawrence, Joshua Oreman, et al.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pz.h"

static int decorations;			/* which decoration set to use */
static int decoration_colors_dirty = 0; /* have the colors changed on us? */

extern int ipod_read_apm(int *battery, int *charging);

static int make_dirty (TWidget *this) { this->dirty++; ttk_dirty |= TTK_DIRTY_HEADER; return 0; }


static void battery_draw (TWidget *this, ttk_surface srf) 
{
	TApItem fill;
	TApItem *ap;
	char buf[32];
	int battery_fill, battery_is_charging;
	ttk_color c;

	ipod_read_apm(&battery_fill, &battery_is_charging);

	/* draw the level as digits/text if that's the user's setting */
	if (pz_get_int_setting (pz_global_config, BATTERY_DIGITS)) {
		battery_fill = battery_fill * 1000 / 512;
	    
		if( battery_fill < 100 ) 
			c = ttk_ap_getx( "battery.fill.low" )->color;
		else
			c = ttk_ap_getx( "battery.fill.normal" )->color;

		if (battery_fill >= 1000) 
			strcpy (buf, "Chrg");
		else
			snprintf( buf, 32, "%c%3d", 
				    (battery_is_charging)?'+':' ',
				    battery_fill );

		pz_vector_string( srf, buf, 
				(this->x)-((decorations == PZ_DEC_AMIGA20)?1:0),
				this->y, 5, 9, 1, c );
		return;
	}

	if (battery_is_charging) {
		memcpy (&fill, ttk_ap_getx ("battery.fill.charge"), 
			sizeof(TApItem));
		ap = ttk_ap_get( "battery.bg.charging" );

	} else if (battery_fill < 64) {
		memcpy (&fill, ttk_ap_getx ("battery.fill.low"), 
			sizeof(TApItem));
		ap = ttk_ap_get( "battery.bg.low" );

	} else {
		memcpy (&fill, ttk_ap_getx ("battery.fill.normal"), 
			sizeof(TApItem));
		ap = ttk_ap_get( "battery.bg" );
	}

	/* fill the container... */
	ttk_ap_fillrect (srf, ap, this->x + 4, this->y + 1, 
				this->x + 22, this->y + 9);
	ttk_draw_icon (ttk_icon_battery, srf, this->x, this->y, 
				ttk_ap_getx ("battery.border")->color, 
				ttk_ap_getx ("header.bg")->color);

	if (fill.type & TTK_AP_SPACING) {
		fill.type &= ~TTK_AP_SPACING;
		ttk_ap_fillrect (srf, &fill,
				this->x + 4 + fill.spacing,
				this->y + 1 + fill.spacing,
				this->x + 4 + fill.spacing 
				+ ((battery_fill * (17 - 2*fill.spacing)
				    + battery_fill / 2) / 512),
				this->y + 9 - fill.spacing);
	} else {
		ttk_ap_fillrect (srf, &fill,
				this->x + 4, this->y + 1,
				 this->x + 4 + ((battery_fill * 17 
				    + battery_fill / 2) / 512),
				 this->y + 9);
	}

	/* overlay the charge icon if we're charging */
	if (battery_is_charging)
		ttk_draw_icon (ttk_icon_charging, srf, this->x, this->y,
			ttk_ap_getx ("battery.border")->color, 
			ttk_ap_getx ("header.bg")->color);
}

static TWidget *new_battery_indicator() 
{
	TWidget *ret = ttk_new_widget (0, 0);
	ret->w = 25;
	ret->h = 10;
	ret->draw = battery_draw;
	ret->timer = make_dirty;
	ttk_widget_set_timer (ret, 1000);

	return ret;
}

/** Load average **/
static double get_load_average( void )
{
	double ret = 0.00;
#ifdef __linux__
	FILE * file;
	float f;

	file = fopen( "/proc/loadavg", "r" );
	if( file ) {
		fscanf( file, "%f", &f );
		ret = f;
		fclose( file );
	} else {
		ret = 0.50;
	}
#else
#ifdef __APPLE__
	double avgs[3];
	if( getloadavg( avgs, 3 ) < 0 ) return( 0.50 );
	ret = avgs[0];
#else 
	ret = (float)(rand()%20) * 0.05;
#endif
#endif
	return( ret );
}


#define LAVG_WIDTH 19
#define _LAMAKETHIS int *avgs = (int *)this->data

static void loadavg_draw (TWidget *this, ttk_surface srf) 
{
	double avg;
	int i;
	/*_LAMAKETHIS; */

	avg = get_load_average();

	i = (this->h-1) - (avg * this->h-1);
	if( i < 0 ) i = 0;
	if( i > (this->h-1)) i= this->h-1;

	/* backing */
	ttk_ap_fillrect (srf, ttk_ap_get ("loadavg.bg"), 
				this->x, this->y,
				this->x + this->w - 2, 
				this->y + i);

	/* body */
	ttk_ap_fillrect (srf, ttk_ap_get ("loadavg.fg"), 
				this->x, this->y+i,
				this->x + this->w - 2, 
				this->y + this->h - 1);

	/* spike line */
	ttk_ap_hline( srf, ttk_ap_get( "loadavg.spike" ),
			    this->x, this->x + this->w - 3, this->y+i );
}

static void loadavg_destroy (TWidget *this) 
{ free (this->data); }

static TWidget *new_load_average_display() 
{
	TWidget *ret = ttk_new_widget (0, 0);
	ret->w = LAVG_WIDTH;
	ret->h = ttk_screen->wy - 2;
	
	ret->data = calloc (LAVG_WIDTH, sizeof(int));
	ret->draw = loadavg_draw;
	ret->timer = make_dirty;
	ret->destroy = loadavg_destroy;
	ttk_widget_set_timer (ret, 1000);

	return ret;
}


/** Hold status **/
extern int pz_hold_is_on;

static TWidget *hwid = 0;

static void hold_unset (TWidget *this) 
{ hwid = 0; }

static void hold_draw (TWidget *this, ttk_surface srf) 
{
    if (pz_hold_is_on) {
	ttk_draw_icon (ttk_icon_hold, srf, this->x + 3, this->y, ttk_ap_getx ("lock.border")->color, 0);
    }
}

static TWidget *new_hold_status_indicator()
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = 14;
    ret->h = 10;
    ret->draw = hold_draw;
    ret->destroy = hold_unset;

    hwid = ret;
    return ret;
}

extern TWindow *pz_last_window;
char *pz_next_header = 0;
#define NHWID 5
static int hwid_left[NHWID], hwid_lnext = 0, hwid_right[NHWID], hwid_rnext = 0;
static TWidget *hwid_lwid[NHWID], *hwid_rwid[NHWID];
static int local = 0;
static TWidget *llwid = 0, *lrwid = 0;

void pz_hwid_pack_left (TWidget *wid) 
{
    int lx = 0;
    if (hwid_lnext != 0) {
	lx = hwid_left[hwid_lnext - 1];
    }
    lx += 2;
    
    hwid_lwid[hwid_lnext] = wid;
    hwid_left[hwid_lnext++] = lx + wid->w;
    wid->x = lx;
    wid->y = (ttk_screen->wy - wid->h) / 2;
    ttk_add_header_widget (wid);
}

void pz_hwid_pack_right (TWidget *wid)
{
    int lx = ttk_screen->w;
    if (hwid_rnext != 0) {
	lx = hwid_right[hwid_rnext - 1];
    }
    lx -= 2 + wid->w;
    
    hwid_rwid [hwid_rnext] = wid;
    hwid_right[hwid_rnext++] = lx;
    wid->x = lx;
    wid->y = (ttk_screen->wy - wid->h) / 2;
    ttk_add_header_widget (wid);
}

void pz_hwid_unpack (TWidget *wid) 
{
    int i;
    for (i = 0; i < NHWID; i++) {
	if (hwid_lwid[i] == wid) {
	    hwid_lnext--;
	    memmove (hwid_lwid + i, hwid_lwid + i + 1, (NHWID - i - 1) * sizeof(TWidget*));
	    memmove (hwid_left + i, hwid_left + i + 1, (NHWID - i - 1) * sizeof(int));
	    break;
	} else if (hwid_rwid[i] == wid) {
	    hwid_rnext--;
	    memmove (hwid_rwid + i, hwid_rwid + i + 1, (NHWID - i - 1) * sizeof(TWidget*));
	    memmove (hwid_right + i, hwid_right + i + 1, (NHWID - i - 1) * sizeof(int));
	    break;
	}
    }
    if (i == NHWID) {
	pz_error ("Unpacking %%%p: not in header\n");
    } else {
	ttk_remove_header_widget (wid);
    }
}

void pz_hwid_reset() 
{
    local = 0;

    while (hwid_lnext)
	pz_hwid_unpack (hwid_lwid[0]);

    while (hwid_rnext)
	pz_hwid_unpack (hwid_rwid[0]);

    hwid_lnext = hwid_rnext = 0;
}

void pz_header_set_local (TWidget *left, TWidget *right) 
{
    int i;

    if (local) {
	pz_error ("Local header created from within local header\n");
	return;
    }

    local++;

    if (left) {
	for (i = 0; i < hwid_lnext; i++)
	    ttk_remove_header_widget (hwid_lwid[i]);
	left->x = 2;
	left->y = (ttk_screen->wy - left->h) / 2;
	ttk_add_header_widget (left);
	llwid = left;
    }

    if (right) {
	for (i = 0; i < hwid_rnext; i++)
	    ttk_remove_header_widget (hwid_rwid[i]);
	right->x = ttk_screen->w - 2 - right->w;
	right->y = (ttk_screen->wy - right->h) / 2;
	ttk_add_header_widget (right);
	lrwid = left;
    }
}

void pz_header_unset_local() 
{
    int i;

    if (!local) {
	pz_error ("Local header destroyed where it didn't exist");
	return;
    }

    if (llwid) {
	ttk_remove_header_widget (llwid);
	for (i = 0; i < hwid_lnext; i++)
	    ttk_add_header_widget (hwid_lwid[i]);
	llwid = 0;
    }
    
    if (lrwid) {
	ttk_remove_header_widget (lrwid);
	for (i = 0; i < hwid_rnext; i++)
	    ttk_add_header_widget (hwid_rwid[i]);
	lrwid = 0;
    }

    local--;
}

void pz_header_set_decorations (int decor) 
{
	if( decorations != decor )  decoration_colors_dirty++;
	decorations = decor;
}

static ttk_color gradcol[32];           /* gradient color buffer */
void pz_header_colors_dirty( void )
{
	decoration_colors_dirty++;
}

/** Decorations: **/
static void draw_decorations (TWidget *this, ttk_surface srf)
{
	int width = ttk_text_width (ttk_menufont, ttk_windows->w->title);

	/* draw the decorations */
	if (decorations == PZ_DEC_AMIGA13 || decorations == PZ_DEC_AMIGA11) {
		/* drag bars */
		if (decorations == PZ_DEC_AMIGA13) {
			/* 1.2/1.3 look */
			int o = ttk_screen->wy / 4;
			ttk_ap_fillrect (srf, ttk_ap_get ("header.fg"),
					ttk_screen->wy + o,
					o,
					hwid_right[hwid_rnext-1]-6 - o + 1,
					o*2-1 );
			ttk_ap_fillrect (srf, ttk_ap_get ("header.fg"), 
					ttk_screen->wy + o,
					ttk_screen->wy - o*2 + 1,
					hwid_right[hwid_rnext-1]-6 - o + 1,
					ttk_screen->wy - o);
	
		} else {
			/* 1.1/1.0 look */
			int i;
			for (i = 1; i < ttk_screen->wy; i += 2) {
				ttk_line (srf, 
					ttk_screen->wy, i,
					hwid_right[hwid_rnext-1]-6, i,
					ttk_ap_getx ("header.fg") -> color);
			}
		}

		/* vertical widget separators */
		ttk_ap_fillrect( srf, ttk_ap_get ("header.fg"), 
			    ttk_screen->wy-1, 0,
			    ttk_screen->wy+1, ttk_screen->wy - 1 );
		ttk_ap_fillrect( srf, ttk_ap_get ("header.fg"), 
			    hwid_right[hwid_rnext-1] - 4, 0,
			    hwid_right[hwid_rnext-1] - 6, ttk_screen->wy - 1 );

		/* faux close widget */
		if ( !pz_get_int_setting (pz_global_config, DISPLAY_LOAD) 
			&& !pz_hold_is_on) {
			double xo = ((float)ttk_screen->wy) / 8.0;
			double yo = ((float)ttk_screen->wy) / 8.0;
			/* this should be tweaked to look better on mini */
			/* i think that the proportions are wrong also */

			ttk_ap_fillrect( srf, ttk_ap_get ("header.fg"), 
					(int) (xo*1),  (int) (yo*1),
					(int) (xo*7),  (int) (yo*7) );
			ttk_ap_fillrect( srf, ttk_ap_get ("header.bg"),
					(int) (xo*1.5),  (int) (yo*1.5),
					(int) (xo*6.5), (int) (yo*6.5) );
			ttk_ap_fillrect (srf, ttk_ap_get ("header.accent"), 
					(int) (xo*3.1), (int) (yo*3.1),
					(int) (xo*4.8), (int) (yo*4.8) );
		}

		/* clear text area */
		if (ttk_ap_getx ("header.bg") -> type & TTK_AP_COLOR) {
			ttk_ap_fillrect( srf, ttk_ap_get ("header.bg"),
				 (ttk_screen->w - width) / 2 - 4, 0,
				 (ttk_screen->w + width) / 2 + 4, 
				 ttk_screen->wy - 2 );
		}

    } else if (decorations == PZ_DEC_AMIGA20) {
		/* top */
		ttk_line( srf, 0, 0, this->w, 0,
				ttk_ap_getx( "header.shine" )->color );
		/* left */
		ttk_line( srf, 0, 0, 0, ttk_screen->wy-2,
				ttk_ap_getx( "header.shine" )->color );
		/* bottom - handled by header.line */
		/* right */
		ttk_line( srf, ttk_screen->w-1, 1,
				ttk_screen->w-1, ttk_screen->wy-2,
				ttk_ap_getx( "header.shadow" )->color );

		/* right widget vertical border bar */
		ttk_line( srf, hwid_right[hwid_rnext-1]-5, 1,
			       hwid_right[hwid_rnext-1]-5, ttk_screen->wy - 1,
			       ttk_ap_getx( "header.shadow" )->color );
		ttk_line( srf, hwid_right[hwid_rnext-1]-4, 1,
			       hwid_right[hwid_rnext-1]-4, ttk_screen->wy - 1,
			       ttk_ap_getx( "header.shine" )->color );

		/* left widget vertical border bar */
		ttk_line( srf, ttk_screen->wy-2, 0,
				ttk_screen->wy-2, ttk_screen->wy - 1,
				ttk_ap_getx( "header.shadow" )->color );
		ttk_line( srf, ttk_screen->wy-1, 0,
				ttk_screen->wy-1, ttk_screen->wy - 1,
				ttk_ap_getx( "header.shine" )->color );

		/* left widget - faux close button */
		if ( !pz_get_int_setting (pz_global_config, DISPLAY_LOAD) 
			&& !pz_hold_is_on) {
			double xo = ((float)ttk_screen->wy) / 7.0;
			double yo = ((float)ttk_screen->wy) / 7.0;
			ttk_fillrect( srf, xo*2.6, yo*2, xo*4.4, yo*5,
				ttk_ap_getx( "header.shine" )->color );
			ttk_rect( srf, xo*2.6, yo*2, xo*4.4, yo*5,
				ttk_ap_getx( "header.shadow" )->color );
		}

    } else if (decorations == PZ_DEC_MROBE) {
	// . X X X .
	// X X X X X
	// X X X X X
	// X X X X X
	// . X X X .
	unsigned short circle[] = { 0x7000, 0xf800, 0xf800, 0xf800, 0x7000 };
	int i;
	// Ugh. TTK doesn't export a proper bitmap function. Yet.
	ttk_gc gc = ttk_new_gc();
	ttk_gc_set_foreground (gc, ttk_ap_getx ("header.accent") -> color);
	TWindow *pixmap = malloc (sizeof(TWindow)); pixmap->srf = srf;
	
	/* draw left side */
	for (i = ((ttk_screen->w - width) >> 1) - 10;
	     i > hwid_left[hwid_lnext-1] + 6;
	     i -= 11) {
	    t_GrBitmap (pixmap, gc, i - 2, (ttk_screen->wy >> 1) - 2, 5, 5, circle);
	}

	/* draw right side */
	for (i = ((ttk_screen->w + width) >> 1) + 7;
	     i < hwid_right[hwid_rnext-1] - 6;
	     i += 11) {
	    t_GrBitmap (pixmap, gc, i - 2, (ttk_screen->wy >> 1) - 2, 5, 5, circle);
	}

	ttk_free_gc (gc);
	free (pixmap);

	/* clear text area */
	if (ttk_ap_getx ("header.bg") -> type & TTK_AP_COLOR) {
	    ttk_ap_fillrect (srf, ttk_ap_get ("header.bg"),
			     (ttk_screen->w - width) / 2, 0,
			     (ttk_screen->w + width) / 2, ttk_screen->wy - 2);
	}
    } else if(  (decorations == PZ_DEC_BIGRAD) ||
		(decorations == PZ_DEC_TRIGRAD) ||
		(decorations == PZ_DEC_BIGRADBAR) ||
		(decorations == PZ_DEC_TRIGRADBAR) ){
	int y;

	if( decoration_colors_dirty )
	{
		int rt, gt, bt;
		int rm, gm, bm;
		int rb, gb, bb;
		int h2 = this->h/2;

		ttk_unmakecol_ex( ttk_ap_getx( "header.gradient.top" )->color,
						&rt, &gt, &bt, srf);
		ttk_unmakecol_ex( ttk_ap_getx( "header.gradient.bottom" )->color,
						&rb, &gb, &bb, srf);

		if( (decorations == PZ_DEC_TRIGRAD)
		    || (decorations == PZ_DEC_TRIGRADBAR ))
		{
		    ttk_unmakecol_ex( ttk_ap_getx( "header.gradient.middle" )->color,
						&rm, &gm, &bm, srf);
		} else {
		    rm = ( rb + rt ) >> 1;
		    gm = ( gb + gt ) >> 1;
		    bm = ( bb + bt ) >> 1;
		}

		ttk_unmakecol_ex( ttk_ap_getx( "header.gradient.top" )->color,
						&rt, &gt, &bt, srf);

		/* that is; when the color scheme is changed, compute an array
		    of 22 or however many lines it is... */
		for( y=0 ; y<this->h ; y++ )
		{
			if( y<h2 ) {
			    int h2my = h2-y;
			    gradcol[y] = ttk_makecol_ex( 
					(rm * y)/h2 + (rt * (h2my)/h2),
					(gm * y)/h2 + (gt * (h2my)/h2),
					(bm * y)/h2 + (bt * (h2my)/h2),
					srf );
			} else {
			    int yy = y-h2;
			    int h2my = h2-yy;
			    gradcol[y] = ttk_makecol_ex( 
					(rb * yy)/h2 + (rm * (h2my)/h2),
					(gb * yy)/h2 + (gm * (h2my)/h2),
					(bb * yy)/h2 + (bm * (h2my)/h2),
					srf );
			}
		}

		/* and clear the flag... */
		decoration_colors_dirty = 0;
	}

	/* draw the gradient to the header... */
	for( y=0 ; y<this->h ; y++ )
	{
	    ttk_line( srf, 0, y, this->w, y, gradcol[y] );
	}

	/* fill the top if it's a half-gradient */
	if( (    (decorations == PZ_DEC_BIGRADBAR)
	      || (decorations == PZ_DEC_TRIGRADBAR)
	    ) && ttk_ap_get( "header.gradient.bar" ) )
	{
	    int spacing =  ttk_ap_get( "header.gradient.bar" )->spacing;
	    int h2 = this->h>>1;

	    /* spacing seems like it gets corrupted? this patches it. */
	    if( ( spacing < (h2*-1)) || ( spacing > h2) ) spacing = 0;

	    if( spacing > 0 )
		    ttk_fillrect( srf, 
			    0, spacing,
			    this->w, this->h>>1, 
			    ttk_ap_get ("header.gradient.bar")->color );
	    if( spacing < 0 ) {
		    spacing *= -1;
		    ttk_fillrect( srf, 
			    0, (this->h>>1) - spacing,
			    this->w, (this->h>>1) + spacing, 
			    ttk_ap_get ("header.gradient.bar")->color );
	    }
	}
    }    
}

static TWidget *new_decorations_widget() 
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = ttk_screen->w;
    ret->h = ttk_screen->wy;
    ret->draw = draw_decorations;
    return ret;
}


static TWidget *decwid = 0;

void pz_header_fix_hold() 
{
    ttk_dirty |= TTK_DIRTY_HEADER;
}

static int inited = 0;
static TWidget *farleft_wids[NHWID];
int farleft_next = 0;
void pz_hwid_put_left (TWidget *wid) 
{
    if (inited)
	pz_hwid_pack_left (wid);
    else
	farleft_wids[farleft_next++] = wid;
}

void pz_header_init() 
{
    if (decwid) ttk_remove_header_widget (decwid);
    ttk_add_header_widget ((decwid = new_decorations_widget()));

    pz_hwid_reset();

    if (pz_get_int_setting (pz_global_config, DISPLAY_LOAD))
	pz_hwid_pack_left (new_load_average_display());

    if (farleft_next > 0) {
	while (--farleft_next) {
	    pz_hwid_pack_left (farleft_wids[farleft_next]); // will put most recently pushed furthest to the left
	}
    }
	
    pz_hwid_pack_left (new_hold_status_indicator());
    pz_hwid_pack_right (new_battery_indicator());

    inited++;
}
