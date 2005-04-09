/*
 * Copyright (C) 2004 David Carne, matz-josh
 * Copyright (C) 2005 Courtney Cavin
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pz.h"

static GR_GC_ID tv_gc;
static GR_WINDOW_ID tv_wid;
static GR_WINDOW_INFO tv_winfo;
static char *g_title;

static int last_y_top;
static unsigned int totalLines = 0;
static int lines_per_screen = 7;
static unsigned int currentLine = 0;
static char ** lineData;

void destroy_textview_window(char *msg)
{
	pz_close_window(tv_wid);
	GrDestroyGC(tv_gc);
	free(g_title);
	if(msg!=NULL)
		pz_error(msg);
}

static void printPage(int startLine, int x, int y, int w, int h)
{
	int i;
	GR_SIZE width, height, base;
	GrGetGCTextSize(tv_gc, "M", -1, GR_TFASCII, &width, &height, &base);

	for (i = startLine;i < startLine + lines_per_screen && i < totalLines;i++)
	{
		GrText(tv_wid, tv_gc, 3, (i - startLine + 1) * height + 4,
				lineData[i], -1, GR_TFASCII);
	}

}

static void buildLineData(char *starttextptr)
{
	char * curtextptr;
	char ** localAr;
	int allocedlines;

	currentLine = 0;
	curtextptr = starttextptr;
	if((localAr = (char **)malloc(sizeof(char *) * 20))==NULL) {
		destroy_textview_window("malloc failed");
		return;
	}
	allocedlines = 20;

	/* While we haven't reached the end of the file */
	while (*curtextptr != '\0')
	{
		char * sol;

		/* See if we need to allocate more lines to the line storage */
		if (allocedlines < currentLine + 1)
		{
			if((localAr = (char**)realloc(localAr, sizeof(char *) *
					(allocedlines + 20)))==NULL) {
				destroy_textview_window("realloc failed");
				return;
			}
			allocedlines += 20;
		}

		sol = curtextptr;
		while (1) {
			GR_SIZE width, height, base;

			if(*curtextptr == '\r') /* ignore '\r' */
				curtextptr++;
			if(*curtextptr == '\n') {
				curtextptr++;
				break;
			}
			else if(*curtextptr == '\0')
				break;

			GrGetGCTextSize(tv_gc, sol, curtextptr - sol + 1,
					GR_TFASCII, &width, &height, &base);

			if(width > tv_winfo.width - 16) {
				char *optr;
				
				/* backtrack to the last word */
				for(optr=curtextptr; optr>sol; optr--) {
					if(isspace(*optr)||*optr=='\t') {
						curtextptr=optr;
						curtextptr++;
						break;
					}
				}
				break;
			}
			curtextptr++;
		}

		if((localAr[currentLine] = malloc((curtextptr-sol + 1) *
						sizeof(char)))==NULL) {
			destroy_textview_window("malloc failed");
			return;
		}
		snprintf(localAr[currentLine], curtextptr-sol+1, "%s", sol);
		
		currentLine++;
	}

	totalLines = currentLine;
	currentLine = 0;
	lineData = localAr;
}

static void draw_scrollbar(const int height, const int y_top)
{
	int y_bottom = y_top + height;

	/* draw the containing box */
	GrRect(tv_wid, tv_gc, tv_winfo.width - 8, 1, 8, tv_winfo.height - 1);
	GrSetGCForeground(tv_gc, WHITE);
	/* erase the scrollbar */
	GrFillRect(tv_wid, tv_gc, tv_winfo.width - (8 - 1), 1 + 1, 
			(8 - 2), y_top - (1 + 1));
	GrFillRect(tv_wid, tv_gc, tv_winfo.width - (8 - 1), y_bottom, (8 - 2), 
			(tv_winfo.height - 3) - y_bottom);
	GrSetGCForeground(tv_gc, GRAY);
	/* draw the bar */
	GrFillRect(tv_wid, tv_gc, tv_winfo.width - (8 - 1), y_top, (8 - 2), height);
	GrSetGCForeground(tv_gc, BLACK);
}

static void drawtext(void)
{
	int per = (totalLines - lines_per_screen) <= lines_per_screen ?
		lines_per_screen * 100 / totalLines :
		lines_per_screen * 100 / (totalLines - lines_per_screen);
	int height = (tv_winfo.height - 2) * (per < 3 ? 3 : per) / 100;
	int y_top = ((((tv_winfo.height - 3) - height) * 100) * currentLine /
			((totalLines - lines_per_screen) )) / 100 + 2;

	GrSetGCForeground(tv_gc, WHITE);
	GrFillRect(tv_wid, tv_gc, 0, 0, tv_winfo.width - 8, tv_winfo.height);
	GrSetGCForeground(tv_gc, BLACK);
	
	/* Draw the text */
	printPage(currentLine, 0,0,0,0);
	
	if (totalLines > lines_per_screen && y_top != last_y_top) {
		draw_scrollbar(height, y_top);
		printf("tl: %d\n", totalLines);
	}
	
	last_y_top = y_top;
}

static void textview_do_draw()
{
	pz_draw_header(g_title);
	last_y_top = -1;
	drawtext();
}

static int textview_do_keystroke(GR_EVENT * event)
{
	int i;
	int ret = 1;
	switch (event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case 'm':
		case 'q':
			destroy_textview_window(NULL);
			for (i = 0; i < totalLines; i++)
				free(lineData[i]);
			free (lineData);
			break;
		case 'r':
			if (currentLine + lines_per_screen < totalLines) {
				currentLine++;
				drawtext();
			}
			break;
		case 'l':
			if (currentLine > 0) {
				currentLine--;
				drawtext();
			}
			break;
		case 'f':
			/* forward key pressed:  go down one screen */
			if (currentLine + lines_per_screen < totalLines) {
				currentLine = currentLine + lines_per_screen;
				if (currentLine + lines_per_screen > totalLines)
					/* if we went down beyond the end of
					 * the text we go to the end of the
					 * text */
				currentLine = totalLines - lines_per_screen;
				drawtext();
			}
			break;
		case 'w':
			/* rewind key pressed:  go up one screen
			 * if it's already the first line, nothing is done */
			if (currentLine > 0) {
				if (currentLine < lines_per_screen)
					/* if there is not a full screen above
					 * the  current one go up to the very
					 * first line */
					currentLine = 0;
				else	/* go up one screen */
					currentLine = currentLine -
						lines_per_screen;
				drawtext();
			}
			break;
		default:
			ret = 0;
		}
		break;
	}
	return ret;
}

int is_text_type(char * extension)
{
	return strcmp(extension, ".txt") == 0;
}

int is_ascii_file(char *filename)
{
	FILE *fp;
	unsigned char buf[20], *ptr;
	long file_len;
	struct stat ftype; 

	stat(filename, &ftype); 
	if(S_ISBLK(ftype.st_mode)||S_ISCHR(ftype.st_mode))
		return 0;
	if((fp=fopen(filename, "r"))==NULL) {
		fprintf(stderr, "Can't open \"%s\"\n", filename);
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	rewind(fp);
	fread(buf, file_len<20?file_len:20, 1, fp);
	fclose(fp);
	
	for(ptr=buf;ptr-buf<(file_len<20?file_len:20);ptr++)
		if(*ptr<7||*ptr>127)
			return 0;
	return 1;
}

void create_textview_window()
{
	GR_SIZE width, height, base;	
	tv_gc = pz_get_gc(1);
	GrSetGCUseBackground(tv_gc, GR_FALSE);
	GrSetGCForeground(tv_gc, BLACK);

	tv_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1),
			textview_do_draw, textview_do_keystroke);

	GrSelectEvents(tv_wid, GR_EVENT_MASK_EXPOSURE| GR_EVENT_MASK_KEY_UP|
			GR_EVENT_MASK_KEY_DOWN);

	GrGetGCTextSize(tv_gc, "M", -1, GR_TFASCII, &width, &height, &base);

	lines_per_screen = (int)(screen_info.rows - (HEADER_TOPLINE + 1)) /
		(height + 1);

	GrMapWindow(tv_wid);
	GrGetWindowInfo(tv_wid, &tv_winfo);
}

void new_textview_window(char *filename)
{
	char *buf;
	char tmp[512];
	FILE *fp;
	size_t file_len;

	g_title = strdup(filename);
	create_textview_window();
	
	if ((fp = fopen(filename,"r"))==NULL) {
		destroy_textview_window("Error opening file");
		return;
	}
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	rewind(fp);
	if(file_len==0) {
		buf = '\0';
		while(fgets(tmp, 512, fp)!=NULL) {
			if((buf = realloc(buf, ((buf=='\0'?0:strlen(buf)) +
						512) * sizeof(char)))==NULL) {
				destroy_textview_window("realloc failed");
				fclose(fp);
				return;
			}
			if(file_len==0) {
				strncpy(buf, tmp, 512);
				file_len=1;
			} else
				strncat(buf, tmp, 512);
		}
	}
	else {
		if((buf = calloc(file_len+1, 1))==NULL) {
			destroy_textview_window("calloc failed");
			fclose(fp);
			return;
		}
		if(fread(buf, 1, file_len, fp)!=file_len)
			pz_error("unknown read error, continuing");
	}
	if(buf=='\0') {
		destroy_textview_window(NULL);
		new_message_window("Empty File");
		fclose(fp);
		return;
	}

	fclose(fp);
	buildLineData(buf);
	free(buf);
}

void new_stringview_window(char *buf, char *title)
{
	char *exbuf;

	g_title = strdup(title);
	create_textview_window();

	if((exbuf = malloc(strlen(buf)+1))==NULL) {
		destroy_textview_window("malloc failed");		
		return;
	}
	snprintf(exbuf, strlen(buf)+1, "%s0", buf);

	buildLineData(exbuf);
	free(exbuf);
}

