/*
 * Clocks 2.0 - Clock engine with expandable clock faces for podzilla 2
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
#include "clocks.h"


/******************************************************************************
** builtin clock faces
*/

/* convert 24 hour time to 12 hour time */
static int clock_convert_12( int hours )
{
        if( hours == 0 )
                hours = 12;
        else if( hours > 12 )
                hours -= 12;
        return( hours );
}


int clock_convert_1224( int hours )
{
        if( pz_get_int_setting( pz_global_config, TIME_1224 ))
                return( hours );
        return( clock_convert_12( hours ));
}


static void clock_draw_vector( ttk_surface srf, clocks_globals *glob )
{
	char buf[80];
	int sX = (int)((float)glob->w/10.0);
	int sY = sX<<1;

	int sX2 = (int)((float)glob->w/20.0);
	int sY2 = sX2<<1;

	/* display the time */
	snprintf( buf, 80, "%02d:%02d:%02d",
			clock_convert_1224( glob->dispTime->tm_hour),
			glob->dispTime->tm_min,
			glob->dispTime->tm_sec );
	pz_vector_string_center( srf,
			buf, glob->w>>1, glob->h/3,
			sX, sY, 1, glob->fg );

	/* display the date */
	strftime( buf, 80, "%Y %b %d", glob->dispTime );
	pz_vector_string_center( srf,
			buf, glob->w>>1, (glob->h*3)>>2,
			sX2, sY2, 1, glob->fg );

        if( glob->editing ) {
                char * fmtt = "";
                char * fmtd = "";
                
                if( glob->editing == CLOCKS_SEL_HOURS ) {
                        fmtt = "%c%c      ";
                } else if( glob->editing == CLOCKS_SEL_MINUTES ) {
                        fmtt = "   %c%c   ";
                } else if( glob->editing == CLOCKS_SEL_SECONDS ) {
                        fmtt = "      %c%c";
                } else if( glob->editing == CLOCKS_SEL_YEARS ) {
                        fmtd = "%c%c%c%c       ";
                } else if( glob->editing == CLOCKS_SEL_MONTHS ) {
                        fmtd = "     %c%c%c   ";
                } else if( glob->editing == CLOCKS_SEL_DAYS ) {
                        fmtd = "         %c%c";
                }

		/* arrows for time */
                snprintf( buf, 80, fmtt,
                        VECTORFONT_SPECIAL_UP, VECTORFONT_SPECIAL_UP  );

		pz_vector_string_center( srf,
			buf, glob->w>>1, (glob->h/3) - sY - 2,
			sX, sY, 1, glob->fg );

                snprintf( buf, 80, fmtt,
                        VECTORFONT_SPECIAL_DOWN, VECTORFONT_SPECIAL_DOWN  );

		pz_vector_string_center( srf,
			buf, glob->w>>1, (glob->h/3) + sY + 2,
			sX, sY, 1, glob->fg );

		/* arrows for date */
                sprintf( buf, fmtd,
                        VECTORFONT_SPECIAL_UP, VECTORFONT_SPECIAL_UP,
                        VECTORFONT_SPECIAL_UP, VECTORFONT_SPECIAL_UP);

		pz_vector_string_center( srf,
			buf, glob->w>>1, ((glob->h*3)>>2) - sY2 - 2,
			sX2, sY2, 1, glob->fg );

                sprintf( buf, fmtd,
                        VECTORFONT_SPECIAL_DOWN, VECTORFONT_SPECIAL_DOWN,
                        VECTORFONT_SPECIAL_DOWN, VECTORFONT_SPECIAL_DOWN );

		pz_vector_string_center( srf,
			buf, glob->w>>1, ((glob->h*3)>>2) + sY2 + 2,
			sX2, sY2, 1, glob->fg );
	}

}



/******************************************************************************/

/* global structure defined in clocks.h with a banana. */
static clocks_globals cglob;


static int clocks_tz_offsets[] = { /* minutes associated with the strings */
           0,  
          60,  120,  180,  210,  240,
         270,  300,  330,  345,  360,
         390,  420,  480,  525,  540,
         570,  600,  630,  660,  690,
         720,  765,  780,  840, 
        -720, -660, -600, -570, -540,
        -480, -420, -360, -300, -240,
        -210, -180, -120, -60
};

static int clocks_dst_offsets[] = {
        0, 30, 60
};

static int monthlens[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
};


/* add a new face callback onto the list... */
void clocks_register_face( draw_face fcn, char * name )
{
	int c;
	for( c=0 ; c<NCLOCK_FACES ; c++ )
	{
		/* I'm in a cage.  Giant fish enraged! */
		if( cglob.faces[c].routine == NULL ) {
			/* copy the function pointer */
			cglob.faces[c].routine = fcn;

			/* copy the name over */
			strncpy( cglob.faces[c].name, name, 32 );
			cglob.faces[c].name[31] = '\0'; /* truncate */

			/* bump the count */
			cglob.nFaces++;
			return;
		}
	}
	fprintf( stderr, "SHARK ATTACK: Unable to add clock face %s\n", name );
}



#define NAME_TIMEOUT	(3) 	/* number of seconds, approx */

/* this basically just calls the current clock face routine */
void draw_clocks( PzWidget *widget, ttk_surface srf )
{
	draw_face fcn;
	time_t t;

	/* setup the time in the global */
	t = time( NULL );
	if( t==(time_t) -1 ) return; /* error */
	t += cglob.offset;
	cglob.dispTime = localtime( &t );

	/* initialize the colors */
	cglob.bg = ttk_ap_get( "window.bg" )->color;
	cglob.fg = ttk_ap_get( "window.fg" )->color;
	cglob.border = ttk_ap_get( "window.border" )->color;

	/* call the current routine */
	fcn = (draw_face) cglob.faces[cglob.cFace].routine;
	if( fcn != NULL ) {
		fcn( srf, &cglob );
	} else {
		/* this should never happen */
		fprintf( stderr, "SCHNIKIES: No clock callback for slot %d\n",
				cglob.cFace );
	}

	/* display name */
	if( cglob.timer < NAME_TIMEOUT ) {
		ttk_fillrect( srf, 0, cglob.h-11, 
			pz_vector_width( cglob.faces[cglob.cFace].name, 
					 5, 9, 1 )+2, cglob.h,
					cglob.bg );
		ttk_rect( srf, -1, cglob.h-12, 
			pz_vector_width( cglob.faces[cglob.cFace].name, 
					 5, 9, 1 )+3, cglob.h+1,
					cglob.fg );
		pz_vector_string( srf,
			cglob.faces[cglob.cFace].name, 
			1, cglob.h-10, 5, 9, 1, cglob.fg );
	}
}


void cleanup_clocks( void ) 
{
}


/*
   what makes a melonball bounce..
   a melonball bounce.
   a melonball bounce.
   what makes a melonball bounce?
	The ice tart taste of Sprite!
*/

static void clock_decrement_time( void )
{
        int m;
        time_t t;
        struct tm * current_time;
        struct timeval tv_s;

        t = time( NULL );
        if ( t == (time_t) - 1 ) return; /* error */

        current_time = localtime( &t );

/* the years and months adjustments here are incorrect.  The time should
   be converted to a  struct tm*  and then incremented by 1 year or one
   month.  For now though, we'll just add 365 days for a year.
   This probably won't work right on leap years.  February will act funny
   in those years, since it's currently hardcoded to be 28 days instead
   of 29 days.  Man, our calendar sucks.
*/

        switch( cglob.editing ) {
        case( CLOCKS_SEL_YEARS ):
                t -= 60*60*24*365;      /* wrongish */
                break;

        case( CLOCKS_SEL_MONTHS ):
                m = current_time->tm_mon-1;
                if( m < 0 ) m=11;
                t -= 60*60*24 * monthlens[ m ];
                break;

        case( CLOCKS_SEL_DAYS ):	t -= 60*60*24;	break;
        case( CLOCKS_SEL_HOURS ):	t -= 60*60;	break;
        case( CLOCKS_SEL_MINUTES ):	t -= 60;	break;
        case( CLOCKS_SEL_SECONDS ):	t -= 1;		break;
        }

	tv_s.tv_sec = t;
	tv_s.tv_usec = 0;
	settimeofday( &tv_s, NULL );
}

static void clock_increment_time( void )
{
        time_t t;
        struct tm * current_time;
        struct timeval tv_s;

        t = time( NULL );
        if ( t == (time_t) - 1 ) return; /* error */

        current_time = localtime( &t );

        switch( cglob.editing ) {
        case( CLOCKS_SEL_YEARS ):
                t += 60*60*24*365;      /* wrongish */
                break;

        case( CLOCKS_SEL_MONTHS ):
                t += 60*60*24 * monthlens[ current_time->tm_mon ];
                break;

        case( CLOCKS_SEL_DAYS ):	t += 60*60*24;	break;
        case( CLOCKS_SEL_HOURS ):	t += 60*60;	break;
        case( CLOCKS_SEL_MINUTES ):	t += 60;	break;
        case( CLOCKS_SEL_SECONDS ):	t += 1;		break;
        }

	tv_s.tv_sec = t;
	tv_s.tv_usec = 0;
	settimeofday( &tv_s, NULL );
}


/* event handler;
	this does one of two things:
	1. lets the user change the clock face / fullscreen
	2. lets the user edit the time
*/
int event_clocks( PzEvent *ev )
{
	switch (ev->type) {
	case PZ_EVENT_SCROLL:
		if( cglob.editing ) {
			TTK_SCROLLMOD( ev->arg, 5 );

			if( ev->arg > 0 ) {
				clock_increment_time();
				ev->wid->dirty = 1;
			} else {
				clock_decrement_time();
				ev->wid->dirty = 1;
			}
			/* restart the timer, since time got all wonky */
			pz_widget_set_timer( cglob.widget, 1000 );
		}
		break;

	case PZ_EVENT_BUTTON_DOWN:
		switch( ev->arg ) {
		case( PZ_BUTTON_HOLD ):
			cglob.fullscreen = 1;
			ttk_window_hide_header( cglob.window );
			cglob.w = cglob.window->w;
			cglob.h = cglob.window->h;
			ev->wid->dirty = 1;
		default:
			break;
		}
		break;

	case PZ_EVENT_BUTTON_UP:
		switch( ev->arg ) {
		case( PZ_BUTTON_MENU ):
			pz_set_priority(PZ_PRIORITY_IDLE); 
			pz_close_window (ev->wid->win);
			break;

		case( PZ_BUTTON_HOLD ):
			cglob.fullscreen = 0;
			ttk_window_show_header( cglob.window );
			cglob.w = cglob.window->w;
			cglob.h = cglob.window->h;
			ev->wid->dirty = 1;
			break;

		case( PZ_BUTTON_PLAY ):
			break;

		case( PZ_BUTTON_ACTION ):
			if( cglob.editing ) {
				/* advance to next selection */
				cglob.editing++;
				if( cglob.editing > CLOCKS_SEL_MAX )
					cglob.editing = CLOCKS_SEL_HOURS;
			} else {
				/* advance to next clock face */
				if( !cglob.fullscreen ) {
					cglob.cFace++;
					if( cglob.cFace >= cglob.nFaces )
						cglob.cFace = 0;
					cglob.timer = 0;
				}
			}
			ev->wid->dirty = 1;
			break;

		default:
			break;
		}
		break;

	case PZ_EVENT_DESTROY:
		cleanup_clocks();
		break;

	case PZ_EVENT_TIMER:
		ev->wid->dirty = 1;
		//if( pz_get_int_setting( pz_global_config, TIME_TICKER ))
		cglob.timer++;
		ttk_click();
		break;

	default:
		break;
	}
	return 0;
}


/* create the window */
static PzWindow *new_clock_window_common( char * title )
{
	/* create the window */
	cglob.window = pz_new_window( title, PZ_WINDOW_NORMAL );

	cglob.w = cglob.window->w;
	cglob.h = cglob.window->h;
	cglob.fullscreen = 0;

	cglob.cFace = 0;

	/* create the widget */
	cglob.widget = pz_add_widget( cglob.window, 
				draw_clocks, event_clocks );

	/* 1 second timeout */
	pz_widget_set_timer( cglob.widget, 1000 );

	/* we're waaaay more important than anyone else */
	pz_set_priority(PZ_PRIORITY_VITAL); 

	/* done in here! */
	return pz_finish_window( cglob.window );
}

PzWindow *new_set_clock_window()
{
	cglob.offset = 0;
	cglob.editing = CLOCKS_SEL_HOURS;
	return( new_clock_window_common( "Set Clock" ));
}

PzWindow *new_local_clock_window()
{
	cglob.offset = 0;
	cglob.editing = CLOCKS_SEL_NOEDIT;
	return( new_clock_window_common( "Clock" ));
}

PzWindow *new_world_clock_window()
{
	int locl_z_offs = clocks_tz_offsets[ pz_get_int_setting( pz_global_config, TIME_ZONE )];
	int targ_z_offs = clocks_tz_offsets[ pz_get_int_setting( pz_global_config, TIME_WORLDTZ )];

	int locl_d_offs = clocks_dst_offsets[ pz_get_int_setting( pz_global_config, TIME_DST )];
	int targ_d_offs = clocks_dst_offsets[ pz_get_int_setting( pz_global_config, TIME_WORLDDST )];

	/*  the way this works is that we look at the difference between
	    the target zone, and the local zone.  That difference is the
	    number of minutes of delta we need to deal with.
	    Once we have that, we multiply it by 60, and use it when we
	    display the time.
	    We do this for the timezone first, then the DST second.
	*/
	cglob.offset = ( targ_z_offs - locl_z_offs )*60;
	cglob.offset += ( targ_d_offs - locl_d_offs )*60;

	cglob.editing = CLOCKS_SEL_NOEDIT;

	return( new_clock_window_common( "World Clock") );
}



/******************************************************************************/

extern const char *clocks_timezones[], *clocks_dsts[];

/* xxx other, external clocks xxx */
void clock_draw_simple_analog( ttk_surface srf, clocks_globals *glob );
void clock_draw_nelson_analog( ttk_surface srf, clocks_globals *glob );
void clock_draw_oversized( ttk_surface srf, clocks_globals *glob );
void clock_draw_oversized_watch( ttk_surface srf, clocks_globals *glob );
void clock_draw_bcd_red( ttk_surface srf, clocks_globals *glob );
void clock_draw_bcd_blue( ttk_surface srf, clocks_globals *glob );
void clock_draw_alien_ap( ttk_surface srf, clocks_globals *glob );
void clock_draw_alien_rgb( ttk_surface srf, clocks_globals *glob );

void init_clocks() 
{
	int c;

	/* internal module name */
	cglob.module = pz_register_module( "clocks", cleanup_clocks );

	/* settings menu */
	pz_menu_add_action( "/Settings/Date & Time/Set Time & Date", 
					    new_set_clock_window );

	/* clock menu */
	pz_menu_add_action( "/Extras/Clock/Local Clock", new_local_clock_window );
	pz_menu_add_action( "/Extras/Clock/World Clock", new_world_clock_window );
	pz_menu_add_setting( "/Extras/Clock/World TZ", TIME_WORLDTZ, 
					    pz_global_config, clocks_timezones);
	pz_menu_add_setting("/Extras/Clock/World DST", TIME_WORLDDST, 
					    pz_global_config, clocks_dsts );


	/* initialize the face list structure */
	for( c=0 ; c<NCLOCK_FACES ; c++ ) {
		cglob.faces[c].routine = NULL;
		cglob.faces[c].name[0] = '\0';
	}
	cglob.nFaces = 0;

	cglob.timer = 0;

	/* register internal clock faces */
	clocks_register_face( clock_draw_vector, "Vector Clock" );

	/* these are in other .c files in this module */
	clocks_register_face( clock_draw_simple_analog, "Simple Analog Clock" );
	clocks_register_face( clock_draw_nelson_analog, "Nelson Ball Clock" );
	clocks_register_face( clock_draw_oversized, "Oversized" );
	clocks_register_face( clock_draw_oversized_watch, "Oversized Watch" );
	clocks_register_face( clock_draw_bcd_red, "BCD Binary Clock" );
	if( ttk_screen->bpp >= 16 ) {
	 clocks_register_face( clock_draw_bcd_blue, "BCD Binary Clock (blue)" );
	}
	clocks_register_face( clock_draw_alien_ap, "Alien Clock" );
	clocks_register_face( clock_draw_alien_rgb, "Alien Clock RGB" );
}

PZ_MOD_INIT (init_clocks)
