/*
 * Copyright (C) 2004 Alastair Stuart
 *
 * Loosely based on Nimesweeper. Inspired by civil wars everywhere :(
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
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#include "pz.h"

static GR_WINDOW_ID    mines_wid;
static GR_WINDOW_INFO  mines_info;
static GR_GC_ID        mines_gc;

#define PHOTO (screen_info.bpp == 16) 

#define HIGHSCORE ".minesweep"
/* mines */
#define CLEAR     0
#define MINED     9
/* flags */
#define UNCOVERED 1
#define FLAGGED   2
/* states */
#define INIT      1
#define RUNNING   2
#define PAUSED    3
#define GAMEWON   4
#define GAMELOST  5

typedef struct _sq_st {
	int mines;
	int flags;
} sq_st;

typedef struct _minefield_st {
	int sq_size;
	int x_offset;
	int y_offset;
	int width;
	int height;
	int mines;
	int flags_used;
	int correct_flags;
	int sel_x;
	int sel_y;
	unsigned long gametime;
	struct timeval start;
	sq_st **grid;
} minefield_st;

static minefield_st *mf =  NULL;
static int state = 0;

static void mines_free_vars(void)
{
	int i;

	if (mf->grid != NULL) {
		for (i = 0; i < mf->width; i++)
			if (mf->grid[i] != NULL) {
				free(mf->grid[i]);
				mf->grid[i] = NULL;
			}
		free(mf->grid);
		mf->grid = NULL;
	}
	
	if (mf != NULL) {
		free(mf);
		mf = NULL;
	}
}

static void mines_exit(void)
{
	mines_free_vars();
	GrDestroyGC(mines_gc);
	pz_close_window(mines_wid);
}

static void mines_die(char *msg)
{
	pz_error(msg);
	mines_exit();
}

static void *mines_malloc(size_t bytes)
{
	void *ptr;
	if (!(ptr = malloc(bytes)))
		mines_die("Minesweeper: Out of memory.");
	if (!(ptr = memset(ptr, 0, bytes)))
		mines_die("Minesweeper: Can't clear memory.");
	return ptr;
}

static void mines_seed_rand(void)
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
}

#define ADD_IF_MINED(X, Y) \
	if (mf->grid[X][Y].mines == MINED) \
	  mf->grid[x][y].mines++;

static void mines_find_neighbours(void)
{
	int x, y;
	for (x = 0; x < mf->width; x++) {
		for (y = 0; y < mf->height; y++) {
			if (mf->grid[x][y].mines == MINED)
				continue;
			if (y > 0)
				ADD_IF_MINED(x, y - 1)
			if (x < mf->width - 1 && y > 0)
				ADD_IF_MINED(x + 1, y - 1)
			if (x < mf->width - 1)
				ADD_IF_MINED(x + 1, y)
			if (x < mf->width - 1 && y < mf->height - 1)
				ADD_IF_MINED(x + 1, y + 1)
			if (y < mf->height - 1)
				ADD_IF_MINED(x, y + 1)
			if (x > 0 && y < mf->height - 1)
				ADD_IF_MINED(x - 1, y + 1)
			if (x > 0)
				ADD_IF_MINED(x - 1, y)
			if (x > 0 && y > 0)
				ADD_IF_MINED(x - 1, y - 1)
		}
	}
}

static void mines_lay_mines(void)
{
	int i, x, y;
	
	mines_seed_rand();
	
	for (i = 0; i < mf->mines; i++) {
		x = (rand() % mf->width);
		y = (rand() % mf->height);
		
		if (mf->grid[x][y].mines == CLEAR)
			mf->grid[x][y].mines = MINED;
		else
			i--;
	}
}

static void mines_init_vars(void)
{
	int i;

	GrGetWindowInfo(mines_wid, &mines_info);
	
	mf = mines_malloc(sizeof(minefield_st));
	
	state = INIT;
	mf->sq_size = 12;
	mf->width = mines_info.width / mf->sq_size;
	mf->height = (mines_info.height / mf->sq_size) - 1;
	mf->sel_x = mf->width / 2;
	mf->sel_y = mf->height / 2;
	mf->x_offset = (mines_info.width -
	                      (mf->sq_size * mf->width + 1)) / 2;
	mf->y_offset = (mines_info.height -
	                      (mf->sq_size * (mf->height + 1))) / 2;
	
	mf->mines = (mf->width * mf->height) / 8;
	mf->flags_used = 0;
	mf->correct_flags = 0;
	mf->gametime = 0;
	gettimeofday(&mf->start, NULL);
	
	mf->grid = mines_malloc(sizeof(sq_st *) * mf->width);
	for (i = 0; i < mf->width; i++) {
		mf->grid[i] = mines_malloc(sizeof(sq_st) * mf->height);
	}
	
	mines_lay_mines();
	mines_find_neighbours();
	state = RUNNING;
}

static void mines_draw_grid(void)
{
	int i;
	
	GrSetGCForeground(mines_gc, GRAY);
	for (i = mf->y_offset; i <= mf->y_offset + (mf->sq_size * mf->height);
	     i += mf->sq_size) {
		GrLine(mines_wid, mines_gc, mf->x_offset, i,
		       mf->x_offset + (mf->width * mf->sq_size), i);
	}
	
	for (i = mf->x_offset; i <= mf->x_offset + (mf->sq_size * mf->width);
	     i += mf->sq_size) {
		GrLine(mines_wid, mines_gc, i, mf->y_offset,
		       i, mf->y_offset + (mf->height * mf->sq_size));
	}
}

static void mines_draw_clear(const int x, const int y)
{
	int x_pos = mf->x_offset + (x * mf->sq_size) + 1;
	int y_pos = mf->y_offset + (y * mf->sq_size) + 1;
	
	if (y == mf->height) //status hack :(
		y_pos += ((mines_info.height - (mf->y_offset +
		         (mf->height * mf->sq_size))) / 2) - (mf->sq_size / 2);
	GrFillRect(mines_wid, mines_gc, x_pos, y_pos,
	           mf->sq_size - 1, mf->sq_size - 1);
}

static void mines_draw_xor(const int x, const int y)
{
	GrSetGCMode(mines_gc, GR_MODE_XOR);
	if (hw_version == 0 || (hw_version >= 60000 && hw_version < 70000))
		GrSetGCForeground(mines_gc, WHITE);
	else
		GrSetGCForeground(mines_gc, BLACK);
	mines_draw_clear(x, y);
	GrSetGCMode(mines_gc, GR_MODE_SET);
}

static void mines_draw_unknown(const int x, const int y)
{
	int x_centre = mf->x_offset + (x * mf->sq_size) + (mf->sq_size / 2);
	int y_centre = mf->y_offset + (y * mf->sq_size) + (mf->sq_size / 2);
	
	GrSetGCForeground(mines_gc, WHITE);
	mines_draw_clear(x, y);
	GrSetGCForeground(mines_gc, LTGRAY);
	GrFillEllipse(mines_wid, mines_gc, x_centre, y_centre,
	              mf->sq_size / 6, mf->sq_size / 6);
}

static void mines_draw_mine(const int x, const int y)
{
	int x_centre = mf->x_offset + (x * mf->sq_size) + (mf->sq_size / 2);
	int y_centre = mf->y_offset + (y * mf->sq_size) + (mf->sq_size / 2);
	GR_COLOR colour;

	GrSetGCForeground(mines_gc, WHITE);
	mines_draw_clear(x, y);
	colour = PHOTO ? GREEN : GRAY;
	GrSetGCForeground(mines_gc, colour);
	GrFillEllipse(mines_wid, mines_gc, x_centre, y_centre,
	              mf->sq_size / 3, mf->sq_size / 3);
	GrSetGCForeground(mines_gc, BLACK);
	GrEllipse(mines_wid, mines_gc, x_centre, y_centre,
	          mf->sq_size / 3, mf->sq_size / 3);
	colour = PHOTO ? LTGREEN : LTGRAY;
	GrSetGCForeground(mines_gc, colour);
	GrFillEllipse(mines_wid, mines_gc, x_centre, y_centre,
                  mf->sq_size / 6, mf->sq_size / 6);
	GrSetGCForeground(mines_gc, BLACK);
	GrEllipse(mines_wid, mines_gc, x_centre, y_centre,
	          mf->sq_size / 6, mf->sq_size / 6);
}

static void mines_draw_flag(const int x, const int y)
{
	int indent = (mf->sq_size / 6);
	int pole_x = mf->x_offset + (x * mf->sq_size) + indent;
	int pole_top_y = mf->y_offset + (y * mf->sq_size) + indent;
	int pole_bottom_y = pole_top_y + (mf->sq_size - (2 * indent));
	GR_COLOR colour;

	if (y == mf->height) { //status draw
		pole_top_y += ((mines_info.height - (mf->y_offset +
		         (mf->height * mf->sq_size))) / 2) - (mf->sq_size / 2);
		pole_bottom_y += ((mines_info.height - (mf->y_offset +
		         (mf->height * mf->sq_size))) / 2) - (mf->sq_size / 2);
	}
	
	GrSetGCForeground(mines_gc, WHITE);
	mines_draw_clear(x, y);
	colour = PHOTO ? LTBLUE : LTGRAY;
	GrSetGCForeground(mines_gc, colour);
	GrFillRect(mines_wid, mines_gc, pole_x, pole_top_y,
	           (4 * indent), (3 * indent));
	GrSetGCForeground(mines_gc, BLACK);
	GrRect(mines_wid, mines_gc, pole_x, pole_top_y, (4 * indent), (3 * indent));
	GrLine(mines_wid, mines_gc, pole_x, pole_top_y, pole_x, pole_bottom_y);
}

static void mines_draw_number(const int x, const int y, const int number)
{
	GR_SIZE width, height, base;
	char buf[3];
	int y_off = 0;
	
	if (y == mf->height) { //status draw
		y_off = ((mines_info.height - (mf->y_offset +
		         (mf->height * mf->sq_size))) / 2) - (mf->sq_size / 2);
	}
	
	snprintf(buf, 3, "%d", number);
	GrGetGCTextSize(mines_gc, buf, -1, GR_TFASCII|GR_TFTOP,
	                &width, &height, &base);
	GrSetGCForeground(mines_gc, WHITE);
	mines_draw_clear(x, y);
	GrSetGCForeground(mines_gc, BLACK);
	GrText(mines_wid, mines_gc,
           mf->x_offset + (x * mf->sq_size) + ((mf->sq_size - width) / 2),
           mf->y_offset + (y * mf->sq_size) + ((mf->sq_size - height) / 2)
           + y_off, buf, -1, GR_TFASCII|GR_TFTOP);
}

static void mines_draw_all_mines(void)
{
	int x, y;
   
	for (x = 0; x < mf->width; x++) {
		for (y = 0; y < mf->height; y++) {
			if (mf->grid[x][y].mines == MINED)
				mines_draw_mine(x, y);
		}
	}
}

static void mines_shift_selection(int direction)
{
	static int tmp_sel_x = -1, tmp_sel_y = -1, last_dir = 0;
	static int xorlast = 1;
	
	if (tmp_sel_x == -1 && tmp_sel_y == -1) {
		tmp_sel_x = mf->sel_x;
		tmp_sel_y = mf->sel_y;
	}
	
	if (!direction) {
		xorlast = 0;
		if (last_dir)
			direction = last_dir;
		else
			direction = 1;
	}
	
	last_dir = direction;
	
	if (mf->sel_x + direction < 0) {
		mf->sel_x = mf->width - 1;
		if (mf->sel_y == 0)
			mf->sel_y = mf->height - 1;
		else
			mf->sel_y = mf->sel_y - 1;
	} else if (mf->sel_x + direction >= mf->width) {
		mf->sel_x = 0;
		if (mf->sel_y == mf->height - 1)
			mf->sel_y = 0;
		else
			mf->sel_y = mf->sel_y + 1;
	} else {
		mf->sel_x = mf->sel_x + direction;
		mf->sel_y = mf->sel_y;
	}
	
	if (mf->grid[mf->sel_x][mf->sel_y].flags & UNCOVERED) {
		mines_shift_selection(direction);
	} else {
		if (xorlast) {
			mines_draw_xor(tmp_sel_x, tmp_sel_y);
		} else {
			xorlast = 1;
		}
		tmp_sel_x = -1;
		tmp_sel_y = -1;
		mines_draw_xor(mf->sel_x, mf->sel_y);
	}
}

static void mines_draw_status(void)
{
	mines_draw_flag(0, mf->height);
	mines_draw_number(1, mf->height, mf->mines - mf->flags_used);
}

static void mines_redraw(void)
{
	int x, y;
	
	GrClearWindow(mines_wid, GR_FALSE);
	mines_draw_grid();
	for (x = 0; x < mf->width; x++) {
		for (y = 0; y < mf->height; y++) {
			if (mf->grid[x][y].flags & UNCOVERED) {
				if (mf->grid[x][y].mines > 0) {
					mines_draw_number(x, y, mf->grid[x][y].mines);
				} else {
					GrSetGCForeground(mines_gc, WHITE);
					mines_draw_clear(x, y);
				}
			} else if (mf->grid[x][y].flags & FLAGGED) {
				mines_draw_flag(x, y);
			} else {
				mines_draw_unknown(x, y);
			}
		}
	}
	mines_draw_status();
	mines_draw_xor(mf->sel_x, mf->sel_y);
}

static void mines_do_draw(void)
{
	pz_draw_header(_("Minesweeper"));
	if (state != GAMEWON && state != GAMELOST)
		mines_redraw();
}

static void mines_uncover_boundary(const int x, const int y);


#define UNCOVER_IF_CLEAR(X, Y) \
	if(mf->grid[X][Y].mines == CLEAR && \
	   !(mf->grid[X][Y].flags & FLAGGED) && \
	   !(mf->grid[X][Y].flags & UNCOVERED)) { \
		mines_clear_path(X, Y); \
	}

static void mines_clear_path(const int x, const int y)
{
	mf->grid[x][y].flags |= UNCOVERED;
	GrSetGCForeground(mines_gc, WHITE);
	mines_draw_clear(x, y);
	
	mines_uncover_boundary(x, y);
	
	if (y > 0)
		UNCOVER_IF_CLEAR(x, y - 1)
	if (x < mf->width - 1 && y > 0)
		UNCOVER_IF_CLEAR(x + 1, y - 1)
	if (x < mf->width - 1)
		UNCOVER_IF_CLEAR(x + 1, y)
	if (x < mf->width - 1 && y < mf->height - 1)
		UNCOVER_IF_CLEAR(x + 1, y + 1)
	if (y < mf->height - 1)
		UNCOVER_IF_CLEAR(x, y + 1)
	if (x > 0 && y < mf->height - 1)
		UNCOVER_IF_CLEAR(x - 1, y + 1)
	if (x > 0)
		UNCOVER_IF_CLEAR(x - 1, y)
	if (x > 0 && y > 0)
		UNCOVER_IF_CLEAR(x - 1, y - 1)
}

static int mines_is_high_score(time)
{
	FILE *fd;
	int highscore = 32768, ret = 0;
	
	if ((fd = fopen(HIGHSCORE, "r"))) {
		fread(&highscore, sizeof(highscore), 1, fd);
		fclose(fd);
	}

	if (time < highscore) {
		if ((fd = fopen(HIGHSCORE, "w"))) {
			fwrite(&time, sizeof(time), 1, fd);
			fclose(fd);
		}
		ret = time;
	}
	
	return ret;
}

static void mines_game_over(void)
{
	GR_SIZE width, height, base;
	GR_COLOR colour;
	char winner_text[] = "Winner!";
	char high_score_text[] = "New Record!";
	char buf[32];
	struct timeval end;
	
	gettimeofday(&end, NULL);
	mf->gametime += (end.tv_sec - mf->start.tv_sec);
	
	switch (state) {
		case GAMEWON:
			GrClearWindow(mines_wid, GR_FALSE);
			colour = PHOTO ? RED : BLACK;
			GrSetGCForeground(mines_gc, colour);
			GrGetGCTextSize(mines_gc, winner_text, -1, GR_TFASCII|GR_TFTOP,
			                &width, &height, &base);
			GrText(mines_wid, mines_gc,
			       (mines_info.width / 2) - (width / 2),
			       (mines_info.height / 2) - height - (height / 2),
			       winner_text, -1, GR_TFASCII|GR_TFTOP);
			snprintf(buf, 31, _("Cleared in %ld seconds."),
					mf->gametime);
			GrGetGCTextSize(mines_gc, buf, -1, GR_TFASCII|GR_TFTOP,
			                &width, &height, &base);
			GrText(mines_wid, mines_gc,
			       (mines_info.width / 2) - (width / 2),
			       (mines_info.height / 2) + height - (height / 2),
			       buf, -1, GR_TFASCII|GR_TFTOP);
			if (mines_is_high_score(mf->gametime)) {
				GrGetGCTextSize(mines_gc, high_score_text, -1,
				                GR_TFASCII|GR_TFTOP, &width, &height, &base);
				GrFillRect(mines_wid, mines_gc, 
				           (mines_info.width / 2) - (width / 2) - 2,
				           (mines_info.height / 2) - (3 * height) - 2,
				           width + 4, height + 4);
				GrSetGCForeground(mines_gc, WHITE);
				GrText(mines_wid, mines_gc,
				       (mines_info.width / 2) - (width / 2),
				       (mines_info.height / 2)  - (3 * height),
				       high_score_text, -1, GR_TFASCII|GR_TFTOP);
			}
			break;
		case GAMELOST:
			mines_redraw();
			mines_draw_all_mines();
		break;
	}
}

static void mines_explode(const int x, const int y)
{
	int x_centre = mf->x_offset + (x * mf->sq_size) + (mf->sq_size / 2);
	int y_centre = mf->y_offset + (y * mf->sq_size) + (mf->sq_size / 2);
	int i = 1;
	GR_COLOR colour;

	GrSetGCForeground(mines_gc, GRAY);
	while (i < (mines_info.width / 2)) {
		if (i % 2) {
			colour = PHOTO ? YELLOW : GRAY;
		} else {
			colour = PHOTO ? RED : BLACK;
		}
		GrSetGCForeground(mines_gc, colour); 
		GrEllipse(mines_wid, mines_gc, x_centre, y_centre, i, i);
		ttk_draw_window (mines_wid);
		ttk_gfx_update (ttk_screen->srf);
		usleep(1000);
		i++;
	}
}

static int mines_uncover(const int x, const int y)
{
	int ret = 0;
	
	if (mf->grid[x][y].flags & FLAGGED) {
		ret = 0;
	} else {
		switch (mf->grid[x][y].mines) {
		case MINED:
			mf->grid[x][y].flags |= UNCOVERED; /* unneeded */
			state = GAMELOST;
			mines_explode(x, y);
			mines_game_over();
			ret = 0;
			break;
		case CLEAR:
			mines_clear_path(x, y);
			ret = 1;
			break;
		default:
			mf->grid[x][y].flags |= UNCOVERED;
			mines_draw_number(x, y, mf->grid[x][y].mines);
			ret = 1;
			break;
		}
	}
	
	return ret;
}

static void mines_put_flag(const int x, const int y)
{
	if (mf->grid[x][y].flags & FLAGGED) { /* already flagged */
		mf->flags_used--;
		if (mf->grid[x][y].mines == MINED)
			mf->correct_flags--;
		mf->grid[x][y].flags ^= FLAGGED;
		mines_draw_unknown(x, y);
		mines_draw_xor(x, y);
	} else if (mf->flags_used < mf->mines) { /* lay a flag */
		mf->flags_used++;
		if (mf->grid[x][y].mines == MINED)
			mf->correct_flags++;
		mf->grid[x][y].flags ^= FLAGGED;
		mines_draw_flag(x, y);
		mines_draw_xor(x, y);
	}
	
	mines_draw_status();
	
	if (mf->correct_flags == mf->mines) {
		state = GAMEWON;
		mines_game_over();
	}
}

#define UNCOVER_IF_NUMBER(X, Y) \
	if(mf->grid[X][Y].mines != MINED && \
	   mf->grid[X][Y].mines != CLEAR && \
	   !(mf->grid[X][Y].flags & FLAGGED) && \
	   !(mf->grid[X][Y].flags & UNCOVERED)) { \
		mines_uncover(X, Y); \
	}

static void mines_uncover_boundary(const int x, const int y)
{
	if (y > 0)
		UNCOVER_IF_NUMBER(x, y - 1)
	if (x < mf->width - 1 && y > 0)
		UNCOVER_IF_NUMBER(x + 1, y - 1)
	if (x < mf->width - 1)
		UNCOVER_IF_NUMBER(x + 1, y)
	if (x < mf->width - 1 && y < mf->height - 1)
		UNCOVER_IF_NUMBER(x + 1, y + 1)
	if (y < mf->height - 1)
		UNCOVER_IF_NUMBER(x, y + 1)
	if (x > 0 && y < mf->height - 1)
		UNCOVER_IF_NUMBER(x - 1, y + 1)
	if (x > 0)
		UNCOVER_IF_NUMBER(x - 1, y)
	if (x > 0 && y > 0)
		UNCOVER_IF_NUMBER(x - 1, y - 1)
}

static void mines_toggle_pause(void)
{
	struct timeval now;
	
	if (state == PAUSED) {
		gettimeofday(&mf->start, NULL);
		mines_redraw();
		state = RUNNING;
	} else if (state == RUNNING) {
		GR_SIZE width, height, base;
		char pause_text[] = "Paused";
		
		state = PAUSED;
		gettimeofday(&now, NULL);
		mf->gametime += (now.tv_sec - mf->start.tv_sec);
		
		GrClearWindow(mines_wid, GR_FALSE);
		GrGetGCTextSize(mines_gc, pause_text, -1, GR_TFASCII|GR_TFTOP,
		                &width, &height, &base);
		GrSetGCForeground(mines_gc, BLACK);
		GrText(mines_wid, mines_gc,
		       (mines_info.width / 2) - (width / 2),
		       (mines_info.height / 2) - (height / 2),
		       pause_text, -1, GR_TFASCII|GR_TFTOP);
	}
}

static int mines_handle_event(GR_EVENT * event)
{
	int ret = 0;
	
	switch (state) {
	case RUNNING:
		switch (event->type) {
			case GR_EVENT_TYPE_KEY_DOWN:
			switch (event->keystroke.ch) {
				case 'h': /* hold */
					mines_toggle_pause();
					break;
				case 'd':
					mines_put_flag(mf->sel_x, mf->sel_y);
					break;
				case '\r':
				case '\n': /* action */
					if(mines_uncover(mf->sel_x, mf->sel_y))
						mines_shift_selection(0);
					ret |= KEY_CLICK;
					break;
				case 'l': /* CCW spin */
					mines_shift_selection(-1);
					ret |= KEY_CLICK;
					break;
				case 'r': /* CW spin */
					mines_shift_selection(1);
					ret |= KEY_CLICK;
					break;
				case 'm':
					mines_exit();
					ret |= KEY_CLICK;
					break;
				default:
					ret |= KEY_UNUSED;
					break;
			}
			break;
		default:
			ret |= EVENT_UNUSED;
			break;
		}
		break;
	case PAUSED:
		switch (event->type) {
			case GR_EVENT_TYPE_KEY_UP:
				if (event->keystroke.ch == 'h')
					mines_toggle_pause();
				break;
			default:
				ret |= EVENT_UNUSED;
				break;
		}
	case GAMELOST:
	case GAMEWON:
		switch (event->type) {
			case GR_EVENT_TYPE_KEY_DOWN:
				switch (event->keystroke.ch) {
					case 'm':
					case '\r':
					case '\n': /* action */
						mines_exit();
					break;
				}
				break;
			default:
				ret |= EVENT_UNUSED;
				break;
		}
	default:
		ret |= EVENT_UNUSED;
		break;
	}
	return ret;
}

void new_mines_window( void )
{
	mines_gc = pz_get_gc(1);
		
	GrSetGCUseBackground(mines_gc, GR_FALSE);

	mines_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
	                          screen_info.rows - (HEADER_TOPLINE + 1), 
	                          mines_do_draw, mines_handle_event);

	GrSelectEvents(mines_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP
	                          |GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(mines_wid);
	
	mines_init_vars();
}

