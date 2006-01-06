/* lithp - pzwrap
 *
 *      a wrapper to use lithp as a podzilla2 module
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
/*
	ttk_color bg = ttk_ap_get( "window.bg" )->color;
	ttk_color fg = ttk_ap_get( "window.fg" )->color;
*/

	// doDraw
}

static void cleanup_lithpwrap( void ) 
{
}


static int event_lithpwrap( PzEvent *ev )
{
	switch (ev->type) {
	case PZ_EVENT_SCROLL:
		if( !lglob.paused ) {
			TTK_SCROLLMOD( ev->arg, 5 );
/*
			if( ev->arg > 0 )
				doLeftScroll
			else
				doRightScroll
*/
			ev->wid->dirty = 1;
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

		case( PZ_BUTTON_PLAY ):
			// do play
			break;

		case( PZ_BUTTON_ACTION ):
			// do action
			break;
		}
		break;

	case PZ_EVENT_DESTROY:
		cleanup_lithpwrap();
		break;

	case PZ_EVENT_TIMER:
		// do timer
		lglob.timer++;
		ev->wid->dirty = 1;
		break;
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
	lglob.lb = NULL;
}

PzWindow *new_lithpwrap_window()
{
	lithpwrap_init_globals();

	lglob.window = pz_new_window( "Lithp", PZ_WINDOW_NORMAL );
	lglob.widget = pz_add_widget( lglob.window, 
				draw_lithpwrap, event_lithpwrap );

	//pz_widget_set_timer( lglob.widget, 150 );

	return pz_finish_window( lglob.window );
}

PzWindow *new_lithpwrap_window_with_file( char * fn )
{
	return new_lithpwrap_window();
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
	pz_menu_add_action( "Lithp Demo", new_lithpwrap_window );

	/* now the file browser hooks */
	lithp_fbx.name = N_( "Open with Lithp" );
	lithp_fbx.makesub = lithp_open_handler;
	pz_browser_add_action( lithp_openable, &lithp_fbx );
}


PZ_MOD_INIT (init_lithpwrap)
