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
#include <string.h>
#include <itunesdb.h>

#include "itunes_db.h"
#include "btree.h"

#define POOLCHUNK 1024 * 128

static int 			 init = 0;
static unsigned int 		 track_cnt = 0;

static struct itdb_parsecont	*pc;

struct track			*tracks = NULL;
atracks				*tracks_id;
struct artist 			*artists = NULL;
struct album  			*albums = NULL;
struct plist  			*plists = NULL;

static struct albumlist  	 fakealbumlist;
static struct artist  	 	 fakeartist;
static struct album  	 	 fakealbum;

static char		 	 track_nullstr[] = "Unnamed Track\0";
static char		 	 album_nullstr[] = "Unnamed Album\0";
static char		 	 artist_nullstr[] = "Unnamed Artist\0";

static char			*poolptr = NULL;
static int			 poolsize = 0;


typedef void (*itunes_action_t) (void);


/*
 * Memory pool handling.
 *
 * So far no freeing. The db stays in mem until the program is finished anyways.
 * If it becomes necessary the allocated mem can be kept track of using a simple
 * linear list.
 */


static void *pmalloc(int size)
{
	int inc = ((int) poolptr) & 3;
	char *oldpp;

	if (inc) {
		inc -= 4;
		poolptr -= inc;
		poolsize += inc;
	}
	if (size > poolsize) {
		poolptr = malloc(POOLCHUNK);
		if (poolptr) {
			poolsize = POOLCHUNK;
		} else {
			return NULL;
		}
	}
	oldpp = poolptr;

	poolptr += size;
	poolsize -= size;

	return oldpp;
}


static void *pmallocc(int size)
{
	char *oldpp;
	if (size > poolsize) poolptr = malloc(POOLCHUNK);
	oldpp = poolptr;

	poolptr += size;
	poolsize -= size;

	return oldpp;
}


static char *pstrdup(char *str)
{
	int size = strlen(str) + 1;
	char *ret = pmallocc(size);

	if (!ret) return ret;

	memcpy(ret, str, size);

	return ret;
}


/*
 * Sorting.
 *
 * NOTE: The init_*_sortkey functions have to be change in accordance to the
 * *_cmp functions.
 *
 */

static int artist_cmp(struct btree_head *a, struct btree_head *b)
{
	int r;

	r = strcmp(((struct artist *) a)->name, ((struct artist *) b)->name);
	return r;
}

inline uint32_t sortkey_alpha(char *s)
{
	return (s[0] << 24) + (s[1] << 16) + (s[2] << 8) + s[3];
}

inline void init_artist_sortkey(struct artist *a)
{
	((struct btree_head *) a)->sortkey = sortkey_alpha(a->name);
}


static int album_cmp(struct btree_head *a, struct btree_head *b)
{
	int r;

	r = strcmp(((struct album *) a)->name, ((struct album *) b)->name);
	return r;
}


inline void init_album_sortkey(struct album *a)
{
	((struct btree_head *) a)->sortkey = sortkey_alpha(a->name);
}


/* FIXME: it would probably be good to rewrite (instead of calling) the compare
 * func here */
static int albumlist_cmp(struct btree_head *a, struct btree_head *b)
{
	return album_cmp((struct btree_head *) ((struct albumlist *) a)->album,
			(struct btree_head *) ((struct albumlist *) b)->album);
}


inline void init_albumlist_sortkey(struct albumlist *al)
{
	((struct btree_head *) al)->sortkey =
		((struct btree_head *) al->album)->sortkey;
}


static int track_cmp(struct btree_head *a, struct btree_head *b)
{
	int r;

	struct track *at = (struct track *) a;
	struct track *bt = (struct track *) b;

	r = strcmp(at->name, bt->name);
	if (r) return r;

	r = at->db_offset - bt->db_offset;
	return r;
}


inline void init_track_sortkey(struct track *t)
{
	((struct btree_head *) t)->sortkey = sortkey_alpha(t->name);
}


static int tracklist_cmp(struct btree_head *a, struct btree_head *b)
{
	int ret = ((struct tracklist *) a)->index -
			((struct tracklist *) b)->index;

	if (ret) return ret;

	return track_cmp((struct btree_head *) ((struct tracklist *) a)->track,
			(struct btree_head *) ((struct tracklist *) b)->track);
}


inline void init_tracklist_sortkey(struct tracklist *t)
{
	((struct btree_head *) t)->sortkey = t->index;
}


inline struct track *bsearch_track(uint32_t ipodid)
{
	unsigned int b1, b2, m;
	int c;

	b1 = 0;
	b2 = track_cnt - 1;

	m = b1 + (b2 - b1) / 2;

	while ((c = (*tracks_id)[m]->ipodid - ipodid) && (b1 != b2)) {
		if (c > 0) {
			b2 = m - 1;
		} else {
			b1 = m + 1;
		}
		m = (b1 + b2) / 2;
	}

	if (!c) return (*tracks_id)[m];

	return NULL;
}


static int plist_cmp(struct btree_head *a, struct btree_head *b)
{
	int r;

	struct plist *ap = (struct plist *) a;
	struct plist *bp = (struct plist *) b;

	r = strcmp(ap->name, bp->name);
	if (r) return r;

	r = ap->db_offset - bp->db_offset;
	return r;
}


inline void init_plist_sortkey(struct plist *p)
{
	((struct btree_head *) p)->sortkey = sortkey_alpha(p->name);
}


/*
 * Database operations
 */

static struct artist *artist_add(char *aname)
{
	struct artist *artist, *artists_local;

	if (!artists) {
		artist = (struct artist *) pmalloc(sizeof(struct artist));
		artist->albums.album = NULL;
		artist->name = pstrdup(aname);
		init_artist_sortkey(artist);

		btree_init((struct btree_head *) artist);
		artists = artist;
		return artist;
	}

	artists_local = artists;
	fakeartist.name = aname;
	init_artist_sortkey(&fakeartist);
	if ((artist = (struct artist *) btree_find((struct btree_head **)
					&artists_local,
					(struct btree_head *) &fakeartist,
					artist_cmp))) {
		return artist;
	} else {
		artist = (struct artist *) pmalloc(sizeof(struct artist));
		artist->albums.album = NULL;
		artist->name = pstrdup(aname);
		((struct btree_head *) artist)->sortkey =
			((struct btree_head *) &fakeartist)->sortkey;
		btree_add((struct btree_head *) artists_local,
				(struct btree_head *) artist, artist_cmp);
		return artist;
	}
}


static struct album *album_add(char *aname)
{
	struct album *album, *albums_local;

	if (!albums) {
		album = (struct album *) pmalloc(sizeof(struct album));
		album->tracks.track = NULL;
		album->name = pstrdup(aname);
		init_album_sortkey(album);

		btree_init((struct btree_head *) album);
		albums = album;
		return album;
	}

	albums_local = albums;
	fakealbum.name = aname;
	init_album_sortkey(&fakealbum);
	if ((album = (struct album *) btree_find((struct btree_head **)
					&albums_local,
					(struct btree_head *) &fakealbum,
					album_cmp))) {
		return album;
	} else {
		album = (struct album *) pmalloc(sizeof(struct album));
		album->tracks.track = NULL;
		album->name = pstrdup(aname);
		((struct btree_head *) album)->sortkey =
			((struct btree_head *) &fakealbum)->sortkey;
		btree_add((struct btree_head *) albums_local,
				(struct btree_head *) album, album_cmp);
		return album;
	}
}


static void track_add(struct track *t)
{
	if (!tracks) {
		btree_init((struct btree_head *) t);
		tracks = t;
		return;
	}
	btree_add((struct btree_head *) tracks, (struct btree_head *) t,
			track_cmp);
}


static void tracklist_add(struct tracklist *tl, struct track *t,
		unsigned short index)
{
	struct tracklist *entry;

	if (!tl->track) {
		btree_init((struct btree_head *) tl);
		tl->track = t;
		tl->index = index;
		init_tracklist_sortkey(tl);
		return;
	}
	entry = (struct tracklist *) pmalloc(sizeof(struct
				tracklist));
	entry->track = t;
	entry->index = index;
	init_tracklist_sortkey(entry);
	btree_add((struct btree_head *) (tl), (struct btree_head *) entry,
			tracklist_cmp);
}


static void albumlist_add(struct albumlist *al, struct album *a)
{
	struct albumlist *entry, *al_local;

	if (!al->album) {
		btree_init((struct btree_head *) al);
		al->album = a;
		init_albumlist_sortkey(al);
		return;
	}

	al_local = al;
	fakealbumlist.album = a;
	init_albumlist_sortkey(&fakealbumlist);
	if (btree_find((struct btree_head **) &al_local,
				(struct btree_head *) &fakealbumlist,
				albumlist_cmp))
		return;

	entry = (struct albumlist *) pmalloc(sizeof(struct
				albumlist));
	entry->album = a;
	((struct btree_head *) entry)->sortkey =
		((struct btree_head *) &fakealbumlist)->sortkey;
	btree_add((struct btree_head *) (al), (struct btree_head *) entry,
			albumlist_cmp);
}


static int notracks_cb(unsigned int num, void *user)
{
	tracks_id = pmalloc(num * 4);
	return 0;
}


static int track_cb(struct itdb_track *t, void *user)
{
	struct artist *artist;
	static struct artist *lastartist = NULL;
	struct album *album;
	static struct album *lastalbum = NULL;
	struct track *track;

	//void (*cb)(int) = user;

	if (!t->title) 	t->title = track_nullstr;
	if (!t->artist) t->artist = artist_nullstr;
	if (!t->album) 	t->album = album_nullstr;

	track = (struct track *) pmalloc(sizeof(struct track));
	track->name = pstrdup(t->title);
	track->db_offset = t->db_offset;
	track->ipodid = t->ipodid;
	init_track_sortkey(track);

	if (lastalbum && (!strcmp(lastalbum->name, t->album))) {
		album = lastalbum;
	} else {
		album = album_add(t->album);
		lastalbum = album;
	}

	if (lastartist && (!strcmp(lastartist->name, t->artist))) {
		artist = lastartist;
	} else {
		artist = artist_add(t->artist);
		lastartist = artist;
	}
	track->artist = artist;

	tracklist_add(&album->tracks, track, t->trackno);
	albumlist_add(&artist->albums, album);
	track_add(track);
	(*tracks_id)[track_cnt++] = track;

//	if (!(track_cnt % 20)) cb(track_cnt);

	return 0;
}


static void plist_add(struct plist *p)
{
	if (!plists) {
		btree_init((struct btree_head *) p);
		plists = p;
		return;
	}

	btree_add((struct btree_head *) plists, (struct btree_head *) p,
			plist_cmp);
}


static int playlist_cb(struct itdb_plist *p, void *user)
{
	struct plist *plist;
	int i;

	plist = pmalloc(sizeof(struct plist));
	if (!plist) return -1;
	plist->tracks = pmalloc((p->notracks + 2) * sizeof(void *));
	if (!plist) return -1;

	plist->name = pstrdup(p->title);
	plist->db_offset = p->db_offset;


	(*plist->tracks)[0] = NULL;
	plist->tracks = (void *) (&(*plist->tracks)[1]);
	init_plist_sortkey(plist);

	for (i = 0; i < p->notracks; i++) {
		(*plist->tracks)[i] = bsearch_track(p->ipodids[i]);
		if (!(*plist->tracks)[i]) printf("could not find: %i\n",
				p->ipodids[i]);
	}
	(*plist->tracks)[i] = NULL;

	plist_add(plist);

	return 0;
}


int db_init(void *user)
{
	struct timeval tv1, tv2, tvdiff;
	float diff;
#ifdef DEBUG
	struct track *t;
	struct tracklist *tl;
	struct artist *ar;
	struct album *al;
	struct albumlist *all;
#endif

	if (init) return 0;

	gettimeofday(&tv1, NULL);

	pc = itdb_new_parsecont();
	itdb_add_notracks_cb(pc, notracks_cb, user);
	itdb_add_track_cb(pc, track_cb, user);
	itdb_add_plist_cb(pc, playlist_cb, user);

	itdb_sel_parseentries(pc, ITDB_PARSE_TITLE | ITDB_PARSE_ARTIST |
			ITDB_PARSE_ALBUM | ITDB_PARSE_PLAYLIST_NORMAL);

	/* 512 kB cache */
	itdb_set_cachesize(pc, 1024 * 512);
#ifdef IPOD
	if (itdb_parse_file(pc, "/iPod_Control/iTunes/iTunesDB")) {
#else
	if (itdb_parse_file(pc, "iTunesDB")) {
#endif
		printf("Reading of db failed.\n");
		itdb_delete_parsecont(pc);
		return -1;
	}

	gettimeofday(&tv2, NULL);
	timersub(&tv2, &tv1, &tvdiff);
	diff = (float) tvdiff.tv_sec + (float) tvdiff.tv_usec / 1000.0 / 1000.0;
	printf("%i tracks parsed in %f seconds.\n", track_cnt, diff);

	itdb_sel_parseentries(pc, ITDB_PARSE_ALL);


#ifdef DEBUG
	t = btree_first(tracks);
	while (t) {
		printf("%s \n", t->name);
		t = btree_next(t);
	}


	al = btree_first(albums);
	while (al) {
		printf("%s \n", al->name);
		al = btree_next(al);
	}

	ar = btree_first(artists);
	while (ar) {
		printf("%s \n", ar->name);

		all = btree_first(&ar->albums);
		while (all) {
			printf("     %s \n", all->album->name);

				tl = btree_first(&all->album->tracks);
				while (tl) {
					if (tl->track->artist == ar) {
						printf("          %s \n",
							tl->track->name);
					}
					tl = btree_next(tl);
				}

			all = btree_next(all);
		}

		ar = btree_next(ar);
	}
#endif
	init = 1;
	return 0;
}


struct itdb_track *db_read_details(struct track *t)
{
	return itdb_read_track(pc, t->db_offset);
}

