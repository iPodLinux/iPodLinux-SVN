/*
 * Copyright (C) 2004 Damien Marchal, Bernard Leach
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdbool.h>

#include "pz.h"
#include "piezo.h"

static GR_WINDOW_ID browser_wid;
static GR_GC_ID browser_gc;
static GR_SCREEN_INFO screen_info;

#define FILE_TYPE_PROGRAM 0
#define FILE_TYPE_DIRECTORY 1
#define FILE_TYPE_OTHER 2
#define MAX_ENTRIES 200

typedef struct {
	char name[32];
	char name_size;
	char *full_name;
	unsigned short type;
} Directory;

int browser_nbEntries = 0;
int browser_currentSelection = 0;
int browser_currentBase = 0;
char *browser_selected_filename = NULL;
int browser_top = 0;
Directory browser_entries[MAX_ENTRIES];

static void browser_exit()
{
	int i;

	for (i = 0; i < browser_nbEntries; i++) {
		free(browser_entries[i].full_name);
	}
	browser_nbEntries = 0;
}

/* the directory to scan */
static void browser_mscandir(char *dira)
{
	DIR *dir = opendir(dira);
	int i;
	struct stat stat_result;
	struct dirent *subdir;
	int size;

	// not very good for fragmentation... 
	for (i = 0; i < browser_nbEntries; i++) {
		free(browser_entries[i].full_name);
	}
	browser_nbEntries = 0;

	while ((subdir = readdir(dir))) {
		if (browser_nbEntries >= MAX_ENTRIES) {
			new_message_window("Too many files.");
			break;
		}

		size = strlen(subdir->d_name);
		browser_entries[browser_nbEntries].full_name = malloc(sizeof(char) * size + 1);
		strcpy(browser_entries[browser_nbEntries].full_name, subdir->d_name);

		if (size > 30) {
			size = 30;
		}

		memcpy(browser_entries[browser_nbEntries].name, subdir->d_name, size);
		browser_entries[browser_nbEntries].name[size] = 0;
		browser_entries[browser_nbEntries].name_size = size;

		browser_entries[browser_nbEntries].type = FILE_TYPE_OTHER;
		stat(subdir->d_name, &stat_result);
		if (S_ISDIR(stat_result.st_mode)) {
			browser_entries[browser_nbEntries].type =
			    FILE_TYPE_DIRECTORY;
		} else if (stat_result.st_mode & S_IXUSR) {
			browser_entries[browser_nbEntries].type =
			    FILE_TYPE_PROGRAM;
		} else {
		}

		browser_nbEntries++;
	}
	closedir(dir);

	browser_currentSelection = 0;
	browser_top = 0;
	browser_currentBase = 0;
}

static void browser_draw_browser()
{
	int i;

	GR_SIZE width, height, base;

	int begin = browser_currentBase;
	int y;

	GrGetGCTextSize(browser_gc, "M", -1, GR_TFASCII, &width, &height, &base);
	height += 2;

	y = 5;
	for (i = begin; i < begin + 6 && i < browser_nbEntries; i++) {
		if (i == browser_currentSelection) {
			GrSetGCForeground(browser_gc, WHITE);
			GrFillRect(browser_wid, browser_gc, 0, y + 2,
				   screen_info.cols, height);
			GrSetGCForeground(browser_gc, BLACK);
			GrSetGCUseBackground(browser_gc, GR_FALSE);
		} else {
			GrSetGCUseBackground(browser_gc, GR_TRUE);
			GrSetGCMode(browser_gc, GR_MODE_SET);
			GrSetGCForeground(browser_gc, BLACK);
			GrFillRect(browser_wid, browser_gc, 0, y + 2,
				   screen_info.cols, height);
			GrSetGCForeground(browser_gc, WHITE);
		}

		y += height;
		GrText(browser_wid, browser_gc, 8, y,
		       browser_entries[i].name, -1, GR_TFASCII);

		GrSetGCForeground(browser_gc, GRAY);
		switch (browser_entries[i].type) {
		case FILE_TYPE_PROGRAM:
			GrText(browser_wid, browser_gc, 1, y, "x", -1,
			       GR_TFASCII);
			break;
		case FILE_TYPE_DIRECTORY:
			GrText(browser_wid, browser_gc, 1, y, ">", -1,
			       GR_TFASCII);
			break;
		case FILE_TYPE_OTHER:
			break;
		}


	}

	/* clear bottom portion of display */
	GrSetGCUseBackground(browser_gc, GR_TRUE);
	GrSetGCMode(browser_gc, GR_MODE_SET);
	GrSetGCForeground(browser_gc, BLACK);
	GrFillRect(browser_wid, browser_gc, 0, y + 2, screen_info.cols,
		   screen_info.rows - (y + 2));
	GrSetGCForeground(browser_gc, WHITE);
}

static void browser_do_draw(GR_EVENT *event)
{
	pz_draw_header("File Browser");

	browser_draw_browser();
}

static void handle_type_other(char *filename)
{
	char *ext;

	ext = strrchr(filename, '.');
	if (ext == 0) return;

	if (is_image_type(ext)) {
		new_image_window(filename);
	} else if (is_text_type(ext)) {
		new_textview_window(filename);
	} else if (is_mp3_type(ext)) {
		new_mp3_window(filename);
	} else {

		new_message_window(filename);
	}
}

static void browser_selection_activated(unsigned short userChoice)
{
	switch (browser_entries[userChoice].type) {
	case FILE_TYPE_DIRECTORY:
		chdir(browser_entries[userChoice].full_name);
		browser_mscandir("./");
		break;
	case FILE_TYPE_PROGRAM:
	case FILE_TYPE_OTHER:
		handle_type_other(browser_entries[userChoice].full_name);
		break;
	}
}

static int browser_do_keystroke(GR_EVENT * event)
{
	int ret = -1;
	switch (event->keystroke.ch) {
	case '\r':
	case '\n':
		browser_selection_activated(browser_currentSelection);
		browser_draw_browser();
		ret = 0;
		break;

	case 'm':
	case 'q':
		browser_exit();
		pz_close_window(browser_wid);
		ret = 0;
		break;

	case 'r':
		if (browser_currentSelection < browser_nbEntries - 1) {
			browser_currentSelection++;

			if (browser_top >= 5) {
				browser_currentBase++;
			} else
				browser_top++;
			browser_draw_browser();
			ret = 0;
		}

		break;
	case 'l':

		if (browser_currentSelection > 0) {
			browser_currentSelection--;

			if (browser_top == 0) {
				browser_currentBase--;
			} else {
				browser_top--;
			}

			browser_draw_browser();
			ret = 0;
		}
		break;
	}
	return ret;

}

static void browser_event_handler(GR_EVENT *event)
{
	int i;

	switch (event->type) {
	case GR_EVENT_TYPE_EXPOSURE:
		browser_do_draw(event);
		break;

	case GR_EVENT_TYPE_KEY_DOWN:
		if (browser_do_keystroke(event) == 0)
			beep();
		break;
	}
}

void new_browser_window(void)
{

	GrGetScreenInfo(&screen_info);


	browser_gc = GrNewGC();
	GrSetGCUseBackground(browser_gc, GR_TRUE);
	GrSetGCForeground(browser_gc, WHITE);

	browser_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
                                    screen_info.rows - (HEADER_TOPLINE + 1),
                                    browser_event_handler);

	GrSelectEvents(browser_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	browser_mscandir("./");

	GrMapWindow(browser_wid);
}
