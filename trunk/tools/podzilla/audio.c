/*
 * Copyright (C) 2004 Bernard Leach
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

#include <sys/types.h>
#include <sys/stat.h>
#include <linux/soundcard.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "pz.h"

static GR_WINDOW_ID dsp_wid;
static GR_GC_ID dsp_gc;
static GR_SCREEN_INFO screen_info;
static pthread_t dsp_thread;
static int mode;
static volatile int playing = 0, recording = 0, paused = 0;

#define RECORD        0
#define PLAYBACK      1
#define PLAYBACK_ONLY 2

static volatile int killed;
static unsigned short dsp_buf[512];

int is_raw_audio_type(char *extension)
{
	return strcmp(extension, ".raw") == 0;
}

static void set_dsp_rate(int fd, int rate)
{
	ioctl(fd, SNDCTL_DSP_SPEED, &rate);
}

static void dsp_do_draw(GR_EVENT * event)
{
	if (mode == RECORD) {
		pz_draw_header("Record");
	}
	else {
		pz_draw_header("Playback");
	}

	GrSetGCForeground(dsp_gc, BLACK);
	GrFillRect(dsp_wid, dsp_gc, 0, 0, screen_info.cols, screen_info.rows);
	GrSetGCForeground(dsp_gc, WHITE);

	if (playing || recording) {
		GrText(dsp_wid, dsp_gc, 8, 20, "Press action to stop", -1, GR_TFASCII);
	}
	else {
		if (mode == RECORD) {
			GrText(dsp_wid, dsp_gc, 8, 20, "Press action to record", -1, GR_TFASCII);
		}
		else {
			GrText(dsp_wid, dsp_gc, 8, 20, "Press action to playback", -1, GR_TFASCII);
		}
	}
}

static void * dsp_record(void *filename)
{
	int dsp_fd, file_fd;

	file_fd = open((char *)filename, O_WRONLY|O_CREAT|O_TRUNC, 666);
	if (file_fd < 0) {
		char buf[256];
		sprintf(buf, "could not open %s\n", filename);
		new_message_window(buf);
		goto no_file;
	}

	dsp_fd = open("/dev/dsp", O_RDONLY);
	if (dsp_fd < 0) {
		new_message_window("could not open /dev/dsp");
		goto no_audio;
	}

	set_dsp_rate(dsp_fd, 44100);

	killed = 0;
	recording = 1;
	paused = 0;

	while (!killed) {
		ssize_t n, rem;

		rem = n = read(dsp_fd, (void *)dsp_buf, 512*2);
		if (n > 0) {
			while (rem) {
				n = write(file_fd, (void *)dsp_buf, rem);
				if (n > 0) {
					rem -= n;
				}
			}
		}
	}

	close(dsp_fd);

no_audio:
	close(file_fd);

no_file:
	recording = 0;
	mode = PLAYBACK;
	dsp_do_draw(0);

	return NULL;
}

static void * dsp_playback(void *filename)
{
	int dsp_fd, file_fd;
	ssize_t count = 0;

	file_fd = open((char *)filename, O_RDONLY);
	if (file_fd < 0) {
		char buf[256];
		sprintf(buf, "could not open %s\n", filename);
		new_message_window(buf);
		goto no_file;
	}

	dsp_fd = open("/dev/dsp", O_WRONLY);
	if (dsp_fd < 0) {
		new_message_window("could not open /dev/dsp");
		goto no_audio;
	}

	set_dsp_rate(dsp_fd, 44100);

	killed = 0;
	playing = 1;
	paused = 0;

	do {
		ssize_t n, rem;

		count = rem = n = read(file_fd, (void *)dsp_buf, 512*2);
		if (n > 0) {
			while (rem) {
				n = write(dsp_fd, (void *)dsp_buf, rem);
				if (n > 0) {
					rem -= n;
				}
			}
		}
	} while (!killed && count > 0);

	close(dsp_fd);

no_audio:
	close(file_fd);

no_file:

	playing = 0;
	if (mode == PLAYBACK) mode = RECORD;
	dsp_do_draw(0);

	return NULL;
}

static void start_recording()
{
	if (pthread_create(&dsp_thread, NULL, dsp_record, "test.raw") != 0) {
		new_message_window("could not create thread");
	}
}

static void start_playback()
{
	if (pthread_create(&dsp_thread, NULL, dsp_playback, "test.raw") != 0) {
		new_message_window("could not create thread");
	}
}

static void stop_dsp()
{
	killed = 1;
	pthread_join(dsp_thread, NULL);
}

static void pause_dsp()
{
	paused = 1;
}

static void resume_dsp()
{
	paused = 0;
}

static int dsp_do_keystroke(GR_EVENT * event)
{
	int ret = 1;

	switch (event->keystroke.ch) {
	case '\r':
	case '\n':
		if (mode == PLAYBACK || mode == PLAYBACK_ONLY) {
			if (playing) {
				stop_dsp();
				if (mode == PLAYBACK) mode = RECORD;
			}
			else {
				start_playback();
			}
		}
		else {
			if (recording) {
				stop_dsp();
				mode = PLAYBACK;
			}
			else {
				start_recording();
			}
		}
		dsp_do_draw(0);
		break;

	case 'm':
		if (playing || recording) {
			stop_dsp();
		}
		pz_close_window(dsp_wid);
		break;

	case 'd':
		if (playing || recording) {
			if (paused) {
				resume_dsp();
			}
			else {
				pause_dsp();
			}
		}
		break;
	}

	return 1;
}

void new_record_window(char *filename)
{
	mode = RECORD;

	dsp_gc = GrNewGC();
	GrSetGCUseBackground(dsp_gc, GR_TRUE);
	GrSetGCForeground(dsp_gc, WHITE);
	GrGetScreenInfo(&screen_info);

	dsp_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), dsp_do_draw, dsp_do_keystroke);

	GrSelectEvents(dsp_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(dsp_wid);
}

void new_playback_window(char *filename)
{
	mode = PLAYBACK_ONLY;

	dsp_gc = GrNewGC();
	GrSetGCUseBackground(dsp_gc, GR_TRUE);
	GrSetGCForeground(dsp_gc, WHITE);
	GrGetScreenInfo(&screen_info);

	dsp_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), dsp_do_draw, dsp_do_keystroke);

	GrSelectEvents(dsp_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(dsp_wid);
}

