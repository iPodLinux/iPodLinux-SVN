/*
 * Copyright (c) 2005 Courtney Cavin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <string.h>
#include "mpdc.h"

typedef struct _Element {
	void *data;
	struct _Element *next;
} Element;

mpd_Song current_song;

void clear_current_song()
{
	current_song.artist = NULL;
	current_song.album = NULL;
}

static Element *listsort(Element *list, int (*cmp)(void *, void *))
{
	Element *p, *q, *e, *tail;
	int insize, nmerges, psize, qsize, i;

	if (!list)
		return NULL;

	insize = 1;

	while (1) {
		p = list;
		list = NULL;
		tail = NULL;

		nmerges = 0;

		while (p) {
			nmerges++;
			q = p;
			psize = 0;
			for (i = 0; i < insize; i++) {
				psize++;
				q = q->next;
				if (!q) break;
			}

			qsize = insize;

			while (psize > 0 || (qsize > 0 && q)) {
				if (psize == 0) {
					e = q; q = q->next; qsize--;
				} else if (qsize == 0 || !q) {
					e = p; p = p->next; psize--;
				} else if (cmp(p->data,q->data) <= 0) {
					e = p; p = p->next; psize--;
				} else {
					e = q; q = q->next; qsize--;
				}

				if (tail) {
					tail->next = e;
				} else {
					list = e;
				}
				tail = e;
			}
			p = q;
		}
		tail->next = NULL;

		if (nmerges <= 1)
			return list;

		insize *= 2;
	}
}

void mpdc_queue_data(int set, void *data, void (*action)(void *, void *),
		int (*cmp)(void *, void *))
{
	static Element *head[2] = {NULL, NULL};
	Element *n, *p;

	if (data && !action) {
		n = (Element *)malloc(sizeof(Element));
		n->data = data;
		n->next = NULL;

		if ((p = head[set]) != NULL) {
			while (p->next != NULL)
				p = p->next;
			p->next = n;
		}
		else
			head[set] = n;
	}
	else if (action) {
		p = head[set];
		while (p) {
			n = p;
			(* action)(data, p->data);
			p = p->next;
			free(n);
		}
		head[set] = NULL;
	}
	else if (cmp) {
		head[set] = listsort(head[set], cmp);
	}
}

