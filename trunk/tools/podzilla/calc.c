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

#include <stdio.h>
#include <stdlib.h>
#include "pz.h"

static GR_WINDOW_ID calc_wid;
static GR_GC_ID calc_gc;
static GR_SCREEN_INFO screen_info;
int xlocal,ylocal,lastxlocal,lastylocal;
char mathloc[16];
double oldnum;
int numfull;

static int current_calc_item = 0;
static int last_current_calc_item;

static void draw_calc() {
	GR_SIZE width, height, base;

	GrSetGCUseBackground(calc_gc, GR_FALSE);
	GrSetGCForeground(calc_gc, WHITE);
	GrRect(calc_wid, calc_gc, 20, 10, 115, 16);
	GrGetGCTextSize(calc_gc, mathloc, -1, GR_TFASCII, &width, &height, &base);
	GrSetGCForeground(calc_gc, BLACK);
	GrFillRect(calc_wid, calc_gc, 21, 11, 113, 14);
	GrSetGCForeground(calc_gc, WHITE);
	GrText(calc_wid, calc_gc, screen_info.cols-(width+27), 23, mathloc, -1, GR_TFASCII);

	if(current_calc_item < 0)
		current_calc_item = 15;
	else if(current_calc_item >= 16)
		current_calc_item = 0;
	xlocal=current_calc_item*30+20-120*(int)(current_calc_item/4);
	ylocal=30+20*(int)(current_calc_item/4);
	lastxlocal=last_current_calc_item*30+20-120*(int)(last_current_calc_item/4);
	lastylocal=30+20*(int)(last_current_calc_item/4);
	GrSetGCForeground(calc_gc, BLACK);
	GrFillRect(calc_wid, calc_gc, lastxlocal, lastylocal, 25, 16);
	GrSetGCForeground(calc_gc, GRAY);
	GrFillRect(calc_wid, calc_gc, xlocal, ylocal, 25, 16);
	GrSetGCForeground(calc_gc, WHITE);

	GrRect(calc_wid, calc_gc, 20, 30, 25, 16);
	GrText(calc_wid, calc_gc, 29, 43, "7", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 50, 30, 25, 16);
	GrText(calc_wid, calc_gc, 59, 43, "8", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 80, 30, 25, 16);
	GrText(calc_wid, calc_gc, 89, 43, "9", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 110, 30, 25, 16);
	GrText(calc_wid, calc_gc, 119, 43, "/", -1, GR_TFASCII);

	GrRect(calc_wid, calc_gc, 20, 50, 25, 16);
	GrText(calc_wid, calc_gc, 29, 63, "4", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 50, 50, 25, 16);
	GrText(calc_wid, calc_gc, 59, 63, "5", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 80, 50, 25, 16);
	GrText(calc_wid, calc_gc, 89, 63, "6", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 110, 50, 25, 16);
	GrText(calc_wid, calc_gc, 119, 63, "*", -1, GR_TFASCII);

	GrRect(calc_wid, calc_gc, 20, 70, 25, 16);
	GrText(calc_wid, calc_gc, 29, 83, "1", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 50, 70, 25, 16);
	GrText(calc_wid, calc_gc, 59, 83, "2", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 80, 70, 25, 16);
	GrText(calc_wid, calc_gc, 89, 83, "3", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 110, 70, 25, 16);
	GrText(calc_wid, calc_gc, 119, 77, "_", -1, GR_TFASCII);

	GrRect(calc_wid, calc_gc, 20, 90, 25, 16);
	GrText(calc_wid, calc_gc, 29, 103, "0", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 50, 90, 25, 16);
	GrText(calc_wid, calc_gc, 59, 103, ".", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 80, 90, 25, 16);
	GrText(calc_wid, calc_gc, 89, 103, "=", -1, GR_TFASCII);
	GrRect(calc_wid, calc_gc, 110, 90, 25, 16);
	GrText(calc_wid, calc_gc, 119, 103, "+", -1, GR_TFASCII);

	GrSetGCForeground(calc_gc, WHITE);
	GrSetGCUseBackground(calc_gc, GR_TRUE);
	GrSetGCMode(calc_gc, GR_MODE_SET);
}

void calc_do_math(int pos) {
	static int opr, tog, littr;
	if((pos==3 || pos==7 || pos==11 || pos>13)&&mathloc[0]!='\0') {
		//convert string to double, store
		if(numfull) {
			if(opr==3) //divide
				sprintf(mathloc, "%g", oldnum/atof(mathloc));
			else if(opr==7) //multiply
				sprintf(mathloc, "%g", oldnum*atof(mathloc));
			else if(opr==11) //subtract
				sprintf(mathloc, "%g", oldnum-atof(mathloc));
			else if(opr==15) //add
				sprintf(mathloc, "%g", oldnum+atof(mathloc));
		}
		oldnum = atof(mathloc);
		opr=pos;
		numfull=1;
		tog=1;
		littr=0;
	}
	else {
		char num[]={'7','8','9','y','4','5','6','y','1','2','3','y','0','.'};
		if(opr==14&& tog) {
			numfull=0;
			mathloc[0]='\0';
			tog=0;
		}
		if(numfull && tog) {
			mathloc[0]='\0';
			tog=0;
		}
		littr++;
		//add number to string
		if(littr<16)
			sprintf(mathloc, "%s%c", mathloc, num[pos]);
	}

}

static void calc_do_draw() {
	pz_draw_header("Calculator");
	mathloc[0]='\0';
	numfull = 0;
	draw_calc();
}

static int calc_do_keystroke(GR_EVENT * event) {
	int ret = 0;

	switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			calc_do_math(current_calc_item);
			draw_calc();
			ret = 1;
			break;
		case 'p':
			calc_do_math(14);
			draw_calc();
			break;
		case 'l':
			last_current_calc_item = current_calc_item;
			current_calc_item--;
			draw_calc();
			ret = 1;
			break;
		case 'r':

			last_current_calc_item = current_calc_item;
			current_calc_item++;
			draw_calc();
			ret = 1;
			break;
		case 'm':
			GrUnmapWindow(calc_wid);
			GrDestroyWindow(calc_wid);
			ret = 1;
			break;
	}
	return ret;
}

void new_calc_window() {
	GrGetScreenInfo(&screen_info);

	calc_gc = GrNewGC();
	GrSetGCUseBackground(calc_gc, GR_TRUE);
	GrSetGCForeground(calc_gc, WHITE);

	calc_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), calc_do_draw, calc_do_keystroke);

	GrSelectEvents(calc_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(calc_wid);
}
