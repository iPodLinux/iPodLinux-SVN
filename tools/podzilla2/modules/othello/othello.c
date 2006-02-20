/*
 * Copyright (C) 2005 Courtney Cavin
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
#include <string.h>
#include <stdint.h>

#include "pz.h"

#define NOMOVE -64

typedef enum _State {
	WAITING, THINKING, NO_MOVE, GAME_OVER
} State;

static signed char weights[] = {
	 8,-2, 5, 4, 4, 5,-2, 8,
	-2,-2, 0, 0, 0, 0,-2,-2,
	 5, 0, 1, 1, 1, 1, 0, 5,
	 4, 0, 1, 1, 1, 1, 0, 4,
	 4, 0, 1, 1, 1, 1, 0, 4,
	 5, 0, 1, 1, 1, 1, 0, 5,
	-2,-2, 0, 0, 0, 0,-2,-2,
	 8,-2, 5, 4, 4, 5,-2, 8
};

#define S_BLACK 1
#define S_WHITE 0

static PzModule *module;

static State othello_state;
static uint64_t board;
static uint64_t b_set;
static signed char cur_bit;
/* board is a 64 bit playing space
 * b_set is a 64 bit field of set pieces
 *
 * board: 01001
 * b_set: 11011
 * white: 10010 or board ^ b_set  (XOR)
 * black: 01001 or board & b_set  (AND) */


#define PC_SZ (wid->h / 10)
#define PC_PAD 3
#define BO_YS ((wid->h - (PC_SZ+PC_PAD)*8)/2)
#define BO_XS ((wid->w - (PC_SZ+PC_PAD)*8)/2)
static void othello_draw(PzWidget *wid, ttk_surface srf)
{
	unsigned char xline, yline, c;
	int coordx, coordy;
	ttk_color fg = ttk_ap_getx("window.fg")->color;

	yline = 0;
	for (c = 0; c < 64; c++) {
		xline = c % 8;
		coordx = xline*(PC_SZ+PC_PAD) + PC_PAD/2 + BO_XS;
		coordy = yline*(PC_SZ+PC_PAD) + PC_PAD/2 + BO_YS;
		if (b_set & (1ULL << c)) {

			if (board & (1ULL << c)) {
				ttk_fillellipse(srf, coordx + PC_SZ/2,
						coordy + PC_SZ/2, PC_SZ/2-1,
						PC_SZ/2, fg);
			}
			ttk_aaellipse(srf, coordx + PC_SZ/2,
					coordy + PC_SZ/2, PC_SZ/2,
					PC_SZ/2, fg);
		}
		else {
			ttk_pixel(srf, coordx + PC_SZ/2,
					coordy + PC_SZ/2, fg);
		}
		if (cur_bit == c) {
			ttk_aaellipse(srf, coordx + PC_SZ/2,
					coordy + PC_SZ/2, PC_SZ/2, PC_SZ/2,
					ttk_ap_getx("scroll.bar")->color);
			ttk_aaellipse(srf, coordx + PC_SZ/2,
					coordy + PC_SZ/2, PC_SZ/4, PC_SZ/4,
					ttk_ap_getx("scroll.bar")->color);
		}
		if (xline == 7) yline++;
	}
	/*XXX*/
}

static void set_piece(uint64_t *bo, uint64_t *bs, char side, unsigned char bit)
{
	switch (side) {
	case S_BLACK:
		*bo |= (1ULL << bit);
		break;
	case S_WHITE:
		*bo &= ~(1ULL << bit);
		break;
	}
	*bs |= (1ULL << bit);
}

static void mask_direction(uint64_t *bo, uint64_t *bs, char side, char bit,
		signed char dx, signed char dy, uint64_t *mask)
{
	uint64_t t_mask = 0;
	char iter;
	char step;
	char s = (side == S_BLACK);

	step = dx + dy*8;
	iter = bit + step;

	/* no piece there */
	if (!(*bs & (1ULL << iter)))
		return;
	
	/* piece is on the same side */
	if ((!!(*bo & (1ULL << iter))) == s)
		return;

	do {
		if (!(*bs & (1ULL << iter)))
			return;
		if ((!!(*bo & (1ULL << iter))) == !s)
			t_mask |= (1ULL << iter);
		else if ((!!(*bo & (1ULL << iter))) == s)
			break;
		if ((dx > 0 && (iter % 8) == 7)||(dx < 0 && (iter % 8) == 0) ||
			(dy > 0 && iter > 55)||(dy < 0 && iter < 8))
			return;
		iter += step;
	} while (1);
	*mask |= t_mask | (1ULL << bit);
}

static uint64_t do_piece_math(uint64_t *bo, uint64_t *bs, char side, char bit)
{
	uint64_t mask = 0;
	signed char dx = 0, dy = 0;

	/* already a piece there */
	if (*bs & (1ULL << bit))
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

static int calculate_pointage(char side, uint64_t *mask)
{
	unsigned char c;
	int points = 0;
	char got_corner = 0;

	for (c = 0; c < 64; c++) {
		if (*mask & (1ULL << c)) {
#if 0
			if (c == 0 || c == 7 || c == 56 || c == 63)
				points += CORNER;
			if ((c % 8) == 0 || (c % 8) == 7) 
				points += EDGE;
			else
				points++;
#else
			if (c == 0 || c == 7 || c == 56 || c == 63)
				got_corner++;
			if (weights[c] < 0) {
				if (got_corner)
					continue;
			}
			points += weights[c];
#endif
		}
	}
	return points;
}

static int valid_move(uint64_t *bo, uint64_t *bs, char side, char bit)
{
	uint64_t mask;

	mask = do_piece_math(bo, bs, side, bit);

	return !!(mask);
}

static int move_piece(uint64_t *bo, uint64_t *bs, char side, char bit)
{
	uint64_t mask;
	int points; 

	if (*bs & (1ULL << bit))
		return NOMOVE;

	mask = do_piece_math(bo, bs, side, bit);
	points = calculate_pointage(side, &mask);
	if (side == S_BLACK)
		*bo |= mask;
	else
		*bo &= ~mask;
	*bs |= mask;

	return points;
}

static signed char best_move(uint64_t *bo, uint64_t *bs, char side)
{
	uint64_t t_bo, t_bs;
	unsigned char c, bc = 0;
	int best = NOMOVE;
	int pts;

	for (c = 0; c < 64; c++) {
		if (!valid_move(bo, bs, side, c))
			continue;
		memcpy(&t_bo, bo, sizeof(uint64_t));
		memcpy(&t_bs, bs, sizeof(uint64_t));
		pts = move_piece(&t_bo, &t_bs, side, c);
		if (pts > best) {
			best = pts;
			bc = c;
		}
	}
	return (best != NOMOVE) ? bc : NOMOVE;
}

static int check_nomove()
{
	unsigned char c;
	if (othello_state != NO_MOVE && othello_state != GAME_OVER) return 0;
	for (c = 0; c < 64; c++) {
		if (valid_move(&board, &b_set, S_WHITE, c)) return 0;
		if (valid_move(&board, &b_set, S_BLACK, c)) return 0;
	}
	othello_state = GAME_OVER;
	return 1;
}

#define TWRAP(x,y,z) if (x > z) x = y + (x - z - 1)
#define BWRAP(x,y,z) if (x < y) x = z - (y - x - 1)
#define WRAP(x,y,z) TWRAP(x,y,z); BWRAP(x,y,z)

static int computer_move(TWidget *this)
{
	signed char c;

	if (check_nomove()) return 0;
	this->dirty = 1;

	if ((c = best_move(&board, &b_set, S_WHITE)) != NOMOVE)
		move_piece(&board, &b_set, S_WHITE, c);
	else
		othello_state = NO_MOVE;
	if (best_move(&board, &b_set, S_BLACK) == NOMOVE)
		return 0;

	do {
		cur_bit++;
		WRAP(cur_bit, 0, 63);
	} while (!valid_move(&board, &b_set, S_BLACK, cur_bit));
	othello_state = WAITING;
	ttk_widget_set_timer(this, 0);
	return 0;
}

static int othello_event(PzEvent *e)
{
	int ret = 0;
	switch (e->type) {
	case PZ_EVENT_BUTTON_DOWN:
		switch (e->arg) {
		case PZ_BUTTON_ACTION:
			if (check_nomove() || othello_state != WAITING) break;
			if (move_piece(&board, &b_set, S_BLACK, cur_bit) !=
					NOMOVE) {
				ttk_widget_set_timer(e->wid, 1000); /* ms */
				othello_state = THINKING;
				e->wid->dirty = 1;
			}
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
		if (check_nomove() || othello_state != WAITING) break;
		TTK_SCROLLMOD(e->arg, 5);
		do {
			cur_bit += e->arg;
			WRAP(cur_bit, 0, 63);
		} while (!valid_move(&board, &b_set, S_BLACK, cur_bit));
		e->wid->dirty = 1;
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
	PzWidget *wid;

	board = b_set = 0;
	cur_bit = 19;
	othello_state = WAITING;

	set_piece(&board, &b_set, S_WHITE, 27);
	set_piece(&board, &b_set, S_WHITE, 36);
	set_piece(&board, &b_set, S_BLACK, 28);
	set_piece(&board, &b_set, S_BLACK, 35);

	ret = pz_new_window(_("Othello"), PZ_WINDOW_NORMAL);

	wid = pz_add_widget(ret, othello_draw, othello_event);

	wid->dirty = 1;
	wid->timer = computer_move;

	return pz_finish_window(ret);
}

static void init_othello()
{
	module = pz_register_module("othello", NULL);
	pz_menu_add_action("/Extras/Games/Othello", new_othello_window);
}

PZ_MOD_INIT(init_othello)
