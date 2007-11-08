/* MandelPod 2

Based conceptually on the original:

 * Copyright (C) 2005 Martin Kaltenbrunner
 * <mkalten@iua.upf.es>
 * due to the present lack of sound support for my 4G iPd
 * just a first little program for trying general issues
 * ... for testing & educational purposes only
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

(Originally written for podzilla0, then colorized by Scott Lawrence)

This has been re-ported from the original integer Mandelbrot explorer code,
available here:
	http://www.geocities.com/CapeCanaveral/5003/fracprog.htm
rather than the legacy version by Martin.  Sorry, but it was easier to 
re-write it as a pz2/ttk module from scratch this time
*/

/* Revision history
**
** 2.00	Scott Lawrence	Initial version, no eploration, julia set nonfunctional
*/

#include "pz.h"


/* ********************************************************************** */

/* all of our state and window variables */
typedef struct mandglobs {
	PzWindow *window;
	PzModule *module;
	PzWidget *widget;
	ttk_surface workBuffer;

	/* have we started things up */
	int initted;
	int mandelbrot;	/* USE_x */

	/* rendering flags */
	int completed;		/* have we completed rendering? */
	int started;		/* have we started rendering? */
	int paletteNo;		/* which palette number are we using */
	int block;		/* which vertical stripe block are we rendering? */

	/* for the various GUI things */
	int displayCursor;	/* should the cursor be displayed? */
	int cursorX, cursorY;	/* X/Y position: [-8, 0, 8] */
	int swipe_xp;		/* current X position for the stripe marker to be drawn */

	/* window for the mandelbrot */
	double mnd_xmin, mnd_xmax, mnd_ymin, mnd_ymax;	/* region of mand set to show */

	/* window for the julia */
	double jul_xmin, jul_xmax, jul_ymin, jul_ymax; 	/* region of julia set to show */
	double jxc, jyc;				/* where in the mand set to center */
} mandglobs;

static mandglobs globs;		/* our globals */


/* these are for functions that can render Mand or Jul */
#define USE_MANDELBROT 	(1)
#define USE_JULIA 	(2)

/* for the zoom in/out functions */
#define ZOOM_CENTER	(0)
#define ZOOM_IN		(1)
#define ZOOM_OUT	(2)


/* Arfour is the Windows ME of the Astromech droids - Mike Nelson */
/* Form a nerdier sentence... no, don't try... it's impossible - Kevin Murphy */
/* (RiffTrax for Star Wars Episode 2) */


/* some helpers for the rendering routine */
#define FIXSIZE 25
#define mul(a,b) ((((long long)a)*(b))>>FIXSIZE)
#define fixpt(a) ((long)(((a)*(1<<FIXSIZE))))
#define integer(a) (((a)+(1<<(FIXSIZE-1)))>>FIXSIZE)

#define STEPS 		(64)	/* color step levels - more = slower, 64=optimal */
#define ZOOMBLOCKS 	(8)	/* number of blocks the screen is broken into */
#define STRIPES		(32)	/* number of vertical stripes the screen is rendered in */
#define XHAIR_SZ 	(5)	/* size of the center of the crosshair */

/* force a full redraw of the view */
static void force_redraw( void )
{
	globs.displayCursor = 0;
	globs.completed = 0;
	globs.started = 0;
	globs.block = 0;
	globs.swipe_xp = 0;
}

/* recenter the cursor at home */
static void home_cursor( void )
{
	globs.cursorX = 0;
	globs.cursorY = 0;
}

/* restore mandelbrot zoom parameters */
static void reset_mandelbrot_zoom( void )
{
	globs.mnd_xmin = -2.5;
	globs.mnd_xmax = 1.5;
	globs.mnd_ymin = -1.5;
	globs.mnd_ymax = 1.5;
}

/* restore julia zoom parameters */
static void reset_julia_zoom( void )
{
	globs.jul_xmin = -1.5;
	globs.jul_xmax = 1.5;
	globs.jul_ymin = -1.5;
	globs.jul_ymax = 1.5;
}

/* initialize the global variables for start-of-session */
static void initialize_globals( void )
{
	globs.workBuffer = ttk_new_surface( ttk_screen->w, ttk_screen->h, ttk_screen->bpp );
	ttk_fillrect( globs.workBuffer, 0, 0, 
			globs.workBuffer->w, globs.workBuffer->h, 
			ttk_makecol( 0, 0, 255 ));
	globs.paletteNo = 0;
	home_cursor();
	force_redraw();
	globs.initted = 1;

	reset_mandelbrot_zoom();
	reset_julia_zoom();
}


/* ********************************************************************** */

/* use the ttk gradient code to do our gradients for us */
static ttk_color determine_color_2( int i, ttk_color c1, ttk_color c2 )
{
	gradient_node * gn = ttk_gradient_find_or_add( c1, c2 );
	if( i> STEPS ) i=STEPS; 
	if( i<0 ) i=0; 
	return( gn->gradient[ (i * 255) / STEPS ] );
}

static ttk_color determine_color_3( int i, ttk_color c1, ttk_color c2, ttk_color c3 )
{
	gradient_node * gn = NULL;

	if( i> STEPS ) i=STEPS;
	if( i<0 ) i=0;

	if( i<=STEPS/2 ) {
		i *= 2;
		gn = ttk_gradient_find_or_add( c1, c2 );
		return( gn->gradient[ (i * 255) / STEPS ] );
	} else {
		i -= (STEPS/2);
		i *= 2;
		gn = ttk_gradient_find_or_add( c2, c3 );
		return( gn->gradient[ (i * 255) / STEPS ] );
	}
	return( 0 );
}

#define CLIMIT( a ) 	( ((a)>255)?255 : ((a)<0)?0 :a )
//#define CLIMIT( a ) (a)

static ttk_color determine_color_4( int i, ttk_color c1, ttk_color c2, ttk_color c3, ttk_color c4 )
{
	gradient_node * gn = NULL;

	if( i> STEPS ) i=STEPS;
	if( i<0 ) i=0;

	if( i<(STEPS/3) ) {
		i = i * 3 * 255 / STEPS;
		gn = ttk_gradient_find_or_add( c1, c2 );
		return( gn->gradient[ CLIMIT( i ) ] );

	} else if( i<=(STEPS*2/3) ) {
		i -= (STEPS/3);
		i = i * 3 * 255 / STEPS;
		gn = ttk_gradient_find_or_add( c2, c3 );
		return( gn->gradient[ CLIMIT( i )] );

	} else {
		i -= (STEPS*2/3);
		i = i * 3 * 255 / STEPS;
		gn = ttk_gradient_find_or_add( c3, c4 );
		return( gn->gradient[ CLIMIT( i ) ] );
	}
	return( c4 );
}

/* the lookups from value [0..STEPS] to ttk_color */
static ttk_color lookup_color( int i )
{
	ttk_color c = 0;
	int r=0, g=0, b=0;

	switch( globs.paletteNo ) {
	case( 0 ):
		// black to white
		c = determine_color_2( i, ttk_makecol( BLACK ), 
					  ttk_makecol( WHITE ));
		break;

	case( 1 ):
		// white to black
		c = determine_color_2( i, ttk_makecol( WHITE ), 
					  ttk_makecol( BLACK ));
		break;

	case( 2 ):
		// red to cyan
		c = determine_color_2( i, ttk_makecol( 255, 0, 0 ), 
					  ttk_makecol( 0, 255, 255 ));
		break;

	case( 3 ):
		// purpley
		c = determine_color_2( i, ttk_makecol( 128, 0, 255 ), 
				 	  ttk_makecol( 255, 0, 0 ));
		break;

	case( 4 ):
		// zebra
		if( i>= STEPS ) i=STEPS;
		r = g = b = (i&0x08)?255:0;
		c = ttk_makecol( r, g, b );
		break;

	case( 5 ):
		// black to white to blue 
		c = determine_color_3( i, ttk_makecol( BLACK ),
					  ttk_makecol( WHITE ),
					  ttk_makecol( 0, 0, 255 )); 
		break;

	case( 6 ):
		// black to red to yellow 
		c = determine_color_3( i, ttk_makecol( BLACK ),
					  ttk_makecol( 255, 0, 0 ),
					  ttk_makecol( 255, 255, 0 )); 
		break;

	case( 7 ):
		// black to red to purple to green
		c = determine_color_4( i, ttk_makecol( BLACK ),
					  ttk_makecol( 255, 0, 0 ),
					  ttk_makecol( 255, 0, 255 ),
					  ttk_makecol( 0, 255, 0 ));
		break;

	default:
		// soft zebra
		c = determine_color_4( i, ttk_makecol( BLACK ),
					  ttk_makecol( WHITE ),
					  ttk_makecol( BLACK ),
					  ttk_makecol( WHITE ) );
	}
	return c;
}
#define PALETTES (8)

/* render the math to the workBuffer, in stripes, if applicable */
static void render_math_stripe( void )
{
	long x0,y0,p,q,xn;
	int blockx, w16;
	double xs=0.0,ys=0.0;
	register int i=0,x=0,y=0;

	if( !globs.started ) {
		globs.started = 1;
	}

	if( globs.mandelbrot == USE_MANDELBROT ) {
		xs=(globs.mnd_xmax-globs.mnd_xmin)/globs.workBuffer->w;
		ys=(globs.mnd_ymax-globs.mnd_ymin)/globs.workBuffer->h;
	} else {
		xs=(globs.jul_xmax-globs.jul_xmin)/globs.workBuffer->w;
		ys=(globs.jul_ymax-globs.jul_ymin)/globs.workBuffer->h;
	}

	w16 = globs.workBuffer->w/STRIPES;
	blockx = globs.block * w16;
	globs.swipe_xp = blockx+w16;

	if( globs.mandelbrot == USE_MANDELBROT ) {
		for (y=0;y<globs.workBuffer->h;y++) {
			for (x=blockx;x<blockx+w16+1;x++) {
				p=fixpt(globs.mnd_xmin+x*xs);
				q=fixpt(globs.mnd_ymin+y*ys);
				xn=0;
				x0=0;
				y0=0;
				i=0;
				while ((mul(xn,xn)+mul(y0,y0))<fixpt(4) && ++i<STEPS)
				{
					xn=mul((x0+y0),(x0-y0)) +p;
					y0=mul(fixpt(2),mul(x0,y0)) +q;
					x0=xn;
				}
				if (i>=100) i=1;

				ttk_pixel( globs.workBuffer, x, y, lookup_color( i ));
			}
		}
	} else {
		long xsq, ysq;
		p = fixpt( globs.jxc );
		q = fixpt( globs.jyc );
		i = 0;

		for (y=0;y<=globs.workBuffer->h;y++) {
			for (x=blockx;x<blockx+w16+1;x++) {
				y0=fixpt(globs.jul_ymax-y*ys);
				x0=fixpt(globs.jul_xmin+x*xs);
				i = 0;

				while ((ysq=mul(y0,y0))+(xsq=mul(x0,x0))<fixpt(4) && ++i<STEPS) {
					y0=mul(x0,y0)+mul(x0,y0) +q;
					x0=xsq-ysq +p;
				}

				//i = (i==STEPS) ? 1 : --i%STEPS; // warnings
				if( i==STEPS ) { // okay
					i = 1;
				} else {
					i--;
					i %= STEPS;
				}
				ttk_pixel( globs.workBuffer, x, y, lookup_color( i + 1));
			}
		} 
	}
	

	globs.block++;
	if( x>=globs.workBuffer->w )
		globs.completed = 1;
}

/* draw it to the screen */
void draw_mandelpod( PzWidget *wid, ttk_surface srf )
{
	int x,y,w,h,w16, h16;
	ttk_blit_image( globs.workBuffer, srf, 0, 0 );

	if( !globs.completed && globs.started ) {
		/* draw a scannerbar thingy */
		ttk_fillrect( srf, globs.swipe_xp,   0, 
				globs.swipe_xp+6, srf->h, 
				ttk_makecol( 255, 0, 0 ));
		ttk_fillrect( srf, globs.swipe_xp+2, 0, 
				globs.swipe_xp+2, srf->h, 
				ttk_makecol( 0, 255, 0 ));

	}

	if( globs.displayCursor ) {
		w = globs.workBuffer->w;
		h = globs.workBuffer->h;
		w16 = w/ZOOMBLOCKS;
		h16 = h/ZOOMBLOCKS;
		x = w/2 - globs.cursorX*w16;
		y = h/2 - globs.cursorY*h16;

		// crosshairs - horiz
		ttk_line( srf, -1, y, x-XHAIR_SZ, y, ttk_makecol( BLACK ));
		ttk_line( srf, x+XHAIR_SZ, y, w, y, ttk_makecol( BLACK ));
		ttk_line( srf, -1, y-1, x-XHAIR_SZ, y-1, ttk_makecol( WHITE ));
		ttk_line( srf, x+XHAIR_SZ, y-1, w, y-1, ttk_makecol( WHITE ));

		// crosshairs - vert
		ttk_line( srf, x, -1, x, y-XHAIR_SZ, ttk_makecol( WHITE ));
		ttk_line( srf, x, y+XHAIR_SZ, x, h, ttk_makecol( WHITE ));
		ttk_line( srf, x-1, -1, x-1, y-XHAIR_SZ, ttk_makecol( BLACK ));
		ttk_line( srf, x-1, y+XHAIR_SZ, x-1, h, ttk_makecol( BLACK ));

		// circle
/*
		ttk_ellipse( srf, x, y, XHAIR_SZ, XHAIR_SZ, ttk_makecol( BLACK ));
		ttk_ellipse( srf, x, y, XHAIR_SZ+1, XHAIR_SZ+1, ttk_makecol( WHITE ));
*/
	}
}


/* ********************************************************************** */

/* zoom in or out on the current position */
static void zoom_inout( int mand, int in_or_out )
{
	double xm=0, ym=0;

	// pick the right one
	double xmin = (mand == USE_MANDELBROT)? globs.mnd_xmin : globs.jul_xmin;
	double xmax = (mand == USE_MANDELBROT)? globs.mnd_xmax : globs.jul_xmax;
	double ymin = (mand == USE_MANDELBROT)? globs.mnd_ymin : globs.jul_ymin;
	double ymax = (mand == USE_MANDELBROT)? globs.mnd_ymax : globs.jul_ymax;

	// field size
	double xsz = xmax - xmin;
	double ysz = ymax - ymin;

	// cursor field size
	double wS = xsz/ZOOMBLOCKS;
	double hS = ysz/ZOOMBLOCKS;

	// field center
	double xc = xmin + (xsz/2);
	double yc = ymin + (ysz/2);



	// apply it!
	switch( in_or_out ) {
	case( ZOOM_IN ):
		xm = xsz/4;
		ym = ysz/4;
		break;
	case( ZOOM_OUT ):
		xm = xsz;
		ym = ysz;
		break;
	case( ZOOM_CENTER ):
		// adjust the field center
		xc -= globs.cursorX * wS;
		if( mand == USE_JULIA )
			yc -= -globs.cursorY * hS; /* flip julia */
		else
			yc -= globs.cursorY * hS;
		xm = xsz/2;
		ym = ysz/2;
		break;
	}

	// and store it
	if( mand == USE_MANDELBROT ) {
		globs.mnd_xmin = xc-xm;
		globs.mnd_xmax = xc+xm;
		globs.mnd_ymin = yc-ym;
		globs.mnd_ymax = yc+ym;
	} else {
		globs.jul_xmin = xc-xm;
		globs.jul_xmax = xc+xm;
		globs.jul_ymin = yc-ym;
		globs.jul_ymax = yc+ym;
	}
}

#define recenter_on_cursor( m )		zoom_inout( (m), ZOOM_CENTER );

/* recenter the julia set on the mandelbrot set */
static void center_julia_on_mandelbrot( void )
{
	globs.jxc = (globs.mnd_xmax + globs.mnd_xmin)/2;
	globs.jyc = (globs.mnd_ymax + globs.mnd_ymin)/2;
}

/* recenter mandelbrot set on the julia set - for going from julia->mandelbrot explore */
static void center_mandelbrot_on_julia( void )
{
	double w2 = (globs.mnd_xmax - globs.mnd_xmin)/2;
	double h2 = (globs.mnd_ymax - globs.mnd_ymin)/2;

	globs.mnd_xmin = globs.jxc - w2;
	globs.mnd_xmax = globs.jxc + w2;
	globs.mnd_ymin = globs.jyc - h2;
	globs.mnd_ymax = globs.jyc + h2;
}

/* ********************************************************************** */

/* handle the TTK (user input) events, and timer event */
int event_mandelpod (PzEvent *ev) 
{
	switch (ev->type) {
	case PZ_EVENT_SCROLL:
		TTK_SCROLLMOD( ev->arg, 5 );
		if( globs.displayCursor ) {
			if( ev->arg > 0 ) {
				// move the cursor
				globs.cursorX--;
			} else {
				// move the cursor
				globs.cursorX++;
			}
			// now adjust it for the screen
			if( globs.cursorX < (-ZOOMBLOCKS/2) ) {
				globs.cursorY--;
				globs.cursorX=ZOOMBLOCKS/2;
			}
			if( globs.cursorX > ZOOMBLOCKS/2 ) {
				globs.cursorY++;
				globs.cursorX=-(ZOOMBLOCKS/2);
			}
			if( globs.cursorY < (-ZOOMBLOCKS/2) ) globs.cursorY=ZOOMBLOCKS/2;
			if( globs.cursorY > ZOOMBLOCKS/2 ) globs.cursorY=-(ZOOMBLOCKS/2);
		}
		globs.displayCursor = 1;
		break;

	case PZ_EVENT_BUTTON_UP:
		switch( ev->arg ) {
		case( PZ_BUTTON_MENU ):
			pz_close_window (ev->wid->win);
			break;

		case( PZ_BUTTON_ACTION ):
			force_redraw();
			globs.paletteNo++;
			if( globs.paletteNo > PALETTES ) globs.paletteNo = 0;
			break;

		case( PZ_BUTTON_NEXT ):
			recenter_on_cursor( globs.mandelbrot );
			zoom_inout( globs.mandelbrot, ZOOM_IN );
			home_cursor();
			force_redraw();
			break;

		case( PZ_BUTTON_PREVIOUS ):
			recenter_on_cursor( globs.mandelbrot );
			zoom_inout( globs.mandelbrot, ZOOM_OUT );
			home_cursor();
			force_redraw();
			break;

		case( PZ_BUTTON_PLAY ):
			if( globs.mandelbrot == USE_MANDELBROT ) {
				globs.mandelbrot = USE_JULIA;
				recenter_on_cursor( USE_MANDELBROT );
				center_julia_on_mandelbrot();
				reset_julia_zoom(); // zoom out
				force_redraw();
				home_cursor();
			} else {
				globs.mandelbrot = USE_MANDELBROT;
				//reset_mandelbrot_zoom(); // zoom out
				center_mandelbrot_on_julia();
				force_redraw();
				home_cursor();
			}
			break;
		case( PZ_BUTTON_HOLD ):
		default:
			break;

		}
		break;

	case PZ_EVENT_TIMER:
		if( !globs.completed ) render_math_stripe();
		ev->wid->dirty = 1;
		pz_widget_set_timer( globs.widget, 5 ); // restart ourselves. (Yield)
		break;

	case PZ_EVENT_DESTROY:
		break;
	}
	return 0;
}


/* ********************************************************************** */

/* create a new window for Mandelbrot or Julia exploration */
static PzWindow *new_window( int mand )
{
	/* set up the pz/ttk windowing stuff */
	globs.window = pz_new_window( "MandelPod", PZ_WINDOW_NORMAL );
	ttk_window_hide_header( globs.window ); /* go fullscreen! */
	globs.widget = pz_add_widget( globs.window, draw_mandelpod, event_mandelpod );

	/* set up a very quick timer, so that we can be interrupted when rendering */
	pz_widget_set_timer( globs.widget, 5 );

	/* now, set up the startup values and such */
	globs.mandelbrot = mand;
	if( mand == USE_JULIA ) {
		/* we were launched from the menu;
		   set Mandelbrot center on someplace pretty */
		globs.jxc = -0.8;
		globs.jyc = 0.156;
	}
	initialize_globals();

	/* finish and return */
	return pz_finish_window( globs.window );
}

/* launch a mandelbrot explorer */
PzWindow *new_mandelpod_window()
{
	return( new_window( USE_MANDELBROT ));
}

/* launch a julia explorer */
PzWindow *new_juliapod_window()
{
	return( new_window( USE_JULIA ));
}

/* exit and clean up after ourselves */
void cleanup_mandelpod() 
{
	/* return if we have nothing to do */
	if( !globs.initted ) return;

	/* restore the header - is this necessary? */
	if( globs.window ) ttk_window_show_header( globs.window );

	/* and free our temporary render buffer */
	if( globs.workBuffer ) ttk_free_surface( globs.workBuffer );

	/* clear the init flag */
	globs.initted = 0;
}

/* our module initialization hook */
void init_mandelpod() 
{
	/* register ourselves as a module, and our cleanup routine */
	globs.module = pz_register_module ("mandelpod", cleanup_mandelpod);

	/* insert our hooks into the podzilla menu */
	pz_menu_add_action_group ("/Extras/Demos/MandelPod", "Toys", new_mandelpod_window);
	pz_menu_add_action_group ("/Extras/Demos/JuliaPod", "Toys", new_juliapod_window);
}

PZ_MOD_INIT (init_mandelpod)
