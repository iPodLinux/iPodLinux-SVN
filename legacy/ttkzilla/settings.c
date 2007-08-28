/*
 * Copyright (C) 2005 Alastair Stuart
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
#include <errno.h>

#define SETTING_INT 0
#define SETTING_STRING 1

#define SETTINGS_MAGIC "EtRR" /* Expanded the Reading/Righting. Or not */

struct _setting_st {
	short type;
	short id;
	void *value;
	struct _setting_st *next;
};

typedef struct _setting_st setting_st;

static setting_st *settings_root = NULL;

#ifdef DEBUG
static void print_setting(setting_st *current_setting)
{
	switch (current_setting->type) {
	case SETTING_INT:
		printf("==> addr: 0x%p  id: %d  value: %d  next: 0x%p\n",
			   current_setting,
			   current_setting->id,
			   *(int *)(current_setting->value),
			   current_setting->next);
		break;
	case SETTING_STRING:
		printf("==> addr: 0x%p  id: %d  value: \"%s\"  next: 0x%p\n",
			   (void *)current_setting,
			   current_setting->id,
			   (char *)(current_setting->value),
			   (void *)current_setting->next);
		break;
	}
}

void print_all_settings(void)
{
	setting_st *current_setting = settings_root;

	while (current_setting != NULL) {
		printf("all: ");
		print_setting(current_setting);
		current_setting = current_setting->next;
	}
}
#endif /* DEBUG */

static setting_st *find_setting(short id)
{
	setting_st *current_setting = settings_root;
	setting_st *prev_setting;

	if (current_setting == NULL)
		return NULL;

	while (current_setting != NULL) {
		prev_setting = current_setting;
		current_setting = current_setting->next;

		if (prev_setting->id == id) {
			current_setting = NULL;
			break;
		}
	}

	return prev_setting;
}

#if 0
setting_st *find_last_setting(void)
{
	setting_st *current_setting = settings_root;
	setting_st *prev_setting;

	if (current_setting == NULL)
		return NULL;

	while (current_setting != NULL) {
		prev_setting = current_setting;
		current_setting = current_setting->next;
	}

	return prev_setting;
}
#endif

void free_setting(setting_st *setting)
{
	if (setting) {
#ifdef DEBUG
		printf("<<< freeing: ");
		print_setting(setting);
#endif
		if (setting->value)
			free(setting->value);
		free(setting);
		setting = NULL;
	}
}

void free_all_settings(void)
{
	setting_st *next_setting, *current_setting;
	next_setting = current_setting = settings_root;

	while (next_setting != NULL) {
		next_setting = current_setting->next;
		free_setting(current_setting);
		current_setting = next_setting;
	}
	settings_root = NULL;
}

static setting_st *new_setting(short type, short id, void *value)
{
	setting_st *new_setting;

	new_setting = malloc(sizeof(setting_st));
	new_setting->type = type;
	new_setting->id = id;

	switch (type) {
	case SETTING_INT:
		new_setting->value = malloc(sizeof(int));
		memcpy(new_setting->value, value, sizeof(int));
		break;
	case SETTING_STRING:
		new_setting->value = strdup((char *)value);
		break;
	default:
		free(new_setting);
		return NULL;
		break;
	}

	new_setting->next = NULL;

	return new_setting;
}

int get_int_setting(short id)
{
	setting_st *setting = settings_root;
	int ret = 0;

	while (setting != NULL) {
		if (setting->id == id) {
			memcpy(&ret, setting->value, sizeof(int));
			break;
		}
		setting = setting->next;
	}

	return ret;
}

void set_int_setting(short id, int value)
{
	setting_st *last_setting = find_setting(id);
	setting_st *setting;

	if (last_setting == NULL ||
	    (last_setting != NULL && last_setting-> id != id)) {
		/* doesn't already exist, and last node isn't one we want to change */
		setting = new_setting(SETTING_INT, id, (void *)&value);
		if (last_setting == NULL) { /* first node */
			settings_root = setting;
		} else {
			last_setting->next = setting;
		}
	} else if (last_setting->id == id) {
		*(int *)(last_setting->value) = value;
	}
}


char *get_string_setting(short id)
{
	setting_st *setting = settings_root;
	char *ret = NULL;

	while (setting != NULL) {
		if (setting->id == id) {
			ret = strdup((char *)setting->value);
			break;
		}
		setting = setting->next;
	}

	return ret;
}


void set_string_setting(short id, char *value)
{
	setting_st *last_setting = find_setting(id);
	setting_st *setting;

	if (last_setting == NULL ||
	    (last_setting != NULL && last_setting->id != id)) {
		/* doesn't already exist, and last node isn't one we want to change */
		setting = new_setting(SETTING_STRING, id, (void *)value);
		if (last_setting == NULL) { /* first node */
			settings_root = setting;
		} else {
			last_setting->next = setting;
		}
	} else if (last_setting->id == id) {
		free(last_setting->value);
		(last_setting->value) = strdup(value);
	}
}


int load_settings(char *settings_file)
{
	FILE *fp;
	int file_len;
	setting_st *setting = NULL, *prev_setting = NULL;
	char buf[32];

	if ((fp = fopen(settings_file, "r")) == NULL) {
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	rewind(fp);
	fread(buf, strlen(SETTINGS_MAGIC), 1, fp);
	fflush(stdout);
	if (memcmp(buf, SETTINGS_MAGIC, strlen(SETTINGS_MAGIC))) {
		fclose(fp);
		return -2;
	}
	while(ftell(fp) < file_len) {
		int len;
		setting = malloc(sizeof(setting_st));
		if (prev_setting != NULL)
			prev_setting->next = setting;
		else /* first node */
			settings_root = setting;
		fread(&(setting->id), sizeof(short), 1, fp);
		fread(&(setting->type), sizeof(short), 1, fp);
		switch (setting->type) {
		case SETTING_INT:
			setting->value = malloc(sizeof(int));
			fread(setting->value, sizeof(int), 1, fp);
			break;
		case SETTING_STRING:
			fread(&len, sizeof(int), 1, fp);
			setting->value = malloc(len);
			fread((setting->value), 1, len, fp);
			break;
		}
		setting->next = NULL;
		prev_setting = setting;
	}
	fclose(fp);
	return 0;
}

int save_settings(char *settings_file)
{
	FILE *fp;
	setting_st *setting = settings_root;

	if ((fp = fopen(settings_file, "w")) == NULL) {
		return -1;
	}
	fwrite(SETTINGS_MAGIC, strlen(SETTINGS_MAGIC), 1, fp);
	while(setting != NULL) {
		int len;
		fwrite(&(setting->id), sizeof(short), 1, fp);
		fwrite(&(setting->type), sizeof(short), 1, fp);
		switch (setting->type) {
		case SETTING_INT:
			fwrite(setting->value, sizeof(int), 1, fp);
			break;
		case SETTING_STRING:
			len = strlen((char *)(setting->value))+1;
			fwrite(&len, sizeof(int), 1, fp);
			fwrite((setting->value), 1, len, fp);
			break;
		}
		setting = setting->next;
	}
	fclose(fp);
	return 0;
}

