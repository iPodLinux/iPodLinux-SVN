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

static void tetris_do_draw(void);
static int tetris_do_keystroke(GR_EVENT *);
static void tetris_init(void);
static GR_TIMER_ID timer_id;

static void tetris_new_game(void);
static void tetris_game_loop(void);
static void tetris_draw_scene(void);
static void tetris_write_score(void);

int tetris_lines, tetris_score; /* Line counter and score */
int tetris_next_piece;          /* Next cluster  */
int tetris_level;               /* Current level */

static int zustand;   /* Current state */
static int byoulose;  /* youlose Animation? */

/*========================================================================
// Name: new_bluecube_window()
// Desc: Entrypoint
//======================================================================*/
void new_bluecube_window()
{	
	byoulose=0;

	srand(time(NULL));  /* Init randomizer */
	
	tetris_init();   /* Let's init the graphics*/
	
	tetris_new_game();
	
	timer_id =	GrCreateTimer(tetris_wid, 150);	/*Create nano-x timer*/
}




/*========================================================================
// Name: tetris_new_game()
// Desc: Starts a new game
//======================================================================*/
static void tetris_new_game()
{
	InitBox(); /* Clear Box */
	BoxDrawInit(); /* Init boxdraw values */
	
	tetris_lines = 0; /* Reset lines counter */
	tetris_score = 0; /* Reset score */
	tetris_level = 0; /* Reset level */
	tetris_next_piece = rand()%7; /* Create random nextPiece */
	NewCluster();

	byoulose = 0;
	
	zustand = STATE_PLAY; /* Set playstate */
}


/*========================================================================
// Name: tetris_game_loop()
// Desc: The main loop for the game
//======================================================================*/
static void tetris_game_loop()
{
	if (!byoulose) {
		if (cluster.dropCount == 0) {
				DrawCluster(2);
				if (MoveCluster(0)) /* If cluster "collides"... */
					NewCluster();   /* then create a new one ;) */
		}

		/* Increase Level */
		if (((tetris_level == 0) && (tetris_lines >=  10)) ||
			((tetris_level == 1) && (tetris_lines >=  20)) ||
			((tetris_level == 2) && (tetris_lines >=  40)) ||
			((tetris_level == 3) && (tetris_lines >=  80)) ||
			((tetris_level == 4) && (tetris_lines >= 100)) ||
			((tetris_level == 5) && (tetris_lines >= 120)) ||
			((tetris_level == 6) && (tetris_lines >= 140)) ||
			((tetris_level == 7) && (tetris_lines >= 160)) ||
			((tetris_level == 8) && (tetris_lines >= 180)) ||
			((tetris_level == 9) && (tetris_lines >= 200))) {
				tetris_level++;
		}
	} else {
		tetris_lose();
	}

	tetris_draw_scene();
}

/*========================================================================
// Name: tetris_draw_scene()
// Desc: Draws the whole scene!
//======================================================================*/
static void tetris_draw_scene()
{
	if (!byoulose) {
		/* Draw border of the box */
		tetris_put_rect(boxdraw.box_x-2, boxdraw.box_y-2,
				boxdraw.box_width + 2*2, boxdraw.box_height + 2*2, 1);
		//tetris_put_rect(boxdraw.box_x-1, boxdraw.box_y-1,
		//		boxdraw.box_width+2, boxdraw.box_height+2, 1);
				
		DrawCluster(3); /* Draw cluster */
	}

	DrawBox();       /* Draw box bricks */
	
	if (byoulose) {   
		tetris_lose();
	}
}

/*========================================================================
// Name: tetris_lose()
// Desc: Losing Screen
//======================================================================*/
void tetris_lose(void)
{  
	char fileScore[50];
	char chYourScore[50];
	char chYourScore2[50];
	int readscore = 0;
	FILE *readfile;
	
	byoulose=1;
	
	if ((readfile = fopen(HIGHSCORE,"r"))) {
		fread(&readscore,sizeof(readscore),1,readfile);
		fclose(readfile);
	}
		
	GrSetGCForeground(tetris_gc, WHITE);
	GrFillRect(tetris_wid, tetris_gc, 0, 0, screen_info.cols,
	           screen_info.rows-21);
	GrSetGCForeground(tetris_gc, BLACK);
	GrText(tetris_wid, tetris_gc, (screen_info.cols/4), 15, "- Game Over -",
	       -1, GR_TFASCII);
	sprintf(chYourScore, "Your Score: %d", tetris_score); 
	GrText(tetris_wid, tetris_gc, (screen_info.cols/4)-5, 27, chYourScore,
	-1, GR_TFASCII);
	
	/*do highscore stuff*/
	sprintf(fileScore, "Best score : %i", readscore);
	GrText(tetris_wid, tetris_gc, (screen_info.cols/4)-5, 39, fileScore,
	       -1, GR_TFASCII);
	
	if (tetris_score > readscore) {
		GrSetGCForeground(tetris_gc, BLACK);
		GrText(tetris_wid, tetris_gc, 5, 61, "Wow, a new record!!",
		       -1, GR_TFASCII);
		sprintf(chYourScore2, "New best score: %i", tetris_score);
		GrText(tetris_wid, tetris_gc, 25, 78, chYourScore2,  -1, GR_TFASCII);
		tetris_write_score();
	}
		
	GrFlush();	
}

static void tetris_init()
{
	tetris_gc = pz_get_gc(1);
	GrSetGCUseBackground(tetris_gc, GR_TRUE);
	GrSetGCBackground(tetris_gc, WHITE);

	tetris_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
	                           screen_info.rows - (HEADER_TOPLINE + 1),
	                           tetris_do_draw, tetris_do_keystroke);

	GrSelectEvents(tetris_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN|
	               GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_TIMER);

	GrMapWindow(tetris_wid);
	
	GrSetGCForeground(tetris_gc, BLACK);
	GrText(tetris_wid, tetris_gc, 1, 20, "Score",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 32, "0",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 45, "Lines",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 57, "0",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 72, "Level",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 84, "1",  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, screen_info.cols-35, 24, "Next",  -1,
	       GR_TFASCII);
	
	tetris_do_draw();
}

static void tetris_exit(void)
{
	GrDestroyTimer(timer_id);
	GrDestroyGC(tetris_gc);
	pz_close_window(tetris_wid);
}

static int tetris_do_keystroke(GR_EVENT * event)
{
	int ret = 0;
	
	switch (event->type) {
		case GR_EVENT_TYPE_TIMER:
			if (!byoulose) {
				cluster.dropCount--;
				tetris_game_loop();
			}
			break;
		case GR_EVENT_TYPE_KEY_DOWN:
			if (!byoulose) {
			switch (event->keystroke.ch) {
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
					DrawCluster(2);
					MoveClusterLeft();
					break;
				case 'r':
					DrawCluster(2);
					MoveClusterRight();
					break;
				case 'm':
					ret=1;
					tetris_exit();
					break;
				default:
					break;
			}
			if (!ret) { // yeah i shoudl use something else :(
				tetris_game_loop();
				ret=1;
			}
			} else {
				if (event->keystroke.ch == '\r')
					tetris_exit();
			}
			break;
		default:
			break;
	}

	return ret;
}

/*=========================================================================
// Name: tetris_put_rect()
// Desc: Draws a filled rectangle onto the screen
//=======================================================================*/
void tetris_put_rect(int x, int y, int w, int h, int color)
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


void tetris_draw_values(void) /*clear and draw score, lines and level values*/
{
	char chScore[30], chLines[30], chLevel[30];

	tetris_put_rect(screen_info.cols-(boxdraw.brick_width*6),40,(boxdraw.brick_width*6),(boxdraw.brick_height*6),2); /* clear "old" next preview... */
	DrawNextPiece(screen_info.cols-(boxdraw.brick_width*6), 40); /*... and draw the new one*/
		
	GrSetGCForeground(tetris_gc, BLACK);
	sprintf(chScore, "%d", tetris_score);
	sprintf(chLines, "%d", tetris_lines);
	sprintf(chLevel, "%d", tetris_level);
	GrText(tetris_wid, tetris_gc, 1, 32, chScore,  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 57, chLines,  -1, GR_TFASCII);
	GrText(tetris_wid, tetris_gc, 1, 84, chLevel,  -1, GR_TFASCII);
}


void tetris_write_score()
{
	FILE *writefile;

	if ((writefile = fopen(HIGHSCORE,"w"))) {
		fwrite(&tetris_score,sizeof(tetris_score),1,writefile);
		fclose(writefile);
	}
}
