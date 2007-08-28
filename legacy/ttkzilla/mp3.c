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

#if 1
#include "pz.h"
void new_mp3_window(char *filename, char *album, char *artist, char *title, int len)
{
    pz_error ("MP3 playback not supported in this build");
}
int is_mp3_type (char *ext) 
{ return strcasecmp (ext, ".mp3") == 0; }
#else


#ifdef IPOD
#define USE_LIBINTEL
#else
#define USE_LIBMAD
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef USE_LIBINTEL
#include <ippdefs.h>
#include <ippAC.h>
#include "mp3decoder.h"
#endif

#include "pz.h"
#include "oss.h"

/* draw_time calls to wait before redrawing when adjusting volume */
#define RECT_CYCLES 2


static GR_WINDOW_ID mp3_wid;
static GR_GC_ID mp3_gc;
static long remaining_time;
static long total_time;
static long next_song_time;
static char current_album[128];
static char current_artist[128];
static char current_title[128];
static char current_pos[20];
static char next_song[128];
static int next_song_queued;
static int vol_delta;
static int decoding_finished;
static int window_open = 0;	/* is the mp3 window open? */
static int mp3_pause;
static int rect_x1;
static int rect_x2;
static int rect_y1;
static int rect_y2;
static int rect_wait;
static dsp_st dspz;

/*variables from playlist.c*/
extern int playlistpos;
extern int playlistlength;
extern void play_prev_track();
extern void play_next_track();

static void start_mp3_playback(char *filename);

int is_mp3_type(char *extension)
{
	return strcmp(extension, ".mp3") == 0;
}

static void draw_bar(int bar_length)
{
	if (!(bar_length < 0 || bar_length > rect_x2-rect_x1-4) ) {
		GrFillRect (mp3_wid, mp3_gc, rect_x1 + 2, rect_y1+2, bar_length, rect_y2-rect_y1-4);
		GrSetGCForeground(mp3_gc, WHITE);
		GrFillRect(mp3_wid, mp3_gc, rect_x1 + 2 + bar_length, rect_y1+2, rect_x2-rect_x1-4 - bar_length, rect_y2-rect_y1-4);
		GrSetGCForeground(mp3_gc, BLACK);
	}
}

static void draw_time()
{
	int bar_length, elapsed;
	char buf[256];
	struct tm *tm;
	time_t tot_time;

	elapsed= total_time - remaining_time;
	bar_length= (elapsed * (rect_x2-rect_x1-4)) / total_time;

	GrSetGCForeground(mp3_gc, WHITE);
	GrFillRect(mp3_wid, mp3_gc, rect_x1, rect_y1-12, rect_x2-rect_x1+1, 10);
	GrSetGCForeground(mp3_gc, BLACK);

	GrText(mp3_wid, mp3_gc, rect_x1, rect_y1-2, "0", -1, GR_TFASCII);
	tot_time = total_time / 1000;
	tm = gmtime(&tot_time);
	sprintf(buf, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
	GrText(mp3_wid, mp3_gc, rect_x2-43, rect_y1-2, buf, -1, GR_TFASCII);
	GrText(mp3_wid, mp3_gc, 50, rect_y1-2, "time", -1, GR_TFASCII);
	draw_bar(bar_length);
}

static void draw_volume()
{
	int vol;
	int bar_length;

	vol = dsp_get_volume(&dspz);
	bar_length = (vol * (rect_x2-rect_x1-4)) / 100;

	GrSetGCForeground(mp3_gc, WHITE);
	GrFillRect(mp3_wid, mp3_gc, rect_x1, rect_y1-12, rect_x2-rect_x1+1, 10);
	GrSetGCForeground(mp3_gc, BLACK);

	GrText(mp3_wid, mp3_gc, rect_x1, rect_y1-2, "0", -1, GR_TFASCII);
	GrText(mp3_wid, mp3_gc, rect_x2-20, rect_y1-2, "100", -1, GR_TFASCII);
	GrText(mp3_wid, mp3_gc, 50, rect_y1-2, "volume", -1, GR_TFASCII);
	draw_bar(bar_length);
}

static void mp3_do_draw()
{
	GrSetGCForeground(mp3_gc, WHITE);
	GrFillRect(mp3_wid, mp3_gc, 0, 0, screen_info.cols, screen_info.rows);
	GrSetGCForeground(mp3_gc, BLACK);

	GrText(mp3_wid, mp3_gc, 8, 20, current_pos, -1, GR_TFASCII);
	GrText(mp3_wid, mp3_gc, 8, 34, current_title, -1, GR_TFASCII);
	GrText(mp3_wid, mp3_gc, 8, 48, current_artist, -1, GR_TFASCII);
	GrText(mp3_wid, mp3_gc, 8, 62, current_album, -1, GR_TFASCII);
	rect_x1 = 8;
	rect_x2 = screen_info.cols - 8;
	rect_y1 =  screen_info.rows - (HEADER_TOPLINE + 1) - 18;
	rect_y2 =  screen_info.rows - (HEADER_TOPLINE + 1) - 8;
	GrRect(mp3_wid, mp3_gc, rect_x1, rect_y1, rect_x2-rect_x1, rect_y2-rect_y1);

	rect_wait = 0;
	draw_time();
}


static void mp3_refresh_state()
{
	dsp_vol_change(&dspz, vol_delta);
	rect_wait = RECT_CYCLES;
	draw_volume();
}

static int mp3_do_keystroke(GR_EVENT * event)
{
	switch (event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			break;
		case 'm':
			decoding_finished = 1;
			break;
		case '4':
		case 'f':
			play_next_track();
			break;
		case '5':
		case 'w':
			play_prev_track();
			break;
		case '1':
		case 'd':
			mp3_pause = !mp3_pause;
			if (mp3_pause) {
				pz_draw_header(_("MP3 Playback - ||"));
			}
			else {
				pz_draw_header(_("MP3 Playback"));
			}
			break;
		case '3':
		case 'l':
			vol_delta--;
			break;
		case '2':
		case 'r':
			vol_delta++;
			break;
		}
		break;
	}
	return 1;
}

static int audiobufpos, audiobuf_len;
static char *audiobuf;

static void mp3_event_handler()
{
	GR_EVENT event;
	int evtcap = 200;

	vol_delta = 0;
	while (GrPeekEvent(&event) && evtcap--)
	{
		
		GrGetNextEventTimeout(&event, 1000);
		if (event.type != GR_EVENT_TYPE_TIMEOUT) {
			pz_event_handler(&event);
		} else {
			break;
		}
	}
	
	if (vol_delta != 0) {
		mp3_refresh_state();		
	}
}

#ifdef USE_LIBMAD
static void decode_mp3()
{
	while (!decoding_finished) {
		mp3_event_handler();
	}

	pz_close_window(mp3_wid);
}
#endif

#ifdef USE_LIBINTEL
static void init_dsp(int channels, MP3DecoderState *ds)
{
	int fs[3] = { 44100, 48000, 32000 };
	IppMP3FrameHeader h = ds->FrameHdr;

#ifdef IPOD
	dsp_setup(&dspz, channels, fs[h.samplingFreq]);
#endif
}

static int abread(void * ptr, size_t size, size_t nmemb)
{
	int copylen = size * nmemb;
	if (copylen + audiobufpos > audiobuf_len) {
		copylen = audiobuf_len - audiobufpos;
	}

	memcpy(ptr, audiobuf + audiobufpos, copylen);
	audiobufpos += copylen;

	return copylen;
}

static int FillMP3BitStream(MP3BitStream *bs)
{
	int Mask;	/* Modulo mask to force end wrap-around for ring buffer */
	int ByteCount;	/* Byte counter for block reads from file */
	int n;		/* Secondary byte counter for block reads */
	Ipp8u *StreamBuf;	/* Ring buffer base pointer */

	/* Set modulo counting mask given ring buffer length */
	Mask = bs->Len-1;

	/* Point to ring buffer */
	StreamBuf = bs->Stream;

	/* Block fill ring buffer from file device using 1 of 3 cases:
	   a) head>tail, b) head<tail, c) head==tail */
	if (bs->Head < bs->Tail)
	{
		ByteCount = abread(&(StreamBuf[((bs->Tail)+1)&Mask]), sizeof(Ipp8u), STREAM_BUF_SIZE-1-(bs->Tail));

		if ((ByteCount == (STREAM_BUF_SIZE-1-(bs->Tail)))&&(bs->Head > 0))
		{
			n = abread(&(StreamBuf[0]), sizeof(Ipp8u), bs->Head);
			bs->Tail = (n-1);
			ByteCount += n;
		}
		else
		{
			bs->Tail = bs->Tail + ByteCount;
		}
	}
	else if (bs->Head>bs->Tail)
	{
		ByteCount = abread(&(StreamBuf[((bs->Tail)+1)&Mask]), sizeof(Ipp8u), (bs->Head)-1-(bs->Tail));
		bs->Tail = bs->Tail + ByteCount;
	}
	else  /* Head==Tail, i.e., ring buffer empty */
	{
		ByteCount = abread(&(StreamBuf[0]), sizeof(Ipp8u), STREAM_BUF_SIZE);
		bs->Tail = ByteCount - 1;
		bs->Head = 0;
	}

	return ByteCount;
}

static void RenderSound(Ipp16s *pcm, MP3DecoderState *ds)
{
	int channels = ds->Channels;
	int len = ds->pcmLen;

	if (channels == 1) {
		int i;

		pz_draw_header(_("MP3 Playback - Mono!"));
		for (i = 0; i < len; i += MAX_CHAN)
			pcm[i+1] = pcm[i];
	}

	dsp_write(&dspz, pcm, sizeof(Ipp16s) * len);
}

static MP3BitStream bs;
static MP3DecoderState DecoderState;
static Ipp16s pcm[MAX_CHAN*MAX_GRAN*IPP_MP3_GRANULE_LEN];

static void decode_mp3()
{
	int dsp_initialised = 0;
	int nframes = 0;

	InitMP3Decoder(&DecoderState, &bs);

	mp3_pause = 0;
	next_song_queued = 0;
	decoding_finished = 0;
	while (!decoding_finished) {
		int refill = 0;

		mp3_event_handler();

		if (mp3_pause) {
			continue;
		}

		if (next_song_queued) {
			return;
		}
		
		switch (DecodeMP3Frame(&bs, pcm, &DecoderState)) {
		case MP3_FRAME_COMPLETE:
			if (!dsp_initialised) {
				init_dsp(MAX_CHAN, &DecoderState);
				dsp_initialised = 1;
			}
			RenderSound(pcm, &DecoderState);
			remaining_time -= 26;
			nframes++;
			if (nframes % 40 == 0 && rect_wait-- <= 0) {
				draw_time();
			}
			refill = ((bs.Head>=bs.Tail) && (STREAM_BUF_SIZE-bs.Head+bs.Tail < FIFO_THRESH)) || ((bs.Tail >= bs.Head) && (bs.Tail-bs.Head < FIFO_THRESH));
			if (refill) {
				decoding_finished = FillMP3BitStream(&bs) == 0;
			}
			break;
		case MP3_BUFFER_UNDERRUN:
		case MP3_SYNC_NOT_FOUND:
			decoding_finished = FillMP3BitStream(&bs) == 0;
			break;
		case MP3_FRAME_UNDERRUN:
			break;
		case MP3_FRAME_HEADER_INVALID:
			break;
		}
	}

	/* did song finished by itself or was Menu pressed? */
	if (FillMP3BitStream(&bs) == 0)
	{
		play_next_track();
	}
}
#endif

static void start_mp3_playback(char *filename)
{
	FILE *file;
	int ret;

	ret = dsp_open(&dspz, DSP_LINEOUT);
	if (ret < 0) {
		pz_perror("/dev/dsp");
		pz_close_window(mp3_wid);
		return;
	}

	do {
		pz_draw_header(_("Buffering..."));
		total_time = remaining_time = next_song_time;
		mp3_do_draw(0);

		file = fopen(filename, "r");
		if (file == 0) {
			pz_close_window(mp3_wid);
			pz_perror(filename);
			dsp_close(&dspz);
			return;
		}

		fseek(file, 0, SEEK_END);
		audiobuf_len = ftell(file);
		audiobufpos = 0;
		audiobuf = malloc(audiobuf_len);
		if (audiobuf == 0) {
			pz_close_window(mp3_wid);
			dsp_close(&dspz);
			fclose(file);

			new_message_window(_("malloc failed"));
			return;
		}

		fseek(file, 0, SEEK_SET);
		fread(audiobuf, audiobuf_len, 1, file);
		fclose(file);

		pz_draw_header(_("MP3 Playback"));

		decode_mp3();

		free(audiobuf);

		filename = next_song;
	}
	while (next_song_queued);

	dsp_close(&dspz);
	pz_close_window(mp3_wid);
}

void new_mp3_window(char *filename, char *album, char *artist, char *title, int len)
{
	strncpy(current_album, album, sizeof(current_album)-1);
	current_album[sizeof(current_album)-1] = 0;

	strncpy(current_artist, artist, sizeof(current_artist)-1);
	current_artist[sizeof(current_artist)-1] = 0;

	strncpy(current_title, title, sizeof(current_title)-1);
	current_title[sizeof(current_title)-1] = 0;

	sprintf(current_pos, _("Song %d of %d"), playlistpos, playlistlength);

	next_song_time = len;
	
	/* play another song when one isn't complete */
	if (window_open)
	{
	    strcpy(next_song, filename);
	    next_song_queued = 1;
	    return;
	}

	window_open = 1;

	mp3_gc = pz_get_gc(1);
	GrSetGCUseBackground(mp3_gc, GR_TRUE);
	GrSetGCBackground(mp3_gc, WHITE);
	GrSetGCForeground(mp3_gc, BLACK);

	mp3_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), mp3_do_draw, mp3_do_keystroke);

	GrSelectEvents(mp3_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_TIMER);

	GrMapWindow(mp3_wid);
	total_time = remaining_time = len;
	mp3_do_draw(0);

	start_mp3_playback(filename);
	window_open = 0;
}

#endif
