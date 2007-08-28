/*
 * Copyright (C) 2005 Filippo Forlani
 * 
 * Invaders for iPod Linux
 *
 * Todo:
 * - animation for hit
 * - many other things!
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "pz.h"
#include "ipod.h"
#include "vectorfont.h"

#ifdef DEBUG
#define Dprintf printf
#else
#define Dprintf(...)
#endif

static GR_WINDOW_ID invaders_wid;
static GR_WINDOW_ID invaders_score_pix;
static GR_GC_ID invaders_gc;
static GR_TIMER_ID invaders_timer_id;

/* GAME VARIABLES AND DEFINITIONS */
#define ALIENS_COLS	(int)8
#define ALIENS_ROWS	(int)4

#define SC_OFFX		(int)5
#define SC_OFFY		(int)20

#define ALIEN_WIDTH	(int)7
#define ALIEN_HEIGHT	(int)8

#define ALIEN_CELL_WIDTH  (int)(ALIEN_WIDTH+4)
#define ALIEN_CELL_HEIGHT (int)(ALIEN_HEIGHT+4)

#define ME_WIDTH	(int)7
#define ME_HEIGHT	(int)4


#define MYFIRE_HEIGHT	(int)6
#define MYFIRE_WIDTH	(int)1

#define ALIENFIRE_HEIGHT (int)6
#define ALIENFIRE_WIDTH  (int)1


#define GAME_STATUS_END	 0
#define GAME_STATUS_PLAY 1

int ME_MAX_FIRES;
int ALIEN_MAX_FIRES;

static int alien_maxx,alien_minx,alien_maxy,alien_miny;
static int me_firing;
static int alien_firing;
static int cell_minx,cell_miny,cell_maxx,cell_maxy;

static int aliens_total;
static int aliens_left;
static int aliens_slow;
static int aliens_dir;
static int aliens_rows;

static int board_left,board_right,board_top,board_bottom;
static int me_left,me_right;

static int score;
static int high_score = 0;
static int game_status;
static int level;
static int onetime;

static int me_accel_counter, me_accel, me_still;

static int gameover_waitcounter;

typedef struct {
	int posx, posy;
	int dir;
} st_fire;

int alien_status[ALIENS_COLS * 15];
int me;
static int me_posx,me_posy;

st_fire myfire[5];
st_fire alienfire[5];

static int itest;
/* 4 bitmap each.(not used for now all!) */
static GR_BITMAP alien_bmp[4][8] = { 
	{0x0100, 0x3800, 0x5400, 0xFE00, 0x6C00, 0x3800, 0xC600, 0x8200},
	{0x0100, 0x3800, 0x5400, 0xFE00, 0x7C00, 0x3800, 0xC600, 0x2800},
	{0xA100, 0xA800, 0x5A00, 0xFA00, 0xAC00, 0x3A00, 0xF600, 0x0200},
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}
};

static GR_BITMAP me_bmp[4][4] = { 
	{0x0100, 0x3800, 0xFE00, 0xFE00},
	{0x0100, 0x3800, 0x5400, 0xFE00},
	{0xA100, 0x1100, 0x2200, 0x3300},
	{0x0000, 0x0000, 0x0000, 0x0000}
};
	
static GR_BITMAP myfire_bmp[4][6] = { 
	{0x8000, 0x8000, 0x8000, 0x8000},
	{0x0100, 0x3800, 0x5400, 0xFE00},
	{0xA100, 0x1100, 0x2200, 0x3300},
	{0x0000, 0x0000, 0x0000, 0x0000}
};

static GR_BITMAP alienfire_bmp[4][6] = { 
	{0x8000, 0x8000, 0x8000, 0x8000},
	{0x0100, 0x3800, 0x5400, 0xFE00},
	{0xA100, 0x1100, 0x2200, 0x3300},
	{0x0000, 0x0000, 0x0000, 0x0000}
};

static void update_score();
static void draw_first();

static int get_rowcol(int x, int y, int* row, int* col)
{
	if (x<cell_minx || y<cell_miny || x>cell_maxx || y>cell_maxy)
		return 0;
	*col = (x - cell_minx) / ALIEN_CELL_WIDTH;
	*row = (y - cell_miny) / ALIEN_CELL_HEIGHT;
	if (*col >= ALIENS_COLS)
		(*col)--;
	if (*row >= aliens_rows)
		(*row)--;
	Dprintf("y,celly,y-celly,row %d,%d,%d,%d\n",x,cell_minx,y,cell_miny);
	return 1;
}

static void invaders_create_board(int level)
{
	int i;
	for(i=0; i < ALIENS_COLS * aliens_rows; i++)
		alien_status[i] = 1;
	alien_maxx = ALIENS_COLS * ALIEN_CELL_WIDTH + SC_OFFX + level * 2;
	alien_minx = SC_OFFX;
	alien_maxy = aliens_rows * ALIEN_CELL_HEIGHT + SC_OFFY + level * 2;
	alien_miny = SC_OFFY;
	cell_minx = alien_minx;
	cell_miny = alien_miny;
	cell_maxx = alien_maxx;
	cell_maxy = alien_maxy;
	
	me_posx = screen_info.cols / 2;
	me_posy = screen_info.rows - HEADER_TOPLINE - 10;
	
	aliens_total = ALIENS_COLS * aliens_rows;
	aliens_left = aliens_total;
#if 0
	aliens_slow = aliens_left / 4 - level + 4;
#endif
	
	alien_firing = 0;
	me_firing = 0;
	
	aliens_dir = 2;
	
	board_left = 5;
	board_right = screen_info.cols - 20 + ALIEN_CELL_WIDTH;
	board_top = 15;
	board_bottom = screen_info.rows - HEADER_TOPLINE - 10;
	me_left = board_left + 10;
	me_right = board_right - 18;
	
	me_accel_counter = 0;
	me_accel = 0;
	
	ME_MAX_FIRES = 1;
	if (level <= 1)
		ME_MAX_FIRES = 2;
	
	ALIEN_MAX_FIRES = (level + 1) / 2 + 1;
	if (ALIEN_MAX_FIRES > 5)
		ALIEN_MAX_FIRES = 5;
}

static void draw_alien(int col, int row, int ibmp)
{
	GrBitmap(invaders_wid, invaders_gc, cell_minx + col * ALIEN_CELL_WIDTH,
			cell_miny + row * ALIEN_CELL_HEIGHT, ALIEN_WIDTH,
			ALIEN_HEIGHT, alien_bmp[ibmp]);
}

static void aliens_draw(int ibmp)
{
	int i, j, ij, row, col;
	col = cell_minx;
	ij = 0;
	for(i = 0; i < ALIENS_COLS; i++) {
		row = cell_miny; 
		for(j = 0; j < aliens_rows; j++){
			if(alien_status[ij] > 0)
				GrBitmap(invaders_wid, invaders_gc, col, row,
						ALIEN_WIDTH, ALIEN_HEIGHT,
						alien_bmp[ibmp]);
			row += ALIEN_CELL_HEIGHT;
			ij++;
		}
		col += ALIEN_CELL_WIDTH;
	}
}

static void aliens_update_position()
{
	int i, j, ij, row, col;
	
	cell_minx += aliens_dir;
	cell_maxx += aliens_dir;
	
	/* update alien_minx and alien_maxx */
	col = cell_minx;
	ij = 0;
	for(i = 0; i < ALIENS_COLS; i++) {
		row = cell_miny; 
		for(j = 0; j < aliens_rows; j++) {
			if(alien_status[ij] != 0) {
				alien_minx = col;
				i = ALIENS_COLS; /* bad way to exit 2for... */
				break;
			}
			row += ALIEN_CELL_HEIGHT;
			ij++;
		}
		col += ALIEN_CELL_WIDTH;
	}
	
	col = cell_maxx;
	ij = ALIENS_COLS * aliens_rows - 1;
	for(i = ALIENS_COLS - 1; i >= 0; i--) {
		for(j = 0; j < aliens_rows; j++) {
			if(alien_status[ij] != 0) {
				alien_maxx = col + ALIEN_CELL_WIDTH;
				Dprintf("col,row,status %d,%d,%d\n", i, j,
						alien_status[ij]);
				i = 0; /* bad way to exit 2for... */
				break;
			}
			ij--;
		}
		col -= ALIEN_CELL_WIDTH;
	}
	
	if(alien_minx < board_left || alien_maxx > board_right) {
		aliens_dir = (aliens_dir == 2) ? -2 : 2;
		alien_maxx += aliens_dir;
		alien_minx += aliens_dir;
		alien_miny += 2;
		alien_maxy += 2;
		cell_minx += aliens_dir;
		cell_maxx += aliens_dir;
		cell_miny += 2;
		cell_maxy += 2;
		if(cell_maxy >= me_posy - ME_HEIGHT - 2)
			game_status = GAME_STATUS_END;
	}
}

static void me_draw()
{
	GrBitmap(invaders_wid,invaders_gc, me_posx, me_posy, ME_WIDTH,
			ME_HEIGHT, me_bmp[0]);
}

static void fire_draw()
{
	int i;
	for(i = 0; i < me_firing; i++) {
		GrBitmap(invaders_wid, invaders_gc, myfire[i].posx,
				myfire[i].posy, MYFIRE_WIDTH, MYFIRE_HEIGHT,
				myfire_bmp[0]);
	}
	
	for(i = 0; i < alien_firing; i++) {
		Dprintf("alien posx,y %d,%d,%d\n", i, alienfire[i].posx,
			alienfire[i].posy);
		GrBitmap(invaders_wid,invaders_gc,
			alienfire[i].posx, alienfire[i].posy,
			ALIENFIRE_WIDTH, ALIENFIRE_HEIGHT, alienfire_bmp[0]);
	}
}

static void myfire_delete(int i)
{
	int j;
	for(j = me_firing - 1; j > i; j--)
	{
		myfire[j-1].posx = myfire[j].posx;
		myfire[j-1].posy = myfire[j].posy;
		myfire[j-1].dir = myfire[j].dir;
	}
	me_firing--;
}

static void alienfire_delete(int i)
{
	int j;
	for(j = alien_firing - 1; j > i; j--) {
		alienfire[j-1].posx = alienfire[j].posx;
		alienfire[j-1].posy = alienfire[j].posy;
		alienfire[j-1].dir = alienfire[j].dir;
	}
	alien_firing--;
}

static void fire_update()
{
	int i = 0;
	int rowhit, colhit;
	while(i < me_firing) {
		myfire[i].posy += myfire[i].dir;
		/* test if hit something */
		if(get_rowcol(myfire[i].posx, myfire[i].posy,
					&rowhit, &colhit)) {
			if(alien_status[rowhit + colhit * aliens_rows] > 0) {
				/* when hit, fire out, destroy it
				 * (for now not animation) */
				myfire_delete(i);
				GrSetGCForeground(invaders_gc, WHITE);
				draw_alien(colhit,rowhit,itest);
				Dprintf("hit %d,%d,%d\n", colhit,rowhit,itest);
				Dprintf(" %d,%d\n",alien_maxx-alien_minx,
						cell_maxx - cell_minx);
				GrSetGCForeground(invaders_gc, BLACK);	
				alien_status[rowhit + colhit * aliens_rows] = 0;
				score += colhit * 2;
				update_score();
				aliens_left -= 1;
				if(aliens_left <= 0) {
					level++;
					invaders_create_board(level);
					draw_first();
					return;
				}
				continue;
			}
		}
		/* test if out board */
		if(myfire[i].posy < board_top) {
			myfire_delete(i);
			i--;
		}
		i++;
	}
	
	i = 0;
	while(i < alien_firing) {
		alienfire[i].posy += alienfire[i].dir;
		Dprintf("alienfire pos %d\n", alienfire[i].posy);
		/* test if hit me */
		if(alienfire[i].posy < (me_posy + ME_HEIGHT) &&
			alienfire[i].posy >= me_posy &&
			alienfire[i].posx >= me_posx &&
			alienfire[i].posx <= me_posx + ME_WIDTH) {
			/* End of game */
			game_status = GAME_STATUS_END;
			return;
		}
		/* test if out board */
		if(alienfire[i].posy >= board_bottom) {
			alienfire_delete(i);
			i--;
		}
		i++;
	}
}

static void draw_first()
{
	GrLine(invaders_wid,invaders_gc, 0, screen_info.rows-HEADER_TOPLINE-2-4,
		screen_info.cols-1, screen_info.rows-HEADER_TOPLINE-2-4);
	update_score();
	onetime=0;
}

static void invaders_DrawScene()
{
	aliens_slow--;
	if(aliens_slow <= 0) {
		GrSetGCForeground(invaders_gc, WHITE);
		aliens_draw(itest);
		
		itest = (itest != 0) ? 0 : 1;
		aliens_update_position();
		
		GrSetGCForeground(invaders_gc, BLACK);
		aliens_draw(itest);
		
		aliens_slow = aliens_left / 4 - level + 4;
	}
	/* evaluate if fire or not */
	if((rand() % 10) == 0 && alien_firing < ALIEN_MAX_FIRES) {
		/* find cols active and set relative row tostart */
		int cols[ALIENS_COLS];
		int i, j, ncol, hitcol;
		alien_firing++;
		ncol = 0;
		for(i = 0; i < ALIENS_COLS; i++){
			for(j = aliens_rows - 1; j >= 0; j--){
				if(alien_status[i * aliens_rows + j] > 0) {
					cols[ncol++] = j;
					break;
				}
			}
		}
		hitcol=rand() % ncol;
		Dprintf("%d\n", hitcol);
		alienfire[alien_firing - 1].posx = cell_minx + hitcol *
			ALIEN_CELL_WIDTH + ALIEN_CELL_WIDTH / 2;
		alienfire[alien_firing - 1].posy = cell_miny + cols[hitcol] *
			ALIEN_CELL_HEIGHT + ALIEN_CELL_HEIGHT;
		alienfire[alien_firing-1].dir = 1;
	}
	
	GrSetGCForeground(invaders_gc, WHITE);
	me_draw();
	fire_draw();

	fire_update();
	GrSetGCForeground(invaders_gc, BLACK);
	me_draw();
	fire_draw();
}

static void invaders_Game_Loop()
{
	invaders_DrawScene();
}

static void invaders_do_draw()
{
	pz_draw_header(_("Invaders"));

	invaders_Game_Loop();
}

static void update_score()
{
	char s[24];

	GrSetGCForeground(invaders_gc, WHITE);
	GrFillRect(invaders_score_pix, invaders_gc, 0, 0,
			screen_info.cols, 13);
	GrSetGCForeground(invaders_gc, BLACK);
	sprintf(s, "Score %.5d", score);
	vector_render_string(invaders_score_pix, invaders_gc, s, 1, 1, 2, 2);
	if(score > high_score)
		high_score = score;
	if(screen_info.cols < 150)
		sprintf(s, "Top: %.5d", high_score);
	else
		sprintf(s, "HiScore: %.5d", high_score);
	vector_render_string_right(invaders_score_pix, invaders_gc, s, 1, 1,
			screen_info.cols - 2, 2);
	GrCopyArea(invaders_wid, invaders_gc, 0, 0, screen_info.cols, 12,
			invaders_score_pix, 0, 0, 0);
}

static int invaders_handle_event(GR_EVENT *event)
{
	int ret = 0;
	static int paused;
	if(game_status) {
		switch(event->type) {
		case ( GR_EVENT_TYPE_TIMER ):
			if (!paused)
				invaders_Game_Loop();
			break;
		case( GR_EVENT_TYPE_KEY_DOWN ):
			switch(event->keystroke.ch) {
			case '\r': /* push button */
				if(me_firing < ME_MAX_FIRES) {
					me_firing++;
					myfire[me_firing - 1].posx = me_posx;
					myfire[me_firing - 1].posy = me_posy -4;
					myfire[me_firing - 1].dir = -2;
				}
				ret |= KEY_CLICK;
				break;

			case 'l':
				GrSetGCForeground(invaders_gc, WHITE);
				me_draw();
				GrSetGCForeground(invaders_gc, BLACK);

				if(me_still == 'l'){
					me_accel_counter++;
					me_accel = (me_accel_counter < 20) ?
						me_accel_counter / 5 : me_accel;
				} else {
					me_still = 'l';
					me_accel_counter = 0;
					me_accel = 2;
				}
				me_posx = (me_posx <= me_left) ? me_left :
					(me_posx - me_accel);
				me_draw();
				ret |= KEY_CLICK;
				break;
			case 'r':
				GrSetGCForeground(invaders_gc, WHITE);
				me_draw();
				GrSetGCForeground(invaders_gc, BLACK);

				if(me_still == 'r'){
					me_accel_counter++;
					me_accel = (me_accel_counter < 20) ?
						me_accel_counter / 5 : me_accel;
				} else {
					me_still = 'r';
					me_accel_counter = 0;
					me_accel = 2;
				}
				me_posx = (me_posx >= me_right) ? me_right :
					me_posx + me_accel;
				me_draw();
				ret |= KEY_CLICK;
				break;	
			case 'm':
				/* if Menu Button destroy all dynamically
				 * created */
				pz_close_window(invaders_wid);
				GrDestroyWindow(invaders_score_pix);
				GrDestroyTimer (invaders_timer_id);
				GrDestroyGC(invaders_gc);
				break;
			case 'h':
				paused = 1;
				break;
			default:
				me_still = 0; /* for what key? */
				ret |= KEY_UNUSED; /* right? */
				break;
			}
			break;
		case( GR_EVENT_TYPE_KEY_UP ):
			switch(event->keystroke.ch) {
			case 'h': /* push button */
				paused = 0;
				break;
			default:
				ret |= KEY_UNUSED;
			}
			break;
		default:
			ret |= KEY_UNUSED;
		}
		return ret;
	}/* if game status play */
	
	/* END OF GAME */
	if(onetime == 0){
		char game_over[] = "GAME OVER";
		int x = screen_info.cols / 2;
		int y = ((screen_info.rows - 21) / 4) * 3;
		int xp = x - (vector_string_pixel_width(game_over, 1, 2) / 2);
		int yp = y - 8;
		GrSetGCForeground(invaders_gc, WHITE);
		GrFillRect(invaders_wid, invaders_gc, xp - 2, yp - 2,
				(x - xp + 2) * 2, (y - yp + 2) * 2);
		GrSetGCForeground(invaders_gc, BLACK);
		vector_render_string(invaders_wid, invaders_gc,
				game_over, 1, 2,
				xp, yp);
		gameover_waitcounter = 30;
	}
	onetime = 1;
	gameover_waitcounter--;
	switch(event->type) {
	case( GR_EVENT_TYPE_KEY_DOWN ):
		switch(event->keystroke.ch) {
		case '\r': /* push button */
			if(gameover_waitcounter <= 0) {
				score = 0;
				GrSetGCForeground(invaders_gc, WHITE);
				GrFillRect(invaders_wid,invaders_gc,
					0, 0, screen_info.cols,
					screen_info.rows);
				GrSetGCForeground(invaders_gc, BLACK);
				level = 0;
				score = 0;
				invaders_create_board(level);
				game_status=GAME_STATUS_PLAY;
				draw_first();
			}
			ret |= KEY_CLICK;
			break;
				
		case 'm':
			/* if Menu Button then destroy all
			 * dynamically created */
			pz_close_window(invaders_wid);
			GrDestroyWindow(invaders_score_pix);
			GrDestroyTimer(invaders_timer_id);
			GrDestroyGC(invaders_gc);
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
	return ret;
}

void new_invaders_window()
{

	invaders_gc = pz_get_gc(1);

	GrSetGCUseBackground(invaders_gc, GR_TRUE);
	GrSetGCBackground(invaders_gc, WHITE);
	GrSetGCForeground(invaders_gc, BLACK);
	
	invaders_score_pix = GrNewPixmap(screen_info.cols, 13, 0);

	invaders_wid = pz_new_window(0,
				HEADER_TOPLINE + 1,
				screen_info.cols,
				screen_info.rows - HEADER_TOPLINE - 1,
				invaders_do_draw,
				invaders_handle_event);

	GrSelectEvents(invaders_wid, GR_EVENT_MASK_TIMER |
			GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_KEY_UP |
			GR_EVENT_MASK_KEY_DOWN);
	
	score = 0;
	level = 0;
	aliens_rows = (screen_info.rows - 40) / 20;	
	
	game_status = GAME_STATUS_PLAY;
	invaders_create_board(level);

	invaders_timer_id = GrCreateTimer(invaders_wid, 50);

	GrMapWindow(invaders_wid);
	
	draw_first();
}
