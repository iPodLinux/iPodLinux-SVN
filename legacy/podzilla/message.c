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

#include "pz.h"

static GR_TIMER_ID msg_timer;
static GR_WINDOW_ID msg_wid = 0;
static GR_GC_ID msg_gc;
static char **msglines;
static int linenum;
static char this_is_error;

static void msg_build_msg(char *msg_message)
{
	char *curtextptr;
	int currentLine = 0;

	curtextptr = msg_message;
	if((msglines = (char **)malloc(sizeof(char *) * 8))==NULL) {
		fprintf(stderr, "malloc failed");
		return;
	}
	
	while(*curtextptr != '\0' || currentLine > 8) {
		char *sol;

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

			GrGetGCTextSize(msg_gc, sol, curtextptr - sol +
					1, GR_TFASCII, &width, &height, &base);

			if(width > screen_info.cols - 15) {
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

		if((msglines[currentLine] = malloc((curtextptr-sol + 1) *
						sizeof(char)))==NULL) {
			fprintf(stderr, "malloc failed");
			return;
		}
		snprintf(msglines[currentLine], curtextptr-sol+1, "%s", sol);
		currentLine++;
	}
	linenum = currentLine;
}


static void msg_do_draw()
{
	GR_WINDOW_INFO winfo;
	int i;

	GrGetWindowInfo(msg_wid, &winfo);

	/* fill the background */
	GrSetGCForeground(msg_gc, appearance_get_color( 
				(this_is_error)?CS_ERRORBG:CS_MESSAGEBG ));
	GrFillRect( msg_wid, msg_gc, 0, 0, winfo.width, winfo.height );

	GrSetGCForeground(msg_gc, appearance_get_color( CS_MESSAGELINE ));
	GrRect(msg_wid, msg_gc, 1, 1, winfo.width - 2, winfo.height - 2);

	GrSetGCForeground(msg_gc, appearance_get_color( CS_MESSAGEFG ));
	for(i=linenum; i; i--)
		GrText(msg_wid, msg_gc, 5, (((winfo.height-10) /
				linenum) * i), msglines[i-1], -1,
				GR_TFASCII);
}

static void msg_destroy_msg()
{
	int i;

	pz_close_window(msg_wid);
	GrDestroyGC(msg_gc);
	GrDestroyTimer(msg_timer);
	for(i = linenum; i; i--)
		free(msglines[i-1]);
	free(msglines);
}

static int msg_do_keystroke(GR_EVENT * event)
{
	int ret = 1;
	switch (event->type) {
	case GR_EVENT_TYPE_TIMER:
	case GR_EVENT_TYPE_KEY_DOWN:
		msg_destroy_msg();
		msg_wid = 0;
	}

	return ret;
}
	

void new_message_common_window(char *message)
{
	GR_SIZE width, height, base;
	int i, maxwidth;

	if (msg_wid) {
		msg_destroy_msg();
	}

	msg_gc = pz_get_gc(1);
	GrSetGCUseBackground(msg_gc, GR_FALSE);
	GrSetGCForeground(msg_gc, appearance_get_color( CS_MESSAGEFG ));

	msg_build_msg(message);
	maxwidth = 0;
	for(i=linenum; i; i--) {
		GrGetGCTextSize(msg_gc, msglines[i-1], -1, GR_TFASCII, &width,
				&height, &base);
		if(width > maxwidth)
			maxwidth = width;
	}

	msg_wid = pz_new_window((screen_info.cols - (maxwidth + 10)) >> 1,
		(screen_info.rows - (((height + 3) * linenum) + 10)) >> 1,
		maxwidth + 10, ((height + 3) * linenum) + 10,
		msg_do_draw, msg_do_keystroke);

	GrSelectEvents(msg_wid, GR_EVENT_MASK_EXPOSURE| GR_EVENT_MASK_KEY_UP|
			GR_EVENT_MASK_KEY_DOWN| GR_EVENT_MASK_TIMER);

	GrMapWindow(msg_wid);

	/* window will auto-close after 6 seconds */
	msg_timer = GrCreateTimer(msg_wid, 6000);
}

void new_message_window(char *message)
{
	this_is_error = 0;
	new_message_common_window(message);
}

void pz_error(char *fmt, ...)
{
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsnprintf(msg, 1023, fmt, ap);
	va_end(ap);
	this_is_error = 1;
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
