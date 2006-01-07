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

	lglob.lb->srf = srf;
	lglob.frames++;
	snprintf( buf, 80, "%d", lglob.frames );
	Lithp_setString( lglob.lb, "Frames", buf );

	Lithp_callDefun( lglob.lb , "OnDraw" );
}

static void cleanup_lithpwrap( void ) 
{
	burritoDelete( lglob.lb );
	lglob.lb = NULL;
}


static int event_lithpwrap( PzEvent *ev )
{
	int t;
	char buf[80];
	char *v;

	if( !lglob.lb ) printf( "no burrito!\n" );

	/* update some lithp variables... */
	lglob.timer++;
	snprintf( buf, 80, "%d", lglob.timer );
	Lithp_setString( lglob.lb, "Ticks", buf );


	switch (ev->type) {
	case PZ_EVENT_SCROLL:
		if( !lglob.paused ) {
			TTK_SCROLLMOD( ev->arg, 5 );
			if( ev->arg > 0 )
				Lithp_callDefun( lglob.lb , "OnRight" );
			else
				Lithp_callDefun( lglob.lb , "OnLeft" );
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
			pz_close_window (ev->wid->win);
			break;

		case( PZ_BUTTON_HOLD ):
			lglob.paused = 0;
			break;

		case( PZ_BUTTON_PREVIOUS ):
			Lithp_callDefun( lglob.lb , "OnPrevious" );
			break;

		case( PZ_BUTTON_NEXT ):
			Lithp_callDefun( lglob.lb , "OnNext" );
			break;

		case( PZ_BUTTON_PLAY ):
			Lithp_callDefun( lglob.lb , "OnPlay" );
			break;

		case( PZ_BUTTON_ACTION ):
			Lithp_callDefun( lglob.lb , "OnAction" );
			break;
		}
		break;

	case PZ_EVENT_DESTROY:
		cleanup_lithpwrap();
		break;

	case PZ_EVENT_TIMER:
		lglob.timer++;
		Lithp_callDefun( lglob.lb , "OnTimer" );
		break;
	}

	/* for some reason, this sometimes needs to be checked */
	if( lglob.lb ) {
		v = Lithp_getString( lglob.lb, "DirtyScreen" );
		if( v!= NULL ) {
			t = atoi( v );
			if( t > 0 ) ev->wid->dirty = 1;
			else        ev->wid->dirty = 0;
		}
		Lithp_setString( lglob.lb, "DirtyScreen", "0" );
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
	"(defun OnInit () (list (setq TimerMSec 1000)))" 
	"(defun OnTimer () (list (princ \"timer!\")(terpri)))"
};


PzWindow *new_lithpwrap_window_with_file( char * fn )
{
	int t;
	char * tbar;
	char buf[32];

	lithpwrap_init_globals();

	Lithp_parseInFile( lglob.lb, fn );
	Lithp_evaluateBurrito( lglob.lb );

	/* various things */
	Lithp_setString( lglob.lb, "DirtyScreen", "0" );
	Lithp_setString( lglob.lb, "Ticks", "0" );
	Lithp_setString( lglob.lb, "Frames", "0" );
	snprintf( buf, 32, "%d", lglob.w );
	Lithp_setString( lglob.lb, "Width", buf );
	snprintf( buf, 32, "%d", lglob.h );
	Lithp_setString( lglob.lb, "Height", buf );

	Lithp_callDefun( lglob.lb, "OnInit" );

	/* adjust the header */
	tbar = Lithp_getString( lglob.lb, "HeaderText" );
	if( !tbar || !strcmp( tbar, "-1" )) tbar = "Lithp";

	/* setup the window */
	lglob.window = pz_new_window( tbar, PZ_WINDOW_NORMAL );
	lglob.widget = pz_add_widget( lglob.window, 
				draw_lithpwrap, event_lithpwrap );

	/* setup a timer, if the user wants */
	t = atoi( Lithp_getString( lglob.lb, "TimerMSec" ));
	if( t > 0 ) pz_widget_set_timer( lglob.widget, t );


	return( pz_finish_window( lglob.window ));
}

PzWindow *new_lithpwrap_window()
{

#ifndef NEVER
	return( new_lithpwrap_window_with_file( "/sample01.lsp" ));
#else
	int t;
	lithpwrap_init_globals();

	Lithp_parseInString( lglob.lb, example );

	Lithp_evaluateBurrito( lglob.lb );

	Lithp_callDefun( lglob.lb, "OnInit" );
	Lithp_setString( lglob.lb, "DirtyScreen", "0" );

	lglob.window = pz_new_window( "Lithp", PZ_WINDOW_NORMAL );
	lglob.widget = pz_add_widget( lglob.window, 
				draw_lithpwrap, event_lithpwrap );

	/* setup a timer, if the user wants */
	t = atoi( Lithp_getString( lglob.lb, "TimerMSec" ));
	if( t > 0 ) pz_widget_set_timer( lglob.widget, t );

	return( pz_finish_window( lglob.window ));
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
	return new_lithpwrap_window_with_file( item->data );
}



/*
**  our module's init function...
*/
void init_lithpwrap() 
{
	/* internal module name */
	lglob.module = pz_register_module( "lithp", cleanup_lithpwrap );

	/* menu item display name */
	pz_menu_add_action( "Extras/Stuff/Lithp Demo", new_lithpwrap_window );
	pz_menu_add_action( "Lithp Demo", new_lithpwrap_window );

	/* now the file browser hooks */
	lithp_fbx.name = N_( "Open with Lithp" );
	lithp_fbx.makesub = lithp_open_handler;
	pz_browser_add_action( lithp_openable, &lithp_fbx );
}


PZ_MOD_INIT (init_lithpwrap)
