/* main.c */
/* main.c part of Nimesweeper */
/*  Copyright (C) 2002 by Daniel Burnett

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
#include <signal.h>

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
