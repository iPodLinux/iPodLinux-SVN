/*
 * Copyright (C) 2004 Jens Taprogge
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
#include <itunesdb.h>

#include "pz.h"
#include "ipod.h"
#include "btree.h"
#include "itunes_db.h"

struct menulist {
	void		*user;
	int		 (*get_prev)(struct menulist *);
	int		 (*get_next)(struct menulist *);
	int		 (*select)(struct menulist *);
	char		*(*get_text)(struct menulist *);

	int		 sel_line;
	GR_WINDOW_ID 	 wid;
	GR_GC_ID 	 gc;
	GR_SCREEN_INFO 	 screen_info;
	GR_SIZE 	 gr_width, gr_height, gr_base;

	struct menulist	*prevml;
};


static struct menulist *currentml;
static struct artist	*selartist = NULL;


static void draw_itunes_parse(int cnt)
{
	char str[10];
	sprintf(str, "%i", cnt);
	
	GrSetGCUseBackground(currentml->gc, GR_TRUE);
	GrSetGCMode(currentml->gc, GR_MODE_SET);
	GrSetGCForeground(currentml->gc, BLACK);
	GrFillRect(currentml->wid, currentml->gc, 0,
		   3 * currentml->gr_height,
		   currentml->screen_info.cols, currentml->gr_height);
	GrSetGCForeground(currentml->gc, WHITE);
	GrText(currentml->wid, currentml->gc, 8, 3 * currentml->gr_height - 3,
			str, -1, GR_TFASCII);
}


static void itunes_draw(struct menulist *ml);
static int itunes_do_draw();
static int itunes_do_keystroke();

struct menulist *new_ml()
{
	struct menulist *ret = 
		(struct menulist *) malloc(sizeof(struct menulist));
	GrGetScreenInfo(&ret->screen_info);

	ret->gc = GrNewGC();
	GrSetGCUseBackground(ret->gc, GR_TRUE);
	GrSetGCForeground(ret->gc, WHITE);

	ret->wid = pz_new_window(0, HEADER_TOPLINE + 1, ret->screen_info.cols,
			ret->screen_info.rows - (HEADER_TOPLINE + 1),
			itunes_do_draw, itunes_do_keystroke);

	GrSelectEvents(ret->wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrGetGCTextSize(ret->gc, "M", -1, GR_TFASCII, &ret->gr_width,
			&ret->gr_height, &ret->gr_base);

	ret->gr_height += 5;

	GrMapWindow(ret->wid);

	ret->prevml = NULL;
	ret->sel_line = 0;
	return ret;
}



static int get_prev(struct menulist *ml)
{
	struct btree_head *p = btree_prev((struct btree_head *) ml->user);
	if (p) {
		ml->user = (void *) p;
		return 1;
	}
	return 0;
}


static int get_next(struct menulist *ml)
{
	struct btree_head *n = btree_next((struct btree_head *) ml->user);
	if (n) {
		ml->user = (void *) n;
		return 1;
	}
	return 0;
}


static int get_prev_tracklist_artselect(struct menulist *ml)
{
	struct tracklist *t = (struct tracklist *) ml->user;
	
	do {
		t = (struct tracklist *)
			btree_prev((struct btree_head *) t);
	} while (t && (t->track->artist != selartist));
	
	if (t && (t->track->artist == selartist)) {
		ml->user = (void *) t;
		return 1;
	}
	return 0;
}


static int get_next_tracklist_artselect(struct menulist *ml)
{
	struct tracklist *t = (struct tracklist *) ml->user;
	
	do {
		t = (struct tracklist *)
			btree_next((struct btree_head *) t);
	} while (t && (t->track->artist != selartist));

	if (t && (t->track->artist == selartist)) {
		ml->user = (void *) t;
		return 1;
	}
	return 0;
}


static int get_prev_array(struct menulist *ml)
{
	void *p = ml->user - sizeof(void *);
	if (*((void **) p)) {
		ml->user = p;
		return 1;
	}
	return 0;
}
	
static int get_next_array(struct menulist *ml)
{
	void *n = ml->user + sizeof(void *);
	if (*((void **) n)) {
		ml->user = n;
		return 1;
	}
	return 0;
}
	
	
static char *get_text_track(struct menulist *ml)
{
	return ((struct track *) ml->user)->name;
}


static char *get_text_tracklist(struct menulist *ml)
{
	return ((struct tracklist *) ml->user)->track->name;
}


static char *get_text_album(struct menulist *ml)
{
	return ((struct album *) ml->user)->name;
}


static char *get_text_albumlist(struct menulist *ml)
{
	return ((struct albumlist *) ml->user)->album->name;
}


static char *get_text_artist(struct menulist *ml)
{
	return ((struct artist *) ml->user)->name;
}


static char *empty_string = "";

static char *get_text_plist(struct menulist *ml)
{
	if (ml->user)
		return ((struct plist *) ml->user)->name;
	return empty_string;
}


static void play_song(struct itdb_track *track)
{
	new_mp3_window(track->path, track->album, track->artist, track->title, track->length);
}

	
static char *get_text_plisttrack(struct menulist *ml)
{
	return ((*(atracks *) ml->user)[0])->name;
}


static int select_track(struct menulist *ml)
{
	struct itdb_track *track = db_read_details((struct track *)ml->user);
	if (!track) {
		fprintf(stderr, "can not get track details. \n");
	}
	play_song(track);
	
	return 0;
}


static int select_tracklist(struct menulist *ml)
{
	struct itdb_track *track = db_read_details(
			((struct tracklist *) ml->user)->track);
	if (!track) {
		fprintf(stderr, "can not get track details. \n");
	}

	play_song(track);

	return 0;
}


static int select_album(struct menulist *ml)
{
	struct album *album = (struct album *) ml->user;
	currentml = new_ml();
	currentml->prevml = ml;
	currentml->get_next = get_next;
	currentml->get_prev = get_prev;
	currentml->get_text = get_text_tracklist;
	currentml->select = select_tracklist;

	currentml->user = (void *) btree_first((struct btree_head *)
			&album->tracks);

	itunes_draw(currentml);
	
	return 0;
}


static int select_albumlist(struct menulist *ml)
{
	struct artist *artist;
	struct albumlist *al= (struct albumlist *) ml->user;
	
	currentml = new_ml();
	currentml->prevml = ml;
	currentml->get_next = get_next_tracklist_artselect;
	currentml->get_prev = get_prev_tracklist_artselect;
	currentml->get_text = get_text_tracklist;
	currentml->select = select_tracklist;

	currentml->user = (void *) btree_first((struct btree_head *)
			&al->album->tracks);
	
	/* make sure a track of the selected artist is selected */
	artist =  ((struct tracklist *) currentml->user)->track->artist;
	if (artist != selartist) get_next_tracklist_artselect(currentml);

	itunes_draw(currentml);
	
	return 0;
}


static int select_artist(struct menulist *ml)
{
	struct artist *artist = (struct artist *) ml->user;
	selartist = artist;
	currentml = new_ml();
	currentml->prevml = ml;
	currentml->get_next = get_next;
	currentml->get_prev = get_prev;
	currentml->get_text = get_text_albumlist;
	currentml->select = select_albumlist;

	currentml->user = (void *) btree_first((struct btree_head *)
			&artist->albums);

	itunes_draw(currentml);

	return 0;
}


static int select_plisttrack(struct menulist *ml)
{
	atracks *at = ml->user;
	struct track *t = (*at)[0];

	
	struct itdb_track *track = db_read_details(t);
	if (!track) {
		fprintf(stderr, "can not get track details. \n");
	}
	play_song(track);
	
	return 0;
}


static int select_plist(struct menulist *ml)
{
	struct plist *plist = ml->user;

	currentml = new_ml();
	currentml->prevml = ml;
	currentml->get_next = get_next_array;
	currentml->get_prev = get_prev_array;
	currentml->get_text = get_text_plisttrack;
	currentml->select = select_plisttrack;

	currentml->user = plist->tracks;

	itunes_draw(currentml);
	
	return 0;
}


static void drawline_hlight(struct menulist *ml, int line) 
{
	GrSetGCForeground(ml->gc, WHITE);
	GrFillRect(ml->wid, ml->gc, 0, 1 + line * ml->gr_height,
			ml->screen_info.cols, ml->gr_height);
	GrSetGCForeground(ml->gc, BLACK);
	GrSetGCUseBackground(ml->gc, GR_FALSE);
	GrText(ml->wid, ml->gc, 8, (line + 1) * ml->gr_height - 3,
			ml->get_text(ml), -1, GR_TFASCII);
	GrSetGCUseBackground(ml->gc, GR_TRUE);
}

static void drawline_normal(struct menulist *ml, int line) 
{
	GrSetGCForeground(ml->gc, BLACK);
	GrFillRect(ml->wid, ml->gc, 0, 1 + line * ml->gr_height,
			ml->screen_info.cols, ml->gr_height);
	GrSetGCForeground(ml->gc, WHITE);
	GrText(ml->wid, ml->gc, 8, (line + 1) * ml->gr_height - 3,
			ml->get_text(ml), -1, GR_TFASCII);
}


static void itunes_draw(struct menulist *ml)
{
	int i;
	int offset = 0, offset2 = 0;

	if (ml->sel_line > 4) {
		ml->sel_line = 4;
	} else {
		if (ml->sel_line < 0) ml->sel_line = 0;
	}

	for (i = 0; i < ml->sel_line; i++) {
		if (ml->get_prev(ml)) offset++;
	}

	for (i = 0; i < 5; i++) {
		if (i == offset) {
			drawline_hlight(ml, i);
		} else {
			drawline_normal(ml, i);
		}
		
		if (ml->get_next(ml)) {
			offset2++;
		} else { 
			break;
		}
	}
	
	for (i = 0; i < offset2 - ml->sel_line; i++) {
		ml->get_prev(ml);
	}
}


static int itunes_do_draw()
{
	itunes_draw(currentml);

	return 0;
}

static int itunes_do_keystroke(GR_EVENT * event)
{
	int ret = 0;
	struct menulist *oldml;

	switch (event->keystroke.ch) {
	case '\r':		/* action key */
	case '\n':
		currentml->select(currentml);
		break;

	case 'm':		/* menu key */
		ret = 1;
		pz_close_window(currentml->wid);
		oldml = currentml;
		currentml = currentml->prevml;
		free(oldml);
		if (currentml) itunes_draw(currentml);
		break;

	case 'l':
		if (currentml->get_prev(currentml)) currentml->sel_line--;
		itunes_draw(currentml);
		ret = 1;
		break;
	case 'r':
		if (currentml->get_next(currentml)) currentml->sel_line++;
		itunes_draw(currentml);
		ret = 1;
		break;
	}

	return ret;
}


void new_itunes_track()
{
	currentml = new_ml();
	currentml->get_next = get_next;
	currentml->get_prev = get_prev;
	currentml->get_text = get_text_track;
	currentml->select = select_track;

	if ((db_init((void *) draw_itunes_parse) < 0) || !tracks) {
		pz_close_window(currentml->wid);
		free(currentml);
		return;
	}
	
	printf("tracks->parent: %p\n ", ((struct btree_head *) tracks)->parent);
	currentml->user = (void *) btree_first((struct btree_head *) tracks);
	printf("currentml->prev: %p\n", ((struct btree_head *) currentml->user)->prev);
	printf("currentml name: %s\n", ((struct track *) currentml->user)->name);
	get_prev(currentml);
	printf("currentml->prev: %p\n", ((struct btree_head *) currentml->user)->prev);
	printf("currentml name: %s\n", ((struct track *) currentml->user)->name);

	itunes_draw(currentml);
}


void new_itunes_artist()
{
	currentml = new_ml();
	currentml->get_next = get_next;
	currentml->get_prev = get_prev;
	currentml->get_text = get_text_artist;
	currentml->select = select_artist;

	if ((db_init((void *) draw_itunes_parse) < 0) || !artists) {
		pz_close_window(currentml->wid);
		free(currentml);
		return;
	}
	currentml->user = (void *) btree_first((struct btree_head *) artists);

	itunes_draw(currentml);
}


void new_itunes_album()
{
	currentml = new_ml();
	currentml->get_next = get_next;
	currentml->get_prev = get_prev;
	currentml->get_text = get_text_album;
	currentml->select = select_album;

	if ((db_init((void *) draw_itunes_parse) < 0) || !albums) {
		pz_close_window(currentml->wid);
		free(currentml);
		return;
	}

	currentml->user = (void *) btree_first((struct btree_head *) albums);

	itunes_draw(currentml);
}


void new_itunes_plist()
{
	currentml = new_ml();
	currentml->get_next = get_next;
	currentml->get_prev = get_prev;
	currentml->get_text = get_text_plist;
	currentml->select = select_plist;

	if ((db_init((void *) draw_itunes_parse) < 0) || !albums) {
		pz_close_window(currentml->wid);
		free(currentml);
		return;
	}

	if (plists) {
	currentml->user = (void *) btree_first((struct btree_head *) plists);
	}
	else {
	currentml->user = 0;
	}

	itunes_draw(currentml);
}
