/*
 * Copyright (C) 2005 Jonathan Bettencourt (jonrelay)
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
#define _XOPEN_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

/* Some systems have util.h, some systems have pty.h, some systems have libutil.h. Feh. */

/* Since we don't have autotools assistance here, we'll have to guess as best as we can.
 * General Guidelines:
 *   linux: pty.h
 *   osx || netbsd || openbsd: util.h
 *   freebsd: libutil.h
 *   qnx: unix.h
 *   solaris: not included
 */

#if defined(IPOD) || defined (__linux__)
#include <pty.h>
#include <utmp.h>
#elif defined(__APPLE__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <util.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#elif defined (__QNX__)
#include <unix.h>
#elif defined(solaris) || (defined(__SVR4) && defined(sun))
#warning "Solaris doesn't support this and needs manual implementation here"
#else
#warning "Unknown operating system, this build will probably fail, sorry. Try adjusting the includes."
#endif

#include <termios.h>
#include <strings.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#define TERMINAL_CONF_FONTNAME 1
#define TERMINAL_CONF_FONTSIZE 2

/* - - terminal widget - - */
static PzModule * terminal_module;
static PzConfig * terminal_conf;
static ttk_font terminal_font;
static PzWindow * terminal_window = 0;
static TWidget * terminal_widget = 0;

/* - - terminal params - - */
static int terminal_tiheight = 0;
static int terminal_h = 0;
static int terminal_w = 0;
static int terminal_ch = 0;
static int terminal_cw = 0;
static int terminal_rows = 0;
static int terminal_cols = 0;
static int terminal_cells = 0;
static int * terminal_buf = 0;

/* - - terminal state - - */
static int terminal_x = 0;
static int terminal_y = 0;
static struct winsize terminal_win;
static int terminal_scroll_top = 0;
static int terminal_scroll_bot = 0;
static int terminal_attr = 0;
static int terminal_escape = 0;
static char terminal_escape_seq[30] = {0};

/* - - pseudoterminal process - - */
static pid_t terminal_child = 0;
static int terminal_master = 0;
static int terminal_slave = 0;
static char terminal_pty_name[30] = {0};

/* ======== basic terminal control ======== */

void terminal_render(TWidget * wid, ttk_surface srf)
{
	uc16 uc[2] = {0, 0};
	int x = wid->x;
	int y = wid->y;
	int cc = 0;
	int p = terminal_y*terminal_cols + terminal_x;
	int i, a;
	TApItem * bg = ttk_ap_getx("window.bg");
	TApItem * fg = ttk_ap_getx("window.fg");
	TApItem * cu = ttk_ap_getx("input.cursor");
	ttk_ap_fillrect(srf, bg, x, y, x+wid->w, y+wid->h);
	for (i=0; i<terminal_cells; i++) {
		uc[0] = (terminal_buf[i] & 0xFFFF);
		a = (terminal_buf[i] >> 16);
		if (a & 0x01) {
			ttk_ap_fillrect(srf, fg, x, y, x+terminal_cw, y+terminal_ch);
			ttk_text_uc16(srf, terminal_font, x, y, bg->color, uc);
			if (a & 0x02) { ttk_text_uc16(srf, terminal_font, x+1, y, bg->color, uc); }
			if (a & 0x04) { ttk_line(srf, x, y+terminal_ch-1, x+terminal_cw, y+terminal_ch-1, bg->color); }
		} else {
			ttk_text_uc16(srf, terminal_font, x, y, fg->color, uc);
			if (a & 0x02) { ttk_text_uc16(srf, terminal_font, x+1, y, fg->color, uc); }
			if (a & 0x04) { ttk_line(srf, x, y+terminal_ch-1, x+terminal_cw, y+terminal_ch-1, fg->color); }
		}
		if (i == p) {
			ttk_ap_fillrect(srf, cu, x, y+terminal_ch-2, x+terminal_cw, y+terminal_ch);
		}
		cc++;
		if (cc >= terminal_cols) {
			x = wid->x;
			cc = 0;
			y += terminal_ch;
		} else {
			x += terminal_cw;
		}
	}
}

void terminal_clear(void)
{
	int i;
	for (i=0; i<terminal_cells; i++) {
		terminal_buf[i]=0;
	}
	terminal_x=0;
	terminal_y=0;
	terminal_widget->dirty=1;
}

void terminal_scrollup(void)
{
	int i, j;
	for (i=((terminal_scroll_top+1)*terminal_cols), j=(terminal_scroll_top*terminal_cols); i<(terminal_scroll_bot*terminal_cols); i++, j++) {
		terminal_buf[j]=terminal_buf[i];
	}
	for (i=((terminal_scroll_bot-1)*terminal_cols); i<(terminal_scroll_bot*terminal_cols); i++) {
		terminal_buf[i]=0;
	}
	terminal_widget->dirty=1;
}

void terminal_scrolldown(void)
{
	int i, j;
	for (i=(terminal_scroll_bot*terminal_cols), j=((terminal_scroll_bot-1)*terminal_cols); j>=(terminal_scroll_top*terminal_cols); i--, j--) {
		terminal_buf[i]=terminal_buf[j];
	}
	for (i=(terminal_scroll_top*terminal_cols); i<((terminal_scroll_top+1)*terminal_cols); i++) {
		terminal_buf[i]=0;
	}
	terminal_widget->dirty=1;
}

void terminal_addch(int ch)
{
	if (terminal_x >= terminal_cols) {
		terminal_x=0;
		terminal_y++;
		if (terminal_y >= terminal_rows) {
			terminal_scrollup();
			terminal_y = terminal_rows-1;
		}
	}
	terminal_buf[terminal_y*terminal_cols + terminal_x] = ch;
	terminal_x++;
	terminal_widget->dirty=1;
}

void terminal_setfont(char * fname, int fsize)
{
	ttk_done_font(terminal_font);
	terminal_font = ttk_get_font(fname, fsize);
	if (terminal_window) {
		terminal_win.ws_ypixel = terminal_h = terminal_window->h-terminal_tiheight;
		terminal_win.ws_xpixel = terminal_w = terminal_window->w;
		terminal_ch = ttk_text_height(terminal_font);
		terminal_cw = ttk_text_width(terminal_font, "M");
		terminal_win.ws_row = terminal_scroll_bot = terminal_rows = terminal_h/terminal_ch;
		terminal_win.ws_col = terminal_cols = terminal_w/terminal_cw;
		terminal_cells = terminal_rows * terminal_cols;
		if (terminal_buf) {
			free(terminal_buf);
			terminal_buf = (int *)calloc(terminal_cells, sizeof(int));
		}
		terminal_scroll_top = terminal_x = terminal_y = 0;
		if (terminal_master && terminal_child) {
			ioctl(terminal_master, TIOCSWINSZ, &terminal_win);
			kill(terminal_child, SIGWINCH);
		}
	}
	pz_set_string_setting(terminal_conf, TERMINAL_CONF_FONTNAME, fname);
	pz_set_int_setting   (terminal_conf, TERMINAL_CONF_FONTSIZE, fsize);
	pz_save_config(terminal_conf);
}

/* ======== VT102 emulation routines ======== */

void terminal_get_esc_seq_parms(char * seq, int * p1, int d1, int * p2, int d2, int * p3, int d3)
{
	int i=0;
	int p=0;
	int p1d = 1;
	int p2d = 1;
	int p3d = 1;
	char ch;
	if (p1) *p1 = d1;
	if (p2) *p2 = d2;
	if (p3) *p3 = d3;
	for (i=0; i<(int)strlen(seq); i++) {
		ch = seq[i];
		if (ch == ';') {
			p++;
		} else if (ch >= '0' && ch <= '9') {
			switch (p) {
			case 0:
				if (p1) {
					if (p1d) { *p1 = 0; p1d = 0; }
					*p1 = ((*p1)*10 + (ch-'0'));
				}
				break;
			case 1:
				if (p2) {
					if (p2d) { *p2 = 0; p2d = 0; }
					*p2 = ((*p2)*10 + (ch-'0'));
				}
				break;
			case 2:
				if (p3) {
					if (p3d) { *p3 = 0; p3d = 0; }
					*p3 = ((*p3)*10 + (ch-'0'));
				}
				break;
			}
		}
	}
}

void terminal_handlech(unsigned short ch)
{
	int i;
	if (terminal_escape) {
		/* inside an escape sequence */
		/*
			We'll be emulating an incomplete VT102.
			All VT102 escape sequences are caught, but not all of them do something.
			We leave stubs for unimplemented sequences.
			We have enough for 'man' to work, but not enough for 'pico' to work.
			
			Things left unimplemented:
				tab stops
				alternate character sets
				editing commands
				printing
				modes (soft switches activated by ESC [ n h or ESC [ n l)
				keypad modes
				double-width characters and double-height lines
				self-test
				VT52 mode
				XON/XOFF flow control
		*/
		if (ch==24 || ch==26) {
			/* cancel sequence */
			terminal_escape = 0;
			terminal_escape_seq[0]=0;
			terminal_addch(9254);
			return;
		} else if (ch==27) {
			/* cancel sequence and start a new one */
			terminal_escape = 1;
			terminal_escape_seq[0]=0;
			return;
		}
		if (ch == '[') {
			terminal_escape = 2;
		} else if (ch == '#') {
			terminal_escape = 3;
		} else if (ch == '(') {
			terminal_escape = 4;
		} else if (ch == ')') {
			terminal_escape = 5;
		} else if (((ch>='A')&&(ch<='Z'))||((ch>='a')&&(ch<='z'))) {
			char resp[40];
			int parm, parm1, parm2;
			switch (terminal_escape) {
			case 1: /* sequence of the form ESC X where X is a letter */
				switch (ch) {
				case 'E': /* new line */
					terminal_x = 0;
				case 'D': /* index */
					terminal_y++;
					if (terminal_y >= terminal_scroll_bot) {
						terminal_scrollup();
						terminal_y = terminal_scroll_bot-1;
					}
					terminal_widget->dirty = 1;
					break;
				case 'H': /* set tab stop */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC H - SET TAB STOP */
					break;
				case 'M': /* reverse index */
					terminal_y--;
					if (terminal_y < terminal_scroll_top) {
						terminal_scrolldown();
						terminal_y = terminal_scroll_top;
					}
					terminal_widget->dirty = 1;
					break;
				case 'N': /* G2 for one character */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC N - USE G2 FOR 1 CHAR */
					break;
				case 'O': /* G3 for one character */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC O - USE G3 FOR 1 CHAR */
					break;
				case 'Z': /* identify terminal */
					resp[0] = 27;
					resp[1] = '[';
					resp[2] = '?';
					resp[3] = '6';
					resp[4] = 'c';
					write(terminal_master, resp, 5);
					break;
				case 'c': /* reset */
					terminal_scroll_top = 0;
					terminal_scroll_bot = terminal_rows;
					terminal_attr = 0;
					terminal_clear();
					break;
				case 'C': /* get cols - specific to Podzilla Terminal */
					resp[0] = '0'+((terminal_cols/100) % 10);
					resp[1] = '0'+((terminal_cols/10) % 10);
					resp[2] = '0'+(terminal_cols % 10);
					resp[3] = '\n';
					write(terminal_master, resp, 4);
					break;
				case 'R': /* get rows - specific to Podzilla Terminal */
					resp[0] = '0'+((terminal_rows/100) % 10);
					resp[1] = '0'+((terminal_rows/10) % 10);
					resp[2] = '0'+(terminal_rows % 10);
					resp[3] = '\n';
					write(terminal_master, resp, 4);
					break;
				}
				break;
			case 2: /* sequence of the form ESC [ 0 ; 0 ; 0 X where X is a letter */
				switch (ch) {
				case 'A': /* cursor up */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, 1, &parm2, 1);
					terminal_y -= parm;
					if (terminal_y < terminal_scroll_top) terminal_y=terminal_scroll_top;
					terminal_widget->dirty = 1;
					break;
				case 'B': /* cursor down */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, 1, &parm2, 1);
					terminal_y += parm;
					if (terminal_y >= terminal_scroll_bot) terminal_y=terminal_scroll_bot-1;
					terminal_widget->dirty = 1;
					break;
				case 'C': /* cursor right */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, 1, &parm2, 1);
					terminal_x += parm;
					while (terminal_x >= terminal_cols) {
						terminal_x = terminal_cols-1;
						/*terminal_x -= terminal_cols;
						terminal_y++;
						if (terminal_y >= terminal_scroll_bot) {
							terminal_y = terminal_scroll_bot-1;
						}*/
					}
					terminal_widget->dirty = 1;
					break;
				case 'D': /* cursor left */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, 1, &parm2, 1);
					terminal_x -= parm;
					while (terminal_x < 0) {
						terminal_x += terminal_cols;
						terminal_y--;
						if (terminal_y < terminal_scroll_top) {
							/*for (i=0; i<terminal_cells; i++) terminal_buf[i] = 0;*/
							terminal_y = terminal_scroll_bot-1;
						}
					}
					terminal_widget->dirty = 1;
					break;
				case 'F': /* switch fonts - specific to Podzilla Terminal */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 0, &parm1, 0, &parm2, 0);
					switch (parm) {
					case 0:
						terminal_setfont("Fixed 6x13",13);
						break;
					case 1:
						terminal_setfont("Fixed 6x12",12);
						break;
					case 2:
						terminal_setfont("Fixed 6x10",10);
						break;
					case 3:
						terminal_setfont("Fixed 6x9",9);
						break;
					case 4:
						terminal_setfont("Fixed 5x8",8);
						break;
					case 5:
						terminal_setfont("Fixed 5x7",7);
						break;
					case 6:
						terminal_setfont("Fixed 4x6",6);
						break;
					case 7:
						terminal_setfont("Fixed 7x13",13);
						break;
					case 8:
						terminal_setfont("Fixed 7x14",14);
						break;
					case 9:
						terminal_setfont("Fixed 8x13",13);
						break;
					case 10:
						terminal_setfont("Fixed 9x15",15);
						break;
					case 11:
						terminal_setfont("Fixed 9x18",18);
						break;
					case 12:
						terminal_setfont("Fixed 10x20",20);
						break;
					case 13:
					case 14:
					case 15:
						terminal_setfont("Unifont",12);
						break;
					case 16:
						terminal_setfont("Apple II 40 Column",9);
						break;
					case 17:
						terminal_setfont("Apple II 80 Column",18);
						break;
					case 18:
						terminal_setfont("Topaz",8);
						break;
					case 19:
						terminal_setfont("Topaz Doubled",16);
						break;
					case 20:
						terminal_setfont("Topaz Sans",8);
						break;
					case 21:
						terminal_setfont("Topaz Sans Doubled",16);
						break;
					case 22:
						terminal_setfont("Opal",9);
						break;
					case 23:
						terminal_setfont("Opal Double",18);
						break;
					case 24:
						terminal_setfont("Diamond",12);
						break;
					case 25:
						terminal_setfont("Onyx",9);
						break;
					case 26:
						terminal_setfont("Peridot",7);
						break;
					case 27:
						terminal_setfont("Sabine Doscbthm",9);
						break;
					case 28:
						terminal_setfont("Sabine Doscbthm Black",9);
						break;
					case 29:
						terminal_setfont("Sabine Doscbthm Script",9);
						break;
					case 30:
						terminal_setfont("Sabine Doscbthm Serif",9);
						break;
					case 31:
						terminal_setfont("Sabine Doscbthm Smallcaps",9);
						break;
					}
					terminal_widget->dirty = 1;
					break;
				case 'H':
				case 'f': /* direct addressing */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, 1, &parm2, 1);
					terminal_x = parm1-1;
					terminal_y = parm-1;
					terminal_widget->dirty = 1;
					break;
				case 'J': /* clear screen */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 0, &parm1, 0, &parm2, 0);
					switch (parm) {
					case 0:
						for (i=(terminal_x+(terminal_y*terminal_cols)); i<terminal_cells; i++) terminal_buf[i] = 0;
						break;
					case 1:
						for (i=0; i<=(terminal_x+(terminal_y*terminal_cols)); i++) terminal_buf[i] = 0;
						break;
					case 2:
						for (i=0; i<terminal_cells; i++) terminal_buf[i] = 0;
						break;
					}
					terminal_widget->dirty = 1;
					break;
				case 'K': /* clear line */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 0, &parm1, 0, &parm2, 0);
					switch (parm) {
					case 0:
						for (i=(terminal_x+(terminal_y*terminal_cols)); i<((terminal_y+1)*terminal_cols); i++) terminal_buf[i] = 0;
						break;
					case 1:
						for (i=(terminal_y*terminal_cols); i<=(terminal_x+(terminal_y*terminal_cols)); i++) terminal_buf[i] = 0;
						break;
					case 2:
						for (i=(terminal_y*terminal_cols); i<((terminal_y+1)*terminal_cols); i++) terminal_buf[i] = 0;
						break;
					}
					terminal_widget->dirty = 1;
					break;
				case 'L': /* insert line */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, 1, &parm2, 1);
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ n L - INSERT LINE */
					break;
				case 'M': /* delete line */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, 1, &parm2, 1);
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ n M - DELETE LINE */
					break;
				case 'P': /* delete characters */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, 1, &parm2, 1);
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ n P - DELETE CHARACTER */
					break;
				case 'c': /* device attributes */
					resp[0] = 27;
					resp[1] = '[';
					resp[2] = '?';
					resp[3] = '6';
					resp[4] = 'c';
					write(terminal_master, resp, 5);
					break;
				case 'g': /* clear tab stop */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 0, &parm1, 0, &parm2, 0);
					switch (parm) {
					case 0: /* clear one */
						/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ 0 g - CLEAR TAB STOP */
						break;
					case 3: /* clear all */
						/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ 3 g - CLEAR ALL TAB STOPS */
						break;
					}
					break;
				case 'h': /* set mode */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ [?] n h - SET MODE */
					break;
				case 'i': /* printing */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ [?] n i - PRINT */
					break;
				case 'l': /* reset mode */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ [?] n l - RESET MODE */
					break;
				case 'm': /* set attributes */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 0, &parm1, -1, &parm2, -1);
					if (parm == 0 || parm1 == 0 || parm2 == 0) terminal_attr = 0;
					if (parm == 7 || parm1 == 7 || parm2 == 7) terminal_attr |= 0x01;
					if (parm ==27 || parm1 ==27 || parm2 ==27) terminal_attr &= (~0x01);
					if (parm == 1 || parm1 == 1 || parm2 == 1) terminal_attr |= 0x02;
					if (parm ==21 || parm1 ==21 || parm2 ==21) terminal_attr &= (~0x02);
					if (parm == 4 || parm1 == 4 || parm2 == 4) terminal_attr |= 0x04;
					if (parm ==24 || parm1 ==24 || parm2 ==24) terminal_attr &= (~0x04);
					break;
				case 'n': /* device status report */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 0, &parm1, 0, &parm2, 0);
					switch (parm) {
					case '5':
						resp[0] = 27;
						resp[1] = '[';
						resp[2] = 'O';
						resp[3] = 'n';
						write(terminal_master, resp, 4);
						break;
					case '6':
						snprintf(resp, 40, "\x1B[%d;%dR", terminal_y+1, terminal_x+1);
						write(terminal_master, resp, strlen(resp));
						break;
					}
					break;
				case 'q': /* keyboard LED control */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ n q - KEYBOARD LED */
					break;
				case 'r': /* set scrolling area */
					terminal_get_esc_seq_parms(terminal_escape_seq, &parm, 1, &parm1, terminal_rows, &parm2, 0);
					terminal_scroll_bot = parm1;
					terminal_scroll_top = parm-1;
					if (terminal_scroll_bot > terminal_rows) { terminal_scroll_bot = terminal_rows; }
					break;
				case 'y': /* self-test */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC [ 2 ; n y - VT102 SELF-TEST */
					break;
				}
				break;
			case 3: /* sequence of the form ESC # X where X is a letter */
				break;
			case 4: /* sequence of the form ESC ( X where X is a letter */
				switch (ch) {
				case 'A':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ( A - US FOR G0 */
					break;
				case 'B':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ( B - UK FOR G0 */
					break;
				}
				break;
			case 5: /* sequence of the form ESC ) X where X is a letter */
				switch (ch) {
				case 'A':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ) A - US FOR G1 */
					break;
				case 'B':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ) B - UK FOR G1 */
					break;
				}
				break;
			}
			terminal_escape = 0;
			terminal_escape_seq[0] = 0;
		} else {
			switch (terminal_escape) {
			case 1: /* sequence of the form ESC X where X is not a letter */
				{
					static int xsave = 0;
					static int ysave = 0;
					static int asave = 0;
					switch (ch) {
					case '7':
						xsave = terminal_x;
						ysave = terminal_y;
						asave = terminal_attr;
						break;
					case '8':
						terminal_x = xsave;
						terminal_y = ysave;
						terminal_attr = asave;
						terminal_widget->dirty = 1;
						break;
					case '=':
						/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC = - APPLICATION KEYPAD MODE */
						break;
					case '>':
						/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC > - NUMERIC KEYPAD MODE */
						break;
					}
				}
				terminal_escape = 0;
				terminal_escape_seq[0] = 0;
				break;
			case 2: /* middle of a sequence of the form ESC [ 0 ; 0 ; 0 X where X is a letter */
				{
					int l = strlen(terminal_escape_seq);
					terminal_escape_seq[l++] = ch;
					terminal_escape_seq[l++] = 0;
				}
				break;
			case 3: /* sequence of the form ESC # X where X is not a letter */
				switch (ch) {
				case '3': /* double-height with top line */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC # 3 - DOUBLE HEIGHT TOP */
					break;
				case '4': /* double-height with bottom line */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC # 4 - DOUBLE HEIGHT BOTTOM */
					break;
				case '5': /* single width and height */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC # 5 - SINGLE HEIGHT WIDTH */
					break;
				case '6': /* double-width */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC # 6 - DOUBLE WIDTH */
					break;
				case '8': /* screen adjustments */
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC # 8 - SCREEN ADJUSTMENTS */
					break;
				}
				terminal_escape = 0;
				terminal_escape_seq[0] = 0;
				break;
			case 4: /* sequence of the form ESC ( X where X is not a letter */
				switch (ch) {
				case '0':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ( 0 - LINE DRAWING FOR G0 */
					break;
				case '1':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ( 1 - ALTERNATE FOR G0 */
					break;
				case '2':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ( 2 - ALTERNATE LINE DRAWING FOR G0 */
					break;
				}
				terminal_escape = 0;
				terminal_escape_seq[0] = 0;
				break;
			case 5: /* sequence of the form ESC ) X where X is not a letter */
				switch (ch) {
				case '0':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ) 0 - LINE DRAWING FOR G1 */
					break;
				case '1':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ) 1 - ALTERNATE FOR G1 */
					break;
				case '2':
					/* !!! UNIMPLEMENTED ESCAPE SEQUENCE - ESC ) 2 - ALTERNATE LINE DRAWING FOR G1 */
					break;
				}
				terminal_escape = 0;
				terminal_escape_seq[0] = 0;
				break;
			}
		}
	} else if (ch >= 32) {
		/* printable character */
		terminal_addch(ch+(terminal_attr<<16));
	} else {
		/* control character */
		switch (ch) {
		case 7:
			ttk_click();
			return;
			break;
		case 8:
			terminal_x--;
			if (terminal_x<0) {
				terminal_x = terminal_cols-1;
				terminal_y--;
				if (terminal_y < terminal_scroll_top) {
					/*for (i=0; i<terminal_cells; i++) terminal_buf[i] = 0;*/
					terminal_y = terminal_scroll_bot-1;
				}
			}
			break;
		case 9:
			do {
				if (terminal_x >= terminal_cols) {
					terminal_x = 0;
					terminal_y++;
					if (terminal_y >= terminal_scroll_bot) {
						terminal_scrollup();
						terminal_y = terminal_scroll_bot-1;
					}
				}
				terminal_x++;
			} while (terminal_x % 8);
			break;
		case 10:
		case 11:
		case 12:
			terminal_y++;
			if (terminal_y >= terminal_scroll_bot) {
				terminal_scrollup();
				terminal_y = terminal_scroll_bot-1;
			}
			break;
		case 13:
			terminal_x=0;
			break;
		case 14:
			/* !!! UNIMPLEMENTED CONTROL CHAR - SO - USE G1 CHARACTER SET */
			break;
		case 15:
			/* !!! UNIMPLEMENTED CONTROL CHAR - SI - USE G0 CHARACTER SET */
			break;
		case 17:
			/* !!! UNIMPLEMENTED CONTROL CHAR - DC1 - XON */
			break;
		case 19:
			/* !!! UNIMPLEMENTED CONTROL CHAR - DC3 - XOFF */
			break;
		case 24:
		case 26:
			terminal_escape = 0;
			terminal_escape_seq[0]=0;
			terminal_addch(9254);
			break;
		case 27:
			terminal_escape = 1;
			terminal_escape_seq[0]=0;
			return;
			break;
		default:
			return;
			break;
		}
		terminal_widget->dirty=1;
	}
}

/* ======== terminal events ======== */

void terminal_childdied()
{
	signal(SIGCHLD, SIG_IGN);
	pz_close_window(terminal_window);
	terminal_widget = 0;
	terminal_window = 0;
}

void terminal_destroy(TWidget * wid)
{
	/* terminate child */
	signal(SIGCHLD, SIG_IGN);
	kill(terminal_child, SIGKILL);
	if (terminal_buf) { free(terminal_buf); terminal_buf = 0; }
	ttk_input_end();
}

int terminal_input(TWidget * wid, int ch)
{
	/* write to stdin of child */
	unsigned char c = ch;
	unsigned char ct[4];
	switch (ch) {
	case TTK_INPUT_END:
		pz_close_window(wid->win);
		terminal_widget = 0;
		terminal_window = 0;
		/* TTK calls terminal_destroy for us */
		break;
	case TTK_INPUT_LEFT:
		ct[0] = 27;
		ct[1] = '[';
		ct[2] = 'D';
		write(terminal_master, ct, 3);
		break;
	case TTK_INPUT_RIGHT:
		ct[0] = 27;
		ct[1] = '[';
		ct[2] = 'C';
		write(terminal_master, ct, 3);
		break;
	case TTK_INPUT_ENTER:
		c = 10;
	default:
		write(terminal_master, &c, 1);
		break;
	}
	return TTK_EV_CLICK;
}

int terminal_timer(TWidget * wid)
{
	/* read from stdout of child */
	int q = 1;
	unsigned char c;
	fd_set set;
	struct timeval timeout;
	while (q) {
		FD_ZERO(&set);
		FD_SET(terminal_master, &set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		select(FD_SETSIZE, &set, NULL, NULL, &timeout);
		if (FD_ISSET(terminal_master, &set)) {
			if (read(terminal_master, &c, 1) > 0) {
				terminal_handlech(c);
			} else { q = 0; }
		} else { q = 0; }
	}
	return 0;
}

/* ======== pseudoterminal initialization ======== */

#ifndef IPOD
/* For some reason vfork doesn't compile on my computer, so this is a kludge. */
#define vfork fork
#endif

int terminal_pty_open(int * master, int * slave, char * pty_name, struct termios * tios, struct winsize * twin, char * termtype, char * signon, char * exec_path, char * exec_name, char * exec_arg)
{
	int slavenum;
	int p;
	if (openpty(master, &slavenum, pty_name, tios, twin)) return -1;
	p = vfork();
	if (p == -1) {
		close(*master);
		return -1; /* could not vfork */
	} else if (p) {
		close(slavenum);
		return p;
	} else {
		close(*master);
		login_tty(slavenum);
		if (twin) {
			char se[50];
			snprintf(se, 50, "LINES=%d", twin->ws_row);
			putenv(se);
			snprintf(se, 50, "COLUMNS=%d", twin->ws_col);
			putenv(se);
		}
		if (termtype) {
			char se[50];
			snprintf(se, 50, "TERM=%s", termtype);
			putenv(se);
		}
		close(0);
		close(1);
		close(2);
		*slave = open(pty_name, O_RDWR);
		if (*slave < 0) {
			_exit(0);
		}
		dup2(*slave,0);
		dup2(*slave,1);
		dup2(*slave,2);
		if (signon) {
			printf("%s\n", signon);
		}
		if ( execl(exec_path, exec_name, exec_arg, 0) ) {
			close(*slave);
			_exit(0);
		}
	}
	return p; /* so control doesn't reach end of non-void function */
}

/* ======== program initialization ======== */

#ifdef IPOD
#define TERMINAL_EXEC_PATH ((char *)pz_module_get_datapath(terminal_module, "minix-sh"))
#define TERMINAL_EXEC_NAME ("minix-sh")
#else
#define TERMINAL_EXEC_PATH ("/bin/sh")
#define TERMINAL_EXEC_NAME ("sh")
#endif

PzWindow * new_terminal_window_with(char * path, char * argv0, char * argv1)
{
	PzWindow * ret;
	TWidget * wid;
	pid_t p;
	
	/* - - create window - - */
	terminal_window = ret = pz_new_window(_("Terminal"), PZ_WINDOW_NORMAL);
	terminal_widget = wid = ttk_new_widget(0,0);
	wid->w = ret->w;
	wid->h = ret->h;
	wid->draw = terminal_render;
	wid->input = terminal_input;
	wid->timer = terminal_timer;
	wid->destroy = terminal_destroy;
	wid->focusable = 1;
	ttk_add_widget(ret, wid);
	ret = pz_finish_window(ret);
	
	/* - - set up terminal - - */
	terminal_tiheight = pz_start_input_for(ret);
	terminal_win.ws_ypixel = terminal_h = ret->h-terminal_tiheight;
	terminal_win.ws_xpixel = terminal_w = ret->w;
	terminal_ch = ttk_text_height(terminal_font);
	terminal_cw = ttk_text_width(terminal_font, "M");
	terminal_win.ws_row = terminal_scroll_bot = terminal_rows = terminal_h/terminal_ch;
	terminal_win.ws_col = terminal_cols = terminal_w/terminal_cw;
	terminal_cells = terminal_rows * terminal_cols;
	if (terminal_buf) free(terminal_buf);
	terminal_buf = (int *)calloc(terminal_cells, sizeof(int));
	terminal_scroll_top = terminal_x = terminal_y = 0;
	terminal_attr = 0;
	terminal_escape = 0;
	terminal_escape_seq[0]=0;
	
	/* - - open the pseudoterminal - - */
	p = terminal_pty_open(&terminal_master, &terminal_slave, terminal_pty_name, 0, &terminal_win, "vt102", _("Welcome to iPodLinux!"), path, argv0, argv1);
	if (p < 0) {
		pz_error("Could not open a pseudoterminal.");
		terminal_child = 0;
	} else if (p > 0) {
		terminal_child = p;
		signal(SIGCHLD, terminal_childdied);
		ttk_widget_set_timer(wid, 10);
	}
	
	return ret;
}

PzWindow * new_terminal_window(void)
{
	return new_terminal_window_with(TERMINAL_EXEC_PATH, TERMINAL_EXEC_NAME, 0);
}

void terminal_mod_init(void)
{
	const char * f; int s;
	terminal_module = pz_register_module("terminal", 0);
	terminal_conf = pz_load_config(pz_module_get_cfgpath(terminal_module, "terminal.conf"));
	f = pz_get_string_setting(terminal_conf,TERMINAL_CONF_FONTNAME);
	s = pz_get_int_setting   (terminal_conf,TERMINAL_CONF_FONTSIZE);
	terminal_font = ttk_get_font((f?f:"Fixed 6x13"), (s?s:13));
	pz_menu_add_action("/Extras/Utilities/Terminal", new_terminal_window);
}

PZ_MOD_INIT(terminal_mod_init)

