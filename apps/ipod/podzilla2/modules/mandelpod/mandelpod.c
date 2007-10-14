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

typedef struct mandglobs {
	PzWindow *window;
	PzModule *module;
	PzWidget *widget;
	ttk_surface workBuffer;

	int initted;
	int mandelbrot;

	int completed;
	int started;
	int paletteNo;
	int block;

	int displayCursor;
	int cursorX, cursorY;

	double xmin, xmax, ymin, ymax;
} mandglobs;

static mandglobs globs;

#define FIXSIZE 25
#define mul(a,b) ((((long long)a)*(b))>>FIXSIZE)
#define fixpt(a) ((long)(((a)*(1<<FIXSIZE))))
#define integer(a) (((a)+(1<<(FIXSIZE-1)))>>FIXSIZE)

#define STEPS (64)	/* color step levels - more = slower */
#define ZOOMBLOCKS (8)	/* number of blocks the screen is broken into */

static void force_redraw( void )
{
	globs.displayCursor = 0;
	globs.completed = 0;
	globs.started = 0;
	globs.block = 0;
	globs.cursorX = 0;
	globs.cursorY = 0;
}

static void initialize_globals( void )
{
	globs.workBuffer = ttk_new_surface( ttk_screen->w, ttk_screen->h, ttk_screen->bpp );
	ttk_fillrect( globs.workBuffer, 0, 0, 
			globs.workBuffer->w, globs.workBuffer->h, 
			ttk_makecol( 0, 0, 255 ));
	globs.paletteNo = 0;
	force_redraw();
	globs.initted = 1;

	if( globs.mandelbrot ) {
		globs.xmin = -2.5;
		globs.xmax = 1.5;
		globs.ymin = -1.5;
		globs.ymax = 1.5;
	} else {
		globs.xmin = -1.5;
		globs.xmax = 1.5;
		globs.ymin = -1.5;
		globs.ymax = 1.5;
	}
}

// -- these should really be just simple lookup tables...
#define PALETTES (4)
static ttk_color lookup_color( int i )
{
	int r=0, g=0, b=0;
	switch( globs.paletteNo ) {
	case( 0 ):
		// black to white
		if( i>=STEPS ) i=STEPS;
		r = i*255/STEPS;
		//if( i>=STEPS ) return( ttk_makecol( 255, 255, 0 ));
		return ttk_makecol( r, r, r );
	case( 1 ):
		// white to black
		if( i>=STEPS ) i=STEPS;
		r = 255-(i*255/STEPS);
		//if( i>=STEPS ) return( ttk_makecol( 255, 255, 0 ));
		return ttk_makecol( r, r, r );
	case( 2 ):
		// red to cyan
		if( i>= STEPS ) i=STEPS;
		r = 255-(i*255/STEPS);
		g = i*255/STEPS;
		b = i*255/STEPS/2;
		return( ttk_makecol( r, g, b ));
	case( 3 ):
		// zebra
		if( i>= STEPS ) i=STEPS;
		r = g = b = (i&0x08)?255:0;
		return( ttk_makecol( r, g, b ));
	case( 4 ):
		// black to white to blue 
		if( i>= STEPS ) i=STEPS;
		if( i< STEPS/2 ) {
			r=g=b = i * 255 / (STEPS/2);
		} else {
			i-=STEPS/2;
			b=255;
			g=r= 255-( i*255/(STEPS/2));
		}
		return( ttk_makecol( r, g, b ));
	}
	return 0;
}

static void render_frame( void )
{
	int blockx, w16;
	long x0,y0,p,q,xn,tot;
	double xs,ys;
	register int i=0,x=0,y=0;
	int xsq, ysq;
	long a = 0;


	if( !globs.started ) {
		globs.started = 1;
	}

	xs=(globs.xmax-globs.xmin)/globs.workBuffer->w;
	ys=(globs.ymax-globs.ymin)/globs.workBuffer->h;

	w16 = globs.workBuffer->w/16;
	blockx = globs.block * w16;

	/* draw a thing to know where we are */
	ttk_fillrect( globs.workBuffer, blockx+w16, 0, blockx+w16+20, globs.workBuffer->h, ttk_makecol( 255, 0, 0 ));
	ttk_fillrect( globs.workBuffer, blockx+w16+5, 0, blockx+w16+15, globs.workBuffer->h, ttk_makecol( 0, 255, 0 ));

	if( globs.mandelbrot ) {
		for (y=0;y<globs.workBuffer->h;y++) {
			for (x=blockx;x<blockx+w16+1;x++) {                        
				p=fixpt(globs.xmin+x*xs);
				q=fixpt(globs.ymin+y*ys);
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
				tot+=i;
				if (i>=100) i=1;

				ttk_pixel( globs.workBuffer, x, y, lookup_color( i ));
			}
		}
	} else {
		a++;
		p=1.5-sin(a/26.6)*3;
		q=1.5-cos(a/31.15)*3;
		for (y=0;y<=globs.workBuffer->h;y++) {
			for (x=blockx;x<blockx+w16+1;x++) {
				y0=globs.ymax-y*ys;
				x0=globs.xmin+x*xs;

				for (i=1;i<STEPS && (ysq=y0*y0)+(xsq=x0*x0)<4;i++) {
					y0=q+(x0*y0)+(x0*y0);
					x0=p+xsq-ysq;
				}

				//i=(i==STEPS) ? 1 : --i%255; 
				ttk_pixel( globs.workBuffer, x, y, lookup_color( i ));
			}
		} 
	}
	

	globs.block++;
	if( x>=globs.workBuffer->w )
		globs.completed = 1;
}

#define XHAIR_SZ 	(20)
void draw_mandelpod( PzWidget *wid, ttk_surface srf )
{
	int x,y,w,h,w16, h16;
	ttk_blit_image( globs.workBuffer, srf, 0, 0 );

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
		ttk_ellipse( srf, x, y, XHAIR_SZ, XHAIR_SZ, ttk_makecol( BLACK ));
		ttk_ellipse( srf, x, y, XHAIR_SZ+1, XHAIR_SZ+1, ttk_makecol( WHITE ));
	}
}


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
			// cheesy zoom in
			globs.xmin /=2;
			globs.xmax /=2;
			globs.ymin /=2;
			globs.ymax /=2;
			force_redraw();
			break;

		case( PZ_BUTTON_PREVIOUS ):
			// cheesy zoom out
			globs.xmin *=2;
			globs.xmax *=2;
			globs.ymin *=2;
			globs.ymax *=2;
			force_redraw();
			break;

		case( PZ_BUTTON_HOLD ):
		case( PZ_BUTTON_PLAY ):
		default:
			break;

		}
		break;

	case PZ_EVENT_TIMER:
		if( !globs.completed ) render_frame();
		ev->wid->dirty = 1;
		pz_widget_set_timer( globs.widget, 5 ); // restart ourselves. (Yield)
		break;

	case PZ_EVENT_DESTROY:
		break;
	}
	return 0;
}

static PzWindow *new_window( int mand )
{
	globs.window = pz_new_window( "MandelPod", PZ_WINDOW_NORMAL );
	ttk_window_hide_header( globs.window );
	globs.widget = pz_add_widget( globs.window, draw_mandelpod, event_mandelpod );
	pz_widget_set_timer( globs.widget, 5 );
	globs.mandelbrot = mand;
	initialize_globals();
	return pz_finish_window( globs.window );
}

PzWindow *new_mandelpod_window()
{
	return( new_window( 1 ));
}

PzWindow *new_juliapod_window()
{
	return( new_window( 0 ));
}

void cleanup_mandelpod() 
{
	if( !globs.initted ) return;
	if( globs.window ) ttk_window_show_header( globs.window );
	if( globs.workBuffer ) ttk_free_surface( globs.workBuffer );
	globs.initted = 0;
}

void init_mandelpod() 
{
	globs.module = pz_register_module ("mandelpod", cleanup_mandelpod);
	pz_menu_add_action ("/Extras/Demos/MandelPod", new_mandelpod_window);
//	pz_menu_add_action ("/Extras/Demos/JuliaPod", new_juliapod_window);
}

PZ_MOD_INIT (init_mandelpod)
