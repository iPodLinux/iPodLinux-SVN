/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pz.h"

#define FONTCONF "font.lst"
#ifdef IPOD
#define PZEXEC "/bin/podzilla"
#define FONTSAVE "/etc/font.conf"
#define FONTDIR "/usr/share/fonts/"
#else
#define PZEXEC "./podzilla"
#define FONTSAVE "font.conf"
#define FONTDIR "fonts/"
#endif


typedef struct _fontlist_st {
	char file[128];
	char name[64];
	int size;
} fontlist_st;

static GR_WINDOW_ID font_wid;
static GR_GC_ID font_gc;
static GR_FONT_ID font_font;

static GR_FONT_ID pz_font;

fontlist_st fl[64]; /* max 64 fonts */
static int num_fonts = 0;
static int cur_font = 0;
static int foc;

int parseline(char *parseline, char *file, char *name, char *size)
{
	int i;
	typedef struct {
		char definer[2];
		char *string;
		int required;
	} vars;
	vars cvars[] = {
		{"[]", NULL, 1},
		{"()", NULL, 1},
		{"<>", NULL, 0},
		{"", NULL}
	};
	char line[128];
	char *ptr;
	char *cur;

	cvars[0].string = file;
	cvars[1].string = name;
	cvars[2].string = size;

	strcpy(line, parseline);
	ptr = line;

	while (1) {
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;
		if (*ptr == '\n' || *ptr == '\r' || *ptr =='\0' || *ptr == '#')
			break;

		for (i = 0; cvars[i].string != NULL; i++) {
			if (*ptr != cvars[i].definer[0])
				continue;
			for (cur = ++ptr; *ptr != cvars[i].definer[1]; ptr++)
				if(*ptr == '\n' || *ptr == '\r' || *ptr == '\0')
					return -1;
			strncpy(cvars[i].string, cur, ptr - cur);
			cvars[i].string[ptr - cur] = '\0';
			if (cvars[i].required == 1)
				cvars[i].required = 2;
			break;
		}
		if (cvars[i].string == NULL)
			return -1;
		ptr++;
	}
	for (i = 0; cvars[i].string != NULL; i++)
		if (cvars[i].required == 1)
			return -1;
	return 0;
}

int populate_fontlist() {
	FILE *fd;

	fd = fopen(FONTDIR FONTCONF, "r");
	if (fd == NULL)
		return -1;
	num_fonts = 0;
	while (!feof(fd) && !ferror(fd)) {
		char file[64];
		char name[64];
		char size[4];
		char line[128];

		fgets(line, 127, fd);
		if (strlen(line) <= 0)
			continue;
		if (line[0] == '#')
			continue;
		if (parseline(line, file, name, size) >= 0) {
			strcpy(fl[num_fonts].file, FONTDIR);
			strcat(fl[num_fonts].file, file);
			strcpy(fl[num_fonts].name, name);
			fl[num_fonts].size = atoi(size);
			if (num_fonts++ >= 63)
				break;
		}
	}
	fclose(fd);
	return num_fonts;
}

static void destroy_font(int local)
{
	if (local && font_font) {
		GrDestroyFont(font_font);
		font_font = 0;
	}
	if (!local && pz_font) {
		GrDestroyFont(pz_font);
		pz_font = 0;
	}
}

static void set_font(char *file, int size, GR_GC_ID gc, int local)
{
	GR_FONT_ID lfont;
	lfont = GrCreateFont((GR_CHAR *)file, size, NULL);
	if (lfont == 0) {
		pz_error("Unable to load font");
		return;
	}
	GrSetGCFont(gc, lfont);
	destroy_font(local);
	if (local) {
		font_font = lfont;
	}
	else {
		pz_font = lfont;
		foc = cur_font;
	}
}

void load_font()
{
	FILE *fp;
	char file[128];
	char *size;

	fp = fopen(FONTSAVE, "r");
	if (fp == NULL)
		return;
	fread(file, sizeof(char), 128, fp);
	fclose(fp);
	size = strrchr(file, ',');
	*size++ = '\0';
	set_font(file, atoi(size), pz_get_gc(0), 0);
}

static void save_font()
{
	FILE *fp;
	char fontline[128];

	fp = fopen(FONTSAVE, "w+");
	if (fp == NULL)
		return;
	snprintf(fontline, 127, "%s,%d", fl[foc].file, fl[foc].size);
	fwrite(fontline, sizeof(char), strlen(fontline), fp);
	fclose(fp);
}

static void font_clear_window()
{
	GrClearWindow(font_wid, GR_FALSE);
	GrSetGCForeground( font_gc, appearance_get_color(CS_BG) );
	GrFillRect( font_wid, font_gc, 0, 0, screen_info.cols, screen_info.rows );
	GrSetGCBackground( font_gc, appearance_get_color(CS_BG) );
	GrSetGCForeground( font_gc, appearance_get_color(CS_FG) );
}

static void draw_font()
{
	char buf[128];
	GR_SIZE width, height, base;

	font_clear_window();

	/* display font name and size */
	snprintf( buf, 128, "%s (%d)", fl[cur_font].name, fl[cur_font].size);
	GrGetGCTextSize(font_gc, buf, -1, GR_TFASCII,
			&width, &height, &base);
	GrText(font_wid, font_gc, (screen_info.cols - width) >> 1, 40,
			buf, -1, GR_TFASCII);

	/* display a sampler */
	GrGetGCTextSize(font_gc, "AaBbCcDdEe", -1, GR_TFASCII,
			&width, &height, &base);
	GrText(font_wid, font_gc, (screen_info.cols - width) >> 1, 65,
			"AaBbCcDdEe", -1, GR_TFASCII);
	GrGetGCTextSize(font_gc, "1234567890", -1, GR_TFASCII,
			&width, &height, &base);
	GrText(font_wid, font_gc, (screen_info.cols - width) >> 1, 65+height,
			"1234567890", -1, GR_TFASCII);

	/* display instructions */
	GrGetGCTextSize(font_gc, "Spin wheel to change", -1, GR_TFASCII,
			&width, &height, &base);
	GrText(font_wid, font_gc, (screen_info.cols-width) >> 1, 10,
			"Spin wheel to change", -1, GR_TFASCII);
}

static void change_font(int num)
{
	GR_SIZE width, height, base;
	char *load = "Loading...";
	
	font_clear_window();
	GrGetGCTextSize(font_gc, load, -1, GR_TFASCII, &width, &height, &base);
	GrText(font_wid, font_gc, (screen_info.cols - width) >> 1,
			(screen_info.rows - HEADER_TOPLINE) >> 1, load,
			-1, GR_TFASCII);
	set_font(fl[cur_font].file, fl[cur_font].size, font_gc, 1);
}

static void font_exposure()
{
	pz_draw_header("Fonts");
	draw_font();
}

static int font_event_handler(GR_EVENT *e)
{
	int ret = 0;
	switch (e->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (e->keystroke.ch) {
		case IPOD_WHEEL_COUNTERCLOCKWISE:
			if( num_fonts > 0 ) {
			    cur_font = (cur_font - 1) < 0 ? num_fonts - 1 :
				    (cur_font - 1);
			    change_font(cur_font);
			    draw_font();
			}
			ret |= KEY_CLICK;
			break;
		case IPOD_WHEEL_CLOCKWISE:
			if( num_fonts > 0 ) {
			    cur_font = (cur_font + 1) > (num_fonts - 1) ?
				    0 : (cur_font + 1);
			    change_font(cur_font);
			    draw_font();
			}
			ret |= KEY_CLICK;
			break;
		case IPOD_BUTTON_ACTION: 
		case '\n':
			if( num_fonts > 0 ) {
				set_font(fl[cur_font].file, fl[cur_font].size,
					pz_get_gc(0), 0);
				new_message_window(
					"Selected font, press Menu to "
					"restart podzilla");
			}
			ret |= KEY_CLICK;
			break;
		case IPOD_BUTTON_MENU:
			destroy_font(1);
			GrDestroyGC(font_gc);
			pz_close_window(font_wid);
			if (foc == -1)
				break;
			save_font();
			GrClose();
			switch (vfork()) {
			case -1:
				perror("vfork");
				break;
			case 0:
				execl(PZEXEC, NULL);
				fprintf(stderr, "execl failed");
				_exit(1);
				break;
			default:
				wait(NULL);
				break;
			}
			exit(0);
			break;
		default:
			ret |= KEY_UNUSED;
			break;
		}
		break;
	default:
		ret |= EVENT_UNUSED;
		break;
	}
	return ret;
}

void new_font_window()
{
	if (num_fonts == 0) {
		if (populate_fontlist() <= 0) {
			pz_error("Unable to load fontlist " FONTDIR FONTCONF);
			return;
		}
	}
	foc = -1;
	font_gc = pz_get_gc(1); /* Grab the root GC */
	GrSetGCUseBackground(font_gc, GR_FALSE); /* Don't Draw Background */
	GrSetGCForeground(font_gc, BLACK); /* Foreground; Black */

	font_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1), font_exposure,
			font_event_handler); /* Create New Window */

	GrSelectEvents(font_wid, GR_EVENT_MASK_EXPOSURE | /* Events Accepted */
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_KEY_UP);

	GrMapWindow(font_wid); /* Draw Window to Screen */
	change_font(cur_font);
}

