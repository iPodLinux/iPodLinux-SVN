/*
 * Copyright (C) 2005 Alastair Stuart
 * 
 * based on mp3.c
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

#ifdef USE_HELIXAACDEC
#include <aacdec.h>
#include <mp4ff.h>
#endif /* USE_HELIXAACDEC */

#include "pz.h"
#include "oss.h"

/* draw_time calls to wait before redrawing when adjusting volume */
#define RECT_CYCLES 2

#ifdef USE_HELIXAACDEC
static GR_WINDOW_ID aac_wid;
static GR_GC_ID aac_gc;
static GR_WINDOW_ID aac_key_timer;
static long remaining_time;
static long total_time;
static char current_album[128];
static char current_artist[128];
static char current_title[128];
static char current_pos[20];
static char next_song[128];
static long next_song_time;
static int next_song_queued;
static int window_open = 0;	/* is the mp3 window open? */
static int vol_delta;
static int get_out;
static int aac_pause;
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
#endif /* USE_HELIXAACDEC */

int is_aac_type(char *extension)
{
	return strcasecmp(extension, ".m4a") == 0
		|| strcasecmp(extension, ".m4b") == 0;
}

#ifdef USE_HELIXAACDEC
static void draw_bar(int bar_length)
{
	if (!(bar_length < 0 || bar_length > rect_x2-rect_x1-4) ) {
		GrFillRect (aac_wid, aac_gc, rect_x1 + 2, rect_y1+2, bar_length, rect_y2-rect_y1-4);
		GrSetGCForeground(aac_gc, WHITE);
		GrFillRect(aac_wid, aac_gc, rect_x1 + 2 + bar_length, rect_y1+2, rect_x2-rect_x1-4 - bar_length, rect_y2-rect_y1-4);
		GrSetGCForeground(aac_gc, BLACK);
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

	GrSetGCForeground(aac_gc, WHITE);
	GrFillRect(aac_wid, aac_gc, rect_x1, rect_y1-12, rect_x2-rect_x1+1, 10);
	GrSetGCForeground(aac_gc, BLACK);

	tot_time = total_time / 1000;
	tm = gmtime(&tot_time);
	sprintf(buf, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
	GrText(aac_wid, aac_gc, rect_x2-43, rect_y1-2, buf, -1, GR_TFASCII);
	GrText(aac_wid, aac_gc, 50, rect_y1-2, "time", -1, GR_TFASCII);
	tot_time = elapsed / 1000;
	tm = gmtime(&tot_time);
	sprintf(buf, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
	GrText(aac_wid, aac_gc, rect_x1, rect_y1-2, buf, -1, GR_TFASCII);
	draw_bar(bar_length);
}

static void draw_volume()
{
	int vol;
	int bar_length;

	vol = dsp_get_volume(&dspz);
	bar_length = (vol * (rect_x2-rect_x1-4)) / 100;

	GrSetGCForeground(aac_gc, WHITE);
	GrFillRect(aac_wid, aac_gc, rect_x1, rect_y1-12, rect_x2-rect_x1+1, 10);
	GrSetGCForeground(aac_gc, BLACK);

	GrText(aac_wid, aac_gc, rect_x1, rect_y1-2, "0", -1, GR_TFASCII);
	GrText(aac_wid, aac_gc, rect_x2-20, rect_y1-2, "100", -1, GR_TFASCII);
	GrText(aac_wid, aac_gc, 50, rect_y1-2, "volume", -1, GR_TFASCII);
	draw_bar(bar_length);
}

static void aac_do_draw()
{
	GrSetGCForeground(aac_gc, WHITE);
	GrFillRect(aac_wid, aac_gc, 0, 0, screen_info.cols, screen_info.rows);
	GrSetGCForeground(aac_gc, BLACK);

	GrText(aac_wid, aac_gc, 8, 20, current_pos, -1, GR_TFASCII);
	GrText(aac_wid, aac_gc, 8, 34, current_title, -1, GR_TFASCII);
	GrText(aac_wid, aac_gc, 8, 48, current_artist, -1, GR_TFASCII);
	GrText(aac_wid, aac_gc, 8, 62, current_album, -1, GR_TFASCII);
	rect_x1 = 8;
	rect_x2 = screen_info.cols - 8;
	rect_y1 =  screen_info.rows - (HEADER_TOPLINE + 1) - 18;
	rect_y2 =  screen_info.rows - (HEADER_TOPLINE + 1) - 8;
	GrRect(aac_wid, aac_gc, rect_x1, rect_y1, rect_x2-rect_x1, rect_y2-rect_y1);

	rect_wait = 0;
	draw_time();
}


static void aac_refresh_state()
{
	dsp_vol_change(&dspz, vol_delta);
	rect_wait = RECT_CYCLES;
	draw_volume();
}

static int scrub = 0;
static int scrub_direction = 0;

static int aac_do_keystroke(GR_EVENT * event)
{
	switch (event->type) {
	case GR_EVENT_TYPE_TIMER:
		if(((GR_EVENT_TIMER *)event)->tid == aac_key_timer) {
			GrDestroyTimer(aac_key_timer);
			aac_key_timer = 0;
			if (scrub_direction > 0)
				scrub = 10;
			else if (scrub_direction < 0)
				scrub = -10;
			printf("timer event: scrub = %d\n", scrub);
		}
		break;
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			break;
		case 'm':
			get_out = 1;
			break;
		case '4':
		case 'f':
			scrub_direction = 1;
			aac_key_timer = GrCreateTimer(aac_wid, 500);
			break;
		case '5':
		case 'w':
			scrub_direction = -1;
			aac_key_timer = GrCreateTimer(aac_wid, 500);
			break;
		case '1':
		case 'd':
			aac_pause = !aac_pause;
			if (aac_pause) {
				pz_draw_header("AAC Playback - ||");
			}
			else {
				pz_draw_header("AAC Playback");
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
	case GR_EVENT_TYPE_KEY_UP:
		switch (event->keystroke.ch) {
		case '4':
		case 'f':
		case '5':
		case 'w':
			if (aac_key_timer) {
				GrDestroyTimer(aac_key_timer);
				aac_key_timer = 0;
				if (scrub_direction > 0)
					get_out = 2;
				else if (scrub_direction < 0)
					get_out = 3;
			} else {
				scrub = 0;
			}
			scrub_direction = 0;
		}
		break;
	}
	return 1;
}

static void aac_event_handler()
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
		aac_refresh_state();		
	}
}

int find_first_aac_track(mp4ff_t *infile)
{
	int i, trackType;
	int numTracks = mp4ff_total_tracks(infile);

	for (i = 0; i < numTracks; i++)
	{
		trackType = mp4ff_get_track_type(infile, 0);
				
		if (trackType < 0)
			continue;
		if (trackType >= 1 && trackType <= 4) /* first aac track */
			return i;
	}

	/* no aac track */
	return -1;
}

uint32_t read_callback(void *user_data, void *buffer, uint32_t length)
{
	return fread(buffer, 1, length, (FILE*)user_data);
}

uint32_t seek_callback(void *user_data, uint64_t position)
{
	return fseek((FILE*)user_data, position, SEEK_SET);
}

static int sample_rates[] = 
{
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 
	16000, 12000, 11025, 8000,  7350,  0,     0,     0
};

static int audiobuf_len;
static unsigned char *audiobuf;
static unsigned short *samplez;
static int sample_len;
static AACFrameInfo aacFrameInfo;

int mp4_to_raw_aac(FILE *infile)
{
	unsigned char *aacbuf;
	int aacbuf_len;
	mp4ff_t *mp4file;
	mp4ff_callback_t *mp4cb;
	int track, numSamples, sampleId;
	unsigned short sampRateIndex;
	long audiobufpos;
	
	fseek(infile, 0, SEEK_END);
	audiobuf_len = ftell(infile);
	audiobuf = malloc(audiobuf_len); /* more than we need */
	if (audiobuf == NULL) {
		return -1;
	}
	fseek(infile, 0, SEEK_SET);
	
	mp4cb = malloc(sizeof(mp4ff_callback_t));
	if (mp4cb == NULL) {
		free(audiobuf);
		return -1;
	}
	mp4cb->read = read_callback;
	mp4cb->seek = seek_callback;
	mp4cb->user_data = infile;
	
	mp4file = mp4ff_open_read(mp4cb);
	if (!mp4file) {
		free(mp4cb);
		free(audiobuf);
		return -1;
	}

	if ((track = find_first_aac_track(mp4file)) < 0) {
		free(mp4cb);
		free(audiobuf);
		mp4ff_close(mp4file);
		return -2;
	}

	aacbuf = NULL;
	aacbuf_len = 0;
	
	mp4ff_get_decoder_config(mp4file, track, &aacbuf,
			(uint32_t *)&aacbuf_len);
	
/* decoder config layout:
 *
 * [.....|...  .|....|.|.|.]
 *    1     2      3  4 5 6
 * 1: AudioObjectType         (5 bits)
 *     - profile: 0 = main, 1 = LC, 2 = SSR, 3 = reserved 
 * 2: samplingFrequencyIndex  (4 bits)
 *		- see sample_rates[]
 *    if (samplingFrequencyIndex == 0xf)
 *       samplingFrequency    (24 bits)
 * 3: channelConfiguration    (4 bits)
 * 4: FrameLengthFlag         (1 bit) 1024 or 960
 * 5: DependsOnCoreCoder      (1 bit) - always 0
 * 6: ExtensionFlag           (1 bit) - always 0
 */
 
	if (aacbuf_len == 2) {
#if 0
		printf("aacbuf: 0x%x 0x%x\n", aacbuf[0], aacbuf[1]);
		printf("decoder config: %d\n", aacbuf_len);
		printf("  AudioObjectType: 0x%x\n", aacbuf[0] >> 4);
		printf("  samplingFrequencyIndex: 0x%d\n",
			 ((aacbuf[0] << 1) & 0xe) | ((aacbuf[1] >> 7) & 0x1));
		printf("  channelConfiguration: 0x%x\n", ((aacbuf[1]) >> 3) & 0xf);
#endif
		aacFrameInfo.profile = aacbuf[0] >> 4;
		sampRateIndex = ((aacbuf[0] << 1) & 0xe) | ((aacbuf[1] >> 7) & 0x1);
		if (sampRateIndex < 0xf) {
			aacFrameInfo.sampRateCore = sample_rates[sampRateIndex];
			aacFrameInfo.nChans = ((aacbuf[1]) >> 3) & 0xf;
		} else {
			aacFrameInfo.sampRateCore = 44100; /* default */
			aacFrameInfo.nChans = 2; /* default */
		}
	} else {
		if (aacbuf != NULL)
			free(aacbuf);
		free(mp4cb);
		free(audiobuf);
		mp4ff_close(mp4file);
		return -4;
	}
	
	free(aacbuf);

	numSamples = mp4ff_num_samples(mp4file, track);
	samplez = malloc(sizeof(unsigned short) * numSamples);
	if (samplez == NULL) {
		pz_draw_header("samplez malloc failed"); sleep(1);

		if (aacbuf != NULL) {
			free(aacbuf);
		}

		free(mp4cb);
		free(audiobuf);
		mp4ff_close(mp4file);

		return -1;
	}

	sample_len = (int) (mp4ff_get_track_duration(mp4file, 0) /
	                    mp4ff_num_samples(mp4file, 0)) 
	                    / (mp4ff_time_scale(mp4file, 0) / 1000);
	
	aacbuf = NULL;
	aacbuf_len = 0;
	audiobufpos = 0;
	
	for (sampleId = 0; sampleId < numSamples; sampleId++) {
		int rc;
		rc = mp4ff_read_sample(mp4file, track, sampleId, &aacbuf,
				(uint32_t *)&aacbuf_len);
		if (rc == 0) {
			if (aacbuf)
				free(aacbuf);
			free(mp4cb);
			free(audiobuf);
			mp4ff_close(mp4file);
			return -4;
		} else {
			memcpy (audiobuf + audiobufpos, aacbuf, aacbuf_len);
			audiobufpos += aacbuf_len;
			samplez[sampleId] = aacbuf_len;
			
			if (aacbuf)
				free(aacbuf);
		}
	}

	free(mp4cb);
	mp4ff_close(mp4file);

	return numSamples;
}

int decode_raw_aac(int numSamples)
{
	HAACDecoder *hAACDecoder;
	int newbuf_len, err, sampleId, stop;
	int dsp_initialised = 0, timestep, hOB, count, scrub_cnt;
	long audiobufpos;
	unsigned char *newbuf;
	short outBuf[AAC_MAX_NCHANS * AAC_MAX_NSAMPS];
	
	hAACDecoder = (HAACDecoder *)AACInitDecoder();
	if (!hAACDecoder) {
		return -1337;
	}
	
	AACSetRawBlockParams(hAACDecoder, 0, &aacFrameInfo);
	
	hOB = sizeof(outBuf) / 2;
	
	audiobufpos=0;
	err = 0;
	count = 0;
	
	next_song_queued = 0;
	
	timestep = 1000 / sample_len;
	
	for (sampleId = 0; sampleId < numSamples; sampleId++)
	{
		aac_event_handler();

		if (get_out || next_song_queued)
			break;

		if (aac_pause) {
			sampleId--;
			continue;
		}

		if (rect_wait-- <= 0 && count == timestep) {
				draw_time();
				count = 0;
		}

		count++;
		
		stop = 0;

		if (scrub < 0) {
			if (sampleId - 1 > 0 && scrub_cnt < 0) {
				scrub_cnt++;
				audiobufpos -= samplez[sampleId - 1];
				sampleId -= 2;
				remaining_time += sample_len;
				stop = 1;
			} else {
				scrub_cnt = scrub;
			}
		}
		if (stop)
			continue;

		newbuf = NULL;
		newbuf_len = 0;
		
		newbuf = audiobuf + audiobufpos;
		audiobufpos += samplez[sampleId];
		newbuf_len = samplez[sampleId];
		
		remaining_time -= sample_len;
		
		if (scrub > 0) {
			if (sampleId + 1< numSamples && scrub_cnt > 0) {
				scrub_cnt--;
				continue;
			} else {
				scrub_cnt = scrub;
			}
		}

		/* decode one AAC frame */

		err = AACDecode(hAACDecoder, &newbuf, &newbuf_len, outBuf);

		if (err == -1) {
			printf("AACDecode: Buffer Underun on sample %d", sampleId);
			fflush(stdout);
			continue;
		}
		
		if (err) {
			break;
		}
		
		/* no error */
		AACGetLastFrameInfo(hAACDecoder, &aacFrameInfo);

		if (!dsp_initialised) {
			dsp_setup(&dspz, aacFrameInfo.nChans,
					aacFrameInfo.sampRateOut);
			dsp_initialised = 1;
		}

		if (aacFrameInfo.nChans == 1) {
			int i;
			for (i = 0; i < hOB; i += AAC_MAX_NCHANS)
				outBuf[i+1] = outBuf[i];
		}
		dsp_write(&dspz, outBuf, (aacFrameInfo.bitsPerSample >> 3) *
		      aacFrameInfo.outputSamps);
	}

	AACFreeDecoder(hAACDecoder);
	
	free(audiobuf);
	free(samplez);
	
	scrub = 0;
	scrub_direction = 0;
	
	if (err != ERR_AAC_NONE)
		return err;
	
	return 0;
}

static void start_aac_playback(char *filename)
{
	FILE *file;
	int raw_samples, dec, ret;

	ret = dsp_open(&dspz, DSP_LINEOUT);
	if (ret < 0) {
		pz_perror("/dev/dsp");
		pz_close_window(aac_wid);
		return;
	}

	get_out = 0;

	do {
		pz_draw_header("Buffering...");
		total_time = remaining_time = next_song_time;
		aac_do_draw(0);
	
		file = fopen(filename, "r");
		if (file == 0) {
			pz_close_window(aac_wid);
			pz_perror(filename);
			dsp_close(&dspz);
			return;
		}
	
	
		raw_samples = mp4_to_raw_aac(file);
	
		fclose(file);
	
		if (raw_samples < 0) {
			pz_close_window(aac_wid);
			dsp_close(&dspz);
			pz_error("error demuxing stream");
			if (audiobuf)
				free(audiobuf);
			if (samplez)
				free(samplez);
			return;
		}
	
		pz_draw_header("AAC Playback");
	
		dec = decode_raw_aac(raw_samples);
		
		if (dec < 0) {
			dsp_close(&dspz);
			pz_close_window(aac_wid);
			pz_error("error decoding stream: %d", dec);
			return;
		} else if (get_out == 1) { /* menu pressed */
			break;
		} else if (get_out == 3) { /* prev */
			play_prev_track();
		} else {                   /* next or song finished */
			play_next_track();
		}
		
		get_out = 0;
		
		filename = next_song;
	}
	while (next_song_queued);
		
	dsp_close(&dspz);
	pz_close_window(aac_wid);
}
#endif /* USE_HELIXAACDEC */

void new_aac_window(char *filename, char *album, char *artist, char *title, long len)
{
#ifdef USE_HELIXAACDEC
	if (album) {
		strncpy(current_album, album, sizeof(current_album)-1);
		current_album[sizeof(current_album)-1] = 0;
	} else {
		current_album[0] = 0;
	}

	if (artist) {
		strncpy(current_artist, artist, sizeof(current_artist)-1);
		current_artist[sizeof(current_artist)-1] = 0;
	} else {
		current_artist[0] = 0;
	}

	if (title) {
		strncpy(current_title, title, sizeof(current_title)-1);
		current_title[sizeof(current_title)-1] = 0;
	} else {
		current_title[0] = 0;
	}

	if (playlistlength) {
		sprintf(current_pos, "Song %d of %d",
		        playlistpos, playlistlength);
	}

	next_song_time = len;
	
	/* play another song when one isn't complete */
	if (window_open)
	{
	    strcpy(next_song, filename);
	    next_song_queued = 1;
	    return;
	}

	window_open = 1;

	aac_gc = pz_get_gc(1);
	GrSetGCUseBackground(aac_gc, GR_TRUE);
	GrSetGCBackground(aac_gc, WHITE);
	GrSetGCForeground(aac_gc, BLACK);

	aac_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), aac_do_draw, aac_do_keystroke);

	GrSelectEvents(aac_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_TIMER);

	GrMapWindow(aac_wid);
	total_time = remaining_time = len;
	aac_do_draw(0);

	start_aac_playback(filename);
	window_open = 0;
#else /* USE_HELIXAACDEC */
	pz_error("No AAC Support");
	return;
#endif /* USE_HELIXAACDEC */
}

void new_aac_window_get_meta(char *filename)
{
#ifdef USE_HELIXAACDEC
	FILE *infile;
	mp4ff_t *mp4file;
	mp4ff_callback_t *mp4cb;	
	char *album, *artist, *title;
	long len;
	int track;
	
	infile = fopen(filename, "rb");
	if (!infile) {
		return;
	}
	
	aac_pause = 0;
	
	mp4cb = malloc(sizeof(mp4ff_callback_t));
	
	mp4cb->read = read_callback;
	mp4cb->seek = seek_callback;
	mp4cb->user_data = infile;

	mp4file = mp4ff_open_read(mp4cb);
	if (!mp4file) {
		free(mp4cb);
		mp4ff_close(mp4file);
		return;
	}

	if ((track = find_first_aac_track(mp4file)) < 0) {
		free(mp4cb);
		mp4ff_close(mp4file);
		return;
	}

	if (!mp4ff_meta_get_artist(mp4file, &artist))
		artist = strdup(" ");
	if (!mp4ff_meta_get_album(mp4file, &album))
		album = strdup(" ");
	if (!mp4ff_meta_get_title(mp4file, &title))
		album = strdup(" ");
		
	len = mp4ff_get_track_duration(mp4file, track) / 
	         (mp4ff_time_scale(mp4file, track) / 1000);

	free(mp4cb);
	mp4ff_close(mp4file);
	fclose(infile);

	new_aac_window(filename, album, artist, title, len);
	
	free(artist);
	free(album);
	free(title);
#else /* USE_HELIXAACDEC */
	pz_error("No AAC Support");
	return;
#endif /* USE_HELIXAACDEC */
}

