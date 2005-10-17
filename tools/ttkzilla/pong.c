/*
 * Copyright (C) 2004, 2005 Courtney Cavin
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

#include <time.h>
#include <stdlib.h>
#include "pz.h"
#include "vectorfont.h"

#define SCRW	screen_info.cols
#define SCRH	(screen_info.rows - HEADER_TOPLINE)

#define XLOWER	0
#define XUPPER	SCRW
#define YLOWER	5
#define YUPPER	SCRH - 10
#define PSIZE	10
#define XPOS1	12
#define XPOS2	SCRW - 13

static unsigned short pong_ball[] = {
    0x6000, // . X X .
    0xf000, // X X X X
    0xf000, // X X X X
    0x6000, // . X X .
    0
};

struct Position {
	int x;
	int y;
};

struct Velocity {
	double x;
	double y;
};

static GR_TIMER_ID pong_timer;
/* static GR_WINDOW_ID pong_wid; */
static GR_WINDOW_ID pong_pix;
static GR_GC_ID pong_gc_fg;
static GR_GC_ID pong_gc_bg;
static GR_GC_ID pong_gc_score;

static struct Position pball, pplayer1, pplayer2;
static struct Velocity vball, vplayer1, vplayer2;
static int roundover, comppoint, userpoint, gameover, paused;

void pong_print_score(num, side)
{
	int ref;
	
	ref = (side - 1) ? ((SCRW >> 2) + (SCRW >> 1)) : SCRW >> 2;
	switch (num) {
	case 0:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 5, 6, 12);
		break;
	case 1:
		GrFillRect(pong_pix, pong_gc_score, ref + 2, 1, 4, 20);
		break;
	case 2:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 5, 5, 10, 4);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 13, 10, 4);
		break;
	case 3:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 5, 5, 10, 4);
		GrFillRect(pong_pix, pong_gc_bg, ref - 5, 13, 10, 4);
		break;
	case 4:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 1, 6, 8);
		GrFillRect(pong_pix, pong_gc_bg, ref - 5, 13, 10, 8);
		break;
	case 5:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 5, 10, 4);
		GrFillRect(pong_pix, pong_gc_bg, ref - 5, 13, 10, 4);
		break;
	case 6:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 5, 10, 4);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 13, 6, 4);
		break;
	case 7:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 5, 5, 10, 16);
		break;
	case 8:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 5, 6, 4);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 13, 6, 4);
		break;
	case 9:
		GrFillRect(pong_pix, pong_gc_score, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref - 1, 5, 6, 4);
		GrFillRect(pong_pix, pong_gc_bg, ref - 5, 13, 10, 4);
		break;
	case 10:
		GrFillRect(pong_pix, pong_gc_score, ref - 11, 1, 4, 20);
		GrFillRect(pong_pix, pong_gc_score, ref - 3, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_bg, ref + 1, 5, 6, 12);
		break;
	case 11:
		GrFillRect(pong_pix, pong_gc_score, ref - 11, 1, 4, 20);
		GrFillRect(pong_pix, pong_gc_score, ref - 3, 1, 4, 20);
		break;
	}
}

void draw_pong() {
	int hscrw = SCRW >> 1;
	int hscrh = SCRH >> 1;
	static int i = 0, k = 0;
	static int lastcpoint, lastupoint;
	static struct Position ptmp1, ptmp2, lastpball;

	if (i != 0) { /* Erasing */
		if(pplayer1.y != ptmp1.y)
			GrFillRect(pong_pix, pong_gc_bg, 5,
					ptmp1.y - 10, 4, 20);
		if(pplayer2.y != ptmp2.y)
			GrFillRect(pong_pix, pong_gc_bg, SCRW-10,
					ptmp2.y - 10, 4, 20);
		if(comppoint != lastcpoint)
			GrFillRect(pong_pix, pong_gc_bg, (SCRW >> 2) - 5, 0,
					20, 21);
		if(userpoint != lastupoint)
			GrFillRect(pong_pix, pong_gc_bg, ((SCRW >> 2) +
						(SCRW >> 1)) - 5, 0, 20, 21);
		GrBitmap (pong_pix, pong_gc_bg, lastpball.x - 2, lastpball.y - 2, 4, 4, pong_ball);
	} i = 1;

	if(comppoint == 11 || userpoint == 11)  { /* Game Over? */
		GrFillRect(pong_pix, pong_gc_bg, 0, 0, SCRW, SCRH);
		gameover = 1;
	}

	pong_print_score(comppoint, 1);
	pong_print_score(userpoint, 2);

	if(comppoint == 11) { /* LOSER */
		int i;
		for (i = 8; i; i--) {
			if (vector_string_pixel_width("LOSER", 0, i) < hscrh*2)
				break;
		}
		vector_render_string_center(pong_pix, pong_gc_fg, "LOSER", 0, i,
				hscrw, hscrh);
		return;
	}

	else if (userpoint == 11) { /* WINNER */
		int i;
		for (i = 8; i; i--) {
			if (vector_string_pixel_width("WINNER", 0, i) < hscrh*2)
				break;
		}
		vector_render_string_center(pong_pix, pong_gc_fg, "WINNER", 0,
				i, hscrw, hscrh);
		return;
	}

	if (roundover == 1) {
		vector_render_string(pong_pix, pong_gc_fg, "READY?", 1, 1,
				(SCRW >> 1) + 15, (SCRH >> 1) - 4);
	}
	
	for (k = 0; k < ((SCRH - 3) >> 3); k++) {
		GrLine(pong_pix, pong_gc_fg, SCRW >> 1,
				(((k + 1) << 2) + (k << 2)), (SCRW >> 1),
				(((k + 2) << 2) + (k << 2)));
	}

	GrLine(pong_pix, pong_gc_fg, 0, SCRH - 5, SCRW, SCRH - 5);
	GrFillRect(pong_pix, pong_gc_bg, 0, SCRH - 4, SCRW, 4);
	/* computer paddle */
	GrFillRect(pong_pix, pong_gc_fg, 5, pplayer1.y - 10, 4, 20);
	/* user paddle */
	GrFillRect(pong_pix, pong_gc_fg, SCRW - 10, pplayer2.y - 10, 4, 20);
	/* ball */
	GrBitmap(pong_pix, pong_gc_fg, pball.x - 2, pball.y - 2, 4, 4, pong_ball);

	ptmp1.y = pplayer1.y;
	ptmp2.y = pplayer2.y;
	lastpball.x = pball.x;
	lastpball.y = pball.y;
	lastupoint = userpoint;
	lastcpoint = comppoint;
}

static void pong_do_draw(void) {
	pz_draw_header(_("Pong"));
	draw_pong();
}

static void pong_move_player(struct Position * player, const int amount) {
	if (player->y + amount < PSIZE)
		player->y = PSIZE;
	else if (player->y + amount > (SCRH - 5) - PSIZE)
		player->y = (SCRH - 5) - PSIZE;
	else
		player->y += amount;
}

int sane() {
	static int rev_timestep = 25;
	static int turn;
	static int inc = 0;
	int mv_amount = 0;
	if (roundover == 2) {  /* initialize ball's velocity */
		pball.x = SCRW >> 1;
		pball.y = SCRH >> 1;
		vball.x = (turn == 1) ? -60 : 60;
		vball.y = 72;
		turn = 0;
		roundover = 0;
		GrFillRect(pong_pix, pong_gc_bg, (SCRW >> 1) + 15,
				(SCRH >> 1) - 4, 40, 16);
	}
	else if (roundover == 1)
		return 0;
	if ( pball.x > XLOWER && pball.x < XUPPER) {  /* update position */
		pball.x += vball.x / rev_timestep;
		pball.y += vball.y / rev_timestep;
		if (++inc > 128) {
			rev_timestep -= 1;
			inc = 0;
			if (rev_timestep < 1)
				rev_timestep = 1;
		}

		/* check for playable walls */
		if ((pball.y <= YLOWER && vball.y < 0)
				|| (pball.y >= YUPPER && vball.y > 0))
			vball.y *= -1;
		
		/* check for interaction with player paddle ( two ifs ) */
		if (vball.x < 0 && pball.x > XPOS1 - 7 
				&& pball.x <= XPOS1 &&
				pball.y <= pplayer1.y + PSIZE &&
				pball.y >= pplayer1.y - PSIZE) {
			vplayer1.y = pball.y - pplayer1.y;
			if (vball.y < 0)
				vball.y += (vplayer1.y - 6) / 3;
			vball.x *= -1;
		}
		else if (vball.x > 0 && pball.x < XPOS2 + 7
				&& pball.x >= XPOS2 &&
				pball.y <= pplayer2.y + PSIZE &&
				pball.y >= pplayer2.y - PSIZE) {
			vplayer2.y = pball.y - pplayer2.y;
			if(vball.y < 0)
				vball.y += (vplayer2.y - 6) / 3;
			vball.x *= -1;
		}

		/* player AI(not really, just follows the ball) */
		if (pball.x < SCRW >> 1 && vball.x < 0) {
			mv_amount = pball.y - pplayer1.y;
			mv_amount = (mv_amount > 0) ?
				((mv_amount > 6) ? 6 : mv_amount) :
				((mv_amount < -6) ? -6 : mv_amount);
		}
		else {
			mv_amount = (SCRH >> 1) - pplayer1.y;
			mv_amount = (mv_amount > 0) ?
				((mv_amount > 3) ? 3 : 1) :
				((mv_amount < -3) ? -3 : mv_amount);
		}
		
		if (mv_amount != 0)
			pong_move_player(&pplayer1, mv_amount);
	}
	else {
		if(pball.x <= XLOWER) {
			userpoint++;
			inc += 32;
			turn = 2;
		}
		else if(pball.x >= XUPPER) {
			comppoint++;
			rev_timestep += 2;
			turn = 1;
		}
		roundover = 1;
	}
	return 0;
}


static int pong_do_keystroke(GR_EVENT * event)
{
	int ret = 0;

	switch (event->type) {
	case GR_EVENT_TYPE_TIMER:
		if(!gameover && !paused) {
			sane();
			draw_pong();
		}
		break;
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case 'l':
			pong_move_player(&pplayer2, -5);
			ret |= KEY_CLICK;
			break;
		case 'r':
			pong_move_player(&pplayer2, 5);
			ret |= KEY_CLICK;
			break;
		case 'h':
			paused = 1;
			break;
		case '\r':
			if(roundover == 1)
				roundover = 2;
			ret |= KEY_CLICK;
			break;
		case 'm':
			pz_close_window(pong_pix);
			GrDestroyGC(pong_gc_score);
			GrDestroyGC(pong_gc_bg);
			GrDestroyGC(pong_gc_fg);
			GrDestroyTimer(pong_timer);
			ret |= KEY_CLICK;
			break;
		default:
			ret |= KEY_UNUSED;
			break;
		}
		break;
	case GR_EVENT_TYPE_KEY_UP:
		switch (event->keystroke.ch) {
		case 'h':
			paused = 0;
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


void new_pong_window()
{
	GrGetScreenInfo(&screen_info);

	pong_gc_fg = pz_get_gc(1);
	GrSetGCForeground(pong_gc_fg, appearance_get_color(CS_FG));

	pong_gc_bg = pz_get_gc(1);
	GrSetGCForeground(pong_gc_bg, appearance_get_color(CS_BG));
	
	pong_gc_score = pz_get_gc(1);
	GrSetGCForeground(pong_gc_score, appearance_get_color(CS_SCRLKNOB));

	pplayer2.y = pplayer1.y = SCRH >> 1;
	pball.y = pball.y = -5;
	roundover = 1;
	comppoint = 0;
	userpoint = 0;
	gameover = 0;
	paused = 0;

	pong_pix = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1), pong_do_draw,
			pong_do_keystroke);

	GrSelectEvents(pong_pix, GR_EVENT_MASK_EXPOSURE |
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP |
			GR_EVENT_MASK_TIMER);

	GrMapWindow(pong_pix);
	GrFillRect(pong_pix, pong_gc_bg, 0, 0, SCRW, SCRH);

	pong_timer = GrCreateTimer(pong_pix, 75);
}

