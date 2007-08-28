/*
 * Copyright (C) 2005 Jonathan Bettencourt (jonrelay)
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

#include "pz.h"
#include <string.h>

#ifdef IPOD
#define PTEXT_WORDS_PATH "/usr/share/ptextwords"
#else
#define PTEXT_WORDS_PATH "ptextwords"
#endif

#ifdef IPOD
#define PTEXT_DB_PATH "/usr/share/ptext.db"
#else
#define PTEXT_DB_PATH "ptext.db"
#endif

PzModule * module;

/* predictive text dictionary in basic mode */
typedef struct ti_ptext_dict_ {
	char * numberstring;
	char * word;
} ti_ptext_dict;

static ti_ptext_dict * ti_ptext_dictionary;
static int ti_ptext_dict_size = 0;
static int ti_ptext_dict_real_size = 0;

/* predictive text dictionary in SQLite mode; soft-dependency */
typedef void tipt_sqlite3;
typedef int (*tipt_sqlite3_callback)(void *, int, char **, char **);
#define TIPT_SQLITE_OK 0

static tipt_sqlite3 * ti_ptext_db = NULL;

static int (*tipt_sqlite3_open)(const char *filename, tipt_sqlite3 **ppDb) = NULL;
static int (*tipt_sqlite3_close)(tipt_sqlite3 *) = NULL;
static int (*tipt_sqlite3_exec)(tipt_sqlite3 *, const char *sql, tipt_sqlite3_callback, void *, char **errmsg) = NULL;
static int (*tipt_sqlite3_get_table)(tipt_sqlite3 *, const char *sql, char ***resultp, int *nrow, int *ncolumn, char **errmsg) = NULL;
static void (*tipt_sqlite3_free_table)(char **result) = NULL;

#define PTEXT_DB (tipt_sqlite3_open && tipt_sqlite3_close && tipt_sqlite3_exec && tipt_sqlite3_get_table && tipt_sqlite3_free_table)


int ti_ptext_add_word(char * ns, char * w)
{
	if (!PTEXT_DB) {
		if (ti_ptext_dict_size >= ti_ptext_dict_real_size) {
			if (( ti_ptext_dictionary = (ti_ptext_dict *)realloc(ti_ptext_dictionary, sizeof(ti_ptext_dict) * (ti_ptext_dict_real_size+512)) ) == NULL ) { return 1; }
			ti_ptext_dict_real_size += 512;
		}
		ti_ptext_dictionary[ti_ptext_dict_size].numberstring = ns;
		ti_ptext_dictionary[ti_ptext_dict_size].word = w;
		ti_ptext_dict_size++;
		return 0;
	} else {
		char q[255];
		snprintf(q, 255, "INSERT INTO ptext VALUES(\'%s\', \'%s\');", ns, w);
		tipt_sqlite3_exec(ti_ptext_db, q, NULL, NULL, NULL);
		return 0;
	}
}

int ti_ptext_inited(void)
{
	if (!PTEXT_DB) {
		return (ti_ptext_dict_size != 0);
	} else {
		return (ti_ptext_db != NULL);
	}
}

int ti_ptext_init(void)
{
	if (!PTEXT_DB) {
		ti_ptext_dict_real_size = 512;
		ti_ptext_dict_size = 0;
		if ( (ti_ptext_dictionary = (ti_ptext_dict *)malloc(ti_ptext_dict_real_size * sizeof(ti_ptext_dict))) == NULL ) {
			pz_error(_("malloc failed; can't load ptext dictionary"));
			return 1;
		} else {
			
			FILE * fp;
			char currline[128];
			char * sof;
			char * fpos;
			char * currfield;
			char * word;
			char * numstr;
			if ( (fp = fopen(PTEXT_WORDS_PATH, "r")) == NULL ) {
				return 1;
			}
			clearerr(fp);
			while ( (fgets(currline, 120, fp) != NULL) && !feof(fp) ) {
				if (currline[strlen(currline)-1] == 10) { currline[strlen(currline)-1] = 0; }
				if ((currline[0] != '#') && (currline[0] != 0)) {
					fpos = currline;
					sof = fpos;
					while (((*fpos) != ',') && ((*fpos) != 0)) { fpos++; }
					if (( currfield = (char *)malloc((fpos - sof + 1) * sizeof(char)) ) == NULL) { return 1; }
					memcpy(currfield, sof, fpos-sof);
					currfield[fpos-sof] = 0;
					numstr = strdup(currfield);
					free(currfield);
					fpos++;
					sof = fpos;
					while (((*fpos) != ',') && ((*fpos) != 0)) { fpos++; }
					if (( currfield = (char *)malloc((fpos - sof + 1) * sizeof(char)) ) == NULL) { return 1; }
					memcpy(currfield, sof, fpos-sof);
					currfield[fpos-sof] = 0;
					word = strdup(currfield);
					free(currfield);
					fpos++;
					sof = fpos;
					if ( ti_ptext_add_word(numstr, word) ) { free(numstr); free(word); return 1; }
					free(numstr);
					free(word);
				}
			}
			fclose(fp);
			return 0;
		}
	} else {
		if (ti_ptext_db != NULL) {
			tipt_sqlite3_close(ti_ptext_db);
			ti_ptext_db = NULL;
		}
		return ((  tipt_sqlite3_open(PTEXT_DB_PATH, &ti_ptext_db)  ) == TIPT_SQLITE_OK);
	}
}

void ti_ptext_free(void)
{
	if (PTEXT_DB) {
		if (ti_ptext_db != NULL) {
			tipt_sqlite3_close(ti_ptext_db);
			ti_ptext_db = NULL;
		}
	}
}

char * ti_ptext_letters_to_numbers(char * s)
{
	char * t = strdup(s);
	int i;
	for (i=0; i<strlen(t); i++) {
		switch (t[i]) {
		case '0':
			/*
			t[i] = '0';
			break;
			*/
		case '1':
			t[i] = '1';
			break;
		case '2':
		case 'a':		case 'b':		case 'c':
		case 'A':		case 'B':		case 'C':
			t[i] = '2';
			break;
		case '3':
		case 'd':		case 'e':		case 'f':
		case 'D':		case 'E':		case 'F':
			t[i] = '3';
			break;
		case '4':
		case 'g':		case 'h':		case 'i':
		case 'G':		case 'H':		case 'I':
			t[i] = '4';
			break;
		case '5':
		case 'j':		case 'k':		case 'l':
		case 'J':		case 'K':		case 'L':
			t[i] = '5';
			break;
		case '6':
		case 'm':		case 'n':		case 'o':
		case 'M':		case 'N':		case 'O':
			t[i] = '6';
			break;
		case '7':
		case 'p':		case 'q':		case 'r':		case 's':
		case 'P':		case 'Q':		case 'R':		case 'S':
			t[i] = '7';
			break;
		case '8':
		case 't':		case 'u':		case 'v':
		case 'T':		case 'U':		case 'V':
			t[i] = '8';
			break;
		case '9':
		case 'w':		case 'x':		case 'y':		case 'z':
		case 'W':		case 'X':		case 'Y':		case 'Z':
			t[i] = '9';
			break;
		}
	}
	return t;
}

char * ti_ptext_look_up(char * s)
{
	if (!PTEXT_DB) {
		int i;
		for (i=0; i<ti_ptext_dict_size; i++) {
			if (!strcmp(s,ti_ptext_dictionary[i].numberstring)) {
				return ti_ptext_dictionary[i].word;
			}
		}
		return s;
	} else {
		char q[255];
		char ** r;
		int nr,nc;
		snprintf(q, 255, "SELECT word FROM ptext WHERE number=\'%s\';", s);
		tipt_sqlite3_get_table(ti_ptext_db, q, &r, &nr, &nc, NULL);
		if (nr < 1) {
			strncpy(q, s, 255);
		} else {
			strncpy(q, r[nc], 255);
		}
		tipt_sqlite3_free_table(r);
		return strdup(q);
	}
}

char * ti_ptext_reverse_look_up(char * s)
{
	if (!PTEXT_DB) {
		int i;
		for (i=0; i<ti_ptext_dict_size; i++) {
			if (!strcmp(s,ti_ptext_dictionary[i].word)) {
				return ti_ptext_dictionary[i].numberstring;
			}
		}
		return ti_ptext_letters_to_numbers(s);
	} else {
		char q[255];
		char ** r;
		int nr,nc;
		snprintf(q, 255, "SELECT number FROM ptext WHERE word=\'%s\';", s);
		tipt_sqlite3_get_table(ti_ptext_db, q, &r, &nr, &nc, NULL);
		if (nr < 1) {
			strncpy(q, ti_ptext_letters_to_numbers(s), 255);
		} else {
			strncpy(q, r[nc], 255);
		}
		tipt_sqlite3_free_table(r);
		return strdup(q);
	}
}

int ti_ptext_alphanum(char ch)
{
	return ( ((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z')) );
}

int ti_ptext_predict(char * buf, int pos, int method)
{
	int mypos = pos;
	int myend;
	int ldiff;
	char localcopy[256];
	char * localcpy;
	char t;
	if (!ti_ptext_inited()) { return 0; }	
	while ((mypos > 0) && ti_ptext_alphanum(buf[mypos-1])) {
		mypos--;
	}
	myend = mypos;
	while (ti_ptext_alphanum(buf[myend])) {
		myend++;
	}
	if (myend > mypos) {
		memcpy(localcopy, buf+mypos, myend-mypos);
		localcopy[myend-mypos] = 0;
		switch (method) {
		case 1:
			if ((localcopy[0] >= '0') && (localcopy[0] <= '9')) {
				localcpy = ti_ptext_letters_to_numbers(localcopy);
				strcpy(localcopy, localcpy);
			} else {
				t = localcopy[myend-mypos-1];
				if (t == '0') { t = '1'; }
				if ((t >= '0') && (t <= '9')) {
					localcopy[myend-mypos-1] = 0;
					localcpy = ti_ptext_reverse_look_up(localcopy);
					strcpy(localcopy, localcpy);
					localcopy[strlen(localcopy)+1] = 0;
					localcopy[strlen(localcopy)] = t;
				} else {
					localcpy = ti_ptext_letters_to_numbers(localcopy);
					strcpy(localcopy, localcpy);
				}
			}
			localcpy = ti_ptext_look_up(localcopy);
			break;
		default:
			localcpy = ti_ptext_letters_to_numbers(localcopy);
			localcpy = ti_ptext_look_up(localcpy);
			break;
		}
		ldiff = (myend-mypos)-strlen(localcpy);
		if (ldiff < 0) { return 0; }
		memcpy(buf+mypos, localcpy, strlen(localcpy));
		if (ldiff) {
			while (buf[myend-1]) {
				buf[myend-ldiff] = buf[myend];
				myend++;
			}
		}
		return ldiff;
	}
	return 0;
}

void init_ti_ptext()
{
	module = pz_register_module ("tiptext", 0);
	
	tipt_sqlite3_open = pz_module_softdep("sqlite", "sqlite3_open");
	tipt_sqlite3_close = pz_module_softdep("sqlite", "sqlite3_close");
	tipt_sqlite3_exec = pz_module_softdep("sqlite", "sqlite3_exec");
	tipt_sqlite3_get_table = pz_module_softdep("sqlite", "sqlite3_get_table");
	tipt_sqlite3_free_table = pz_module_softdep("sqlite", "sqlite3_free_table");
}

PZ_MOD_INIT(init_ti_ptext)
