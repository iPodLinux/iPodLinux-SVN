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
#include "grafix.h"
#include "global.h"
#include "box.h"
#include "../piezo.h"
#include "../ipod.h"
#include "../pz.h"

#define HIGHSCORE ".bluecube"
void tetris_do_draw(void);
int tetris_do_keystroke(GR_EVENT *);
int new_high_score;
static void NewGame(void);
static void Game_Loop(void);
static void DrawScene(void);
void youlose(void);
GR_TIMER_ID timer_id;

void PutRect(int , int , int , int , int color);
void DrawValues(void);
void writescore(void);
int softwheelleft=0;
int softwheelright=0;
int readscore;

int lines, score;       /* Line counter and score */
int nextPiece;          /* Next cluster  */
int level;              /* Current level */

int wheeldeb,buttondeb;

int zustand;     /* Current state */
int bDone;     /* Want to Exit? */
int byoulose;   /* youlose Animation? */
int x,y;         /* Current explosion coordinates */

/*=========================================================================
// Name: main()
// Desc: Entrypoint
//=======================================================================*/
void new_bluecube_window()
{	

	/* To handle signals */

	bDone=0;		/*Reset game values*/
	byoulose=0;

	srand(time(NULL));  /* Init randomizer */

	
	InitWindow();   /* Let's init the graphics*/
	
	NewGame();
	
	timer_id =	GrCreateTimer(tetris_wid, 150);	/*Create nano-x timer*/

	while(1) {
		GR_EVENT event;
		GrGetNextEvent(&event);
		tetris_do_keystroke(&event);
		if (bDone==1)  {GrDestroyTimer(timer_id);
		break;}
		}

}




/*=========================================================================
// Name: NewGame()
// Desc: Starts a new game
//=======================================================================*/
static void NewGame()
{   FILE *readfile;
	readscore=0;
	
	if ((readfile = fopen(HIGHSCORE,"r"))) 
		{
		fread(&readscore,sizeof(readscore),1,readfile);
		fclose(readfile);
		}
	
	InitBox(); /* Clear Box */
	BoxDrawInit(); /* Init boxdraw values */
	new_high_score=0;
	
	lines = 0; /* Reset lines counter */
	score = 0; /* Reset score */
	level = 0; /* Reset level */
	nextPiece = rand()%7; /* Create random nextPiece */
	NewCluster();

	byoulose = 0;
	
	zustand = STATE_PLAY; /* Set playstate */
}


/*=========================================================================
// Name: Game_Loop()
// Desc: The main loop for the game
//=======================================================================*/
static void Game_Loop()
{
	
		if (!byoulose)
		{
			
			if (cluster.dropCount == 0)
			{   DrawCluster(2);
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
			}
		}
		else
		{
			youlose();
		}


	
	DrawScene();
}

/*=========================================================================
// Name: DrawScene()
// Desc: Draws the whole scene!
//=======================================================================*/
static void DrawScene()
{

		if (!byoulose)
	{
		
	
		/* Draw border of the box */
		PutRect(boxdraw.box_x-5, boxdraw.box_y-5,
				boxdraw.box_width + 2*5, boxdraw.box_height + 2*5, 1);
		PutRect(boxdraw.box_x, boxdraw.box_y,
				boxdraw.box_width, boxdraw.box_height, 1);
	
	
	
		DrawCluster(3); /* Draw cluster */
		
	}

	DrawBox();       /* Draw box bricks */
	
		if (byoulose)
	{   
		youlose();
		
	}

}



/*=========================================================================
// Name: youloseAnimation()
// Desc: youlose Animation!
//=======================================================================*/
void youlose(void)
{  
	
	char fileScore[50];
	char chYourScore[50];
	char chYourScore2[50];
	byoulose=1;
	bDone=1;
	
		GrSetGCForeground(tetris_gc, WHITE);
		GrFillRect(tetris_wid, tetris_gc, 0, 0, 168, 128);
		GrSetGCForeground(tetris_gc, BLACK);
		GrText(tetris_wid, tetris_gc, 40, 15, "- Game Over -",  -1, GR_TFASCII);
		sprintf(chYourScore, "Your Score: %d", score); 
		GrText(tetris_wid, tetris_gc, 35, 27, chYourScore,  -1, GR_TFASCII);
	
	

	/*do highscore stuff*/
	
	sprintf(fileScore, "Best score : %i",readscore);
	GrText(tetris_wid, tetris_gc, 35, 39, fileScore,  -1, GR_TFASCII);
	
	
	
	if (score > readscore)
		{
		GrSetGCForeground(tetris_gc, BLACK);
		GrText(tetris_wid, tetris_gc, 5, 61, "Wow! You made a new record!!",  -1, GR_TFASCII);
		sprintf(chYourScore2, "New best score: %i",score);
		GrText(tetris_wid, tetris_gc, 25, 78, chYourScore2,  -1, GR_TFASCII);
		new_high_score=1;
		}
		
/* GrFlush; // - statement with no effect */
	
	
}

void InitWindow()
{
	tetris_gc = pz_get_gc(1);
	GrSetGCUseBackground(tetris_gc, GR_TRUE);
	GrSetGCBackground(tetris_gc, WHITE);

	tetris_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), tetris_do_draw, tetris_do_keystroke);

	GrSelectEvents(tetris_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_TIMER);

	GrMapWindow(tetris_wid);
	
	GrSetGCForeground(tetris_gc, BLACK);
	GrText(tetris_wid, tetris_gc, 1, 20, "Score",  -1, GR_TFASCII);  /*  Show SCORE */
	GrText(tetris_wid, tetris_gc, 1, 32, "0",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 45, "Lines",  -1, GR_TFASCII); /*  Show number of killed LINES */
	GrText(tetris_wid, tetris_gc, 1, 57, "0",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 72, "Level",  -1, GR_TFASCII);  /*  Show current LEVEL */
	GrText(tetris_wid, tetris_gc, 1, 84, "1",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 125, 24, "Next",  -1, GR_TFASCII);
	
	tetris_do_draw();
}


int tetris_do_keystroke(GR_EVENT * event) /* Events */
	{   
	
	
	if (!byoulose) /* Is the game active? */
			{
				switch (event->type) {
	case GR_EVENT_TYPE_TIMER:
	{
	cluster.dropCount--;  /* Decrease time until the next fall */;
	break;
	}
	case GR_EVENT_TYPE_KEY_DOWN:
				switch (event->keystroke.ch)
				{
					case '\r':
						DrawCluster(2);
						TurnClusterRight();
						
						
						break;
					case 'd':
						MoveCluster(1); /* "drop" cluster...      */
						DrawCluster(2); /* erase old position     */
						NewCluster();   /* ... and create new one */
						break;
						
					case 'f':	
					case 'w':
					
						DrawCluster(2);
						if (MoveCluster(0))
							NewCluster();
						break;
						
					
					case 'l':
						softwheelleft++;
						if (softwheelleft>3)
						{
						softwheelleft=0;
						DrawCluster(2);
						MoveClusterLeft();
						}
						break;
					case 'r':
						softwheelright++;
						if (softwheelright>3)
						{
						softwheelright=0;
						DrawCluster(2);
						MoveClusterRight();
						}
						break;
					
					case 'm':
						pz_close_window(tetris_wid);
						bDone=1;
						break;

					default: break;
					}
				}
			}
		else 
			{
			if (new_high_score==1) writescore();
			pz_close_window(tetris_wid);
			}

			Game_Loop();
			return 1;
		}





/*=========================================================================
// Name: PutRect()
// Desc: Draws a filled rectangle onto the screen
//=======================================================================*/
void PutRect(int x, int y, int w, int h, int color)
{
	if (color==0) GrSetGCForeground(tetris_gc, WHITE);
	if (color==1) GrSetGCForeground(tetris_gc, BLACK);
	GrRect(tetris_wid, tetris_gc, x, y, w, h);
	
	if (color==2) {
	GrSetGCForeground(tetris_gc, WHITE);
	GrFillRect(tetris_wid, tetris_gc, x, y, w, h);
	}
	
	if (color==3) {
	GrSetGCForeground(tetris_gc, BLACK);
	GrFillRect(tetris_wid, tetris_gc, x, y, w, h);
	}
}

void tetris_do_draw()
{
	pz_draw_header("Tetris");
}


void DrawValues(void)    /*clear and draw score, lines and level values*/
{
		char chScore[30],
			 chLines[30],
			 chLevel[30];
			 
		
		PutRect(130,40,20,30,2); /* clear "old" nextcluster preview... */
		DrawNextPiece(130, 40); /*... and draw the new one*/
		
		GrSetGCForeground(tetris_gc, BLACK);
		sprintf(chScore, "%d", score);
		sprintf(chLines, "%d", lines);
		sprintf(chLevel, "%d", level);
		GrText(tetris_wid, tetris_gc, 1, 32, chScore,  -1, GR_TFASCII);
		GrText(tetris_wid, tetris_gc, 1, 57, chLines,  -1, GR_TFASCII);
		GrText(tetris_wid, tetris_gc, 1, 84, chLevel,  -1, GR_TFASCII);
		
		
		
		
}


void writescore()
{
FILE *writefile;

if ((writefile = fopen(HIGHSCORE,"w"))) 
		{
		fwrite(&score,sizeof(score),1,writefile);
		fclose(writefile);
		}
}
