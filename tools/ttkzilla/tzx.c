/*
 * Copyright (C) 2004-2005 Bernard Leach, Matthew Westcott
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
#include <fcntl.h>

#include "pz.h"
#include "ipod.h"
#include "oss.h"

extern void new_browser_window(char *);

static long currenttime = 0;
static GR_TIMER_ID timer_id;
static GR_WINDOW_ID tzx_wid;
static GR_GC_ID tzx_gc;
#ifdef PZ_USE_THREADS
static pthread_t pcm_thread;
#endif
static volatile int playing = 0, paused = 0;
static char tzx_filename[128];
static dsp_st dspz;

#define TZX_Z80_FREQ	3500000
#define TZX_RATE	44100

typedef enum _tzx_block_type {
	PURE_TONE,
	DATA_BLOCK,
	PAUSE,
	NOP,
	PULSE_LIST,
	JUMP,
	LOOP_START,
	LOOP_END
} tzx_block_type;

typedef struct _tzx_block {
	tzx_block_type type;
	union block_info {
		struct _pure_tone {
			unsigned short pulse_period;
			unsigned short pulse_count;
			unsigned short pulses_done;
		} pure_tone;
		struct _data_block {
			int bit_count;
			unsigned short zero_pulse_period;
			unsigned short one_pulse_period;
			unsigned char this_byte;
			char this_byte_bits_done;
			char this_bit_pulses_done;
			int bits_done;
			long seek_pos; /* absolute offset into TZX file to seek to reach first data byte */
		} data_block;
		struct _pause {
			int pause_length;
			int pause_done; /* how much of the pause has been generated thus far */
			char has_smoothed; /* whether we have already dealt with holding the output high for 1ms */
		} pause;
		struct _pulse_list {
			long seek_pos;
			unsigned char pulse_count;
			unsigned char pulses_done;
		} pulse_list;
		struct _jump {
			int destination_block_number;
		} jump;
		struct _loop_start {
			unsigned short loop_count;
		} loop_start;
	} info;
} TZX_BLOCK;

typedef struct _tzx {
	TZX_BLOCK blocks[1000];
	TZX_BLOCK *numbered_blocks[1000];
	TZX_BLOCK *current_block;
	TZX_BLOCK *end;
	short polarity;
	int block_count;
	int numbered_block_count;
	unsigned short loops_remaining;
	TZX_BLOCK *loop_to;
	FILE *file;
} TZX;

static volatile int killed;

#define DSP_PLAY_SIZE	4*1024*2

static unsigned short pcm_buf[16*1024];

int is_tzx_audio_type(char *extension)
{
	return strcmp(extension, ".tzx") == 0;
}

static void tzx_do_draw()
{
	pz_draw_header("Playback");

	GrSetGCForeground(tzx_gc, WHITE);
	GrFillRect(tzx_wid, tzx_gc, 0, 0, screen_info.cols, screen_info.rows);
	GrSetGCForeground(tzx_gc, BLACK);

	if (playing) {
		GrText(tzx_wid, tzx_gc, 8, 20, _("Press action to stop"), -1, GR_TFASCII);
		if (paused) {
			GrText(tzx_wid, tzx_gc, 8, 35, _("Press Play/Pause to resume"), -1, GR_TFASCII);
		}
		else {
			GrText(tzx_wid, tzx_gc, 8, 35, _("Press Play/Pause to pause"), -1, GR_TFASCII);
		}
	}
	else {
		GrText(tzx_wid, tzx_gc, 8, 20, _("Press action to playback"), -1, GR_TFASCII);
	}
}

static void draw_time()
{
	struct tm *tm;
	char onscreentimer[128];

	/* draw the time using currenttime */
	tm = gmtime(&currenttime);
	sprintf(onscreentimer, "Time: %02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);

	GrText(tzx_wid, tzx_gc, 40, 80, onscreentimer, -1, GR_TFASCII);
}

void tzx_init(TZX *tzx)
{
	tzx->end = tzx->blocks;
	tzx->block_count = 0;
	tzx->numbered_block_count = 0;
	tzx->polarity = 0x7fff;
}

TZX_BLOCK *append_block(TZX *tzx)
{
	(tzx->end)++;
	return &(tzx->blocks[(tzx->block_count)++]);
}

TZX_BLOCK *append_numbered_block(TZX *tzx)
{
	tzx->numbered_blocks[(tzx->numbered_block_count)++] = tzx->end;
	return append_block(tzx);
}

/* Load a TZX file into a TZX structure */
void load_tzx(FILE *file, TZX *tzx)
{
	unsigned char tzx_block_type;
	TZX_BLOCK *block;
	unsigned short pause_length_ms;
	unsigned short pilot_pulse_length;
	unsigned short first_sync_pulse_length;
	unsigned short second_sync_pulse_length;
	unsigned short zero_pulse_length;
	unsigned short one_pulse_length;
	unsigned short pilot_pulse_count;
	unsigned char trailing_bit_count;
	unsigned short sample_rate;
	unsigned short offset;
	unsigned short repeat_count;
	char snapshot_type;
	int data_length;

	tzx->file = file;

	/* assume a 10-byte TZX header, skip to first TZX block */
	fseek(file, 10, SEEK_SET);
	while (!feof(file)) {
		tzx_block_type = getc(file);
		if (feof(file)) {
			break;
		}
		switch (tzx_block_type) {
			case 0x10: /* standard speed data block */
				/* fetch params */
				pause_length_ms = getc(file) | (getc(file) << 8);
				data_length = getc(file) | (getc(file) << 8);
				/* pilot tone */
				block = append_numbered_block(tzx);
				block->type = PURE_TONE;
				block->info.pure_tone.pulse_count = 5000; /* fixme: use 8064 for header, 3220 for data */
				block->info.pure_tone.pulse_period = 2168;
				/* first sync pulse */
				block = append_block(tzx);
				block->type = PURE_TONE;
				block->info.pure_tone.pulse_count = 1;
				block->info.pure_tone.pulse_period = 667;
				/* second sync pulse */
				block = append_block(tzx);
				block->type = PURE_TONE;
				block->info.pure_tone.pulse_count = 1;
				block->info.pure_tone.pulse_period = 735;
				/* data */
				block = append_block(tzx);
				block->type = DATA_BLOCK;
				block->info.data_block.bit_count = data_length << 3;
				block->info.data_block.zero_pulse_period = 855;
				block->info.data_block.one_pulse_period = 1710;
				block->info.data_block.seek_pos = ftell(file);
				/* pause */
				if (pause_length_ms > 0) {
					block = append_block(tzx);
					block->type = PAUSE;
					block->info.pause.pause_length = pause_length_ms * (TZX_Z80_FREQ / 1000);
				}
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x11: /* turbo loading data block */
				/* fetch params */
				pilot_pulse_length = getc(file) | (getc(file) << 8);
				first_sync_pulse_length = getc(file) | (getc(file) << 8);
				second_sync_pulse_length = getc(file) | (getc(file) << 8);
				zero_pulse_length = getc(file) | (getc(file) << 8);
				one_pulse_length = getc(file) | (getc(file) << 8);
				pilot_pulse_count = getc(file) | (getc(file) << 8);
				trailing_bit_count = getc(file);
				pause_length_ms = getc(file) | (getc(file) << 8);
				data_length = getc(file) | (getc(file) << 8) | (getc(file) << 16);
				/* pilot tone */
				block = append_numbered_block(tzx);
				block->type = PURE_TONE;
				block->info.pure_tone.pulse_count = pilot_pulse_count;
				block->info.pure_tone.pulse_period = pilot_pulse_length;
				/* first sync pulse */
				block = append_block(tzx);
				block->type = PURE_TONE;
				block->info.pure_tone.pulse_count = 1;
				block->info.pure_tone.pulse_period = first_sync_pulse_length;
				/* second sync pulse */
				block = append_block(tzx);
				block->type = PURE_TONE;
				block->info.pure_tone.pulse_count = 1;
				block->info.pure_tone.pulse_period = second_sync_pulse_length;
				/* data */
				block = append_block(tzx);
				block->type = DATA_BLOCK;
				block->info.data_block.bit_count = ((data_length - 1) << 3) + trailing_bit_count;
				block->info.data_block.zero_pulse_period = zero_pulse_length;
				block->info.data_block.one_pulse_period = one_pulse_length;
				block->info.data_block.seek_pos = ftell(file);
				/* pause */
				if (pause_length_ms > 0) {
					block = append_block(tzx);
					block->type = PAUSE;
					block->info.pause.pause_length = pause_length_ms * (TZX_Z80_FREQ / 1000);
				}
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x12: /* pure tone */
				pilot_pulse_length = getc(file) | (getc(file) << 8);
				pilot_pulse_count = getc(file) | (getc(file) << 8);
				block = append_numbered_block(tzx);
				block->type = PURE_TONE;
				block->info.pure_tone.pulse_period = pilot_pulse_length;
				block->info.pure_tone.pulse_count = pilot_pulse_count;
				break;
			case 0x13: /* pulse sequence */
				data_length = getc(file);
				block = append_numbered_block(tzx);
				block->type = PULSE_LIST;
				block->info.pulse_list.seek_pos = ftell(file);
				block->info.pulse_list.pulse_count = data_length;
				fseek(file, data_length << 1, SEEK_CUR);
				break;
			case 0x14: /* pure data */
				/* fetch params */
				zero_pulse_length = getc(file) | (getc(file) << 8);
				one_pulse_length = getc(file) | (getc(file) << 8);
				trailing_bit_count = getc(file);
				pause_length_ms = getc(file) | (getc(file) << 8);
				data_length = getc(file) | (getc(file) << 8) | (getc(file) << 16);
				/* data */
				block = append_numbered_block(tzx);
				block->type = DATA_BLOCK;
				block->info.data_block.bit_count = ((data_length - 1) << 3) + trailing_bit_count;
				block->info.data_block.zero_pulse_period = zero_pulse_length;
				block->info.data_block.one_pulse_period = one_pulse_length;
				block->info.data_block.seek_pos = ftell(file);
				/* pause */
				if (pause_length_ms > 0) {
					block = append_block(tzx);
					block->type = PAUSE;
					block->info.pause.pause_length = pause_length_ms * (TZX_Z80_FREQ / 1000);
				}
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x15: /* direct recording */
				sample_rate = getc(file) | (getc(file) << 8);
				pause_length_ms = getc(file) | (getc(file) << 8);
				trailing_bit_count = getc(file);
				data_length = getc(file) | (getc(file) << 8) | (getc(file) << 16);
				block = append_numbered_block(tzx);
				block->type = NOP; /* TODO: implement this block type */
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x20: /* pause / stop */
				pause_length_ms = getc(file) | (getc(file) << 8);
				if (pause_length_ms > 0) { /* TODO: implement HALT behaviour where length = 0 */
					block = append_block(tzx);
					block->type = PAUSE;
					block->info.pause.pause_length = pause_length_ms * (TZX_Z80_FREQ / 1000);
				}
				break;
			case 0x21: /* group start */
				data_length = getc(file);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x22: /* group end */
				block = append_numbered_block(tzx);
				block->type = NOP;
				break;
			case 0x23: /* jump to block */
				offset = getc(file) | (getc(file) << 8);
				block = append_numbered_block(tzx);
				block->type = JUMP;
				block->info.jump.destination_block_number = (tzx->numbered_block_count - 1)  + offset;
				break;
			case 0x24: /* loop start */
				repeat_count = getc(file) | (getc(file) << 8);
				block = append_numbered_block(tzx);
				block->type = LOOP_START;
				block->info.loop_start.loop_count = repeat_count;
				break;
			case 0x25: /* loop end */
				block = append_numbered_block(tzx);
				block->type = LOOP_END;
				break;
			case 0x26: /* call sequence */
				data_length = getc(file) | (getc(file) << 8);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length * 2, SEEK_CUR);
				break;
			case 0x27: /* return from sequence */
				block = append_numbered_block(tzx);
				block->type = NOP;
				break;
			case 0x28: /* select block */
				data_length = getc(file) | (getc(file) << 8);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x30: /* text description */
				data_length = getc(file);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x31: /* message block */
				pause_length_ms = getc(file);
				/* actually the pause is counted in seconds, but it isn't worth defining another variable
				just to be thrown away... */
				data_length = getc(file);
				block->type = NOP;
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x32: /* archive info */
				data_length = getc(file) | (getc(file) << 8);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x33: /* hardware type */
				data_length = getc(file);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length * 3, SEEK_CUR);
				break;
			case 0x34: /* emulation info */
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, 8, SEEK_CUR);
				break;
			case 0x35: /* custom info block */
				fseek(file, 10, SEEK_CUR);
				data_length = getc(file) | (getc(file) << 8) | (getc(file) << 16) | (getc(file) << 24);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x40: /* snapshot block */
				snapshot_type = getc(file);
				data_length = getc(file) | (getc(file) << 8) | (getc(file) << 16);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length, SEEK_CUR);
				break;
			case 0x5a: /* TZX header from concatenated file */
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, 9, SEEK_CUR);
				break;
			default: /* other block, following extension rule */
				data_length = getc(file) | (getc(file) << 8) | (getc(file) << 16) | (getc(file) << 24);
				block = append_numbered_block(tzx);
				block->type = NOP;
				fseek(file, data_length, SEEK_CUR);
				break;
		}
	}
}

/* initialise the current block of the TZX file */
void tzx_block_init(TZX *tzx)
{
	while(1) {
		/* keep iterating until we reach a block that generates some output, or the end */
		if (tzx->current_block == tzx->end) {
			return;
		}
		switch (tzx->current_block->type) {
			case PURE_TONE:
				tzx->current_block->info.pure_tone.pulses_done = 0;
				return;
			case DATA_BLOCK:
				fseek(tzx->file, tzx->current_block->info.data_block.seek_pos, SEEK_SET);
				tzx->current_block->info.data_block.this_byte = getc(tzx->file);
				tzx->current_block->info.data_block.this_byte_bits_done = 0;
				tzx->current_block->info.data_block.this_bit_pulses_done = 0;
				tzx->current_block->info.data_block.bits_done = 0;
				return;
			case PAUSE:
				tzx->current_block->info.pause.pause_done = 0;
				tzx->current_block->info.pause.has_smoothed = 0;
				return;
			case NOP:
				(tzx->current_block)++;
				break;
			case PULSE_LIST:
				fseek(tzx->file, tzx->current_block->info.pulse_list.seek_pos, SEEK_SET);
				tzx->current_block->info.pulse_list.pulses_done = 0;
				return;
			case JUMP:
				tzx->current_block = tzx->numbered_blocks[tzx->current_block->info.jump.destination_block_number];
				break;
			case LOOP_START:
				tzx->loops_remaining = tzx->current_block->info.loop_start.loop_count;
				(tzx->current_block)++;
				tzx->loop_to = tzx->current_block;
				break;
			case LOOP_END:
				if (--(tzx->loops_remaining)) {
					tzx->current_block = tzx->loop_to;
				} else {
					(tzx->current_block)++;
				}
				break;
		}
	}
}

int get_next_pulse(TZX *tzx)
{
	int pulse_length;
	unsigned char bitfield;

	while(1) {
		/* keep iterating until we reach a block that generates some output, or the end */
		if (tzx->current_block == tzx->end) {
			return 0;
		}
		switch (tzx->current_block->type) {
			case PURE_TONE:
				if (tzx->current_block->info.pure_tone.pulses_done < tzx->current_block->info.pure_tone.pulse_count) {
					tzx->polarity = -(tzx->polarity);
					tzx->current_block->info.pure_tone.pulses_done++;
					return tzx->current_block->info.pure_tone.pulse_period;
				} else {
					(tzx->current_block)++;
					tzx_block_init(tzx);
				}
				break;
			case DATA_BLOCK:
				if (tzx->current_block->info.data_block.bits_done < tzx->current_block->info.data_block.bit_count) {
					if (tzx->current_block->info.data_block.this_bit_pulses_done == 2) {
						/* move on to next bit */
						(tzx->current_block->info.data_block.bits_done)++;
						(tzx->current_block->info.data_block.this_byte_bits_done)++;
						if (tzx->current_block->info.data_block.this_byte_bits_done == 8) {
							/* move on to next byte */
							tzx->current_block->info.data_block.this_byte = getc(tzx->file);
							tzx->current_block->info.data_block.this_byte_bits_done = 0;
						}
						tzx->current_block->info.data_block.this_bit_pulses_done = 0;
					}
					tzx->polarity = -(tzx->polarity);
					bitfield = 1 << (7 - tzx->current_block->info.data_block.this_byte_bits_done);
					if (tzx->current_block->info.data_block.this_byte & bitfield) {
						pulse_length = tzx->current_block->info.data_block.one_pulse_period;
					} else {
						pulse_length = tzx->current_block->info.data_block.zero_pulse_period;
					}
					(tzx->current_block->info.data_block.this_bit_pulses_done)++;
					return pulse_length;
				} else {
					(tzx->current_block)++;
					tzx_block_init(tzx);
				}
				break;
			case PAUSE:
				if (tzx->current_block->info.pause.pause_done < tzx->current_block->info.pause.pause_length) {
					if (tzx->polarity > 0 && !(tzx->current_block->info.pause.has_smoothed)) {
						/* sustain the high signal for 1ms more */
						tzx->current_block->info.pause.pause_done += (TZX_Z80_FREQ / 1000);
						tzx->current_block->info.pause.has_smoothed = 1;
						return (TZX_Z80_FREQ / 1000);
					} else {
						/* output a low signal for the remainder of the pause */
						tzx->polarity = -0x7fff;
						pulse_length = tzx->current_block->info.pause.pause_length - tzx->current_block->info.pause.pause_done;
						tzx->current_block->info.pause.pause_done += pulse_length;
						return pulse_length;
					}
				} else {
					(tzx->current_block)++;
					tzx_block_init(tzx);
				}
				break;
			case PULSE_LIST:
				if (tzx->current_block->info.pulse_list.pulses_done < tzx->current_block->info.pulse_list.pulse_count) {
					tzx->polarity = -(tzx->polarity);
					pulse_length = fgetc(tzx->file) | (fgetc(tzx->file) << 8);
					tzx->current_block->info.pulse_list.pulses_done++;
					return pulse_length;
				} else {
					(tzx->current_block)++;
					tzx_block_init(tzx);
				}
				break;
			default:
				printf("Running get_next_pulse with inappropriate block type!\n");
				break;
		}
	}
}

static void * tzx_playback(void *filename)
{
	int ret;
	FILE *tzx_file;
	/* FILE *debug_file; */
	TZX tzx;

	int localpaused = 0;
	int finished = 0;
	int ticks_remaining;
	unsigned short value;
	ssize_t count;

	tzx_file = fopen((char *)filename, "r");
	if (tzx_file == NULL) {
		pz_perror((char *)filename);
		goto no_file;
	}
	tzx_init(&tzx);
	load_tzx(tzx_file, &tzx);
	
	/* debug_file = fopen("dsp_debug", "w"); */

	ret = dsp_open(&dspz, DSP_LINEOUT);
	if (ret < 0) {
		pz_perror("/dev/dsp");
		goto no_audio;
	}

	dsp_setup(&dspz, 1, TZX_RATE);

	killed = 0;
	playing = 1;
	paused = 0;

	tzx_do_draw(0);

	timer_id = GrCreateTimer(tzx_wid, 1000);
	draw_time();

	tzx.current_block = tzx.blocks;
	tzx_block_init(&tzx);
	ticks_remaining = 0;
	finished = (tzx.current_block == tzx.end);

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
			timer_id = GrCreateTimer(tzx_wid, 1000);
		}

		if (!paused) {
			count = 0;

			/* output samples at the current polarity or until the buffer ends, whichever is sooner */
			while(ticks_remaining > 0 && count < DSP_PLAY_SIZE) {
				value = tzx.polarity;
#ifdef IS_BIG_ENDIAN
				value = bswap_16(value);
#endif
				p[count++] = value;
				/* putc(value & 0xff, debug_file); */
				/* putc(value >> 8, debug_file); */
				ticks_remaining -= (TZX_Z80_FREQ / TZX_RATE);
			}
			dsp_write(&dspz, (void *)p, count * 2);

			/* get next pulse */
			while (ticks_remaining <= 0 && !finished) {
				ticks_remaining += get_next_pulse(&tzx);
				finished = (ticks_remaining <= 0 && tzx.current_block == tzx.end);
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
	} while (!killed && !finished);

	dsp_close(&dspz);

	if (!localpaused) {
		GrDestroyTimer(timer_id);
	}

	if (!killed) {
		pz_close_window(tzx_wid);
	}

no_audio:
	/* fclose(debug_file); */
	fclose(tzx_file);

no_file:
	playing = 0;

	return NULL;
}

static void start_playback()
{
#ifdef PZ_USE_THREADS
	if (pthread_create(&pcm_thread, NULL, tzx_playback, tzx_filename) != 0) {
		new_message_window("could not create thread");
	}
#else
	tzx_playback(tzx_filename);
#endif
}

static void stop_tzx()
{
	killed = 1;
#ifdef PZ_USE_THREADS
	pthread_join(pcm_thread, NULL);
#endif
}

static void pause_tzx()
{
	paused = 1;
}

static void resume_tzx()
{
	paused = 0;
}

static int tzx_do_keystroke(GR_EVENT * event)
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
			if (playing) {
				stop_tzx();
				pz_close_window(tzx_wid);
			}
			else {
				start_playback();
			}
			ret |= KEY_CLICK;
			break;
	
		case 'm':
			if (playing) {
				stop_tzx();
			}
			pz_close_window(tzx_wid);
			break;
	
		case '1':
		case 'd':
			if (playing) {
				if (paused) {
					/* timer_id = GrCreateTimer(tzx_wid, 1000); draw_time(); */
					resume_tzx();
				}
				else {
					pause_tzx();
					/*GrDestroyTimer(timer_id); draw_time();*/
				}
			}
			tzx_do_draw(0);
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
	
void new_tzx_audio_window(char *filename)
{
	strncpy(tzx_filename, filename, sizeof(tzx_filename) - 1);

	playing = 0;
	paused = 0;
	currenttime = 0;

	tzx_gc = pz_get_gc(1);
	GrSetGCUseBackground(tzx_gc, GR_TRUE);
	GrSetGCForeground(tzx_gc, BLACK);
	GrSetGCBackground(tzx_gc, WHITE);

	tzx_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			screen_info.rows - (HEADER_TOPLINE + 1),
			tzx_do_draw, tzx_do_keystroke);

	GrSelectEvents(tzx_wid, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_KEY_UP |
			GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);

	GrMapWindow(tzx_wid);
}

void new_tzx_playback_window(char *filename)
{
	new_tzx_audio_window(filename);

	tzx_do_draw(0);

	start_playback();
}
#endif
