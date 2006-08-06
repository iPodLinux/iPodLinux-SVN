#include <stdlib.h>
#include <unistd.h>
#include "hotdog.h"

#ifdef IPOD
uint32 *screen;

static uint32 WIDTH, HEIGHT;
#else
#include "SDL.h"
SDL_Surface *screen;

#define WIDTH 220
#define HEIGHT 176
#endif

hd_engine *engine;

static void update(hd_engine *e, int x, int y, int w, int h)
{
#ifdef IPOD
	HD_LCD_Update (e->screen.framebuffer, 0, 0, WIDTH, HEIGHT);
#else
	SDL_UpdateRect(SDL_GetVideoSurface(), x, y, w, h);
#endif
}

int main(int argc, char **argv)
{
	char eop = 0;
	hd_surface srf;
	hd_object *obj;
#ifndef IPOD
#define IMGPREFIX ""
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr,"Unable to init SDL: %s\n",SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);
	screen = SDL_SetVideoMode(WIDTH, HEIGHT,16,SDL_SWSURFACE);
	if (screen == NULL) {
		fprintf(stderr,"Unable to init SDL video: %s\n",SDL_GetError());
		exit(1);
	}
	engine = HD_Initialize (WIDTH, HEIGHT, 16, screen->pixels, update);
#else
#define IMGPREFIX "/mnt/"
	HD_LCD_Init();
	HD_LCD_GetInfo(0, &WIDTH, &HEIGHT, 0);
	screen = malloc(WIDTH * HEIGHT * 2);
	engine = HD_Initialize(WIDTH, HEIGHT, 16, screen, update);
#endif
#

	if (!access(IMGPREFIX "bg2.png", R_OK))
		obj = HD_PNG_Create(IMGPREFIX "bg2.png");
	else {
		obj = HD_New_Object();
		obj->type = HD_TYPE_CANVAS;
		obj->canvas = HD_NewSurface(WIDTH, HEIGHT);
		obj->natw = WIDTH;
		obj->nath = HEIGHT;
	}
	obj->x = 0;
	obj->y = 0;
	obj->w = WIDTH;
	obj->h = HEIGHT;
	obj->render = HD_Canvas_Render;
        obj->destroy = HD_Canvas_Destroy;
	HD_Register(engine, obj);
#define PREM(a) (HD_RGBA(((a) & 0x00ff0000) >> 16, \
			 ((a) & 0x0000ff00) >> 8,  \
			 ((a) & 0x000000ff),       \
			 ((a) & 0xff000000) >> 24))

	srf = obj->canvas;
	HD_Rect(srf, WIDTH/4, HEIGHT/3, WIDTH/2, HEIGHT/2, PREM(0xff808080));
	HD_AALine(srf, 0, 0, WIDTH/2, HEIGHT/2, PREM(0xffff0000));
	HD_Line(srf, WIDTH/2, HEIGHT/2, WIDTH/2, HEIGHT, PREM(0xffff0000));
	HD_AAFillCircle(srf, WIDTH/4, HEIGHT/4, WIDTH/6, PREM(0xd0ff00ff));
	HD_FillRect(srf, WIDTH/4 + 10, HEIGHT/4, WIDTH/2+WIDTH/4,
			HEIGHT/2+HEIGHT/4, PREM(0xd000ff00));
	HD_Circle(srf, WIDTH/2, HEIGHT/2, WIDTH/5, PREM(0xff0000ff));
	HD_AAEllipse(srf, WIDTH/2, HEIGHT/2, WIDTH/6, HEIGHT/2, PREM(0xffffff00));
	HD_FillEllipse(srf, WIDTH/4, HEIGHT-HEIGHT/3, WIDTH/12, HEIGHT/6,
			PREM(0x8000ffff));
	{
		hd_point lines[] = {
			{ 4, 4}, {10, 4}, {10,20}, { 4,10},
			{ 4,22}, {10,22}, {10,38}, { 4,28}
		};
		HD_Lines(srf, lines, 4, 0xff0000ff);
		HD_AAPoly(srf, lines + 4, 4, 0xff00ff00);
	}
	{
		unsigned short bits[] = {
			0x3800, // ..## #... .... ....
			0x4700, // .#.. .### .... ....
			0x38e0, // ..## #... ###. ....
			0x471c, // .#.. .### ...# ##..
			0x38e2, // ..## #... ###. ..#.
			0x471c, // .#.. .### ...# ##..
			0x38e0, // ..## #... ###. ....
			0x4700, // .#.. .### .... ....
			0x3800};// ..## #... .... ....
		HD_Bitmap(srf, WIDTH - WIDTH/4, 0, 16, 9, bits, 0xffff0000);
	}
	{
		hd_point fre[] = {
			{0,0}, {0,HEIGHT}, {WIDTH,0}, {WIDTH,HEIGHT}
		};
		HD_AABezier(srf, 3, fre, 6, 0xffffffff);
	}
	HD_Blur(srf, 0, HEIGHT/2, WIDTH, 24, 5);
	if (!access("Aiken14.png", R_OK)) {
		hd_font *font = HD_Font_LoadSFont("Aiken14.png");
		HD_Font_Draw(srf, font, 4, HEIGHT-15, PREM(0xfeaa66ff),
				"This is an SFont.");
	}
	if (!access("6x13.fff", R_OK)) {
		hd_font *font = HD_Font_LoadFFF("6x13.fff");
		HD_Font_Draw(srf, font, 20, 15, 0xffffffff, "This is a FFF.");
	}

	while (!eop) {
#ifndef IPOD
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
				case SDLK_ESCAPE:
					eop = 1;
					break;
				default: break;
				}
				break;
			case SDL_QUIT:
				eop = 1;
				break;
			}
		}
		SDL_Delay(30);

#endif
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
	return 0;
	
}
