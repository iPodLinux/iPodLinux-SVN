/*
 * Copyright (c) 2005, James Jacobsson
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 * Neither the name of the organization nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior
 * written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "hotdog.h"

#define WIDTH  320
#define HEIGHT 240

#ifdef IPOD
#include <termios.h> 
#include <sys/time.h>

uint32 *screen;

static struct termios stored_settings; 

void set_keypress(void)
{
	struct termios new_settings;
	tcgetattr(0,&stored_settings);
	new_settings = stored_settings;
	new_settings.c_lflag &= (~ICANON);
	new_settings.c_cc[VTIME] = 0;
	tcgetattr(0,&stored_settings);
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0,TCSANOW,&new_settings);
}

void reset_keypress(void)
{
	tcsetattr(0,TCSANOW,&stored_settings);
}

extern void _HD_ARM_Update5G (uint16 *fb, int x, int y, int w, int h);
static void update (hd_engine *eng, int x, int y, int w, int h)
{
	_HD_ARM_Update5G (eng->screen.framebuffer, x, y, w, h);
}

uint32 GetTimeMillis(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint32)((tv.tv_sec % 0xffff) * 1000 + tv.tv_usec / 1000);
}

#else
#include "SDL.h"

SDL_Surface *screen;

uint32 GetTimeMillis(void)
{ return (uint32)SDL_GetTicks(); }

static void update (hd_engine *eng, int x, int y, int w, int h)
{
	SDL_UpdateRect (SDL_GetVideoSurface(), x, y, w, h);
}
#endif

typedef struct circle_object {
	hd_object *object;
	int32 position;
} circle_object;

#define check_pending() add_pending(0)
static int add_pending(int dir)
{
	static unsigned char s;
	static struct _pends {
		char dir;
		int32 time;
	} pends[2];

	if (!dir) {
		int ra,rb;
	       	ra = GetTimeMillis()-pends[0].time;
		rb = GetTimeMillis()-pends[1].time;
		return (ra > 350 && rb > 350) ? 0 : pends[(ra > rb)].dir;
	}

	s = !s;
	pends[s].dir = (char)dir;
	pends[s].time = GetTimeMillis();
	return 0;
}
	
static void circle_rotate(circle_object *circle, int dir)
{
	HD_AnimateCircle(circle->object, 80, 50, 50, (50 << 16) /
			circle->object->w, (70 << 16) / circle->object->w,
			circle->position, dir*1024, 25);
	if (dir < 0) {
		circle->position = (circle->position - 1024);
		if (circle->position < 0) circle->position += 4096;
	}
	if (dir > 0) circle->position = (circle->position + 1024) % 4096;
}

int main(int argc, char *argv[]) {
	int i, t = 0;
	uint32 done = 0;
	char ch, benchmark = 0;
	hd_engine *engine;
	circle_object obj[5];

	for (i = 1; i < argc; i++)
		if (*argv[i] == '-')
			if (strcmp("benchmark", argv[i]+1) == 0)
				benchmark = 1;

#ifndef IPOD
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr,"Unable to init SDL: %s\n",SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);

	screen = SDL_SetVideoMode(WIDTH,HEIGHT,16,SDL_SWSURFACE);
	if (screen == NULL) {
		fprintf(stderr,"Unable to init SDL video: %s\n",SDL_GetError());
		exit(1);
	}
	engine = HD_Initialize (WIDTH, HEIGHT, 16, screen->pixels, update);
#define IMGPREFIX ""
#else

	screen = malloc (WIDTH * HEIGHT * 2);
	engine = HD_Initialize (WIDTH, HEIGHT, 16, screen, update);
#define IMGPREFIX "/mnt/"
#endif

	obj[4].object    = HD_PNG_Create (IMGPREFIX "bg.png");
	obj[4].object->x = 0;
	obj[4].object->y = 0;
	obj[4].object->w = 220;
	obj[4].object->h = 176;

	obj[0].object    = HD_PNG_Create (IMGPREFIX "photos.png");
	obj[0].object->x = 0;
	obj[0].object->y = 0;
	obj[0].object->w = 75;
	obj[0].object->h = 150;

	obj[1].object    = HD_PNG_Create (IMGPREFIX "music.png");
	obj[1].object->x = 0;
	obj[1].object->y = 0;
	obj[1].object->w = 75;
	obj[1].object->h = 150;

	obj[2].object    = HD_PNG_Create (IMGPREFIX "dvd.png");
	obj[2].object->x = 0;
	obj[2].object->y = 0;
	obj[2].object->w = 75;
	obj[2].object->h = 150;

	obj[3].object    = HD_PNG_Create (IMGPREFIX "movies.png");
	obj[3].object->x = 0;
	obj[3].object->y = 0;
	obj[3].object->w = 75;
	obj[3].object->h = 150;

	HD_Register(engine,obj[4].object);
	HD_Register(engine,obj[0].object);
	HD_Register(engine,obj[1].object);
	HD_Register(engine,obj[2].object);
	HD_Register(engine,obj[3].object);

	obj[0].position = 0;
	obj[1].position = 1024;
	obj[2].position = 2048;
	obj[3].position = 3072;

	if (benchmark) {
		HD_AnimateCircle(obj[0].object, 80, 50, 50, (50 << 16) /
				obj[0].object->w, (70 << 16) / obj[0].object->w,
				obj[0].position, 4096, -100);
		HD_AnimateCircle(obj[1].object, 80, 50, 50, (50 << 16) /
				obj[1].object->w, (70 << 16) / obj[1].object->w,
				obj[1].position, 4096, -100);
		HD_AnimateCircle(obj[2].object, 80, 50, 50, (50 << 16) /
				obj[2].object->w, (70 << 16) / obj[2].object->w,
				obj[2].position, 4096, -100);
		HD_AnimateCircle(obj[3].object, 80, 50, 50, (50 << 16) /
				obj[3].object->w, (70 << 16) / obj[3].object->w,
				obj[3].position, 4096, -100);
	}
	else {
		circle_rotate(&obj[0], 1);
		circle_rotate(&obj[1], 1);
		circle_rotate(&obj[2], 1);
		circle_rotate(&obj[3], 1);
	}

#ifdef IPOD
	uint32 srtc = *(volatile uint32 *)0x60005010;

	set_keypress();
	fd_set rd;
	struct timeval tv;
	int n;

#endif

	while(!done) {
		if (!benchmark && (ch = check_pending()) &&
				!obj[0].object->animating) {
			circle_rotate(&obj[0], ch);
			circle_rotate(&obj[1], ch);
			circle_rotate(&obj[2], ch);
			circle_rotate(&obj[3], ch);
		}
#ifndef IPOD
		SDL_Event event;

		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym) {
				case SDLK_SPACE:
				case SDLK_BACKSPACE:
				case SDLK_DOWN:
				case SDLK_UP:
					break;
				case SDLK_LEFT:
					if (benchmark) break;
					if (obj[0].object->animating) {
						add_pending(1);
						break;
					}
					circle_rotate(&obj[0], 1);
					circle_rotate(&obj[1], 1);
					circle_rotate(&obj[2], 1);
					circle_rotate(&obj[3], 1);
					break;
				case SDLK_RIGHT:
					if (benchmark) break;
					if (obj[0].object->animating) {
						add_pending(-1);
						break;
					}
					circle_rotate(&obj[0], -1);
					circle_rotate(&obj[1], -1);
					circle_rotate(&obj[2], -1);
					circle_rotate(&obj[3], -1);
					break;
				case SDLK_ESCAPE:
					done = 1;
					break;
				default:
					break;
				}
				break;
			case SDL_QUIT:
				return(0);
				break;
			default:
				break;
			}
		}

		SDL_Delay (30);
#else
		FD_ZERO(&rd);
		FD_SET(0, &rd);

		tv.tv_sec = 0;
		tv.tv_usec = 100;

		n = select(0+1, &rd, NULL, NULL, &tv);
		if (FD_ISSET(0, &rd) && (n > 0)) {
			read(0, &ch, 1);
			switch(ch) {
			case 'm':
				done = 1;
				break;
			case 'r':
				if (benchmark) break;
				if (obj[0].object->animating) {
					add_pending(1);
					break;
				}
				circle_rotate(&obj[0], 1);
				circle_rotate(&obj[1], 1);
				circle_rotate(&obj[2], 1);
				circle_rotate(&obj[3], 1);
				break;
			case 'l':
				if (benchmark) break;
				if (obj[0].object->animating) {
					add_pending(-1);
					break;
				}
				circle_rotate(&obj[0], -1);
				circle_rotate(&obj[1], -1);
				circle_rotate(&obj[2], -1);
				circle_rotate(&obj[3], -1);
				break;
			case 'w':
			case 'f':
			case 'd':
			default:
				break;
			}
		}
#endif
		t++;
		if (benchmark && t > 200)
			done = 1;

		HD_Animate (engine);

#ifndef IPOD
		if( SDL_MUSTLOCK(screen) )
			SDL_LockSurface(screen);
#endif

		HD_Render(engine);

#ifndef IPOD
		if( SDL_MUSTLOCK(screen) )
			SDL_UnlockSurface(screen);
#endif
	}
#ifdef IPOD
	uint32 ertc = *(volatile uint32 *)0x60005010;
	printf ("%d frames in %d microseconds = %d.%02d frames/sec\n",
		t, ertc - srtc, 1000000 * t / (ertc - srtc),
		(1000000 * t / ((ertc - srtc) / 100)) % 100);
	sleep (5);
	reset_keypress();
#endif
	return(0);
}
