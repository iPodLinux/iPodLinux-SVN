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

#define LINESPERSCREEN 7

static GR_GC_ID tv_gc;
static GR_WINDOW_ID tv_wid;
static GR_WINDOW_INFO tv_winfo;
static GR_SCREEN_INFO screen_info;
static char *g_title;

static unsigned int totalLines = 0;
static unsigned int currentLine = 0;
static char ** lineData;

void destroy_textview_window(char *msg)
{
	pz_close_window(tv_wid);
	GrDestroyGC(tv_gc);
	free(g_title);
	if(msg!=NULL) {
		fprintf(stderr, "%s\n", msg);
		new_message_window(msg);
	}
}

static void printPage(int startLine, int x, int y, int w, int h)
{
	int i;
	GR_SIZE width, height, base;
	GrGetGCTextSize(tv_gc, "M", -1, GR_TFASCII, &width, &height, &base);

	for (i = startLine;i < startLine + LINESPERSCREEN && i < totalLines;i++)
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

			if(width > tv_winfo.width - 15) {
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

static void drawtext(void)
{
	/* calculate scrollbar sizes */
	float pixPerLine = (tv_winfo.height - 16 - 2) / (float) totalLines;
	int block_start_pix = 9 + pixPerLine * currentLine;
	int block_height = 9 + pixPerLine * (currentLine + LINESPERSCREEN) -
		block_start_pix;

	GrSetGCForeground(tv_gc, BLACK);

	if (block_height < 2) /* the minimum height of the handle is set to 2 */
		block_height = 2;

	GrClearWindow(tv_wid, 0);

	/* Draw the text */
	printPage(currentLine, 0,0,0,0);

	/* Draw the ScrollBar track */
	GrRect(tv_wid, tv_gc, tv_winfo.width - 6, 8, 2, tv_winfo.height - 16 );

	/* Draw the Handle on the ScrollBar */
	GrRect(tv_wid, tv_gc, tv_winfo.width - 7, block_start_pix, 4,
			block_height);
	GrSetGCForeground(tv_gc, WHITE);
	GrRect(tv_wid, tv_gc, tv_winfo.width - 6, block_start_pix + 1, 2,
			block_height - 2);
}

static void textview_do_draw()
{
	pz_draw_header(g_title);
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
			if (currentLine + LINESPERSCREEN < totalLines) {
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
			if (currentLine + LINESPERSCREEN < totalLines) {
				currentLine = currentLine + LINESPERSCREEN;
				if (currentLine + LINESPERSCREEN > totalLines)
					/* if we went down beyond the end of
					 * the text we go to the end of the
					 * text */
				currentLine = totalLines - LINESPERSCREEN;
				drawtext();
			}
			break;
		case 'w':
			/* rewind key pressed:  go up one screen
			 * if it's already the first line, nothing is done */
			if (currentLine > 0) {
				if (currentLine < LINESPERSCREEN)
					/* if there is not a full screen above
					 * the  current one go up to the very
					 * first line */
					currentLine = 0;
				else	/* go up one screen */
					currentLine = currentLine -
						LINESPERSCREEN;
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
	for(ptr=buf;ptr-buf<(file_len<20?file_len:20);ptr++)
		if(*ptr<7||*ptr>127)
			return 0;
	return 1;
}

void create_textview_window()
{
	GrGetScreenInfo(&screen_info);

	tv_gc = GrNewGC();
	GrSetGCUseBackground(tv_gc, GR_FALSE);
	GrSetGCForeground(tv_gc, BLACK);

	tv_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1),
			textview_do_draw, textview_do_keystroke);

	GrSelectEvents(tv_wid, GR_EVENT_MASK_EXPOSURE| GR_EVENT_MASK_KEY_UP|
			GR_EVENT_MASK_KEY_DOWN);

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
		while(fgets(tmp, 512, fp)!=NULL) {
			if((buf = realloc(buf, (strlen(buf) +
						512) * sizeof(char)))==NULL) {
				destroy_textview_window("malloc failed");
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
		if((buf = malloc(file_len+1))==NULL) {
			destroy_textview_window("malloc failed");
			return;
		}
		if(fread(buf, 1, file_len, fp)!=file_len)
			new_message_window("unknown read error, continuing");
	}
	buf[strlen(buf)+1] = '\0';

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

