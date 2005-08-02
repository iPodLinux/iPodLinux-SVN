/*
 * Copyright (C) 2005 Filippo Forlani
 * 
 * iPod Bobble for iPod Linux ver. 0.1.0
 *
 * Todo:
 * 
 * History:
 * - started 29-03-05 at 20.35
 * - 30 working collision
 * - 31 can play... no animation... bug? ver.0.0.1
 * - 10 April - ver.0.1.0 podzillabitmap, levels, acceleration wheel
 * - 30 May - added colors and functionality for iPhoto (must test iMini)
 *
 * contact: filfor@tin.it (contact is appreciated)
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

#ifdef DEBUG
#define Dprintf printf
#else
#define Dprintf(...)
#endif

static GR_WINDOW_ID ipobble_wid;
static GR_GC_ID ipobble_gc;
static GR_TIMER_ID ipobble_timer_id;

#define DELTA_TIME  10
/* GAME VARIABLES AND DEFINITIONS */
#define GAME_STATUS_PLAY (int)1
#define GAME_STATUS_END	(int)0
#define BALL_WIDTH	(int)9
#define BALL_RADIUS	(int)4
#define B_LINE_HEIGHT	(int)8 

#define ME_RADIUS	(int)6

#define BCOLS		(int)7
#define BROWS		(int)12

#define BRD_MINX	(int)20 - 1
#define BRD_MAXX	(int)((BCOLS * BALL_WIDTH + BRD_MINX + BALL_RADIUS) + 1)

#define BRD_MAXY	(int)(BROWS+1)*B_LINE_HEIGHT-ME_RADIUS

#define BALLS_N		8

/* global variables */
int ipobble_high_score;
int ipodc=0;

/* static variables; */
static GR_BITMAP balls_bmp[BALLS_N][8] = { 
	{0x3C00, 0x4200, 0x8100, 0x9900, 0x9900, 0x8100, 0x4200, 0x3C00},
	{0x3C00, 0x4E00, 0x8F00, 0x8F00, 0xF100, 0xF100, 0x7200, 0x3C00},
	{0x3C00, 0x3A00, 0xD500, 0xAD00, 0xD500, 0xAB00, 0x5600, 0x3C00},
	{0x3C00, 0x7200, 0xF100, 0xF100, 0xF100, 0xF100, 0x7200, 0x3C00},
	{0x3C00, 0x7E00, 0xF300, 0xF300, 0xFF00, 0xFF00, 0xFE00, 0x3C00},
	{0x3C00, 0x4200, 0x8100, 0x8100, 0xFF00, 0xFF00, 0xFE00, 0x3C00},
	{0x3C00, 0x4200, 0x8100, 0x8100, 0x8100, 0x8100, 0x4200, 0x3C00},
	{0x3C00, 0x4200, 0xA500, 0x9900, 0x9900, 0xA500, 0x4200, 0x3C00}
};

static int	podzilla_bmp_var;
static GR_BITMAP podzilla_bmp[2][11]={
	{0x03C0, 0x07A0, 0x0FF0, 0x07D0, 0x03F0, 0x0780,
		0x0FF0, 0x1F80, 0x3F80, 0x7FD0, 0xFBE0},
	{0x03C0, 0x07A0, 0x0FF0, 0x07D0, 0x03F0, 0x0780,
		0x0F40, 0x1FA0, 0x3F80, 0x7FD0, 0xFBE0}
};
	
GR_COLOR BallColors[] = {
    GR_RGB(255,   0,   0),
    GR_RGB(  0, 128,   0),
    GR_RGB(  0,   0, 255),
    GR_RGB(255,   0, 128),
    GR_RGB(128, 128,   0),
    GR_RGB(128, 128,  64),
    GR_RGB(255, 255, 255),
    GR_RGB( 64, 255,   0)
};

static int board[BCOLS * BROWS];
static int cboard[BCOLS * BROWS];
static int score, level;
static int game_status; /* GAME_STATUS_PLAY or GAME_STATUS_END */
static int onetime;

static int fire_counter, fire_counter_start; /* counter before auto fire... */

static int brd_ox, brd_oy;
static int me_posx, me_posy;

static int current_angle;
static int delta_angle;
static int ball_firing;
static int ball_type;
static float ball_x, ball_y;

static int new_ball_x, new_ball_y, new_ball_type;
static int current_ball_x, current_ball_y, current_ball_type;

static int accel, accel_status;
static int counter;

static int level_connection;
static int gameover_waitcounter;

static float ball_vx, ball_vy;

static void draw_first();
static void update_score();

static int new_ball(int level)
{
	if (level + 5 >= BALLS_N)
		return rand() % BALLS_N;
	else
		return rand() % (level + 5);
}

static void ipobble_create_board(int level)
{
	int i, j, irw;
	brd_ox = BRD_MINX + 1;
	brd_oy = 1;
	me_posx = 20 + BCOLS * BALL_WIDTH / 2;
	me_posy = BRD_MAXY;
	current_angle = 0;
	delta_angle = 2;
	ball_firing = 0;
	irw = (level > 8) ? 8 : level;
	for(i = 0; i < BROWS; i++) {
		for(j = 0; j < BCOLS; j++){
#if 0
			if(board[i * BCOLS + j] != 0)
#endif
			if (i < irw)
				board[i * BCOLS + j] = new_ball(level);
			else
				board[i * BCOLS + j] =- 1;
		}
	}
	level_connection = 3; /* for now let's take 3... */
	new_ball_x = 0;
	new_ball_y = me_posy - 2;
	new_ball_type = new_ball(level);
	current_ball_x = me_posx - BALL_RADIUS + 1;
	current_ball_y = me_posy - ME_RADIUS + BALL_RADIUS - 1;
	current_ball_type = new_ball(level);
	update_score();
	if(level > 6)
		fire_counter_start = 4000;
	else
		fire_counter_start = (10000 - level * 1000) / DELTA_TIME;
	podzilla_bmp_var = 0;
	accel = 0;
	accel_status = 0;
	counter = 0;
}

static void draw_ball(int col,int row,int ibmp,int icolor)
{
    if(ipodc == 0 ){
	GrBitmap(ipobble_wid, ipobble_gc, col, row, BALL_WIDTH - 1,
		 BALL_WIDTH - 1, balls_bmp[ibmp]);
    } else {
        if (icolor!=-1)GrSetGCForeground(ipobble_gc, BallColors[icolor]);
	GrBitmap(ipobble_wid, ipobble_gc, col, row, BALL_WIDTH - 1,
		 BALL_WIDTH - 1, balls_bmp[ibmp]);
    }
    if(icolor!=-1)GrSetGCForeground(ipobble_gc, BLACK);
}

static void draw_ballij(int icol,int irow,int itype, int icolor)
{
	draw_ball(brd_ox + icol * BALL_WIDTH + BALL_WIDTH * (irow % 2) / 2,
				brd_oy + irow * B_LINE_HEIGHT, itype, icolor);
}

static void boards_ball_draw(int ibmp)
{
	int i, j;
	int ioff, istep;
	for(i = 0; i < BROWS; i++) {
		ioff = brd_oy + i * B_LINE_HEIGHT;
		istep = BALL_WIDTH * (i % 2) / 2;
		for(j = 0; j < BCOLS; j++){
			if(board[i * BCOLS + j] >= 0)
				draw_ball(brd_ox + j * BALL_WIDTH + istep,ioff,
				    board[i * BCOLS + j], board[i * BCOLS +j]);
		}
	}
	GrSetGCForeground(ipobble_gc, WHITE);
}

static void me_draw(int angle)
{
	int vx,vy;
	angle += 180;
	GrEllipse(ipobble_wid, ipobble_gc, me_posx, me_posy,
			ME_RADIUS, ME_RADIUS);
	/* draw arrow... for now just a line... */
	
	vx = (int)((float)(2.5 * ME_RADIUS) * (float)sin((float)angle /
				180 * 3.1415));
	vy = (int)((float)(2.5 * ME_RADIUS) * (float)cos((float)angle /
				180 * 3.1415));
	GrLine(ipobble_wid, ipobble_gc, me_posx, me_posy, me_posx + vx,
			me_posy + vy);
	
}

static void draw_podzilla(int whichbmp)
{
	GrBitmap(ipobble_wid, ipobble_gc, me_posx - 18, me_posy - 5,
			11, 11, podzilla_bmp[whichbmp]);
	
}

static void draw_first()
{
	GrLine(ipobble_wid,ipobble_gc, BRD_MINX, 0, 
			BRD_MINX, BRD_MAXY+ME_RADIUS);
	GrLine(ipobble_wid,ipobble_gc, BRD_MAXX, 0, 
			BRD_MAXX, BRD_MAXY+ME_RADIUS);
	GrLine(ipobble_wid,ipobble_gc, 0, BRD_MAXY+ME_RADIUS,
			BRD_MAXX, BRD_MAXY+ME_RADIUS);
	update_score();
	
}

static void do_fire()
{
	fire_counter = fire_counter_start;
	ball_firing = 1;
	ball_vx = (float)sin((float)(current_angle + 180.0) / 180.0 * 3.1415);
	ball_vy = (float)cos((float)(current_angle + 180.0) / 180.0 * 3.1415);
	ball_x = current_ball_x;
	ball_y = current_ball_y;
	GrSetGCForeground(ipobble_gc, WHITE);
	draw_ball(new_ball_x, new_ball_y, new_ball_type,-1);
	draw_ball(current_ball_x, current_ball_y, current_ball_type,-1);
	ball_type = current_ball_type;
	new_ball_x = 0;
	new_ball_y = me_posy - 1;
	current_ball_type = new_ball_type;
	counter++;
	if(counter % 30 == 0) level++;
	new_ball_type = new_ball(level);
	GrSetGCForeground(ipobble_gc, BLACK);
	draw_ball(new_ball_x, new_ball_y, new_ball_type, new_ball_type);
	draw_ball(current_ball_x, current_ball_y, current_ball_type,
						 current_ball_type);
}

static int test_collision()
{
	/* the idea, giving x,y i must know irow and icol of board and check
	 * status for now no optimization check through all balls_bmp */
	int i, j;
	int ix, iy;
	int ni, nj;
	int back;
	/* first check if ball is at top */
	if(brd_oy > ball_y + BALL_RADIUS) {
		GrSetGCForeground(ipobble_gc, WHITE);
		draw_ball(ball_x, ball_y, ball_type, -1);
		ni = (int)(((float)ball_y - (float)brd_oy) /
				(float)B_LINE_HEIGHT + 0.5);
		nj = (int)(((float)ball_x - (float)brd_ox -
				(float)((int)(BALL_WIDTH * (ni % 2) / 2))) /
				(float)(BALL_WIDTH) + 0.5);
		board[ni * BCOLS + nj] = ball_type; 
		ball_firing = 0;
		GrSetGCForeground(ipobble_gc, BLACK);
		score += (BROWS - ni) / 3 + 1;
		draw_ballij(nj, ni, ball_type, ball_type);
		update_score();
		return 1;
	}
	/* now test collision with balls */
	for(i = BROWS - 1; i >= 0; i--) {
		for(j = BCOLS-1; j >= 0; j--) {
			ix = brd_ox + j * BALL_WIDTH + BALL_WIDTH * (i % 2) / 2;
			iy=brd_oy+i*B_LINE_HEIGHT;
			if(board[i * BCOLS + j] >= 0
				&& abs(ix - (int)ball_x) <= 2 * BALL_WIDTH
				&& abs(iy - (int)ball_y) <= 2 * B_LINE_HEIGHT) {
				/* first the distance in inside radius */
				if((ball_y - (float)iy) * (ball_y - (float)iy)
						+ (ball_x - (float)ix) *
						(ball_x - (float)ix) <=
						(BALL_WIDTH*BALL_WIDTH)) {
					/* collision detected */
					GrSetGCForeground(ipobble_gc, WHITE);
					draw_ball(ball_x, ball_y, ball_type, -1);
					do {
						ni=(int)(((float)ball_y -
							(float)brd_oy) /
							(float)B_LINE_HEIGHT +
							0.5);
						nj = (int)(((float)ball_x -
							(float)brd_ox -
							(float)((int)
								(BALL_WIDTH *
								 (ni % 2) /
								 2))) /
								(float)
								(BALL_WIDTH) +
								0.5);
						ball_x -= ball_vx;
						ball_y -= ball_vy;
						back = 0;
						if(ni == i && nj == j) back = 1;
						if(back == 0 &&
							(board[ni * BCOLS+nj]
							 != - 1))
							back = 1;
						if(ni>=BROWS-1 || nj>=BCOLS){
							game_status =
								GAME_STATUS_END;
							onetime = 0;
						};
					} while(back);
					board[ni * BCOLS + nj] = ball_type;
					GrSetGCForeground(ipobble_gc, BLACK);
					
					if(game_status==GAME_STATUS_PLAY){
						score+=(BROWS-ni)/4+1;
						draw_ballij(nj, ni, ball_type, ball_type);
						update_score();
					}
					Dprintf("i,j ni,nj %d,%d %d,%d %d\n",
							i, j, ni, nj,
							board[ni * BCOLS + nj]);
					ball_firing = 0;
					
					return 1;
				/* second the ball+vx ball+vy in inside the
				 * balls */
				}
			}
		}
	}
	return 0; 
	
}

static void test_connection(int nconn)
{
	int value,dir;
	int irow,jcol;
	int tpos;
	int cball;
	int balls_found;
	int btype;
	int bc_row[BCOLS*BROWS];
	int bc_col[BCOLS*BROWS];
	int ccol,crow;
	int pscore,nhit;
	/* how to do
	 * for each ball
	 * for now no optimization
	 */
	memset( cboard,0,sizeof(cboard));
	
	for(irow = 0;irow < BROWS; irow++){
		for(jcol = 0; jcol < BCOLS; jcol++){
			value = 0;
			
			btype=board[irow * BCOLS + jcol];
			if(btype == -1 || cboard[irow * BCOLS + jcol] == -1)
				continue;
			
			balls_found = 0;
			bc_row[balls_found] = irow;
			bc_col[balls_found] = jcol;
			
			if(cboard[irow * BCOLS + jcol] == 0){
				balls_found++;
				value++;
			}
			cball = 0;
			
			cboard[irow * BCOLS + jcol] = -1;
			/* now test through all near balls */
			do {
				crow = bc_row[cball];
				ccol = bc_col[cball];
				tpos = crow * BCOLS + ccol;
				if(ccol > 0 && (board[tpos - 1] == btype) &&
						cboard[tpos - 1] == 0){
					/* found a ball connected not yet
					 * calculated */
					value++;
					bc_row[balls_found] = crow;
					bc_col[balls_found] = ccol - 1;
					balls_found++;
					cboard[tpos - 1] = 1;
					Dprintf("1 ");
				}
				if(ccol < (BCOLS - 1) && board[tpos + 1] ==
						btype && cboard[tpos + 1]==0) {
					/* found a ball connected not yet
					 * calculated */
					value++;
					bc_row[balls_found] = crow;
					bc_col[balls_found] = ccol + 1;
					balls_found++;
					cboard[tpos + 1] = 1;
					Dprintf("2 ");
				}
				/* now row up and down, remember pair and not */
				dir=((crow % 2) == 0) ? -1: +1;
				if(dir > 0 && (ccol + dir) > (BCOLS-1))
					dir = 0;
				if(dir < 0 && (ccol+dir) < 0)
					dir = 0;
				
				if(crow > 0 && board[tpos - BCOLS] == btype &&
						cboard[tpos - BCOLS] == 0){
					/* found a ball connected not yet
					 * calculated */
					value++;
					bc_row[balls_found] = crow - 1;
					bc_col[balls_found] = ccol;
					balls_found++;
					cboard[tpos-BCOLS] = 1;
					Dprintf("3 ");
				}
				if(crow > 0 &&
					board[tpos - BCOLS + dir] == btype &&
					cboard[tpos - BCOLS + dir] == 0) {
					/* found a ball connected not yet
					 * calculated */
					value++;
					bc_row[balls_found] = crow - 1;
					bc_col[balls_found] = ccol + dir;
					balls_found++;
					cboard[tpos - BCOLS + dir] = 1;
					Dprintf("4 %d\n",ccol+dir);
				}
				if(crow < BROWS - 2 && board[tpos + BCOLS] ==
					btype && cboard[tpos+BCOLS] == 0) {
					/* found a ball connected not yet
					 * calculated */
					value++;
					bc_row[balls_found] = crow + 1;
					bc_col[balls_found] = ccol;
					balls_found++;
					cboard[tpos+BCOLS] = 1;
					Dprintf("5 ");
				}
				if(crow < BROWS - 2 &&
					board[tpos + BCOLS + dir] == btype &&
					cboard[tpos + BCOLS + dir] == 0) {
					/* found a ball connected not yet
					 * calculated */
					value++;
					bc_row[balls_found] = crow + 1;
					bc_col[balls_found] = ccol + dir;
					balls_found++;
					cboard[tpos + BCOLS + dir] = 1;
					Dprintf("6 %d\n",ccol + dir);
				}
				cball++;
			} while(cball < balls_found);
			pscore = 0;
			/* if value > 3 or 4 ok... let's destroy the same balls
			 * connected! */
			if(value >= level_connection){
				
				GrSetGCForeground(ipobble_gc, WHITE);
				for(cball = 0; cball < balls_found; cball++){
					draw_ballij(bc_col[cball],
							bc_row[cball],btype, -1);
					board[bc_row[cball] *
						BCOLS + bc_col[cball]] = -1;
					pscore += (BROWS - irow + 1);	
				}
				GrSetGCForeground(ipobble_gc, BLACK);
			}
		}
	}
	/* now test balls that are not attached... and falls down
	 * ...ANIMATION TO DO! */
	memset(cboard, 0, sizeof(cboard));
	
	balls_found = 0;
	irow = 0;
	
	for(jcol = 0;jcol < BCOLS; jcol++){
		if(board[jcol] < 0)
			continue;
		if(cboard[jcol] == -1)
			continue;
			
		bc_row[balls_found] = irow;
		bc_col[balls_found] = jcol;
		balls_found++;

		cball = 0;
		cboard[jcol] = -1;
		/* now walk through connected first row balls */
		do {
			crow=bc_row[cball];
			ccol=bc_col[cball];
			Dprintf("crow,ccol :%d,%d\t",crow,ccol);
			tpos=crow*BCOLS+ccol;
			cboard[tpos] = -1;
			
			if(ccol > 0 && (board[tpos - 1] >= 0) &&
					cboard[tpos - 1] == 0) {
				bc_row[balls_found] = crow;
				bc_col[balls_found] = ccol-1;
				balls_found++;
				cboard[tpos-1] = 1;
				Dprintf("7 %d\n",ccol-1);
			}
			if(ccol < (BCOLS - 1) && board[tpos + 1] >= 0 &&
					cboard[tpos + 1] == 0) {
				bc_row[balls_found] = crow;
				bc_col[balls_found] = ccol + 1;
				balls_found++;
				cboard[tpos + 1] = 1;
				Dprintf("8 ");
			}
			
			dir=((crow % 2) == 0) ? -1: +1;
			if(dir > 0 && (ccol + dir) > (BCOLS - 1))
				dir = 0;
			if(dir < 0 && (ccol + dir) < 0)
				dir = 0;
			
			if(crow > 0 && board[tpos-BCOLS] >= 0 &&
					cboard[tpos-BCOLS] == 0) {
				bc_row[balls_found] = crow-1;
				bc_col[balls_found] = ccol;
				balls_found++;
				cboard[tpos-BCOLS] = 1;
				Dprintf("9 ");
			}
			if(crow > 0 && board[tpos-BCOLS+dir] >= 0 &&
					cboard[tpos-BCOLS+dir] == 0){
				bc_row[balls_found] = crow - 1;
				bc_col[balls_found] = ccol + dir;
				balls_found++;
				cboard[tpos - BCOLS + dir] = 1;
				Dprintf("10 %d\n", ccol+dir);
			}
			if(crow <= BROWS - 2 && board[tpos + BCOLS] >= 0 &&
					cboard[tpos + BCOLS] == 0) {
				bc_row[balls_found] = crow + 1;
				bc_col[balls_found] = ccol;
				balls_found++;
				cboard[tpos+BCOLS] = 1;
				Dprintf("11 ");
			}
			if(crow < BROWS - 2 && board[tpos + BCOLS + dir] >= 0
					&& cboard[tpos + BCOLS + dir] == 0){
				bc_row[balls_found] = crow + 1;
				bc_col[balls_found] = ccol + dir;
				balls_found++;
				cboard[tpos+BCOLS+dir] = 1;
				Dprintf("12 ");
			}
			cball++;
		}while(cball<balls_found);
	}
	 /* destroy balls unconnected to the top */
	GrSetGCForeground(ipobble_gc, WHITE);
	
	nhit = 0;
	for(irow = 0; irow < BROWS; irow++) {
		for(jcol = 0; jcol < BCOLS; jcol++) {
			if(cboard[irow * BCOLS + jcol] == 0 &&
					board[irow * BCOLS + jcol] >= 0) {
				Dprintf("\n %d,%d \n",irow,jcol);
				draw_ballij(jcol, irow,
						board[irow * BCOLS + jcol], -1);
				board[irow * BCOLS + jcol] = -1;
				nhit++;
				pscore += (BROWS - irow + 1) / 3 * nhit;
			}
		}
	}
	score+=pscore;
	GrSetGCForeground(ipobble_gc, BLACK);
	update_score();
}

static void ipobble_DrawScene()
{
	char s[24];
	draw_podzilla(podzilla_bmp_var);
	me_draw(current_angle);
	draw_ball(new_ball_x, new_ball_y, new_ball_type, new_ball_type);
	draw_ball(current_ball_x, current_ball_y, current_ball_type,
						     current_ball_type);
	if(ball_firing){
		/* first 'white' the ball */
		GrSetGCForeground(ipobble_gc, WHITE);
		draw_ball(ball_x,ball_y,ball_type, -1);
		/* now test if bumping */
		if((ball_x + (float)BALL_WIDTH) >= (float)BRD_MAXX ||
				(ball_x - 2) <= (float)BRD_MINX)
			ball_vx=-ball_vx;
		ball_x+=ball_vx;
		ball_y+=ball_vy;
		/* now test if attaching */
			/* if yes set flag to test new 4s... */
		GrSetGCForeground(ipobble_gc, BLACK);
		draw_ball(ball_x,ball_y,ball_type, ball_type);
		if(test_collision()==1){
			boards_ball_draw(0);
			test_connection(level_connection);
		}
	} else {
		fire_counter--;
		if(fire_counter * DELTA_TIME % 100 == 0) {
			sprintf(s, "time: %.3d", fire_counter *
					DELTA_TIME / 100);
			GrText(ipobble_wid,ipobble_gc,
			screen_info.cols - 50, screen_info.rows -
			HEADER_TOPLINE - 15, s, -1, GR_TFASCII);
		}
		if(fire_counter <= 0)
			do_fire();
	}
}

static void ipobble_Game_Loop()
{
	ipobble_DrawScene();
}

static void ipobble_do_draw()
{
	pz_draw_header(_("iPobble"));
	ipobble_Game_Loop();
}

static void update_score()
{
	char s[24];
	int lev_pos = 80;
	GrText(ipobble_wid, ipobble_gc, screen_info.cols - 40, 14, "Score:",
			-1, GR_TFASCII);
	sprintf(s,"%.6d",score);
	GrText(ipobble_wid, ipobble_gc, screen_info.cols - 40, 28, s, -1,
			GR_TFASCII);
	if(score > ipobble_high_score)
		ipobble_high_score = score;
	GrText(ipobble_wid, ipobble_gc, screen_info.cols - 40, 52, "HScore:",
			-1, GR_TFASCII);
	sprintf(s,"%.6d",ipobble_high_score);
	GrText(ipobble_wid, ipobble_gc, screen_info.cols - 40, 64, s, -1,
			GR_TFASCII);
		
	sprintf(s, "Lev.%d", level);
	if (screen_info.rows < 120)
		lev_pos = 40;
	GrText(ipobble_wid, ipobble_gc, screen_info.cols - 30, lev_pos, s, -1,
			GR_TFASCII);
}

static int ipobble_handle_event(GR_EVENT *event)
{
	int ret = 0;
	static int paused = 0;
	
	if(game_status) {
		switch(event->type) {
		case ( GR_EVENT_TYPE_TIMER ):
			if (!paused)
				ipobble_Game_Loop();
			break;
		/* if in game there is 1 status: waiting for direction */
		case( GR_EVENT_TYPE_KEY_DOWN ):
			switch(event->keystroke.ch) {
			case '\r': /* push button : do fire! */
				if(ball_firing)
					break;
				do_fire();
				ret |= KEY_CLICK;
				break;

			case 'l':
				GrSetGCForeground(ipobble_gc, WHITE);
				me_draw(current_angle);
				draw_podzilla(podzilla_bmp_var);
				if(accel_status>0)
					accel_status++;
				else
					accel_status=1;
				current_angle = (current_angle >= 80) ? 80 :
					current_angle + delta_angle *
					(accel_status / 5 + 1);
				GrSetGCForeground(ipobble_gc, BLACK);
				podzilla_bmp_var = !podzilla_bmp_var;
				draw_podzilla(podzilla_bmp_var);
				me_draw(current_angle);
				ret |= KEY_CLICK;
				break;

			case 'r':
				GrSetGCForeground(ipobble_gc, WHITE);
				me_draw(current_angle);
				draw_podzilla(podzilla_bmp_var);
				if (accel_status < 0)
					accel_status--;
				else
					accel_status=-1;
				current_angle = (current_angle <= -80) ? -80 :
					current_angle + delta_angle *
					(accel_status / 5 - 1);
				GrSetGCForeground(ipobble_gc, BLACK);
				podzilla_bmp_var = !podzilla_bmp_var;
				draw_podzilla(podzilla_bmp_var);
				me_draw(current_angle);
				ret |= KEY_CLICK;
				break;	

			case 'm':
				/* if Menu Button then destroy all dynamically
				 * created */
				pz_close_window(ipobble_wid);
				GrDestroyTimer (ipobble_timer_id);
				GrDestroyGC(ipobble_gc);
				break;

			case 'h':
				paused = 1;
				break;
				
			default:
				ret |= KEY_UNUSED;
				break;
				
			}
			
			break;

		case( GR_EVENT_TYPE_KEY_UP ):
			if (event->keystroke.ch == 'h')
				paused = 0;	
			break;

		default:
			ret |= EVENT_UNUSED;
			break;
		}
		return ret;
	} /* if game status play */
	
	/* END OF GAME */
	
	if(onetime == 0){

		GrSetGCForeground(ipobble_gc, BLACK);
		GrText(ipobble_wid, ipobble_gc, screen_info.cols / 2 - 36,
				screen_info.rows / 2 - 24, "GAME OVER", -1,
				GR_TFASCII);
		gameover_waitcounter = 30;
	}
	onetime = 1;
	gameover_waitcounter--;
	switch(event->type) {
	case( GR_EVENT_TYPE_KEY_DOWN ):
		switch(event->keystroke.ch) {
		case '\r': /* push button */
			if(gameover_waitcounter <= 0){
				score = 0;
				GrSetGCForeground(ipobble_gc, WHITE);
				GrFillRect(ipobble_wid, ipobble_gc, 0, 0,
						screen_info.cols,
						screen_info.rows);
				GrSetGCForeground(ipobble_gc, BLACK);
				level = 0;
				score = 0;
				ipobble_create_board(level);
				game_status = GAME_STATUS_PLAY;
				draw_first();
				ret |= KEY_CLICK;
			}
			break;
				
		case 'm':
			/* if Menu Button then destroy all dynamically
			 * created */
			pz_close_window(ipobble_wid);
			GrDestroyTimer(ipobble_timer_id);
			GrDestroyGC(ipobble_gc);
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

void new_ipobble_window()
{

	ipobble_gc = pz_get_gc(1);

	GrSetGCUseBackground(ipobble_gc, GR_TRUE);
	GrSetGCForeground(ipobble_gc, BLACK);
	GrSetGCBackground(ipobble_gc, WHITE);

	ipobble_wid = pz_new_window(0, HEADER_TOPLINE + 1,
			screen_info.cols,
			screen_info.rows - HEADER_TOPLINE - 1,
			ipobble_do_draw, ipobble_handle_event);

	GrSelectEvents(ipobble_wid, GR_EVENT_MASK_TIMER |
			GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_KEY_UP |
			GR_EVENT_MASK_KEY_DOWN);
	if( screen_info.bpp > 2) {
	    ipodc=1;
	}
	
	score = 0;
	level = 0;
	game_status = GAME_STATUS_PLAY;
	ipobble_create_board(level);

	ipobble_timer_id = GrCreateTimer(ipobble_wid, DELTA_TIME);

	GrMapWindow(ipobble_wid);
	
	draw_first();
}
