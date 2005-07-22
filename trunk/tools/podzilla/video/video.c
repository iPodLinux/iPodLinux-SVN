/*
 * Copyright (C) 2005 Adam Johnston <agjohnst@uiuc.edu>
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux
#include <linux/soundcard.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "avifile.h"
#include "videovars.h"
#include "../pz.h"
#include "../oss.h"
#include "../ipod.h"









#ifdef IPOD
#define VIDEO_CONTROL_MODE_RUNNING 	0
#define VIDEO_CONTROL_MODE_PAUSED	1
#define VIDEO_CONTROL_MODE_SEARCH	(2 | VIDEO_CONTROL_MODE_PAUSED)
#define VIDEO_CONTROL_MODE_VOLUME	(4 | VIDEO_CONTROL_MODE_RUNNING)
#define VIDEO_CONTROL_MODE_EXITING	(8 | VIDEO_CONTROL_MODE_PAUSED)

static GR_WINDOW_ID video_wid;
static GR_GC_ID video_gc;
static GR_IMAGE_ID video_id;

#define inl(a) (*(volatile unsigned long *) (a))
#define outl(a, b) (*(volatile unsigned long *) (b) = (a))

//////////////////////STATUS VARS
static int video_status = VIDEO_CONTROL_MODE_RUNNING;
static int video_useAudio = 0;
static int video_screenWidth;
static int video_screenHeight;
static int video_screenBPP;
static int video_useKeys = 1;
static long hw_vers = 0;
static int curbuffer = 0;
static int video_vol_delta = 0;
static int video_waitUsecPause = 5000;
static struct filebuffer fileBuffers[VIDEO_FILEBUFFER_COUNT];
static unsigned filesize;
static FILE * curFile;

static dsp_st dspz;
extern void ipod_handle_video(void);
static struct framet audioframes[100];
static unsigned int * indexes;
static int audioreadoff = 0;
static int audiowriteoff = 0;
static unsigned int framestartoff = 0, frameendoff = 0;
static AVIMAINHEADER mainHeader;

static BITMAPINFO bitmapInfo;
static int video_curPosition = 0;

struct framet {
	unsigned char * buffer;
	int x;
	int y;
	int w;
} framet_t;

struct filebuffer {
	unsigned char * buffer;
	unsigned startOff, globalOff, localOff, size, filesize;
	FILE * file;
} filebuffer_t;

////////////////////////////////
// PROTOS
static void cop_wakeup();
static void cop_initVideo();
static void cop_setVideoParams(int, int, int);
//static void cop_waitReady(int);
static void cop_presentFrames(int, struct framet *, int);
static void cop_setPlay(int);
static int fbread(void *, unsigned, struct filebuffer *);
static void fbseek(struct filebuffer *, int, int);
static unsigned getFilesize(FILE *);
static int fourccCmp(char[4], char[4]);
static int openAviFile(char *);
static int closeAviFile();
static int readVideoInfo();
//static void fillIndexes(FILE *);
static int bufferFrames(struct filebuffer *, struct framet *, int);
static int video_seek(unsigned int);
static void audio_open();
static void audio_close();
//static void audio_adjustVolume(int);
static int playVideo(char *);
static void video_draw_pause();
static void video_draw_play();
static void video_draw_searchbar();
static void video_draw_exit();
static void video_refresh_control_state();
static void video_check_messages();
static void video_status_message();
static int video_do_keystroke(GR_EVENT *);

static void video_status_message(char *);

//////////////////////////////////////////////////////////
//  COP FUNCTIONS
//  /////////////////////////////////////////////////////
static void cop_wakeup()
{
	if (hw_vers >= 40000) {
		outl(0x0, 0x60007004);
	}
	else {
		outl(0xce, 0xcf004058);
	}
}

static void cop_initVideo()
{
	int i;

	outl(0, VAR_VIDEO_MODE);
	outl(FRAMES_PER_BUFFER, VAR_VIDEO_FRAMES_PER_BUFFER);
	outl(0, VAR_VIDEO_CURBUFFER_PLAYING);
	outl(0, VAR_VIDEO_CURFRAME_PLAYING);

	for (i = 0; i < VIDEO_FILEBUFFER_COUNT; i++) {
		outl(1, VAR_VIDEO_BUFFER_DONE + i * sizeof(unsigned int));
		outl(0, VAR_VIDEO_BUFFER_READY + i * sizeof(unsigned int));
	}
}

static void cop_setVideoParams(int width, int height, int usecsPerFrame)
{
	outl(width, VAR_VIDEO_FRAME_WIDTH);
	outl(height, VAR_VIDEO_FRAME_HEIGHT);
	outl(usecsPerFrame, VAR_VIDEO_MICROSECPERFRAME);
}

/*static void cop_waitReady(int curbuffer)
{
	while (inl(VAR_VIDEO_BUFFER_DONE + curbuffer * sizeof(unsigned int)) == 0); 
}*/

static void cop_presentFrames(int curbuffer, struct framet * frames, int count)
{
	unsigned i;

	for (i = 0; i < count; i++) {
		outl((unsigned int)(frames[i].buffer), VAR_VIDEO_FRAMES + FRAMES_PER_BUFFER * curbuffer * sizeof(unsigned int) + i * sizeof(unsigned int));
	}

	outl(0, VAR_VIDEO_BUFFER_DONE + curbuffer * sizeof(unsigned int));
	outl(count, VAR_VIDEO_BUFFER_READY + curbuffer * sizeof(unsigned int));
}

static void cop_setPlay(int val)
{
	outl(val, VAR_VIDEO_MODE);
}

static int fbread(void * dest, unsigned size, struct filebuffer * f)
{
	if ((f->buffer==NULL) || (f->localOff + size > f->size)) {
		return 0;
	}

	memcpy(dest, f->buffer+f->localOff, size);
	f->localOff += size;
	f->globalOff += size;

	return 1;
}

static void fbseek(struct filebuffer * f, int off, int seekstate)
{
	if (seekstate == SEEK_CUR) {
		f->globalOff += off;
		f->localOff += off;
	} else if (seekstate == SEEK_SET) {
		f->globalOff = off;
		f->localOff = off - f->startOff;
	}
}

static unsigned  getFilesize(FILE * f)
{
	long w;
	long l;

	w = ftell(f);
	fseek(f, 0, SEEK_END);
	l = ftell(f);
	fseek(f, w, SEEK_SET);

	return l;
}

static int fourccCmp(char s1[4], char s2[4])
{
	if ((s1[0]==s2[0]) &&
	    (s1[1]==s2[1]) &&
	    (s1[2]==s2[2]) &&
	    (s1[3]==s2[3])) {
		return 1;
	}

	return 0;
}




static int openAviFile(char * filename)
{
	int i;

	curFile = fopen(filename, "r");
	if (curFile) {
		filesize = getFilesize(curFile);
		for (i = 0; i < VIDEO_FILEBUFFER_COUNT; i++) {
			fileBuffers[i].buffer = malloc(VIDEO_FILEBUFFER_SIZE);
			fileBuffers[i].filesize = filesize;
			fileBuffers[i].file = curFile;
			if (fileBuffers[i].buffer == NULL) {
				for (i = i-1; i >= 0; i--) {
					free(fileBuffers[i].buffer);
				}
				exit(1);
			}
		}

		return 0;
	}

	return -1;
}

static int closeAviFile()
{
	int i;

	fclose(curFile);
	if (indexes != NULL) {
		free(indexes);
	}

	for (i = 0; i < VIDEO_FILEBUFFER_COUNT; i++) {
		if (fileBuffers[i].buffer!=NULL)
			free(fileBuffers[i].buffer);
	}
	return 0;
}

static int readVideoInfo(FILE * f)
{
	aviChunkHdr hdr;
	AVIMAINHEADER aviHeader;
	AVISTREAMHEADER streamHeader;
	riffHdr fileHdr;
	BITMAPINFO bitmapHeader;
	char holder[512];
	int done = 0;
	unsigned lastStartRead;

	fread(&fileHdr, sizeof(fileHdr), 1, f);
	if (fourccCmp(fileHdr.ckID, "RIFF")) {
		while (!done) {
			lastStartRead = ftell(f);
			fread(&hdr, sizeof(hdr), 1, f);
			while (hdr.ckID[0]=='\0') {
				lastStartRead++;
				fseek(f, lastStartRead, SEEK_SET);
				fread(&hdr, sizeof(hdr), 1, f);
			}
			if (fourccCmp(hdr.ckID, "LIST")) {
				fseek(f, 4, SEEK_CUR);
			} else if (fourccCmp(hdr.ckID, "avih")) {
				fread(&aviHeader, sizeof(aviHeader), 1, f);
				memcpy(&mainHeader, &aviHeader, sizeof(aviHeader));
			} else if (fourccCmp(hdr.ckID, "strh")) {
				fread(&streamHeader, sizeof(streamHeader), 1, f);
			} else if (fourccCmp(hdr.ckID, "strf")) {
				if (fourccCmp(streamHeader.fccType, "vids")) {
					fread(&bitmapHeader,
							sizeof(bitmapHeader),
							1,
							f);
					fseek(f, -sizeof(bitmapHeader), SEEK_CUR);
					memcpy(&bitmapInfo, &bitmapHeader, sizeof(bitmapHeader));
				} else if ((fourccCmp(streamHeader.fccType, "auds"))) {
					fread(&holder, 1, hdr.ckSize, f);
					fseek(f, -hdr.ckSize, SEEK_CUR);
				}

				fseek(f, hdr.ckSize, SEEK_CUR);
			} else if ((fourccCmp(hdr.ckID, "00db")) || (fourccCmp(hdr.ckID, "00dc"))) {
				done = 1;
				fseek(f, -sizeof(hdr), SEEK_CUR);
			} else if (fourccCmp(hdr.ckID, "00wb")) {
				done = 1;
				fseek(f, -sizeof(hdr), SEEK_CUR);

			} else {
				fseek(f, hdr.ckSize, SEEK_CUR);
			}
		}
	}

	framestartoff = ftell(f);
	frameendoff = fileBuffers[0].filesize;
	return 0;
}


/*static void fillIndexes(FILE * f)
{
	unsigned start;

	int curframe = 0;
	aviChunkHdr hdr;
	char status[256];

	indexes = (unsigned int *) malloc(mainHeader.dwTotalFrames * sizeof(unsigned int));

	start = ftell(f);

	while ((!feof(f)) && (curframe < mainHeader.dwTotalFrames)) {
		fread(&hdr, sizeof(hdr),1, f);
		if (fourccCmp(hdr.ckID, "LIST")) {
			fseek(f, 4, SEEK_CUR);
		} else if ((fourccCmp(hdr.ckID, "00db")) || (fourccCmp(hdr.ckID, "00dc"))) {
			fseek(f, hdr.ckSize, SEEK_CUR);

		}  else if ((fourccCmp(hdr.ckID, "00wb")) || (fourccCmp(hdr.ckID, "01wb"))) {
			indexes[curframe++] = ftell(f);
			sprintf(status, "Indexing %d", curframe);
			video_status_message(status);

			fseek(f, hdr.ckSize, SEEK_CUR);
		} else {
			fseek(f, hdr.ckSize, SEEK_CUR);
		}
	}

	fseek(f, start, SEEK_SET);
}*/


// WILL FAIL IF FILE JUST OPENED
// Movie info must have been read and we must be at mov offset
static int bufferFrames(struct filebuffer * fb, struct framet * frames, int count)
{
	int curframe = 0;
	unsigned cur_off = 0;
	int doneBuff = 0;
	unsigned lastRead;
	aviChunkHdr hdr;
	unsigned char * frameptr;

	fb->startOff = ftell(fb->file);
	fb->globalOff = fb->startOff;
	fb->localOff = 0;
	fb->size = fread(fb->buffer, 1, VIDEO_FILEBUFFER_SIZE, fb->file);
	cur_off = fb->globalOff;

	while ((!doneBuff) && (curframe < count) && (fb->globalOff < fb->filesize)) {
		lastRead = fb->globalOff;

		if (fbread(&hdr, sizeof(hdr), fb)) {
			if (fourccCmp(hdr.ckID, "LIST")) {
				fbseek(fb, 4, SEEK_CUR);
			} else if ((fourccCmp(hdr.ckID, "00db")) || (fourccCmp(hdr.ckID, "00dc"))) {
				if (fb->localOff+hdr.ckSize > fb->size) {
					doneBuff = 1;
					fbseek(fb, -sizeof(hdr), SEEK_CUR);
				} else {
					frameptr = fb->buffer;
					frameptr += fb->localOff;
					frames[curframe].buffer = frameptr;
					frames[curframe].x = bitmapInfo.bmiHeader.biWidth;
					frames[curframe].y = bitmapInfo.bmiHeader.biHeight;
					frames[curframe].w = bitmapInfo.bmiHeader.biBitCount;
					curframe++;
					fbseek(fb, hdr.ckSize, SEEK_CUR);
				}
			}  else if ((fourccCmp(hdr.ckID, "00wb")) || (fourccCmp(hdr.ckID, "01wb"))) {

				if (fb->localOff+hdr.ckSize > fb->size) {
					doneBuff=1;
					fbseek(fb, -sizeof(hdr), SEEK_CUR);
				} else {
					frameptr = fb->buffer;
					frameptr += fb->localOff - sizeof(hdr);

					audioframes[audiowriteoff].buffer = frameptr;
					audioframes[audiowriteoff].x = hdr.ckSize;
					fbseek(fb, hdr.ckSize, SEEK_CUR);
					audiowriteoff = (audiowriteoff + 1) % 100;
				}
			} else {
				fbseek(fb, hdr.ckSize, SEEK_CUR);
			}
		} else {
			doneBuff=1;
		}
	}

	fseek(fb->file, fb->globalOff, SEEK_SET);

	return curframe;
}

static int video_seek(unsigned int fileOff)
{
	char * curbuf;
	unsigned int off = 0;
	struct filebuffer * fb;

	fb = &fileBuffers[0];
	fseek(fb->file, fileOff, SEEK_SET);
	fb->startOff = ftell(fb->file);
	fb->globalOff = fb->startOff;
	fb->localOff = 0;
	fb->size = fread(fb->buffer, 1, VIDEO_FILEBUFFER_SIZE, fb->file);
	curbuf = fb->buffer;
	while ((curbuf < (char *)(fb->buffer+fb->size)) && ((*(char *)(curbuf)!='0') || (*(char *)(curbuf+1)!='0') || (*(char *)(curbuf+2)!='d'))) {

		curbuf++;
		off++;
	}

	if (curbuf < (char *)(fb->buffer+fb->size)) {
		cop_initVideo();
		curbuffer = 0;
		fseek(fb->file, fileOff+off, SEEK_SET);

		return 1;
	} else {
		return 0;
	}
}

static void audio_open()
{
	if (video_useAudio) {
		dsp_open(&dspz, 1); // DSP_LINEOUT);
		dsp_setup(&dspz, 2, 44100);
	}
}

static void audio_close()
{
	if (video_useAudio) {
		dsp_close(&dspz);
	}
}

/*static void audio_adjustVolume(int diff)
{
	if (video_useAudio) {
		dsp_vol_change(&dspz, diff);
	}
}*/

static int playVideo(char * filename)
{
	int curframes;
	struct framet frames[VIDEO_FILEBUFFER_COUNT][NUM_FRAMES_BUFFER];
	int i;
	int buffersProcessed = 0;
	
	curbuffer = 0;
	hw_vers = ipod_get_hw_version();
	if (hw_vers < 40000) {
		video_screenWidth=160;
		video_screenHeight=128;
		video_screenBPP=2;
		video_useAudio=0;
	} else if (((hw_vers >= 40000) && (hw_vers < 50000)) || ((hw_vers >= 70000) && (hw_vers < 80000))) {
		video_screenWidth=138;
		video_screenHeight=110;
		video_screenBPP=2;
		video_useAudio=0;
	} else if ((hw_vers>=50000) && (hw_vers < 60000)) {
		video_screenWidth=160;
		video_screenHeight=128;
		video_screenBPP=2;
		video_useAudio=0;
	} else if ((hw_vers>=60000) && (hw_vers < 70000)) {
		video_screenWidth=220;
		video_screenHeight=176;
		video_screenBPP=16;
		video_useAudio = 0;
		video_waitUsecPause = 15000;
	}

	i = 0;
	

	openAviFile(filename);
	readVideoInfo(curFile);
	outl((unsigned int)&ipod_handle_video, VAR_COP_HANDLER);
	cop_wakeup();

	audio_open();
	cop_initVideo();
	cop_setVideoParams(bitmapInfo.bmiHeader.biWidth, bitmapInfo.bmiHeader.biHeight, mainHeader.dwMicroSecPerFrame);
	curframes = 1;
	while ((curframes > 0)) {
		while (inl(VAR_VIDEO_BUFFER_DONE + curbuffer * sizeof(unsigned int)) == 0) {
			if (video_useKeys) {
				video_refresh_control_state();
			}

			if (video_status == VIDEO_CONTROL_MODE_EXITING) {
				return 1;
			}
			if ((video_useAudio)) {
				while (audioreadoff != audiowriteoff) {
					dsp_write(&dspz, audioframes[audioreadoff].buffer, audioframes[audioreadoff].x);
					audioreadoff = (audioreadoff + 1) % 100;
				}
			}
		}

		curframes = bufferFrames(&fileBuffers[curbuffer], frames[curbuffer], NUM_FRAMES_BUFFER);

		cop_presentFrames(curbuffer, frames[curbuffer], curframes);
		video_curPosition+=curframes;

		if (buffersProcessed >= VIDEO_FILEBUFFER_COUNT - 1) {
			cop_setPlay(1);
		} else {
			buffersProcessed++;
		}

		curbuffer = (curbuffer+1)%VIDEO_FILEBUFFER_COUNT;

		if (video_useKeys) {
			video_refresh_control_state();
		}
		if (video_status == VIDEO_CONTROL_MODE_EXITING) {
			return 1;
		}
		if ((video_useAudio)) {
			if (audioreadoff != audiowriteoff) {
				dsp_write(&dspz, audioframes[audioreadoff].buffer, audioframes[audioreadoff].x);
				audioreadoff = (audioreadoff + 1) % 100;
			}
		}
	}

	curbuffer -= 2;
	if (curbuffer < 0) {
		curbuffer += VIDEO_FILEBUFFER_COUNT;
	}

	while (!inl(VAR_VIDEO_BUFFER_DONE + curbuffer * sizeof(unsigned int))) {
		video_refresh_control_state();
		if (video_status == VIDEO_CONTROL_MODE_EXITING) {
			return 1;
		}
	}

	cop_setPlay(0);
	video_status_message("Video Done. Exiting.");
	closeAviFile();
	audio_close();
	pz_close_window(video_wid);
	GrFreeImage(video_id);
	GrDestroyGC(video_gc);
	return 0;
}

static void video_draw_pause()
{
	GrSetGCForeground(video_gc, WHITE);
	GrFillRect(video_wid, video_gc, 8, 8, 16, 32);
	GrFillRect(video_wid, video_gc, 32, 8, 16, 32);
	GrSetGCForeground(video_gc, BLACK);
	GrRect(video_wid, video_gc, 8, 8, 16, 32);
	GrRect(video_wid, video_gc, 32, 8, 16, 32);
}

static void video_draw_play()
{
}

static void video_draw_searchbar()
{
	int height = .1 * video_screenHeight;
	int offX;

        offX = (int)((double)((double)video_curPosition/mainHeader.dwTotalFrames) * (video_screenWidth-30)) + 16;

	GrSetGCForeground(video_gc, WHITE);
	GrFillRect(video_wid, video_gc, 16, video_screenHeight-2 * height, video_screenWidth-30+6, height);
	GrSetGCForeground(video_gc, BLACK);
	GrRect(video_wid, video_gc, 16, video_screenHeight-2 * height, video_screenWidth-30+6, height);
	GrFillRect(video_wid, video_gc, offX, video_screenHeight-2*height, 5, height);
}

static void video_draw_exit()
{
	video_status_message("Exiting");
}

static void video_refresh_control_state()
{
	int currun, oldPos;

	video_check_messages();
	cop_setPlay(!(video_status & VIDEO_CONTROL_MODE_PAUSED));
	currun = 0;
	oldPos = video_curPosition;
	while (video_status & VIDEO_CONTROL_MODE_PAUSED) {
		if ((currun==0) || (oldPos!=video_curPosition)) {
			usleep(video_waitUsecPause);
			if (video_status==VIDEO_CONTROL_MODE_PAUSED) {
				video_draw_pause();
			}
			else if (video_status==VIDEO_CONTROL_MODE_SEARCH) {
				video_draw_searchbar();
			}
			else if (video_status==VIDEO_CONTROL_MODE_EXITING) {
				video_draw_exit();
			}
			else if (video_status==VIDEO_CONTROL_MODE_RUNNING) {
				video_draw_play();
			}
			currun++;
		}

		oldPos = video_curPosition;
		video_check_messages();

		if (video_status == VIDEO_CONTROL_MODE_EXITING) {
			sleep(1);
			closeAviFile();
			audio_close();

			pz_close_window(video_wid);
			GrFreeImage(video_id);
			GrDestroyGC(video_gc);
			return;
		}
	}
}

static void video_check_messages()
{
	GR_EVENT event;
	int evtcap = 200;

	while (GrPeekEvent(&event) && evtcap--) {
		GrGetNextEventTimeout(&event, 500);
		if (event.type != GR_EVENT_TYPE_TIMEOUT) {
			pz_event_handler(&event);
		} else {
			break;
		}
	}
}

static void video_status_message(char *msg)
{
	GR_SIZE txt_width, txt_height, txt_base;
	GR_COORD txt_x, txt_y;

	GrGetGCTextSize(video_gc, msg, -1, GR_TFASCII, &txt_width, &txt_height, &txt_base);
	txt_x = (screen_info.cols - (txt_width + 10)) >> 1;
	txt_y = (screen_info.rows - (txt_height + 10)) >> 1;
	GrSetGCForeground(video_gc, WHITE);
	GrFillRect(video_wid, video_gc, txt_x, txt_y, txt_width + 10, txt_height + 10);
	GrSetGCForeground(video_gc, BLACK);
	GrRect(video_wid, video_gc, txt_x, txt_y, txt_width + 10, txt_height + 10);
	GrText(video_wid, video_gc, txt_x + 5, txt_y + txt_base + 5, msg, -1, GR_TFASCII);
}

static int video_do_keystroke(GR_EVENT * event)
{
	int ret = 0;

	switch(event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case 'm': /* Free and exit */
			video_status = VIDEO_CONTROL_MODE_EXITING;
			break;
		case '2':
		case 'r': /* Scroll right/down */
			if (video_status==VIDEO_CONTROL_MODE_SEARCH) {
				video_curPosition+=(.01*mainHeader.dwTotalFrames);
			} else  {
				video_vol_delta++;
			}
			if (video_curPosition > mainHeader.dwTotalFrames) {
				video_curPosition = mainHeader.dwTotalFrames;
			}
			break;
		case '3':
		case 'l': /* Scroll left/up */
			if (video_status==VIDEO_CONTROL_MODE_SEARCH) {
				video_curPosition-=(.01*mainHeader.dwTotalFrames);
			} else {
				video_vol_delta--;
			}

			if (video_curPosition < 0) {
				video_curPosition = 0;
			}
			break;
		case '\r':
		case '\n':
			if ((video_status == VIDEO_CONTROL_MODE_RUNNING) || (video_status==VIDEO_CONTROL_MODE_PAUSED)) {
				video_status = VIDEO_CONTROL_MODE_SEARCH;
			}
			else if (video_status == VIDEO_CONTROL_MODE_SEARCH) {
				video_seek(((double)video_curPosition/mainHeader.dwTotalFrames) * (frameendoff - framestartoff) + framestartoff);
				video_status = VIDEO_CONTROL_MODE_RUNNING;
			} else {
				video_status = VIDEO_CONTROL_MODE_RUNNING;
			}
		case 'w': /* Zoom out */
			break;
		case 'f': /* Zoom in */
			break;
		case '1':
		case 'd': /* Zoom fit/actual */
			if (video_status == VIDEO_CONTROL_MODE_PAUSED) {
				video_status = VIDEO_CONTROL_MODE_RUNNING;
			}
			else if (video_status != VIDEO_CONTROL_MODE_SEARCH) {
				video_status = VIDEO_CONTROL_MODE_PAUSED;
			}
			break;
		default:
			ret = 0;
			break;
		}
		break;
	}

	return ret;
}

#endif /* IPOD */

int is_video_type(char *extension)
{
	return strcasecmp(extension, ".avi") == 0;
}

void video_do_draw()
{
}

void new_video_window(char *filename)
{
#ifndef IPOD
	pz_error("No video support on the desktop.");
#else /* IPOD */
	video_status = VIDEO_CONTROL_MODE_RUNNING;
	video_curPosition = 0;
	video_gc = pz_get_gc(1);
	GrSetGCUseBackground(video_gc, GR_FALSE);
	GrSetGCForeground(video_gc, BLACK);

	video_wid = pz_new_window(0, 0, screen_info.cols, screen_info.rows, video_do_draw, video_do_keystroke);

	GrSelectEvents(video_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN| GR_EVENT_MASK_KEY_UP);
	GrMapWindow(video_wid);

	GrClearWindow(video_wid, GR_FALSE);

	video_status_message("Loading video...");
	playVideo(filename);
#endif
}

