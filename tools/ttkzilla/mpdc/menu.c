/*
 * Copyright (C) 2005 Adam Johnston <aegray@ipodlinux.org>,
 * 			Courtney Cavin <courtc@ipodlinux.org>
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
#include "../pz.h"
#include "../mlist.h"
#include "mpdc.h"
#define MM_LEVEL_PLAYLIST 0
#define MM_LEVEL_GENRE	  1  /* will be changed when genre support is added */
#define MM_LEVEL_ARTIST	  2
#define MM_LEVEL_ALBUM	  3
#define MM_LEVEL_SONG	  4
#define MM_MAX_LEVEL 5
#define MM_LEVEL_BUFFER_SIZE	6
#define MM_LEVEL_QUEUE	5
#define MPD_TABLE_GENRE  25  /* temporary hack workaround while genres not supported */

#ifdef DEBUG
#define Dprintf printf
#else
#define Dprintf(...)
#endif

extern void mms_modify_song(menu_st *);

int mm_opening = 1;

static GR_WINDOW_ID mm_wid;
static GR_GC_ID mm_gc;
static GR_TIMER_ID queue_key_tid;
static menu_st *mmz;

static char * mm_level_search[MM_LEVEL_BUFFER_SIZE];
static int mm_level_searchTable[MM_LEVEL_BUFFER_SIZE];
static int mm_curlevel;
static int mm_startlevel;
static int mm_nbEntries;
static char * mm_curartist=NULL;
static char * mm_curplaylist=NULL;
static char * mm_curalbum=NULL;

static void mm_add_item(char * item);

static int mm_sort_list[MM_LEVEL_BUFFER_SIZE] = { 1,
					   1,
					   1,
					   1,
					   1,
					   0 };

static char * mm_titles[MM_LEVEL_BUFFER_SIZE] = { "Playlists",
					  "Genres",
					  "Artists",
					  "Albums",
					  "Songs",
					  "Now Playing"};

typedef struct {
	char * full_name;
	struct MMDisplayItem * next;
} MMDisplayItem;

MMDisplayItem * mm_root = NULL;

static int mm_fill_playlists(char * search);
#if 0
static int mm_fill_genres(char * search);
#endif
static int mm_fill_artists(char * search, int searchTable);
static int mm_fill_albums(char * search, int searchTable);
static int mm_fill_songs(char * search, int searchTable);
static int mm_fill_queue(char * search);
static void mm_up_level();
void mm_playlists();
void mm_genres();
void mm_artists();
void mm_albums();
void mm_songs();
void mm_queue();
void new_mm_window(int, char *, int);
static void mm_expose();
void mm_queue_songs(int, char *);

static void mm_show_now_playing()
{
	mpd_currently_playing();
}

/* aweful for fragmentation */
static MMDisplayItem * mm_ll_destroy(MMDisplayItem * rootn)
{
	MMDisplayItem * curnode;
	MMDisplayItem * tmpnode;
	Dprintf("Destroying...\n");
	for (curnode = rootn; (curnode != NULL);)
	{
		tmpnode = curnode;
		curnode = (MMDisplayItem*)curnode->next;
		/* Don't have do do this - menu_destroy does it for us */
		if (tmpnode->full_name!=NULL)
			free(tmpnode->full_name);
		free(tmpnode);
	}
	return NULL;
}

static void mm_destroy_menu()
{
	mm_root = mm_ll_destroy(mm_root);
}

static void mm_load_playlist(char *path)
{
	if (mpdc_tickle() < 0)
		return;
	if (path == NULL)
		return;
	mpd_sendClearCommand(mpdz);
	mpd_finishCommand(mpdz);

	mpd_sendLoadCommand(mpdz, path);
	mpd_finishCommand(mpdz);
	if (mpdz->error) {
		mpdc_tickle();
		return;
	}
	mpd_sendPlayCommand(mpdz, -1);
	mpd_finishCommand(mpdz);
	mm_show_now_playing();
}

static int mm_queue_music(char * string)
{
	switch (mm_curlevel) {
	case MM_LEVEL_PLAYLIST:
		break;
	case MM_LEVEL_ARTIST:
		mm_queue_songs(MPD_TABLE_ARTIST, string);
		return 1;
	case MM_LEVEL_ALBUM:
		mm_queue_songs(MPD_TABLE_ALBUM, string);
		return 1;
	case MM_LEVEL_SONG:
		mm_queue_songs(MPD_TABLE_TITLE, string);
		return 1;
	case MM_LEVEL_QUEUE:
		mms_modify_song(mmz);
		break;
	}
	return 0;
}
typedef struct _Tsortitem {
	char *text;
	int track;
} Tsortitem;

#if 0
static int track_cmp(const void *x, const void *y)
{
	int a = ((Tsortitem *)x)->track;
	int b = ((Tsortitem *)x)->track;

	return (a > b) ? 1 : (a < b) ? -1 : 0;
}
#else

static inline void swap(Tsortitem a[], int i, int j)
{
	Tsortitem t = a[i];
	a[i] = a[j];
	a[j] = t;
}

static void quisort(Tsortitem a[], int l, int u)
{
	int i, m;
	Tsortitem p;

	if (l < u) {
		swap(a, l, (u + l) / 2);
		p = a[l];
		m = l;
		for (i = l + 1; i <= u; i++)
			if (a[i].track < p.track)
				swap(a, ++m, i);
		swap(a, l, m);
		quisort(a, l, m - 1);
		quisort(a, m + 1, u);
	}
}
#endif

static void mm_sort_track(char *text, char *track)
{
	static Tsortitem *head = NULL;
	static int n_items = 0, a_items = 0;
	int i;
	if (text == NULL) {
#if 0
		qsort(head, n_items, sizeof(Tsortitem), track_cmp);
#else
		quisort(head, 0, n_items - 1);
#endif
		for (i = 0; i < n_items; i++) {
			mm_add_item(head[i].text);
			free(head[i].text);
		}
		free(head);
		head = NULL;
		a_items = n_items = 0;
		return;
	}
	if (a_items <= n_items) {
		head = realloc(head, sizeof(Tsortitem) * (a_items + 10));
		a_items += 10;
	}
	head[n_items].text = strdup(text);
	head[n_items].track = track ? atoi(track) : 0;
	n_items++;
}

/* assumes mmz = an inited menu */
static void mm_fill_menu(int arrows)
{
	MMDisplayItem *curnode;
	for (curnode = mm_root; (curnode!=NULL);)
	{
		menu_add_item(mmz, curnode->full_name, NULL, 0, arrows ?
				ARROW_MENU : STUB_MENU);
		curnode = (MMDisplayItem*)curnode->next;
	}
}

static void mm_scan_level(int level, char *searchstr, int searchIndex)
{
	int searchTable = -1;
	int ret;

	if (mm_root != NULL)
		mm_destroy_menu();
	if (mmz!=NULL) {
		menu_destroy(mmz);
		mmz = NULL;
	}
	mm_nbEntries = 0;
	mm_curlevel = level;
	mmz = menu_init(mm_wid, mm_titles[level], 0, 0, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1), NULL, NULL,
			UTF8);
	if (searchstr!=NULL) {
		if (mm_curlevel-1==MM_LEVEL_ARTIST) {
			if ((searchIndex == 0) && (!strcmp(searchstr, "All"))) {
			/* check for all just for coob - so he can have no all
			 * entry if theres only one album hehe */
				mm_curartist = NULL;
				searchTable = MPD_TABLE_GENRE;
			} else {
				mm_curartist = malloc(strlen(searchstr)+1);
				memcpy(mm_curartist, searchstr, strlen(searchstr));
				searchTable = MPD_TABLE_ARTIST; 	
				mm_curartist[strlen(searchstr)]='\0';
			}		

		} else if (mm_curlevel-1==MM_LEVEL_ALBUM) {

			if ((searchIndex == 0) && (!strcmp(searchstr, "All"))) {
				mm_curalbum = NULL;
				searchTable = MPD_TABLE_ARTIST;	
			} else {
				mm_curalbum = malloc(strlen(searchstr)+1);
				memcpy(mm_curalbum, searchstr, strlen(searchstr));
				mm_curalbum[strlen(searchstr)]='\0';
				searchTable = MPD_TABLE_ALBUM;
			}	
		} else if (mm_curlevel-1 == MM_LEVEL_PLAYLIST) {
			mm_curplaylist = malloc(strlen(searchstr)+1);
			memcpy(mm_curplaylist, searchstr, strlen(searchstr));
			mm_curplaylist[strlen(searchstr)]='\0';
		}
	}
	mm_level_searchTable[level] = searchIndex;
	switch (level) {
		case MM_LEVEL_PLAYLIST:
			if (mm_fill_playlists(searchstr) <= 0) {
				mm_up_level();
				pz_error("No Playlists");
				return;
			}
			mm_fill_menu(0);
			break;
#if 0
		case MM_LEVEL_GENRE:
			mm_fill_genres(searchstr);
			break;
#endif
		case MM_LEVEL_ARTIST:
			if (mm_fill_artists(searchstr, searchTable) <= 0) {
				mm_up_level();
				pz_error("No Artists");
				return;
			}
			mm_fill_menu(1);
			break;
		case MM_LEVEL_ALBUM:
			if (searchTable == MPD_TABLE_GENRE)
				ret = mm_fill_albums(NULL, MPD_TABLE_ARTIST); 
			else	
				ret = mm_fill_albums(searchstr, searchTable);
			if (ret <= 0) {
				mm_up_level();
				pz_error("No Albums");
				return;
			}
			mm_fill_menu(1);
			break;
		case MM_LEVEL_SONG:
			if (searchTable==MPD_TABLE_ARTIST)
				ret = mm_fill_songs(mm_curartist, searchTable);
			else	
				ret =mm_fill_songs(searchstr, searchTable);
			mm_sort_list[MM_LEVEL_SONG] =
				(mm_startlevel == MM_LEVEL_SONG);
			if (ret <= 0) {
				mm_up_level();
				pz_error("No Songs");
				return;
			}
			mm_fill_menu(0);
			break;
		case MM_LEVEL_QUEUE:
			if (mm_fill_queue(searchstr) <= 0) {
				mm_up_level();
				pz_error("No Queued items");
				return;
			}
			mm_fill_menu(0);
			break;
		default:
			break;
	}
#if 0	/* FIXME: THIS NEEDS TO BE CHECKED OUT!!
	 * - not sure if not freeing here will cause a memory leak
	 * If you uncomment this, sometimes you get a double free though. */	
	if (searchstr != NULL)
		free(searchstr);
#endif
	if (mm_sort_list[mm_curlevel]) {
		if ((mm_curlevel==MM_LEVEL_ARTIST) ||
		   (mm_curlevel==MM_LEVEL_ALBUM)) {	
			quicksort(mmz->items, 1, mmz->num_items-1);	
		} else {
			quicksort(mmz->items, 0, mmz->num_items-1);	
		}	
	}	
	mm_expose();
}
static void mm_next_level(char * searchstr, int searchIndex)
{
	if (mm_curlevel+1 >= MM_MAX_LEVEL) {
		if (mpdc_tickle() < 0)
			return;

		if (mm_startlevel == MM_LEVEL_QUEUE) {
			mpd_sendPlayCommand(mpdz, mmz->sel);
			mpd_finishCommand(mpdz);
		} else {
			int i;
			mpd_sendClearCommand(mpdz);
			mpd_finishCommand(mpdz);

			for (i = 0; i < mmz->num_items; i++)
				mm_queue_music(mmz->items[i].text);
			mpd_sendPlayCommand(mpdz, mmz->sel);
			mpd_finishCommand(mpdz);
		}
		if (mpdz->error) {
			mpdc_tickle();
			return;
		}
		mm_show_now_playing();
	} else if (mm_startlevel == MM_LEVEL_PLAYLIST) {
		mm_load_playlist(searchstr);
	} else {
		new_mm_window(mm_curlevel+1, searchstr, searchIndex);
	}
}

static void mm_up_level()
{
	if (mm_level_search[mm_curlevel]!=NULL) {
		free(mm_level_search[mm_curlevel]);
		mm_level_search[mm_curlevel] = NULL;
	}

	mm_curlevel--;
	switch (mm_curlevel) {
	case MM_LEVEL_PLAYLIST:
		if (mm_curplaylist!=NULL) {
			free(mm_curplaylist);
			mm_curplaylist = NULL;
		}
		break;
	case MM_LEVEL_ALBUM:
		if (mm_curalbum!=NULL) {
			free(mm_curalbum);
			mm_curalbum = NULL;
		}
		break;
	case MM_LEVEL_ARTIST:
		if (mm_curartist!=NULL) {
			free(mm_curartist);
			mm_curartist=NULL;
		}
		break;
	}
	if (mm_curlevel < mm_startlevel) {
		/* handle exit */
		if ((mmz = menu_destroy(mmz)) == NULL) {
			mm_destroy_menu();
			pz_close_window(mm_wid);
			mm_wid = 0;
			mm_opening = 1;

			if (!queue_key_tid)
				GrDestroyTimer(queue_key_tid);
			GrDestroyGC(mm_gc);
		}

	} else {
		/* handle nav up */
		new_mm_window(mm_curlevel, mm_level_search[mm_curlevel], mm_level_searchTable[mm_curlevel]);
	}
}

MMDisplayItem * mm_ll_add_item(MMDisplayItem * rootn, char * item)
{
	MMDisplayItem * tmpnode;
	MMDisplayItem * travnode;

	if (item==NULL)
		return rootn;
	tmpnode = malloc(sizeof(MMDisplayItem));
	if (tmpnode==NULL)
		return rootn;

	tmpnode->full_name = malloc(strlen(item)+1); // Plus 1 for null ?

	if (tmpnode->full_name==NULL)
		return rootn;
	tmpnode->next = NULL;
	memcpy(tmpnode->full_name, item, strlen(item));
	tmpnode->full_name[strlen(item)] = '\0';

	if (rootn == NULL)
		rootn = tmpnode;
	else
	{
		travnode = rootn;
		while (travnode->next != NULL)
			travnode = (MMDisplayItem*)travnode->next;
		travnode->next = (struct MMDisplayItem *)tmpnode;
	}
	mm_nbEntries++;
	return rootn;
}

static void mm_add_item(char * item)
{
	mm_root = mm_ll_add_item(mm_root, item);
}

static void mm_expose()
{
	/* window is focused */
	if(mm_wid == GrGetFocus()) {
		pz_draw_header(mmz->title);
		menu_draw(mmz);
	}
}

static int mm_events(GR_EVENT *e)
{
	int ret = 0;

	switch (e->type) {
	case GR_EVENT_TYPE_TIMER:
		if (((GR_EVENT_TIMER *)e)->tid == queue_key_tid) {
			GrDestroyTimer(queue_key_tid);
			queue_key_tid = 0;
			mm_opening = 1;
			if (mm_queue_music(mmz->items[mmz->sel].text))
				menu_flash_selected(mmz);
		}
		else
			menu_draw_timer(mmz);
		break;
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (e->keystroke.ch) {
		case '\r':
		case '\n':
			mmz = menu_handle_item(mmz, mmz->sel);
			mm_expose();
			ret |= KEY_CLICK;

			if (!queue_key_tid)
				queue_key_tid = GrCreateTimer(mm_wid, 500);
			break;
		case 'l':
			if (menu_shift_selected(mmz, -1)) {
				menu_draw(mmz);
				ret |= KEY_CLICK;
			}
			break;
		case 'r':
			if (menu_shift_selected(mmz, 1)) {
				menu_draw(mmz);
				ret |= KEY_CLICK;
			}
			break;
		case 'q':
		case 'm':
			ret |= KEY_CLICK;
			mm_up_level();
			break;
		default:
			ret |= KEY_UNUSED;
			break;
		}
		break;

	case GR_EVENT_TYPE_KEY_UP:
		switch (e->keystroke.ch) {
		case '\r':
		case '\n':
			if (mm_opening)
				mm_opening = 0;
			else {
				if (queue_key_tid) {
					GrDestroyTimer(queue_key_tid);
					queue_key_tid = 0;
				}
				mm_next_level(mmz->items[mmz->sel].text, mmz->sel);
			}
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

void new_mm_window(int level, char * search, int searchIndex)
{
	char * searchstr = NULL;

	if (search!=NULL)
	{
		searchstr = malloc(strlen(search)+1); // +1 for null?
		memcpy(searchstr, search, strlen(search));
		searchstr[strlen(search)] = '\0';
	}
	mm_level_search[level] = searchstr;
	mm_gc = pz_get_gc(1);
	mm_curlevel = level;
	Dprintf("getting mmz\n");
	if (mmz!=NULL)
	{
		menu_destroy(mmz);
		mmz = NULL;
	}
	Dprintf("Getting mm_root\n");
	if (mm_root!=NULL)
		mm_destroy_menu();

	if (mm_wid==0)
		mm_wid = pz_new_window(0, HEADER_TOPLINE + 2, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 2), mm_expose,
			mm_events);

	GrSelectEvents(mm_wid, GR_EVENT_MASK_EXPOSURE| GR_EVENT_MASK_KEY_UP|
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);

	mm_scan_level(mm_curlevel, searchstr, searchIndex);
	GrMapWindow(mm_wid);

}

static int mm_fill_playlists(char * search)
{
	mpd_InfoEntity entity;
	char is_playlists = 0;

	if (mpdc_tickle() < 0)
		return -1;
	mpd_sendLsInfoCommand(mpdz, "");
	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		mpd_PlaylistFile *playlist = entity.info.playlistFile;
		if (entity.type != MPD_INFO_ENTITY_TYPE_PLAYLISTFILE) {
			continue;
		}
		Dprintf("Adding: %s\n", playlist->path);
		mm_add_item(playlist->path);
		is_playlists = 1;
	}

	mpd_finishCommand(mpdz);
	if (mpdz->error) {
		mpdc_tickle();
		return -1;
	}
	return is_playlists;
}

#if 0
static int mm_fill_genres(char * search)
{
	return -1;
}
#endif

static int mm_fill_artists(char * search, int searchTable)
{
	char * artist;
	char is_artists = 0;

	if (mpdc_tickle() < 0)
		return -1;
	mpd_sendListCommand(mpdz, MPD_TABLE_ARTIST, search);
	if (mpdz->error) {
		mpdc_tickle();
		return -1;
	}

	mm_add_item("All");	
	while ((artist = mpd_getNextArtist(mpdz))) {
		mm_add_item(artist);
		free(artist);
		is_artists = 1;
	}

	mpd_finishCommand(mpdz);
	if (mpdz->error) {
		mpdc_tickle();
		return -1;
	}
	return is_artists;
}

static int mm_fill_songs(char * search, int searchTable)
{
	mpd_InfoEntity entity;
	char by_album = 0;
	char is_songs = 0;

	if(mpdc_tickle() < 0)
		return -1;

	if (search==NULL)
		mpd_sendListallInfoCommand(mpdz, "");
	else {
		mpd_sendFindCommand(mpdz, searchTable, search);
		by_album = 1;
	}

	if (mpdz->error) {
		mpdc_tickle();
		return -1;
	}

	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		int found = 1;
		mpd_Song *song = entity.info.song;
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG) {
			continue;
		}

		if (mm_curartist!=NULL)
			found &= (strcmp(mm_curartist, song->artist)==0);
		if (mm_curalbum!=NULL)
			found &= (strcmp(mm_curalbum, song->album)==0);
		if (found) {
			char *text = song->title ? song->title : song->file;
			if (by_album)
				mm_sort_track(text, song->track);
			else
				mm_add_item(text);
			is_songs = 1;
		}

	}
	mm_sort_track(NULL, NULL);
	mpd_finishCommand(mpdz);
	if(mpdz->error) {
		mpdc_tickle();
		return -1;
	}
	return is_songs;
}

static int mm_playlist_files()
{
	int n_files = 0;
	mpd_InfoEntity entity;

	mpd_sendPlaylistInfoCommand(mpdz, -1);
	if (mpdz->error) {
		mpdc_tickle();
		return -1;
	}

	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG)
			n_files++;
	}
	mpd_finishCommand(mpdz);
	return n_files;
}

void mm_queue_songs(int searchtable, char * search)
{
	MMDisplayItem * root = NULL;
	MMDisplayItem * tmpnode = NULL;
	mpd_InfoEntity entity;
	char is_one = 0, plist_empty;
	if (mpdc_tickle() < 0)
		return;

	if (search==NULL)
		mpd_sendListallInfoCommand(mpdz, "");
	else if (searchtable==MPD_TABLE_TITLE)
		mpd_sendSearchCommand(mpdz, searchtable, search);
	else
		mpd_sendFindCommand(mpdz, searchtable, search);
gotos_suck:
	if (is_one)
		mpd_sendSearchCommand(mpdz, MPD_TABLE_FILENAME, search);

	if (mpdz->error) {
		mpdc_tickle();
		return;
	}

	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		int found = 1;
		mpd_Song *song = entity.info.song;
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG) {
			continue;
		}
		is_one = 1;

		Dprintf("Song: %s \n", song->file);

		if (mm_curartist)
			found &= (song->artist &&
				strcmp(mm_curartist, song->artist)==0);
		if (mm_curalbum)
			found &= (song->album &&
				strcmp(mm_curalbum, song->album)==0);
		if (searchtable==MPD_TABLE_TITLE)
			found &= ((song->title ?
					(strcmp(search, song->title)==0) : 0) ||
					strcmp(search, song->file)==0);
		if (found)
			root = mm_ll_add_item(root, song->file);

	}
	mpd_finishCommand(mpdz);
	if (!is_one) {
		is_one = 1;
		goto gotos_suck;
	}
	plist_empty = (mm_playlist_files() == 0);
	for (tmpnode=root; tmpnode!=NULL;) {
		mpd_sendAddCommand(mpdz, tmpnode->full_name);
		mpd_finishCommand(mpdz);
		tmpnode = (MMDisplayItem*)tmpnode->next;
	}
	mm_ll_destroy(root);
	if(mpdz->error) {
		mpdc_tickle();
		return;
	}
	if (plist_empty) {
		int state;
		if ((state = mpdc_status(mpdz)) < 0)
			return;
		if (state != MPD_STATUS_STATE_PAUSE) {
			mpd_sendPlayCommand(mpdz, -1);
			mpd_finishCommand(mpdz);
		}
	}
}

static int mm_fill_albums(char * search, int searchTable)
{
	char * album;
	char is_albums = 0;

	if (mpdc_tickle < 0)
		return -1;
	mpd_sendListCommand(mpdz, MPD_TABLE_ALBUM, search);
	if (mpdz->error) {
		mpdc_tickle();
		return -1;
	}
	mm_add_item("All");
	while ((album = mpd_getNextAlbum(mpdz))) {
		mm_add_item(album);
		is_albums = 1;
		free(album);
	}

	mpd_finishCommand(mpdz);
	if (mpdz->error) {
		/* it errors out "connection closed" here sometimes, why
		 * doesn't it do it before it starts listing artists? and why
		 * doesn't mpdc_tickle() detect a closed connection? */
		mpdc_tickle();
		return -1;
	}
	return is_albums;
}

static int mm_fill_queue(char * search)
{
	mpd_InfoEntity entity;
	char is_queuing = 0;

	if(mpdc_tickle() < 0)
		return -1;
	mpd_sendPlaylistInfoCommand(mpdz, -1);
	if (mpdz->error) {
		mpdc_tickle();
		return -1;
	}

	while ((mpd_getNextInfoEntity_st(&entity, mpdz))) {
		mpd_Song *song = entity.info.song;
		if (entity.type != MPD_INFO_ENTITY_TYPE_SONG) {
			continue;
		}
		mm_add_item(song->title ? strdup(song->title) :
				strdup(song->file));
		is_queuing = 1;
	}
	mpd_finishCommand(mpdz);
	if(mpdz->error) {
		mpdc_tickle();
		return -1;
	}
	return is_queuing;
}
void mm_playlists()
{
	if (mpdc_tickle() < 0)
		return;
	mm_startlevel = MM_LEVEL_PLAYLIST;
	new_mm_window(mm_startlevel, NULL, -1);
}
void mm_artists()
{
	if (mpdc_tickle() < 0)
		return;
	mm_startlevel = MM_LEVEL_ARTIST;
	new_mm_window(mm_startlevel, NULL, -1);
}
void mm_genres()
{
	if (mpdc_tickle() < 0)
		return;
	mm_startlevel = MM_LEVEL_GENRE;
	new_mm_window(mm_startlevel, NULL, -1);
}
void mm_albums()
{
	if (mpdc_tickle() < 0)
		return;
	mm_startlevel = MM_LEVEL_ALBUM;
	new_mm_window(mm_startlevel, NULL, -1);
}
void mm_songs()
{
	if (mpdc_tickle() < 0)
		return;
	mm_startlevel = MM_LEVEL_SONG;
	new_mm_window(mm_startlevel, NULL, -1);
}
void mm_queue()
{
	if (mpdc_tickle() < 0)
		return;
	mm_startlevel = MM_LEVEL_QUEUE;
	new_mm_window(mm_startlevel, NULL, -1);
}
item_st mpdc_menu[] = {
	{"Playlists", mm_playlists, ACTION_MENU | ARROW_MENU},
	{"Artists", mm_artists, ACTION_MENU | ARROW_MENU},
	{"Albums", mm_albums, ACTION_MENU | ARROW_MENU},
	{"Songs", mm_songs, ACTION_MENU | ARROW_MENU},
	{"Queue", mm_queue, ACTION_MENU | ARROW_MENU},
	{0}
};
