/*
 * Resistors - a restor color code lookup doojobby
 *  
 * Copyright (C) 2006 Scott Lawrence
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include "pz.h"

/* the globals */
typedef struct _resistors_globals {
	/* pz meta stuff */
	PzModule * module;
	PzWindow * window;
	PzWidget * widget;

	int w, h;

	int bandNo;
	int bandData[4];

	int paused;
	int timer;
} resistors_globals;

static resistors_globals rglob;

/*
static char *names[13] = {
	"Black", "Brown", "Red", "Orange", "Yellow", "Green",
	"Blue", "Violet", "Gray", "White", "Gold", "Silver", "None"
};
*/

static char *shortNames[13] = {
	"Blk", "Brn", "Red", "Org", "Yel", "Grn",
	"Blu", "Vio", "Gry", "Wht", "Gld", "Slv", "---"
};

static char *tolerances[13] = {
	"", "1", "2", "", "", "0.5",
	"0.25", "0.1", "0.05", "", "5", "10", "20"
};

static int mults[12]   = { 1, 10, 100,  1, 10, 100,  1, 10, 100,  1, 10, 100 };
static char *units[12] = {	"", "", "",	"K", "K", "K",
				"M", "M", "M", 	"G", "G", "G" };

static int ranges[4][2] = {
	{ 0, 9 }, { 0, 9 }, { 0, 9 }, { 10, 12 }
};

static ttk_color cursor[4];
static ttk_color rcolors[13];


/* wrap around values that are out of range */
static void resistors_rangeAdjust( int i )
{
	if( rglob.bandData[i] > ranges[i][1] )
		rglob.bandData[i] = ranges[i][0];

	if( rglob.bandData[i] < ranges[i][0] )
		rglob.bandData[i] = ranges[i][1];
}


void draw_resistors( PzWidget *widget, ttk_surface srf )
{
	int i, v;
	int yo = (ttk_screen->bpp == 2)? 11 : 12;
	int vc = 20;
	int w4 = rglob.w>>2;
	float bo = ((float)(rglob.w>>1)/5.0);
	int bo2 = (int) bo/2.0;
	int bw = (int) (bo*3)/4.0;
	ttk_color lttan = ttk_makecol( 205, 205, 155 );
	ttk_color dktan = ttk_makecol( 150, 150, 100 );
	ttk_color bg = ttk_ap_get( "window.bg" )->color;
	ttk_color fg = ttk_ap_get( "window.fg" )->color;
	char buf[80];

	/* leads */
	if( ttk_screen->bpp == 2 )
		ttk_rect( srf, -1, vc-2, rglob.w+1, vc+2, fg );
	else
		ttk_vgradient( srf, 0, vc-2, rglob.w, vc+2, 
			ttk_makecol( GREY ), ttk_makecol( DKGREY ) );

	/* body */
	if( ttk_screen->bpp == 2 ) {
		ttk_rect( srf, w4, vc-12, w4*3, vc+12, fg );
		ttk_rect( srf, w4-20, vc-15, w4, vc+15, fg );
		ttk_rect( srf, w4*3, vc-15, w4*3+20, vc+15, fg );
		ttk_fillrect( srf, w4-19, vc-11, w4*3+19, vc+11, bg );
	} else {
		ttk_vgradient( srf, w4, vc-12, w4*3, vc+12, lttan, dktan );
		ttk_vgradient( srf, w4-20, vc-15, w4, vc+15, lttan, dktan );
		ttk_vgradient( srf, w4*3, vc-15, w4*3+20, vc+15, lttan, dktan );
	}

	for( i=0 ; i<4 ; i++ ){
		if( i<3 || (i==3 && rglob.bandData[i]<12) )
			ttk_fillrect( srf, 3+w4+bo2+(int)(bo*i), vc-yo, 
				   3+w4+bo2+(int)(bo*i)+bw, vc+yo,
				   rcolors[ rglob.bandData[i] ] );

		if( (i==rglob.bandNo) && !rglob.paused )
			ttk_rect( srf, 3+w4+bo2+(int)(bo*i), vc-12, 
				3+w4+bo2+(int)(bo*i)+bw, vc+12,
				cursor[ rglob.timer&3 ] );
	}

	/* just so we know what the hell we're doing on a 2bpp screen */
	snprintf( buf, 80, "%s %s %s %s",
	    shortNames[ rglob.bandData[0] ], shortNames[ rglob.bandData[1] ],
	    shortNames[ rglob.bandData[2] ], shortNames[ rglob.bandData[3] ] );

	pz_vector_string_center( srf, buf, rglob.w>>1, vc+30, 5, 9, 1, fg );

	/* additional cursor on that string */
	if( (rglob.timer & 2) && !rglob.paused ) {
		snprintf( buf, 80, "%3s %3s %3s %3s",
				(rglob.bandNo == 0) ? "___" : "   ",
				(rglob.bandNo == 1) ? "___" : "   ",
				(rglob.bandNo == 2) ? "___" : "   ",
				(rglob.bandNo == 3) ? "___" : "   " );
		pz_vector_string_center( srf, buf, rglob.w>>1, vc+33, 
				5, 9, 1, ttk_ap_get( "window.fg" )->color );
	}

	/* print out the translation */
	v = (rglob.bandData[0] * 10) + rglob.bandData[1];
	v *= mults[ rglob.bandData[2] ];

	snprintf( buf, 80, "%d%s ohm  %s%%", v,
			units[ rglob.bandData[2] ],
			tolerances[ rglob.bandData[3] ] );

	pz_vector_string_center( srf, buf, rglob.w>>1, vc+55, 
				5, 9, 1, ttk_ap_get( "window.fg" )->color );
}

void cleanup_resistors( void ) 
{
}


int event_resistors( PzEvent *ev )
{
	switch (ev->type) {
	case PZ_EVENT_SCROLL:
		if( !rglob.paused ) {
			TTK_SCROLLMOD( ev->arg, 5 );
			if( ev->arg > 0 )
				rglob.bandNo++;
			else
				rglob.bandNo--;
			if( rglob.bandNo > 3 ) rglob.bandNo = 0;
			if( rglob.bandNo < 0 ) rglob.bandNo = 3;
			rglob.timer = 0;
			ev->wid->dirty = 1;
		}
		break;

	case PZ_EVENT_BUTTON_DOWN:
		switch( ev->arg ) {
		case( PZ_BUTTON_HOLD ):
			rglob.paused = 1;
			break;
		}
		break;

	case PZ_EVENT_BUTTON_UP:
		switch( ev->arg ) {
		case( PZ_BUTTON_MENU ):
			pz_close_window (ev->wid->win);
			break;

		case( PZ_BUTTON_HOLD ):
			rglob.paused = 0;
			break;

		case( PZ_BUTTON_PLAY ):
			break;

		case( PZ_BUTTON_ACTION ):
			if( !rglob.paused ) {
				rglob.bandData[ rglob.bandNo ]++;
				resistors_rangeAdjust( rglob.bandNo );
				rglob.timer = 0;
				ev->wid->dirty = 1;
			}
			break;
		}
		break;

	case PZ_EVENT_DESTROY:
		cleanup_resistors();
		break;

	case PZ_EVENT_TIMER:
		rglob.timer++;
		ev->wid->dirty = 1;
		break;
	}
	return 0;
}


PzWindow *new_resistors_window()
{
	int x;
	rglob.window = pz_new_window( "Resistors", PZ_WINDOW_NORMAL );

	/* make it full screen */
	rglob.w = ttk_screen->w - ttk_screen->wx;
	rglob.h = ttk_screen->h - ttk_screen->wy;

	rglob.widget = pz_add_widget( rglob.window, 
				draw_resistors, event_resistors );

	pz_widget_set_timer( rglob.widget, 150 );

	rglob.bandNo = 0;
	rglob.paused = 0;
	rglob.timer = 0;

	for( x=0 ; x<4 ; x++ ) {
		rglob.bandData[x] = 10;
		resistors_rangeAdjust( x );
	}

	return pz_finish_window( rglob.window );
}


void init_resistors() 
{
	/* internal module name */
	rglob.module = pz_register_module( "resistors", cleanup_resistors );

	/* menu item display name */
	pz_menu_add_action( "/Extras/Stuff/Resistor Codes",
				new_resistors_window );


	cursor[0] = ttk_makecol( WHITE );
	cursor[1] = ttk_makecol( BLACK );
	if( ttk_screen->bpp == 2 ) {
		cursor[2] = cursor[0];
		cursor[3] = cursor[1];
	} else {
		cursor[2] = ttk_makecol( 255, 255, 0 );
		cursor[3] = ttk_makecol( 0, 255, 255 );
	}

	rcolors[0]  = ttk_makecol(   0,   0,   0 );	/* black */
	rcolors[1]  = ttk_makecol(  79,  52,  51 );	/* brown */
	rcolors[2]  = ttk_makecol( 255,   0,   0 );	/* red */
	rcolors[3]  = ttk_makecol( 238, 104,  15 );	/* orange */
	rcolors[4]  = ttk_makecol( 255, 255,  40 );	/* yellow */
	rcolors[5]  = ttk_makecol(  80, 255,  80 );	/* green */
	rcolors[6]  = ttk_makecol(  40,  40, 255 );	/* blue */
	rcolors[7]  = ttk_makecol( 200,  40, 255 );	/* violet */
	rcolors[8]  = ttk_makecol( 148, 148, 148 );	/* gray */
	rcolors[9]  = ttk_makecol( 255, 255, 255 );	/* white */
	rcolors[10] = ttk_makecol( 198, 152,  58 );	/* gold */
	rcolors[11] = ttk_makecol( 204, 204, 204 );	/* silver */
	rcolors[12] = ttk_makecol(   0,   0,   0 );	/* none */
}

PZ_MOD_INIT (init_resistors)
