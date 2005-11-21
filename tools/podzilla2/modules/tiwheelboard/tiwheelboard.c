/*
 * Copyright (C) 2005 Stefan Lange-Hegermann (BlackMac)
 * Modifications by Jonathan Bettencourt (jonrelay)
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

/*
 *  /!\ Warning! This is NOT the same as legacy wheelboard.c!
 */

#include "pz.h"
#include <string.h>

static PzModule * module;

const char ti_wlb_lowercase[]=  "abcdefghijklmnopqrstuvwxyz";
const char ti_wlb_uppercase[]=  "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char ti_wlb_numeric1[]=   "0123456789.,/!@#$%^&*()[]?";
const char ti_wlb_numeric2[]=   "~`+-={}<>|:;\\\'\"_£¢´µÖ©¨ÁÀ¤";
const char ti_wlb_accentlc[]=   "ˆ‡‰ŠŒ‹Ž‘“’”•˜—™š¿›œžŸ–Ø";
const char ti_wlb_accentuc[]=   "Ëçå€Ìéƒæèíêëìñîï…¯Íôòó†„§";
const char ti_wlb_numericmode[]="0123456789.-E01234567+-*/^";
const char ti_wlb_cursormode[] ="Move         Cursor       ";
static int ti_wlb_cset = 0;
static int ti_wlb_numeric = 0;
static char ti_wlb_charset[27];
static int ti_wlb_startpos=0;
static int ti_wlb_limit=13;
static int ti_wlb_position=3;

extern TWidget * ti_create_tim_widget(int ht, int wd);

void ti_wlb_reset(void)
{
	ti_wlb_cset = 0;
	if (ti_wlb_numeric) {
		strcpy(ti_wlb_charset, ti_wlb_numericmode);
	} else {
		strcpy(ti_wlb_charset, ti_wlb_lowercase);
	}
	ti_wlb_startpos=0;
	ti_wlb_limit=13;
	ti_wlb_position=3;
}

void ti_wlb_switch_cset(void)
{
	ti_wlb_limit=13;
	ti_wlb_position=3;
	ti_wlb_startpos=0;
	if (ti_wlb_numeric) {
		if (ti_wlb_cset) {
			strcpy(ti_wlb_charset, ti_wlb_numericmode);
			ti_wlb_cset = 0;
		} else {
			strcpy(ti_wlb_charset, ti_wlb_cursormode);
			ti_wlb_cset = -1;
		}
	} else {
		ti_wlb_cset++;
		if (ti_wlb_cset >= 6) { ti_wlb_cset = -1; }
		switch (ti_wlb_cset) {
		case 0:
			strcpy(ti_wlb_charset, ti_wlb_lowercase);
			break;
		case 1:
			strcpy(ti_wlb_charset, ti_wlb_uppercase);
			break;
		case 2:
			strcpy(ti_wlb_charset, ti_wlb_numeric1);
			break;
		case 3:
			strcpy(ti_wlb_charset, ti_wlb_numeric2);
			break;
		case 4:
			strcpy(ti_wlb_charset, ti_wlb_accentlc);
			break;
		case 5:
			strcpy(ti_wlb_charset, ti_wlb_accentuc);
			break;
		case -1:
			strcpy(ti_wlb_charset, ti_wlb_cursormode);
			break;
		}
	}
}

void ti_wlb_advance_level(int i)
{
	int add;
	ti_wlb_position=3;
	if (ti_wlb_limit%2!=0) 
		add=1;
	else
		add=0;
		
	if (ti_wlb_limit>1) {
		if (i==1) {
			ti_wlb_startpos=ti_wlb_startpos+(ti_wlb_limit)-add;
		}
		ti_wlb_limit=(ti_wlb_limit)/2+add;
	} else {
		if (i==1) {
			ttk_input_char(ti_wlb_charset[ti_wlb_startpos+1]);
		} else {
			ttk_input_char(ti_wlb_charset[ti_wlb_startpos]);
		}
		ti_wlb_limit=13;
		ti_wlb_startpos=0;
	}
	
}

void ti_wlb_draw(TWidget * wid, ttk_surface srf)
{
	ttk_color tcol;
	char disp[14];
    ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(0));
    
    if (ti_wlb_position<1) {
    	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w/2, wid->y+wid->h, ti_ap_get(2));
    	tcol = ti_ap_get(3);
    } else {
    	tcol = ti_ap_get(1);
    }
    
    strncpy(disp, &ti_wlb_charset[ti_wlb_startpos], ti_wlb_limit);
    disp[ti_wlb_limit]=0;
    ttk_text(srf, ttk_menufont, wid->x+1, wid->y+8-ttk_text_height(ttk_menufont)/2, tcol, disp);
           
	if (ti_wlb_position>5) {
    	ttk_fillrect(srf, wid->x+wid->w/2, wid->y, wid->x+wid->w, wid->y+wid->h, ti_ap_get(2));
    	tcol = ti_ap_get(3);
    } else {
    	tcol = ti_ap_get(1);
    }
	strncpy(disp, &ti_wlb_charset[ti_wlb_limit+ti_wlb_startpos], ti_wlb_limit);
    disp[ti_wlb_limit]=0;
    ttk_text(srf, ttk_menufont, wid->x+wid->w/2+1, wid->y+8-ttk_text_height(ttk_menufont)/2, tcol, disp);
    
    /* Position */
    ttk_line(srf, wid->x+wid->w/2, wid->y+0, wid->x+((ti_wlb_position*wid->w)/6), wid->y+0, ttk_makecol(GREY));
    ttk_line(srf, wid->x+wid->w/2, wid->y+1, wid->x+((ti_wlb_position*wid->w)/6), wid->y+1, ttk_makecol(GREY));
    ttk_line(srf, wid->x+wid->w/2, wid->y+14, wid->x+((ti_wlb_position*wid->w)/6), wid->y+14, ttk_makecol(GREY));
    ttk_line(srf, wid->x+wid->w/2, wid->y+15, wid->x+((ti_wlb_position*wid->w)/6), wid->y+15, ttk_makecol(GREY));
}

int ti_wlb_down(TWidget * wid, int btn)
{
    switch (btn)
    {
    case TTK_BUTTON_ACTION:
    	ti_wlb_switch_cset();
    	wid->dirty=1;
        break;
    case TTK_BUTTON_PLAY:
    	ttk_input_char(TTK_INPUT_ENTER);
        break;
    case TTK_BUTTON_PREVIOUS:
    	ttk_input_char(TTK_INPUT_BKSP);
        break;
    case TTK_BUTTON_NEXT:
    	ttk_input_char(32);
        break;
    case TTK_BUTTON_MENU:
    	ttk_input_end();
        break;
    default:
    	return TTK_EV_UNUSED;
        break;
    }
    return TTK_EV_CLICK;
}

int ti_wlb_scroll(TWidget * wid, int dir)
{
	TTK_SCROLLMOD (dir,4)
	
	if (dir<0) {
    	if (ti_wlb_cset<0) {
    		ttk_input_char(TTK_INPUT_LEFT);
    	} else {
        	if ((ti_wlb_position--)==0) {
        		ti_wlb_advance_level(0);
        	}
        	wid->dirty=1;
        }
	} else {
    	if (ti_wlb_cset<0) {
    		ttk_input_char(TTK_INPUT_RIGHT);
    	} else {
        	if ((ti_wlb_position++)==6) {
    			ti_wlb_advance_level(1);
        	}
    		wid->dirty=1;
        }
	}
	return TTK_EV_CLICK;
}

TWidget * ti_wlb_create(int n)
{
	TWidget * wid = ti_create_tim_widget(15, 0);
	wid->draw = ti_wlb_draw;
	wid->down = ti_wlb_down;
	wid->scroll = ti_wlb_scroll;
	ti_wlb_numeric = n;
	ti_wlb_reset();
	return wid;
}

TWidget * ti_wlb_screate()
{
	return ti_wlb_create(0);
}

TWidget * ti_wlb_ncreate()
{
	return ti_wlb_create(1);
}

void ti_wlb_init()
{
	module = pz_register_module("tiwheelboard", 0);
	ti_register(ti_wlb_screate, ti_wlb_ncreate, "Wheelboard", 6);
}

PZ_MOD_INIT(ti_wlb_init)

