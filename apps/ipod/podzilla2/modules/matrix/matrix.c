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
#include "matrixfont.h"

static PzWindow		*matrix_wid;
static ttk_color	background;

/* Matrix typedef */
typedef struct cmatrix {
	int val;
	int bold;
	ttk_color col;
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

static void matrix_cleanup(void)
{
	matrix_free_var();
	pz_close_window(matrix_wid);
}

/* What we do when we're all set to exit */
static void matrix_die(char *msg)
{
	pz_error(msg);
	matrix_cleanup();
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

	background = ttk_makecol(BLACK);

	lines = (matrix_wid->h/COL_H)+1;
	cols = (matrix_wid->w/COL_W)+1;

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

static void matrix_blit_char(ttk_surface srf, const int r, const int c, int cha, ttk_color col)
{
	if (cha == 129)
		cha = 0;
	ttk_bitmap (srf, COL_W*c, COL_H*r, COL_W, COL_H,
	          matrix_code_font[cha], col);
}

static void matrix_draw(PzWidget *wid, ttk_surface srf)
{
	ttk_color fg;
	int i, j;
	ttk_fillrect(matrix_wid->srf, 0,0, matrix_wid->w, matrix_wid->h, background);
	for (j = 0; j <= cols - 1; j++) {
		for (i = 1; i <= lines; i++) {
			fg = matrix[i][j].col;
			if (matrix[i][j].val == 0 || matrix[i][j].bold == 2) {
				if (matrix[i][j].val == 0)
					matrix_blit_char(srf, i - 1, j, 20, fg);
				else
					matrix_blit_char(srf, i - 1, j, matrix[i][j].val, fg);
			} else {
				if (matrix[i][j].val == 1) 
					matrix_blit_char(srf, i - 1, j, 2, fg);
				else if (matrix[i][j].val == -1)
					matrix_blit_char(srf, i - 1, j, 129, fg);
				else
					matrix_blit_char(srf, i - 1, j, matrix[i][j].val, fg);
			}
		}
	}
}

static void matrix_update()
{
	ttk_color fg;
	int i, j = 0, y, z, firstcoldone = 0;
	static int count = 0;

	count++;
	if (count > 4)
		count = 1;

	for (j = 0; j <= cols - 1; j++) {
		if (count <= updates[j])
			continue;
		/* New style scrolling */
		if (matrix[0][j].val == -1 && matrix[1][j].val == 129
		    && spaces[j] > 0) {
			matrix[0][j].val = -1;
			spaces[j]--;
		} else if (matrix[0][j].val == -1 && matrix[1][j].val == 129) {
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
				//matrix_blit_char(z - 1, j, matrix[z][j].val, fg);
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
				fg = inverted ? ttk_makecol(BLACK) : ttk_makecol(WHITE);
			} else {
				if (photo) {
					fg = inverted ?
						ttk_makecol(0, (int) rand() % 35 + 220, 0) :
						ttk_makecol(0, (int) rand() % 100 + 120, 0);
				}
				else if (matrix[i][j].val % 2 != 0)
					fg = inverted ? ttk_makecol(GREY) : ttk_makecol(DKGREY);
				else
					fg = inverted ? ttk_makecol(DKGREY) : ttk_makecol(GREY);
			}
			matrix[i][j].col = fg;
		}
	}
	matrix_wid->dirty = 1;
}

static void matrix_timer_adjust(const int amount)
{
	if (tmbeatcmp + amount >= 0) {
		tmbeatcmp += amount;
	}
}

static int matrix_handle_event(PzEvent * event)
{
	static int tmbeat = 5;
	int ret = 0;
	switch( event->type )
	{
	case( PZ_EVENT_TIMER ):
	tmbeat++;
	if (tmbeat > tmbeatcmp) {
		if (running)
			matrix_update();
		tmbeat = 0;
	}
	break;

	case( PZ_EVENT_BUTTON_DOWN ):
	switch( event->arg )
	{
		case PZ_BUTTON_HOLD:
			running = 0;
			break;
		case PZ_BUTTON_ACTION:
			inverted = 1 - inverted;
			background = inverted ? ttk_makecol(WHITE) : ttk_makecol(BLACK);
			event->wid->win->dirty = 1;
			break;
		case PZ_BUTTON_MENU:
			matrix_cleanup();
			break;
	}
	break;
	case( PZ_EVENT_SCROLL ):
		matrix_timer_adjust(event->arg);
		break;
	case( PZ_EVENT_BUTTON_UP ):
	switch( event->arg )
	{
		case PZ_BUTTON_HOLD:
			running=1;
			break;
	}
	break;
	}

	return ret;
}

PzWindow *new_matrix_window( void )
{

	PzWindow *ret = matrix_wid = pz_new_window("Matrix", PZ_WINDOW_NORMAL);
	photo = (ttk_screen->bpp == 16) ? 1 : 0;

	ttk_window_hide_header(ret);
	PzWidget *wid = pz_add_widget(ret, matrix_draw, matrix_handle_event);

	pz_widget_set_timer(wid, 20);

	matrix_init();

	running = 1;
	return pz_finish_window(ret);
}

static void init_matrix()
{
	pz_register_module("matrix", NULL);
	pz_menu_add_action_group ("/Extras/Demos/Matrix", "Toys",
			new_matrix_window);
}

PZ_MOD_INIT (init_matrix)
