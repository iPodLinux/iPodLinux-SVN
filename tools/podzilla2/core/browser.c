/*
 * Copyright (C) 2004 Damien Marchal, Bernard Leach
 * Copyright (C) 2005 Courtney Cavin, Joshua Oreman
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include "pz.h"

static TWindow *browser_pipe_exec (ttk_menu_item *item)
{
	int len;
	char *buf = '\0';
	char *execline = item->data;
	char st_buf[512];
	FILE *fp;
	
	if((fp = popen(execline, "r")) == NULL) {
		pz_perror(execline);
		return TTK_MENU_UPONE;
	}
	len = 0;
	while(fgets(st_buf, 512, fp) != NULL) {
		buf = (char *)realloc(buf, ((buf == '\0' ? 0 : strlen(buf)) +
				512) * sizeof(char));
		if(buf == NULL) {
			pz_error(_("malloc failed"));
			return TTK_MENU_UPONE;
		}
		if(len == 0) {
			strncpy(buf, st_buf, 512);
			len = 1;
		}
		else 
			strncat(buf, st_buf, 512);
	}
	pclose(fp);
	TWindow *ret;
	if(buf=='\0') {
		pz_message (_("No output."));
		ret = TTK_MENU_DONOTHING;
	} else {
		ttk_show_window (pz_create_stringview(buf, _("Pipe Output")));
		ret = TTK_MENU_REPLACE;
	}
	free(execline);
	free(buf);
	return ret;
}

static int is_ascii_file(const char *filename)
{
#define ASCIILEN 64
	FILE *fp;
	unsigned char buf[ASCIILEN], *ptr;
	unsigned long min_len;
	struct stat st; 

	stat(filename, &st); 
	if(S_ISBLK(st.st_mode)||S_ISCHR(st.st_mode))
		return 0;
	if((fp=fopen(filename, "r"))==NULL) {
		perror(filename);
		return 0;
	}
	min_len = st.st_size < ASCIILEN ? st.st_size : ASCIILEN;
	fread(buf, min_len, 1, fp);
	fclose(fp);
	
	for(ptr=buf;ptr-buf<min_len;ptr++)
		if(*ptr<7||*ptr>127)
			return 0;
	return 1;
}

static TWindow *new_textview_window(char *filename)
{
    struct stat st;
    if (stat (filename, &st) < 0)
	return 0;
    char *buf = malloc (st.st_size);
    if (!buf)
	return 0;
    int fd = open (filename, O_RDONLY);
    if (fd < 0)
	return 0;
    read (fd, buf, st.st_size);
    close (fd);
    
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, ttk_new_textarea_widget (ret->w, ret->h, buf, ttk_textfont, ttk_text_height (ttk_textfont) + 2));
    ttk_window_title (ret, strrchr (filename, '/')? strrchr (filename, '/') + 1 : filename);
    free (buf); // strdup'd in textarea
    return ret;
}

static TWindow *browser_textview (ttk_menu_item *item)
{
	return new_textview_window((char *)item->data);
}

typedef struct browser_handler
{
    int (*pred)(const char *);
    TWindow *(*handler)(const char *);
    struct browser_handler *next;
} browser_handler;
static browser_handler *handler_head;

typedef struct browser_action 
{
    int (*pred)(const char *);
    ttk_menu_item *action;
    struct browser_action *next;
} browser_action;
static browser_action *action_head;

void pz_browser_set_handler (int (*pred)(const char *),
		TWindow *(*handler)(const char *)) 
{
    browser_handler *cur = handler_head;
    if (!cur) {
	cur = handler_head = malloc (sizeof(browser_handler));
    } else {
	while (cur->next) cur = cur->next;
	cur->next = malloc (sizeof(browser_handler));
	cur = cur->next;
    }
    cur->pred = pred;
    cur->handler = handler;
    cur->next = 0;
}
void pz_browser_remove_handler (int (*pred)(const char *)) 
{
    browser_handler *cur = handler_head, *last = 0;
    while (cur) {
	if (cur->pred == pred) {
	    if (last) {
		last->next = cur->next;
		free (cur);
		cur = last->next;
	    } else {
		handler_head = cur->next;
		free (cur);
		cur = handler_head;
	    }
	} else {
	    cur = cur->next;
	}
    }
}

void pz_browser_add_action (int (*pred)(const char *), ttk_menu_item *action)
{
    browser_action *cur = action_head;
    if (!cur) {
	cur = action_head = malloc (sizeof(browser_action));
    } else {
	while (cur->next) cur = cur->next;
	cur->next = malloc (sizeof(browser_action));
	cur = cur->next;
    }
    cur->pred = pred;
    cur->action = action;
    cur->next = 0;
}
void pz_browser_remove_action (int (*pred)(const char *)) 
{
    browser_action *cur = action_head, *last = 0;
    while (cur) {
	if (cur->pred == pred) {
	    if (last) {
		last->next = cur->next;
		free (cur);
		cur = last->next;
	    } else {
		action_head = cur->next;
		free (cur);
		cur = action_head;
	    }
	} else {
	    cur = cur->next;
	}
    }
}

TWindow *pz_browser_open (const char *path) 
{
    browser_handler *cur = handler_head;
    while (cur) {
	if (cur->pred && (*(cur->pred))(path))
	    return (*(cur->handler))(path);
	cur = cur->next;
    }
    return 0;
}

static void rm_rf (const char *path) 
{
    struct stat st;
    DIR *dp;
 
    stat (path, &st);
    if (!S_ISDIR (st.st_mode)) {
	unlink (path);
    } else {
	struct dirent *d;
	
	chdir (path);
	dp = opendir (".");
	if (!dp) return;
	while ((d = readdir (dp)) != 0) {
	    if ((strcmp (d->d_name, ".") != 0) && (strcmp (d->d_name, "..") != 0))
		rm_rf (d->d_name);
	}
	closedir (dp);
	chdir ("..");
	rmdir (path);
    }
}

static TWindow *browser_aborted (ttk_menu_item *item) { return TTK_MENU_UPONE; }
static TWindow *browser_do_delete (ttk_menu_item *item) 
{
#define PATHSIZ 512
	struct stat st;
	const char *filename = item->data;
	if (stat (filename, &st) < 0) {
		pz_perror (filename);
		return TTK_MENU_UPONE;
	}
	if (S_ISDIR (st.st_mode)) {
		/* we change the current directory here in order to find the
		 * absolute path of the specified directory with getcwd. this
		 * removes the need to check for references to the parent
		 * directory or follow along a path until we find the end
		 * result. in addition, opening the current directory and using
		 * its file descriptor to return to it is faster than storing
		 * its path */
		if (filename[0]) {
			int fd;
			char full_path[PATHSIZ];
			if ((fd = open(".", O_RDONLY)) == -1)
				return TTK_MENU_UPONE;
			chdir(filename);
			getcwd(full_path, PATHSIZ);
			fchdir(fd);
			close(fd);
			if (full_path[0] && full_path[1] &&
					strchr(full_path + 2, '/'))
				rm_rf (filename); // not "" or "/" or a root dir
			else
				pz_error (_("Dangerous rm -rf aborted.\n"));
		}
	} else {
		unlink (filename);
	}
	return TTK_MENU_QUIT;
}

static ttk_menu_item rmdir_menu[] = {
    { N_("No, don't delete it."), { browser_aborted }, 0, 0 },
    { N_("Yes, delete along with everything inside."), { browser_do_delete }, TTK_MENU_ICON_EXE, 0 },
    { 0, {0}, 0, 0 }
};

static TWindow *browser_rmdir (ttk_menu_item *item) 
{
    TWindow *ret = ttk_new_window();
    ttk_window_set_title (ret, _("Really Delete Directory?"));
    rmdir_menu[0].flags = 0; rmdir_menu[1].flags = TTK_MENU_ICON_EXE;
    rmdir_menu[0].data = rmdir_menu[1].data = item->data;
    ttk_add_widget (ret, ttk_new_menu_widget (rmdir_menu, ttk_textfont, item->menuwidth, item->menuheight));
    ttk_set_popup (ret);
    return ret;
}

static ttk_menu_item delete_menu[] = {
    { N_("Whoops, I didn't mean that."), { browser_aborted }, 0, 0 },
    { N_("Yes, delete it."), { browser_do_delete }, TTK_MENU_ICON_EXE, 0 },
    { 0, {0}, 0, 0 }
};

static TWindow *browser_delete (ttk_menu_item *item) 
{
    TWindow *ret = ttk_new_window();
    ttk_window_set_title (ret, _("Really Delete?"));
    delete_menu[0].flags = 0; delete_menu[1].flags = TTK_MENU_ICON_EXE;
    delete_menu[0].data = delete_menu[1].data = item->data;
    ttk_add_widget (ret, ttk_new_menu_widget (delete_menu, ttk_textfont, item->menuwidth, item->menuheight));
    ttk_set_popup (ret);
    return ret;
}

static ttk_menu_item empty_menu[] = {
    { 0, { 0 }, 0, 0 },
    // Items after here are not put in the menu, but can be referenced by browser_handle_action().
    /* dirs: */ { N_("Delete"), { browser_rmdir }, TTK_MENU_ICON_EXE, 0 },
    /* apps: */ { N_("Read output"), { browser_pipe_exec }, TTK_MENU_ICON_EXE, 0 },
    /* files:*/ { N_("Delete"), { browser_delete }, 0, 0 },
    /* files:*/ { N_("View contents"), { browser_textview }, 0, 0 }
};

TWidget *pz_browser_get_actions (const char *path)
{
    struct stat st;
    browser_action *cur = action_head;
    TWidget *ret = ttk_new_menu_widget (empty_menu, ttk_menufont, ttk_screen->w - ttk_screen->wx,
					ttk_screen->h - ttk_screen->wy);
    // add default handlers XXX
    empty_menu[1].flags = empty_menu[2].flags = TTK_MENU_ICON_EXE;
    empty_menu[4].flags = 0;
    empty_menu[1].data = empty_menu[2].data = empty_menu[3].data = empty_menu[4].data = (char *)path;
    if (stat (path, &st) >= 0) {
	if (st.st_mode & S_IFDIR) {
	    ttk_menu_append (ret, empty_menu + 1);
	} else if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode)) {
	    if (st.st_mode & S_IXUSR) {
	    	ttk_menu_append (ret, empty_menu + 2);
	    }
	    ttk_menu_append (ret, empty_menu + 3);
	    if (is_ascii_file(path)) {
		ttk_menu_append (ret, empty_menu + 4);
	    }
	}
    }
    while (cur) {
	if (cur->pred && (*(cur->pred))(path)) {
	    cur->action->data = (char *)path;
	    ttk_menu_append (ret, cur->action);
	}
	cur = cur->next;
    }
    return ret;
}

