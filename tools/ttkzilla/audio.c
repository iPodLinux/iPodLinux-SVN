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

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#ifdef PZ_USE_THREADS
#include <pthread.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <byteswap.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pz.h"
#include "ipod.h"
#include "oss.h"

extern void new_browser_window(char *);

static GR_TIMER_ID timer_id;
static GR_WINDOW_ID pcm_wid;
static GR_GC_ID pcm_gc;
#ifdef PZ_USE_THREADS
static pthread_t pcm_thread;
#endif
static int mode;
static volatile int playing = 0, recording = 0, paused = 0;
static char pcm_file[128];
static long currenttime = 0;
static int recording_src;
static dsp_st dspz;


#define RECORD        0
#define PLAYBACK      1

static volatile int killed;

#ifdef IPOD
#define RECORDINGS "/Recordings"
#else
#define RECORDINGS "Recordings"
#endif

#define DSP_REC_SIZE	4*1024*2
#define DSP_PLAY_SIZE	4*1024*2

static unsigned short pcm_buf[16*1024];

unsigned int read_32_le(int file_fd)
{
	int value;

	read(file_fd, &value, 4);
#ifdef IS_BIG_ENDIAN
	value = bswap_32(value);
#endif

	return value;
}

unsigned short read_16_le(int file_fd)
{
	short value;

	read(file_fd, &value, 2);
#ifdef IS_BIG_ENDIAN
	value = bswap_16(value);
#endif

	return value;
}

int write_32_le(int file_fd, unsigned int value)
{
#ifdef IS_BIG_ENDIAN
	value = bswap_32(value);
#endif
	return write(file_fd, &value, 4) == 4;
}

int write_16_le(int file_fd, unsigned short value)
{
#ifdef IS_BIG_ENDIAN
	value = bswap_16(value);
#endif
	return write(file_fd, &value, 2) == 2;
}

int write_wav_header(int file_fd, int samplerate, int channels)
{
	write(file_fd, "RIFF", 4);
	write_32_le(file_fd, 0);	// remaining length after this header
	write(file_fd, "WAVE", 4);

	/* fmt chunck */
	write(file_fd, "fmt ", 4);
	write_32_le(file_fd, 16);	// length of fmt is 16

	write_16_le(file_fd, 1);		// WAVE_FORMAT_PCM
	write_16_le(file_fd, channels);		// no. channels
	write_32_le(file_fd, samplerate);	// sample rate
	write_32_le(file_fd, samplerate * channels * 2);// avg bytes per second
	write_16_le(file_fd, channels * 2);	// block align
	write_16_le(file_fd, 16);		// bits per sample

	return 0;
}

int is_raw_audio_type(char *extension)
{
	return strcmp(extension, ".raw") == 0 || strcmp(extension, ".wav") == 0;
}

int get_user_sample_rate()
{
	int setting = ipod_get_setting(DSPFREQUENCY);

	switch (setting) {
	case 0:
		return 8000;
	case 1:
		return 32000;
	case 2:
		return 44100;
	case 3:
		return 88200;
	case 4:
		return 96000;
	}
	return 41000;
}

static void pcm_do_draw()
{
	if (mode == RECORD) {
		pz_draw_header(_("Record"));
	}
	else {
		pz_draw_header(_("Playback"));
	}

	GrSetGCForeground(pcm_gc, WHITE);
	GrFillRect(pcm_wid, pcm_gc, 0, 0, screen_info.cols, screen_info.rows);
	GrSetGCForeground(pcm_gc, BLACK);

	if (playing || recording) {
		GrText(pcm_wid, pcm_gc, 8, 20, _("Press action to stop"), -1, GR_TFASCII);
		if (paused) {
			GrText(pcm_wid, pcm_gc, 8, 35, _("Press Play/Pause to resume"), -1, GR_TFASCII);
		}
		else {
			GrText(pcm_wid, pcm_gc, 8, 35, _("Press Play/Pause to pause"), -1, GR_TFASCII);
		}
	}
	else {
		if (mode == RECORD) {
			GrText(pcm_wid, pcm_gc, 8, 20, _("Press action to record"), -1, GR_TFASCII);
		}
		else {
			GrText(pcm_wid, pcm_gc, 8, 20, _("Press action to playback"), -1, GR_TFASCII);
		}
	}
}

static void draw_time()
{
	struct tm *tm;
	char onscreentimer[128];

	/* draw the time using currenttime */
	tm = gmtime(&currenttime);
	sprintf(onscreentimer, "Time: %02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);

	GrText(pcm_wid, pcm_gc, 40, 80, onscreentimer, -1, GR_TFASCII);
}

static void * pcm_record(void *filename)
{
	int ret;
	int file_fd;
	int samplerate;
	int channels = 1;
	int start_pos;
	int filelength;
	int localpaused = 0;

	file_fd = open((char *)filename, O_WRONLY|O_CREAT|O_TRUNC,
			S_IRUSR|S_IWUSR);
	if (file_fd < 0) {
		pz_perror((char *)filename);
		goto no_file;
	}

	samplerate = get_user_sample_rate();
	if (recording_src == DSP_LINEIN) {
		channels = 2;
	}

	ret = dsp_open(&dspz, recording_src);
	if (ret < 0) {
		pz_perror("/dev/dsp");
		goto no_audio;
	}

	dsp_setup(&dspz, channels, samplerate);

	killed = 0;
	recording = 1;
	paused = 0;

	pcm_do_draw(0);

	write_wav_header(file_fd, samplerate, channels);

	/* start of data chunk */
	start_pos = (int)lseek(file_fd, 0, SEEK_CUR);
	write(file_fd, "data", 4);
	write_32_le(file_fd, 0);	// dummy length value

	timer_id = GrCreateTimer(pcm_wid, 1000);
	draw_time();

	while (!killed) {
		ssize_t n, rem;
		unsigned short *p = pcm_buf;
#ifndef PZ_USE_THREADS
		GR_EVENT event;
#endif

		if (paused && !localpaused) {
			localpaused = 1;
			GrDestroyTimer(timer_id);
		}
		if (!paused && localpaused) {
			localpaused = 0;
			timer_id = GrCreateTimer(pcm_wid, 1000);
		}

		if (!paused) {
			rem = n = dsp_read(&dspz, (void *)p, DSP_REC_SIZE *
					(samplerate > 8000 ? 2 : 1));
			if (n > 0) {
				while (rem) {
					n = write(file_fd, (void *)p, rem);
					if (n > 0) {
						rem -= n;
						p += (n/2);
					}
				}
			}
		}

#ifndef PZ_USE_THREADS
		if (paused || GrPeekEvent(&event)) {
repeat:
			GrGetNextEventTimeout(&event, 1000);
			if (event.type != GR_EVENT_TYPE_TIMEOUT) {
				pz_event_handler(&event);

				if (GrPeekEvent(&event)) goto repeat;
			}
		}
#endif
	}

	if (!localpaused) {
		GrDestroyTimer(timer_id);
	}

	dsp_close(&dspz);

	/* get file length by position which is 1 more than last */
	filelength = (int)lseek(file_fd, 1, SEEK_CUR) - 1;

	/* correct length value for the data chunk */
	lseek(file_fd, start_pos + 4, SEEK_SET);
	write_32_le(file_fd, filelength - (start_pos + 8));

	/* correct length value for the RIFF header */
	lseek(file_fd, 4, SEEK_SET);
	write_32_le(file_fd, filelength - 8);

no_audio:
	close(file_fd);

no_file:
	recording = 0;
	pcm_do_draw(0);

	return NULL;
}

static void * pcm_playback(void *filename)
{
	int ret;
	int file_fd;
	ssize_t count = 0;
	int samplerate;
	int channels;
	int localpaused = 0;

	file_fd = open((char *)filename, O_RDONLY);
	if (file_fd < 0) {
		pz_perror((char *)filename);
		goto no_file;
	}

	/* assume a basic WAV header, jump to number channels */
	lseek(file_fd, 22, SEEK_SET);
	channels = read_16_le(file_fd);
	samplerate = read_32_le(file_fd);

	ret = dsp_open(&dspz, DSP_LINEOUT);
	if (ret < 0) {
		pz_perror("/dev/dsp");
		goto no_audio;
	}

	dsp_setup(&dspz, channels, samplerate);

	killed = 0;
	playing = 1;
	paused = 0;

	pcm_do_draw(0);

	/* assume a basic WAV header, skip to start of PCM data */
	lseek(file_fd, 44, SEEK_SET);

	timer_id = GrCreateTimer(pcm_wid, 1000);
	draw_time();

	do {
		unsigned short *p = pcm_buf;
#ifndef PZ_USE_THREADS
		GR_EVENT event;
#endif

		if (paused && !localpaused) {
			localpaused = 1;
			GrDestroyTimer(timer_id);
		}
		if (!paused && localpaused) {
			localpaused = 0;
			timer_id = GrCreateTimer(pcm_wid, 1000);
		}

		if (!paused) {
			count = read(file_fd, (void *)p,
				DSP_PLAY_SIZE * (samplerate > 8000 ? 2 : 1));
			dsp_write(&dspz, (void *)p, count);
		}

#ifndef PZ_USE_THREADS
		if (paused || GrPeekEvent(&event)) {
repeat:
			GrGetNextEventTimeout(&event, 1000);
			if (event.type != GR_EVENT_TYPE_TIMEOUT)
			{
				pz_event_handler(&event);
				if (GrPeekEvent(&event)) goto repeat;
			}
		}
#endif
	} while (!killed && count > 0);

	dsp_close(&dspz);

	if (!localpaused) {
		GrDestroyTimer(timer_id);
	}

	if (!killed) {
		pz_close_window(pcm_wid);
	}

no_audio:
	close(file_fd);

no_file:
	playing = 0;

	return NULL;
}

static void start_recording()
{
#ifdef PZ_USE_THREADS
	if (pthread_create(&pcm_thread, NULL, pcm_record, pcm_file) != 0) {
		new_message_window("could not create thread");
	}
#else
	pcm_record(pcm_file);
#endif
}

static void start_playback()
{
#ifdef PZ_USE_THREADS
	if (pthread_create(&pcm_thread, NULL, pcm_playback, pcm_file) != 0) {
		new_message_window("could not create thread");
	}
#else
	pcm_playback(pcm_file);
#endif
}

static void stop_pcm()
{
	killed = 1;
#ifdef PZ_USE_THREADS
	pthread_join(pcm_thread, NULL);
#endif
}

static void pause_pcm()
{
	paused = 1;
}

static void resume_pcm()
{
	paused = 0;
}

static int pcm_do_keystroke(GR_EVENT * event)
{
	int ret = 0;

	switch (event->type) {
	case GR_EVENT_TYPE_TIMER:
		currenttime = currenttime + 1;
		draw_time();
		break;
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			if (playing || recording) {
				stop_pcm();
				pz_close_window(pcm_wid);
			}
			else {
				if (mode == PLAYBACK) {
					start_playback();
				}
				else {
					start_recording();
				}
			}
			ret |= KEY_CLICK;
			break;
	
		case 'm':
			if (playing || recording) {
				stop_pcm();
			}
			pz_close_window(pcm_wid);
			break;
	
		case '1':
		case 'd':
			if (playing || recording) {
				if (paused) {
					/* timer_id = GrCreateTimer(pcm_wid, 1000); draw_time(); */
					resume_pcm();
				}
				else {
					pause_pcm();
					/*GrDestroyTimer(timer_id); draw_time();*/
				}
			}
			pcm_do_draw(0);
			draw_time();
			ret |= KEY_CLICK;
			break;
		case '3':
		case 'l':
			if (playing) {
				dsp_vol_down(&dspz);
				ret |= KEY_CLICK;
			}
			break;
		case '2':
		case 'r':
			if (playing) {
				dsp_vol_up(&dspz);
				ret |= KEY_CLICK;
			}
			break;
		}
		break;
	}

	return ret;
}
	
void new_audio_window(char *filename)
{
	strncpy(pcm_file, filename, sizeof(pcm_file) - 1);

	playing = 0;
	recording = 0;
	paused = 0;
	currenttime = 0;

	pcm_gc = pz_get_gc(1);
	GrSetGCUseBackground(pcm_gc, GR_TRUE);
	GrSetGCForeground(pcm_gc, BLACK);
	GrSetGCBackground(pcm_gc, WHITE);

	pcm_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1),
			pcm_do_draw, pcm_do_keystroke);

	GrSelectEvents(pcm_wid, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_KEY_UP |
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);

	GrMapWindow(pcm_wid);
}

void new_record_window()
{
	char myfilename[128];
	time_t now;
	struct tm *tm;
	int hw_ver = hw_version/10000;

	// 3G, 4G and photo only
	if (!(hw_ver == 3 || hw_ver == 5 || hw_ver == 6)) {
		pz_error(_("Recording is unsupported on this hardware."));
		return;
	}

	time(&now);
	tm = localtime(&now);

	/* MMDDYYYY HHMMSS */
	sprintf(myfilename, "%s/%02d%02d%04d %02d%02d%02d.wav",
			RECORDINGS,
			tm->tm_mon+1, tm->tm_mday, tm->tm_year + 1900,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	
	mode = RECORD;
	new_audio_window(myfilename);
}

void new_record_mic_window()
{
	recording_src = DSP_MIC;
	new_record_window();
}

void new_record_line_in_window()
{
	recording_src = DSP_LINEIN;
	new_record_window();
}

void new_playback_window(char *filename)
{
	mode = PLAYBACK;
	new_audio_window(filename);

	pcm_do_draw(0);

	start_playback();
}

void new_playback_browse_window(void)
{
	new_browser_window(RECORDINGS);
}
#endif
