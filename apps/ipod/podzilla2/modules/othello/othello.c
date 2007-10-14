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

static struct game_st {
	State state;
	uint64_t board;
	uint64_t b_set;
	signed char cur_bit, nocolor;
	ttk_surface black, white, selection;
} * _othello = 0;

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
static void othello_mono_draw(PzWidget *wid, ttk_surface srf)
{
	unsigned char xline, yline, c;
	int coordx, coordy;
	ttk_color fg = ttk_ap_getx("window.fg")->color;

#ifdef SDL
	ttk_fillrect(srf, 0,0, wid->w, wid->h, ttk_ap_getx("window.bg")->color);
#endif
	yline = 0;
	for (c = 0; c < 64; c++) {
		xline = c % 8;
		coordx = xline*(PC_SZ+PC_PAD) + PC_PAD/2 + BO_XS;
		coordy = yline*(PC_SZ+PC_PAD) + PC_PAD/2 + BO_YS;
		if (_othello->b_set & (1ULL << c)) {

			if (_othello->board & (1ULL << c)) {
				ttk_aafillellipse(srf, coordx + PC_SZ/2,
						coordy + PC_SZ/2, PC_SZ/2,
						PC_SZ/2, fg);
			}
			else {
				ttk_aaellipse(srf, coordx + PC_SZ/2,
						coordy + PC_SZ/2, PC_SZ/2,
						PC_SZ/2, fg);
			}
		}
		else {
			ttk_pixel(srf, coordx + PC_SZ/2,
					coordy + PC_SZ/2, fg);
		}
		if (_othello->cur_bit == c) {
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

void draw_image(ttk_surface img, ttk_surface srf, int x, int y)
{
	int w, h;
	ttk_surface_get_dimen(img, &w, &h);
	ttk_blit_image(img, srf, x - w/2, y - h/2);
}

#define BGCOLOR ttk_makecol(0x0d, 0x77, 0x0b)
static void othello_color_draw(PzWidget *wid, ttk_surface srf)
{
	unsigned char xline, yline, c;
	int x, y, step, startx, endx, size;

#ifdef SDL
	ttk_fillrect(srf, 0,0, wid->w, wid->h, ttk_ap_getx("window.bg")->color);
#endif
	step = (wid->h / 8);
	size = step * 8;
	startx = (wid->w - wid->h) / 2;
	endx = startx + size;
	ttk_fillrect(srf, startx,0, endx, size, BGCOLOR);
	for (x = 0; x < 9; ++x) {
		int mod = (size / 8) * x;
		ttk_line(srf, startx, mod, endx, mod, 0x000000);
		ttk_line(srf, startx + mod, 0, startx + mod, size, 0x000000);
	}
	yline = 0;
	for (c = 0; c < 64; c++) {
		xline = c % 8;
		x = xline * step + step/2 + startx;
		y = yline * step + step/2 + 0;
		if (_othello->b_set & (1ULL << c)) {
			if (_othello->board & (1ULL << c))
				draw_image(_othello->black, srf, x, y);
			else
				draw_image(_othello->white, srf, x, y);
		}
		else if (_othello->cur_bit == c)
			draw_image(_othello->selection, srf, x, y);
		if (xline == 7) yline++;
	}
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
	if (_othello->state != NO_MOVE && _othello->state != GAME_OVER)
		return 0;
	for (c = 0; c < 64; c++) {
		if (valid_move(&_othello->board, &_othello->b_set, S_WHITE, c))
			return 0;
		if (valid_move(&_othello->board, &_othello->b_set, S_BLACK, c))
			return 0;
	}
	_othello->state = GAME_OVER;
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

	if ((c = best_move(&_othello->board, &_othello->b_set, S_WHITE)) != NOMOVE)
		move_piece(&_othello->board, &_othello->b_set, S_WHITE, c);
	else
		_othello->state = NO_MOVE;
	if (best_move(&_othello->board, &_othello->b_set, S_BLACK) == NOMOVE)
		return 0;

	do {
		_othello->cur_bit++;
		WRAP(_othello->cur_bit, 0, 63);
	} while (!valid_move(&_othello->board, &_othello->b_set, S_BLACK, _othello->cur_bit));
	_othello->state = WAITING;
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
			if (check_nomove() || _othello->state != WAITING) break;
			if (move_piece(&_othello->board, &_othello->b_set, S_BLACK, _othello->cur_bit) !=
					NOMOVE) {
				ttk_widget_set_timer(e->wid, 1000); /* ms */
				_othello->state = THINKING;
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
		if (check_nomove() || _othello->state != WAITING) break;
		TTK_SCROLLMOD(e->arg, 5);
		do {
			_othello->cur_bit += e->arg;
			WRAP(_othello->cur_bit, 0, 63);
		} while (!valid_move(&_othello->board, &_othello->b_set, S_BLACK, _othello->cur_bit));
		e->wid->dirty = 1;
		break;
	default:
		ret |= TTK_EV_UNUSED;
		break;
	}
	return ret;
}

#define safe_free(x) do { if (x) free(x), x = 0; } while (0)

static void othello_destroy()
{
	if (!_othello || _othello->state != GAME_OVER)
		return;

	ttk_free_surface(_othello->black);
	ttk_free_surface(_othello->white);
	ttk_free_surface(_othello->selection);
	safe_free(_othello);
}

static struct game_st *othello_create()
{
	char image[256];
	const char *imgpath = pz_module_get_datapath(module, "images");
	struct game_st *o = calloc(1, sizeof(struct game_st));

	snprintf(image, 255, "%s/%d/black.png", imgpath, ttk_screen->h);
	if (!(o->black = ttk_load_image(image)))
		goto err;

	snprintf(image, 255, "%s/%d/white.png", imgpath, ttk_screen->h);
	if (!(o->white = ttk_load_image(image)))
		goto err;

	snprintf(image, 255, "%s/%d/selection.png", imgpath, ttk_screen->h);
	if (!(o->selection = ttk_load_image(image)))
		goto err;

	return o;
err:
	fprintf(stderr, "Unable to load image `%s'\n", image);
	o->nocolor = 1;
	return o;
}

static PzWindow *new_othello_window()
{
	PzWindow *ret;
	PzWidget *wid;

	if (_othello) {
		if (pz_dialog(_("New Game?"), _("You have an unfinished game; "
				"continue?"), 2, 0, _("Yes"), _("No"))) {
			_othello->state = GAME_OVER;
			othello_destroy();
		}
	}
	if (!_othello) {
		if (!(_othello = othello_create()))
			return TTK_MENU_DONOTHING;
		_othello->board = _othello->b_set = 0;
		_othello->cur_bit = 19;
		_othello->state = WAITING;

		set_piece(&_othello->board, &_othello->b_set, S_WHITE, 27);
		set_piece(&_othello->board, &_othello->b_set, S_WHITE, 36);
		set_piece(&_othello->board, &_othello->b_set, S_BLACK, 28);
		set_piece(&_othello->board, &_othello->b_set, S_BLACK, 35);
	}
	ret = pz_new_window(_("Othello"), PZ_WINDOW_NORMAL);

	if (ttk_screen->bpp == 2 || _othello->nocolor)
		wid = pz_add_widget(ret, othello_mono_draw, othello_event);
	else
		wid = pz_add_widget(ret, othello_color_draw, othello_event);

	wid->dirty = 1;
	wid->timer = computer_move;

	return pz_finish_window(ret);
}

static void init_othello()
{
	module = pz_register_module("othello", NULL);
	pz_menu_add_action_group("/Extras/Games/Othello", "Classic", new_othello_window);
}

PZ_MOD_INIT(init_othello)
