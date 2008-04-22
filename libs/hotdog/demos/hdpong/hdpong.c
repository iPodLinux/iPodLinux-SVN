/*
 * Copyright (C) 2004, 2005 Courtney Cavin, Alastair Stuart
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
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "hotdog.h"

#ifdef IPOD
static uint32 WIDTH, HEIGHT;
uint32 *screen;
#else
#include "SDL.h"
SDL_Surface *screen;
#define WIDTH 220
#define HEIGHT 176
#endif

#define RBEGIN  0
#define INGAME  1
#define GOVER   2
#define BATW   12
#define BATH   48
#define BALLD  10
#define BALLR  BALLD/2
#define BATMV  3

#define ROYAL_BLUE \
	HD_RGBA(65, 105, 225, 200)
#define ORANGE_BOLD \
	HD_RGBA(255, 165, 0, 250)
#define RED_BOLD \
	HD_RGBA(255, 0, 0, 250)

struct velocity {
	double x;
	double y;
};

static inline int constrain(int min, int max, int val)
{
	if (val > max) val = max;
	if (val < min) val = min;
	return (val);
}

static void update (hd_engine *eng, int x, int y, int w, int h) 
{
#ifdef IPOD
	HD_LCD_Update (eng->screen.framebuffer, 0, 0, WIDTH, HEIGHT);
#else
	SDL_UpdateRect (SDL_GetVideoSurface(), x, y, w, h);
#endif
}

#ifdef IPOD
#include <termios.h> 
#include <sys/time.h>

static struct termios stored_settings; 

static void set_keypress()
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

static void reset_keypress()
{
	tcsetattr(0,TCSANOW,&stored_settings);
}

#endif

int main(int argc, char *argv[]) {
	int done = 0;
	int batmv = 0;
	int state = RBEGIN;
	char score[256] = "COM: 0 | PLAYER: 0";
	struct velocity vball, vlbat, vrbat;

	hd_engine *engine;
	hd_object *bg, *lbat, *rbat, *ball;
	hd_font *f;
	hd_surface srf;
	hd_object *lscore;
	
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
	engine = HD_Initialize(WIDTH,HEIGHT,16,screen->pixels,update);
#define IMGPREFIX ""
#else
	HD_LCD_Init();
	HD_LCD_GetInfo (0, &WIDTH, &HEIGHT, 0);
	screen = xmalloc (WIDTH * HEIGHT * 2);
	engine = HD_Initialize (WIDTH, HEIGHT, 16, screen, update);
#define IMGPREFIX ""
#endif
	
	bg = HD_PNG_Create(IMGPREFIX "bg.png");
	bg->x = 0;
	bg->y = 0;
	bg->w = WIDTH;
	bg->h = HEIGHT;
	bg->z = 3;
	
	lbat = HD_PNG_Create(IMGPREFIX "bat.png");
	lbat->x = 0;
	lbat->y = 0;
	lbat->w = BATW;
	lbat->h = BATH;
	lbat->z = 2;
	
	rbat = HD_PNG_Create(IMGPREFIX "bat.png");
	rbat->x = WIDTH-BATW;
	rbat->y = HEIGHT/2-BATH/2;
	rbat->w = BATW;
	rbat->h = BATH;
	rbat->z = 2;

	ball = HD_PNG_Create(IMGPREFIX "ball.png");
	ball->x = WIDTH/2-BALLR;
	ball->y = HEIGHT/2-BALLR;
	ball->w = ball->h = BALLD;
	ball->z = 1;

	f = HD_Font_LoadSFont ("Aiken14.png");
	lscore = HD_Canvas_Create (WIDTH, HEIGHT);
	lscore->x = 0;
	lscore->y = 0;
	lscore->z = 2;
	srf = lscore->canvas;
	HD_Font_Draw (srf, f, WIDTH/2-60, // Roughly centred
		BATW, ROYAL_BLUE, score);
	HD_Font_Draw (srf, f, WIDTH/2-25, // Roughly centred
		BATW*2+5, ROYAL_BLUE, "hd-pong");

	HD_Register(engine,bg);
	HD_Register(engine,lbat);
	HD_Register(engine,rbat);
	HD_Register(engine,ball);
	HD_Register(engine,lscore);
	

#ifdef IPOD
	set_keypress();
	fd_set rd;
	struct timeval tv;
	int n;
	char ch;
#endif
	done = 0;
	while(!done) {
		static int turn;
		static int inc = 0;
		static int rev_timestep = 25;
		static int comppoint = 0, userpoint = 0;
		int mv_amount = 0;
		
#ifndef IPOD
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym) {
					case SDLK_DOWN:
						batmv = 1;
						break;
					case SDLK_UP:
						batmv = -1;
						break;
					case SDLK_ESCAPE:
						done = 1;
					default:
						break;
				}
				break;
			case SDL_KEYUP:
				switch(event.key.keysym.sym) {
					case SDLK_SPACE:
						if (state==RBEGIN) state = INGAME;
						break;
					case SDLK_DOWN:
					case SDLK_UP:
						batmv = 0;
						break;
					case SDLK_LEFT:
						break;
					case SDLK_RIGHT:
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
#else

#define SCROLL_MOD(n) \
  ({ \
    static int sofar = 0; \
    int use = 0; \
    if (++sofar >= n) { \
      sofar -= n; \
      use = 1; \
    } \
    (use == 1); \
  })
  		for (;;) {
			FD_ZERO(&rd);
			FD_SET(0, &rd);
			tv.tv_sec = 0;
			tv.tv_usec = 100;
			n = select(0+1, &rd, NULL, NULL, &tv);
			if (!FD_ISSET(0, &rd) || (n <= 0))
				break;
			read(0, &ch, 1);
			switch(ch) {
			case 'r':
				if (SCROLL_MOD(4))
					batmv = 1;
				break;
			case 'l':
				if (SCROLL_MOD(4))
					batmv = -1;
				break;
			case 'h':
			case 'm':
				done = 1;
				break;
			case 'f': // Start game
			case 'w':
			case 'd':
				if (state==RBEGIN) state = INGAME;
				break;
			default:
				break;
			}
		}
#endif

		switch(state) {
		case RBEGIN:
		/* initals for the round */
		ball->x = WIDTH/2-BALLR;
		ball->y = HEIGHT/2-BALLR;
		vball.x = (turn == 1) ? -60 : 60;
		vball.y = 72;
		
		lbat->y = rbat->y = HEIGHT/2-BATH/2;
		vlbat.y = vrbat.y = 0;
		
		turn = 0;
		break;
		case INGAME:
		if (ball->x > (BATW/2) && ball->x + BALLR <= WIDTH - (BATW/2)) {
			/* update ball position */
			ball->x += vball.x / rev_timestep;
			ball->y += vball.y / rev_timestep;
			ball->y = constrain(0, HEIGHT-BALLD, ball->y);
			
			if (++inc > 128) {
				rev_timestep -= 1;
				inc = 0;
				if (rev_timestep < 1)
					rev_timestep = 1;
			}
			
			/* check for playable walls */
			if ((ball->y <= 0 && vball.y < 0)
					|| (ball->y+BALLD >= HEIGHT && vball.y > 0))
				vball.y *= -1;
	
			/* check for interaction with player paddle ( two ifs ) */
			if (vball.x < 0 && ball->x >= BATW/2
					&& ball->x <= BATW &&
					ball->y <= lbat->y + BATH &&
					ball->y + BALLD >= lbat->y) {
				vlbat.y = ball->y + BALLR - vlbat.y;
				if (vball.y < 0)
					vball.y += (vlbat.y - 6) / 3;
				vball.x *= -1;
			}
			else if (vball.x > 0 && ball->x + BALLD <= WIDTH - BATW/2
					&& ball->x + BALLD >= WIDTH - BATW &&
					ball->y <= rbat->y + BATH &&
					ball->y + BALLD >= rbat->y) {
				vrbat.y = ball->y + BALLR - rbat->y;
				if(vball.y < 0)
					vball.y += (vrbat.y - 6) / 3;
				vball.x *= -1;
			}
	
			/* lbat player AI (not really, just follows the ball) */
			if (ball->x < WIDTH / 2 && vball.x < 0) {
				mv_amount = ball->y + BALLR - (lbat->y + BATH/2);
				mv_amount = (mv_amount > 0) ?
					((mv_amount > 6) ? 6 : mv_amount) :
					((mv_amount < -6) ? -6 : mv_amount);
			}
			else {
				mv_amount = (HEIGHT/2) - (lbat->y - BATH/2);
				mv_amount = (mv_amount > 0) ?
					((mv_amount > 3) ? 3 : 1) :
					((mv_amount < -3) ? -3 : mv_amount);
			}
			
			if (mv_amount != 0) {
				lbat->y += mv_amount;
				lbat->y = constrain(0, HEIGHT-BATH, lbat->y);
			}
			
			/* update rbat position */
			if (batmv != 0) {
				rbat->y += batmv * BATMV;
				rbat->y = constrain(0, HEIGHT-BATH, rbat->y);
#ifdef IPOD
				batmv = 0;
#endif
			}
		} else {
			if(ball->x < BATW/2) {
				userpoint++;
				inc += 32;
				turn = 2;
			}
			else if(ball->x + BALLD > WIDTH - (BALLD)) {
				comppoint++;
				rev_timestep += 2;
				turn = 1;
			}
			state = RBEGIN;

			snprintf(score, 256, "COM: %d | PLAYER: %d", comppoint, userpoint);
			if (comppoint == 10 || userpoint == 10) {
				state = GOVER;
			} else {
				HD_FillRect (srf, 0,0, WIDTH, HEIGHT, HD_CLEAR);
				HD_Font_Draw (srf, f, WIDTH/2-60, // Roughly centred
					BATW, ROYAL_BLUE, score);
				HD_Font_Draw (srf, f, WIDTH/2-25, // Roughly centred
					BATW*2+5, ROYAL_BLUE, "hd-pong");
			}
		}

		break;
		case GOVER:
		if (comppoint == 10) {
			HD_FillRect (srf, 0,0, WIDTH, HEIGHT, HD_CLEAR);
			HD_Font_Draw (srf, f, WIDTH/2-65, // Roughly centred
				BATW, ROYAL_BLUE, score);
			HD_Font_Draw (srf, f, WIDTH/2-30, // Roughly centred
				BATW*2+5, RED_BOLD, "You lose!");
		} else if (userpoint == 10) {
			HD_FillRect (srf, 0,0, WIDTH, HEIGHT, HD_CLEAR);
			HD_Font_Draw (srf, f, WIDTH/2-65, // Roughly centred
				BATW, ROYAL_BLUE, score);
			HD_Font_Draw (srf, f, WIDTH/2-30, // Roughly centred
				BATW*2+5, ORANGE_BOLD, "You win!");
		}
		state = RBEGIN;
		comppoint = userpoint = 0;
		break;
		}

#ifndef IPOD
		if( SDL_MUSTLOCK(screen) ) 
			SDL_LockSurface(screen);
#endif

		HD_Render(engine);

#ifndef IPOD
		if( SDL_MUSTLOCK(screen) ) 
			SDL_UnlockSurface(screen);
#endif
		usleep(10000);
	}

	HD_Destroy(bg);
	HD_Destroy(lbat);
	HD_Destroy(rbat);
	HD_Destroy(ball);
	free(engine->buffer);
	free(engine);
#ifdef IPOD
	HD_LCD_Quit();
	reset_keypress();
#endif
	return(0);
}
