/*
 * ipodinfo.c - output iPod revision and fstype
 *
 * Copyright (C) 2005 Courtney Cavin, Alastair Stuart
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
#include <limits.h>
#include <unistd.h>

#include "ipod.h"

int main(int argc, char **argv) {
	int ch;
	ipod_lcd_info *lcd;
	char buf[64];

	buf[0] = '\0';
	while ((ch = getopt(argc, argv, "rflc")) != -1) {
		switch (ch) {
		case 'r':
			sprintf(buf, "%lx", ipod_get_hw_version());
			break;
		case 'f':
			sprintf(buf, "%c", ipod_get_fs_type());
			break;
		case 'l':
			lcd = ipod_get_lcd_info();
			sprintf(buf, "%dx%d %d", lcd->width, lcd->height, lcd->type);
			free(lcd);
			break;
		case 'c':
			lcd = ipod_get_lcd_info();
			ipod_alloc_fb(lcd);
			memset(lcd->fb, 0, lcd->width * lcd->height * 2);
			ipod_update_colour_lcd(lcd, 0, 0, lcd->width, lcd->height);
			sleep(5);
			//sprintf(buf, "%dx%d %d", lcd->width, lcd->height, lcd->type);
			ipod_free_fb(lcd);
			free(lcd);
			break;
		case '?':
		default:
			fprintf(stderr, "%s [-r|-f|-l]\n"
					"\t-r  get ipod revision\n"
					"\t-f  get filesystem type F|H\n"
					"\t-l  get lcd info\n",
					argv[0]);
			return 1;
		}
		puts(buf);
	}
	return 0;
}
