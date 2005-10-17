/*
 * Copyright (C) 2004 Bernard Leach, Scott Lawrence, etc.
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
#define PZMOD
#include "pz.h"
#include "appearance.h"
#include "ipod.h"


int make_dirty (TWidget *this) { this->dirty++; return 0; }

/** Battery charge: **/
void battery_draw_digit (ttk_surface srf, int x, int y, int w, int h, ttk_color col, char digit) 
{
    int sx = x, sy = y, ex = x + w - 1, ey = y + h - 1;
    int my = sy + (ey - sy) / 2, ty = sy + (my - sy) / 2, by = ey - (ey - my) / 2;

    switch (digit) {
    // Numbers:
    //  [0]   [1]   [2]    [3]    [4] 
    // +---+   +   +---+  +---+  +   +
    // |   |   |       |      |  |   |
    // |   |   |       |      |  |   |
    // |   |   |       |      |  |   |
    // |   |   |   +---+  +---+  +---+
    // |   |   |   |          |      |
    // |   |   |   |          |      |
    // |   |   |   |          |      |
    // |   |   |   |          |      |
    // +---+   +   +---+  +---+      +
    case '0':
	ttk_rect (srf, sx, sy, ex, ey, col);
	break;
    case '1':
	ttk_line (srf, ex, sy, ex, ey, col);
	break;
    case '2':
	ttk_line (srf, sx, sy, ex, sy, col);
	ttk_line (srf, ex, sy, ex, my, col);
	ttk_line (srf, sx, my, ex, my, col);
	ttk_line (srf, sx, my, sx, ey, col);
	ttk_line (srf, sx, ey, ex, ey, col);
	break;
    case '3':
	ttk_line (srf, sx, sy, ex, sy, col);
	ttk_line (srf, ex, sy, ex, ey, col);
	ttk_line (srf, sx, my, ex, my, col);
	ttk_line (srf, sx, ey, ex, ey, col);
	break;
    case '4':
	ttk_line (srf, sx, sy, sx, my, col);
	ttk_line (srf, sx, my, ex, my, col);
	ttk_line (srf, ex, sy, ex, ey, col);
	break;
    //  [5]    [6]    [7]    [8]    [9]
    // +---+  +---+  +---+  +---+  +---+
    // |      |      |   |  |   |  |   |
    // |      |      +   |  |   |  |   |
    // |      |          |  |   |  |   |
    // +---+  +---+      |  +---+  +---+
    //     |  |   |      |  |   |      |
    //     |  |   |      |  |   |      |
    //     |  |   |      |  |   |      |
    //     |  |   |      |  |   |      |
    // +---+  +---+      +  +---+  +---+
    case '5':
	ttk_line (srf, sx, sy, ex, sy, col);
	ttk_line (srf, sx, sy, sx, my, col);
	ttk_line (srf, sx, my, ex, my, col);
	ttk_line (srf, ex, my, ex, ey, col);
	ttk_line (srf, sx, ey, ex, ey, col);
	break;
    case '6':
	ttk_line (srf, sx, sy, ex, sy, col);
	ttk_line (srf, sx, sy, sx, ey, col);
	ttk_rect (srf, sx, my, ex, ey, col);
	break;
    case '7':
	ttk_line (srf, sx, sy, sx, ty, col);
	ttk_line (srf, sx, sy, ex, sy, col);
	ttk_line (srf, ex, sy, ex, ey, col);
	break;
    case '8':
	ttk_rect (srf, sx, sy, ex, ey, col);
	ttk_line (srf, sx, my, ex, my, col);
	break;
    case '9':
	ttk_rect (srf, sx, sy, ex, my, col);
	ttk_line (srf, ex, my, ex, ey, col);
	ttk_line (srf, sx, ey, ex, ey, col);
	break;
    
    // Letters:
    //  [C]    [h]    [r]    [g]
    // +---+  +
    // |      |
    // |      |             +---+
    // |      |             |   |
    // |      +---+   +--+  |   |
    // |      |   |  ++     |   |
    // |      |   |  |      +---+
    // |      |   |  |          |
    // |      |   |  |      +   |
    // +---+  +   +  +      +---+
    case 'C':
	ttk_line (srf, sx, sy, ex, sy, col);
	ttk_line (srf, sx, sy, sx, ey, col);
	ttk_line (srf, sx, ey, ex, ey, col);
	break;
    case 'h':
	ttk_line (srf, sx, sy, sx, ey, col);
	ttk_line (srf, sx, my, ex, my, col);
	ttk_line (srf, ex, my, ex, ey, col);
	break;
    case 'r':
	ttk_line (srf, sx, my+1, sx, ey, col);
	ttk_line (srf, sx+1, my, ex, my, col);
	ttk_pixel (srf, sx+1, my+1, col);
	break;
    case 'g':
	ttk_rect (srf, sx, ty, ex, by, col);
	ttk_line (srf, ex, by, ex, ey, col);
	ttk_line (srf, sx, ey, ex, ey, col);
	ttk_pixel (srf, sx, ey-1, col);
	break;
	
    case '/':
	ttk_draw_icon (ttk_icon_charging, srf, sx, sy, 0);
	break;
    case ' ':
	break;

    default:
	fprintf (stderr, "Warning: undrawable vletter `%c'\n", digit);
	break;
    }
}

void battery_draw (TWidget *this, ttk_surface srf) 
{
    char buf[32];
    int battery_fill, battery_is_charging;

    battery_fill = ipod_get_battery_level();
    battery_is_charging = ipod_is_charging();
	

    if (ipod_get_setting (BATTERY_DIGITS)) {
	battery_fill = battery_fill * 1000 / 512;
	
	snprintf (buf, 32, "%3d", battery_fill);
	if (battery_is_charging) strcat (buf, "/");
	if (battery_fill >= 1000) strcpy (buf, "Chrg");

	char *p = buf;
	int x = this->x;
	while (*p) {
	    battery_draw_digit (srf, x, this->y, 5, this->h, ttk_ap_getx ((battery_fill < 100)? "battery.fill.low" : "battery.fill.normal") -> color, *p);
	    x += 6;
	    p++;
	}
	return;
    }

    TApItem fill;
    if (battery_is_charging) {
	memcpy (&fill, ttk_ap_getx ("battery.fill.charge"), sizeof(TApItem));
    } else if (battery_fill < 64) {
	memcpy (&fill, ttk_ap_getx ("battery.fill.low"), sizeof(TApItem));
    } else {
	memcpy (&fill, ttk_ap_getx ("battery.fill.normal"), sizeof(TApItem));
    }

    ttk_draw_icon (ttk_icon_battery, srf, this->x, this->y, (ttk_ap_getx ("battery.border")->color == ttk_makecol (WHITE)));
    ttk_ap_fillrect (srf, ttk_ap_get ("battery.bg"), this->x + 4, this->y + 1, this->x + 20, this->y + 8);

    if (fill.type & TTK_AP_SPACING) {
	fill.type &= ~TTK_AP_SPACING;
	ttk_ap_fillrect (srf, &fill, this->x + 4 + fill.spacing, this->y + 1 + fill.spacing,
			 this->x + 4 + fill.spacing + ((battery_fill * (16 - 2*fill.spacing) + battery_fill / 2) / 512),
			 this->y + 8 - fill.spacing);
    } else {
	ttk_ap_fillrect (srf, &fill, this->x + 4, this->y + 1,
			 this->x + 4 + ((battery_fill * 16 + battery_fill / 2) / 512),
			 this->y + 8);
    }

    if (battery_is_charging)
	ttk_draw_icon (ttk_icon_charging, srf, this->x, this->y, (ttk_ap_getx ("battery.border")->color == ttk_makecol (WHITE)));
}

TWidget *new_battery_indicator() 
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
#ifdef IPOD
#ifdef __linux__
	FILE * file;
	float f;
#else
	double avgs[3];
#endif
	double ret = 0.00;

#ifdef __linux__
	file = fopen( "/proc/loadavg", "r" );
	if( file ) {
		fscanf( file, "%f", &f );
		ret = f;
		fclose( file );
	} else {
		ret = 0.50;
	}
#else
	if( getloadavg( avgs, 3 ) < 0 ) return( 0.50 );
	ret = avgs[0];
#endif
	return( ret );
#else
	return (float)(rand()%20) * 0.05;
#endif
}


#define LAVG_WIDTH 20
#define _LAMAKETHIS int *avgs = (int *)this->data

void loadavg_draw (TWidget *this, ttk_surface srf) 
{
    float avg;
    int i;
    _LAMAKETHIS;

    avg = get_load_average();

    memmove (avgs, avgs + 1, sizeof(int) * (LAVG_WIDTH - 1));
    avgs[LAVG_WIDTH - 1] = (int)(avg * (float)this->h);

    ttk_ap_fillrect (srf, ttk_ap_get ("loadavg.bg"), this->x, this->y, this->x + this->w - 1, this->y + this->h - 1);
    for (i = 0; i < LAVG_WIDTH; i++) {
	if (avg < this->h) {
	    ttk_ap_hline (srf, ttk_ap_get ("loadavg.fg"), this->x + i,
			  this->y + this->h - avgs[i] - 1, this->y + this->h - 1);
	} else {
	    ttk_ap_hline (srf, ttk_ap_get ("loadavg.spike"), this->x + i,
			  this->y, this->y + this->h - 1);
	}
    }
}

void loadavg_destroy (TWidget *this) 
{ free (this->data); }

TWidget *new_load_average_display() 
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = LAVG_WIDTH;
    ret->h = ttk_screen->wy - 3;
    
    ret->data = calloc (LAVG_WIDTH, sizeof(int));
    ret->draw = loadavg_draw;
    ret->timer = make_dirty;
    ret->destroy = loadavg_destroy;
    ttk_widget_set_timer (ret, 1000);

    return ret;
}


/** Hold status **/
extern int hold_is_on;

static TWidget *hwid = 0;

void hold_unset (TWidget *this) 
{ hwid = 0; }

void hold_draw (TWidget *this, ttk_surface srf) 
{
    if (hold_is_on) {
	ttk_draw_icon (ttk_icon_hold, srf, this->x + 3, this->y, (ttk_ap_getx ("lock.border")->color == ttk_makecol (WHITE)));
    }
}

TWidget *new_hold_status_indicator()
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = 14;
    ret->h = 10;
    ret->draw = hold_draw;
    ret->destroy = hold_unset;

    hwid = ret;
    return ret;
}

#ifdef MPDC
#error Implement MPD status indicator . . .
#endif

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


/** Decorations: **/
void draw_decorations (TWidget *this, ttk_surface srf)
{
    int decorations = appearance_get_decorations();
    int width = ttk_text_width (ttk_menufont, ttk_windows->w->title);

    if (decorations == DEC_AMIGA13 || decorations == DEC_AMIGA11) {
	/* drag bars */
	if (decorations == DEC_AMIGA13) {
	    ttk_ap_fillrect (srf, ttk_ap_get ("header.accent"), hwid_left[hwid_lnext-1] + 6, 4,
			     hwid_right[hwid_rnext-1] - 6, 7);
	    ttk_ap_fillrect (srf, ttk_ap_get ("header.accent"), hwid_left[hwid_lnext-1] + 6, ttk_screen->wy - 8,
			     hwid_right[hwid_rnext-1] - 6, ttk_screen->wy - 5);
	} else {
	    int i;
	    for (i = 2; i < ttk_screen->wy; i += 2) {
		ttk_line (srf, hwid_left[hwid_lnext-1] + 2, i, hwid_right[hwid_rnext-1] - 2, i, ttk_ap_getx ("header.accent") -> color);
	    }
	}

	/* vertical widget separators */
	ttk_ap_fillrect (srf, ttk_ap_get ("header.accent"), hwid_left[hwid_lnext-1] + 2, 0,
			 hwid_left[hwid_lnext-1] + 3, ttk_screen->wy - 1);
	ttk_ap_fillrect (srf, ttk_ap_get ("header.accent"), hwid_right[hwid_rnext-1] - 2, 0,
			 hwid_right[hwid_rnext-1] - 3, ttk_screen->wy - 1);

	/* "close box" */
	if (!ipod_get_setting (DISPLAY_LOAD) && !hold_is_on) {
	    ttk_ap_fillrect (srf, ttk_ap_get ("header.accent"), 2, 2, hwid_left[hwid_lnext-1] - 2, ttk_screen->wy - 4);
	    ttk_ap_fillrect (srf, ttk_ap_get ("header.bg"), 4, 4, hwid_left[hwid_lnext-1] - 4, ttk_screen->wy - 6);
	    ttk_ap_fillrect (srf, ttk_ap_get ("header.fg"), 7, 7, hwid_left[hwid_lnext-1] - 7, ttk_screen->wy - 9);
	}

	/* clear text area */
	if (ttk_ap_getx ("header.bg") -> type & TTK_AP_COLOR) {
	    ttk_ap_fillrect (srf, ttk_ap_get ("header.bg"),
			     (ttk_screen->w - width) / 2 - 4, 0,
			     (ttk_screen->w + width) / 2 + 4, ttk_screen->wy - 2);
	}
    } else if (decorations == DEC_MROBE) {
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
    }    
}

TWidget *new_decorations_widget() 
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = ttk_screen->w;
    ret->h = ttk_screen->wy;
    ret->draw = draw_decorations;
    return ret;
}


TWidget *decwid = 0;

void header_fix_hold() 
{
    ttk_dirty |= TTK_DIRTY_HEADER;
}

void header_init() 
{
    if (decwid) ttk_remove_header_widget (decwid);
    ttk_add_header_widget ((decwid = new_decorations_widget()));

    pz_hwid_reset();

    if (ipod_get_setting (DISPLAY_LOAD))
	pz_hwid_pack_left (new_load_average_display());
    
#ifdef MPDC
    pz_hwid_pack_left (new_mpdc_status_indicator());
#endif
    pz_hwid_pack_left (new_hold_status_indicator());
    pz_hwid_pack_right (new_battery_indicator());
}

void pz_draw_header (char *str) 
{
    if (pz_last_window)
	ttk_window_set_title (pz_last_window, str);
    else
	pz_next_header = str;
}
