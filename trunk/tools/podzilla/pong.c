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

#define SCRW	screen_info.cols
#define SCRH	(screen_info.rows - HEADER_TOPLINE)

#define XLOWER	0
#define XUPPER	SCRW
#define YLOWER	5
#define YUPPER	SCRH - 10
#define PSIZE	10
#define XPOS1	12
#define XPOS2	SCRW - 13

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
static GR_GC_ID pong_gc_black;
static GR_GC_ID pong_gc_white;
static GR_GC_ID pong_gc_gray;

static struct Position pball, pplayer1, pplayer2;
static struct Velocity vball, vplayer1, vplayer2;
static int roundover, comppoint, userpoint, gameover, paused;

void pong_print_score(num, side)
{
	int ref;
	
	ref = (side - 1) ? ((SCRW >> 2) + (SCRW >> 1)) : SCRW >> 2;
	switch (num) {
	case 0:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 5, 6, 12);
		break;
	case 1:
		GrFillRect(pong_pix, pong_gc_gray, ref + 2, 1, 4, 20);
		break;
	case 2:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 5, 5, 10, 4);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 13, 10, 4);
		break;
	case 3:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 5, 5, 10, 4);
		GrFillRect(pong_pix, pong_gc_white, ref - 5, 13, 10, 4);
		break;
	case 4:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 1, 6, 8);
		GrFillRect(pong_pix, pong_gc_white, ref - 5, 13, 10, 8);
		break;
	case 5:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 5, 10, 4);
		GrFillRect(pong_pix, pong_gc_white, ref - 5, 13, 10, 4);
		break;
	case 6:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 5, 10, 4);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 13, 6, 4);
		break;
	case 7:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 5, 5, 10, 16);
		break;
	case 8:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 5, 6, 4);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 13, 6, 4);
		break;
	case 9:
		GrFillRect(pong_pix, pong_gc_gray, ref - 5, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref - 1, 5, 6, 4);
		GrFillRect(pong_pix, pong_gc_white, ref - 5, 13, 10, 4);
		break;
	case 10:
		GrFillRect(pong_pix, pong_gc_gray, ref - 11, 1, 4, 20);
		GrFillRect(pong_pix, pong_gc_gray, ref - 3, 1, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, ref + 1, 5, 6, 12);
		break;
	case 11:
		GrFillRect(pong_pix, pong_gc_gray, ref - 11, 1, 4, 20);
		GrFillRect(pong_pix, pong_gc_gray, ref - 3, 1, 4, 20);
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
			GrFillRect(pong_pix, pong_gc_white, 5,
					ptmp1.y - 10, 4, 20);
		if(pplayer2.y != ptmp2.y)
			GrFillRect(pong_pix, pong_gc_white, SCRW-10,
					ptmp2.y - 10, 4, 20);
		if(comppoint != lastcpoint)
			GrFillRect(pong_pix, pong_gc_white, (SCRW >> 2) - 5, 0,
					20, 21);
		if(userpoint != lastupoint)
			GrFillRect(pong_pix, pong_gc_white, ((SCRW >> 2) +
						(SCRW >> 1)) - 5, 0, 20, 21);
		GrFillEllipse(pong_pix, pong_gc_white, lastpball.x, lastpball.y,
				2, 2);
	} i = 1;

	if(comppoint == 11 || userpoint == 11)  { /* Game Over? */
		GrFillRect(pong_pix, pong_gc_white, 0, 0, SCRW, SCRH);
		gameover = 1;
	}

	pong_print_score(comppoint, 1);
	pong_print_score(userpoint, 2);

	if(comppoint == 11) { /* LOSER */
		/********************** L ************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw-45, hscrh-14, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw-41, hscrh-14, 10, 16);
		/********************** O ************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw-27, hscrh-14, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw-23, hscrh-10, 6, 12);
		/********************** S ************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw-9, hscrh-14, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw-5, hscrh-10, 10, 4);
		GrFillRect(pong_pix, pong_gc_white, hscrw-9, hscrh-2, 10, 4);
		/********************** E ************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw+9, hscrh-14, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw+13, hscrh-10, 10, 4);
		GrFillRect(pong_pix, pong_gc_white, hscrw+13, hscrh-2, 10, 4);
		/********************** R ************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw+27, hscrh-14, 16, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw+31, hscrh-10, 8, 3);
		GrFillRect(pong_pix, pong_gc_white, hscrw+31, hscrh-2, 4, 8);
		GrFillRect(pong_pix, pong_gc_white, hscrw+39, hscrh-2, 4, 4);
		return;
	}

	else if (userpoint == 11) { /* WINNER */
		/*********************** W *************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw-45, hscrh-14, 20, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw-41, hscrh-14, 4, 16);
		GrFillRect(pong_pix, pong_gc_white, hscrw-33, hscrh-14, 4, 16);
		/*********************** I *************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw-21, hscrh-14, 4, 20);
		/*********************** N *************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw-13, hscrh-14, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw-9, hscrh-14, 3, 4);
		GrFillRect(pong_pix, pong_gc_white, hscrw-6, hscrh-14, 3, 8);
		GrFillRect(pong_pix, pong_gc_white, hscrw-9, hscrh-2, 3, 8);
		GrFillRect(pong_pix, pong_gc_white, hscrw-6, hscrh+2, 3, 4);
		/*********************** N *************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw+5, hscrh-14, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw+9, hscrh-14, 3, 4);
		GrFillRect(pong_pix, pong_gc_white, hscrw+12, hscrh-14, 3, 8);
		GrFillRect(pong_pix, pong_gc_white, hscrw+9, hscrh-2, 3, 8);
		GrFillRect(pong_pix, pong_gc_white, hscrw+12, hscrh+2, 3, 4);
		/*********************** E *************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw+23, hscrh-14, 14, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw+27, hscrh-10, 10, 4);
		GrFillRect(pong_pix, pong_gc_white, hscrw+27, hscrh-2, 10, 4);
		/*********************** R *************************/
		GrFillRect(pong_pix, pong_gc_black, hscrw+41, hscrh-14, 16, 20);
		GrFillRect(pong_pix, pong_gc_white, hscrw+45, hscrh-10, 8, 3);
		GrFillRect(pong_pix, pong_gc_white, hscrw+45, hscrh-2, 4, 8);
		GrFillRect(pong_pix, pong_gc_white, hscrw+53, hscrh-2, 4, 4);
		return;
	}

	if (roundover == 1) {
		GrText(pong_pix, pong_gc_black, (SCRW >> 1) + 15,
				SCRH >> 1, "Ready?", -1, GR_TFASCII);
	}
	
	for (k = 0; k < ((SCRH - 3) >> 3); k++) {
		GrLine(pong_pix, pong_gc_black, SCRW >> 1,
				(((k + 1) << 2) + (k << 2)), (SCRW >> 1),
				(((k + 2) << 2) + (k << 2)));
	}

	GrLine(pong_pix, pong_gc_black, 0, SCRH - 5, SCRW, SCRH - 5);
	GrFillRect(pong_pix, pong_gc_white, 0, SCRH - 4, SCRW, 4);
	/* computer paddle */
	GrFillRect(pong_pix, pong_gc_black, 5, pplayer1.y - 10, 4, 20);
	/* user paddle */
	GrFillRect(pong_pix, pong_gc_black, SCRW - 10, pplayer2.y - 10, 4, 20);
	/* ball */
	GrFillEllipse(pong_pix, pong_gc_black, pball.x, pball.y, 2, 2);

	ptmp1.y = pplayer1.y;
	ptmp2.y = pplayer2.y;
	lastpball.x = pball.x;
	lastpball.y = pball.y;
	lastupoint = userpoint;
	lastcpoint = comppoint;
}

static void pong_do_draw(void) {
	pz_draw_header("Pong");
	draw_pong();
}

static void pong_move_player(struct Position * player, const int ammount) {
	if (player->y + ammount < PSIZE)
		player->y = PSIZE;
	else if (player->y + ammount > (SCRH - 5) - PSIZE)
		player->y = (SCRH - 5) - PSIZE;
	else
		player->y += ammount;
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
		GrFillRect(pong_pix, pong_gc_white, (SCRW >> 1) + 15,
				(SCRH >> 1) - 14, 40, 16);
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
			GrDestroyGC(pong_gc_gray);
			GrDestroyGC(pong_gc_white);
			GrDestroyGC(pong_gc_black);
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

	pong_gc_black = pz_get_gc(1);
	GrSetGCUseBackground(pong_gc_black, GR_FALSE);
	GrSetGCForeground(pong_gc_black, BLACK);

	pong_gc_white = pz_get_gc(1);
	GrSetGCForeground(pong_gc_white, WHITE);
	
	pong_gc_gray = pz_get_gc(1);
	GrSetGCForeground(pong_gc_gray, GRAY);

	pplayer2.y = SCRH >> 1;
	pplayer1.y = SCRH >> 1;
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
	pong_timer = GrCreateTimer(pong_pix, 75);
}

