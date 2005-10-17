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

extern int playlistpos, playlistlength;

extern void add_track_to_queue(struct track* song);
extern void clear_play_queue();
extern void start_play_queue();

struct menulist {
	void		*user;
	int		 (*get_prev)(struct menulist *);
	int		 (*get_next)(struct menulist *);
	int		 (*select)(struct menulist *);
	char		*(*get_text)(struct menulist *);
	struct track*	 (*get_track)(struct menulist *);
	GR_WINDOW_ID 	 wid;
	GR_GC_ID 	 gc;
	GR_SCREEN_INFO 	 screen_info;
	GR_SIZE 	 gr_width, gr_height, gr_base;
	TWidget *itunes_menu;
	char init;
	struct menulist	*prevml;
};


static struct menulist *currentml;
static struct artist	*selartist = NULL;

void queue_all_tracks(struct menulist * ml);


static void draw_itunes_parse(int cnt)
{
	char str[10];
	sprintf(str, "%i", cnt);

	GrSetGCUseBackground(currentml->gc, GR_FALSE);
	GrSetGCMode(currentml->gc, GR_MODE_SET);
	GrSetGCForeground(currentml->gc, WHITE);
	GrFillRect(currentml->wid, currentml->gc, 0,
		   3 * currentml->gr_height,
		   currentml->screen_info.cols, currentml->gr_height);
	GrSetGCForeground(currentml->gc, BLACK);
	GrText(currentml->wid, currentml->gc, 8, 3 * currentml->gr_height - 3,
			str, -1, GR_TFASCII);
}


static void itunes_draw(struct menulist *ml);
static void itunes_do_draw();
static int itunes_do_keystroke();

struct menulist *new_ml()
{
	struct menulist *ret =
		(struct menulist *) malloc(sizeof(struct menulist));
		
	GrGetScreenInfo(&ret->screen_info);

	ret->gc = pz_get_gc(1);
	GrSetGCUseBackground(ret->gc, GR_FALSE);
	GrSetGCForeground(ret->gc, BLACK);
	GrSetGCBackground(ret->gc, BLACK);

	ret->wid = pz_new_window(0, HEADER_TOPLINE + 1, ret->screen_info.cols,
			ret->screen_info.rows - (HEADER_TOPLINE + 1),
			itunes_do_draw, itunes_do_keystroke);

	GrSelectEvents(ret->wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_TIMER);

	GrGetGCTextSize(ret->gc, "M", -1, GR_TFASCII, &ret->gr_width,
			&ret->gr_height, &ret->gr_base);

	ret->gr_height += 4;

	GrMapWindow(ret->wid);
	ret->itunes_menu = ttk_new_menu_widget (0, ttk_menufont, ret->wid->w, ret->wid->h);
	ttk_window_set_title (ret->wid, "Music");
	ret->init = 0;
	ret->prevml = NULL;

	return ret;
}

static void itunes_do_draw()
{

	//pz_draw_header(currentml->itunes_menu->title);
	itunes_draw(currentml);
}

static int get_prev(struct menulist *ml)
{
	struct btree_head *p;
	
	if (ml->user)
		p = btree_prev((struct btree_head *) ml->user);
	else
		return 0;

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


static void play_song(struct menulist *ml, struct track *track)
{
	static int queuesong = 0;
	if (queuesong == 0) {
		queuesong = 1;
		queue_all_tracks(ml);
		queuesong = 0;

		/* all the songs have now been added, time to play songs. */
		start_play_queue();

		/* clear the play list info */
		clear_play_queue();
	}
	else {
		add_track_to_queue(track);
	}
}


static char *get_text_plisttrack(struct menulist *ml)
{
	return ((*(atracks *) ml->user)[0])->name;
}


static int select_track(struct menulist *ml)
{
	play_song(ml, (struct track *)ml->user);

	return 0;
}

static struct track * get_track(struct menulist *ml)
{
	return (struct track *)ml->user;
}


static int select_tracklist(struct menulist *ml)
{
	play_song(ml, ((struct tracklist *)ml->user)->track);
	return 0;
}

static struct track* get_tracklist(struct menulist *ml)
{
	return ((struct tracklist *)ml->user)->track;
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
	currentml->get_track = get_tracklist;

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
	currentml->get_track = get_tracklist;

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
	play_song(ml, t);

	return 0;
}
static struct track * get_plisttrack(struct menulist *ml)
{
	atracks *at = ml->user;
	return (*at)[0];
}

static int select_plist(struct menulist *ml)
{
	struct plist *plist = ml->user;

	if (!plist)
		return 0;
	
	currentml = new_ml();
	currentml->prevml = ml;
	currentml->get_next = get_next_array;
	currentml->get_prev = get_prev_array;
	currentml->get_text = get_text_plisttrack;
	currentml->select = select_plisttrack;
	currentml->get_track = get_plisttrack;

	currentml->user = plist->tracks;

	itunes_draw(currentml);

	return 0;
}




static void itunes_draw(struct menulist *ml)
{
	int counter=0;

	if (!currentml->user)
		return;
	
	if (!ml->init)
	{
		//add to courtc's menu from the original list.
		while (currentml->get_prev(currentml))counter--;
		do 
		{
			ttk_menu_item *item = calloc (1, sizeof(ttk_menu_item));
			item->name = currentml->get_text (currentml);
			item->sub = TTK_MENU_DONOTHING;
			ttk_menu_append (currentml->itunes_menu, item);
			counter++;
		}while (currentml->get_next(currentml));
			while (counter != 0)
			{
				if (counter> 0) {
					counter--;
					currentml->get_prev(currentml);
				}
				if (counter < 0) {
					counter++;
					currentml->get_next(currentml);
				}
			}

		ml->init=1;

	}

	ml->itunes_menu->dirty++;
}

/*
 * This function is called to queue up all tracks in the current menu
 * in the play queue.  It does this by going through the menu and calling
 * select on each item which will then call this function again - during
 * the second call though the actual item will be queued.
 *
 * Yes this is a hack, and it will be simplified hopefully!
 */
void queue_all_tracks(struct menulist * ml)
{
	int tracknum = 0;

	if (playlistlength != 0) {
		clear_play_queue();
	}

	playlistpos = 1;

	/*go through list and printf*/
	while (currentml->get_prev(currentml))
	{    
		playlistpos++;
		tracknum--;
	}

	/*is now at the top*/
	/*now go down to bottom and printf*/
	printf("Playlistpos : %d\n", playlistpos);
	while (1)
	{
		currentml->select(currentml);
		if (!currentml->get_next(currentml)) {
			break;
		}
		tracknum++;
	}

	while (tracknum != 0)
	{
		if (tracknum > 0) {
			tracknum--;
			currentml->get_prev(currentml);
		}
		if (tracknum < 0) {
			tracknum++;
			currentml->get_next(currentml);
		}
	}

	itunes_draw(currentml);
}

static int itunes_do_keystroke(GR_EVENT * event)
{
	int ret = 0;
	struct menulist *oldml;

	switch (event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
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
			if (currentml) itunes_draw (currentml);
			break;

		case 'r':
			if(currentml->get_next(currentml))
			{
				ttk_menu_scroll (currentml->itunes_menu, 1);
				itunes_do_draw();
				ret |= KEY_CLICK;
			}
			break;

		case 'l':
			if(currentml->get_prev(currentml))
			{
				ttk_menu_scroll (currentml->itunes_menu, 1);
				itunes_do_draw();
				ret |= KEY_CLICK;
			}
			break;
		}
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
	currentml->get_track = get_track;

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

