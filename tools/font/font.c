/*
 * Copyright (C) 2006 Jonathan Bettencourt (jonrelay)
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static char * fontNames[] = {
	"Fixed 6x13",
	"Fixed 6x12",
	"Fixed 6x10",
	"Fixed 6x9",
	"Fixed 5x8",
	"Fixed 5x7",
	"Fixed 4x6",
	"Fixed 7x13",
	"Fixed 7x14",
	"Fixed 8x13",
	"Fixed 9x15",
	"Fixed 9x18",
	"Fixed 10x20",
	"Unifont",
	"Unifont",
	"Unifont",
	"Apple II 40 Column",
	"Apple II 80 Column",
	"Topaz",
	"Topaz Doubled",
	"Topaz Sans",
	"Topaz Sans Doubled",
	"Opal",
	"Opal Double",
	"Diamond",
	"Onyx",
	"Peridot",
	"Sabine Doscbthm",
	"Sabine Doscbthm Black",
	"Sabine Doscbthm Script",
	"Sabine Doscbthm Serif",
	"Sabine Doscbthm Smallcaps",
	0
};

int main(int argc, char * argv[])
{
	switch (argc) {
		case 1: {
			char * linesstr = getenv("LINES");
			int lines;
			int i = 0;
			signed char ch;
			if (!linesstr) {
				lines = 1000;
			} else {
				lines = atoi(linesstr);
				if (!lines) lines = 1000;
			}
			while (fontNames[i]) {
				printf("%3d %s\n", i, fontNames[i]);
				i++;
				if ( (i % (lines-1)) == 0 ) {
					printf("--More--");
					while ((ch = getchar()) != '\n' && ch != EOF);
					printf("\n");
				}
			}
		} break;
		case 2: {
			int which;
			
			if (isdigit(argv[1][0])) {
				which = atoi(argv[1]);
			} else {
				int i = 0;
				which = -1;
				while (fontNames[i]) {
					if (!strcasecmp(fontNames[i],argv[1])) {
						which = i;
						break;
					}
					i++;
				}
				if (which < 0) {
					printf("Font %s not found.\n", argv[1]);
					return 0;
				}
			}
			
			printf("%c[%dF", 27, which);
		} break;
		case 3: {
			int w = atoi(argv[1]);
			int h = atoi(argv[2]);
			int i = 0;
			int which = -1;
			char fn[50];
			
			snprintf(fn, 50, "Fixed %dx%d", w, h);
			while (fontNames[i]) {
				if (!strcasecmp(fontNames[i],fn)) {
					which = i;
					break;
				}
				i++;
			}
			if (which < 0) {
				printf("Font %s not found.\n", fn);
				return 0;
			}
			
			printf("%c[%dF", 27, which);
		} break;
		default: {
			printf("Too many parameters, I don't know how to deal with them all.\n");
		} break;
	}
	return 0;
}
