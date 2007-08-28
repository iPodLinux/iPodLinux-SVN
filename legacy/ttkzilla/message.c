/*
 * Copyright (C) 2004 Bernard Leach
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PZMOD
#include "pz.h"

static int this_is_error;
static int writeable;

#if 0

// [text] mustn't be a const char *
extern char **wrap (int w, ttk_font font, char *text, int *out_lines); // from dialog.c
static char **msg_wrapats;
static TWindow *msgwin;
static TWidget *msgwid;
static int msg_lines;

void msg_draw (TWidget *wid, ttk_surface srf) { ttk_blit_image ((ttk_surface)wid->data, srf, wid->x, wid->y); }
int msg_down (TWidget *wid, int button) { return TTK_EV_DONE; }
int msg_timer (TWidget *wid) { return TTK_EV_DONE; }

void new_message_common_window(char *cmsg)
{
    char *msg;
    int freeit;
    int tfh = ttk_text_height (ttk_textfont);

    if (writeable)
	msg = cmsg, freeit = 0;
    else
	msg = strdup (cmsg), freeit = 1;

    msgwin = ttk_new_window();
    msgwin->w -= 40;
    msg_wrapats = wrap (msgwin->w, ttk_textfont, msg, &msg_lines);
    msgwin->h = msg_lines * (tfh + 1);
    msgwin->x = (ttk_screen->w - ttk_screen->wx - msgwin->w) / 2;
    msgwin->y = (ttk_screen->h - ttk_screen->wy - msgwin->h) / 2;
    msgwin->dirty = 0;
    
    if (this_is_error)
	ttk_ap_fillrect (msgwin->srf, ttk_ap_get ("dialog.bg"), 0, 0, msgwin->w, msgwin->h);
    else
	ttk_ap_fillrect (msgwin->srf, ttk_ap_get ("error.bg"), 0, 0, msgwin->w, msgwin->h);

    ttk_color textcolor = ttk_ap_getx (this_is_error? "dialog.fg" : "error.fg") -> color;

    msgwid = ttk_new_widget (0, 0);
    msgwid->data = ttk_new_surface (msgwin->w, msgwin->h, ttk_screen->bpp);
    msgwid->focusable = 1;
    msgwid->dirty = 1;
    msgwid->draw = msg_draw;
    msgwid->down = msg_down;
    msgwid->timer = msg_timer;
    msgwid->w = msgwin->w;
    msgwid->h = msgwin->h;
    ttk_widget_set_timer (msgwid, 10000); // popdown automatically in 10 sec

    // write text on msgwin->srf
    ttk_surface srf = (ttk_surface)msgwid->data;
    
#define BASELINESKIP (tfh + 1)
    int y = 0;
    char *End = msg + strlen (msg);
    char **end = msg_wrapats;
    char *p = msg;
    
    while (p < End) {
	if (!*end) {
	    ttk_text (srf, ttk_textfont, 0, y, textcolor, p);
	    p = End;
	} else {
	    char *nextstart = *end + 1;
	    char svch = *nextstart;
	    *nextstart = 0;
	    ttk_text (srf, ttk_textfont, 0, y, textcolor, p);
	    if ((**end != ' ') && (**end != '\t') && (**end != '\n') && (**end != '-')) { // midword brk
		ttk_pixel (srf, msgwin->w - 2, y + (BASELINESKIP / 2), ttk_makecol (BLACK));
	    }
	    p = nextstart;
	    *nextstart = svch;
	    y += BASELINESKIP;
	    end++;
	}
    }

    ttk_add_widget (msgwin, msgwid);
    ttk_popup_window (msgwin);
    ttk_run();
    ttk_free_window (msgwin);
}

#else

void new_message_common_window (char *message) 
{
    if (!this_is_error)
	DIALOG_MESSAGE (_("Information"), message, _("Ok"));
    else
	DIALOG_ERROR (_("Error"), message, _("Ok"));
}

#endif

void new_message_window(char *message)
{
	this_is_error = writeable = 0;
	new_message_common_window(message);
}

void pz_error(char *fmt, ...)
{
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsnprintf(msg, 1023, fmt, ap);
	va_end(ap);
	this_is_error = writeable = 1;
	new_message_common_window(msg);
	fprintf(stderr, "%s\n", msg);
}

void pz_perror(char *msg)
{
	char fullmsg[1024];

	snprintf(fullmsg, 1023, "%s: %s", msg, strerror(errno));
	this_is_error = 1;
	new_message_common_window(fullmsg);
	fprintf(stderr, "%s\n", fullmsg);
}
