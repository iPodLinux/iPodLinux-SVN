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
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef PZ_USE_THREADS
#include <pthread.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <byteswap.h>

#include "pz.h"
#include "ipod.h"

extern void new_browser_window(char *);

static GR_TIMER_ID timer_id;
static GR_WINDOW_ID dsp_wid;
static GR_GC_ID dsp_gc;
#ifdef PZ_USE_THREADS
static pthread_t dsp_thread;
#endif
static int mode;
static volatile int playing = 0, recording = 0, paused = 0;
static char pcm_file[128];
static long currenttime = 0;
static int mixer_fd = -1;
static int pcm_vol = -1;
static int recording_src;


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

static unsigned short dsp_buf[16*1024];

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

static void set_dsp_fmt(int fd)
{
	int fmt = AFMT_S16_LE;
	ioctl(fd, SNDCTL_DSP_SETFMT, &fmt);
}

static void set_dsp_rate(int fd, int rate)
{
	/* sample rate */
	ioctl(fd, SNDCTL_DSP_SPEED, &rate);
}

static void set_dsp_channels(int fd, int channels)
{
	/* set mono or stereo */
	ioctl(fd, SNDCTL_DSP_CHANNELS, &channels);
}

static void dsp_do_draw()
{
	if (mode == RECORD) {
		pz_draw_header("Record");
	}
	else {
		pz_draw_header("Playback");
	}

	GrSetGCForeground(dsp_gc, WHITE);
	GrFillRect(dsp_wid, dsp_gc, 0, 0, screen_info.cols, screen_info.rows);
	GrSetGCForeground(dsp_gc, BLACK);

	if (playing || recording) {
		GrText(dsp_wid, dsp_gc, 8, 20, "Press action to stop", -1, GR_TFASCII);
		if (paused) {
			GrText(dsp_wid, dsp_gc, 8, 35, "Press Play/Pause to resume", -1, GR_TFASCII);
		}
		else {
			GrText(dsp_wid, dsp_gc, 8, 35, "Press Play/Pause to pause", -1, GR_TFASCII);
		}
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

static void draw_time()
{
	struct tm *tm;
	char onscreentimer[128];

	/* draw the time using currenttime */
	tm = gmtime(&currenttime);
	sprintf(onscreentimer, "Time: %02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);

	GrText(dsp_wid, dsp_gc, 40, 80, onscreentimer, -1, GR_TFASCII);
}

static void * dsp_record(void *filename)
{
	int dsp_fd, file_fd;
	int samplerate;
	int channels = 1;
	int start_pos;
	int filelength;
	int localpaused = 0;

	file_fd = open((char *)filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
	if (file_fd < 0) {
		char buf[256];
		sprintf(buf, "Cant open %s", (char *)filename);
		new_message_window(buf);
		goto no_file;
	}

	dsp_fd = open("/dev/dsp", O_RDONLY);
	if (dsp_fd < 0) {
		new_message_window("Cant open /dev/dsp");
		goto no_audio;
	}

	samplerate = get_user_sample_rate();
	if (recording_src == SOUND_MASK_LINE) {
		channels = 2;
	}
	set_dsp_channels(dsp_fd, channels);
	set_dsp_rate(dsp_fd, samplerate);
	set_dsp_fmt(dsp_fd);

	mixer_fd = open("/dev/mixer", O_RDWR);
	if (mixer_fd >= 0) {
		ioctl(mixer_fd, SOUND_MIXER_WRITE_RECSRC, &recording_src);
		close(mixer_fd);
		mixer_fd = -1;
	}

	killed = 0;
	recording = 1;
	paused = 0;

	dsp_do_draw(0);

	write_wav_header(file_fd, samplerate, channels);

	/* start of data chunk */
	start_pos = (int)lseek(file_fd, 0, SEEK_CUR);
	write(file_fd, "data", 4);
	write_32_le(file_fd, 0);	// dummy length value

	timer_id = GrCreateTimer(dsp_wid, 1000);
	draw_time();

	while (!killed) {
		ssize_t n, rem;
		unsigned short *p = dsp_buf;
#ifndef PZ_USE_THREADS
		GR_EVENT event;
#endif

		if (paused && !localpaused) {
			localpaused = 1;
			GrDestroyTimer(timer_id);
		}
		if (!paused && localpaused)
		{
			localpaused = 0;
			timer_id = GrCreateTimer(dsp_wid, 1000);
		}

		if (!paused) {
			rem = n = read(dsp_fd, (void *)p, DSP_REC_SIZE * (samplerate > 8000 ? 2 : 1));
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

	close(dsp_fd);

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
	dsp_do_draw(0);

	return NULL;
}

static void * dsp_playback(void *filename)
{
	int dsp_fd, file_fd;
	ssize_t count = 0;
	int samplerate;
	int channels;
	int localpaused = 0;

	file_fd = open((char *)filename, O_RDONLY);
	if (file_fd < 0) {
		char buf[256];
		sprintf(buf, "Cant open %s\n", (char *)filename);
		new_message_window(buf);
		goto no_file;
	}

	dsp_fd = open("/dev/dsp", O_WRONLY);
	if (dsp_fd < 0) {
		new_message_window("Cant open /dev/dsp");
		goto no_audio;
	}

	mixer_fd = open("/dev/mixer", O_RDWR);
	if (mixer_fd >= 0) {
		ioctl(mixer_fd, SOUND_MIXER_READ_PCM, &pcm_vol);
		pcm_vol = pcm_vol & 0xff;
	}

	/* assume a basic WAV header, jump to number channels */
	lseek(file_fd, 22, SEEK_SET);
	channels = read_16_le(file_fd);
	samplerate = read_32_le(file_fd);

	set_dsp_channels(dsp_fd, channels);
	set_dsp_rate(dsp_fd, samplerate);
	set_dsp_fmt(dsp_fd);

	killed = 0;
	playing = 1;
	paused = 0;

	dsp_do_draw(0);

	/* assume a basic WAV header, skip to start of PCM data */
	lseek(file_fd, 44, SEEK_SET);

	timer_id = GrCreateTimer(dsp_wid, 1000);
	draw_time();

	do {
		ssize_t n, rem;
		unsigned short *p = dsp_buf;
#ifndef PZ_USE_THREADS
		GR_EVENT event;
#endif

		if (paused && !localpaused) {
			localpaused = 1;
			GrDestroyTimer(timer_id);
		}
		if (!paused && localpaused)
		{
			localpaused = 0;
			timer_id = GrCreateTimer(dsp_wid, 1000);
		}

		if (!paused) {
			count = rem = n = read(file_fd, (void *)p, DSP_PLAY_SIZE * (samplerate > 8000 ? 2 : 1));
			if (n > 0) {
				while (rem) {
					n = write(dsp_fd, (void *)p, rem);
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
			if (event.type != GR_EVENT_TYPE_TIMEOUT)
			{
				pz_event_handler(&event);
				if (GrPeekEvent(&event)) goto repeat;
			}
		}
#endif
	} while (!killed && count > 0);

	if (mixer_fd >= 0) {
		close(mixer_fd);
		mixer_fd = -1;
	}

	close(dsp_fd);

	if (!localpaused) {
		GrDestroyTimer(timer_id);
	}

	if (!killed) {
		pz_close_window(dsp_wid);
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
	if (pthread_create(&dsp_thread, NULL, dsp_record, pcm_file) != 0) {
		new_message_window("could not create thread");
	}
#else
	dsp_record(pcm_file);
#endif
}

static void start_playback()
{
#ifdef PZ_USE_THREADS
	if (pthread_create(&dsp_thread, NULL, dsp_playback, pcm_file) != 0) {
		new_message_window("could not create thread");
	}
#else
	dsp_playback(pcm_file);
#endif
}

static void stop_dsp()
{
	killed = 1;
#ifdef PZ_USE_THREADS
	pthread_join(dsp_thread, NULL);
#endif
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
				stop_dsp();
				pz_close_window(dsp_wid);
			}
			else {
				if (mode == PLAYBACK) {
					start_playback();
				}
				else {
					start_recording();
				}
			}
			break;
	
		case 'm':
			if (playing || recording) {
				stop_dsp();
			}
			pz_close_window(dsp_wid);
			break;
	
		case '1':
		case 'd':
			if (playing || recording) {
				if (paused) {
					/* timer_id = GrCreateTimer(dsp_wid, 1000); draw_time(); */
					resume_dsp();
				}
				else {
					pause_dsp();
					/*GrDestroyTimer(timer_id); draw_time();*/
				}
			}
			dsp_do_draw(0);
			draw_time();
			break;
		case '3':
		case 'l':
			if (playing && mixer_fd >= 0 && pcm_vol > 0) {
				int vol;
				pcm_vol--;
				vol = pcm_vol << 8 | pcm_vol;
				ioctl(mixer_fd, SOUND_MIXER_WRITE_PCM, &vol);
			}
			break;
		case '2':
		case 'r':
			if (playing && mixer_fd >= 0 && pcm_vol < 100) {
				int vol;
				pcm_vol++;
				vol = pcm_vol << 8 | pcm_vol;
				ioctl(mixer_fd, SOUND_MIXER_WRITE_PCM, &vol);
			}
			break;
		}
		break;
	}

	return 1;
}
	

void new_record_window()
{
	char myfilename[128];
	time_t now;
	struct tm *tm;

	if (!hw_version || hw_version < 30000 || hw_version >= 40000 ) { // 3G only
		pz_error("Recording is unsupported on this hardware.");
		return;
	}

	time(&now);
	tm = localtime(&now);

	/* MMDDYYYY HHMMSS */
	sprintf(myfilename, "%s/%02d%02d%04d %02d%02d%02d.wav",
			RECORDINGS,
			tm->tm_mon+1, tm->tm_mday, tm->tm_year + 1900, tm->tm_hour,
			tm->tm_min, tm->tm_sec);
	
	strncpy(pcm_file, myfilename, sizeof(pcm_file) - 1);
	mode = RECORD;

	playing = 0;
	recording = 0;
	paused = 0;
	currenttime = 0;

	dsp_gc = pz_get_gc(1);
	GrSetGCUseBackground(dsp_gc, GR_TRUE);
	GrSetGCForeground(dsp_gc, BLACK);
	GrSetGCBackground(dsp_gc, WHITE);

	dsp_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), dsp_do_draw, dsp_do_keystroke);

	GrSelectEvents(dsp_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_TIMER);

	GrMapWindow(dsp_wid);
}

void new_record_mic_window()
{
	recording_src = SOUND_MASK_MIC;
	new_record_window();
}

void new_record_line_in_window()
{
	recording_src = SOUND_MASK_LINE;
	new_record_window();
}

void new_playback_window(char *filename)
{
	if (!hw_version || hw_version >= 40000) { // no playback > 3G
		pz_error("Audio playback is unsupported on this hardware.");
		return;
	}
	
	mode = PLAYBACK;
	strncpy(pcm_file, filename, sizeof(pcm_file) - 1);

	playing = 0;
	recording = 0;
	paused = 0;
	currenttime = 0;

	dsp_gc = pz_get_gc(1);
	GrSetGCUseBackground(dsp_gc, GR_TRUE);
	GrSetGCForeground(dsp_gc, BLACK);
	GrSetGCBackground(dsp_gc, WHITE);

	dsp_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), dsp_do_draw, dsp_do_keystroke);

	GrSelectEvents(dsp_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_TIMER);

	GrMapWindow(dsp_wid);

	dsp_do_draw(0);

	start_playback();
}

void new_playback_browse_window(void)
{
	new_browser_window(RECORDINGS);
}
#endif
