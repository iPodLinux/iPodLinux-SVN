/*
 * Copyright (C) 2004 David Carne
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

#include "pz.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#define LINESPERSCREEN 7

static GR_GC_ID tv_gc;
static GR_WINDOW_ID tv_wid;
static GR_WINDOW_INFO tv_winfo;
static GR_SCREEN_INFO screen_info;
static char * g_filename;

static unsigned int totalLines = 0;
static unsigned int currentLine = 0;
static char ** lineData; 

static void printPage(int startLine, int x, int y, int w, int h)
{
	int i;
	GR_SIZE width, height, base;
	GrGetGCTextSize(tv_gc, "M", -1, GR_TFASCII, &width, &height, &base);

	for (i = startLine; i < startLine + LINESPERSCREEN && i < totalLines; i++)
	{
		GrText(tv_wid, tv_gc, 3, (i - startLine + 1) * height + 4, lineData[i], -1, GR_TFASCII); 
	}

}

// This buildlines code is a horrible, horrible hack. It may also (more than likely) be (very) broken.
// I also see many ways to optimize it, but I'm really to lazy to at the moment.
static void buildLineData()
{
	
	char * starttextptr;
	FILE * fp;
	long unsigned int file_len;
	char * curtextptr;
	char ** localAr;
	int allocedlines;
	char * temp;

	currentLine = 0;
	fp = fopen(g_filename,"r");
	if (!fp)
	{
		printf("cannot open file %s\n", g_filename);
		return;
	}
	
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	starttextptr = (char *) malloc (file_len + 1);
	fread(starttextptr, file_len, 1, fp);
	starttextptr[file_len] = 0;

	curtextptr = starttextptr;
	localAr = (char **)malloc(sizeof(char *) * 20);
	allocedlines = 20;

	// While we haven't reached the end of the file
	while (*curtextptr != 0)
	{
		int charcnt = 0;
		char * sol;

		// See if we need to allocate more lines to the line storage
		if (allocedlines < currentLine + 1)
		{
			//fprintf(stderr,"Realloced Mem\n");
			char ** temp = localAr;
			localAr = (char **)malloc(sizeof(char *) * (allocedlines + 20));
			memcpy(localAr, temp, sizeof(char *) * allocedlines);
			free(temp);
			allocedlines += 20;
		}

		localAr[currentLine] = (char *) malloc(100);
		sol = curtextptr;
		while (true)
		{
			GR_SIZE width, height, base;

			if (*curtextptr == '\n')
			{
				curtextptr++;
				break;
			} else if (*curtextptr == 0) {
				break;
			}

			GrGetGCTextSize ( tv_gc , sol , curtextptr - sol + 1 , GR_TFASCII, &width , &height, &base );

			if (width > tv_winfo.width - 15)
			{
				// check if we there is a line break in this line
				if (!strchr(localAr[currentLine], ' ') && !strchr(localAr[currentLine], '\t'))
					// Guess not, leave
					break;

				// now backtrack to the last word
				while (!isspace(localAr[currentLine][charcnt]))
				{
					localAr[currentLine][charcnt] = 0;
					curtextptr --;
					charcnt --;
				}
				// skip over the char we broke the line on.
				curtextptr++;
				charcnt++;
				break;
			}

			localAr[currentLine][charcnt++] = *curtextptr++;
		}

		localAr[currentLine][charcnt] = 0;

		// trim the line down to size
		temp = localAr[currentLine];
		localAr[currentLine] = malloc(strlen(temp)+1);
		strcpy(localAr[currentLine], temp);
		free(temp);

		// fprintf(stderr,"Built Line %s\n", localAr[currentLine]);

		currentLine++;
	}

	totalLines = currentLine;
	currentLine = 0;
	lineData = localAr;
	free(starttextptr);
}

static void drawtext(void)
{
	// calculate scrollbar sizes
	float pixPerLine = (tv_winfo.height - 16 - 2) / (float) totalLines;
	int block_start_pix = 9 + pixPerLine * currentLine;
	int block_height = 9 + pixPerLine * (currentLine + LINESPERSCREEN) - block_start_pix;

	GrSetGCForeground(tv_gc, WHITE);

	// Clear the Screen
	GrClearWindow(tv_wid, 0);

	// Draw the text
	printPage(currentLine, 0,0,0,0);

	// Draw the ScrollBar track
	GrRect (tv_wid , tv_gc , tv_winfo.width - 6, 8, 2, tv_winfo.height - 16 );

	// Draw the Handle on the ScrollBar
	GrRect (tv_wid , tv_gc , tv_winfo.width - 7, block_start_pix, 4, block_height);
	GrSetGCForeground(tv_gc, BLACK);
	GrRect (tv_wid , tv_gc , tv_winfo.width - 6, block_start_pix + 1, 2, block_height - 2);
}

static void textview_do_draw(GR_EVENT * event)
{
	pz_draw_header(g_filename);
	drawtext();
}

static int textview_do_keystroke(GR_EVENT * event){
	int i;
	int ret = 0;
	switch (event->keystroke.ch) {
		case 'm':
		case 'q':
			pz_close_window(tv_wid);
			for (i = 0; i < totalLines; i++)
				free(lineData[i]);
			free (lineData);
			break;

		case 'r':
			if (currentLine + LINESPERSCREEN < totalLines)
				currentLine ++;
			drawtext();
			ret = 1;
			break;

		case 'l':
			
			if (currentLine > 0)
				currentLine --;
			drawtext();
			ret = 1;
			break;
	}
	return ret;
}

bool is_text_type(char * extension)
{
	return strcmp(extension, ".txt") == 0;
}

void new_textview_window(char * filename)
{		
	g_filename = (char *)strdup(filename);

	GrGetScreenInfo(&screen_info);

	tv_gc = GrNewGC();
	GrSetGCUseBackground(tv_gc, GR_TRUE);
	GrSetGCForeground(tv_gc, WHITE);
	
	tv_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), textview_do_draw, textview_do_keystroke);
	
	GrSelectEvents(tv_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(tv_wid);
	GrGetWindowInfo( tv_wid, &tv_winfo);

	buildLineData();
}
