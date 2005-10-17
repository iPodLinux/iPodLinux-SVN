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

#ifndef __AVIFILE_H_
#define __AVIFILE_H_

typedef struct {
	char r, g, b, res;
} RGBQUAD;


extern int timer_get_current();


typedef struct {
	short int wFormatTag;
	short int nChannels;
	unsigned int nSamplesPerSec;
	unsigned int nAvgBytesPerSec;
	short int nBlockAlign;
	short int wBitsPerSample;
	short int cbSize;
} WAVEFORMATEX;



typedef struct {
	unsigned int biSize;
	long biWidth;
	long biHeight;
	short int biPlanes;
	short int biBitCount;
	unsigned int biCompression;
	unsigned int biSizeImage;
	long biXPelsPerMeter;
	long biYPelsPerMeter;
	unsigned int biClrUsed;
	unsigned int biClrImportant;
} BITMAPINFOHEADER;

typedef struct {
	BITMAPINFOHEADER bmiHeader;
} BITMAPINFO;

typedef struct {
	unsigned int dwMicroSecPerFrame;
	unsigned int dwMaxBytesPerSec;
	unsigned int dwPadingGranularity;
	unsigned int dwFlags;
	unsigned int dwTotalFrames;
	unsigned int dwInitialFrames;
	unsigned int dwStreams;
	unsigned int dwSuggestedBufferSize;
	unsigned int dwWidth;
	unsigned int dwHeight;
	unsigned int dwReserved[4];
} AVIMAINHEADER;

typedef struct {
	char fccType[4];
	char fccHandler[4];
	unsigned int dwFlags;
	short int wPriority;
	short int wLanguage;
	unsigned int dwInitialFrames;
	unsigned int dwScale;
	unsigned int dwRate;
	unsigned int dwStart;
	unsigned int dwLength;
	unsigned int dwSuggestedBufferSize;
	unsigned int dwQuality;
	unsigned int dwSampleSize;
	struct {
		short int left;
		short int top;
		short int right;
		short int bottom;
	} rcFrame;
} AVISTREAMHEADER;

typedef struct {

	AVIMAINHEADER hdr;

} AVIFILE;

typedef struct {
	char ckID[4];
	unsigned int ckSize;
	char ckType[4];
} riffHdr;

typedef struct {
	char ckID[4];
	unsigned int ckSize;
} aviChunkHdr;

typedef struct {
	char ckID[4];
	unsigned int ckSize;
	void * ckData;
} aviChunk;

#endif
