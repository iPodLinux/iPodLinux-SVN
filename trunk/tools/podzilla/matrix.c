/*
 * matrix_saver.c
 * Copyright (C) 1999 Andrew Arensburger
 * ported to iPodLinux/podzilla 2005 - Alastair S
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* matrix_saver
 *
 * This is a screen saver inspired by an effect in the 1999 movie,
 * "The Matrix". It was more immediately inspired by a cool display
 * hack, 'cmatrix', by Chris Allegretta. I wound up not using any of
 * his code, but I guess this is still in some sense based on his
 * work.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "pz.h"
#include "ipod.h"
#include "matrixfont.h"

static GR_WINDOW_ID		matrix_wid;
static GR_GC_ID			matrix_gc;
static GR_TIMER_ID		matrix_timer;

static int blanked;
static int running=0;
static int inverted=0;
static int fullscreen=0;
static int height;
static int tmbeatcmp=10;

/* Columns:
 * The screen saver displays a number of falling columns. Each column
 * consists of an optional head (a white "&"), some random characters
 * above that, and some blanks above that.
 *
 * 'lengths[x]' gives the total length of the column at x.
 *
 * 'tails[x]' is initially always less than 'lengths[x]', and gives
 * the number of blanks that terminate the column. 'lengths[x]' is
 * decremented each time the column is dropped, so when 'lengths[x]'
 * becomes less than 'tails[x]', we should draw a blank, rather than a
 * random character.
 *
 * 'speeds[x]' gives the speed of the screen column at x. See the
 * "speeds" comment, below.
 */
static char *lengths = NULL;
static char *tails = NULL;
static char *speeds = NULL;

static char *runt = NULL;

/* Speeds:
 * Each column has a speed in the range 0..NUM_SPEEDS-1. The higher
 * the speed, the faster the column falls. The array 'beat_map',
 * below, is fairly magical, and has been specially set up for a
 * length of 16. So don't change NUM_SPEEDS without studying this
 * code.
 *
 * Think of the main loop (matrix_saver()) as counting beats, at 16
 * beats per measure: 0, 1, 2, ...15, 0, 1, 2, ... 15, and so forth.
 * The simple thing to do would be to drop each column if its speed is
 * greater than or equal to the current beat. But this looks jerky and
 * ugly: a column with a speed of 7, for instance, stays immobile for
 * 8 beats, then drops 8 columns quickly; what we really want is for
 * it to drop one line every other beat. A column with a speed of 3
 * should drop by one line every four beats, and so forth.
 *
 * The 'beat_map' array defines when each column falls: a column falls
 * if its speed is greater than or equal to beat_map[beat]. Thus, on
 * beat 0, all columns fall whose speed is >= 15; on beat 1, all
 * columns fall whose speed is >= 7, and so forth.
 *
 * XXX - I wish 'beat_map' weren't so magical. If you find a quick way
 * to generate it arithmetically (possibly something involving Grey
 * codes or standard dithering algorithms), please let me know.
 */
#define NUM_SPEEDS	16
static char beat_map[NUM_SPEEDS] = {
	15,  7, 11,  3, 13,  5,  9,  1,
	14,  6, 10,  2, 12,  4,  8,  0,
};

static int no_cols, no_rows;

static int **screen;

#define COL_CHAR (get_rand(MAXCHARS))

static int
get_rand(int max)
{
	static int seedinit = 0;
	
	if (!seedinit)
	{
		int fd, data;

		fd = open("/dev/random", O_RDONLY);
		if (fd == -1) {
			data = (int)time(NULL);
		}
		else {
			read(fd, &data, sizeof(data));
			close(fd);
		}
		srand(data);
		seedinit = 1;
	}

	return 1 + (int)((float)max * rand() / (RAND_MAX + 1.0));
}


static void matrix_clear_screen( void )
{
	GR_COLOR bg;
	bg = inverted ? WHITE : BLACK;
	GrSetGCForeground(matrix_gc, bg);
	GrFillRect(matrix_wid, matrix_gc,
		0, 0, screen_info.cols,
		height);
}


static void matrix_timer_adjust(const int ammount)
{
	if (tmbeatcmp+ammount>=0) {
		tmbeatcmp += ammount;
	}
}

static void matrix_blit_char(const int col, const int row, int cha)
{
	GR_COLOR colour, bg;
	colour = inverted ? GRAY : LTGRAY;
	if (cha > MAXCHARS) {
		colour = inverted ? BLACK: WHITE;
		cha = cha - MAXCHARS;
	}
	if (get_rand(5) == 1) {
		colour = inverted ? LTGRAY : GRAY;
	}
	bg = inverted ? WHITE : BLACK;
	GrSetGCForeground(matrix_gc, bg);
	GrFillRect(matrix_wid, matrix_gc, COL_W*col, COL_H*row,
		COL_W, COL_H);
	GrSetGCForeground(matrix_gc, colour);
	GrBitmap (matrix_wid, matrix_gc, COL_W*col, COL_H*row, COL_W, COL_H, matrix_code_font[cha]);
}


/* matrix_saver
 * Main loop for the screen saver. Most of this is voodoo code.
 */
static void
matrix_saver(int blank)
{
	static char	beat = 0;	/* Current beat. See comment above */
	int		x, y, i;

	beat++;
	beat %= NUM_SPEEDS;

	if (blank) {
		if (blanked == 0) {
			blanked = 1;

            /* clear the screen */
			matrix_clear_screen();

			/* Generate the bottoms of the columns, at the
			 * top of the screen */
			for (x = 0; x < no_cols; x++) {
				screen[x][0] = COL_CHAR+MAXCHARS;
				matrix_blit_char(x, 0, screen[x][0]);
			}
		}

		/* Drop and update each column in turn. */
		for (i = 0; i < no_cols; i++) {
			
			x = (int)runt[i];

			if (beat_map[(int) beat] > speeds[x])
				/* The current column does not drop on
				 * this beat. See the "speeds"
				 * comment, above.
				 */
				continue;

			/* Columns' speeds occasionally change */
			if (get_rand(100) == 1)
				speeds[x] = get_rand(NUM_SPEEDS+1)-1;

			/* Drop this column by one space */
			for (y = no_rows -1; y > 0; y--) {
				screen[x][y] = screen[x][y-1];
				matrix_blit_char(x, y, screen[x][y]);
			}
			if (lengths[x] <= 0)
			{
				/* Start a new trail */
				if (get_rand(5) == 1) {
					/* A new trail has a 1 in 5
					 * chance of having a white
					 * head ("&").
					 */
					screen[x][0] = 1+MAXCHARS;
					matrix_blit_char(x, 0, screen[x][0]);
				} else {
					screen[x][0] = COL_CHAR;
					matrix_blit_char(x, 0, screen[x][0]);
				}

				/* Pick the length of the inter-trail
				 * space first, then add to that value
				 * to get the total length; this
				 * ensures that trails are never
				 * empty.
				 * The numbers here are chosen for
				 * esthetic value. Go ahead and tweak
				 * them.
				 */
				tails[x] = get_rand(31)-1;
				lengths[x] = tails[x] + get_rand(6)-1 + 2;
			} else {
				/* We're continuing an existing trail,
				 * not starting a new one.
				 */
				if (lengths[x] < tails[x]) {
					/* Draw the inter-trail space */
					screen[x][0] = 0;
					matrix_blit_char(x, 0, screen[x][0]);
				} else {
					screen[x][0] = COL_CHAR;
					matrix_blit_char(x, 0, screen[x][0]);
				}
				lengths[x]--;
			}
		}
	} else { /* Turn off the screen saver */
		blanked = 0;
	}
}

static int rndcompfunc(const void * a, const void * b)
{
	return (get_rand(11)-1)-5;
}

static void
matrix_term(void)
{
	int x;
	free(lengths);
	free(tails);
	free(speeds);
	for (x = 0; x < no_cols ; x++ ) {
		free(screen[x]);
	}
	free(screen);
}


static void matrix_die(void)
{
	matrix_term();
	running=0; blanked=0, inverted=0; fullscreen=0;
	GrDestroyTimer( matrix_timer );
	pz_close_window( matrix_wid );
}

static void
matrix_init(void)
{
	int i;
	no_cols = ((screen_info.cols/COL_W)+1);
	no_rows = ((height/COL_H)+1);
	/* Allocate the 'lenghts', 'tails' and 'speeds' arrays. They
	 * have as many entries as there are columns on the screen.
	 */
	if ((lengths = malloc(no_cols)) == NULL)
		matrix_die();
	if ((tails = malloc(no_cols)) == NULL)
		matrix_die();
	if ((speeds = malloc(no_cols)) == NULL)
		matrix_die();
	if ((runt = malloc(no_cols)) == NULL)
		matrix_die();
	if ((screen = malloc(no_cols*sizeof(int *))) == NULL)
		matrix_die();
	for (i = 0; i < no_cols; i++)
	{
		screen[i] = malloc(no_rows*sizeof(int));
		memset(screen[i], 0, (no_rows*sizeof(int)));
		lengths[i] = get_rand(11)-1;
		tails[i] =  get_rand(16)-1 + 3;
		speeds[i] = get_rand(NUM_SPEEDS+1)-1;
		runt[i] = i;
	}
	qsort(runt, no_cols, 1, rndcompfunc);
}

static void matrix_do_draw( void )
{
	pz_draw_header("Matrix");
}

static int matrix_handle_event(GR_EVENT * event)
{
	static int tmbeat=10, hold=0;
	int ret = 0;
	switch( event->type )
	{
	case( GR_EVENT_TYPE_TIMER ):
		tmbeat++;
		if (tmbeat>tmbeatcmp) {
			if (running) {
				matrix_saver(1);
			}
			tmbeat=0;
		}
	break;
	

	case( GR_EVENT_TYPE_KEY_DOWN ):
	if (hold)
		return 0;
	switch( event->keystroke.ch )
	{
		case 'h': /* hold */
			running=0;
			hold=1;
			break;
		case '\r':
		case '\n': /* action */
			inverted = inverted ? 0 : 1;
			matrix_clear_screen();
			break;
		case 'p': /* play/pause */
		case 'd': /*or this */
			if (fullscreen) {
				running=0;
				matrix_term();
				height = (screen_info.rows - (HEADER_TOPLINE + 1));
				GrMoveWindow(matrix_wid, 0, HEADER_TOPLINE + 1);
				GrResizeWindow(matrix_wid, screen_info.cols, height);
				fullscreen=0;
				blanked=0;
				matrix_init();
				running=1;
			} else {
				running=0;
				matrix_term();
				height = screen_info.rows;
				GrResizeWindow(matrix_wid, screen_info.cols, height);
				GrMoveWindow(matrix_wid, 0, 0);
				fullscreen=1;
				blanked=0;
				matrix_init();
				running=1;
			}
			break;
		case 'l': /* CCW spin */
			matrix_timer_adjust(1);
			break;
		case 'r': /* CW spin */
			matrix_timer_adjust(-1);
			break;
		case 'm':
			matrix_die();
			ret=1;
		break;
	}
	break;
	case( GR_EVENT_TYPE_KEY_UP ):
	switch( event->keystroke.ch )
	{
		case 'h': /* hold */
			running=1;
			hold=0;
			break;
	}
	break;
	}

	return ret;
}


void new_matrix_window( void )
{
	matrix_gc = pz_get_gc(1);
	GrSetGCUseBackground(matrix_gc, GR_FALSE);
	GrSetGCForeground(matrix_gc, BLACK);

	matrix_wid = pz_new_window(0, HEADER_TOPLINE + 1, 
	screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), 
	matrix_do_draw, matrix_handle_event);

	GrSelectEvents( matrix_wid, GR_EVENT_MASK_TIMER|
	GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN);

    matrix_timer = GrCreateTimer( matrix_wid, 10 );

	GrMapWindow( matrix_wid );

	height = (screen_info.rows - (HEADER_TOPLINE + 1));
	
	matrix_init();
	running=1;	
}
