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

#include "pz.h"
#include "ipod.h"
#include "mlist.h"
#include "piezo.h"

static GR_TIMER_ID browser_key_timer;
static GR_WINDOW_ID browser_wid;
static GR_GC_ID browser_gc;
static menu_st *browser_menu, *browser_menu_overlay;

#define FILE_TYPE_PROGRAM 0
#define FILE_TYPE_DIRECTORY 1
#define FILE_TYPE_OTHER 2
#define MAX_ENTRIES 256

typedef struct {
	char name[64];
	char *full_name;
	unsigned short type;
	unsigned int op;
} Directory;

static char current_dir[128];
static char *current_file;
static int browser_nbEntries = 0;
static Directory browser_entries[MAX_ENTRIES];

extern int is_video_type(char *extension);
extern void new_video_window(char *filename);
extern void new_textview_window(char * filename);
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
extern void new_stringview_window(char *buf, char *title);
void new_exec_window(char *filename);

static int dir_cmp(const void *x, const void *y) 
{
	Directory *a = (Directory *)x;
	Directory *b = (Directory *)y;

	if (a->type == FILE_TYPE_DIRECTORY &&
		b->type != FILE_TYPE_DIRECTORY)
		return -1;
	if (a->type != FILE_TYPE_DIRECTORY &&
		b->type == FILE_TYPE_DIRECTORY)
		return 1;
	
	return strcasecmp(a->name, b->name);
} 

static void browser_exit()
{
	int i;

	for (i = 0; i < browser_nbEntries; i++) {
		free(browser_entries[i].full_name);
	}
	browser_nbEntries = 0;
}

/* the directory to scan */
static void browser_mscandir(char *dira)
{
	DIR *dir = opendir(dira);
	int i, size, op;
	struct stat stat_result;
	struct dirent *subdir;

	if(browser_menu != NULL) {
		menu_destroy(browser_menu);
	}
	browser_menu = menu_init(browser_wid, _("File Browser"), 0, 0,
			screen_info.cols, screen_info.rows -
			(HEADER_TOPLINE + 1), NULL, NULL, ASCII);

	/* not very good for fragmentation... */
	for (i = 0; i < browser_nbEntries; i++) {
		free(browser_entries[i].full_name);
	}
	browser_nbEntries = 0;

	while ((subdir = readdir(dir))) {
		if(strncmp(subdir->d_name, ".", strlen(subdir->d_name)) == 0) {
			continue;
		}
		if(strncmp(subdir->d_name, "..", strlen(subdir->d_name)) == 0) {
			strcpy(browser_entries[browser_nbEntries].name,
				_(".. [Parent Directory]"));
			browser_entries[browser_nbEntries].full_name =
				strdup(browser_entries[browser_nbEntries].name);
			browser_entries[browser_nbEntries].type =
				FILE_TYPE_DIRECTORY;
			browser_entries[browser_nbEntries].op = STUB_MENU;
			browser_nbEntries++;
			continue;
		}
		if(!ipod_get_setting(BROWSER_HIDDEN)) {
			if (subdir->d_name[0] == '.') {
				continue;
			}
		}
		if(browser_nbEntries >= MAX_ENTRIES) {
			pz_error(_("Directory contains too many files."));
			break;
		}
		
		size = strlen(subdir->d_name);
		size = (size > 62 ? 62 : size);
		browser_entries[browser_nbEntries].full_name =
			strdup(subdir->d_name);

		strncpy(browser_entries[browser_nbEntries].name,
				subdir->d_name, size);
		browser_entries[browser_nbEntries].name[size] = '\0';

		stat(subdir->d_name, &stat_result);
		if(S_ISDIR(stat_result.st_mode)) {
			browser_entries[browser_nbEntries].type =
			    FILE_TYPE_DIRECTORY;
			op = ARROW_MENU;
		}
		else if(stat_result.st_mode & S_IXUSR) {
			browser_entries[browser_nbEntries].type =
			    FILE_TYPE_PROGRAM;
			op = EXECUTE_MENU;
		}
		else {
			browser_entries[browser_nbEntries].type =
				FILE_TYPE_OTHER;
			op = STUB_MENU;
		}
		browser_entries[browser_nbEntries].op = op;

		browser_nbEntries++;
	}
	closedir(dir);

	qsort(browser_entries, browser_nbEntries, sizeof(Directory), dir_cmp);
	for (i = 0; i < browser_nbEntries; i++)
		menu_add_item(browser_menu, browser_entries[i].name,
				NULL, 0, browser_entries[i].op);
}

static void browser_exec_file(char *filename)
{
	int len;
	char *path;

	len = strlen(filename) + strlen(current_dir) + 2;
	path = (char *)malloc(len * sizeof(char));
	snprintf(path, len, "%s/%s", current_dir, filename);
	new_exec_window(path);
	free(path);
}

static char short_current_dir[80];
static char * browser_shorten_path()
{
	int rlen = strlen( current_dir );
	int pos;
	int max = 20;

	/* this following block is untested... */
	if( screen_info.cols > 160 ) /* photo */
		max = 35;
	else if( screen_info.cols < 160 ) /* mini */
		max = 17;

	if( rlen > 20 ) /* 20 is the cutoff point */
	{
		strcpy( short_current_dir, "--" );
		for( pos=2; pos<(20+2) ; pos++ )
		{
			short_current_dir[pos] = current_dir[rlen-20+pos];
		}
		short_current_dir[pos] = '\0';
	} else {
		strcpy( short_current_dir, current_dir );
	}
	return( short_current_dir );
}

static void browser_do_draw()
{
	/* window is focused */
	if(browser_wid == GrGetFocus()) {
		if( ipod_get_setting( BROWSER_PATH ))
			pz_draw_header(browser_shorten_path());
		else
			pz_draw_header(browser_menu->title);
		menu_draw(browser_menu);
	}
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

static void handle_type_other(char *filename)
{
	char *ext;

	ext = strrchr(filename, '.');
	if (ext == 0) {
		if (is_ascii_file(filename)) {
			new_textview_window(filename);
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
		new_textview_window(filename);
	}
	else if (is_binary_type(filename)) {
		browser_exec_file(filename);
	}
	else  {
		new_message_window(_("No Default Action for this Filetype"));
	}
}

static void browser_selection_activated(unsigned short userChoice)
{
	if (userChoice == 0) {
		chdir("..");
		getcwd(current_dir, sizeof(current_dir));
		browser_mscandir("./");
	}
	switch (browser_entries[userChoice].type) {
	case FILE_TYPE_DIRECTORY:
		chdir(browser_entries[userChoice].full_name);
		getcwd(current_dir, sizeof(current_dir));
		browser_mscandir("./");
		break;
	case FILE_TYPE_PROGRAM:
	case FILE_TYPE_OTHER:
		handle_type_other(browser_entries[userChoice].full_name);
		break;
	}
}

static void browser_vip_open_file()
{
	int len;
	char *execline;
	
	len = strlen(current_dir) + strlen(current_file) + 8;
	execline = (char *)malloc(len * sizeof(char));
	snprintf(execline, len, "viP \"%s/%s\"", current_dir, current_file);
	new_exec_window(execline);
	free(execline);
}

static void browser_pipe_exec()
{
	int len;
	char *buf = '\0';
	char *execline;
	char st_buf[512];
	FILE *fp;
	
	len = strlen(current_dir) + strlen(current_file) + 2;
	execline = (char *)malloc(len * sizeof(char));
	snprintf(execline, len, "%s/%s", current_dir, current_file);

	if((fp = popen(execline, "r")) == NULL) {
		pz_perror(execline);
		return;
	}
	len = 0;
	while(fgets(st_buf, 512, fp) != NULL) {
		buf = (char *)realloc(buf, ((buf == '\0' ? 0 : strlen(buf)) +
				512) * sizeof(char));
		if(buf == NULL) {
			pz_error(_("malloc failed"));
			return;
		}
		if(len == 0) {
			strncpy(buf, st_buf, 512);
			len = 1;
		}
		else 
			strncat(buf, st_buf, 512);
	}
	pclose(fp);
	if(buf=='\0')
		new_message_window("No Output");
	else
		new_stringview_window(buf, _("Pipe Output"));
	free(execline);
	free(buf);
}

static void browser_rmdir(char *dirname)
{
	DIR *dir = opendir(dirname);
	struct stat stat_result;
	struct dirent *subdir;

	if(strncmp(dirname, ".", strlen(dirname)) == 0 ||
			strncmp(dirname, "..", strlen(dirname)) == 0)
		return;
	chdir(dirname);

	while((subdir = readdir(dir))) {
		if(strncmp(subdir->d_name, ".", strlen(subdir->d_name)) == 0 ||
				strncmp(subdir->d_name, "..",
				strlen(subdir->d_name)) == 0)
			continue;
		stat(subdir->d_name, &stat_result);
		if(S_ISDIR(stat_result.st_mode))
			browser_rmdir(subdir->d_name);
		else
			unlink(subdir->d_name);
	}
	chdir("../");
	rmdir(dirname);
	closedir(dir);
}

static void browser_delete_file()
{
	struct stat stat_result;

	browser_menu_overlay = browser_menu;
	stat(current_file, &stat_result);
	if(S_ISDIR(stat_result.st_mode)) {
		browser_rmdir(current_file);
	}
	else if(unlink(current_file) == -1) {
		pz_perror(current_file);
		return;
	}
	
	while(browser_menu_overlay->parent != NULL)
		browser_menu_overlay = menu_destroy(browser_menu_overlay);
	menu_delete_item(browser_menu_overlay, browser_menu_overlay->sel);
}

static void browser_delete_confirm()
{
	struct stat stat_result;
	static item_st delete_confirm_menu[] = {
		{N_("Whoops. No thanks."), NULL, SUB_MENU_PREV},
		{N_("Yes, Delete it."), browser_delete_file, ACTION_MENU},
		{0}
	};

	browser_menu = menu_init(browser_wid, _("Are You Sure?"), 0, 0,
			screen_info.cols, screen_info.rows -
			(HEADER_TOPLINE + 1), browser_menu,
			delete_confirm_menu, ASCII);
	browser_menu_overlay = browser_menu;

	stat(current_file, &stat_result);
	if(S_ISDIR(stat_result.st_mode)) {
		pz_error(_("Warning: this will delete everything under %s"),
				current_file);
	}
}

static void browser_action(unsigned short userChoice)
{
	current_file = browser_entries[userChoice].full_name;
	if(strncmp(current_file, ".", strlen(current_file)) == 0 ||
			strncmp(current_file, "..", strlen(current_file)) == 0)
		return;

	browser_menu = menu_init(browser_wid, browser_entries[userChoice].name,
			0, 0, screen_info.cols, screen_info.rows -
			(HEADER_TOPLINE + 1), browser_menu, NULL, ASCII);

	switch (browser_entries[userChoice].type) {
	case FILE_TYPE_DIRECTORY:
		break;
	case FILE_TYPE_PROGRAM:
		menu_add_item(browser_menu, _("Pipe output to textviewer"),
				browser_pipe_exec, 0, ACTION_MENU |
				SUB_MENU_PREV);
	case FILE_TYPE_OTHER:
		if(access("/bin/viP", X_OK) == 0)
			menu_add_item(browser_menu, _("Open with viP"),
					browser_vip_open_file, 0, ACTION_MENU |
					SUB_MENU_PREV);
		break;
	}
	menu_add_item(browser_menu, _("Delete"),  browser_delete_confirm, 0,
			ACTION_MENU | ARROW_MENU);
}

static int browser_do_keystroke(GR_EVENT * event)
{
	int ret = 0;

	switch(event->type) {
	case GR_EVENT_TYPE_TIMER:
		if(((GR_EVENT_TIMER *)event)->tid == browser_key_timer) {
			GrDestroyTimer(browser_key_timer);
			browser_key_timer = 0;
			menu_handle_timer(browser_menu, 1);
			browser_action(browser_menu->items[browser_menu->sel].orig_pos);
			browser_do_draw();
		}
		else
			menu_draw_timer(browser_menu);
		break;

	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			if(browser_menu->parent == NULL)
				browser_key_timer = GrCreateTimer(browser_wid,
						500);
			else {
				menu_handle_timer(browser_menu, 1);
				browser_menu = menu_handle_item(browser_menu, 
						browser_menu->sel);
				if(browser_menu_overlay) {
					browser_menu = browser_menu_overlay;
					browser_menu_overlay = 0;
				}
				browser_do_draw();
			}	
			break;

		case 'm':
		case 'q':
			browser_menu = menu_destroy(browser_menu);
			ret |= KEY_CLICK;
			if(browser_menu != NULL) {
				browser_do_draw();
				break;
			}
			browser_exit();
			GrDestroyGC(browser_gc);
			pz_close_window(browser_wid);
			break;

		case 'r':
			if (menu_shift_selected(browser_menu, 1)) {
				menu_draw(browser_menu);
				ret |= KEY_CLICK;
			}
			break;

		case 'l':
			if (menu_shift_selected(browser_menu, -1)) {
				menu_draw(browser_menu);
				ret |= KEY_CLICK;
			}
			break;
		default:
			ret |= KEY_UNUSED;
		}
		break;
	case GR_EVENT_TYPE_KEY_UP:
		switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			if(browser_key_timer) {
				GrDestroyTimer(browser_key_timer);
				browser_key_timer = 0;
				menu_handle_timer(browser_menu, 1);
				browser_selection_activated(browser_menu->items[browser_menu->sel].orig_pos);
				browser_do_draw();
			}
			break;
		}
		break;
	default:
		ret |= EVENT_UNUSED;
		break;
	}
	return ret;
}

void new_browser_window(char *initial_path)
{
	if (initial_path) {
		chdir(initial_path);
	}
	getcwd(current_dir, sizeof(current_dir));

	browser_gc = pz_get_gc(1);
	GrSetGCUseBackground(browser_gc, GR_FALSE);
	GrSetGCBackground(browser_gc, WHITE);
	GrSetGCForeground(browser_gc, BLACK);

	browser_wid = pz_new_window(0, HEADER_TOPLINE + 2, screen_info.cols,
                                    screen_info.rows - (HEADER_TOPLINE + 2),
                                    browser_do_draw, browser_do_keystroke);

	GrSelectEvents(browser_wid, GR_EVENT_MASK_EXPOSURE |
			GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_KEY_DOWN |
			GR_EVENT_MASK_TIMER);

	browser_menu = NULL;
	browser_menu_overlay = NULL;
	browser_mscandir("./");

	GrMapWindow(browser_wid);
}

void new_exec_window(char *filename)
{
#ifdef IPOD
	static const char *const tty0[] = {"/dev/tty0", "/dev/vc/0", 0};
	static const char *const vcs[] = {"/dev/vc/%d", "/dev/tty%d", 0};
	int i, tty0_fd, ttyfd, oldvt, curvt, fd, status;
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
			if (oldfd >= 0)
				if (ioctl(oldfd, VT_DISALLOCATE, curvt)) {
					perror("VT_DISALLOCATE");
					return;
				}
				close(oldfd);
			}
		}

		browser_do_draw();
		break;
	}
#else
	new_message_window(filename);
#endif /* IPOD */
}

