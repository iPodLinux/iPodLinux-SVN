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
#include <string.h>
#include <ctype.h>

#include "pz.h"
#include "ipod.h"

#define BUFSIZR 2048

static GR_WINDOW_ID about_wid;
static GR_WINDOW_ID about_bottom_wid;
static GR_GC_ID about_gc_black;
static GR_SCREEN_INFO screen_info;

char kern[BUFSIZR], gen[BUFSIZR], fstype[4];
int page=0, reltop=20, j;

int GrTextEx(GR_WINDOW_ID textwid,  GR_GC_ID textgc, char *buf, int x, int y, int w, int h, int s) {
	GR_SIZE width, height, base;
	int i=0;
	char *ptr, *optr, lsw='*';

	while(1) {
		GrGetGCTextSize(textgc, buf, -1, GR_TFASCII, &width, &height, &base);
		if(width<=w || (!strchr(buf, ' ') && !strchr(buf, '\t'))) {
			GrText(about_wid, about_gc_black, x, y+(i++*s), buf, -1, GR_TFASCII);
			if(lsw!='*')
				*ptr=lsw;
			else
				break;
			buf=ptr+1;
			lsw='*';
		}
		else {
			if(lsw!='*')
				optr=ptr;
			if(!(ptr=strchr(buf, '\n')))
				ptr=strrchr(buf, ' ');
			if(lsw!='*')
				*optr=lsw;
			lsw=*ptr;
			*ptr='\0';
		}
	}
	return i;
}

static void about_switch_window() {
	GR_SIZE width, height, base;
	about_bottom_wid = GrNewWindowEx(GR_WM_PROPS_APPFRAME |
			    GR_WM_PROPS_CAPTION |
			    GR_WM_PROPS_CLOSEBOX,
			    "About_Bottom",
			    GR_ROOT_WINDOW_ID,
			    0, screen_info.rows-15, screen_info.cols, 15, WHITE);

	GrSelectEvents(about_bottom_wid, GR_EVENT_MASK_EXPOSURE);
	GrMapWindow(about_bottom_wid);
	GrGetGCTextSize(about_gc_black, "Press Action for Credits/Info", -1, GR_TFASCII, &width, &height, &base);
	GrSetGCForeground(about_gc_black, BLACK);
	GrLine(about_bottom_wid, about_gc_black, 0, 0, screen_info.cols, 0);
	GrText(about_bottom_wid, about_gc_black, (screen_info.cols-width)/2, 12, "Press Action for Credits/Info", -1, GR_TFASCII);
}


static void draw_about() {
	int i;
	GR_SIZE width, height, base;
	char *ptr, ipodgen[18], ipodrev[32];
	char *cnames[]={"Bernard Leach", "Matthew J. Sahagian", "Courtney Cavin", "matz-josh", "Matthis Rouch", "ansi", "Jens Taprogge", "Fredrik Bergholtz", "\0"};

	GrSetGCForeground(about_gc_black, WHITE);
	GrFillRect(about_wid, about_gc_black, 6, 0, screen_info.cols, screen_info.rows);
	GrSetGCForeground(about_gc_black, BLACK);
	GrFillRect(about_wid, about_gc_black, 2, 0, 4, screen_info.rows);

	j=0;

	if(!page) {

		GrText(about_wid, about_gc_black, 8, 15, PZ_VER, -1, GR_TFASCII);

		j=GrTextEx(about_wid, about_gc_black, kern, 8, 35, screen_info.cols-16, screen_info.rows-55, 15);

		for(i=0; !isspace(gen[i]); i++);
		for(; isspace(gen[i]); i++);
		ptr=gen+i+2;
		sprintf(ipodgen, "%s iPod.  Gen. %c", fstype, *ptr);
		GrText(about_wid, about_gc_black, 8, 40+(15*j++), ipodgen, -1, GR_TFASCII);
		sprintf(ipodrev, "Rev. %s", ptr);
		GrText(about_wid, about_gc_black, 8, 40+(15*j++), ipodrev, -1, GR_TFASCII);
	}
	else {
		GrText(about_wid, about_gc_black, 8, reltop+(15*j++), "Brought to you by:", -1, GR_TFASCII);
		for(i=0; cnames[i]!="\0"; i++)
			GrText(about_wid, about_gc_black, 17, reltop+(15*j++), cnames[i], -1, GR_TFASCII);
		GrText(about_wid, about_gc_black, 8, reltop+(15*++j), "http://www.ipodlinux.org", -1, GR_TFASCII);
	}
	GrSetGCForeground(about_gc_black, WHITE);
	GrFillRect(about_wid, about_gc_black, 0, screen_info.rows-34, screen_info.cols, 14);
	GrGetGCTextSize(about_gc_black, "Press Action for Credits/Info", -1, GR_TFASCII, &width, &height, &base);
	GrSetGCForeground(about_gc_black, BLACK);
	GrLine(about_wid, about_gc_black, 0, screen_info.rows-35, screen_info.cols, screen_info.rows-35);
	GrText(about_wid, about_gc_black, (screen_info.cols-width)/2, screen_info.rows-23, "Press Action for Credits/Info", -1, GR_TFASCII);
}

static void about_start_draw() {
	char fslist[BUFSIZR], fsmount[11];
	FILE *ptr;

	pz_draw_header("About");

	if((ptr = popen("uname -rv", "r")) != NULL) {
		while(fgets(kern, BUFSIZR, ptr)!=NULL);
		pclose(ptr);
	}

#ifdef IPOD
	if((ptr = fopen("/proc/cpuinfo", "r")) != NULL) {
		while(fgets(gen, BUFSIZR, ptr)!=NULL)
			if(strncmp(gen, "Revision", 8)==0)
				break;
		fclose(ptr);
	}
#else
	strcpy(gen, "Revision     : 30001");
#endif

	if((ptr = fopen("/proc/mounts", "r")) != NULL) {
		while(fgets(fslist, BUFSIZR, ptr)!=NULL) {
			sscanf(fslist, "%s%*s%s", fsmount, fstype);
			if(strcmp(fsmount, "/dev/root")==0)
				break;
		}
		if(strncmp(fstype, "hfs", 3)==0)
			sprintf(fstype, "Mac");
		else
			sprintf(fstype, "Win");
		fclose(ptr);
	}

	draw_about();
}

static int about_parse_keystroke(GR_EVENT * event) {
	int ret=1;
	switch (event->keystroke.ch) {
	case '\r':
	case '\n':
		page=!page;
		draw_about();
		break;
	case 'l':
		if(page) {
			reltop+=3;
			if(reltop>20)
				reltop=20;
			draw_about();
		}
		break;
	case 'r':
		if(page) {
			reltop-=3;
			if(reltop<(j+1)*15-screen_info.cols-40)
				reltop=(j+1)*15-screen_info.cols-40;
			draw_about();
		}
		break;
	case 'm':
		GrUnmapWindow(about_bottom_wid);
		GrDestroyWindow(about_bottom_wid);
		pz_close_window(about_wid);
		break;
	default:
		ret=0;
	}
	return ret;
}

void about_window() {
	GrGetScreenInfo(&screen_info);

	about_gc_black = GrNewGC();
	GrSetGCUseBackground(about_gc_black, GR_TRUE);
	GrSetGCBackground(about_gc_black, WHITE);
	GrSetGCForeground(about_gc_black, BLACK);

	about_switch_window();
	about_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1) - 15, about_start_draw, about_parse_keystroke);

	GrSelectEvents(about_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(about_wid);
}
