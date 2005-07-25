/*
 * dialog - modal dialog box for podzilla
 * Copyright (C) 19105 Scott Lawrence
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

/* NOTE:

    For now, this opens a dialog box almost the size of the full
    screen, and pretty much overall just looks horrible.  This will
    be cleaned up later, once the mechanism has been worked out,
    and it is determined that this is really something worthwhile
    for the project.

							    -scott
 */

#include <strings.h>	/* for strcasecmp */
#include <string.h>	/* for strncpy */
#include <stdio.h>	/* for printf */

#include "ipod.h"
#include "pz.h"

static GR_WINDOW_ID	dialog_wid = 0;
static GR_WINDOW_ID	dialog_bufwid = 0;
static GR_GC_ID		dialog_gc = 0;
static GR_TIMER_ID	dialog_tid = 0;

static int		dialog_width = 0;
static int		dialog_height = 0;
static int 		dialog_quit = 0;
static int		dialog_timed_out = 0;
static int		dialog_is_error = 0;

static char *		dialog_title = NULL;
static char *		dialog_text = NULL;
static char * 		dialog_b[3];
static int		dialog_timeout = 0;
static int		dialog_nbuttons = 0;
static int		dialog_current = 0;

/* 

Functions needed:
    - convert the passed in text to a block of text to be output (word wrap)
    - take the above text block, determine width and height

** for now, text isn't word wrapped or anything spiffy like that **

*/



/* draw a button. 
   the value returned is the xpos passed in, minus the size of the button
*/
static int dialog_draw_button( char * text, int xpos, int selected )
{
	int width, height, base, ypos;

	GrGetGCTextSize( dialog_gc, text, -1, GR_TFASCII,
			 &width, &height, &base );
	ypos = dialog_height - height - 15;


	/* draw the bounding box, filled */
	GrSetGCForeground( dialog_gc, appearance_get_color( CS_MESSAGEBG ));
	GrFillRect( dialog_bufwid, dialog_gc,
			xpos-width-10, ypos,
			width+10, height+10 );
	GrSetGCForeground( dialog_gc, appearance_get_color( CS_MESSAGELINE ));
	GrRect( dialog_bufwid, dialog_gc,
			xpos-width-10, ypos,
			width+10, height+10 );

	/* if selected, draw an extra inner box */
	if( selected )
	{
		GrSetGCForeground( dialog_gc, 
			appearance_get_color( CS_MESSAGEFG ));
		GrRect( dialog_bufwid, dialog_gc,
			xpos-width-9, ypos+1,
			width+8, height+8 );
	}

	/* button text */
	GrSetGCForeground( dialog_gc, appearance_get_color( CS_MESSAGEFG ));
	GrSetGCBackground( dialog_gc, appearance_get_color( CS_MESSAGEBG ));
	GrText( dialog_bufwid, dialog_gc, xpos-width-5, ypos+base+5,
			text, -1, GR_TFASCII );

	return( xpos - width - 10 );
}


/* redraw the dialog */
static void dialog_redraw( void )
{
	GR_COLOR hl;
	int c;
	int xpos;

	if( dialog_is_error ) 
		hl = appearance_get_color( CS_ERRORBG );
	else
		hl = appearance_get_color( CS_MESSAGEBG );

	/* clear the interociter */
	GrSetGCForeground( dialog_gc, hl );
	GrFillRect( dialog_bufwid, dialog_gc, 0, 0,
			dialog_width, dialog_height );
	

	/* draw the title bar area */
	GrSetGCForeground( dialog_gc, appearance_get_color( CS_TITLEBG ));
	GrFillRect( dialog_bufwid, dialog_gc, 0, 0,
			dialog_width, HEADER_TOPLINE );
	/* outlines */
	GrSetGCForeground( dialog_gc, appearance_get_color( CS_MESSAGELINE ));
	GrRect( dialog_bufwid, dialog_gc, 0, 0, dialog_width, HEADER_TOPLINE );
	GrRect( dialog_bufwid, dialog_gc, 0, 0, dialog_width, dialog_height );
	GrRect( dialog_bufwid, dialog_gc, 1, 1, dialog_width-2, dialog_height-2 );

	/* title text */
	GrSetGCBackground( dialog_gc, appearance_get_color( CS_TITLEBG ));
	GrSetGCForeground( dialog_gc, appearance_get_color( CS_TITLEFG ));
	GrText( dialog_bufwid, dialog_gc, 5, HEADER_BASELINE + 1,
			dialog_title, -1, GR_TFASCII );

	/* accent lines */
	GrSetGCForeground( dialog_gc, hl );
	GrRect( dialog_bufwid, dialog_gc, 2, 2, dialog_width-4, dialog_height-4 );
	GrRect( dialog_bufwid, dialog_gc, 3, 3, dialog_width-6, dialog_height-6 );

	/* body text */
	GrSetGCBackground( dialog_gc, hl );
	GrSetGCForeground( dialog_gc, appearance_get_color( CS_MESSAGEFG ));
	GrText( dialog_bufwid, dialog_gc, 10, dialog_height/2,
			dialog_text, -1, GR_TFASCII );

	xpos = dialog_width-5;
	/* draw the buttons */
	for( c=2 ; c>=0 ; c-- )
	{
		if( dialog_b[c] ) {
			xpos = dialog_draw_button( dialog_b[c], xpos, 
						    (c==dialog_current)?1:0 );
			xpos -= 5; /* button kerning */
		}
	}

	/* blit it on over to clean the penguin */
	GrCopyArea( dialog_wid, dialog_gc, 0, 0,
			dialog_width, dialog_height,
			dialog_bufwid, 0, 0, MWROP_SRCCOPY );
}

/* draw handler */
static void dialog_do_draw( void )
{
	dialog_redraw();
}


/* event handler... */
static int dialog_handle_event( GR_EVENT * event )
{
	switch( event->type )
	{
	    case( GR_EVENT_TYPE_TIMER ):
		dialog_quit = 1;
		dialog_timed_out = 1;
		break;

	    case( GR_EVENT_TYPE_KEY_DOWN ): 
		switch( event->keystroke.ch )
		{
		    case( IPOD_WHEEL_CLOCKWISE ): /* wheel cw */
		    case( IPOD_REMOTE_FORWARD ): /* forward on the remote */
			dialog_current++;
			if( dialog_current > (dialog_nbuttons-1) )
				dialog_current = 0;
			dialog_redraw();
			break;

		    case( IPOD_WHEEL_COUNTERCLOCKWISE ): /* wheel ccw */
		    case( IPOD_REMOTE_REWIND ): /* rewind on the remote */
			dialog_current--;
			if( dialog_current < 0 )
				dialog_current = dialog_nbuttons-1;
			dialog_redraw();
			break;

		    case( IPOD_BUTTON_ACTION ): /* wheel button */
			dialog_quit = 1;
			break;

		    case( IPOD_BUTTON_MENU ): /* menu button */
			//dialog_quit = 1;
			break;

		    case( IPOD_REMOTE_VOL_UP ):	/* volume up on the remote */
		    case( IPOD_REMOTE_VOL_DOWN ):/* volume down on the remote */
		    case( IPOD_BUTTON_PLAY ): /* play/pause */
		    case( IPOD_REMOTE_PLAY ): /* play/pause on the remote */
		    case( IPOD_BUTTON_REWIND ): /* rewind */
		    case( IPOD_BUTTON_FORWARD ): /* forward */
			break;
		}
		break;

	    default:
		break;
	}
	return( 1 );
}


/* only allow our events through... */
static void dialog_do_loop( void )
{
	int pollit = 1;
	GR_EVENT event;

	while( !dialog_quit )
	{
		pollit = 1;
		if( GrPeekEvent( &event ) )
		{
			GrGetNextEventTimeout( &event, 60 );
			if( event.type != GR_EVENT_TYPE_TIMEOUT )
			{
				pz_event_handler( &event );
				pollit = 0;
			}
		}

		//if( pollit && !dialog_quit ) dialog_redraw( );
	}
	
	/* shut them down..  SHUT THEM ALL DOWN!!! */
	if( dialog_timeout > 0 ) GrDestroyTimer( dialog_tid );
	GrDestroyGC( dialog_gc );
	pz_close_window( dialog_wid );
	GrDestroyWindow( dialog_bufwid );
}


/* this is where all of the launchers hook into... */
int dialog_create( char * title, char * text,
		char * button0, char * button1, char * button2,
		int timeout, int is_error )
{
	int c;

	/* store these aside */
	dialog_title	= title;
	dialog_text	= text;
	dialog_b[0]	= button0;
	dialog_b[1]	= button1;
	dialog_b[2] 	= button2;
	dialog_timeout	= timeout;
	dialog_is_error = is_error;

	/* start these out sane... */
	dialog_nbuttons = 0;
	dialog_current  = 0;
	dialog_quit	= 0;

	/* count up the number of buttons */
	for( c=0 ; (c<3) && (dialog_b[c] != NULL) ; c++ )
		dialog_nbuttons++;

	/* setup the nanox stuff */
	dialog_gc = GrNewGC();
	GrSetGCUseBackground( dialog_gc, GR_TRUE );

	dialog_width = screen_info.cols - 20;
	dialog_height = screen_info.rows - 20;

	dialog_bufwid = GrNewPixmap( dialog_width, dialog_height, NULL );
	dialog_wid = pz_new_window(      10,
					 10,
					 dialog_width,
					 dialog_height,
					 dialog_do_draw,
					 dialog_handle_event );
	GrSelectEvents( dialog_wid,
			     GR_EVENT_MASK_KEY_DOWN
			   | GR_EVENT_MASK_KEY_UP
			   | GR_EVENT_MASK_TIMER
			   | GR_EVENT_MASK_EXPOSURE );

	GrMapWindow( dialog_wid );

	/* start it up and run it! */

	/* set up the timer if applicable */
	if( dialog_timeout > 0 )
		dialog_tid = GrCreateTimer( dialog_wid, dialog_timeout*1000 );

	dialog_do_loop();

	if( dialog_timed_out ) return( -1 );

	return( dialog_current );
}


/* test function thingy */
void new_dialog_window(void)
{   
    int c;
    c = DIALOG_MESSAGE3( "message:", "I like cheese", "Ok", "Good", "Sure", 0 );
    printf( ">> RETURNED %d\n", c );

    c = DIALOG_ERROR2( "ERROR!", "You ran this test!", "No", "Bah", 0 );
    printf( ">> RETURNED %d\n", c );

    c = DIALOG_MESSAGE( "Timeout Test", "2 seconds.", "Great!", 2 );
    printf( ">> RETURNED %d\n", c );
}
