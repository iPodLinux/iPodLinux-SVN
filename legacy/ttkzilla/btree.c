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
#include "btree.h"

void btree_init(struct btree_head *head) 
{
	head->prev = head->next = head->parent = NULL;
}


struct btree_head *btree_find(struct btree_head **root, struct btree_head
		*search, btree_cmp cmp) 
{
	int c;

	while (*root && ((c = search->sortkey - (*root)->sortkey) 
				|| (c = cmp(search, *root)))) {
		if (c > 0) {
			if (!(*root)->next) return NULL;
			*root = (*root)->next;
		} else {
			if (!(*root)->prev) return NULL;
			*root = (*root)->prev;
		}
	}

	return *root;
}


int btree_add(struct btree_head *root, struct btree_head *new, btree_cmp cmp)
{
	struct btree_head *parent = root;
	int c;

	if (btree_find(&parent, new, cmp)) {
		fprintf(stderr, "Entry already exists.\n");
		return -1;
	}

	new->next = new->prev = NULL;
	new->parent = parent;

	c = new->sortkey - parent->sortkey;
	if (!c) c = cmp(new, parent);
	
	if (c > 0) {
		parent->next = new;
	} else {
		parent->prev = new;
	}

	return 0;	
}


struct btree_head *btree_next(struct btree_head *current) 
{
	struct btree_head *i;
	
	if (current == NULL) return NULL;

	if (current->next) {
		i = current->next;
		while (i->prev) i = i->prev;
		return i;
	}
	
	i = current->parent;

	while (i) {
		if (i->prev == current) return i;
		current = i;
		i = i->parent;
	}
	return NULL;
}


struct btree_head *btree_prev(struct btree_head *current) 
{
	struct btree_head *i;
	
	if (current->prev) {
		i = current->prev;
		while (i->next) i = i->next;
		return i;
	}
	
	i = current->parent;

	while (i) {
		if (i->next == current) return i;
		current = i;
		i = i->parent;
	}
	return NULL;
}


struct btree_head *btree_first(struct btree_head *current) 
{
	while (current->prev) current = current->prev;
	return current;
}


struct btree_head *btree_last(struct btree_head *current) 
{
	while (current->next) current = current->next;
	return current;
}
