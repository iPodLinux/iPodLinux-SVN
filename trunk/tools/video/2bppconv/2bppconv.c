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
 
 0.1 - Program born by Adam Johnston <agjohnst@uiuc.edu>
 0.2 - Cleaned up slightly, added error messages and big endian support 
	(still bugs) by Jeffrey Nelson <nelsonjm@vt.edu>
 */


#include <stdio.h>
#include <stdlib.h>
#include "avifile.h"

#define DEF_RES_X_NEW 160
#define DEF_RES_Y_NEW 128


//#define PRINTDEBUG 1

int be;

struct imagem { 
	unsigned char *buffer;
	int x;
	int y;
	int w;
} imagem_t;




static int fourccCmp(char s1[4], char s2[4])
{
	if ((s1[0] == s2[0]) &&
	    (s1[1] == s2[1]) &&
	    (s1[2] == s2[2]) &&
	    (s1[3] == s2[3]))
		return 1;
	return 0;
}

int main(int argc, char * argv[])
{
	return convVideo(argv[1], argv[2]);
}

void closeFiles(FILE * f, FILE * f2)
{
	if (f)close(f);
	if (f2) close(f2);
	
}

// Simple endian switching function.
unsigned switch_32(unsigned l)
{
    if (be)
		return ((l & 0xff) << 24)
			| ((l & 0xff00) << 8)
			| ((l & 0xff0000) >> 8)
			| ((l & 0xff000000) >> 24);
    return l;
}

// Simple endian switching function.
short int switch_16(short int l)
{
	if (be)
		return (
				(short int) ((short int)(l) << 0x08) | 
				(short int) ((short int)(l) >> 0x08));
	return l;
}

// Check to see if this is a big or little endian computer.
void test_endian(void)
{
    char ch[4] = { '\0', '\1', '\2', '\3' };
    unsigned i = 0x00010203;
	
    if (*((int *)ch) == i) 
		be = 1;
    else
		be = 0;
    return;
}

int convVideo(char * filename, char * outfilename)
{
	FILE * f;
	FILE * f2;
	unsigned int filesize;	
	AVIFILE af;
	struct imagem curframe[NUM_FRAMES_BUFFER];		
	
	char fourcc[4];
	unsigned char * buf_tmp;
	long frames;
	AVISTREAMHEADER avsh;
	BITMAPINFO binf;
	int gotinfo;
	unsigned char * pchar;
	unsigned char somechar[3];
	long x, y, z, w;
	unsigned char redValue, greenValue, blueValue;	
	int curbufferframe;
	int curonframe;
	unsigned short *word;
	unsigned char * chunkBuffer;
	unsigned r, g, b;
	int countpix;	
	long ccoff;	
	int width, height;
	
	unsigned char * randombuff;	
	int bitcount = 2;
	RGBQUAD * colortable = NULL;	
	riffHdr fileHdr;
	aviChunkHdr hdr;
	pchar = (unsigned char *)&somechar;
	gotinfo = 0;
	frames = 0;
	curbufferframe = 0;	
	
	int counter = 0;
	// See if we are big or little endian and set a flag if we are.
	test_endian();
	
	// open files
	f = fopen(filename, "r");
	f2 = fopen(outfilename, "w");
	if (!f || !f2)
	{
		printf("Error: can't open one or more of the files.\n");
		closeFiles(f, f2);
		return 1; 
	}
	
	// Get size of file.
	fseek(f, 0, SEEK_END);
	filesize = ftell(f);
	rewind(f);
	printf("Filesize is %d\n", filesize);	
	
	// Make sure file is big enough to read first few bits.
	if (filesize < sizeof(fileHdr))
	{
		printf("Error: file too small.\n");
		closeFiles(f, f2);
		return 1;
	}
	
	// Read format to make sure it is (RIFF)
	fread(&fileHdr, sizeof(fileHdr), 1, f);	
	if (!fourccCmp(fileHdr.ckID, "RIFF"))
	{
		printf("Error: bad file format\n");
		closeFiles(f, f2);
		return 1;			
	}
	// Write format (RIFF) to output.
	fwrite(&fileHdr, sizeof(fileHdr), 1, f2);	
	
	// Nullify buffer stuff.
	for (x=0; x<NUM_FRAMES_BUFFER; x++)	
		curframe[x].buffer = NULL;
	
	
	// Keep going until end of stream. 
	while (!feof(f))
	{
		ccoff = ftell(f);	
		fread(&hdr.ckID, sizeof(hdr), 1, f);
		
		while (hdr.ckID[0]=='\0')
		{
			fseek(f, ccoff+1, SEEK_SET);
			ccoff++;
			fread(&hdr, sizeof(hdr), 1, f);
		}
		
#ifdef PRINTDEBUG
		printf("Got: %.4s at off %d\n", hdr.ckID, ftell(f));
		//getchar();	
#endif
		
		if (fourccCmp(hdr.ckID, "LIST")) { // Got LIST code
			fread(&fourcc, sizeof(fourcc), 1, f);
			fwrite(&hdr, sizeof(hdr), 1, f2);
			fwrite(&fourcc, sizeof(fourcc), 1, f2);
		} else if (fourccCmp(hdr.ckID, "avih")) { // Got avih code
			chunkBuffer = (unsigned char *)malloc(switch_32(hdr.ckSize));	
			fread(&(af.hdr), sizeof(af.hdr), 1, f);
			fseek(f, -sizeof(af.hdr), SEEK_CUR);
			fread(chunkBuffer, 1, switch_32(hdr.ckSize), f);	
			fwrite(&hdr, sizeof(hdr), 1, f2);
			fwrite(chunkBuffer, 1, switch_32(hdr.ckSize), f2);		
			free(chunkBuffer);
			printf("AVI File has %d frames.\n", switch_32(af.hdr.dwTotalFrames));
		} else if (fourccCmp(hdr.ckID, "strh")) { // Got strh code
			fread(&avsh, sizeof(avsh), 1, f);
			fseek(f, -sizeof(avsh), SEEK_CUR);
			chunkBuffer = (unsigned char *) malloc(switch_32(hdr.ckSize));	
			fread(chunkBuffer, 1, switch_32(hdr.ckSize), f);	
			fwrite(&hdr, sizeof(hdr), 1, f2);
			fwrite(chunkBuffer, 1, switch_32(hdr.ckSize), f2);
			free(chunkBuffer);
		} else if (fourccCmp(hdr.ckID, "strf")) { // Got strf code.
			if (gotinfo==0)
			{
				printf("gotinfo=0\n");
				BITMAPINFO binfcopy;	
				fread(&binf, sizeof(binf), 1, f);
				
				
				width = (switch_32(binf.bmiHeader.biWidth) > DEF_RES_X_NEW) ?
					switch_32(DEF_RES_X_NEW) : binf.bmiHeader.biWidth;
				height = (switch_32(binf.bmiHeader.biHeight) > DEF_RES_Y_NEW) ?
					switch_32(DEF_RES_Y_NEW) : binf.bmiHeader.biHeight;	
				
				printf("biSize: %d\n"
					   "biWidth: %d\n"
					   "biHeight: %d\n"
					   "biBitCount: %d\n"
					   "biCompression: %d\n",
					   switch_32(binf.bmiHeader.biSize),
					   switch_32(binf.bmiHeader.biWidth),
					   switch_32(binf.bmiHeader.biHeight),
					   switch_16(binf.bmiHeader.biBitCount),
					   switch_32(binf.bmiHeader.biCompression));	
				for (x = 0; x < NUM_FRAMES_BUFFER; x++)	
#ifdef FORCE16BIT
					curframe[x].buffer = (unsigned char *)malloc(DEF_RES_X_NEW*DEF_RES_Y_NEW*2);
#else 
				curframe[x].buffer = (unsigned char *)malloc(DEF_RES_X_NEW * DEF_RES_Y_NEW * 3); 
#endif	
				
				if (switch_16(binf.bmiHeader.biBitCount)==8) {
					colortable = malloc(256*sizeof(RGBQUAD));	
					fread(colortable, sizeof(RGBQUAD), 256, f);
					fseek(f, -256*sizeof(RGBQUAD), SEEK_CUR);
				}	
				gotinfo = 1;	
				fseek(f, -sizeof(binf), SEEK_CUR);
				fseek(f, switch_32(hdr.ckSize), SEEK_CUR);	
				memcpy(&binfcopy, &binf, sizeof(binf)); //***
				
				binfcopy.bmiHeader.biWidth = width;
				binfcopy.bmiHeader.biHeight = height;	
				binfcopy.bmiHeader.biBitCount = switch_16(bitcount);
				hdr.ckSize = switch_32(sizeof(binfcopy));	
				fwrite(&hdr, sizeof(hdr), 1, f2);
				fwrite(&binfcopy, 1, sizeof(binfcopy), f2);
				
				
				
			} else {
				printf("gotinfo!=0\n"); 
				chunkBuffer = (unsigned char *) malloc(switch_32(hdr.ckSize));
				fread(chunkBuffer, 1, switch_32(hdr.ckSize), f);
				fwrite(&hdr, sizeof(hdr), 1, f2);
				fwrite(chunkBuffer, 1, switch_32(hdr.ckSize), f2);
				free(chunkBuffer);	
			}
			
			
		} else if ((fourccCmp(hdr.ckID, "00dc")) || (fourccCmp(hdr.ckID, "00db"))) {
			if (curframe[curbufferframe].buffer!=NULL)
			{	
				int factor = 4;
				int curspot = 3;
				
				countpix = 0;
				buf_tmp = curframe[curbufferframe].buffer;	
				curframe[curbufferframe].x = width;
				curframe[curbufferframe].y = height;
				curframe[curbufferframe].w = 2;
				
				w = ftell(f);
				
				buf_tmp = curframe[curbufferframe].buffer;
				word = (unsigned short *)(curframe[curbufferframe].buffer);
				
				z = 0;
				curspot = 3;
				for (y = switch_32(binf.bmiHeader.biHeight)-1; (y>=0) && (z < DEF_RES_Y_NEW); y--)
				{
					fseek(f, w+z*switch_32(binf.bmiHeader.biWidth)*switch_16(binf.bmiHeader.biBitCount)/8, SEEK_SET);
					
					for (x = 0; (x < switch_32(width)/factor); x++)
					{
						//*****
						if ((factor==1) && (switch_16(binf.bmiHeader.biBitCount)==24)) { // never accessed
							fread(pchar, sizeof(char), 3, f);
							r = pchar[2] << 8;
							g = pchar[1] << 8;
							b = pchar[0] << 8;
							r &= 0xf800;
							g &= 0xfc00;
							b &= 0xf800;
							*(word) = r|g>>5|b>>11;
							word++;
						} else if ((factor==4) && (switch_16(binf.bmiHeader.biBitCount)==24))
						{	
							char col;	
							unsigned char curcol;	
							col = 0;
							curcol = 0;
							for (curspot = 0; curspot < 4; curspot++)
							{
								unsigned char pc;
								
								fread(&pc, sizeof(char), 1, f);
								blueValue = (255-pc);
								
								fread(&pc, sizeof(char), 1, f);
								greenValue = (255-pc);
								fread(&pc, sizeof(char), 1, f);
								redValue = (255-pc);
								curcol = (0.3*redValue+0.59*greenValue+0.11*blueValue);
								curcol = curcol/64;
								col ^= (curcol << (curspot << 1));	
							}
							countpix += 4;
							*buf_tmp = col;	
							buf_tmp++;	
							
						}
						
						
					}
					
					z++;
				}
				
				fseek(f, w+switch_32(hdr.ckSize), SEEK_SET);
				hdr.ckSize = switch_32(switch_32(curframe[curbufferframe].x) * switch_32(curframe[curbufferframe].y) * 
									   curframe[curbufferframe].w / 8);
				fwrite(&hdr, sizeof(hdr), 1, f2);
				fwrite(curframe[curbufferframe].buffer, 1, switch_32(hdr.ckSize), f2); 	
				
				printf("Writing frame \t %d \t at position \t %d\n", frames, ftell(f));	
				frames++;
				curbufferframe++;
				curonframe = 0;
				
			} 
		} else if ((fourccCmp(hdr.ckID, "idx1")) || (fourccCmp(hdr.ckID, "indx"))){
			fseek(f, hdr.ckSize, SEEK_CUR); 	
		} else {
			randombuff = malloc(switch_32(hdr.ckSize));
			fread(randombuff, 1, switch_32(hdr.ckSize), f);
			fwrite(&hdr, sizeof(hdr), 1, f2);
			fwrite(randombuff, 1, switch_32(hdr.ckSize), f2);
			free(randombuff);			
		}
		
		if (curbufferframe>=NUM_FRAMES_BUFFER)
		{
			curbufferframe = 0;
			curonframe = 0;
		}	
	}
	
	// Close the files
	closeFiles(f, f2);
	
	// Deallocation
	for (x = 0; x < NUM_FRAMES_BUFFER; x++)
		if (curframe[x].buffer!=NULL)
			free(curframe[x].buffer);
	if (colortable!=NULL)
		free(colortable);
}


