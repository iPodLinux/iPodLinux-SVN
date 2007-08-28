/*
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
 */

#include <stdlib.h>
#include <stdio.h> 
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "pz.h"
#include "piezo.h"

#define ITERATIONS 24

// microwindows objects
static GR_WINDOW_ID mandel_wid;
#ifdef MANDELPOD_STATUS
static GR_WINDOW_ID status_wid;
#endif
static GR_WINDOW_ID status_image[16];
static GR_GC_ID mandel_gc;
static GR_WINDOW_INFO wi;
static	GR_EVENT break_event;
static GR_TIMER_ID mandel_timer_id;
static const GR_COLOR gray_palette[] = { GRAY, GRAY, LTGRAY,  WHITE, LTGRAY, LTGRAY, GRAY, BLACK };
static const GR_COLOR color_palette[] = { 
	GR_RGB( 255,   0,   0 ),
	GR_RGB( 255, 128,   0 ),
	GR_RGB( 255, 255,   0 ),
	GR_RGB(   0, 255,   0 ),
	GR_RGB(   0, 255, 255 ),
	GR_RGB(   0,   0, 255 ),
	GR_RGB( 255,   0, 255 ),
	GR_RGB(   0,   0,   0 )
};

// function declarations
void new_mandel_window();
static int handle_event(GR_EVENT *event);
static void mandel_quit();

static void draw_mandel();
#ifdef MANDELPOD_STATUS
static void draw_idle_status();
static void draw_busy_status();
#endif
static void draw_header();
static void draw_cursor();
static void create_status();
static void init_values();
static void calculate_mandel();
static void check_event();
static void pause_drawing();

static void move_right();
static void move_left();
static void zoom_in();
static void zoom_out();

// level data structure
typedef struct level
{
	int cursor_pos;
	GR_WINDOW_ID mandel_buffer;
} LEVEL;
static LEVEL level[7];

// various variables
static int screen_width,screen_height;
static int selection_width,selection_height;
static const char selection_size = 8;
static signed char cursor_x, cursor_y;
static signed char current_depth;
static const signed char max_depth = 6;

// mandelbrot variables
static int iterations;
static double xMin,yMin,xMax,yMax;

// defines for the mandelbrot algorithm
#define FIXSIZE 25
#define mul(a,b) ((((long long)a)*(b))>>FIXSIZE)
#define fixpt(a) ((long)(((a)*(1<<FIXSIZE))))
#define integer(a) (((a)+(1<<(FIXSIZE-1)))>>FIXSIZE)

// status variables
static char rendering = 0;
static char show_cursor = 0;
static char paused = 0;
#ifdef MANDELPOD_STATUS
static int status_counter = 0;
#endif
static char active_renderer = 0;

// creates a new MandelPod window
void new_mandel_window(void)
{
	// get the graphics context
	mandel_gc = pz_get_gc(1);
		
	// create the main window
	mandel_wid = pz_new_window (0, 21,
				screen_info.cols, screen_info.rows - (HEADER_TOPLINE+1),
				draw_header, handle_event);
#ifdef MANDELPOD_STATUS
	// create the status window
	status_wid = pz_new_window (22, 4, 12, 12, draw_idle_status, handle_event);
#endif
	 // get screen info
	GrGetWindowInfo(mandel_wid, &wi);
	
	// select the event types
	GrSelectEvents (mandel_wid, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_TIMER);
		
	// display the window
	GrMapWindow (mandel_wid);
#ifdef MANDELPOD_STATUS
	GrMapWindow (status_wid);
#endif

    // create the timer for the busy status animation
    mandel_timer_id = GrCreateTimer (mandel_wid, 250);

	// start main app
	init_values();
	create_status();
	draw_header();
	calculate_mandel();
}
	
// draw the title bar.
static void draw_header()
{
	pz_draw_header (_("MandelPod"));
}

// reset the variables to their initial values
static void init_values() {
	
	int i;
	screen_width=screen_info.cols;
	screen_height=screen_info.rows - (HEADER_TOPLINE + 1);
	
	selection_width = screen_width/selection_size;
	selection_height = screen_height/selection_size;

	iterations = ITERATIONS;

	xMin=-2.0;
	yMin=-1.25;
	xMax=1.25;
	yMax=1.25;

	cursor_x=0;
	cursor_y=0;
	current_depth = 0;
	level[current_depth].cursor_pos = -1;

	active_renderer=0;
	
	GrSetGCForeground(mandel_gc, BLACK);
	for (i=0;i<max_depth+1;i++)  {
		level[i].mandel_buffer = GrNewPixmap(screen_width, screen_height,  NULL);
		GrFillRect(level[i].mandel_buffer,mandel_gc,0,0,screen_width, screen_height);	
	}
}

// create the circular status animation
static void create_status() {

	int i;
	int xpie[] = {  0,  3, 6,  3, 0, -3, -6, -3 }; 
	int ypie[] = { -6, -3, 0,  3, 6,  3,  0, -3 }; 

	// we need 16 frames
	GrSetGCForeground (mandel_gc, appearance_get_color( CS_TITLEBG ));

	for (i=0;i<16;i++) {
		status_image[i] = GrNewPixmap(12, 12,  NULL);
		GrFillRect(status_image[i],mandel_gc,0,0,12,12);	
	}
	
	// the background
	for (i=1;i<8;i++) {
		GrSetGCForeground(mandel_gc, appearance_get_color(CS_TITLEFG));
		GrFillEllipse(status_image[i],mandel_gc,6,6,6,6);
		GrSetGCForeground(mandel_gc, appearance_get_color(CS_TITLEBG));
		GrArc(status_image[i],mandel_gc,6,6,6,6,0,-6,xpie[i],ypie[i],MWPIE);
	}
	// the foreground part
	GrSetGCForeground(mandel_gc, appearance_get_color(CS_TITLEFG));
	GrFillEllipse(status_image[8],mandel_gc,6,6,6,6);
	for (i=9;i<16;i++) GrArc(status_image[i],mandel_gc,6,6,6,6,0,-6,xpie[i-8],ypie[i-8],MWPIE);
}

// delete the status window contents
#ifdef MANDELPOD_STATUS
static void draw_idle_status() {
	GrSetGCForeground (mandel_gc, appearance_get_color( CS_TITLEBG ));
	GrFillRect( status_wid, mandel_gc, 0, 0, 12, 12 );
	status_counter=0;
}

// update the status window contents
static void draw_busy_status() {
	GrCopyArea(status_wid, mandel_gc, 0, 0,  12, 12,status_image[status_counter%16], 0, 0, MWROP_SRCCOPY);
	status_counter++;
}
#endif


// calculate the current mandelbrot set
static void calculate_mandel()
{		
	const GR_COLOR * the_palette = gray_palette;

	const int sx = screen_width;
	const int sy = screen_height;
	const int iter = iterations-1;
	
	const double xmin=xMin;
	const double ymin=yMin;
	const double xmax=xMax;
	const double ymax=yMax;

	const double xs=(xmax-xmin)/sx;
	const double ys=(ymax-ymin)/sy;

	register long x0,y0,p,q,xn,tot;
	register int i=0,x=0,y=0;

	const int depth = current_depth;

	if( screen_info.bpp == 16 )
		the_palette = color_palette;
	
	rendering=1;
	show_cursor=0;
	active_renderer++;
	
	// start main loop
	for (y=0;y<sy;y++) {
		for (x=0;x<sx;x++) { 
			p=fixpt(xmin+x*xs);
			q=fixpt(ymin+y*ys);
			xn=0;
			x0=0;
			y0=0;
			i=0;
			while ((mul(xn,xn)+mul(y0,y0))<fixpt(4) && ++i<iter)  {
				xn=mul((x0+y0),(x0-y0)) +p;
				y0=mul(fixpt(2),mul(x0,y0)) +q;
				x0=xn;
			}
			tot+=i;
		
			GrSetGCForeground(mandel_gc, the_palette[i%8]);
			GrPoint(level[depth].mandel_buffer, mandel_gc,x,y);
		}
		
		// for every line only:
		// check if we had an event
		check_event();
		// check if we need to quit
		if (!rendering) return;
		// update the screen
		draw_mandel();
	}
	// end main loop
	
	active_renderer--;
}

// pause
static void pause_drawing() {

#ifdef MANDELPOD_STATUS
	draw_idle_status();
#endif
	while (paused) {
		check_event();
		usleep(100);
	}

}

// check for events & delete the event queue
static void check_event() {
		if (GrPeekEvent(&break_event)) { 
			GrGetNextEventTimeout(&break_event, 1000);
			if (break_event.type != GR_EVENT_TYPE_TIMEOUT) {
				pz_event_handler(&break_event);
				// delete the rest of the event queue
				while ( break_event.type != GR_EVENT_TYPE_NONE ) 
				    GrGetNextEventTimeout(&break_event, 0);
			}
		}
}

// draw the current mandelbro set
static void draw_mandel() {
		GrCopyArea(mandel_wid, mandel_gc, 0, 0,
			   screen_width, screen_height,
			   level[current_depth].mandel_buffer, 0, 0, MWROP_SRCCOPY);
			   
		if (show_cursor) draw_cursor();
}

// draw the selector rectangle
static void draw_cursor() {
	show_cursor=1;
	cursor_x = level[current_depth].cursor_pos%selection_size;
	cursor_y = level[current_depth].cursor_pos/selection_size;

	// update the screen with last picture
	GrCopyArea(mandel_wid, mandel_gc, 0, 0,
			   screen_width, screen_height,
			   level[current_depth].mandel_buffer, 0, 0, MWROP_SRCCOPY);	
	GrSetGCForeground(mandel_gc, BLACK);
	GrRect(mandel_wid, mandel_gc, cursor_x*selection_width-1, cursor_y*selection_height-1, selection_width+2, selection_height+2);
	GrSetGCForeground(mandel_gc, WHITE);
	GrRect(mandel_wid, mandel_gc, cursor_x*selection_width-2, cursor_y*selection_height-2, selection_width+4, selection_height+4);
}

// move cursor right
static void move_right() {
	level[current_depth].cursor_pos++;
	if(level[current_depth].cursor_pos>(selection_size*selection_size-1)) level[current_depth].cursor_pos=0;
	draw_cursor();
}

// move cursor left
static void move_left() {
	level[current_depth].cursor_pos--;
	if(level[current_depth].cursor_pos<0) level[current_depth].cursor_pos=(selection_size*selection_size-1);
	draw_cursor();
}

// as its name suggests
static void zoom_in() {

	double dx,dy;

	current_depth++;
	if (current_depth>max_depth) {
		current_depth=max_depth;
		beep();
		return;
	}
	if (current_depth%2) iterations=iterations*2;

	if (xMax>xMin) dx = (xMax -xMin)/selection_size;
	else dx = (xMin -xMax)/selection_size;
	xMin+=cursor_x*dx;
	xMax-=(selection_size-1-cursor_x)*dx;

	if (yMax>yMin) dy = (yMax -yMin)/selection_size;
	else dy = (yMin -yMax)/selection_size;
	yMin+=(cursor_y)*dy;
	yMax-=(selection_size-1-cursor_y)*dy;
		
	level[current_depth].cursor_pos=0;
	cursor_x=0;
	cursor_y=0;
	
	GrSetGCForeground(mandel_gc, BLACK);
	GrFillRect(level[current_depth].mandel_buffer,mandel_gc,0,0,screen_width, screen_height);	
	calculate_mandel();
}

// as its name suggests
static void zoom_out() {

	double dx,dy;

	current_depth--;
	if (current_depth<0) {
		current_depth=0;
		mandel_quit();
		return;
	}
	if (!(current_depth%2)) iterations=iterations/2;

	cursor_x = level[current_depth].cursor_pos%selection_size;
	cursor_y = level[current_depth].cursor_pos/selection_size;

	if (xMax>xMin) dx = xMax-xMin;
	else dx = xMin -xMax;
	xMin-=cursor_x*dx;
	xMax+=(selection_size-1-cursor_x)*dx;

	if (yMax>yMin) dy = yMax -yMin;
	else dy = yMin -yMax;
	yMin-=cursor_y*dy;
	yMax+=(selection_size-1-cursor_y)*dy;

	GrCopyArea(mandel_wid, mandel_gc, 0, 0,
			   screen_width, screen_height,
			   level[current_depth].mandel_buffer, 0, 0, MWROP_SRCCOPY);

	draw_cursor();
}

// clean up & quit
static void mandel_quit() {
	int i;
	rendering=0;
	paused=0;
	
	pz_close_window (mandel_wid);
#ifdef MANDELPOD_STATUS
	pz_close_window (status_wid);
#endif
	for (i=0;i<max_depth+1;i++) 
		GrDestroyWindow(level[i].mandel_buffer);
	for (i=0;i<16;i++) 
		GrDestroyWindow(status_image[i]);
	GrDestroyGC(mandel_gc);
	
}

// handle key events
static int handle_event(GR_EVENT *event)
{
	 int ret = 0;
    static int last_active_renderer = -1;

    switch (event->type)
    {
		case GR_EVENT_TYPE_TIMER:
			if( active_renderer != last_active_renderer ) {
				if( active_renderer ) 
					pz_draw_header("working...");
				else
					draw_header();
				last_active_renderer = active_renderer;
			}

#ifdef MANDELPOD_STATUS
			if (active_renderer) draw_busy_status();
			else draw_idle_status();
#endif
			break;

		case GR_EVENT_TYPE_KEY_DOWN:
			switch (event->keystroke.ch)
			{
				case 'm': // Menu button.
					mandel_quit();
					break;
				case '\r': // action button
					zoom_in();
					break;
				case 'w': // rewind button
					zoom_out();
					break;
				case 'f': // fast forward button
					zoom_in();
					break;
				case 'l': // scroll wheel left
					if (current_depth<max_depth) move_left();
					else ret |= KEY_CLICK;
					break;
				case 'r': // scroll wheel right
					if (current_depth<max_depth) move_right();
					else ret |= KEY_CLICK;
					break;
				case 'd': // play button
					paused=!(paused);
					if (show_cursor) {
						show_cursor=0;
						draw_mandel();
					}
					if (paused) pause_drawing();
				case 'h': // Hold button
					ret |= KEY_UNUSED;
					break;
				default:
					break;
			}
			break; 
		default:
			ret |= EVENT_UNUSED;
			break;
    }
	return ret;
}


