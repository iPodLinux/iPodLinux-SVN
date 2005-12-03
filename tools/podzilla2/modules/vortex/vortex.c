/*
 * Vortex
 *  
 * Copyright (C) 2005 Scott Lawrence
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
#include <errno.h>
#include "pz.h"
#include "console.h"

static PzModule *module = NULL;
static PzWidget *widget = NULL;


int Vortex_Rand( int max )
{
        return (int)((float)max * rand() / (RAND_MAX + 1.0));
}

void draw_vortex (PzWidget *widget, ttk_surface srf) 
{

	Vortex_Console_Render( srf, ttk_makecol( BLACK ));

	if( Vortex_Console_GetZoomCount() == 0 )
		pz_vector_string_center( srf, "[menu] to exit.",
                            (ttk_screen->w - ttk_screen->wx)>>1, 10,
                            5, 9, 1, ttk_makecol( BLACK ));

/*
	//ttk_line (srf, widget->x + 5, 10, widget->x + widget->w - 5, 10, ttk_makecol (DKGREY));
	printf( "%d %d\n", Vortex_Console_GetZoomCount(),
				Vortex_Console_GetStaticCount() );
*/
}

void cleanup_vortex() 
{
}


int event_vortex (PzEvent *ev) 
{
    switch (ev->type) {
    case PZ_EVENT_SCROLL:
	{
		TTK_SCROLLMOD( ev->arg, 4 );

		if( ev->arg > 0 ) {
			Vortex_Console_AddItem( "LEFT", 
					Vortex_Rand(4)-2, Vortex_Rand(4)-2, 
					VORTEX_STYLE_NORMAL );
		} else {
			Vortex_Console_AddItem( "RIGHT", 
					Vortex_Rand(4)-2, Vortex_Rand(4)-2, 
					VORTEX_STYLE_NORMAL );
		}
	}
	break;

    case PZ_EVENT_BUTTON_UP:
	switch( ev->arg ) {
	    case( PZ_BUTTON_MENU ):
		pz_close_window (ev->wid->win);
		break;

	    default:
		break;
	}
	break;

    case PZ_EVENT_DESTROY:
	cleanup_vortex();
	break;

    case PZ_EVENT_TIMER:
	Vortex_Console_Tick();
	ev->wid->dirty = 1;
	break;
    }
    return 0;
}

PzWindow *new_vortex_window()
{
    PzWindow *window;
    FILE *fp;
    
    //fp = fopen (pz_module_get_datapath (module, "message.txt"), "r");

    srand( time( NULL ));
    Vortex_Console_Init();

    window = pz_new_window( "Vortex", PZ_WINDOW_NORMAL );
    widget = pz_add_widget( window, draw_vortex, event_vortex );
    pz_widget_set_timer( widget, 40 );

    Vortex_Console_AddItem( "VORTEX", 0, 0, VORTEX_STYLE_NORMAL );

    return pz_finish_window( window );
}

void init_vortex() 
{
    /* internal module name */
    module = pz_register_module ("vortex", cleanup_vortex);

    /* menu item display name */
    pz_menu_add_action ("/Extras/Games/Vortex WIP", new_vortex_window);
}

PZ_MOD_INIT (init_vortex)
