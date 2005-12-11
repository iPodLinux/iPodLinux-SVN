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
#include "hotdog_png.h"
/* #include "hotdog_font.h" */

#include "SDL.h"

#define RBEGIN  0
#define INGAME  1
#define GOVER   2

#define WIDTH  220
#define HEIGHT 176
#define BATW   12
#define BATH   48
#define BALLD  10
#define BALLR  BALLD/2
#define BATMV  3

SDL_Surface *screen;
uint16      *renderBuffer;

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
    SDL_UpdateRect (SDL_GetVideoSurface(), x, y, w, h);
}

int main(int argc, char *argv[]) {
	int done = 0;
	int batmv = 0;
	int state = RBEGIN;
	struct velocity vball, vlbat, vrbat;

	hd_engine *engine;
	hd_object *bg, *lbat, *rbat, *ball;
	/* hd_object lscore; */
	
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
  
	renderBuffer = (uint16 *)malloc(WIDTH*HEIGHT*2);
	assert(renderBuffer != NULL);

	engine = HD_Initialize(WIDTH,HEIGHT,16,screen->pixels,update);
	
	bg = HD_PNG_Create("bg.png");
	bg->x = 0;
	bg->y = 0;
	bg->w = WIDTH;
	bg->h = HEIGHT;
	bg->z = 3;
	bg->type = HD_TYPE_PNG;
	
	lbat = HD_PNG_Create("bat.png");
	lbat->x = 0;
	lbat->y = 0;
	lbat->w = BATW;
	lbat->h = BATH;
	lbat->z = 2;
	lbat->type = HD_TYPE_PNG;
	
	rbat = HD_PNG_Create("bat.png");
	rbat->x = WIDTH-BATW;
	rbat->y = HEIGHT/2-BATH/2;
	rbat->w = BATW;
	rbat->h = BATH;
	rbat->z = 2;
	rbat->type = HD_TYPE_PNG;

	ball = HD_PNG_Create("ball.png");
	ball->x = WIDTH/2-BALLR;
	ball->y = HEIGHT/2-BALLR;
	ball->w = ball->h = BALLD;
	ball->z = 1;
	ball->type = HD_TYPE_PNG;

/*
	lscore.y = 2;
	lscore.h = 40;
	lscore.z = 2;
	lscore.type = HD_TYPE_FONT;
	lscore.sub.font  = HD_Font_Create("../font.ttf", lscore.h, "0");
	lscore.w = lscore.sub.font->w;
	lscore.x = (WIDTH/3)-(lscore.w/2);
	for(done=0;done<(lscore.h*lscore.w);done++)
		lscore.sub.font->argb[done] &|= 0x0000FF00;
*/

	HD_Register(engine,bg);
	HD_Register(engine,lbat);
/*
	HD_Register(engine,&lscore);
*/
	HD_Register(engine,rbat);
	HD_Register(engine,ball);

	done = 0;
	while(!done) {
		SDL_Event event;
		static int turn;
		static int inc = 0;
		static int rev_timestep = 25;
		static int comppoint = 0, userpoint = 0;
		int mv_amount = 0;
		
		
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
			printf("COM: %d | PLAYER: %d\n", comppoint, userpoint);
			
			if (comppoint == 10 || userpoint == 10) state = GOVER;
		}

		break;
		case GOVER:
		if (comppoint == 10) {
			printf("You lose!\n");
		} else if (userpoint == 10) {
			printf("You win!\n");
		}
		state = RBEGIN;
		comppoint = userpoint = 0;
		break;
		}

		if( SDL_MUSTLOCK(screen) ) 
			SDL_LockSurface(screen);

		HD_Render(engine);

		if( SDL_MUSTLOCK(screen) ) 
			SDL_UnlockSurface(screen);
		usleep(10000);
	}

	return(0);
}
