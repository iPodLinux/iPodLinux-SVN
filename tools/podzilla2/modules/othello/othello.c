/*
 * Copyright (C) 2004 Courtney Cavin
 * Based on original code by David Weekly
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
#include <math.h>

#include "pz.h"

#if 0
extern void new_browser_window(void);
extern void toggle_backlight(void);

static GR_WINDOW_ID oth_wid;
int xlocal,ylocal,lastxlocal,lastylocal;

static void oth_set_piece(int pos, int coloresq);

static int current_oth_item = 19;
static int last_current_oth_item;
static int status[64];
static int testb[64];
static int over = 0;

static int ogs, oxoff, oyoff;

static void othello_draw(PzWidget *wid, ttk_surface srf)
{
	int i;
	GrSetGCForeground(oth_gc, BLACK);
	for (i = 0; i <= 8; i++) {
		ttk_line(srf, oxoff, oyoff+(ogs*i), oxoff+(ogs*8),
				oyoff+(ogs*i), ttk_ap_getx("window.fg")->color);
		ttk_line(srf, oxoff+(ogs*i), oyoff, oxoff+(ogs*i),
				oyoff+(ogs*8), ttk_ap_getx("window.fg")->color);
	}

	if(current_oth_item < 0)
		current_oth_item = (ogs*8)-1;
	else if(current_oth_item >= ogs*8)
		current_oth_item = 0;
	xlocal=current_oth_item * ogs + oxoff - (ogs*8)*(int)(current_oth_item/8);
	ylocal=oyoff+ogs*(int)(current_oth_item/8);
	lastxlocal=last_current_oth_item * ogs + oxoff
	            - (ogs*8)*(int)(last_current_oth_item/8);
	lastylocal=oyoff+ogs*(int)(last_current_oth_item/8);
	ttk_rect(srf, xlocal+1, ylocal+1, xlocal+ogs, ylocal+ogs,
			ttk_ap_getx("window.fg")->color);

	if (current_oth_item != last_current_oth_item) {
		ttk_rect(srf, lastxlocal+1, lastylocal+1,
				lastxlocal+ogs, lastxlocal+ogs,
				ttk_ap_getx("window.fg")->color);
		if (status[last_current_oth_item] != 3)
			oth_set_piece(last_current_oth_item,
					status[last_current_oth_item]);
	}
}

static void oth_set_piece(int pos, int coloresq)
{
	GR_POINT cheese[] = {
		{pos*ogs +(oxoff+1) -(ogs*8)*(int)(pos/8), 
		  (oyoff+(ogs/2)) +ogs*(int)(pos/8)},
		{pos*ogs +(oxoff+(ogs/2)) -(ogs*8)*(int)(pos/8),
		  (oyoff+1) +ogs*(int)(pos/8)},
		{pos*ogs +(oxoff+(ogs-1)) -(ogs*8)*(int)(pos/8),
		  (oyoff+(ogs/2)) +ogs*(int)(pos/8)},
		{pos*ogs +(oxoff+(ogs/2)) -(ogs*8)*(int)(pos/8),
		  (oyoff+(ogs-1)) +ogs*(int)(pos/8)},
		{pos*ogs +(oxoff+1) -(ogs*8)*(int)(pos/8),
		  (oyoff+(ogs/2)) +ogs*(int)(pos/8)}
	};
	status[pos] = coloresq;
	GrSetGCForeground(oth_gc, BLACK);
	if (coloresq==0)
		GrFillPoly(oth_wid, oth_gc, 5, cheese);
	else if(coloresq==1) {
		GrSetGCForeground(oth_gc, WHITE);
		GrFillPoly(oth_wid, oth_gc, 5, cheese);
		GrSetGCForeground(oth_gc, BLACK);
		GrPoly(oth_wid, oth_gc, 5, cheese);
	}
}

static int endgame(int isOver)
{
	int i;
	int human=0,computer=0;
	int s;
	char *lose[5] = {
		"That was really sad...\0",
		"I can\'t believe you lost!\0",
		"Hah, taught you a lesson!\0",
		"Can\'t handle it can you?\0",
		"At least make an effort...\0"
	};
	char *win[5] = {
		"Good Job!\0",
		"You won!\0",
		"Congrats\0",
		"Way to go!\0",
		"Woo Hoo!\0"
	};
	char *tie[5] = {
		"A tie? Strange.\0",
		"Nice Tie!\0",
		"Tieing isnt winning...\0",
		"Want some coke with that tie?\0",
		"A Tie huh?\0"
	};
	char comp[8];
	char hum[8];
	srand(current_oth_item*last_current_oth_item);
	s = rand() % 5;
	for(i = 0; i < 64; i++) {
		switch(status[i]) {
			case 3:
				if(!isOver)
					return 0;
				break;
			case 1:
				computer++;
				break;
			case 0:
				human++;
				break;
		}
	}
	GrSetGCForeground(oth_gc, WHITE);
	GrFillRect(oth_wid, oth_gc, oxoff, oyoff, (ogs*8)+1, (ogs*8)+1);
	GrSetGCForeground(oth_gc, BLACK);
	sprintf(comp, "Me: %d", computer);
	sprintf(hum, "You: %d", human);
	GrText(oth_wid, oth_gc, oxoff, oyoff, comp, -1, GR_TFASCII|GR_TFTOP);
	GrText(oth_wid, oth_gc, oxoff, oyoff+18, hum, -1, GR_TFASCII|GR_TFTOP);

	if(human>computer) {
		GrText(oth_wid, oth_gc, oxoff/4, oyoff+40,
		       win[s], -1, GR_TFASCII|GR_TFTOP);
		over=1;

	}
	else if(computer>human) {
		GrText(oth_wid, oth_gc, oxoff/4, oyoff+40,
		       lose[s], -1, GR_TFASCII|GR_TFTOP);
		over=1;
	}
	else {
		GrText(oth_wid, oth_gc, oxoff/4, oyoff+40,
		       tie[s], -1, GR_TFASCII|GR_TFTOP);
		over=1;
	}
	return 1;
}

static int testmove(int xy,char test,int side,int dx,int dy,char execute) {
	int pieces=0;
	int oxy;
	char found_end='N';

	oxy=xy;
	xy+=dx;
	xy+=(dy*8);
	while ((xy < 64) && (xy >= 0) && found_end == 'N') {
		if (status[xy] == side)
			found_end = 'Y';
		else if(status[xy] == 3)
			break;
		else
			pieces++;
		if ((xy % 8) == 0) {
			if(dx == -1)
				break;
		}
		else if ((xy % 8) == 7) {
			if(dx == 1)
				break;
		}
		xy += dx;
		xy += (dy*8);

	}
	if(found_end == 'Y') {
		if(execute == 'Y') {
			while(xy != oxy) {
				xy -= dx;
				xy -= (dy*8);
				if (test != 'Y')
					oth_set_piece(xy, side);
				else
					testb[xy] = side;
			}
		}
		return pieces;
	}
	return 0;
}


static int validmove(int xy,char test,int side,char execute) {
	int opp;
	int pieces=0;

	opp = !side;

	if (status[xy] != 3)
		return 0;

	if (xy % 8) {
		if (xy > 7)
			if (status[xy-9] == opp)
				pieces += testmove(xy,test,side,-1,-1,execute);
		if (status[xy-1] == opp)
			pieces+=testmove(xy,test,side,-1,0,execute);
		if (xy < 56)
			if (status[xy+7] == opp)
				pieces += testmove(xy,test,side,-1,1,execute);
	}
	if (xy > 7)
		if (status[xy-8] == opp)
			pieces += testmove(xy,test,side,0,-1,execute);
	if (xy < 56)
		if(status[xy+8]==opp)
			pieces+=testmove(xy,test,side,0,1,execute);
	if ((xy % 8) != 7) {
		if (xy > 7)
			if (status[xy-7] == opp)
				pieces += testmove(xy,test,side,1,-1,execute);
		if (status[xy+1] == opp)
			pieces += testmove(xy,test,side,1,0,execute);
		if (xy < 56)
			if (status[xy+9] == opp)
				pieces += testmove(xy,test,side,1,1,execute);
	}
	return pieces;
}

static int canmove(int side) {
	int i;
	for (i=0; i < 64; i++)
		if (status[i] == 3)
			if (validmove(i,'N',side,'N'))
				return 1;
	return 0;
}
static PzWindow *module;


static float movevalue(int xy,int side,int depth) {
	int opp;
	int i,pieces,maxpieces=-500,nmoves=0;
	int oxy;
	float value;

	opp = !side;

	/* copy the board */
	memcpy(&testb, &status, 64 * sizeof(int));

	/* play the space */
	value = (float)validmove(xy,'Y',side,'Y');
	if (xy == 0 || xy == 7 || xy == 56 || xy == 63)
		value += 7;
	else if ((xy % 8) == 0 || (xy % 8) == 7)
		value += 3;

	/* assume an immediately optimal opponent and find best move */
	for (i=0;i<64;i++) {
		if (status[i] == 3) {
			if ((pieces = validmove(xy,'Y',opp,'N')) > 0) {
				if (i == 0 || i == 7 || i == 56 || i == 63)
					pieces += 7;
				else if ((i % 8) == 0 || (i % 8) == 7)
					pieces += 3;
				if (pieces > maxpieces) {
					maxpieces = pieces;
					oxy = i;
				}
				nmoves++;
			}
		}
	}

	if (nmoves)
		value -= (float)(validmove(oxy,'Y',opp,'Y')-1);
	else {
		value += 1;
	}
	return value;
}

static void computermove(int side) {
	int i,mxy;
	int pieces;
	float value,maxvalue=-1000000;
	for (i = 0; i < 64; i++) {
		if (status[i] == 3) {
			if ((pieces = validmove(i,'N',side,'N')) > 0) {
				value = movevalue(i,side,1);
				if (value > maxvalue) {
					maxvalue=value;
					mxy=i;
				}
			}
		}
	}
	pieces=validmove(mxy,'N',side,'Y');
}

static int othello_event(PzEvent *e)
{
	int ret = 0;
	switch (event->type) {
	case PZ_EVENT_BUTTON_DOWN:
		/*global keystrokes*/
		switch (e->arg) {
		case PZ_BUTTON_MENU:
			pz_close_window(e->wid->win);
			return 0;
		}
		if (!over) break;
		if (canmove(0)) {
			/*keystrokes during gameplay*/
			switch (e->arg) {
			case PZ_BUTTON_ACTION:
				if (validmove(current_oth_item,'N',0,'Y') > 0) {
					if(!over)
						computermove(1);
				}
				break;
		}
		else if (canmove(1)) {
			computermove(1);
			draw_oth();
		}
		else
			endgame(1);
		break;
	case PZ_EVENT_SCROLL:
		last_current_oth_item = current_oth_item;
		do {
			current_oth_item+= e->arg;
			WRAP(current_oth_item, 0, 63);
		} while (validmove(current_oth_item,'N',0,'N') == 0);
		e->wid->dirty = 1;
		break;
	default:
		ret |= TTK_EV_UNUSED;
		break;
	}
	return ret;
}

static PzWindow *new_oth_window()
{
	PzWindow *ret;
	int i;

	ogs = (int)(screen_info.cols/13);
	oxoff = ((screen_info.cols-(ogs*8))/2);
	oyoff = (((screen_info.rows-(HEADER_TOPLINE + 1))-(ogs*8))/2);

	for (i = 0; i < 64; i++)
		status[i] = 3;
	over = 0;
	current_oth_item = 19;

	oth_set_piece(27, 1);
	oth_set_piece(28, 0);
	oth_set_piece(35, 0);
	oth_set_piece(36, 1);

	ret = pz_new_window(_("Othello"), PZ_WINDOW_NORMAL);
	
	pz_add_widget(ret, othello_draw, othello_event)->dirty = 1;
	return pz_finish_window(ret);
}

static void init_othello()
{
	module = pz_register_module("othello", NULL);
	pz_menu_add_action("/Extras/Games/Othello", new_othello_window);
}

PZ_MOD_INIT(init_othello)

//#else

#define CORNER 7
#define EDGE 3

static PzModule *module;

static uint64_t board;
static uint64_t b_set;
static char cur_bit;
/* board is a 64 bit playing space
 * b_set is a 64 bit field of set pieces
 *
 * board: 01001
 * b_set: 11011
 * white: 10010 or board ^ b_set  (XOR)
 * black: 01001 or board & b_set  (AND) */

static void print_board(uint64_t *bo, uint64_t *bs)
{
	/*XXX*/
}

static void set_piece(uint64_t *bo, uint64_t *bs, char side, char bit)
{
	switch (side) {
	case S_BLACK:
		*bo |= 1 << bit;
		break;
	case S_WHITE:
		*bo &= ~(1 << bit)
		break;
	}
	*bs |= 1 << bit;
}

static void mask_direction(uint64_t *bo, uint64_t *bs, char side, char bit,
		char dx, char dy, uint64_t *mask)
{
	char xm = 0, ym = 0;
	if (dx != 0)
	
}

static uint64_t do_piece_math(uint64_t *bo, uint64_t *bs, char side, char bit)
{
	uint64_t mask = 0;
	char c;
	char dx = 0, dy = 0;

	/* already a piece there */
	if (*bs & (1 << bit))
		return 0;

	if ((bit % 8) == 0) dx++; /* left side */
	if ((bit % 8) == 7) dx--; /* right side */
	if (bit < 8) dy++; /* top */
	if (bit > 55) dy--; /* bottom */

	if (dx >= 0) mask_direction(bo, bs, side, bit, 1,0, &mask);
	if (dx <= 0) mask_direction(bo, bs, side, bit, -1,0, &mask);
	if (dy >= 0) mask_direction(bo, bs, side, bit, 0,1, &mask);
	if (dy <= 0) mask_direction(bo, bs, side, bit, 0,-1, &mask);
	if (dx >= 0 && dy >= 0) mask_direction(bo, bs, side, bit, 1,1, &mask);
	if (dx >= 0 && dy <= 0) mask_direction(bo, bs, side, bit, 1,-1, &mask);
	if (dx <= 0 && dy >= 0) mask_direction(bo, bs, side, bit, -1,1, &mask);
	if (dx <= 0 && dy <= 0) mask_direction(bo, bs, side, bit, -1,-1, &mask);

	return mask;
}

static int calculate_pointage(uint64_t *bo, uint64_t *bs, char side,
		uint64_t *mask)
{
	/*XXX*/
	return points;
}

static int move_piece(uint64_t *bo, uint64_t *bs, char side, char bit)
{
	uint64_t mask;
	int points; 

	mask = do_piece_math(bo, bs, side, bit);
	points = calculate_pointage(bo, bs, side, &mask);
	if (side == S_BLACK)
		*bo |= mask;
	else
		*bo &= ~mask;
	*bs |= mask;

	return points;
}

static char best_move(uint64_t *bo, uint64_t *bs, char side, int *points)
{
	uint64_t t_bo, t_bs;
	unsigned char c;
	int points, tp1, tp2;

	for (c = 0; c < 64; c++) {
		if (!valid_move(bo, bs, c, side))
			continue;
		memcpy(&t_bo, bo, sizeof(uint64_t));
		memcpy(&t_bs, bs, sizeof(uint64_t));
		tp1 = move_piece(&t_bo, &t_bs, side);
	}
	return -1; /* no valid moves */
}

static void computer_move()
{
	int i;
}

#define TWRAP(x,y,z) if (x > z) x = y + (x - z)
#define BWRAP(x,y,z) if (x < y) x = z - (y - x)
#define WRAP(x,y,z) TWRAP(x,y,z); BWRAP(x,y,z)

static int othello_event(PzEvent *e)
{
	int ret = 0;
	switch (e->type) {
	case PZ_EVENT_BUTTON:
		switch (e->arg) {
		case PZ_BUTTON_ACTION:
			move_piece(&board, cur_bit);
			break;
		case PZ_BUTTON_MENU:
			pz_close_window(e->wid->win);
			break;
		default:
			ret |= TTK_EV_UNUSED;
			break;
		}
		break;
	case PZ_EVENT_SCROLL:
		cur_bit += e->arg;
		WRAP(cur_bit, 0, 63);
		break;
	default:
		ret |= TTK_EV_UNUSED;
		break;
	}
	return ret;
}

static PzWindow *new_othello_window()
{
	PzWindow *ret;

	cur_bit = 19;
	set_piece(&board, &b_set, S_BLACK, 27);
	set_piece(&board, &b_set, S_BLACK, 36);
	set_piece(&board, &b_set, S_WHITE, 28);
	set_piece(&board, &b_set, S_WHITE, 35);

	ret = pz_new_window(_("Othello"), PZ_WINDOW_NORMAL);

	pz_add_widget(ret, othello_draw, othello_event)->dirty = 1;
	return pz_finish_window(ret);
}

static void init_othello()
{
	module = pz_register_module("othello", NULL);
	pz_menu_add_action("/Extras/Games/Othello", new_othello_window);
}

PZ_MOD_INIT(init_othello)
#endif
