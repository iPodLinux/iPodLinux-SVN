/*
 * Copyright (C) 2005 Preshant V
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
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <itunesdb.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <limits.h>
#endif

#include "ipod.h"
#include "itunes_db.h"

struct tracknode {
	struct track *cur;
	struct track origcur;
	struct tracknode *prevnode;
	struct tracknode *nextnode;
};

void play_next_track();
void play_prev_track();

static struct tracknode *get_tracknode(int nodenum);
static void shuffle_songs(int preserve);
static void set_shuffle_mode(int newmode);
static void set_repeat_mode(int newmode);
static void swap_tracknodes(struct tracknode *one, struct tracknode *two);
static int get_rand_numb(int themax);

static int shuffle = 0, repeat = 0;
static struct tracknode *songhead = NULL;
static struct tracknode *cursong = NULL;

/*
 * global so other parts know the position, mainly [aac|mp3].c
 */
int playlistpos = 0, playlistlength = 0;

extern void new_mp3_window(char *filename, char *album, char *artist, char *title, int len);
extern void new_aac_window(char *filename, char *album, char *artist, char *title, int len);

extern int is_mp3_type(char *extension);
extern int is_aac_type(char *extension);

extern void new_message_window(char *message);

static void
play_track(struct tracknode *tracknode)
{
	struct itdb_track *track = NULL;
	char mpath[PATH_MAX];
	char *fileext;

	if (tracknode == NULL) {
		printf("play_track got NULL!\n");
		return;
	}

	track = db_read_details(tracknode->cur);
	if (!track) {
		printf("error with track!");
		return;
	}

	snprintf(mpath, PATH_MAX-1, "/%s", track->path);

	/* only decode mp3 files */
	fileext = strrchr(mpath, '.');
	if (fileext)
	{
		printf("playing %d of %d (%s)\n", playlistpos, playlistlength, mpath);
		if (is_mp3_type(fileext)) {
#ifdef IPOD
			new_mp3_window(mpath, track->album, track->artist,
				track->title, track->length);
#endif
		} else if (is_aac_type(fileext)) {
#ifdef USE_HELIXAACDEC
			new_aac_window(mpath, track->album, track->artist,
				track->title, track->length);
#endif
		}
	}
	else
	{
		new_message_window("Invalid format");
	}
}

void
start_play_queue()
{
	shuffle = ipod_get_setting(SHUFFLE);
	repeat = ipod_get_setting(REPEAT);
	if (shuffle < 0)
		shuffle = 0;
	if (shuffle > 1)
		shuffle = 1;
	if (repeat < 0)
		repeat = 0;
	if (repeat > 2)
		repeat = 2;

	/*
	 * the playlist has been copied, the length autoset by the
	 * add_track_to_queue and the playlistpos has been set to
	 * current point 
	 */
	cursong = get_tracknode(playlistpos);
	if (shuffle == 0)
	{
		// no moving necessary
	}
	else if (shuffle == 1)
	{
		swap_tracknodes(songhead, cursong);
		cursong = get_tracknode(1);
		playlistpos = 1;
		shuffle_songs(1);
	}

	play_track(cursong);

	/*
	 * shuffle albums not yet supported 
	 */
}

void
checkshufrpt()
{
	if (ipod_get_setting(SHUFFLE) != shuffle) {
		set_shuffle_mode(ipod_get_setting(SHUFFLE));
	}
	if (ipod_get_setting(REPEAT) != repeat) {
		set_repeat_mode(ipod_get_setting(REPEAT));
	}
}

static void
swap_tracknodes(struct tracknode *one, struct tracknode *two)
{
	/* swaps two tracknodes */
	struct track *temp = two->cur;

	two->cur = one->cur;
	one->cur = temp;
}

static int
get_rand_numb(int max)
{
	static int seedinit = 0;
	
	if (!seedinit)
	{
		int fd, data;

		fd = open("/dev/random", O_RDONLY);
		if (fd == -1) {
			data = (int)time(NULL);
		}
		else {
			read(fd, &data, sizeof(data));
			close(fd);
		}
		srand(data);
		seedinit = 1;
	}

	return 1 + (int)((float)max * rand() / (RAND_MAX + 1.0));
}

static struct tracknode *
get_tracknode(int nodenum)
{
	struct tracknode *tempnode;
	int counter = 1;

	tempnode = songhead;
	while (counter < nodenum)
	{
		if (tempnode == NULL) {
			printf("get_tracknode error, couldn't get %d!\n", nodenum);
			return songhead;
		}
		tempnode = tempnode->nextnode;
		counter++;
	}

	return tempnode;
}

static void
shuffle_songs(int preserve)
{
	struct tracknode *cur;

	if (cursong == NULL) {
		return;
	}

	/* does a suffle of songs only, only the head stays at pos */
	if (preserve == 1) {
		cur = cursong;
	} else {
		cur = songhead;
	}

	if (preserve == 1) {
		cur = cur->nextnode;
		if (cur == NULL) {
			return;
		}
	}

	while (cur!=NULL)
	{
		int swapnode = 1;
		while (swapnode == 1)
		{
			swapnode = get_rand_numb(playlistlength);
		}
		swap_tracknodes(cur, get_tracknode(swapnode));
		cur = cur->nextnode;
	}
}

static void
shuffle_albums()
{
	/*
	 * this changes position in playlist as well 
	 */
}

static void
shuffle_off()
{
	char found = 0;
	struct track *curtrack;
	struct tracknode *cur;

	curtrack = cursong->cur;
	cur = songhead;
	playlistpos = 1;
	while (cur != NULL)
	{
		cur->cur = &(cur->origcur);
		if (&(cur->origcur) == curtrack)
		{

			/*
			 * find the new position in this playlist 
			 */
			cursong = cur;
			found = 1;
		}
		if (found == 0) {
			playlistpos++;
		}
		cur = cur->nextnode;
	}
}

void
add_track_to_queue(struct track *song)
{
	struct tracknode *newsong;
	struct tracknode *nextnode;
	struct track *asong;

	asong = (struct track *)malloc(sizeof(struct track));
	*asong = *song;

	newsong = (struct tracknode *)malloc(sizeof(struct tracknode));
	nextnode = songhead;
	while (nextnode != NULL && nextnode->nextnode != NULL)
	{
		nextnode = nextnode->nextnode;
	}

	if (nextnode != NULL && nextnode->nextnode == NULL)
	{
		nextnode->nextnode = newsong;
		newsong->prevnode = nextnode;
	}
	else
	{
		songhead = newsong;
		newsong->prevnode = NULL;
	}

	newsong->cur = asong;
	memcpy(&(newsong->origcur),asong,sizeof(struct track));
	newsong->nextnode = NULL;

	playlistlength++;
}

/*
 * delete functionality a little buggy void deletenode(struct tracknode *
 * todel) { struct tracknode * prev; struct tracknode * next;
 * prev=todel->prevnode; next=todel->nextnode;
 * if(prev!=NULL){prev->nextnode=next;}else{songhead=next;}
 * if(next!=NULL){next->prevnode=prev;} cout<<"deleting
 * :"<<todel->cur->title<<endl; delete todel; }
 * 
 * 
 * void deletesong(struct itdb_track * todel) { struct tracknode *
 * cur=songhead; while(cur!=NULL) { if(cur->cur = todel) {
 * deletenode(cur); playlistlength--; return; } cur=cur->nextnode; } } so
 * has been disabled. 
 */
void
clear_play_queue()
{
	struct tracknode *curnode;
	struct tracknode *nextnode;

	curnode = songhead;
	while (curnode != NULL)
	{
		nextnode = curnode->nextnode;
		free(curnode);
		curnode = nextnode;
	}

	songhead = NULL;
	playlistlength = 0;
	playlistpos = 0;
}

void
play_next_track()
{
	checkshufrpt();
	if (cursong == NULL) {
		return;
	}

	if (repeat == 1) {
		play_track(cursong);

		return;
	}

	if (repeat == 0 || cursong->nextnode != NULL) {
		playlistpos++;
		cursong = cursong->nextnode;
		play_track(cursong);

		return;
	}
	else if (cursong->nextnode == NULL && repeat == 2) {
		shuffle_songs(0);
		cursong = songhead;
		playlistpos = 1;
		play_track(cursong);
	}
}

void
play_prev_track()
{
	struct tracknode *lasttrack;

	checkshufrpt();
	if (cursong == NULL) {
		return;
	}

	if (repeat == 1) {
		play_track(cursong);
		return;
	}

	if (cursong->prevnode != NULL || repeat == 0) {
		cursong = cursong->prevnode;
		playlistpos--;
		play_track(cursong);

		return;
	}

	if (cursong->prevnode == NULL && repeat == 2) {
		lasttrack = songhead;
		playlistpos = 1;
		while (lasttrack->nextnode != NULL) {
			lasttrack = lasttrack->nextnode;
			playlistpos++;
		}
		shuffle_songs(0);
		cursong = lasttrack;
		play_track(cursong);
	}
}

static void
set_shuffle_mode(int newmode)
{
	if (newmode == -1) {
		shuffle++;
	}
	else {
		shuffle = newmode;
	}

	if (shuffle > 1) {
		shuffle = 0;
	}

	if (shuffle == 1)
	{
		/*
		 * mode 1- shuffle songs 
		 */
		shuffle_songs(1);
	}
	else if (shuffle == 2)
	{
		shuffle_albums();
	}
	else if (shuffle == 0)
	{
		shuffle_off();
	}
}

static void
set_repeat_mode(int newmode)
{
	if (newmode == -1) {
		repeat++;
	}
	else {
		repeat = newmode;
	}

	repeat++;
	if (repeat > 2) {
		repeat = 0;
	}
}
