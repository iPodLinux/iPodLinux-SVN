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

#define YOFF 45
#define XRADIUS 60
#define YRADIUS 40

#if defined(NANO)
#define LCD_TYPE 2
#define WIDTH   176
#define HEIGHT  132
#define SWIDTH  176
#define SHEIGHT 132
#elif defined(PHOTO) || defined(COLOR)
#ifdef PHOTO
#define LCD_TYPE 0
#else
#define LCD_TYPE 1
#endif
#define WIDTH   220
#define HEIGHT  176
#define SWIDTH  220
#define SHEIGHT 176
#else /* 5g */
/* width/height of the region we draw */
#define WIDTH   320
#define HEIGHT  240
/* width/height of the screen */
#define SWIDTH  320
#define SHEIGHT 240
#endif

static uint32 object_topwid, object_bottomwid;
static uint32 rotation_angle, rotation_frames;

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
	new_settings.c_lflag &= ~(ICANON | ECHO | ISIG);
	new_settings.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
	new_settings.c_cc[VTIME] = 0;
	tcgetattr(0,&stored_settings);
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0,TCSANOW,&new_settings);
}

void reset_keypress(void)
{
	tcsetattr(0,TCSANOW,&stored_settings);
}

static void update (hd_engine *eng, int x, int y, int w, int h) 
{
	HD_LCD_Update (eng->screen.framebuffer, 0, 0, SWIDTH, SHEIGHT);
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
	char *name;
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

int zorder[4] = { 2, 1, 2, 3 };

static void circle_rotate(circle_object *circle, int dir)
{
	circle->object->w = 75 * WIDTH / 220;
	circle->object->h = 150 * WIDTH / 220;
	HD_XAnimateCircle(circle->object, 70 * WIDTH / 220, YOFF * HEIGHT / 176,
			  XRADIUS * WIDTH / 220, YRADIUS * HEIGHT / 176,
			  object_topwid, object_bottomwid, (circle->position + 1024) & 0xfff,
			  dir*rotation_angle, rotation_frames);
	if (dir < 0) {
		circle->position = (circle->position - rotation_angle);
		if (circle->position < 0) circle->position += 4096;
	}
	if (dir > 0) circle->position = (circle->position + rotation_angle) & 0xfff;
}

int main(int argc, char *argv[]) {
	int i, t = 0;
	uint32 done = 0;
	char ch, benchmark = 0, noinput = 0;
	hd_engine *engine;
	circle_object *obj, *objp;
	hd_object *bgobj;
	int nobj = 0;
	const char *objlist = "ildr", *p;

	for (i = 1; i < argc; i++) {
		if (*argv[i] == '-') {
			if (strcmp("benchmark", argv[i]+1) == 0)
				benchmark = 1;
			if (strcmp("noinput", argv[i]+1) == 0)
				noinput = 1;
		} else {
			objlist = argv[i];
		}
	}

	noinput = noinput && benchmark;

#ifndef IPOD
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr,"Unable to init SDL: %s\n",SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);

	screen = SDL_SetVideoMode(SWIDTH, SHEIGHT,16,SDL_SWSURFACE);
	if (screen == NULL) {
		fprintf(stderr,"Unable to init SDL video: %s\n",SDL_GetError());
		exit(1);
	}
	engine = HD_Initialize (SWIDTH, SHEIGHT, 16, screen->pixels, update);
#define IMGPREFIX ""
#else

	screen = malloc (SWIDTH * SHEIGHT * 2);
	engine = HD_Initialize (SWIDTH, SHEIGHT, 16, screen, update);
	HD_LCD_Init();
#define IMGPREFIX "/mnt/"
#endif

	obj = objp = calloc (sizeof(circle_object), strlen (objlist) + 1);
	for (p = objlist; *p; p++) {
		int ok = 1;
		switch (*p) {
		case 'i':
			objp->object = HD_PNG_Create (IMGPREFIX "retailos.png");
			break;
		case 'l':
			objp->object = HD_PNG_Create (IMGPREFIX "ipl.png");
			break;
		case 'r':
			objp->object = HD_PNG_Create (IMGPREFIX "rockbox.png");
			break;
		case 'd':
			objp->object = HD_PNG_Create (IMGPREFIX "diskmode.png");
			break;
		default:
			ok = 0;
			break;
		}
		if (!ok) continue;
		
		objp->object->x = 0;
		objp->object->y = 0;
		objp->object->w = 75 * WIDTH / 220;
		objp->object->h = 150 * WIDTH / 220;
		objp++;
		nobj++;
	}
	objp++->object = 0;

	bgobj = HD_PNG_Create ("bg.png");
	bgobj->x = bgobj->y = 0;
	bgobj->w = WIDTH;
	bgobj->h = HEIGHT;

	rotation_angle = 4096 / nobj;
	rotation_frames = 100 / nobj;

	HD_Register(engine,bgobj);
	for (i = 0, objp = obj; objp->object; objp++, i++) {
		objp->position = i * rotation_angle;
		HD_Register(engine,objp->object);
	}

	object_topwid = ((20 * WIDTH / 220) << 16) / obj[0].object->w;
	object_bottomwid = ((70 * WIDTH / 220) << 16) / obj[0].object->w;

	if (benchmark) {
		for (objp = obj; objp->object; objp++) {
			HD_XAnimateCircle(objp->object, 80 * WIDTH / 220, YOFF * HEIGHT / 176,
					  XRADIUS * WIDTH / 220, YRADIUS * HEIGHT / 176,
					  object_topwid, object_bottomwid,
					  (objp->position + 1024) & 0xfff, 4096, -100);
		}
	} else {
		for (objp = obj; objp->object; objp++) {
			circle_rotate (objp, 0);
		}
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
			for (objp = obj; objp->object; objp++)
				circle_rotate (objp, ch);
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
					for (objp = obj; objp->object; objp++)
						circle_rotate (objp, 1);
					break;
				case SDLK_RIGHT:
					if (benchmark) break;
					if (obj[0].object->animating) {
						add_pending(-1);
						break;
					}
					for (objp = obj; objp->object; objp++)
						circle_rotate (objp, -1);
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
		if (!noinput) {
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
	HD_LCD_Quit();
	uint32 ertc = *(volatile uint32 *)0x60005010;
	printf ("%d frames in %d microseconds = %d.%02d frames/sec\n",
		t, ertc - srtc, 1000000 * t / (ertc - srtc),
		(1000000 * t / ((ertc - srtc) / 100)) % 100);
	sleep (5);
	reset_keypress();
#endif
	return(0);
}

/*
 * Local Variables:
 * indent-tabs-mode: t
 * c-basic-offset: 8
 * End:
 */

