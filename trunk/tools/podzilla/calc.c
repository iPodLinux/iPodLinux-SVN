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

int xlocal,ylocal,lastxlocal,lastylocal;
char mathloc[16];
double oldnum;
int numfull, littr;

static int current_calc_item = 0;
static int last_calc_item;
char *num[]={"7","8","9","/","4","5","6","*","1","2","3","-","0",".","=","+"};

static int cxgap = 5, cygap = 4;
static int cbw = 25, cbh = 16;

#define CHEIGHT	((5*cbh)+(4*cygap))
#define CWIDTH	((4*cbw)+(3*cxgap))
#define CXOFF	((screen_info.cols-CWIDTH)/2)
#define CYOFF	(((screen_info.rows-(HEADER_TOPLINE+1))-CHEIGHT)/2)
#define CBYOFF	(CYOFF+cbh+cygap)

static void draw_calc() {
	GR_SIZE width, height, base;

	GrSetGCUseBackground(calc_gc, GR_FALSE);
	GrSetGCForeground(calc_gc, BLACK);
	GrRect(calc_wid, calc_gc, CXOFF, CYOFF, CWIDTH, cbh);
	GrGetGCTextSize(calc_gc, mathloc, -1, GR_TFASCII, &width, &height, &base);
	GrSetGCForeground(calc_gc, WHITE);
	GrFillRect(calc_wid, calc_gc, CXOFF+1, CYOFF+1, CWIDTH-2, cbh-2);
	GrSetGCForeground(calc_gc, BLACK);
	GrText(calc_wid, calc_gc, CXOFF+(CWIDTH-width-4),
		CYOFF+((cbh/2)-(height/2)), mathloc, -1, GR_TFASCII|GR_TFTOP);

	if(current_calc_item < 0)
		current_calc_item = 15;
	else if(current_calc_item >= 16)
		current_calc_item = 0;
		
	xlocal=CXOFF+((cbw+cxgap)*(current_calc_item%4));
	ylocal=CBYOFF+((cbh+cygap)*(current_calc_item/4));
	lastxlocal=CXOFF+((cbw+cxgap)*(last_calc_item%4));
	lastylocal=CBYOFF+((cbh+cygap)*(last_calc_item/4));
	
	GrSetGCForeground(calc_gc, WHITE);
	GrFillRect(calc_wid, calc_gc, lastxlocal+1, lastylocal+1, cbw-2, cbh-2);
	GrSetGCForeground(calc_gc, BLACK);
	GrGetGCTextSize(calc_gc, num[last_calc_item], -1, GR_TFASCII,
	                &width, &height, &base);
	GrText(calc_wid, calc_gc,
	       lastxlocal+((cbw/2)-(width/2)), lastylocal+((cbh/2)-(height/2)),
	       num[last_calc_item], -1, GR_TFASCII|GR_TFTOP);
	GrFillRect(calc_wid, calc_gc, xlocal, ylocal, cbw, cbh);
	GrSetGCForeground(calc_gc, WHITE);
	GrGetGCTextSize(calc_gc, num[current_calc_item], -1, GR_TFASCII,
	                &width, &height, &base);
	GrText(calc_wid, calc_gc,
	       xlocal+((cbw/2)-(width/2)), ylocal+((cbh/2)-(height/2)),
	       num[current_calc_item], -1, GR_TFASCII|GR_TFTOP);
	GrSetGCUseBackground(calc_gc, GR_FALSE);
	GrSetGCMode(calc_gc, GR_MODE_SET);
}

void calc_do_math(int pos) {
	static int opr, tog;
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
			sprintf(mathloc, "%s%s", mathloc, num[pos]);
	}

}

static void calc_do_draw() {
	int i;
	GR_SIZE width, height, base;
	pz_draw_header("Calculator");
	mathloc[0]='\0';
	numfull = 0;
	littr = 0;
	GrSetGCForeground(calc_gc, BLACK);
	for(i=0; i<=15; i++) {
		GrGetGCTextSize(calc_gc, num[i], -1, GR_TFASCII,
		                &width, &height, &base);
		GrRect(calc_wid, calc_gc,CXOFF+((cbw+cxgap)*(i%4)), 
		       CBYOFF+((cbh+cygap)*(i/4)), cbw, cbh);
		GrText(calc_wid, calc_gc,
		       CXOFF+((cbw+cxgap)*(i%4))+((cbw/2)-(width/2)),
		       CBYOFF+((cbh+cygap)*(i/4))+((cbh/2)-(height/2)),
		       num[i], -1, GR_TFASCII|GR_TFTOP);
	}
	draw_calc();
}

static int calc_do_keystroke(GR_EVENT * event) {
	int ret = 0;

	switch(event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			calc_do_math(current_calc_item);
			draw_calc();
			ret |= KEY_CLICK;
			break;

		case 'l':
			last_calc_item = current_calc_item;
			current_calc_item--;
			draw_calc();
			ret |= KEY_CLICK;
			break;

		case 'r':
			last_calc_item = current_calc_item;
			current_calc_item++;
			draw_calc();
			ret |= KEY_CLICK;
			break;

		case 'm':
			GrDestroyGC(calc_gc);
			GrUnmapWindow(calc_wid);
			GrDestroyWindow(calc_wid);
			ret |= KEY_CLICK;
			break;

		default:
			ret |= KEY_UNUSED;
		}
		break;

	default:
		ret |= EVENT_UNUSED;
		break;
	}
	return ret;
}

void new_calc_window() {
	calc_gc = pz_get_gc(1);
	GrSetGCUseBackground(calc_gc, GR_FALSE);
	GrSetGCForeground(calc_gc, BLACK);

	if (screen_info.cols < 160) { //mini
		cxgap = 3; cygap = 2;
		cbw = 22; cbh = 15;
	}

	calc_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), calc_do_draw, calc_do_keystroke);

	GrSelectEvents(calc_wid, GR_EVENT_MASK_EXPOSURE |
			GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(calc_wid);
}
