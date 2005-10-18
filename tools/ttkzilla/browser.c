/*
 * Copyright (C) 2004 Damien Marchal, Bernard Leach
 * Copyright (C) 2005 Courtney Cavin
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
#ifdef IPOD
#include <linux/vt.h>
#endif
#include <dirent.h>
#include <string.h>

#define PZMOD
#include "pz.h"
#include "ipod.h"
#include "piezo.h"

#define FILE_TYPE_PROGRAM 0
#define FILE_TYPE_DIRECTORY 1
#define FILE_TYPE_OTHER 2

typedef struct {
    char *name;
    char *full_name;
    unsigned short type;
    int dotdot;
    TWidget *mlink;
    ttk_menu_item *ilink;
} Directory;

#define _MAKEDIR Directory *dir = (Directory *)item->data

static char *current_file;
static char curdir[2048];
static char curdir_hdr[40];

extern int is_video_type(char *extension);
extern void new_video_window(char *filename);
extern TWindow *new_textview_window(char * filename);
extern int is_image_type(char *extension);
#ifdef MIKMOD
extern int is_mikmod_playlist_type(char *extension);
extern int is_mikmod_song_type(char *extension);
extern void new_mikmod_player(char *filename);
extern void new_mikmod_player_song(char *filename);
#endif
#ifdef __linux__
#ifndef MPDC
extern int is_mp3_type(char *extension);
extern int is_aac_type(char *extension);
extern void new_mp3_window(char *filename, char *album, char *artist,
		char *title, unsigned short len);
extern void new_aac_window_get_meta(char *filename);
#endif /* !MPDC */
extern int is_raw_audio_type(char *extension);
extern void new_playback_window(char *filename);
extern int is_tzx_audio_type(char *extension);
extern void new_tzx_playback_window(char *filename);
#endif /* __linux__ */
extern int is_text_type(char * extension);
extern int is_ascii_file(char *filename);
extern TWindow *new_stringview_window(char *buf, char *title);
void new_exec_window(char *filename);

static TWidget *browser_new_menu (const char *dir, int w, int h);

void browser_make_short_dir() 
{
    if (strlen (curdir) > 20) {
	char *lastpart = strrchr (curdir, '/');
	if (!lastpart) {
	    strncpy (curdir_hdr, curdir, 20);
	    curdir_hdr[20] = 0;
	} else {
	    char *p = curdir;
	    while (*p == '/') p++;

	    curdir_hdr[0] = 0;

	    do {
		if (strlen (curdir_hdr) + strlen (lastpart) < 20) {
		    char *q = p, *r = curdir_hdr + strlen (curdir_hdr);
		    int n = 30;
		    while (n && *q && (*q != '/')) {
			*r++ = *q++;
			n--;
		    }
		    *r = 0;
		} else {
		    strcpy (curdir_hdr + 20 - strlen (lastpart), "..");
		    strcat (curdir_hdr, lastpart);
		    break;
		}
	    } while ((p = strchr (p + 1, '/')) != 0);
	}
    }
}

int browser_item_visible (ttk_menu_item *item) 
{
    _MAKEDIR;
    if (!strcmp (dir->name, "."))
	return 0;
    if (dir->name[0] == '.' && !dir->dotdot && !ipod_get_setting (BROWSER_HIDDEN))
	return 0;
    return 1;
}

static int dir_cmp(const void *x, const void *y) 
{
    ttk_menu_item *A = *(ttk_menu_item **)x, *B = *(ttk_menu_item **)y;
    Directory *a = (Directory *)A->data;
    Directory *b = (Directory *)B->data;

	if (a->type == FILE_TYPE_DIRECTORY &&
		b->type != FILE_TYPE_DIRECTORY)
		return -1;
	if (a->type != FILE_TYPE_DIRECTORY &&
		b->type == FILE_TYPE_DIRECTORY)
		return 1;
	
	return strcasecmp(a->name, b->name);
} 

static void browser_exec_file(char *filename)
{
	int len;
	char *path;

	len = strlen(filename) + strlen(curdir) + 2;
	path = (char *)malloc(len * sizeof(char));
	snprintf(path, len, "%s/%s", curdir, filename);
	new_exec_window(path);
	free(path);
}

static int is_script_type(char *extension)
{
	return strcmp(extension, ".sh") == 0;
}

static int is_binary_type(char *filename)
{
	FILE *fp;
	unsigned char header[12];
	struct stat ftype; 

	stat(filename, &ftype); 
	if(S_ISBLK(ftype.st_mode)||S_ISCHR(ftype.st_mode))
		return 0;
	if((fp = fopen(filename, "r"))==NULL) {
		perror(filename);
		return 0;
	}

	fread(header, sizeof(char), 12, fp);
	
	fclose(fp);
	if(strncmp((const char *)header, "bFLT", 4)==0)
		return 1;
	if(strncmp((const char *)header, "#!/bin/sh", 9)==0)
		return 1;
	return 0;
}

static TWindow *browser_vip_open_file()
{
	int len;
	char *execline;
	
	len = strlen(curdir) + strlen(current_file) + 8;
	execline = (char *)malloc(len * sizeof(char));
	snprintf(execline, len, "viP \"%s/%s\"", curdir, current_file);
	new_exec_window(execline);
	free(execline);
	return TTK_MENU_UPONE;
}

static TWindow *browser_pipe_exec (ttk_menu_item *item)
{
	int len;
	char *buf = '\0';
	char *execline;
	char st_buf[512];
	FILE *fp;
	
	len = strlen(curdir) + strlen(current_file) + 2;
	execline = (char *)malloc(len * sizeof(char));
	snprintf(execline, len, "%s/%s", curdir, current_file);

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
		new_message_window("No Output");
		ret = TTK_MENU_DONOTHING;
	} else {
		ttk_show_window (new_stringview_window(buf, _("Pipe Output")));
		ret = TTK_MENU_REPLACE;
	}
	free(execline);
	free(buf);
	return ret;
}

static void handle_type_other(char *filename)
{
	char *ext;

	ext = strrchr(filename, '.');
	if (ext == 0) {
		if (is_ascii_file(filename)) {
			ttk_show_window (new_textview_window(filename));
		}
		else if (is_binary_type(filename)) {
			browser_exec_file(filename);
		}
		else {
			new_message_window(_("No Default Action for this Filetype"));
		}
	}
#ifdef MIKMOD
	else if (is_mikmod_playlist_type(ext)) {
		new_mikmod_player(filename);
	}
	else if (is_mikmod_song_type(ext)) {
		new_mikmod_player_song(filename);
	}
#endif
	else if (is_image_type(ext)) {
		new_image_window(filename);
	}
#ifdef __linux__
#ifndef MPDC
	else if (is_mp3_type(ext)) {
		new_mp3_window(filename, _("Unknown Album"),
				_("Unknown Artist"), _("Unknown Title"), 0);
	}
	else if (is_aac_type(ext)) {
		new_aac_window_get_meta(filename);
	}
#endif /* !MPDC */
	else if (is_tzx_audio_type(ext)) {
		new_tzx_playback_window(filename);
	}
	else if (is_raw_audio_type(ext)) {
		new_playback_window(filename);
	}
#endif /* __linux __ */
	else if (is_video_type(ext)) {
		new_video_window(filename);
	}
	else if (is_script_type(ext)) {
		browser_exec_file(filename);
	}
	else if (is_text_type(ext)||is_ascii_file(filename)) {
		ttk_show_window (new_textview_window(filename));
	}
	else if (is_binary_type(filename)) {
		browser_exec_file(filename);
	}
	else  {
		new_message_window(_("No Default Action for this Filetype"));
	}
}

TWindow *browser_mh (ttk_menu_item *item) 
{
    _MAKEDIR;
    
    if (dir->type == FILE_TYPE_DIRECTORY) {
	if (dir->dotdot) {
	    chdir ("..");
	    getcwd (curdir, 2048);
	    browser_make_short_dir();
	    return TTK_MENU_UPONE;
	} else {
	    TWindow *ret = ttk_new_window();

	    chdir (dir->full_name);
	    getcwd (curdir, 2048);
	    browser_make_short_dir();
	    if (ipod_get_setting (BROWSER_PATH)) {
		ttk_window_set_title (ret, curdir_hdr);
	    } else {
		ttk_window_set_title (ret, _("File Browser"));
	    }
	    return ttk_add_widget (ret, browser_new_menu (curdir, item->menuwidth, item->menuheight));
	}
    } else {
	TWindow *old = ttk_windows->w;
	handle_type_other (dir->full_name);

	if (ttk_windows->w != old) {
	    item->sub = ttk_windows->w;
	    return TTK_MENU_ALREADYDONE;
	} else {
	    item->flags &= ~TTK_MENU_MADESUB;
	    return TTK_MENU_DONOTHING;
	}
    }
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
    _MAKEDIR;
    if (dir->type == FILE_TYPE_DIRECTORY) {
	if (dir->full_name[0] && dir->full_name[1] && strchr (dir->full_name + 2, '/')) // not "" or "/" or a root dir
	    rm_rf (dir->full_name);
	else
	    pz_error ("Dangerous rm -rf aborted.\n");
    } else {
	unlink (dir->full_name);
    }
    ttk_menu_remove_by_ptr (dir->mlink, dir->ilink);
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
    // Items after here are not put in the menu, but can be referenced by browser_action().
    /* dirs: */ { N_("Delete"), { browser_rmdir }, TTK_MENU_ICON_EXE, 0 },
    /* apps: */ { N_("Read output"), { browser_pipe_exec }, TTK_MENU_ICON_EXE, 0 },
    /* other:*/ { N_("Edit with viP"), { browser_vip_open_file }, TTK_MENU_ICON_SUB, 0 },
    /* files:*/ { N_("Delete"), { browser_delete }, 0, 0 }
};

static int action_maybequit (TWidget *this, int button, int time) 
{
    if (button != TTK_BUTTON_MENU)
	return ttk_menu_button (this, button, 0);
    return TTK_EV_DONE;
}

static int browser_action (TWidget *this, int button)
{
    if (button != TTK_BUTTON_ACTION)
	return 0;
    
    TWindow *popwin = ttk_new_window();
    TWidget *popmenu = ttk_new_menu_widget (empty_menu, ttk_menufont, ttk_screen->w * 2 / 3, ttk_screen->h / 3);

    Directory *dir = (Directory *) ttk_menu_get_selected_item (this) -> data;
    empty_menu[1].flags = empty_menu[2].flags = TTK_MENU_ICON_EXE; empty_menu[3].flags = TTK_MENU_ICON_SUB;
    empty_menu[4].flags = 0;
    empty_menu[1].data = empty_menu[2].data = empty_menu[3].data = empty_menu[4].data = dir;
    current_file = dir->name;
    switch (dir->type) {
    case FILE_TYPE_DIRECTORY:
	ttk_menu_append (popmenu, empty_menu + 1);
	break;
    case FILE_TYPE_PROGRAM:
	ttk_menu_append (popmenu, empty_menu + 2);
	ttk_menu_append (popmenu, empty_menu + 4);
	break;
    case FILE_TYPE_OTHER:
	if (access ("/bin/viP", X_OK) == 0)
	    ttk_menu_append (popmenu, empty_menu + 3);
	ttk_menu_append (popmenu, empty_menu + 4);
	break;
    }
    popmenu->button = action_maybequit;
    ttk_window_set_title (popwin, _("Select Action"));
    ttk_add_widget (popwin, popmenu);
    ttk_popup_window (popwin);
    ttk_run();
    ttk_free_window (popwin);
    return 0;
}

static int browser_down (TWidget *this, int button) 
{
    if (button == TTK_BUTTON_MENU)
	ttk_widget_set_timer (this, 1000);
    else if (button != TTK_BUTTON_ACTION)
	return ttk_menu_down (this, button);
    return 0;
}
static int browser_quit (TWidget *this) 
{
    int r;
    ttk_widget_set_timer (this, 0);
    while (ttk_windows->w->focus->down == browser_down &&
	   (r = ttk_hide_window (ttk_windows->w)) != -1)
	ttk_click();

    return 0;
}
static int browser_button (TWidget *this, int button, int time) 
{
    if (button == TTK_BUTTON_MENU) {
	ttk_widget_set_timer (this, 0);
	if (time > 1750)
	    browser_quit (this);
	return ttk_menu_button (this, button, time);
    } else if (button == TTK_BUTTON_ACTION) {
	return ttk_menu_down (this, button);
    }
    return 0;
}

static TWidget *browser_new_menu (const char *dirc, int w, int h)
{
    TWidget *ret = ttk_new_menu_widget (empty_menu, ttk_menufont, w, h);
    ret->holdtime = 500;
    ret->held = browser_action;
    ret->down = browser_down; // handles Menu holding, along with
    ret->button = browser_button; // this and
    ret->timer = browser_quit; // this

    chdir (dirc);
    
    DIR *dp = opendir (dirc);
    struct dirent *d;
    while ((d = readdir (dp))) {
	const char *name;
	Directory *dir = calloc (1, sizeof(Directory));

	if (!strcmp (d->d_name, "."))
	    continue;
	if (!strcmp (d->d_name, "..")) {
	    name = _(".. [Parent Directory]");
	    dir->dotdot = 1;
	} else {
	    name = d->d_name;
	}

	dir->name = malloc (strlen (name) + 1);
	strcpy (dir->name, name);
	dir->full_name = malloc (strlen (d->d_name) + strlen (dirc) + 2);
	strcpy (dir->full_name, dirc);
	if (dir->full_name[strlen (dir->full_name) - 1] != '/')
	    strcat (dir->full_name, "/");
	strcat (dir->full_name, d->d_name);
	
	ttk_menu_item *item = calloc (1, sizeof(struct ttk_menu_item));
	item->name = dir->name;
	item->makesub = browser_mh;
	item->data = (void *)dir; dir->mlink = ret; dir->ilink = item;
	item->visible = browser_item_visible;

	struct stat st;
	if (stat (dir->full_name, &st) < 0) {
	    pz_perror (dir->full_name);
	} else {
	    if (S_ISDIR (st.st_mode)) {
		dir->type = FILE_TYPE_DIRECTORY;
		item->flags = TTK_MENU_ICON_SUB;
	    } else if (st.st_mode & S_IXUSR) {
		dir->type = FILE_TYPE_PROGRAM;
		item->flags = TTK_MENU_ICON_EXE;
	    } else {
		dir->type = FILE_TYPE_OTHER;
		item->flags = 0;
	    }
	    
	    ttk_menu_append (ret, item);
	}
    }

    closedir (dp);
    ttk_menu_sort_my_way (ret, dir_cmp);

    return ret;
}

void new_browser_window(char *initial_path)
{
    TWindow *ret;

    if (initial_path) {
	chdir(initial_path);
    }
    getcwd(curdir, sizeof(curdir));
    ret = ttk_new_window();

    browser_make_short_dir();
    if (ipod_get_setting (BROWSER_PATH)) {
	ttk_window_set_title (ret, curdir_hdr);
    } else {
	ttk_window_set_title (ret, _("File Browser"));
    }
    ttk_add_widget (ret, browser_new_menu (curdir, ret->w, ret->h));
    ttk_show_window (ret);
}


void new_exec_window(char *filename)
{
#ifdef IPOD
	static const char * const tty0[] = { "/dev/tty0", "/dev/vc/0", 0 };
	static const char * const vcs[] = { "/dev/vc/%d", "/dev/tty%d", 0 };
	int i, tty0_fd, ttyfd = -1, oldvt, curvt, fd, status;
	pid_t pid;

	/* query for a free VT */
	ttyfd = -1;
	tty0_fd = -1;
	for (i = 0; tty0[i] && (tty0_fd < 0); ++i) {
		tty0_fd = open(tty0[i], O_WRONLY, 0);
	}

	if (tty0_fd < 0) {
		tty0_fd = dup(0); /* STDIN is a VT? */
	}
	ioctl(tty0_fd, VT_OPENQRY, &curvt);
	close(tty0_fd);
	
	if ((geteuid() == 0) && (curvt > 0)) {
		for (i = 0; vcs[i] && (ttyfd < 0); ++i) {
			char vtpath[12];

			sprintf(vtpath, vcs[i], curvt);
			ttyfd = open(vtpath, O_RDWR);
		}
	}
	if (ttyfd < 0) {
		fprintf(stderr, "No available TTYs.\n");
		return;
	}

	if (ttyfd >= 0) {
		/* switch to the correct vt */
		if (curvt > 0) {
			struct vt_stat vtstate;

			if (ioctl(ttyfd, VT_GETSTATE, &vtstate) == 0) {
				oldvt = vtstate.v_active;
			}
			if (ioctl(ttyfd, VT_ACTIVATE, curvt)) {
				perror("child VT_ACTIVATE");
				return;
			}
			if (ioctl(ttyfd, VT_WAITACTIVE, curvt)) {
				perror("child VT_WAITACTIVE");
				return;
			}
		}
	}

	switch(pid = vfork()) {
	case -1: /* error */
		perror("vfork");
		break;
	case 0: /* child */
		close(ttyfd);
		if(setsid() < 0) {
			perror("setsid");
			_exit(1);
		}
		close(0);
		if((fd = open("/dev/console", O_RDWR)) == -1) {
			perror("/dev/console");
			_exit(1);
		}
		if(dup(fd) == -1) {
			perror("stdin dup");
			_exit(1);
		}
		close(1);
		if(dup(fd) == -1) {
			perror("stdout dup");
			_exit(1);
		}
		close(2);
		if(dup(fd) == -1) {
			perror("stderr dup");
			_exit(1);
		}

		execl("/bin/sh", "sh", "-c", filename);
		fprintf(stderr, _("Exec failed! (Check Permissions)\n"));
		_exit(1);
		break;
	default: /* parent */
		waitpid(pid, &status, 0);
		sleep(5);
		
		if (oldvt > 0) {
        		if (ioctl(ttyfd, VT_ACTIVATE, oldvt)) {
				perror("parent VT_ACTIVATE");
				return;
			}
        		if(ioctl(ttyfd, VT_WAITACTIVE, oldvt)) {
				perror("parent VT_WAITACTIVE");
				return;
			}
		}
		if (ttyfd > 0)
			close(ttyfd);

		if (curvt > 0) {
			int oldfd;

			if ((oldfd = open("/dev/vc/1", O_RDWR)) < 0)
				oldfd = open("/dev/tty1", O_RDWR);
			if (oldfd >= 0) {
				if (ioctl(oldfd, VT_DISALLOCATE, curvt)) {
					perror("VT_DISALLOCATE");
					return;
				}
				close(oldfd);
			}
		}
		break;
	}

	if (oldvt > 0) {
	    ioctl (ttyfd, VT_ACTIVATE, oldvt);
	    ioctl (ttyfd, VT_WAITACTIVE, oldvt);
	}
	if (ttyfd > 0) {
	    ioctl (ttyfd, VT_DISALLOCATE, curvt);
	    close (ttyfd);
	}
#else
	new_message_window(filename);
#endif /* IPOD */
}
