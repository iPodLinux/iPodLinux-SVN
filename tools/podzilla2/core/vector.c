/*
 * polyfont - vector polyline font
 *
 *  Copyright (C) 2005 Scott Lawrence and Joshua Oreman
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


#include <stdio.h>	/* for NULL */
#include <string.h>	/* for strlen */
#include "pz.h"


/** The points are specified as follows: 0xXXYY
 * XX and YY indicate where the point is. The values
 * are 00 for the top / left or ff for the bottom / right.

  00 40 80 b0 ff
  --------------+
  00 27 01 28 02| 00
  18 -- -- -- 17| 2a
  03 vv 04 -- 05| 56 <-+ Between these rows: 22 between 3 and 6,
  06 ^^ 34 07 08| 80 <-+ 23 between vv and ^^
  19 -- 09 -- 10| aa
  11 24 12 25 13| d6 <-+ Between these rows: 32 between 24 and 29,
  14 29 15 30 16| ff <-+ 26 between 12 and 15, 31 between 25 and 30
*/
#define PT0  0x0000
#define PT1  0x8000
#define PT2  0xff00
#define PT3  0x0056
#define PT4  0x8056
#define PT5  0xff56
#define PT6  0x0080
#define PT7  0xb080
#define PT8  0xff80
#define PT9  0x80aa
#define PT10 0xffaa
#define PT11 0x00d6
#define PT12 0x80d6
#define PT13 0xffd6
#define PT14 0x00ff
#define PT15 0x80ff
#define PT16 0xffff
#define PT17 0xff2a
#define PT18 0x002a
#define PT19 0x00aa
#define PT22 0xb064
#define PT23 0x4064
#define PT24 0xb0d6
#define PT25 0x40d6
#define PT26 0x80e4
#define PT27 0x4000
#define PT28 0xb000
#define PT29 0x40ff
#define PT30 0xb0ff
#define PT31 0xb0e4
#define PT32 0x40e4
/* PT33 is out-of-bounds */
#define PT34 0x8080

#define SKIP 0x10000

struct vector_polystruct 
{
    int nv;
    int v[16];
};

/* digits for text rendering */
static struct vector_polystruct V_digits[] = {
    /* 0 */ {5, {PT0, PT2, PT16, PT14, PT0, PT4|SKIP, PT9}},
    /* 1 */ {2, {PT1, PT15}},
    /* 2 */ {6, {PT0, PT2, PT8, PT6, PT14, PT16}},
    /* 3 */ {6, {PT0, PT2, PT16, PT14, PT6|SKIP, PT8}},
    /* 4 */ {5, {PT0, PT6, PT8, PT2|SKIP, PT16}},
    /* 5 */ {6, {PT2, PT0, PT6, PT8, PT16, PT14}},
    /* 6 */ {5, {PT0, PT14, PT16, PT8, PT6}},
    /* 7 */ {3, {PT0, PT2, PT16}},
    /* 8 */ {8, {PT8, PT2, PT0, PT6, PT8, PT16, PT14, PT6}},
    /* 9 */ {5, {PT8, PT6, PT0, PT2, PT16}},
};

/* letters for alphabetical (English latin font) */
static struct vector_polystruct V_alpha[] =
{
    /* A */ {7, {PT14, PT3, PT1, PT5, PT16, PT6|SKIP, PT8}},
    /* B */ {10, {PT0, PT2, PT5, PT7, PT6, PT7, PT10, PT16, PT14, PT0}},
    /* C */ {4, {PT2, PT0, PT14, PT16}},
    /* D */ {7, {PT14, PT0, PT1, PT17, PT13, PT15, PT14}},
    /* E */ {6, {PT2, PT0, PT14, PT16, PT6|SKIP, PT7}},
    /* F */ {5, {PT2, PT0, PT14, PT6|SKIP, PT7}},
    /* G */ {7, {PT5, PT2, PT0, PT14, PT16, PT10, PT9}},
    /* H */ {6, {PT0, PT14, PT6|SKIP, PT8, PT2|SKIP, PT16}},
    /* I */ {6, {PT0, PT2, PT1|SKIP, PT15, PT14|SKIP, PT16}},
    /* J */ {6, {PT0, PT2, PT28|SKIP, PT30, PT14, PT11}},
    /* K */ {5, {PT0, PT14, PT2|SKIP, PT6, PT16}},
    /* L */ {3, {PT0, PT14, PT16}},
    /* M */ {5, {PT14, PT0, PT4, PT2, PT16}},
    /* N */ {4, {PT14, PT0, PT16, PT2}},
    /* O */ {5, {PT0, PT2, PT16, PT14, PT0}},
    /* P */ {5, {PT14, PT0, PT2, PT8, PT6}},
    /* Q */ {8, {PT0, PT2, PT13, PT15, PT14, PT0, PT12|SKIP, PT16}},
    /* R */ {6, {PT14, PT0, PT2, PT8, PT6, PT16}},
    /* S */ {6, {PT2, PT0, PT6, PT8, PT16, PT14}},
    /* T */ {4, {PT0, PT2, PT1|SKIP, PT15}},
    /* U */ {4, {PT0, PT14, PT16, PT2}},
    /* V */ {3, {PT0, PT15, PT2}},
    /* W */ {5, {PT0, PT14, PT12, PT16, PT2}},
    /* X */ {4, {PT0, PT16, PT2|SKIP, PT14}},
    /* Y */ {5, {PT0, PT4, PT2, PT4|SKIP, PT15}},
    /* Z */ {4, {PT0, PT2, PT14, PT16}},
};

/* punctuation */
static struct vector_polystruct V_punctuation[] =
{
    /* ! */ {8, {PT9, PT1, PT2, PT9, PT26|SKIP, PT29, PT30, PT26}},
    /* : */ {10, {PT1, 0xc02a, PT4, 0x402a, PT1,  PT9|SKIP, 0xc0d6, PT15, 0x40d6, PT9}},
    /* / */ {2, {PT14, PT2}},
    /* \ */ {2, {PT16, PT0}},
    /* _ */ {2, {PT14, PT16}},
    /* . */ {5, {PT12, PT31, PT15, PT32, PT12}},
    /* , */ {6, {PT12, PT31, PT15, PT29, PT32, PT12}},
    /* [ */ {4, {PT27, PT0, PT14, PT29}},
    /* ] */ {4, {PT28, PT2, PT16, PT30}},
    /* ' */ {4, {PT0, PT1, PT3, PT0}},
    /* - */ {2, {PT6, PT8}},
    /* & */ {7, {PT16, PT3, PT1, PT5, PT11, PT15, PT31}},
    /* ( */ {4, {PT27, PT18, PT11, PT29}},
    /* ) */ {4, {PT28, PT17, PT13, PT30}},
    /* % */ {12, {PT0, 0x6000, 0x6060, 0x0060, PT0,  PT16|SKIP, 0xa0ff, 0xa0a0, 0xffa0, PT16,  PT2|SKIP, PT14}},
    /* ~ */ {4, {PT6, PT23, 0xb096, PT8}},
    /* ` */ {4, {PT18, PT4, 0x902a, PT27}},
    /* @ */ {10, {PT16, PT2, PT0, PT14, PT16, 0xb0aa, 0xb056, 0x4056, 0x40aa, 0xb0aa}},
    /* # */ {8, {0x4000, 0x40ff, 0xc000|SKIP, 0xc0ff, 0x0060|SKIP, 0xff60, 0x00a0|SKIP, 0xffa0}},
    /* $ */ {8, {PT17, PT18, PT6, PT8, PT13, PT11, PT1|SKIP, PT15}},
    /* ^ */ {3, {PT3, PT1, PT5}},
    /* * */ {4, {0xb056, 0x40aa, 0x4056|SKIP, 0xb0aa}},
    /* ; */ {11, {PT1, 0xc02a, PT4, 0x402a, PT1,  PT9|SKIP, 0xc0d6, PT15, 0x40ff, 0x40d6, PT9}},
    /* ? */ {9, {PT18, PT1, PT17, PT34, PT9,  PT12|SKIP, PT29, PT30, PT12}},
    /* = */ {4, {PT3, PT5,  PT19|SKIP, PT10}},
    /* + */ {4, { 0x8822, 0x88dd, 0x0088|SKIP, 0xff88 }},
};

/* specials */
static struct vector_polystruct V_specials[] =
{
    /* ^ */ {4, {0x00ee, 0x8888, 0xffee, 0x00ee}},
    /* < */ {4, {0xff22, 0x6688, 0xffdd, 0xff22}},
    /* v */ {4, {0x0011, 0x8888, 0xff11, 0x0011}},
    /* > */ {4, {0x0022, 0xbb88, 0x00dd, 0x0022}},
};

static struct vector_polystruct *lookup[] = {
    [0x30] = 	V_digits + 0,  V_digits + 1,  V_digits + 2,  V_digits + 3, 
		V_digits + 4,  V_digits + 5,  V_digits + 6,  V_digits + 7,
		V_digits + 8,  V_digits + 9,

    [0x41] = 	V_alpha + 0,  V_alpha + 1,  V_alpha + 2,  V_alpha + 3, 
		V_alpha + 4,  V_alpha + 5,  V_alpha + 6,  V_alpha + 7, 
		V_alpha + 8,  V_alpha + 9,  V_alpha + 10, V_alpha + 11,
             	V_alpha + 12, V_alpha + 13, V_alpha + 14, V_alpha + 15, 
		V_alpha + 16, V_alpha + 17, V_alpha + 18, V_alpha + 19, 
		V_alpha + 20, V_alpha + 21, V_alpha + 22, V_alpha + 23,
             	V_alpha + 24, V_alpha + 25,

    [0x61] = 	V_alpha + 0,  V_alpha + 1,  V_alpha + 2,  V_alpha + 3, 
		V_alpha + 4,  V_alpha + 5,  V_alpha + 6,  V_alpha + 7, 
		V_alpha + 8,  V_alpha + 9,  V_alpha + 10, V_alpha + 11,
             	V_alpha + 12, V_alpha + 13, V_alpha + 14, V_alpha + 15, 
		V_alpha + 16, V_alpha + 17, V_alpha + 18, V_alpha + 19, 
		V_alpha + 20, V_alpha + 21, V_alpha + 22, V_alpha + 23,
             	V_alpha + 24, V_alpha + 25,

    ['!'] = V_punctuation + 0, 		[':'] = V_punctuation + 1,
    ['/'] = V_punctuation + 2,		['\\'] = V_punctuation + 3, 
    ['_'] = V_punctuation + 4, 		['.'] = V_punctuation + 5,
    [','] = V_punctuation + 6, 		['['] = V_punctuation + 7, 
    [']'] = V_punctuation + 8,		['\''] = V_punctuation + 9, 
    ['-'] = V_punctuation + 10, 	['&'] = V_punctuation + 11,
    ['('] = V_punctuation + 12, 	[')'] = V_punctuation + 13, 
    ['%'] = V_punctuation + 14, 	['~'] = V_punctuation + 15, 
    ['`'] = V_punctuation + 16, 	['@'] = V_punctuation + 17,
    ['#'] = V_punctuation + 18, 	['$'] = V_punctuation + 19, 
    ['^'] = V_punctuation + 20, 	['*'] = V_punctuation + 21, 
    [';'] = V_punctuation + 22, 	['?'] = V_punctuation + 23,
    ['='] = V_punctuation + 24,		['+'] = V_punctuation + 25,

    [250] = 	V_specials + 0, V_specials + 1, V_specials + 2, V_specials + 3
};

// Returns the number of pixels wide it was.
static void render_polystruct (ttk_surface srf, struct vector_polystruct *v, int x, int y, int cw, int ch,
			       ttk_color col) 
{
    int i;
    int lx = -1, ly = -1;

    cw--; ch--; // if a char is 5px wide the left edge will be at x+0 and right at x+4... cw is used below as
                // x+cw being right side, etc.

    for (i = 0; i < v->nv; i++) {
	int vv = v->v[i] & ~SKIP;
	if (v->v[i] & SKIP)
	    lx = ly = -1;
	if (lx != -1 && ly != -1) {
	    ttk_line (srf, lx, ly, x + (vv >> 8) * cw / 0xff, y + (vv & 0xff) * ch / 0xff, col);
	}
	lx = x + (vv >> 8) * cw / 0xff;
	ly = y + (vv & 0xff) * ch / 0xff;
    }
}

void pz_vector_string (ttk_surface srf, const char *string, int x, int y, int cw, int ch, int kern, ttk_color col) 
{
    int sx = x;
    while (*string) {
	if (*string == '\n') {
	    x = sx;
	    y += ch + (ch/4);
	} else {
	    if (lookup[*(unsigned const char *)string])
		render_polystruct (srf, lookup[*(unsigned const char *)string],
				x, y, cw, ch, col);
	    x += cw + (cw/8) + 1 + kern;
	}
	string++;
    }
}

void pz_vector_string_center (ttk_surface srf, const char *string, int x, int y, int cw, int ch,
			      int kern, ttk_color col) 
{
    int w = pz_vector_width (string, cw, ch, kern);
    x -= w/2;
    y -= ch/2;
    pz_vector_string (srf, string, x, y, cw, ch, kern, col);
}

int pz_vector_width (const char *string, int cw, int ch, int kern) 
{
    return (strlen (string) * cw) + ((strlen (string) - 1) * (cw/8 + kern + 1));
}
