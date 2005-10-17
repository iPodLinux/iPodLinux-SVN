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

#define DIV 3
#define MULT 7
#define SUB 11
#define ADD 15

#define CHEIGHT	((cbh*5)+(cygap << 2))
#define CWIDTH	((cbw << 2)+(cxgap*3))
#define CXOFF	((screen_info.cols-CWIDTH) >> 1)
#define CYOFF	(((screen_info.rows-(HEADER_TOPLINE+1))-CHEIGHT) >> 1)
#define CBYOFF	(CYOFF+cbh+cygap)

static void draw_calc() {
	GR_SIZE width, height, base;

	GrSetGCForeground(calc_gc, appearance_get_color(CS_FG));
	GrRect(calc_wid, calc_gc, CXOFF, CYOFF, CWIDTH, cbh);
	GrGetGCTextSize(calc_gc, mathloc, -1, GR_TFASCII, &width, &height,
			&base);
	GrSetGCForeground(calc_gc, appearance_get_color(CS_BG));
	GrFillRect(calc_wid, calc_gc, CXOFF+1, CYOFF+1, CWIDTH-2, cbh-2);
	GrSetGCForeground(calc_gc, appearance_get_color(CS_FG));
	GrText(calc_wid, calc_gc, CXOFF+(CWIDTH-width-4),
		CYOFF+((cbh/2)-(height/2)), mathloc, -1, GR_TFASCII|GR_TFTOP);

	if(current_calc_item < 0)
		current_calc_item = 15;
	else if(current_calc_item >= 16)
		current_calc_item = 0;
		
	xlocal=CXOFF+((cbw+cxgap)*(current_calc_item%4));
	ylocal=CBYOFF+((cbh+cygap)*(current_calc_item >> 2));
	lastxlocal=CXOFF+((cbw+cxgap)*(last_calc_item%4));
	lastylocal=CBYOFF+((cbh+cygap)*(last_calc_item >> 2));
	
	GrSetGCForeground(calc_gc, appearance_get_color(CS_BG));
	GrFillRect(calc_wid, calc_gc, lastxlocal+1, lastylocal+1, cbw-2, cbh-2);
	GrSetGCForeground(calc_gc, appearance_get_color(CS_FG));
	GrGetGCTextSize(calc_gc, num[last_calc_item], -1, GR_TFASCII,
	                &width, &height, &base);
	GrText(calc_wid, calc_gc,
	       lastxlocal+((cbw/2)-(width/2)), lastylocal+((cbh/2)-(height/2)),
	       num[last_calc_item], -1, GR_TFASCII|GR_TFTOP);
	GrSetGCForeground(calc_gc, appearance_get_color(CS_SELBG));
	GrFillRect(calc_wid, calc_gc, xlocal, ylocal, cbw, cbh);
	GrSetGCForeground(calc_gc, appearance_get_color(CS_SELFG));
	GrGetGCTextSize(calc_gc, num[current_calc_item], -1, GR_TFASCII,
	                &width, &height, &base);
	GrText(calc_wid, calc_gc,
	       xlocal+((cbw/2)-(width/2)), ylocal+((cbh/2)-(height/2)),
	       num[current_calc_item], -1, GR_TFASCII|GR_TFTOP);
}

void calc_do_math(int pos) {
	static int opr, tog;
	if((pos==DIV || pos==MULT || pos==SUB || pos>13)&&mathloc[0]!='\0') {
		/* convert string to double, store */
		if(numfull) {
			if(opr==DIV) /* divide */
				sprintf(mathloc, "%g", oldnum/atof(mathloc));
			else if(opr==MULT) /* multiply */
				sprintf(mathloc, "%g", oldnum*atof(mathloc));
			else if(opr==SUB) /* subtract */
				sprintf(mathloc, "%g", oldnum-atof(mathloc));
			else if(opr==ADD) /* add */
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
		/* add number to string */
		if(littr<16)
			sprintf(mathloc, "%s%s", mathloc, num[pos]);
	}

}

static void calc_do_draw() {
	int i;
	GR_SIZE width, height, base;
	pz_draw_header(_("Calculator"));
	GrSetGCForeground(calc_gc, appearance_get_color(CS_FG));
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
		case IPOD_BUTTON_ACTION:
		case '\n':
			calc_do_math(current_calc_item);
			draw_calc();
			ret |= KEY_CLICK;
			break;

		case IPOD_WHEEL_COUNTERCLOCKWISE:
			last_calc_item = current_calc_item;
			current_calc_item--;
			draw_calc();
			ret |= KEY_CLICK;
			break;

		case IPOD_WHEEL_CLOCKWISE:
			last_calc_item = current_calc_item;
			current_calc_item++;
			draw_calc();
			ret |= KEY_CLICK;
			break;

		case IPOD_BUTTON_MENU:
			GrDestroyGC(calc_gc);
			pz_close_window(calc_wid);
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
	GrSetGCForeground(calc_gc, appearance_get_color(CS_FG));

	mathloc[0]='\0';
	numfull = 0;
	littr = 0;

	if (screen_info.cols < 160) { /* mini */
		cxgap = 3; cygap = 2;
		cbw = 22; cbh = 15;
	}

	calc_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1), calc_do_draw,
			calc_do_keystroke);
	GrSetWindowBackgroundColor(calc_wid, appearance_get_color(CS_BG));

	GrSelectEvents(calc_wid, GR_EVENT_MASK_EXPOSURE |
			GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(calc_wid);
}
