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
//#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "pz.h"
#include "ipod.h"
//#include "piezo.c"

extern void new_browser_window(void);
extern void toggle_backlight(void);

static GR_WINDOW_ID oth_wid;
static GR_GC_ID oth_gc;
static GR_SCREEN_INFO screen_info;
int xlocal,ylocal,lastxlocal,lastylocal;

void quit_podzilla(void);
void reboot_ipod(void);
static void oth_set_piece(int pos, int coloresq);
   
static int current_oth_item = 19;
static int last_current_oth_item;
static int status[64];
static int testb[64];
static int over = 0;
/*typedef struct piece PIECE;
struct piece {
	int piecex;
	int piecey;
};*/

static void draw_oth()
{
	GR_SIZE width, height, base;

	GrGetGCTextSize(oth_gc, "M", -1, GR_TFASCII, &width, &height, &base);

	GrSetGCUseBackground(oth_gc, GR_FALSE);
	GrSetGCForeground(oth_gc, WHITE);
	GrLine(oth_wid, oth_gc, 33, 8, 127, 8);
	GrLine(oth_wid, oth_gc, 32, 20, 128, 20);
	GrLine(oth_wid, oth_gc, 32, 32, 128, 32);
	GrLine(oth_wid, oth_gc, 32, 44, 128, 44);
	GrLine(oth_wid, oth_gc, 32, 56, 128, 56);
	GrLine(oth_wid, oth_gc, 32, 68, 128, 68);
	GrLine(oth_wid, oth_gc, 32, 80, 128, 80);
	GrLine(oth_wid, oth_gc, 32, 92, 128, 92);
	GrLine(oth_wid, oth_gc, 33, 104, 127, 104);
	
	GrLine(oth_wid, oth_gc, 32, 9, 32, 103);
	GrLine(oth_wid, oth_gc, 44, 8, 44, 104);
	GrLine(oth_wid, oth_gc, 56, 8, 56, 104);
	GrLine(oth_wid, oth_gc, 68, 8, 68, 104);
	GrLine(oth_wid, oth_gc, 80, 8, 80, 104);
	GrLine(oth_wid, oth_gc, 92, 8, 92, 104);
	GrLine(oth_wid, oth_gc, 104, 8, 104, 104);
	GrLine(oth_wid, oth_gc, 116, 8, 116, 104);
	GrLine(oth_wid, oth_gc, 128, 9, 128, 103);
	GrSetGCForeground(oth_gc, WHITE);
	GrSetGCUseBackground(oth_gc, GR_TRUE);
	
	if(current_oth_item < 0)
		current_oth_item = 63;
	else if(current_oth_item >= 64)
		current_oth_item = 0;
	xlocal=current_oth_item * 12 + 32 - 96*(int)(current_oth_item/8);
	ylocal=8+12*(int)(current_oth_item/8);
	lastxlocal=last_current_oth_item * 12 + 32 - 96*(int)(last_current_oth_item/8);
	lastylocal=8+12*(int)(last_current_oth_item/8);
	GrRect(oth_wid, oth_gc, xlocal,ylocal, 12, 12); 

	GrSetGCForeground(oth_gc, BLACK);
	if(current_oth_item != last_current_oth_item) {
		GrLine(oth_wid, oth_gc, lastxlocal+1,lastylocal+11,lastxlocal+11,lastylocal+11);
		GrLine(oth_wid, oth_gc, lastxlocal+11,lastylocal+11,lastxlocal+11,lastylocal+1);
		if(status[last_current_oth_item] != 3)
			oth_set_piece(last_current_oth_item, status[last_current_oth_item]);
	}

	GrSetGCMode(oth_gc, GR_MODE_SET);
}

static void oth_set_piece(int pos, int coloresq)
{
	GR_POINT cheese[] = {
		{pos*12 +33 -96*(int)(pos/8), 14 +12*(int)(pos/8)},
		{pos*12 +38 -96*(int)(pos/8), 9 +12*(int)(pos/8)},
		{pos*12 +43 -96*(int)(pos/8), 14 +12*(int)(pos/8)},
		{pos*12 +38 -96*(int)(pos/8), 19 +12*(int)(pos/8)},
		{pos*12 +33 -96*(int)(pos/8), 14 +12*(int)(pos/8)}
	};
	status[pos] = coloresq;
	GrSetGCForeground(oth_gc, WHITE);
	if(coloresq==0)
		GrFillPoly(oth_wid, oth_gc, 5, cheese);
	else if(coloresq==1) {
		GrSetGCForeground(oth_gc, BLACK);
		GrFillPoly(oth_wid, oth_gc, 5, cheese);
		GrSetGCForeground(oth_gc, WHITE);
		GrPoly(oth_wid, oth_gc, 5, cheese);
	}
}

static void oth_do_draw()
{
	int i;
	pz_draw_header("Othello");
	for (i=0;i<64;i++) {
		status[i]=3;
	}
	over = 0;
	current_oth_item = 19;
	draw_oth();
	oth_set_piece(27, 1);
	oth_set_piece(28, 0);
	oth_set_piece(35, 0);
	oth_set_piece(36, 1);
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
	for(i=0;i<64;i++) {
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
	GrSetGCForeground(oth_gc, BLACK);
	GrFillRect(oth_wid, oth_gc, 32, 8, 97, 97);
	GrSetGCForeground(oth_gc, WHITE);
	sprintf(comp, "Me: %d", computer);
	sprintf(hum, "You: %d", human);
	GrText(oth_wid, oth_gc, 35, 32, comp, -1, GR_TFASCII);
	GrText(oth_wid, oth_gc, 80, 32, hum, -1, GR_TFASCII);

	if(human>computer) {
		GrText(oth_wid, oth_gc, 8, 56, win[s], -1, GR_TFASCII);
		over=1;
		printf("\n\nCongratulations! You beat the computer %d to %d !\n\n",human,computer);
		
	}
	else if(computer>human) {
		GrText(oth_wid, oth_gc, 8, 56, lose[s], -1, GR_TFASCII);
		over=1;
		printf("\n\nDoh! The computer beat you %d to %d!\n\n",computer,human);
		printf("%s %s", comp,hum);
	}
	else {
		GrText(oth_wid, oth_gc, 8, 56, tie[s], -1, GR_TFASCII);
		over=1;
		printf("Amazing! You tied the computer! This is a very uncommon event!\n");
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
	while((xy<64) && (xy>=0) && found_end=='N') {
		if(status[xy]==side)
			found_end='Y';
		else if(status[xy]==3)
			break;
		else 
			pieces++;
		if(xy==0||xy==8||xy==16||xy==24||xy==32||xy==40||xy==48||xy==56) {
			if(dx==-1)
				break;
		}
		else if(xy==7||xy==15||xy==23||xy==31||xy==39||xy==47||xy==55||xy==63) {
			if(dx==1)
				break;
		}
		xy+=dx;
		xy+=(dy*8);
		
	}
	if(found_end=='Y') {
		if(execute=='Y') {
			while(xy!=oxy) {
				xy-=dx;
				xy-=(dy*8);
				if(test != 'Y')
					oth_set_piece(xy, side);
				else
					testb[xy]=side;
			}
		}
		return pieces;
	}
	return 0;
}


static int validmove(int xy,char test,int side,char execute) {
	int opp;
	int pieces=0;

	if(side==0) opp=1;
	if(side==1) opp=0;

	if(status[xy] != 3)
		return 0;

	if(xy>0 && xy!=8  && xy!=16 && xy!=24 && xy!=32 && xy!=40 && xy!=48 && xy!=56) {
		if(xy>7)
			if(status[xy-9]==opp)
				pieces+=testmove(xy,test,side,-1,-1,execute);
		if(status[xy-1]==opp)
			pieces+=testmove(xy,test,side,-1,0,execute);
		if(xy<56)
			if(status[xy+7]==opp)
				pieces+=testmove(xy,test,side,-1,1,execute);
	}
	if(xy>7)
		if(status[xy-8]==opp)
			pieces+=testmove(xy,test,side,0,-1,execute);
	if(xy<56)
		if(status[xy+8]==opp)
			pieces+=testmove(xy,test,side,0,1,execute);
	if(xy!=7 && xy!=15 && xy!=23 && xy!=31 && xy!=39 && xy!=47 && xy!=55 && xy!=63) {
		if(xy>7)
			if(status[xy-7]==opp)
				pieces+=testmove(xy,test,side,1,-1,execute);
		if(status[xy+1]==opp)
			pieces+=testmove(xy,test,side,1,0,execute);
		if(xy<56)
			if(status[xy+9]==opp)
				pieces+=testmove(xy,test,side,1,1,execute);
	}
	return pieces;
}

static int canmove(int side) {
	int i; 
	for(i=0;i<64;i++)
		if(status[i] == 3)
			if(validmove(i,'N',side,'N'))
				return 1;
	return 0;
}

static float movevalue(int xy,int side,int depth) {
	int opp;
	int i,pieces,maxpieces=-500,nmoves=0;
	int oxy;
	float value;

	if(side==0) opp=1;
	if(side==1) opp=0;

	/* copy the board */
	for(i=0;i<64;i++)
		testb[i]=status[i];

	/* play the space */
	value = (float)validmove(xy,'Y',side,'Y');
	if(xy == 0 || xy == 7 || xy == 56 || xy == 63)
		value+=7;
	if(xy == 8 || xy == 16 || xy == 24 || xy == 32 || xy == 40 || xy == 48 || xy == 15 || xy == 23 || xy == 31 || xy == 39 || xy == 47 || xy == 55)
		value +=3;

	/* assume an immediately optimal opponent and find best move */
	for(i=0;i<64;i++)
		if(status[i]==3)
			if((pieces=validmove(xy,'Y',opp,'N')) > 0) {
				if(i == 0 || i == 7 || i == 56 || i == 63)
					pieces+=7;
				if(i == 8 || i == 16 || i == 24 || i == 32 || i == 40 || i == 48 || i == 15 || i == 23 || i == 31 || i == 39 || i == 47 || i == 55)
					pieces+=3;
				if(pieces>maxpieces) {
					maxpieces=pieces;
					oxy=i;
				}
				nmoves++;
			}

	if(nmoves)
		value -= (float)(validmove(oxy,'Y',opp,'Y')-1);
	else {
		#ifdef VERBOSE
			printf(" [calc] no possible opponent moves!\n");
		#endif
		value += 1;
	}
	#ifdef VERBOSE
		printf(" [calc] %c%d = %f\n",x+'A',y+1,value);
	#endif
	return value;
}

static void computermove(int side) {
	int i,mxy;
	int pieces;
	float value,maxvalue=-1000000;
	for(i=0;i<64;i++)
		if(status[i]==3)
			if((pieces=validmove(i,'N',side,'N')) > 0) {
				value=movevalue(i,side,1);
				if(value>maxvalue) {
					maxvalue=value;
					mxy=i;
				}
			}
	pieces=validmove(mxy,'N',side,'Y');
	//printf("I moved at %d and captured %d of your pieces!\n",mxy,pieces);
}

static int oth_do_keystroke(GR_EVENT * event)
{
	static int rcount = 0;
	static int lcount = 0;
	int ret = 0;

	if(!over) {
		if(canmove(0)) {
			/*keystrokes during gameplay*/
			switch (event->keystroke.ch) {
				case '\r':
					if(validmove(current_oth_item, 'N', 0, 'Y') > 0) {
						if(!over)
							computermove(1);
					}
					ret = 1;
					break;
			case 'l':
				lcount++;
				if (lcount < 1) {
					break;
				}
				lcount = 0;
				last_current_oth_item = current_oth_item;
				current_oth_item--;
				while(validmove(current_oth_item,'N',0,'N') == 0) {
				//while(status[current_oth_item] != 3) {
					current_oth_item--;
					if(current_oth_item < 0)
						current_oth_item = 63;
				}
				draw_oth();
				ret = 1;
				break;
			case 'r':
				rcount++;
				if (rcount < 1) {
					break;
				}
				rcount = 0;
				last_current_oth_item = current_oth_item;
				current_oth_item++;
				while(validmove(current_oth_item,'N',0,'N') == 0) {
				//while(status[current_oth_item] != 3) {
					current_oth_item++;
					if(current_oth_item > 63)
						current_oth_item = 0;
				}
				draw_oth();
				ret = 1;
				break;
			}
		}
		else if (canmove(1)) {
			computermove(1);
			draw_oth();
		}
		else
			endgame(1);
	}
	/*global keystrokes*/
	switch (event->keystroke.ch) {
	case 'm':
		pz_close_window(oth_wid);
		ret = 1;
		break;
	}
	return ret;
}

void new_oth_window()
{
	GrGetScreenInfo(&screen_info);

	oth_gc = GrNewGC();
	GrSetGCUseBackground(oth_gc, GR_TRUE);
	GrSetGCForeground(oth_gc, WHITE);

	oth_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), oth_do_draw, oth_do_keystroke);

	GrSelectEvents(oth_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(oth_wid);
}
