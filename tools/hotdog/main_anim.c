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

#include "SDL.h"

#define WIDTH  320
#define HEIGHT 240

#ifndef IPOD
SDL_Surface *screen;

uint32 GetTimeMillis(void) {
  uint32 millis;

  millis = SDL_GetTicks();

  return(millis);
}
#else
uint32 *screen;
#endif

#ifdef IPOD
extern void _HD_ARM_Update5G (uint16 *fb, int x, int y, int w, int h);
static void update (hd_engine *eng, int x, int y, int w, int h)
{
    printf ("Updating\n");
    _HD_ARM_Update5G (eng->screen.framebuffer, x, y, w, h);
    printf ("done\n");
    exit (0);
}
#else
static void update (hd_engine *eng, int x, int y, int w, int h) 
{
    SDL_UpdateRect (SDL_GetVideoSurface(), x, y, w, h);
}
#endif

int main(int argc, char *argv[]) {
  uint32  done = 0;
	uint16* pFB=0;
	hd_engine *engine;
	hd_object *obj[5];
	double f = 0.0;

#ifndef IPOD
  if( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
    fprintf(stderr,"Unable to init SDL: %s\n",SDL_GetError());
    exit(1);
  }
  atexit(SDL_Quit);
  
  screen = SDL_SetVideoMode(WIDTH,HEIGHT,16,SDL_SWSURFACE);
  if(screen == NULL) {
    fprintf(stderr,"Unable to init SDL video: %s\n",SDL_GetError());
    exit(1);
  }
  engine = HD_Initialize (WIDTH, HEIGHT, 16, screen->pixels, update);
#else
  screen = malloc (WIDTH * HEIGHT * 2);
  engine = HD_Initialize (WIDTH, HEIGHT, 16, screen, update);
#endif
  
        obj[4]    = HD_PNG_Create ("bg.png");
        obj[4]->x = 0;
        obj[4]->y = 0;
        obj[4]->w = 220;
        obj[4]->h = 176;

        obj[0]    = HD_PNG_Create ("photos.png");
        obj[0]->x = 0;
        obj[0]->y = 0;
        obj[0]->w = 75;
        obj[0]->h = 150;

        obj[1]    = HD_PNG_Create ("music.png");
        obj[1]->x = 0;
        obj[1]->y = 0;
        obj[1]->w = 75;
        obj[1]->h = 150;

        obj[2]    = HD_PNG_Create ("dvd.png");
        obj[2]->x = 0;
        obj[2]->y = 0;
        obj[2]->w = 75;
        obj[2]->h = 150;

        obj[3]    = HD_PNG_Create ("movies.png");
        obj[3]->x = 0;
        obj[3]->y = 0;
        obj[3]->w = 75;
        obj[3]->h = 150;

	HD_Register(engine,obj[4]);
	HD_Register(engine,obj[0]);
	HD_Register(engine,obj[1]);
	HD_Register(engine,obj[2]);
	HD_Register(engine,obj[3]);

        HD_AnimateCircle (obj[0], 80, 50, 50, (50 << 16) / obj[0]->w, (70 << 16) / obj[0]->w,    0, 4096, -100);
        HD_AnimateCircle (obj[1], 80, 50, 50, (50 << 16) / obj[1]->w, (70 << 16) / obj[1]->w, 1024, 4096, -100);
        HD_AnimateCircle (obj[2], 80, 50, 50, (50 << 16) / obj[2]->w, (70 << 16) / obj[2]->w, 2048, 4096, -100);
        HD_AnimateCircle (obj[3], 80, 50, 50, (50 << 16) / obj[3]->w, (70 << 16) / obj[3]->w, 3072, 4096, -100);
        HD_StopAnimation (obj[2]);

  while(!done) {
#ifndef IPOD
    SDL_Event event;

    while(SDL_PollEvent(&event)) {
      switch(event.type) {
      case SDL_KEYDOWN:
	  switch(event.key.keysym.sym) {
	  case SDLK_SPACE:
	    break;
	  case SDLK_BACKSPACE:
	    break;
	  case SDLK_DOWN:
	    break;
	  case SDLK_UP:
	    break;
	  case SDLK_LEFT:
	    break;
	  case SDLK_RIGHT:
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
#endif
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

  return(0);
}
