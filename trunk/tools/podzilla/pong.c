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

#define SCRW  screen_info.cols
#define SCRH  (screen_info.rows-HEADER_TOPLINE)

#define LMID  ((SCRW/2)/2)
#define RMID  (((SCRW/2)/2)+(SCRW/2))

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
	static int i=0, k=0, lastpballx, lastpbally, lastcpoint, lastupoint;
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
			GrFillRect(pong_wid, pong_gc, SCRW-10, ptmp2.y-10, 4, 20);
		if(comppoint!=lastcpoint)
			GrFillRect(pong_wid, pong_gc, LMID-5, 0, 20, 21);
		if(userpoint!=lastupoint)
			GrFillRect(pong_wid, pong_gc, RMID-5, 0, 20, 21);
		GrFillPoly(pong_wid, pong_gc, 9, breaks);
		if(roundover==2)
			GrFillRect(pong_wid, pong_gc, (SCRW/2)+15, (SCRH/2)-14, 40, 16);
	}i=1;

	if(comppoint==11 || userpoint==11)  { //Game Over?
		GrFillRect(pong_wid, pong_gc, 0, 0, SCRW, SCRH);
		gameover=1;
	}

	GrSetGCForeground(pong_gc, LTGRAY);

	//Score Keeper
	switch(comppoint) {
		case 0:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-1, 5, 6, 12);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 1:
			GrFillRect(pong_wid, pong_gc, LMID+2, 1, 4, 20);
			break;
		case 2:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-5, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, LMID-1, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 3:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-5, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, LMID-5, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 4:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-1, 1, 6, 8);
			GrFillRect(pong_wid, pong_gc, LMID-5, 13, 10, 8);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 5:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-1, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, LMID-5, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 6:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-1, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, LMID-1, 13, 6, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 7:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-5, 5, 10, 16);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 8:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-1, 5, 6, 4);
			GrFillRect(pong_wid, pong_gc, LMID-1, 13, 6, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 9:
			GrFillRect(pong_wid, pong_gc, LMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID-1, 5, 6, 4);
			GrFillRect(pong_wid, pong_gc, LMID-5, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 10:
			GrFillRect(pong_wid, pong_gc, LMID-11, 1, 4, 20);
			GrFillRect(pong_wid, pong_gc, LMID-3, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, LMID+1, 5, 6, 12);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 11:
			GrFillRect(pong_wid, pong_gc, LMID-11, 1, 4, 20);
			GrFillRect(pong_wid, pong_gc, LMID-3, 1, 4, 20);
			break;
	}
	switch(userpoint) {
		case 0:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-1, 5, 6, 12);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 1:
			GrFillRect(pong_wid, pong_gc, RMID+2, 1, 4, 20);
			break;
		case 2:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-5, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, RMID-1, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 3:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-5, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, RMID-5, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 4:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-1, 1, 6, 8);
			GrFillRect(pong_wid, pong_gc, RMID-5, 13, 10, 8);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 5:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-1, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, RMID-5, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 6:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-1, 5, 10, 4);
			GrFillRect(pong_wid, pong_gc, RMID-1, 13, 6, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 7:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-5, 5, 10, 16);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 8:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-1, 5, 6, 4);
			GrFillRect(pong_wid, pong_gc, RMID-1, 13, 6, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 9:
			GrFillRect(pong_wid, pong_gc, RMID-5, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID-1, 5, 6, 4);
			GrFillRect(pong_wid, pong_gc, RMID-5, 13, 10, 4);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 10:
			GrFillRect(pong_wid, pong_gc, RMID-11, 1, 4, 20);
			GrFillRect(pong_wid, pong_gc, RMID-3, 1, 14, 20);
			GrSetGCForeground(pong_gc, WHITE);
			GrFillRect(pong_wid, pong_gc, RMID+1, 5, 6, 12);
			GrSetGCForeground(pong_gc, LTGRAY);
			break;
		case 11:
			GrFillRect(pong_wid, pong_gc, RMID-11, 1, 4, 20);
			GrFillRect(pong_wid, pong_gc, RMID-3, 1, 4, 20);
			break;
	}

	GrSetGCForeground(pong_gc, BLACK);

	if(comppoint==11) {//LOSER
		/*********************** L ************************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-45, (SCRH/2)-14, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-41, (SCRH/2)-14, 10, 16);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** O ************************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-27, (SCRH/2)-14, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-23, (SCRH/2)-10, 6, 12);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** S ************************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-9, (SCRH/2)-14, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-5, (SCRH/2)-10, 10, 4);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-9, (SCRH/2)-2, 10, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** E ***********************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+9, (SCRH/2)-14, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+13, (SCRH/2)-10, 10, 4);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+13, (SCRH/2)-2, 10, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** R ***********************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+27, (SCRH/2)-14, 16, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+31, (SCRH/2)-10, 8, 3);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+31, (SCRH/2)-2, 4, 8);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+39, (SCRH/2)-2, 4, 4);
		GrSetGCForeground(pong_gc, BLACK);
		return;
	}

	else if (userpoint==11) { //WINNER
		/*********************** W ************************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-45, (SCRH/2)-14, 20, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-41, (SCRH/2)-14, 4, 16);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-33, (SCRH/2)-14, 4, 16);
		GrSetGCForeground(pong_gc, BLACK);
		/*********************** I *************************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-21, (SCRH/2)-14, 4, 20);
		/********************** N *************************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-13, (SCRH/2)-14, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-9, (SCRH/2)-14, 3, 4);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-6, (SCRH/2)-14, 3, 8);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-9, (SCRH/2)-2, 3, 8);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)-6, (SCRH/2)+2, 3, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** N ************************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+5, (SCRH/2)-14, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+9, (SCRH/2)-14, 3, 4);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+12, (SCRH/2)-14, 3, 8);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+9, (SCRH/2)-2, 3, 8);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+12, (SCRH/2)+2, 3, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** E *************************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+23, (SCRH/2)-14, 14, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+27, (SCRH/2)-10, 10, 4);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+27, (SCRH/2)-2, 10, 4);
		GrSetGCForeground(pong_gc, BLACK);
		/********************** R ***********************/
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+41, (SCRH/2)-14, 16, 20);
		GrSetGCForeground(pong_gc, WHITE);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+45, (SCRH/2)-10, 8, 3);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+45, (SCRH/2)-2, 4, 8);
		GrFillRect(pong_wid, pong_gc, (SCRW/2)+53, (SCRH/2)-2, 4, 4);
		GrSetGCForeground(pong_gc, BLACK);
		return;
	}

	if(roundover==1) {
		GrText(pong_wid, pong_gc, (SCRW/2)+15, (SCRH/2), "Ready?", -1, GR_TFASCII);
	}
	
	for (k = 0; k < ((SCRH-3)/8); k++) {
		GrLine(pong_wid, pong_gc, (SCRW/2), (((k+1)*4)+(k*4)), (SCRW/2), (((k+2)*4)+(k*4)));
	}

	GrLine(pong_wid, pong_gc, 0, SCRH-5, SCRW, SCRH-5);
	GrSetGCForeground(pong_gc, WHITE);
	GrFillRect(pong_wid, pong_gc, 0, SCRH-4, SCRW, 4);
	GrSetGCForeground(pong_gc, BLACK);
	GrFillRect(pong_wid, pong_gc, 5, pplayer1.y-10, 4, 20); //computer paddle
	GrFillRect(pong_wid, pong_gc, SCRW-10, pplayer2.y-10, 4, 20); //user paddle
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
	pplayer2.y=(SCRH/2);
	pplayer1.y=(SCRH/2);
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
	double xl=0,xu=SCRW,yl=5,yu=(SCRH-10),d=10,vmax=500;
	float timestep=.3;
	double xp1=12, xp2=(SCRW-13);
	static int turn=0;
	vplayer1.x=0;
	vplayer2.x=0;
	// vplayer1.y=cpp1;
	// vplayer2.y=cpp2;
	if(roundover==2) {  // initialize ball's velocity
		pball.x=(SCRW/2);
		pball.y=(SCRH/2);
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
		if(pball.x < (SCRW/2) && vball.x < 0) {
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
			if (pplayer1.y < (SCRH/2)) {
				if(((SCRH/2)-pplayer1.y) < 3)
					pplayer1.y++;
				else
					pplayer1.y+=3;
			}
			else if(pplayer1.y > (SCRH/2))
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
	else if(pplayer2.y>(SCRH-20))
		pplayer2.y=(SCRH-20);
	if(pplayer1.y<10)
		pplayer1.y=10;
	else if(pplayer1.y>(SCRH-20))
		pplayer1.y=(SCRH-20);
	if(!gameover)
		draw_pong();
	return ret;
}


void new_pong_window()
{
	pong_gc = pz_get_gc(1);
	GrSetGCUseBackground(pong_gc, GR_FALSE);
	GrSetGCForeground(pong_gc, BLACK);

	pong_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), pong_do_draw, pong_do_keystroke);

	GrSelectEvents(pong_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_TIMER);

	GrMapWindow(pong_wid);
	timer_id = GrCreateTimer(pong_wid, 75); /*Create nano-x timer*/
}

