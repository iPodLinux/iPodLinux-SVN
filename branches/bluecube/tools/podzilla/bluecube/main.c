/*
 * BlueCube - just another tetris clone 
 * Copyright (C) 2003  Sebastian Falbesoner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "global.h"
#include "grafix.h"
#include "sound.h"
#include "particle.h"
#include "font.h"
#include "box.h"
#include "credits.h"

extern SDL_Surface* screen;

Uint32 TimeLeft(void);
static void NewGame(void);
static void MainMenu_Loop(void);
static void Game_Loop(void);
static void DrawScene(void);
void StartGameOverAnimation(void);
static void GameOverAnimation(void);

SDL_Event event;

int lines, score;       /* Line counter and score */
int nextPiece;          /* Next cluster  */
int level;              /* Current level */

int zustand;     /* Current state */
int bDone=0;     /* Want to Exit? */
int bPause;      /* Pause? */
int bCrazy;      /* Yahooooooooooo ;) CRAZY MODE ^.^ */
int bGameOver;   /* GameOver Animation? */
int bExplode;    /* Explosion is active? */
int x,y;         /* Current explosion coordinates */

/*=========================================================================
// Name: main()
// Desc: Entrypoint
//=======================================================================*/
int main(int argc, char *argv[])
{
	/* Init randomizer */
	srand(time(NULL));
	
	/* Let's init the graphics and sound system */
	InitSDLex();
	InitSound();
	
	/* Load font */
	font = LoadFontfile("font/font.dat", "font/widths.dat");
	/* Load soundfiles */
	LoadSound("sound/killline.snd", &sndLine);
	LoadSound("sound/nextlev.snd",  &sndNextlevel);

	/* Init star background */
	InitStars();

	ClearPlayingSounds();
	SDL_PauseAudio(0);

	zustand = STATE_MENU;


	while (!bDone) /* Mainloop */
	{
		switch (zustand)
		{
		case STATE_MENU:
			MainMenu_Loop();
			SDL_Delay(TimeLeft());
			break;

		case STATE_PLAY:
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_QUIT:
					bDone = 1;
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym)
					{
					case SDLK_ESCAPE:
						zustand = STATE_MENU;
						break;
					default: break;
					}

					if (!bPause && !bGameOver) /* Is the game active? */
					switch (event.key.keysym.sym)
					{
					case SDLK_SPACE:
						MoveCluster(1); /* "drop" cluster...      */
						NewCluster();   /* ... and create new one */
						break;
					case SDLK_DOWN:
						if (MoveCluster(0))
							NewCluster();
						break;
					case SDLK_LEFT:
						MoveClusterLeft();
						break;
					case SDLK_RIGHT:
						MoveClusterRight();
						break;
					case SDLK_UP:
						TurnClusterRight();
						break;
					case SDLK_p:
						bPause = 1; /* Activate pause mode */
						break;
					
					case SDLK_c:
						if (!bCrazy)
							bCrazy = 1;
						else {
							BoxDrawInit();
							bCrazy = 0;
						}
						
					/* Only for debugging...
					case SDLK_w:
						if (level < 10) {
							level++;
							PutSound(&sndNextlevel);
						}
						break;
					case SDLK_q:
						if (level > 0)
							level--;
						break;
					*/
						
					default: break;
					}
					else
					switch (event.key.keysym.sym)
					{
					case SDLK_p:
						bPause = 0;
					default: break;
					}

					break;
				}
			}
			Game_Loop();

			SDL_Delay(TimeLeft());
			break;

		case STATE_GAMEOVER:
			break;

		case STATE_CREDITS:
			/* Credits_Loop(false); */
			SDL_Delay(TimeLeft());
			break;

		case STATE_EXIT:
			bDone = 1;
			break;
		}
	}

	/* Free sounds again */
	SDL_FreeWAV(sndLine.samples); 
	SDL_FreeWAV(sndNextlevel.samples);

	/* Free font again */
	FreeFont(font);

	return 0;
}



/*=========================================================================
// Name: TimeLeft()
// Desc: Returns the number of ms to wait that the framerate is constant
//=======================================================================*/
Uint32 TimeLeft()
{
	static Uint32 next_time=0;
	Uint32 now;

	now = SDL_GetTicks();
	if (next_time <= now) {
		next_time = now + TICK_INTERVAL;
		return 0;
	}

	return (next_time - now);
}


/*=========================================================================
// Name: NewGame()
// Desc: Starts a new game
//=======================================================================*/
static void NewGame()
{
	InitBox(); /* Clear Box */
	BoxDrawInit(); /* Init boxdraw values */
	
	lines = 0; /* Reset lines counter */
	score = 0; /* Reset score */
	level = 0; /* Reset level */
	nextPiece = rand()%7; /* Create random nextPiece */
	NewCluster();

	bPause = 0; /* No pause */
	bCrazy = 0; /* No crazymode :) */
	bGameOver = 0;

	zustand = STATE_PLAY; /* Set playstate */
}

/*=========================================================================
// Name: MainMenu_Loop()
// Desc: The main menu loop (nasty function...)
//=======================================================================*/
static void MainMenu_Loop()
{
	int i;
	static int iAuswahl = 1;  /* Current selection in the menu */
	static int MoveValue = 0; /* X-Position of the text */
	static int bMoveDirection = 1;  /* 0=left, 1=right */
	const int yPositions[3] = {180, 250, 320};
	const int xPositions[3] = {251, 271, 291};
	const char *labels[3] = {"    .go!", "  .about", "    .quit"};
	const char *descriptions[3] = {
		"Let the fun begin!", "Credits and other stuff...", "Please don't leave me alone!" };

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_QUIT:
			bDone = 1;
			break;
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
			case SDLK_UP:
				iAuswahl--;      
				if (iAuswahl < 1)
					iAuswahl = 3;
				break;
			case SDLK_DOWN:
				iAuswahl++;
				if (iAuswahl > 3)
					iAuswahl = 1;
				break;
			case SDLK_RETURN:
			case SDLK_SPACE:
				switch (iAuswahl)
				{
				case 1:
					NewGame();
					break;
				case 2:
					zustand = STATE_CREDITS;
					ShowCredits();
					zustand = STATE_MENU;
					break;
				case 3:
					zustand = STATE_EXIT;
					break;
				}
				break;
			default: break;
			}
			break;
		}
	}

	/* Move activated text */
	if (bMoveDirection)
	{
		MoveValue+=2;
		if (MoveValue == 6)
			bMoveDirection = 0;
	}
	else
	{
		MoveValue-=2;
		if (MoveValue == -6)
			bMoveDirection = 1;
	}

	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0,0,0));
	PutRect(220, 150, 210, 230, 16,16,16);
	MoveStars();   /* Move stars */
	DrawStars();   /* Draw stars */
	WriteTextCenter(font, 80, HEADLINE);

	/* Draw "text marker" */
	PutRect(xPositions[iAuswahl-1]-10, yPositions[iAuswahl-1]+MoveValue, 140, 32, 
		0,(iAuswahl-1)*32,(iAuswahl+1)*50);
	
	for (i=0; i<3; i++)
		if (i == iAuswahl-1)
			WriteText(font, xPositions[i]+MoveValue, yPositions[i]+(MoveValue/2), labels[i]);
		else
			WriteText(font, xPositions[i], yPositions[i], labels[i]);
		
	PutRect(0,0,640,40, 32,32,32);
	PutRect(0,40,640,3,128,128,128);
	WriteTextCenter(font, 3, "just another tetris clone");
	
	PutRect(0,437,640,3, 128,128,128);
	PutRect(0,440,640,40, 32,32,32);
	WriteTextCenter(font, 445, descriptions[iAuswahl-1]);

	SDL_Flip(screen);
}

/*=========================================================================
// Name: Game_Loop()
// Desc: The main loop for the game
//=======================================================================*/
static void Game_Loop()
{
	if (!bPause)
	{
		if (!bGameOver)
		{
			cluster.dropCount--;  /* Decrease time until the next fall */
			if (cluster.dropCount == 0)
			{
				if (MoveCluster(0)) /* If cluster "collides"... */
					NewCluster();   /* then create a new one ;) */
			}

			/* Increase Level */
			if (((level == 0) && (lines >=  10)) ||
				((level == 1) && (lines >=  20)) ||
				((level == 2) && (lines >=  40)) ||
				((level == 3) && (lines >=  80)) ||
				((level == 4) && (lines >= 100)) ||
				((level == 5) && (lines >= 120)) ||
				((level == 6) && (lines >= 140)) ||
				((level == 7) && (lines >= 160)) ||
				((level == 8) && (lines >= 180)) ||
				((level == 9) && (lines >= 200)))
			{
				level++;
				PutSound(&sndNextlevel);
			}
		}
		else
		{
			GameOverAnimation();
		}

		if (bCrazy)        /* If crazy mode is actived... */
			BoxDrawMove(); /* ... change box settings!    */
		MoveStars();       /* Move stars */
		UpdateParticles(); /* Move particles */
	}

	DrawScene();
}

/*=========================================================================
// Name: DrawScene()
// Desc: Draws the whole scene!
//=======================================================================*/
static void DrawScene()
{
	char chScore[30],
		 chLines[30],
		 chLevel[30];

	/* Black background */
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0,0,0));
	DrawStars(); /* Starfield */

	if (bCrazy)
		WriteText(font, 250, 30+(boxdraw.box_x), "Cr4zY m0d3!"); /* woo hoo! */

	if (!bGameOver)
	{
		/* Draw border of the box */
		PutRect(boxdraw.box_x-5, boxdraw.box_y-5,
				boxdraw.box_width + 2*5, boxdraw.box_height + 2*5, 150,150,150);
		PutRect(boxdraw.box_x, boxdraw.box_y,
				boxdraw.box_width, boxdraw.box_height, 90,90,90);

		DrawCluster(); /* Draw cluster */
		sprintf(chScore, "%d", score);
		sprintf(chLines, "%d", lines);
		sprintf(chLevel, "%d", level);
		WriteText(font, 490, 80, "Score");  /*  Show SCORE */
		WriteText(font, 490, 105, chScore); 
		WriteText(font, 495, 180, "Next");  /*  Show NEXT piece */
		DrawNextPiece(490, 220);
		WriteText(font, 490, 350, "Lines"); /*  Show number of killed LINES */
		WriteText(font, 490, 375, chLines);
		WriteText(font, 95, 350, "Level");  /*  Show current LEVEL */
		WriteText(font, 95, 375, chLevel);
	}

	DrawBox();       /* Draw box bricks */
	DrawParticles();

	if (bPause)
		WriteText(font, 265, 20, "- PAUSE -");
	if (bGameOver && !bExplode)
	{
		WriteTextCenter(font, 120, "GAME OVER");
		sprintf(chScore, "Your Score: %d", score); 
		WriteTextCenter(font, 250, chScore);
	}

	SDL_Flip(screen); /* Let's flip! */
}

/*=========================================================================
// Name: StartGameOverAnimation()
// Desc: Starts the gameover animation
//=======================================================================*/
void StartGameOverAnimation()
{
	x = 0;
	y = 0;
	bGameOver = 1;
	bExplode = 1;
}

/*=========================================================================
// Name: GameOverAnimation()
// Desc: Gameover Animation!
//=======================================================================*/
static void GameOverAnimation()
{
	static int counter = 1; /* Sound counter */

	if (bExplode)
	{
		while (!IsBrickSet(x,y)) /* Search for the next brick */
		{
			x++;
			if (x == BOX_BRICKS_X)
			{
				x = 0;
				y++;
				if (y == BOX_BRICKS_Y) /* End reached? */
				{
					bExplode = 0;
					return;
				}
			}
		} /* Booooom! ;) */

		BrickExplosion(x, y, 2, 20);
		SetBrick(x, y, 0);
		counter--;
		if (counter == 0) { /* Sound will be played every six bricks */
			PutSound(&sndLine);
			counter = 6;
		}
	}
	else
	{
		if (NoParticlesLeft())
			zustand = STATE_MENU;
	}	
}
