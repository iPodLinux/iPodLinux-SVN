/*
 * Copyright (C) 2004 Damien Marchal, Bernard Leach
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#ifdef IPOD
#include <linux/vt.h>
#endif
#include <dirent.h>
#include <string.h>

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

static char current_dir[128];
int browser_nbEntries = 0;
int browser_currentSelection = 0;
int browser_currentBase = 0;
char *browser_selected_filename = NULL;
int browser_top = 0;
Directory browser_entries[MAX_ENTRIES];

int items_offset = 0;

extern void new_textview_window(char * filename);
extern int is_image_type(char *extension);
#ifdef __linux__
extern int is_mp3_type(char *extension);
extern void new_mp3_window(char *filename, char *album, char *artist, char *title, unsigned short len);
extern int is_raw_audio_type(char *extension);
extern void new_playback_window(char *filename);
#endif
extern int is_text_type(char * extension);
extern int is_ascii_file(char *filename);
void new_exec_window(char *filename);

static void browser_exit()
{
	int i;

	for (i = 0; i < browser_nbEntries; i++) {
		free(browser_entries[i].full_name);
	}
	browser_nbEntries = 0;

	if (current_dir[0] != 0) {
		chdir(current_dir);
	}
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
		if (strncmp(subdir->d_name, ".", strlen(subdir->d_name)) != 0) {
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
	for (i = begin; i < begin + 5+items_offset && i < browser_nbEntries; i++) {
		if (i == browser_currentSelection) {
			GrSetGCForeground(browser_gc, BLACK);
			GrFillRect(browser_wid, browser_gc, 0, y + 2,
				   screen_info.cols, height);
			GrSetGCForeground(browser_gc, WHITE);
			GrSetGCUseBackground(browser_gc, GR_TRUE);
		} else {
			GrSetGCUseBackground(browser_gc, GR_FALSE);
			GrSetGCMode(browser_gc, GR_MODE_SET);
			GrSetGCForeground(browser_gc, WHITE);
			GrFillRect(browser_wid, browser_gc, 0, y + 2,
				   screen_info.cols, height);
			GrSetGCForeground(browser_gc, BLACK);
		}

		y += height;

		if (strncmp(browser_entries[i].name, "..", 2) == 0) {
			GrText(browser_wid, browser_gc, 8, y,
				"<", -1, GR_TFASCII);
		}
		else {
			GrText(browser_wid, browser_gc, 8, y,
				browser_entries[i].name, -1, GR_TFASCII);
		}

		GrSetGCForeground(browser_gc, LTGRAY);
		switch (browser_entries[i].type) {
		case FILE_TYPE_PROGRAM:
			GrText(browser_wid, browser_gc, 1, y, "x", -1,
			       GR_TFASCII);
			break;
		case FILE_TYPE_DIRECTORY:
			if (strncmp(browser_entries[i].name, "..", 2) == 0) {
				GrText(browser_wid, browser_gc, 1, y, "<", -1,
					GR_TFASCII);
			}
			else {
				GrText(browser_wid, browser_gc, 1, y, ">", -1,
					GR_TFASCII);
			}
			break;
		case FILE_TYPE_OTHER:
			break;
		}
	}

	/* clear bottom portion of display */
	GrSetGCUseBackground(browser_gc, GR_FALSE);
	GrSetGCMode(browser_gc, GR_MODE_SET);
	GrSetGCForeground(browser_gc, WHITE);
	GrFillRect(browser_wid, browser_gc, 0, y + 2, screen_info.cols,
		   screen_info.rows - (y + 2));
	GrSetGCForeground(browser_gc, BLACK);
}

static void browser_do_draw()
{
	pz_draw_header("File Browser");

	browser_draw_browser();
}

static int is_script_type(char *extension)
{
	return strcmp(extension, ".sh") == 0;
}


static int is_binary_type(char *filename)
{
	FILE *fp;
	unsigned char header[12];
	struct stat ftype; 

	stat(filename, &ftype); 
	if(S_ISBLK(ftype.st_mode)||S_ISCHR(ftype.st_mode))
		return 0;
	if((fp = fopen(filename, "r"))==NULL) {
		fprintf(stderr, "Can't open \"%s\"\n", filename);
		return 0;
	}

	fread(header, sizeof(char), 12, fp);
	
	fclose(fp);
	if(strncmp(header, "bFLT", 4)==0)
		return 1;
	if(strncmp(header, "#!/bin/sh", 9)==0)
		return 1;
	return 0;
}

static void handle_type_other(char *filename)
{
	char *ext;

	ext = strrchr(filename, '.');
	if (ext==0) {
		if (is_ascii_file(filename)) {
			new_textview_window(filename);
		}
		else if (is_binary_type(filename)) {
			new_exec_window(filename);
		}
		else {
			new_message_window("Unknown Filetype");
		}
	}
	else if (is_image_type(ext)) {
		new_image_window(filename);
	}
#ifdef __linux__
	else if (is_mp3_type(ext)) {
		new_mp3_window(filename, "Unknown Album", "Unknown Artist", "Unknown Title", 0);
	}
	else if (is_raw_audio_type(ext)) {
		new_playback_window(filename);
	}
#endif
	else if (is_script_type(ext)) {
		new_exec_window(filename);
	}
	else if (is_text_type(ext)||is_ascii_file(filename)) {
		new_textview_window(filename);
	}
	else if (is_binary_type(filename)) {
		new_exec_window(filename);
	}
	else  {
		new_message_window("Unknown Filetype");
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
	int ret = 0;
	switch (event->keystroke.ch) {
	case '\r':
	case '\n':
		browser_selection_activated(browser_currentSelection);
		GrClearWindow(browser_wid, 1);
		browser_draw_browser();
		ret = 1;
		break;

	case 'm':
	case 'q':
		browser_exit();
		pz_close_window(browser_wid);
		ret = 1;
		break;

	case 'r':
		if (browser_currentSelection < browser_nbEntries - 1) {
			browser_currentSelection++;

			if (browser_top >= 4+items_offset) {
				browser_currentBase++;
			} else
				browser_top++;
			browser_draw_browser();
			ret = 1;
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
			ret = 1;
		}
		break;
	}
	return ret;
}

void new_browser_window(char *initial_path)
{
	if (initial_path) {
		getcwd(current_dir, sizeof(current_dir));
		chdir(initial_path);
	}
	else {
		current_dir[0] = 0;
	}

	GrGetScreenInfo(&screen_info);

	if (screen_info.cols != 138) { /* not mini */
		items_offset = 1;
	}

	browser_gc = GrNewGC();
	GrSetGCUseBackground(browser_gc, GR_FALSE);
	GrSetGCForeground(browser_gc, BLACK);

	browser_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
                                    screen_info.rows - (HEADER_TOPLINE + 1),
                                    browser_do_draw, browser_do_keystroke);

	GrSelectEvents(browser_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN);

	browser_mscandir("./");

	GrMapWindow(browser_wid);
}

void new_exec_window(char *filename)
{
#ifdef IPOD
	int ttyfd, status, fd;
	pid_t pid;

	if((ttyfd = open("/dev/console", O_RDWR)) == -1) {
                perror("/dev/console");
		return;
	}
	if(ioctl(ttyfd, VT_ACTIVATE, 0x2)) {
		perror("child VT_ACTIVATE");
		_exit(1);
	}
	if(ioctl(ttyfd, VT_WAITACTIVE, 0x2)) {
		perror("child VT_WAITACTIVE");
		_exit(1);
	}
	
	switch(pid = vfork()) {
	case -1: /* error */
		perror("vfork");
		break;
	case 0: /* child */
		close(ttyfd);
		if(setsid() < 0) {
			perror("setsid");
			_exit(1);
		}
		close(0);
		if((fd = open("/dev/console", O_RDWR)) == -1) {
			perror("/dev/console");
			_exit(1);
		}
		if(dup(fd) == -1) {
			perror("stdin dup");
			_exit(1);
		}
		close(1);
		if(dup(fd) == -1) {
			perror("stdout dup");
			_exit(1);
		}
		close(2);
		if(dup(fd) == -1) {
			perror("stderr dup");
			_exit(1);
		}

		execl("/bin/sh", "sh", "-c", filename);
		fprintf(stderr, "exec failed!\n");
		exit(1);
		break;
	default: /* parent */
		waitpid(pid, &status, 0);
		sleep(5);
		
        	if(ioctl(ttyfd, VT_ACTIVATE, 0x1)) {
			perror("parent VT_ACTIVATE");
			return;
		}
        	if(ioctl(ttyfd, VT_WAITACTIVE, 0x1)) {
			perror("parent VT_WAITACTIVE");
			return;
		}
		if(ioctl(ttyfd, VT_DISALLOCATE, 0x2)) {
			perror("VT_DISALLOCATE");
			return;
		}
		close(ttyfd);

		browser_do_draw();
		break;
	}
#else
	new_message_window(filename);
#endif
}

