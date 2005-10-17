/**************************************************************************
 *   cmatrix.c
 *
 *   Copyright (C) 1999 Chris Allegretta
 *   Copyright (C) 2005 Alastair S - ported to podzilla
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include "pz.h"
#include "ipod.h"
#include "matrixfont.h"

static GR_WINDOW_ID		matrix_wid;
static GR_WINDOW_INFO	matrix_info;
static GR_GC_ID			matrix_gc;
static GR_TIMER_ID		matrix_timer;

/* Matrix typedef */
typedef struct cmatrix {
	int val;
	int bold;
} cmatrix;

/* Global variables, unfortunately */
static cmatrix **matrix = (cmatrix **) NULL; /* The matrix has you */
static int *length = NULL;                   /* Length of cols in each line */
static int *spaces = NULL;                   /* spaces left to fill */
static int *updates = NULL;                  /* What does this do again? :) */

static int inverted = 0, running = 0, tmbeatcmp = 5, photo = 0;
static int lines, cols;

static void matrix_free_var(void)
{
	int i;

	if (matrix != NULL) {
		for (i = 0; i <= lines; i++)
			if (matrix[i] != NULL) {
				free(matrix[i]);
				matrix[i] = NULL;
			}
		free(matrix);
		matrix = NULL;
	}
	
	if (length != NULL) {
		free(length);
		length = NULL;
	}
	if (spaces != NULL) {
		free(spaces);
		spaces = NULL;
	}
	if (updates != NULL) {
		free(updates);
		updates = NULL;
	}
}

static void matrix_exit(void)
{
	matrix_free_var();
	GrDestroyGC(matrix_gc);
	GrDestroyTimer(matrix_timer);
	pz_close_window(matrix_wid);
}

/* What we do when we're all set to exit */
static void matrix_die(char *msg)
{
	pz_error(msg);
	matrix_exit();
}

/* nmalloc from nano by Big Gaute */
static void *nmalloc(size_t howmuch)
{
	void *r;

	/* Panic save? */

	if (!(r = malloc(howmuch)))
		matrix_die("Matrix: malloc: out of memory!");

	return r;
}

/* Initialize the global variables */
static void matrix_var_init(void)
{
	int i, j;

	GrGetWindowInfo(matrix_wid, &matrix_info);

	GrClearWindow(matrix_wid, GR_FALSE);

	lines = (matrix_info.height/COL_H)+1;
	cols = (matrix_info.width/COL_W)+1;

	matrix = nmalloc(sizeof(cmatrix *) * (lines + 1));
	for (i = 0; i <= lines; i++)
		matrix[i] = nmalloc(sizeof(cmatrix) * cols);

	length = nmalloc(cols * sizeof(int));
	spaces = nmalloc(cols* sizeof(int));

	updates = nmalloc(cols * sizeof(int));

	/* Make the matrix */
	for (i = 0; i <= lines; i++) {
		for (j = 0; j <= cols - 1; j++ ) {
			matrix[i][j].val = -1;
			matrix[i][j].bold = 0;
		}
	}

	for (j = 0; j <= cols - 1; j++) {
		/* Set up spaces[] array of how many spaces to skip */
		spaces[j] = (int) rand() % lines + 1;

		/* And length of the stream */
		length[j] = (int) rand() % (lines - 3) + 3;

		/* Sentinel value for creation of new objects */
		matrix[1][j].val = 129;

		/* And set updates[] array for update speed. */
		updates[j] = (int) rand() % 3 + 1;
	}
}

static void matrix_init(void)
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
	
	matrix_free_var(); 
	matrix_var_init();
}

static void matrix_blit_char(const int row, const int col, int cha)
{
	if (cha == 129)
		cha = 0;
	GrBitmap (matrix_wid, matrix_gc, COL_W*col, COL_H*row, COL_W, COL_H,
	          matrix_code_font[cha]);
}

static void matrix_loop(void)
{
	int i, j = 0, y, z, firstcoldone = 0;
	static int count = 0;
	GR_COLOR fg;
		
	count++;
	if (count > 4)
		count = 1;

	for (j = 0; j <= cols - 1; j++) {
		if (count > updates[j]) {
			/* New style scrolling */
			if (matrix[0][j].val == -1 && matrix[1][j].val == 129
			    && spaces[j] > 0) {
				matrix[0][j].val = -1;
				spaces[j]--;
			} else if (matrix[0][j].val == -1 && matrix[1][j].val == 129){
				length[j] = (int) rand() % (lines - 3) + 3;
				matrix[0][j].val = (int) rand() % (MAXCHARS-1) + 1;
				if ((int) rand() % 2 == 1)
					matrix[0][j].bold = 2;
				spaces[j] = (int) rand() % lines + 1;
			}
			i = 0;
			y = 0;
			firstcoldone = 0;
			while (i <= lines) {
					/* Skip over spaces */
				while (i <= lines && (matrix[i][j].val == 129 ||
				       matrix[i][j].val == -1))
					i++;

				if (i > lines)
					break;

				/* Go to the head of this collumn */
				z = i;
				y = 0;
				while (i <= lines && (matrix[i][j].val != 129 &&
					matrix[i][j].val != -1)) {
					i++;
					y++;
				}

				if (i > lines) {
					matrix[z][j].val = 129;
					matrix[lines][j].bold = 1;
					matrix_blit_char(z - 1, j, matrix[z][j].val);
					continue;
				}

				matrix[i][j].val = (int) rand() % (MAXCHARS-1) + 1;

				if (matrix[i - 1][j].bold == 2) {
					matrix[i - 1][j].bold = 1;
					matrix[i][j].bold = 2;
				}

				/* If we're at the top of the collumn and it's reached its
				 * full length (about to start moving down), we do this
				 * to get it moving. This is also how we keep segments 
				 * not already growing from growing accidentally =>
				 */
				if (y > length[j] || firstcoldone) {
					matrix[z][j].val = 129;
					matrix[0][j].val = -1;
				}
				firstcoldone = 1;
				i++;
			}
			for (i = 1; i <= lines; i++) {
				if (matrix[i][j].val == 0 || matrix[i][j].bold == 2) {
					fg = inverted ? BLACK : WHITE;
					GrSetGCForeground(matrix_gc, fg);
					if (matrix[i][j].val == 0)
						matrix_blit_char(i - 1, j, 20);
					else
						matrix_blit_char(i - 1, j, matrix[i][j].val);
				} else {
					if (photo)
						fg = inverted ?
							GR_RGB(0, (int) rand() % 35 + 220, 0) :
							GR_RGB(0, (int) rand() % 100 + 120, 0);
					else
						fg = inverted ? LTGRAY : GRAY;
					GrSetGCForeground(matrix_gc, fg);
					if (matrix[i][j].val % 2 == 0) {
						if (photo)
							fg = inverted ?
								GR_RGB(0, (int) rand() % 35 + 220, 0) :
								GR_RGB(0, (int) rand() % 100 + 120, 0);
						else
							fg = inverted ? GRAY : LTGRAY;
						GrSetGCForeground(matrix_gc, fg);
					}
					if (matrix[i][j].val == 1) 
						matrix_blit_char(i - 1, j, 2);
					else if (matrix[i][j].val == -1)
						matrix_blit_char(i - 1, j, 129);
					else
						matrix_blit_char(i - 1, j, matrix[i][j].val);
				}
			}
		}
	}
}

static void matrix_do_draw(void)
{
	pz_draw_header(_("Matrix"));
}

static void matrix_timer_adjust(const int ammount)
{
	if (tmbeatcmp+ammount>=0) {
		tmbeatcmp += ammount;
	}
}

static void matrix_clear_screen( void )
{
	GR_GC_INFO gc_info;
	GrGetGCInfo(matrix_gc, &gc_info);
	GrSetGCForeground(matrix_gc, gc_info.background);
	GrFillRect(matrix_wid, matrix_gc, 0, 0,
	           matrix_info.width, matrix_info.height);
}

static int matrix_handle_event(GR_EVENT * event)
{
	GR_COLOR bg;
	static int tmbeat=5;
	int ret = 0;
	switch( event->type )
	{
	case( GR_EVENT_TYPE_TIMER ):
	tmbeat++;
	if (tmbeat>tmbeatcmp) {
		if (running)
			matrix_loop();
		tmbeat=0;
	}
	break;

	case( GR_EVENT_TYPE_KEY_DOWN ):
	switch( event->keystroke.ch )
	{
		case 'h': /* hold */
			running=0;
			break;
		case '\r':
		case '\n': /* action */
			running = 0;
			inverted = 1 - inverted;
			bg = inverted ? WHITE : BLACK;
			GrSetGCBackground(matrix_gc, bg);
			matrix_clear_screen();
			running = 1;
			break;
		case 'p': /* play/pause */
		case 'd': /*or this */
			running = 0;
			//matrix_clear_screen();
			matrix_free_var();
			if (matrix_info.height == screen_info.rows) {
				GrResizeWindow(matrix_wid, screen_info.cols,
				               screen_info.rows - (HEADER_TOPLINE + 1));
				GrMoveWindow(matrix_wid, 0, HEADER_TOPLINE + 1);
				
			} else {
				GrResizeWindow(matrix_wid, screen_info.cols, screen_info.rows);
				GrMoveWindow(matrix_wid, 0, 0);
			}
			matrix_var_init();
			matrix_clear_screen();
			matrix_loop();
			running = 1;
			break;
		case 'l': /* CCW spin */
			matrix_timer_adjust(1);
			break;
		case 'r': /* CW spin */
			matrix_timer_adjust(-1);
			break;
		case 'm':
			matrix_exit();
			ret=1;
		break;
	}
	break;
	case( GR_EVENT_TYPE_KEY_UP ):
	switch( event->keystroke.ch )
	{
		case 'h': /* hold */
			running=1;
			break;
	}
	break;
	}

	return ret;
}

void new_matrix_window( void )
{
	matrix_gc = pz_get_gc(1);
	photo = (screen_info.bpp == 16) ? 1 : 0;
		
	GrSetGCUseBackground(matrix_gc, GR_TRUE);
	GrSetGCBackground(matrix_gc, BLACK);

	matrix_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
	                           screen_info.rows - (HEADER_TOPLINE + 1), 
	                           matrix_do_draw, matrix_handle_event);

	GrSelectEvents( matrix_wid, GR_EVENT_MASK_TIMER|
	GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN);

    matrix_timer = GrCreateTimer(matrix_wid, 2);

	GrMapWindow(matrix_wid);

	matrix_init();
	matrix_clear_screen();
	running=1;
}

