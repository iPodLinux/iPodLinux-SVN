/*
 * Copyright (C) 2004 Bernard Leach
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


static GR_WINDOW_ID msg_wid;
static GR_GC_ID msg_gc;
static GR_SCREEN_INFO screen_info;
static char *msg_message;

static void msg_do_draw(void)
{
	GR_SIZE width, height, base;

	GrGetGCTextSize(msg_gc, msg_message, -1, GR_TFASCII, &width, &height, &base);

	GrRect(msg_wid, msg_gc, 1, 1, width + 8, height + 8);
	GrText(msg_wid, msg_gc, 5, base + 5, msg_message, -1, GR_TFASCII);
}

static void msg_event_handler(GR_EVENT *event)
{
	int i;

	switch (event->type) {
	case GR_EVENT_TYPE_EXPOSURE:
		msg_do_draw();
		break;

	case GR_EVENT_TYPE_KEY_DOWN:
		/* any key exits */
		pz_close_window(msg_wid);
		break;
	}
}

void new_message_window(char *message)
{
	GR_SIZE width, height, base;

	msg_message = message;

	GrGetScreenInfo(&screen_info);

	msg_gc = GrNewGC();
	GrSetGCUseBackground(msg_gc, GR_TRUE);
	GrSetGCForeground(msg_gc, WHITE);

	GrGetGCTextSize(msg_gc, message, -1, GR_TFASCII, &width, &height, &base);

	msg_wid = pz_new_window((screen_info.cols - (width + 10)) >> 1,
		(screen_info.rows - (height + 10)) >> 1,
		width + 10, height + 10, msg_event_handler);

	GrSelectEvents(msg_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(msg_wid);
}
