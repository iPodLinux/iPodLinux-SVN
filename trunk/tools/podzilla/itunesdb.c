/*
 * Copyright (C) 2004 Courtney Cavin, Bernard Leach
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
#include <pthread.h>
#include <assert.h>

#include "pz.h"
#include "ipod.h"

#define MAX_ITUNES_ITEMS 5
#define MAXLINELENGTH 256

static GR_WINDOW_ID itunes_wid;
static GR_GC_ID itunes_gc;
static GR_SCREEN_INFO screen_info;

typedef void (*itunes_action_t) (void);

struct itunes_item {
	char *text;
	int type;
	int length;
	void *ptr;
};

static struct itunes_item main_itunes[2048];
static struct itunes_item itunes_artist[2048][128];
static int songperartist[2048];

static int current_itunes_item = 0;
static int top_itunes_item = 0;
static int in_contrast = 0;

static struct itunes_item *itunes = main_itunes;
static struct itunes_item *itunes_stack[5];
static int itunes_item_stack[5];
static int top_itunes_item_stack[5];
static int itunes_stack_pos = 0;
static char *buffer;
static int already_opened = 0;

static void draw_itunes()
{
	int i;
	GR_SIZE width, height, base;
	struct itunes_item *m = &itunes[top_itunes_item];

	GrGetGCTextSize(itunes_gc, "M", -1, GR_TFASCII, &width, &height, &base);
	height += 5;

	i = 0;
	while (i <= 5) {
		if (i + top_itunes_item == current_itunes_item) {
			GrSetGCForeground(itunes_gc, WHITE);
			GrFillRect(itunes_wid, itunes_gc, 0,
				   1 + i * height,
				   screen_info.cols, height);
			GrSetGCForeground(itunes_gc, BLACK);
			GrSetGCUseBackground(itunes_gc, GR_FALSE);
		} else {
			GrSetGCUseBackground(itunes_gc, GR_TRUE);
			GrSetGCMode(itunes_gc, GR_MODE_SET);
			GrSetGCForeground(itunes_gc, BLACK);
			GrFillRect(itunes_wid, itunes_gc, 0,
				   1 + i * height,
				   screen_info.cols, height);
			GrSetGCForeground(itunes_gc, WHITE);
		}

		if (m->text != 0) {
			GrText(itunes_wid, itunes_gc, 8, 1 + (i + 1) * height - 4, m->text, -1, GR_TFASCII);
			m++;
		}

		i++;

		if (i == MAX_ITUNES_ITEMS)
			break;
	}
	GrSetGCMode(itunes_gc, GR_MODE_SET);
}

static void itunes_do_draw()
{
	if(!already_opened)
		pz_draw_header("Loading...");
	else
		pz_draw_header("Artists");
	draw_itunes();
}

static char *find_mount_point(void)
{
	static char mountpoint[MAXLINELENGTH];

	char input[MAXLINELENGTH];
	char device[MAXLINELENGTH];
	char filesystem[MAXLINELENGTH];
	char itunesloc[MAXLINELENGTH];
	char command[2*MAXLINELENGTH];
	int mounted=0;

	FILE *file;

	if ((file = fopen("/proc/mounts", "r")) == NULL){
		printf("Unable to open mounts!\n");
		return;
	}

	printf("Checking mounts for /dev/hda2\n");

	while(fgets(input, MAXLINELENGTH-1, file)) {
		sscanf(input, "%s%s%s", &device, &mountpoint, &filesystem);
		if(strcmp(device, "/dev/hda2")==0) {
			if(strcmp(filesystem, "vfat")!=0) {
				printf("/dev/hda2 mounted as %s, remounting as vfat\n", filesystem);
				sprintf(command, "umount %s", mountpoint);
				system(command);
				sprintf(command, "mount -t vfat %s %s", device, mountpoint);
				system(command);
			}
			printf("Mounted on %s\n", mountpoint);
			mounted=1;
			break;
		}
	}
	fclose(file);
	if(!mounted) {
		printf("/dev/hda2 not found in mounts.. Mounting..\n");
		system("mkdir /mnt/music");
		system("mount -t vfat /dev/hda2 /mnt/music");
		sprintf(mountpoint, "/mnt/music");
	}

	return mountpoint;
}

void *mountitunes() {
	char itunesloc[MAXLINELENGTH];
	char mountpoint[MAXLINELENGTH];

	char msg[MAXLINELENGTH];
	long fs;

	FILE *itunesdb;

	mountpoint[0] = 0;

	strcat(itunesloc, "/iPod_Control/iTunes/iTunesDB");
	if((itunesdb = fopen(itunesloc, "rb"))==NULL) {
		strcpy(itunesloc, "iTunesDB");
		if((itunesdb = fopen(itunesloc, "rb"))==NULL) {
			strcpy(mountpoint, find_mount_point());

			strcpy(itunesloc, mountpoint);
			strcat(itunesloc, "/iPod_Control/iTunes/iTunesDB");
			itunesdb = fopen(itunesloc, "rb");
		}
	}

	if(itunesdb ==NULL) {
		new_message_window("Error: can't open file.\n");
		return;
	}

	printf("File found at: %s. Parsing...\n", itunesloc);
	fseek(itunesdb, 0, SEEK_END);
	fs = ftell(itunesdb);
	printf("File size: %dK\n", fs/1024);
	rewind(itunesdb);

	buffer = malloc(fs * sizeof(char));
	if(buffer==NULL) {
		sprintf(msg, "malloc: %d\n", fs);
		new_message_window(msg);
		return;
	}
	if(fread(buffer, 1, fs, itunesdb)!=fs) {
		sprintf(msg, "read: %d\n", fs);
		new_message_window(msg);
		return;
	}
	fclose(itunesdb);

	parse(mountpoint);

	printf("Thread finished parsing...\n");
}

static unsigned int get_int(char *p)
{
	if ((int)p&0x3==0) {
		return *(unsigned int *)p;
	}
	else {
		return (*(unsigned short *)p) | *(unsigned short *)(p + 2) << 16;
	}
}

int parse(char *mountpoint) {
	char filepos2[80], buf, *curitem;
	char filepos[128], itemtext[128], artisttext[128], songtext[128];
	unsigned int isitem=0, oldartist=0, whichartist, songpos, whichitem;
	int songlength;
	int i, j, pos=-1, lasti;

	curitem = buffer;
	buf = *curitem;

	printf("Begin!\n");
	while(1) {
		if(((int)curitem)%2==0 && buf=='m') {
			unsigned long id;

			id = get_int(curitem);

#define MHIT_ID 0x7469686d /* "mhit" */
#define MHOD_ID 0x646f686d /* "mhod" */
#define MHLP_ID 0x706c686d /* "mhlp" Playlists header */

#define MHDB_ID 0x6462686d /* "mhdb" */
#define MHSD_ID 0x6473686d /* "mhsd" */
#define MHLT_ID 0x746c686d /* "mhlt" */

			if(id == MHIT_ID) {
				if(pos>=0) {
					for(i=pos; i--;) {
						if(strcmp(main_itunes[i].text, artisttext)==0) {
							oldartist=1;
							whichartist=i;
							break;
						}
					}
				}
				if(!oldartist) {
					main_itunes[pos].text = malloc(strlen(artisttext)+1);
					strcpy(main_itunes[pos].text, artisttext);
					main_itunes[pos].type = 0;
					main_itunes[pos].ptr = itunes_artist[pos];
					whichartist=pos;
					songperartist[pos]=0;
					if(pos==11)
						draw_itunes();
					pos++;
				}
				oldartist=0;

				itunes_artist[whichartist][songperartist[whichartist]].text = malloc(strlen(songtext)+1);
				strcpy(itunes_artist[whichartist][songperartist[whichartist]].text, songtext);
				
				itunes_artist[whichartist][songperartist[whichartist]].type = 1;
				itunes_artist[whichartist][songperartist[whichartist]].length = songlength/1000;
				itunes_artist[whichartist][songperartist[whichartist]].ptr = malloc(strlen(filepos)+1);
				strcpy(itunes_artist[whichartist][songperartist[whichartist]].ptr, filepos);
				songperartist[whichartist]++;
				whichitem=0;
#if 0
				printf("Filesz : %ld\n", get_int(curitem+34));
				printf("Len    : %ld\n", get_int(curitem+40));
				printf("Track #: %ld\n", get_int(curitem+44));
				printf("Total  : %ld\n", get_int(curitem+48));
				printf("Year   : %ld\n", get_int(curitem+52));
				printf("Bitrate: %ld\n", get_int(curitem+56));
				printf("Sample : %ld\n", get_int(curitem+60));
				printf("Voladj : %ld\n", get_int(curitem+64));
#endif
				songlength = get_int(curitem+40);
				curitem += get_int(curitem+4);
			}
			else if(id == MHOD_ID) {
				whichitem = get_int(curitem+12);
#define ITEM_SONG	1
#define ITEM_FILE	2
#define ITEM_ARTIST	4
				if(whichitem==ITEM_ARTIST || whichitem==ITEM_SONG || whichitem==ITEM_FILE)
				{
					int c = 0;
					char *p;

					int itemlen = get_int(curitem+28);
					p = curitem + 40;

					if (itemlen > sizeof(itemtext) - 1) {
						itemlen = sizeof(itemtext) - 1;
					}

					for (c = 0; c < itemlen/2; c++) {
						itemtext[c] = *p;
						p+=2;
					}
					itemtext[c] = 0;

					if(whichitem==ITEM_SONG) {
						strncpy(songtext, itemtext, sizeof(songtext)-1);
						songtext[sizeof(songtext)-1] = 0;
					}
					if(whichitem==ITEM_ARTIST) {
						strncpy(artisttext, itemtext, sizeof(artisttext)-1);
						artisttext[sizeof(artisttext)-1] = 0;
					}
					if(whichitem==ITEM_FILE) {
						strncpy(filepos2, itemtext, sizeof(filepos2)-1);
						for(j=0; filepos2[j]!='\0'; j++) {
							if(filepos2[j]==':')
								filepos2[j]='/';
						}
						snprintf(filepos, sizeof(filepos)-1, "%s%s", mountpoint, filepos2);
						filepos[sizeof(filepos)-1] = 0;
					}
				}

				curitem += get_int(curitem+4);
			}
			else if(id == MHLP_ID) {
				if(pos>=0) {
					for(i=pos; i--;) {
						if(strcmp(main_itunes[i].text, artisttext)==0) {
							oldartist=1;
							whichartist=i;
							break;
						}
					}
				}
				if(!oldartist) {
					main_itunes[pos].text = malloc(strlen(artisttext)+1);
					strcpy(main_itunes[pos].text, artisttext);
					main_itunes[pos].type = 0;
					main_itunes[pos].ptr = itunes_artist[pos];
					whichartist=pos;
					songperartist[pos]=0;
					pos++;
				}

				itunes_artist[whichartist][songperartist[whichartist]].text = malloc(strlen(songtext)+1);
				strcpy(itunes_artist[whichartist][songperartist[whichartist]].text, songtext);

				itunes_artist[whichartist][songperartist[whichartist]].type = 1;
				itunes_artist[whichartist][songperartist[whichartist]].length = songlength/1000;
				itunes_artist[whichartist][songperartist[whichartist]].ptr = malloc(strlen(filepos)+1);
				strcpy(itunes_artist[whichartist][songperartist[whichartist]].ptr, filepos);
				songperartist[whichartist]++;

				/* mark the "end of the list" */
				main_itunes[pos].text = 0;
				main_itunes[pos].type = 0;
				main_itunes[pos].ptr = 0;
				break; /* skip play list section */
			}
			else if(id == MHDB_ID) {
				curitem += get_int(curitem+4);
			}
			else if(id == MHSD_ID) {
				curitem += get_int(curitem+4);
			}
			else if(id == MHLT_ID) {
				curitem += get_int(curitem+4);
			}
			else {
// printf("%04x (%04x)\n", id, lasti);
				curitem++;
			}

			lasti = id;
		}
		else {
				curitem++;
		}

		buf = *curitem;
	}

	free(buffer);
	already_opened=1;
	pz_draw_header("Artists");
}

static int itunes_do_keystroke(GR_EVENT * event)
{
	int ret = 0;

	switch (event->keystroke.ch) {
	case '\r':		/* action key */
	case '\n':
		ret = 1;
		switch (itunes[current_itunes_item].type) {
		case 0:
			if (itunes[current_itunes_item].ptr != 0) {
				itunes_stack[itunes_stack_pos] = itunes;
				itunes_item_stack[itunes_stack_pos] = current_itunes_item;
				top_itunes_item_stack[itunes_stack_pos++] = top_itunes_item;

				pz_draw_header(itunes[current_itunes_item].text);
				itunes = (struct itunes_item *)itunes[current_itunes_item].ptr;
				current_itunes_item = 0;
				top_itunes_item = 0;
				draw_itunes();
			}
			break;
		case 1:
			if (itunes[current_itunes_item].ptr != 0) {
				//printf("File: %s\n", itunes[current_itunes_item].ptr);
				new_mp3_window(itunes[current_itunes_item].ptr, main_itunes[itunes_item_stack[(itunes_stack_pos)-1]].text, itunes[current_itunes_item].text);
			}
			break;
		}
		break;

	case 'm':		/* menu key */
		ret = 1;
		if (itunes_stack_pos > 0) {
			itunes = itunes_stack[--itunes_stack_pos];
			current_itunes_item = itunes_item_stack[itunes_stack_pos];
			top_itunes_item = top_itunes_item_stack[itunes_stack_pos];

			if(itunes_stack_pos==0)
				pz_draw_header("Artists");
			else
				pz_draw_header(itunes[current_itunes_item].text);
			draw_itunes();
			ret = 1;
		}
		else
			pz_close_window(itunes_wid);
		break;

	case 'l':
		if (current_itunes_item) {
			if (current_itunes_item == top_itunes_item) {
				top_itunes_item--;
			}
			current_itunes_item--;
			draw_itunes();
			ret = 1;
		}
		break;

	case 'r':
		if (itunes[current_itunes_item + 1].text != 0) {
			current_itunes_item++;
			if (current_itunes_item - MAX_ITUNES_ITEMS == top_itunes_item) {
				top_itunes_item++;
			}
			draw_itunes();
			ret = 1;
		}
		break;
	}

	return ret;
}

void new_itunes_window()
{
	pthread_attr_t pattr;
	pthread_t parser;
	int status;

	pthread_attr_init(&pattr);
	//pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);

	if(!already_opened)
		pthread_create(&parser, &pattr, mountitunes, NULL);

	GrGetScreenInfo(&screen_info);

	itunes_gc = GrNewGC();
	GrSetGCUseBackground(itunes_gc, GR_TRUE);
	GrSetGCForeground(itunes_gc, WHITE);

	itunes_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), itunes_do_draw, itunes_do_keystroke);

	GrSelectEvents(itunes_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(itunes_wid);
}
