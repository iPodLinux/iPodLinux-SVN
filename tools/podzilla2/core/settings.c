/* core/settings.c -*- mode: c; c-basic-offset: 8; -*-
 * Settings interface for PZ2.
 *
 * Copyright (C) 2005 Alastair Stuart and Joshua Oreman
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

typedef struct _pz_Config
{
    const char *filename;
    struct _pz_ConfItem *settings;
} PzConfig;

#define NODEF_CONFIG
#include "pz.h"

#define SETTINGS_MAGIC "EtRR" /* Expanded the Reading/Righting. Or not */

static PzConfItem *find_setting(PzConfig *conf, unsigned int sid)
{
	PzConfItem *current_setting = conf->settings;
	PzConfItem *prev_setting;

	if (current_setting == NULL)
		return NULL;

	while (current_setting != NULL) {
		prev_setting = current_setting;
		current_setting = current_setting->next;

		if (prev_setting->sid == sid) {
			break;
		}
	}

	if (current_setting)
		return prev_setting;
	else
		return 0;
}

void free_setting(PzConfItem *setting)
{
	if (setting) {
		int i;
		switch (setting->type) {
		case PZ_SETTING_STRING:
			if (setting->strval) free (setting->strval);
			break;
		case PZ_SETTING_ILIST:
			if (setting->ivals) free (setting->ivals);
			break;
		case PZ_SETTING_SLIST:
			if (setting->strvals) {
				for (i = 0; i < setting->nstrvals; i++)
					if (setting->strvals[i]) free (setting->strvals[i]);
				free (setting->strvals);
			}
			break;
		case PZ_SETTING_BLOB:
			if (setting->blobval) free (setting->blobval);
			break;
		}
		free(setting);
	}
}

void free_all_settings(PzConfig *conf)
{
	PzConfItem *next_setting, *current_setting;
	next_setting = current_setting = conf->settings;

	while (next_setting != NULL) {
		next_setting = current_setting->next;
		free_setting(current_setting);
		current_setting = next_setting;
	}
	conf->settings = NULL;
}

PzConfItem *pz_get_setting (PzConfig *conf, unsigned int sid) 
{
	PzConfItem *setting = conf->settings;
	while (setting) {
		if (setting->sid == sid)
			return setting;
		setting = setting->next;
	}
	return 0;
}

void pz_unset_setting (PzConfig *conf, unsigned int sid) 
{
	PzConfItem *setting = conf->settings, *last = 0;
	while (setting) {
		if (setting->sid == sid) {
			last->next = setting->next;
			free_setting (setting);
			setting = last->next;
		} else {
			last = setting;
			setting = setting->next;
		}
	}
}

int pz_get_int_setting (PzConfig *conf, unsigned int sid) 
{
	PzConfItem *ci;
	if ((ci = pz_get_setting (conf, sid)) && (ci->type == PZ_SETTING_INT)) {
		return ci->ival;
	}
	return -1;
}

const char *pz_get_string_setting (PzConfig *conf, unsigned int sid) 
{
	PzConfItem *ci;
	if ((ci = pz_get_setting (conf, sid)) && (ci->type == PZ_SETTING_STRING)) {
		return ci->strval;
	}
	return "";
}

void pz_set_int_setting(PzConfig *conf, unsigned int sid, int value)
{
	PzConfItem *setting, *last_setting;

	if (find_setting (conf, sid)) {
		setting = find_setting (conf, sid);
		setting->ival = value;
	} else {
		if (!conf->settings) {
			last_setting = conf->settings = malloc (sizeof(PzConfItem));
		} else {
			last_setting = conf->settings;
			while (last_setting->next) last_setting = last_setting->next;
		}
		last_setting->sid = sid;
		last_setting->type = PZ_SETTING_INT;
		last_setting->ival = value;
	}
}

void pz_set_string_setting(PzConfig *conf, unsigned int sid, const char *value)
{
	PzConfItem *setting, *last_setting;

	if (find_setting (conf, sid)) {
		setting = find_setting (conf, sid);
		free (setting->strval);
		setting->strval = strdup (value);
	} else {
		if (!conf->settings) {
			last_setting = conf->settings = malloc (sizeof(PzConfItem));
		} else {
			last_setting = conf->settings;
			while (last_setting->next) last_setting = last_setting->next;
		}
		last_setting->sid = sid;
		last_setting->type = PZ_SETTING_STRING;
		last_setting->strval = strdup (value);
	}
}

void pz_set_float_setting(PzConfig *conf, unsigned int sid, double value)
{
	PzConfItem *setting, *last_setting;

	if (find_setting (conf, sid)) {
		setting = find_setting (conf, sid);
		setting->fval = value;
	} else {
		if (!conf->settings) {
			last_setting = conf->settings = malloc (sizeof(PzConfItem));
		} else {
			last_setting = conf->settings;
			while (last_setting->next) last_setting = last_setting->next;
		}
		last_setting->sid = sid;
		last_setting->type = PZ_SETTING_FLOAT;
		last_setting->fval = value;
	}
}


PzConfig *pz_load_config(const char *settings_file)
{
	FILE *fp;
	int file_len;
	PzConfItem *setting = NULL, *prev_setting = NULL;
	PzConfig *ret = malloc (sizeof(PzConfig));
	char buf[32];

	ret->filename = settings_file;
	ret->settings = 0;

	if ((fp = fopen(settings_file, "r")) == NULL) {
		return ret;
	}
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	rewind(fp);
	fread(buf, strlen(SETTINGS_MAGIC), 1, fp);
	fflush(stdout);
	if (memcmp(buf, SETTINGS_MAGIC, strlen(SETTINGS_MAGIC))) {
		fclose(fp);
		return ret;
	}
	while(ftell(fp) < file_len) {
		int i, len;
		setting = malloc(sizeof(PzConfItem));
		if (prev_setting != NULL)
			prev_setting->next = setting;
		else /* first node */
			ret->settings = setting;
		fread(&(setting->sid), sizeof(int), 1, fp);
		fread(&(setting->type), sizeof(int), 1, fp);
		switch (setting->type) {
		case PZ_SETTING_INT:
			fread (&setting->ival, sizeof(int), 1, fp);
			break;
		case PZ_SETTING_STRING:
			fread (&len, sizeof(int), 1, fp);
			setting->strval = malloc (len + 1);
			fread (setting->strval, len, 1, fp);
			setting->strval[len] = 0;
			break;
		case PZ_SETTING_FLOAT:
			fread (&setting->fval, sizeof(double), 1, fp);
			break;
		case PZ_SETTING_ILIST:
			fread (&setting->nivals, sizeof(int), 1, fp);
			setting->ivals = malloc (setting->nivals * sizeof(int));
			fread (setting->ivals, sizeof(int), setting->nivals, fp);
			break;
		case PZ_SETTING_SLIST:
			fread (&setting->nstrvals, sizeof(int), 1, fp);
			setting->strvals = malloc ((setting->nstrvals + 1) * sizeof(char*));
			setting->strvals[setting->nstrvals] = 0;
			for (i = 0; i < setting->nstrvals; i++) {
				fread (&len, sizeof(int), 1, fp);
				setting->strvals[i] = malloc (len + 1);
				fread (setting->strvals[i], len, 1, fp);
				setting->strvals[i][len] = 0;
			}
			break;
		case PZ_SETTING_BLOB:
			fread (&setting->bloblen, sizeof(int), 1, fp);
			setting->blobval = malloc (setting->bloblen);
			fread (setting->blobval, setting->bloblen, 1, fp);
			break;
		}
		setting->next = NULL;
		prev_setting = setting;
	}
	fclose(fp);
	return ret;
}

void pz_save_config(PzConfig *conf)
{
	FILE *fp;
	PzConfItem *setting = conf->settings;

	if ((fp = fopen(conf->filename, "w")) == NULL) {
		pz_perror (conf->filename);
		return;
	}
	fwrite(SETTINGS_MAGIC, strlen(SETTINGS_MAGIC), 1, fp);
	while(setting != NULL) {
		int i, len;
		fwrite(&(setting->sid), sizeof(int), 1, fp);
		fwrite(&(setting->type), sizeof(int), 1, fp);
		switch (setting->type) {
		case PZ_SETTING_INT:
			fwrite(&setting->ival, sizeof(int), 1, fp);
			break;
		case PZ_SETTING_STRING:
			len = strlen(setting->strval);
			fwrite(&len, sizeof(int), 1, fp);
			fwrite(setting->strval, len, 1, fp);
			break;
		case PZ_SETTING_FLOAT:
			fwrite (&setting->fval, sizeof(double), 1, fp);
			break;
		case PZ_SETTING_ILIST:
			fwrite (&setting->nivals, sizeof(int), 1, fp);
			fwrite (setting->ivals, sizeof(int), setting->nivals, fp);
			break;
		case PZ_SETTING_SLIST:
			fwrite (&setting->nstrvals, sizeof(int), 1, fp);
			for (i = 0; i < setting->nstrvals; i++) {
			    len = strlen (setting->strvals[i]);
			    fwrite (&len, sizeof(int), 1, fp);
			    fwrite (setting->strvals[i], len, 1, fp);
			}
			break;
		case PZ_SETTING_BLOB:
			fwrite (&setting->bloblen, sizeof(int), 1, fp);
			fwrite (setting->blobval, setting->bloblen, 1, fp);
			break;
		}
		setting = setting->next;
	}
	fclose(fp);
}

void pz_blast_config (PzConfig *conf) 
{
	free_all_settings (conf);
	pz_save_config (conf);
}

void pz_free_config (PzConfig *conf) 
{
	free_all_settings (conf);
	free (conf);
}

void pz_config_iterate (PzConfig *conf, void (*fn)(PzConfItem *)) 
{
	PzConfItem *cur = conf->settings;
	while (cur) {
		(*fn)(cur);
		cur = cur->next;
	}
}
