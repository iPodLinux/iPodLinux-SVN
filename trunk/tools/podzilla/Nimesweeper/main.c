/* main.c */
/* main.c part of Nimesweeper */
/*  Copyright (C) 2002 by Daniel Burnett

    Copyright (C) 2004 by Matthis Rouch (iPod port)
	- with lots of code taken from Courtney Cavin's ipod-othello port

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mine.h"
#ifdef USE_NCURSES
#include <signal.h>
#else
#include "../pz.h"
#endif

int ylen = 4;
int xlen = 4;
int xoff = 20;
int yoff = 20;
int nummines = 1;

#ifdef USE_NCURSES
int main(int argc, char **argv)
{
	/* Main game data structure */
        GameStats Game;

	/* To temporarily store gameplay time */
	struct timeval start, stop;

	int choice;	 	  /* To store getch() input */
	unsigned int restart = 2; /* Restart status; 2 is normal,
				     1 causes a restart,
				     0 causes normal termination. */
	/* To handle signals */
	signal(SIGTERM, CleanUp);
        signal(SIGHUP,  CleanUp);
        signal(SIGILL,  CleanUp);
        signal(SIGSEGV, CleanUp);
        signal(SIGKILL, CleanUp);
        signal(SIGQUIT, CleanUp);
        signal(SIGBUS,  CleanUp);

	/* Set difficulty, initiate variables and screen, start timer.
	   Exit if a non gameplaying/incorrect argument has been passed, Eg. -v */
	if( (Game.Difficulty = Args(argc,argv)) == FALSE )
		return 0;


	Init_Game(&Game,&start,argv[0],&restart);

	/* And so we begin the game... */
	do
	{
	ShowRemainingFlags(&Game);
	move(Game.Y,Game.X);
	wrefresh(GameWin);
	fflush(stdin);
	choice = toupper(getch());
	switch(choice)
	{
		case KEY_UP:
			MoveUp(&Game);
			break;
		case KEY_DOWN:
			MoveDown(&Game);
			break;
		case KEY_LEFT:
			MoveLeft(&Game);
			break;
		case KEY_RIGHT:
			MoveRight(&Game);
			break;

		/* Show the Help Window */
		case KEY_F(1):
		case KEY__HELP:
			ShowHelp(Game.Width,Game.Height);
			break;

		/* Show best times */
		case KEY_F(2):
		case KEY__TIMES:
			if( CheckScoresFile() == TRUE )
				ShowHighScores(&Game,FALSE);
			break;

		/* Change difficulty level */
		case KEY_F(3):
		case KEY__DIFF:
			switch(ChangeDifficultyLevel(&Game))
			{
				case TRUE:
					restart = TRUE;
					break;
				case KEY__QUIT:
					choice = KEY__QUIT;
					break;
				default:
					break;
			}
			break;

		/* Restart the game */
		case KEY_F(5):
		case KEY__RESTART:
			restart = TRUE;
			break;

		/* Pause game and timer */
		case KEY__PAUSE:
			if( Pause(&Game, &start, &stop) == KEY__QUIT )
				choice = KEY__QUIT;
			break;


		/* Flag/Unflag a square */
		case KEY__FLAG:
			/* Test for Winner */
			if( FlagSquare(&Game) == TRUE )
			{
				gettimeofday(&stop,NULL);
				Game.Timer+= (stop.tv_sec - start.tv_sec);
				restart = Winner(&Game,TRUE);
			}
			break;


		/* Uncover a square */
		case KEY__UNCOVER:
			/* Only bother if square is unflagged */
			if( flags[Game.x][Game.y] == FALSE )
				/* Uncover sqaure and test for a mine */
				if( Uncover(Game.x,Game.y,Game.Width,Game.Height) == TRUE )
					restart = Winner(&Game,FALSE);
			break;


		/* Capture Esc to quit */
		case 27:
			choice = KEY__QUIT;
			break;

#ifdef CHEATERS_R_US		/* For no good cheating s.o.b's	that read the source. :-) */
		case KEY_F(11):
			gettimeofday(&stop,NULL);
			Game.Timer+= (stop.tv_sec - start.tv_sec);
			restart = Winner(&Game,TRUE);
			break;

		/* ;-) */
		case KEY_F(12):
			if( grid[Game.x][Game.y] == MINED )
				beep();
			break;
#endif
		default:
			flash();
			break;
	}

	/* Restart game, re-initialise variables,
	   reset mines, reset timer, reset screen etc.*/
	if( restart == TRUE )
	{
		endwin();
		Init_Game(&Game,&start,argv[0],&restart);
	}
	if( restart == FALSE )
		choice = KEY__QUIT;

	}while(choice != KEY__QUIT);
	/* End of do{...}while(); */

	clear();
	refresh();
	endwin();
	return 0;
}

#else

GR_WINDOW_ID mines_wid;
GR_GC_ID mines_gc;
GR_SCREEN_INFO screen_info;

static GameStats Game;

static struct timeval start, stop;

static unsigned int restart; /* Restart status; 2 is normal,
			     1 causes a restart,
			     0 causes normal termination. */

static int lastx;
static int lasty;

static void draw_current_pos(int moved)
{
#if 0
	/* this doesnt seem to work on the ipod? */
	GrSetGCForeground(mines_gc, WHITE);
	GrSetGCMode(mines_gc, GR_MODE_XOR);
	GrFillRect(mines_wid, mines_gc, (Game.x*12)+3, (Game.y*12)+7, 11, 11);

	if (moved) {
		GrFillRect(mines_wid, mines_gc, (lastx*12)+3, (lasty*12)+7, 11, 11);

		lastx = Game.x;
		lasty = Game.y;
	}

	GrSetGCMode(mines_gc, GR_MODE_SET);
#else
	GrSetGCForeground(mines_gc, BLACK);
	GrRect(mines_wid, mines_gc, (Game.x*SQSIZE)+xoff+1, (Game.y*SQSIZE)+yoff+1, SQSIZE-1, SQSIZE-1);

	if (moved) {
		GrSetGCForeground(mines_gc, WHITE);
		GrRect(mines_wid, mines_gc, (lastx*SQSIZE)+xoff+1, (lasty*SQSIZE)+yoff+1, SQSIZE-1, SQSIZE-1);

		lastx = Game.x;
		lasty = Game.y;
	}

#endif
}

static void mines_do_draw()
{
	int i,a,b;
	char buf[18];
	
	snprintf(buf, 18, "Flags left: %d", nummines);
	
	pz_draw_header(buf);

	GrSetGCForeground(mines_gc, BLACK);
	for (i = yoff; i <= yoff+(SQSIZE*ylen); i += SQSIZE) {
		GrLine(mines_wid, mines_gc, xoff, i, (xlen*SQSIZE)+xoff, i);
	}

	for (i = xoff; i <= xoff+(SQSIZE*xlen); i += SQSIZE) {
		GrLine(mines_wid, mines_gc, i, yoff, i, (ylen*SQSIZE)+yoff);
	}

	for (i = 0; i < ylen*xlen;i++){
		a=i * SQSIZE + xoff - (xlen*SQSIZE)*(int)(i/xlen);
		b=yoff+SQSIZE*(int)(i/xlen);

		GrFillRect(mines_wid, mines_gc, a+4,b+4, 5, 5);
	}

	draw_current_pos(FALSE);
}

static int mines_do_keystroke(GR_EVENT * event)
{
	/* keystrokes during gameplay */
	if (restart==2)
	{
		switch(event->type) {
		case GR_EVENT_TYPE_KEY_DOWN:
			switch (event->keystroke.ch) {
			case 'l':
				MoveLeft(&Game);
				draw_current_pos(TRUE);
				break;

			case 'r':
				MoveRight(&Game);
				draw_current_pos(TRUE);
				break;

			case 'm':
				pz_close_window(mines_wid);
				break;

			/* Flag/Unflag a square */
			case 'd':
				/* Test for Winner */
				if( FlagSquare(&Game) == TRUE )
				{
					gettimeofday(&stop,NULL);
					Game.Timer += (stop.tv_sec - start.tv_sec);
					restart = Winner(&Game,TRUE);
				}
				else {
					draw_current_pos(FALSE);
				}
				break;


			/* Uncover a square */
			case '\r':
				/* Only bother if square is unflagged */
				if( flags[Game.x][Game.y] == FALSE ) {
					/* Uncover sqaure and test for a mine */
					if( Uncover(Game.x,Game.y,Game.Width,Game.Height) == TRUE ) {
						restart = Winner(&Game,FALSE);
					}
					else {
						draw_current_pos(FALSE);
					}
				}
				break;
			}
			break;
		}
	}
	else {
		pz_close_window(mines_wid);
	}
	return 1;
}

void new_mines_window()
{
	FILE *File;
    GrGetScreenInfo(&screen_info);

    if (screen_info.cols == 138) { /* mini */
    	ylen = 7;
		xlen = 11;
		xoff = 2;
		yoff = 2;
		nummines = 10;
	} else {
		ylen = 8;
		xlen = 13;
		xoff = 2;
		yoff = 6;
		nummines = 12;
	}

	/* check highscore file exists, if not create it*/
	if ((File = fopen(HIGHSCORES,"r")) == NULL) {
		CreateScoresFile();
	}

	Game.Difficulty = 1;
	Init_Game(&Game,&start,0,&restart);
	lastx = Game.x;
	lasty = Game.y;

	mines_gc = GrNewGC();
	GrSetGCForeground(mines_gc, BLACK);
	GrSetGCUseBackground(mines_gc, GR_FALSE);
	GrSetGCMode(mines_gc, GR_MODE_SET);

	mines_wid = pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), mines_do_draw, mines_do_keystroke);

	GrSelectEvents(mines_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(mines_wid);

	GrSetGCForeground(mines_gc, BLACK);
}

#endif
