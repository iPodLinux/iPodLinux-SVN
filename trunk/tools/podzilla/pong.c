/*
 * Copyright (C) 2004 Courtney Cavin
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
#include <math.h>
#include "pz.h"

/****************** ISO Lib prototypes ***********************/
double atan(double x);
double cos(double x);
double sin(double x);
double sqrt(double x);
/************************* Structs  ****************************/
struct Position
{ int x;
  int y;
};
struct Velocity
{ double x;
  double y;
};
/************************** globals  ***************************/
static GR_TIMER_ID timer_id;
static GR_WINDOW_ID pong_wid;
static GR_GC_ID pong_gc;
static GR_SCREEN_INFO screen_info;
static struct Position pball, pplayer1, pplayer2;
static struct Velocity vball, vplayer1, vplayer2;
int roundover=2, comppoint=0, userpoint=0, gameover=0;
/************************* Prototypes  *************************/
static void draw_pong();
void new_pong_window();
static void pong_do_draw();
static int pong_do_keystroke(GR_EVENT * event);
struct Position add(const struct Position,const struct Position);
/***************************************************************/

void draw_pong() {
	static int i=0, lastpballx, lastpbally, lastcpoint, lastupoint;
	GR_POINT breakfast[] = {
		{pball.x-1, pball.y-3},
		{pball.x-3, pball.y-1},
		{pball.x-3, pball.y+1},
		{pball.x-1, pball.y+3},
		{pball.x+1, pball.y+3},
		{pball.x+3, pball.y+1},
		{pball.x+3, pball.y-1},
		{pball.x+1, pball.y-3},
		{pball.x-1, pball.y-3}};
	GR_POINT breaks[] = {
		{lastpballx-1, lastpbally-3},
		{lastpballx-3, lastpbally-1},
		{lastpballx-3, lastpbally+1},
		{lastpballx-1, lastpbally+3},
		{lastpballx+1, lastpbally+3},
		{lastpballx+3, lastpbally+1},
		{lastpballx+3, lastpbally-1},
		{lastpballx+1, lastpbally-3},
		{lastpballx-1, lastpbally-3}};
	static struct Position ptmp1, ptmp2;

	GrSetGCUseBackground(pong_gc, GR_FALSE);
	GrSetGCForeground(pong_gc, WHITE);
	if (i!=0) { //Erasing
		if(pplayer1.y!=ptmp1.y)
			GrFillRect(pong_wid, pong_gc, 5, ptmp1.y-10, 4, 20);
		if(pplayer2.y!=ptmp2.y)
			GrFillRect(pong_wid, pong_gc, 150, ptmp2.y-10, 4, 20);
		if(comppoint!=lastcpoint)
			GrFillRect(pong_wid, pong_gc, 35, 0, 20, 21);
		if(userpoint!=lastupoint)
			GrFillRect(pong_wid, pong_gc, 115, 0, 20, 21);
		GrFillPoly(pong_wid, pong_gc, 9, breaks);
		if(roundover==2)
			GrFillRect(pong_wid, pong_gc, 95, 40, 40, 14);
	}i=1;

	if(comppoint==11 || userpoint==11)  { //Game Over?
		GrFillRect(pong_wid, pong_gc, 0, 0, 160, 118);
		gameover=1;
	}

	GrSetGCForeground(pong_gc, LTGRAY);

	//Score Keeper
	switch(comppoint) {
		case 0:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 39, 5, 6, 12);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 1:
			GrFillRect(pong_wid, pong_gc, 42, 1, 4, 20);
			break;
		case 2:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 35, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, 39, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 3:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 35, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, 35, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 4:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 39, 1, 6, 8);
			GrFillRect(pong_wid, pong_gc, 35, 13, 10, 8);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 5:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 39, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, 35, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 6:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 39, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, 39, 13, 6, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 7:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 35, 5, 10, 16);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 8:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 39, 5, 6, 4);
			GrFillRect(pong_wid, pong_gc, 39, 13, 6, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 9:
			GrFillRect(pong_wid, pong_gc, 35, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 39, 5, 6, 4);
			GrFillRect(pong_wid, pong_gc, 35, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 10:
			GrFillRect(pong_wid, pong_gc, 29, 1, 4, 20);
			GrFillRect(pong_wid, pong_gc, 37, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 41, 5, 6, 12);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 11:
			GrFillRect(pong_wid, pong_gc, 29, 1, 4, 20);
			GrFillRect(pong_wid, pong_gc, 37, 1, 4, 20);
			break;
	}
	switch(userpoint) {
		case 0:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 119, 5, 6, 12);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 1:
			GrFillRect(pong_wid, pong_gc, 122, 1, 4, 20);
			break;
		case 2:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 115, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, 119, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 3:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 115, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, 115, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 4:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 119, 1, 6, 8);
			GrFillRect(pong_wid, pong_gc, 115, 13, 10, 8);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 5:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 119, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, 115, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 6:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 119, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, 119, 13, 6, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 7:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 115, 5, 10, 16);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 8:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 119, 5, 6, 4);
			GrFillRect(pong_wid, pong_gc, 119, 13, 6, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 9:
			GrFillRect(pong_wid, pong_gc, 115, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 119, 5, 6, 4);
			GrFillRect(pong_wid, pong_gc, 115, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 10:
			GrFillRect(pong_wid, pong_gc, 109, 1, 4, 20);
			GrFillRect(pong_wid, pong_gc, 117, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, 121, 5, 6, 12);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 11:
			GrFillRect(pong_wid, pong_gc, 109, 1, 4, 20);
			GrFillRect(pong_wid, pong_gc, 117, 1, 4, 20);
			break;
	}

	GrSetGCForeground(pong_gc, BLACK);

	if(comppoint==11) {//LOSER
		/*********************** L ************************/
		GrFillRect(pong_wid, pong_gc, 35, 40, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 39, 40, 10, 16);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** O ************************/
		GrFillRect(pong_wid, pong_gc, 53, 40, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 57, 44, 6, 12);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** S ************************/
		GrFillRect(pong_wid, pong_gc, 71, 40, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 75, 44, 10, 4);
		GrFillRect(pong_wid, pong_gc, 71, 52, 10, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** E ***********************/
		GrFillRect(pong_wid, pong_gc, 89, 40, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 93, 44, 10, 4);
		GrFillRect(pong_wid, pong_gc, 93, 52, 10, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** R ***********************/
		GrFillRect(pong_wid, pong_gc, 107, 40, 16, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 111, 44, 8, 3);
		GrFillRect(pong_wid, pong_gc, 111, 52, 4, 8);
		GrFillRect(pong_wid, pong_gc, 119, 52, 4, 4);
		GrSetGCForeground(pong_gc, BLACK);
		return;
	}

	else if (userpoint==11) { //WINNER
		/*********************** W ************************/
		GrFillRect(pong_wid, pong_gc, 35, 40, 20, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 39, 40, 4, 16);
		GrFillRect(pong_wid, pong_gc, 47, 40, 4, 16);
		GrSetGCForeground(pong_gc, BLACK);
		/*********************** I *************************/
		GrFillRect(pong_wid, pong_gc, 59, 40, 4, 20);
		/********************** N *************************/
		GrFillRect(pong_wid, pong_gc, 67, 40, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 71, 40, 3, 4);
		GrFillRect(pong_wid, pong_gc, 74, 40, 3, 8);
		GrFillRect(pong_wid, pong_gc, 71, 52, 3, 8);
		GrFillRect(pong_wid, pong_gc, 74, 56, 3, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** N ************************/
		GrFillRect(pong_wid, pong_gc, 85, 40, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 89, 40, 3, 4);
		GrFillRect(pong_wid, pong_gc, 92, 40, 3, 8);
		GrFillRect(pong_wid, pong_gc, 89, 52, 3, 8);
		GrFillRect(pong_wid, pong_gc, 92, 56, 3, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** E *************************/
		GrFillRect(pong_wid, pong_gc, 103, 40, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 107, 44, 10, 4);
		GrFillRect(pong_wid, pong_gc, 107, 52, 10, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** R ***********************/
		GrFillRect(pong_wid, pong_gc, 121, 40, 16, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, 125, 44, 8, 3);
		GrFillRect(pong_wid, pong_gc, 125, 52, 4, 8);
		GrFillRect(pong_wid, pong_gc, 133, 52, 4, 4);
		GrSetGCForeground(pong_gc, BLACK);
		return;
	}

	if(roundover==1) {
		GrText(pong_wid, pong_gc, 95, 52, "Ready?", -1, GR_TFASCII);
	}
	GrLine(pong_wid, pong_gc, 80, 4, 80, 8);
	GrLine(pong_wid, pong_gc, 80, 12, 80, 16);
	GrLine(pong_wid, pong_gc, 80, 20, 80, 24);
	GrLine(pong_wid, pong_gc, 80, 28, 80, 32);
	GrLine(pong_wid, pong_gc, 80, 36, 80, 40);
	GrLine(pong_wid, pong_gc, 80, 44, 80, 48);
	GrLine(pong_wid, pong_gc, 80, 52, 80, 56);
	GrLine(pong_wid, pong_gc, 80, 60, 80, 64);
	GrLine(pong_wid, pong_gc, 80, 68, 80, 72);
	GrLine(pong_wid, pong_gc, 80, 76, 80, 80);
	GrLine(pong_wid, pong_gc, 80, 84, 80, 88);
	GrLine(pong_wid, pong_gc, 80, 92, 80, 96);
	GrLine(pong_wid, pong_gc, 80, 100, 80, 103);

	GrLine(pong_wid, pong_gc, 0, 103, 160, 103);
	GrFillRect(pong_wid, pong_gc, 5, pplayer1.y-10, 4, 20); //computer paddle
	GrFillRect(pong_wid, pong_gc, 150, pplayer2.y-10, 4, 20); //user paddle
	GrFillPoly(pong_wid, pong_gc, 9, breakfast); //Ball

	ptmp1.y=pplayer1.y;
	ptmp2.y=pplayer2.y;
	lastpballx=pball.x;
	lastpbally=pball.y;
	lastupoint=userpoint;
	lastcpoint=comppoint;
}

static void pong_do_draw() {
	pz_draw_header("Pong");
	pplayer2.y=54;
	pplayer1.y=54;
	comppoint=0;
	userpoint=0;
	gameover=0;
	draw_pong();
}

/*struct Position add(const struct Position p1,const struct Position p2)
{ double px, py;
  px = p1.x + p2.x;
  py = p1.y + p2.y;
  struct Position P;
  P.x=px;
  P.y=py;
  return P;
}*/

int sane() {
	char setting[] = "easy";
	double xl=0,xu=160,yl=5,yu=98,d=10,vmax=500;
	float timestep=.3;
	static double xp1=12, xp2=147;
	static int turn=0;
	vplayer1.x=0;
	vplayer2.x=0;
	// vplayer1.y=cpp1;
	// vplayer2.y=cpp2;
	if(roundover==2) {  // initialize ball's velocity
		pball.x=80;
		pball.y=54;
		if(turn==1)
			vball.x=-6;
		else
			vball.x=6;
		vball.y=7;
		turn=0;
		roundover=0;
	}
	else if(roundover==1)
		return 0;
	if( pball.x > xl && pball.x < xu) {  // update position
		pball.x+=(vball.x)*timestep;
		pball.y+=(vball.y)*timestep;

		// check for playable walls
 		if ((pball.y <= yl && vball.y < 0)|| (pball.y >= yu&& vball.y > 0)) {
			vball.y= -1*vball.y;
		}
		// check for interaction with player paddle ( two ifs )
		if (vball.x < 0 && pball.x > (xp1-7) && pball.x <= xp1 && pball.y <= pplayer1.y+d && pball.y >= pplayer1.y-d) {
			vplayer1.y=pball.y-pplayer1.y;
			if(vball.y < 0)
				vball.y+=(vplayer1.y-6)/3;
			vball.x *= -1;
		}

		else if (vball.x > 0 && pball.x < (xp2+7) && pball.x >= xp2 && pball.y <= pplayer2.y+d && pball.y >= pplayer2.y-d) {
			vplayer2.y=pball.y-pplayer2.y;
			if(vball.y < 0)
				vball.y+=(vplayer2.y-6)/3;
			vball.x *= -1;
		}

		//player AI(not really, just follows the ball)
		if(pball.x < 80 && vball.x < 0) {
			if(pball.y > pplayer1.y) {
				if((pball.y-pplayer1.y) < 6)
					pplayer1.y++;
				else
					pplayer1.y+=6;
			}
			else if(pball.y < pplayer1.y) {
				if((pplayer1.y-pball.y) < 6)
					pplayer1.y--;
				else
					pplayer1.y-=6;
			}
		}
		else {
			if (pplayer1.y < 54) {
				if((54-pplayer1.y) < 3)
					pplayer1.y++;
				else
					pplayer1.y+=3;
			}
			else if(pplayer1.y > 54)
				pplayer1.y-=3;
		}

		// regulate max speed ( set high at first )
		if (sqrt(vball.x*vball.x+vball.y*vball.y)>vmax) {
			if (setting=="easy") {
				double theta;
				theta=atan(vball.y/vball.x);
				vball.x=vmax*cos(theta);
				vball.y=vmax*sin(theta);
			}
		}
	}
	else {
		if(pball.x <= xl) {
			userpoint++;
			turn=2;
		}
		else if(pball.x >= xu) {
			comppoint++;
			turn=1;
		}
		roundover=1;
	}
	return 0;
}


static int pong_do_keystroke(GR_EVENT * event) {
	int ret = 0;
	switch (event->type) {
		case GR_EVENT_TYPE_TIMER:
			if(!gameover)
				sane();
			ret = 1;
			break;
		case GR_EVENT_TYPE_KEY_DOWN:
			switch (event->keystroke.ch) {
				case '\r':
					if(roundover==1)
						roundover=2;
					ret = 1;
					break;
				case 'l':
					pplayer2.y-=5;
					ret = 1;
					break;
				case 'r':
					pplayer2.y+=5;
					ret = 1;
					break;
#ifndef IPOD
				case 'q':
					GrClose();
					exit(0);
					break;
#endif
				case 'm':
					roundover=2;
					gameover=0;
					pz_close_window(pong_wid);
					ret = 1;
					break;
			}
			break;
	}
	if(pplayer2.y<10)
		pplayer2.y=10;
	else if(pplayer2.y>88)
		pplayer2.y=88;
	if(pplayer1.y<10)
		pplayer1.y=10;
	else if(pplayer1.y>88)
		pplayer1.y=88;
	if(!gameover)
		draw_pong();
	return ret;
}


void new_pong_window()
{
	GrGetScreenInfo(&screen_info);

	pong_gc = GrNewGC();
	GrSetGCUseBackground(pong_gc, GR_FALSE);
	GrSetGCForeground(pong_gc, BLACK);

	pong_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), pong_do_draw, pong_do_keystroke);

	GrSelectEvents(pong_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_TIMER);

	GrMapWindow(pong_wid);
	timer_id = GrCreateTimer(pong_wid, 75); /*Create nano-x timer*/
}

