/* lithp - pzwrap
 *
 *      a wrapper to use lithp as a podzilla2 module
 *      
 * 	This module should also show how to use lithp in your own modules
 *
 *      (c)2006 Scott Lawrence   yorgle@gmail.com
 */

/*
 * Wrapper to run lithp programlets in podzilla
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
#include "lithp.h"
#include "pz.h"

/*
** the globals for the module
*/
typedef struct _lithpwrap_globals {
	/* pz meta stuff */
	PzModule * module;
	PzWindow * window;
	PzWidget * widget;

	/* screen stuff */
	int w, h;

	/* various helpers */
	int paused;
	int timer;
	int frames;

	/* this is where the lisp gets parsed into */
	lithp_burrito * lb;
} lithpwrap_globals;

/* and define an instance of the above... */
static lithpwrap_globals lglob;


/*
** The event catchers.
** These all just simply call the associated functions in the Lithp world
*/
static void draw_lithpwrap( PzWidget *widget, ttk_surface srf )
{
	char buf[80];
/*
	ttk_color bg = ttk_ap_get( "window.bg" )->color;
	ttk_color fg = ttk_ap_get( "window.fg" )->color;
*/

	if( !lglob.lb ) return;
	lglob.lb->srf = srf;
	lglob.frames++;
	snprintf( buf, 80, "%d", lglob.frames );
	Lithp_setString( lglob.lb, "pz2:Frames", buf );

	Lithp_callDefunViaVariable( lglob.lb , "pz2:Draw" );
}

/* gets called on shutdown of the session.. */
static void session_end( void )
{
	pz_widget_set_timer( lglob.widget, 0 );
	Lithp_callDefunViaVariable( lglob.lb, "pz2:Shutdown" );
	burritoDelete( lglob.lb );
	lglob.lb = NULL;
}



/* this gets called on shutdown of podzilla */
static void cleanup_lithpwrap( void ) 
{
}

static int event_lithpwrap( PzEvent *ev )
{
	int t;
	char buf[80];
	char *v;

	if( !lglob.lb ) {
		printf( "no burrito!\n" );
		return( 0 );
	}

	/* update some lithp variables... */
	snprintf( buf, 80, "%d", lglob.timer );
	Lithp_setString( lglob.lb, "pz2:Ticks", buf );


	switch (ev->type) {
	case PZ_EVENT_SCROLL:
		if( !lglob.paused ) {
			TTK_SCROLLMOD( ev->arg, 5 );
			if( ev->arg > 0 )
				Lithp_callDefunViaVariable( lglob.lb , "pz2:Right" );
			else
				Lithp_callDefunViaVariable( lglob.lb , "pz2:Left" );
		}
		break;

	case PZ_EVENT_BUTTON_DOWN:
		switch( ev->arg ) {
		case( PZ_BUTTON_HOLD ):
			lglob.paused = 1;
			break;
		}
		break;

	case PZ_EVENT_BUTTON_UP:
		switch( ev->arg ) {
		case( PZ_BUTTON_MENU ):
			session_end();
			pz_close_window (ev->wid->win);
			break;

		case( PZ_BUTTON_HOLD ):
			lglob.paused = 0;
			break;

		case( PZ_BUTTON_PREVIOUS ):
			Lithp_callDefunViaVariable( lglob.lb , "pz2:Previous" );
			break;

		case( PZ_BUTTON_NEXT ):
			Lithp_callDefunViaVariable( lglob.lb , "pz2:Next" );
			break;

		case( PZ_BUTTON_PLAY ):
			Lithp_callDefunViaVariable( lglob.lb , "pz2:Play" );
			break;

		case( PZ_BUTTON_ACTION ):
			Lithp_callDefunViaVariable( lglob.lb , "pz2:Action" );
			break;
		}
		break;

	case PZ_EVENT_DESTROY:
		cleanup_lithpwrap();
		break;

	case PZ_EVENT_TIMER:
		lglob.timer++;
		Lithp_callDefunViaVariable( lglob.lb , "pz2:Timer" );
		break;
	}

	/* for some reason, this sometimes needs to be checked */
	if( lglob.lb ) {
		v = Lithp_getString( lglob.lb, "pz2:DirtyScreen" );
		if( v!= NULL ) {
			t = atoi( v );
			if( t > 0 ) ev->wid->dirty = 1;
			else        ev->wid->dirty = 0;
		}
		Lithp_setString( lglob.lb, "pz2:DirtyScreen", "0" );
	}
	return 0;
}


/*
** initialization and window/parse tree setup routines
*/
static void lithpwrap_init_globals( void )
{
	lglob.module = NULL;
	lglob.window = NULL;
	lglob.widget = NULL;
	lglob.w = ttk_screen->w - ttk_screen->wx;
	lglob.h = ttk_screen->h - ttk_screen->wy;
	lglob.paused = 0;
	lglob.timer = 0;
	lglob.frames = 0;
	lglob.lb = burritoNew();
	lglob.lb->isMono = ( ttk_screen->bpp < 16 )?1:0;
}


/* eventually, this example will display some text to the screen and such */
static char example[] = {
"(require 'pz2:0.8)"
/* hooks for our functions */
"(setq pz2:Startup   myStartup)"
"(setq pz2:PreFlight myPreFlight)"
"(setq pz2:Timer     myTimer)"
"(setq pz2:Draw      myDraw)"

"(setq textcolor    '(255 0 100 BLACK))"

/* and now our functions... */
"(defun myStartup () (list"
"	(princ 'Startup')(terpri)"
"	(setq pz2:Persistent 1)"
"	(setq pz2:HeaderText 'Example')"
"	(setq pz2:TimerMSec 200)"
"	(princ 'end')(terpri)"
"))"

"(defun myPreFlight () (list"
"	(princ 'PreFlight')(terpri)"
"))"

"(defun myTimer ()"
"	(setq pz2:DirtyScreen 1)"
")"

"(defun myDraw () (list"
"	(DrawPen (Rand 255) (Rand 255) (Rand 255) (RandomOf WHITE DKGREY GREY) )"
"	(DrawFillRect (Rand pz2:Width) (Rand pz2:Height)"
"                     (Rand pz2:Width) (Rand pz2:Height))"
"	(DrawFillRect (Rand pz2:Width) (Rand pz2:Height)"
"                     (Rand pz2:Width) (Rand pz2:Height))"
"	(DrawPen textcolor)"
"	(DrawVectorTextCentered (/ pz2:Width 2) 10 5 9 'MENU_TO_RETURN' )"
"))"
};


PzWindow *new_lithpwrap_window_with_file_or_string( char * fn, int isFile )
{
	int t;
	char * tbar;
	char * fs;
	char * pers;
	char buf[32];

	lithpwrap_init_globals();

	if( isFile )
		Lithp_parseInFile( lglob.lb, fn );
	else 
		Lithp_parseInString( lglob.lb, fn );
	Lithp_evaluateBurrito( lglob.lb );

	/* call the init routine */
	Lithp_callDefunViaVariable( lglob.lb, "pz2:Startup" );

	/* various things */
	Lithp_setString( lglob.lb, "pz2:DirtyScreen", "0" );
	Lithp_setString( lglob.lb, "pz2:Ticks", "0" );
	Lithp_setString( lglob.lb, "pz2:Frames", "0" );
	snprintf( buf, 32, "%d", lglob.w );
	Lithp_setString( lglob.lb, "pz2:Width", buf );
	snprintf( buf, 32, "%d", lglob.h );
	Lithp_setString( lglob.lb, "pz2:Height", buf );

	/* adjust the header */
	tbar = Lithp_getString( lglob.lb, "pz2:HeaderText" );
	if( !tbar || !strcmp( tbar, "-1" )) tbar = "Lithp";

	/* setup the window */
	lglob.window = pz_new_window( tbar, PZ_WINDOW_NORMAL );

	/* adjust for fullscreen? */
	fs = Lithp_getString( lglob.lb, "pz2:FullScreen" );
	if( !strcmp( fs, "1" )){
		ttk_window_hide_header( lglob.window );
		lglob.window->x = 0;
		lglob.window->y = 0;
		lglob.window->w = ttk_screen->w;
		lglob.window->h = ttk_screen->h;
		lglob.w = ttk_screen->w;
		lglob.h = ttk_screen->h;
	}

	lglob.widget = pz_add_widget( lglob.window, 
				draw_lithpwrap, event_lithpwrap );

	/* adjust for persistence */
	pers = Lithp_getString( lglob.lb, "pz2:Persistent" );
	if( !strcmp( pers, "1"))  lglob.widget->w = lglob.widget->h = 0;

	/* setup a timer, if the user wants */
	t = atoi( Lithp_getString( lglob.lb, "pz2:TimerMSec" ));
	if( t > 0 ) pz_widget_set_timer( lglob.widget, t );

	/* call the init routine */
	Lithp_callDefunViaVariable( lglob.lb, "pz2:PreFlight" );

	return( pz_finish_window( lglob.window ));
}

PzWindow *new_lithpwrap_window()
{
	return new_lithpwrap_window_with_file_or_string( 
#ifdef NEVER
			"/sample01.lsp", 1 );
#else
			example, 0);
#endif
}


/*
**  some stuff necessary to hook into the file browser
**   so that we can launch .lsp files from there 
*/

static ttk_menu_item lithp_fbx;

int lithp_openable( const char * filename )
{
	int len;
	int invalid;
	if( !filename )  return 0;

	len = strlen( filename );
	if( len < 4 ) return 0;

	invalid = strcmp( ".lsp", filename+len-4 );

	return( !invalid );
}

PzWindow * lithp_open_handler( ttk_menu_item * item )
{
	return new_lithpwrap_window_with_file_or_string( item->data, 1 );
}


/*
**  our module's init function...
*/
void init_lithpwrap() 
{
	/* internal module name */
	lglob.module = pz_register_module( "lithp", cleanup_lithpwrap );

	/* menu item display name */
	pz_menu_add_action_group( "/Extras/Demos/Lithp Demo", "Tech",  new_lithpwrap_window );

	/* now the file browser hooks */
	lithp_fbx.name = N_( "Open with Lithp" );
	lithp_fbx.makesub = lithp_open_handler;
	pz_browser_add_action( lithp_openable, &lithp_fbx );
}


PZ_MOD_INIT (init_lithpwrap)
