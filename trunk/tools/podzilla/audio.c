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
#include <string.h>
#include <time.h>
#include <byteswap.h>
#include "pz.h"
#include "ipod.h"

static GR_WINDOW_ID dsp_wid;
static GR_GC_ID dsp_gc;
static GR_SCREEN_INFO screen_info;
static pthread_t dsp_thread;
static int mode;
static volatile int playing = 0, recording = 0, paused = 0;
static char *pcm_file;

#define RECORD        0
#define PLAYBACK      1

static volatile int killed;

#define DSP_REC_SIZE	512
#define DSP_PLAY_SIZE	16*1024*2

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
	write(file_fd, &value, 4);
}

int write_16_le(int file_fd, unsigned short value)
{
#ifdef IS_BIG_ENDIAN
	value = bswap_16(value);
#endif
	write(file_fd, &value, 2);
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

static void set_dsp_rate(int fd, int rate)
{
	/* sample rate */
	ioctl(fd, SNDCTL_DSP_SPEED, &rate);
}

static void set_dsp_channels(int fd, int channels)
{
	/* set mono or stereo */
	ioctl(fd,SNDCTL_DSP_CHANNELS, &channels);
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
	int samplerate;
	int channels = 1;
	int start_pos;
	int filelength;

	file_fd = open((char *)filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
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

	samplerate = ipod_get_setting(DSPFREQUENCY);
	if (samplerate == 0) {
		samplerate = 44100;
	}

	set_dsp_rate(dsp_fd, samplerate);
	set_dsp_channels(dsp_fd, channels);
	killed = 0;
	recording = 1;
	paused = 0;

	write_wav_header(file_fd, samplerate, channels);

	/* start of data chunk */
	start_pos = (int)lseek(file_fd, 0, SEEK_CUR);
	write(file_fd, "data", 4);
	write_32_le(file_fd, 0);	// dummy length value

	while (!killed) {
		ssize_t n, rem;
		unsigned short *p = dsp_buf;

		rem = n = read(dsp_fd, (void *)p, DSP_REC_SIZE);
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

	/* get file length by position which is 1 more than last */
	filelength = (int)lseek(file_fd, 1, SEEK_CUR) - 1;

	/* correct length value for the data chunk */
	lseek(file_fd, start_pos + 4, SEEK_SET);
	write_32_le(file_fd, filelength - (start_pos + 8));

	/* correct length value for the RIFF header */
	lseek(file_fd, 4, SEEK_SET);
	write_32_le(file_fd, filelength - 8);

	close(dsp_fd);

no_audio:
	close(file_fd);

no_file:
	recording = 0;
	free(pcm_file);
	pz_close_window(dsp_wid);

	return NULL;
}

static void * dsp_playback(void *filename)
{
	int dsp_fd, file_fd;
	ssize_t count = 0;
	int samplerate;
	int channels;

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

	/* assume a basic WAV header, jump to number channels */
	lseek(file_fd, 22, SEEK_SET);
	channels = read_16_le(file_fd);
	samplerate = read_32_le(file_fd);

	set_dsp_rate(dsp_fd, samplerate);
	set_dsp_channels(dsp_fd, channels);

	killed = 0;
	playing = 1;
	paused = 0;

	/* assume a basic WAV header, skip to start of PCM data */
	lseek(file_fd, 22, SEEK_SET);

	do {
		ssize_t n, rem;
		unsigned short *p = dsp_buf;

		count = rem = n = read(file_fd, (void *)p, DSP_PLAY_SIZE);
		if (n > 0) {
			while (rem) {
				n = write(dsp_fd, (void *)p, rem);
				if (n > 0) {
					rem -= n;
					p += (n/2);
				}
			}
		}
	} while (!killed && count > 0);

	close(dsp_fd);

no_audio:
	close(file_fd);

no_file:

	playing = 0;
	free(pcm_file);
	pz_close_window(dsp_wid);
	return NULL;
}

static void start_recording()
{
	if (pthread_create(&dsp_thread, NULL, dsp_record, pcm_file) != 0) {
		new_message_window("could not create thread");
	}
}

static void start_playback()
{
	if (pthread_create(&dsp_thread, NULL, dsp_playback, pcm_file) != 0) {
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
		if (playing || recording) {
			stop_dsp();
		}
		else if (mode == PLAYBACK) {
			start_playback();
		}
		else {
			start_recording();
		}
		dsp_do_draw(0);
		break;

	case 'm':
		if (playing || recording) {
			stop_dsp();
		}
		else {
			free(pcm_file);
			pz_close_window(dsp_wid);
		}
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

void record_set_8()
{
	ipod_set_setting(DSPFREQUENCY, 8000);
}

void record_set_32()
{
	ipod_set_setting(DSPFREQUENCY, 32000);
}

void record_set_44()
{
	ipod_set_setting(DSPFREQUENCY, 44100);
}

void record_set_88()
{
	ipod_set_setting(DSPFREQUENCY, 88200);
}

void record_set_96()
{
	ipod_set_setting(DSPFREQUENCY, 96000);
}

void new_record_window(char *filename)
{
	char myfilename[128];
	time_t now;
	struct tm *tm;

	time(&now);
	tm = localtime(&now);

	/* MMDDYYYY HHMMSS */
	sprintf(myfilename, "Recordings/%02d%02d%04d %02d%02d%02d.wav",
		tm->tm_mon+1, tm->tm_mday, tm->tm_year + 1900, tm->tm_hour,
		tm->tm_min, tm->tm_sec);

	pcm_file = strdup(myfilename);
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
	mode = PLAYBACK;
	pcm_file = strdup(filename);

	dsp_gc = GrNewGC();
	GrSetGCUseBackground(dsp_gc, GR_TRUE);
	GrSetGCForeground(dsp_gc, WHITE);
	GrGetScreenInfo(&screen_info);

	dsp_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), dsp_do_draw, dsp_do_keystroke);

	GrSelectEvents(dsp_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(dsp_wid);
}

void new_playback_browse_window(void)
{
	new_browser_window("Recordings");
}

