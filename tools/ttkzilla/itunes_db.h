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

#ifndef __ITUNES_LDB_H
#define __ITUNES_LDB_H

#include "btree.h"

struct track {
	struct btree_head	 btree;
	char 			*name;
	struct artist		*artist;
	unsigned int 		 db_offset;
	uint32_t		 ipodid;
};

struct tracklist {
	struct btree_head	 btree;
	struct track		*track;
	unsigned short		 index;
};

struct album {
	struct btree_head	 btree;
	char 			*name;
	struct tracklist	 tracks;
	unsigned int 		 db_offset;
};

struct albumlist {
	struct btree_head	 btree;
	struct album		*album;
};

struct artist {
	struct btree_head	 btree;
	char 			*name;
	struct albumlist	 albums;
	unsigned int 		 db_offset;
};

typedef struct track * ptrack;
typedef ptrack atracks[];

struct plist {
	struct btree_head	 btree;
	char			*name;
	unsigned int 		 db_offset;
	atracks			*tracks;
};

extern struct track *tracks;
extern struct artist *artists;
extern struct album *albums;
extern struct plist *plists;

int    db_init(void *user);
struct itdb_track *db_read_details(struct track *t);

#endif
