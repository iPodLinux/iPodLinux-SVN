/* windows.c */
/* windows.c part of Nimesweeper */
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

/* function to draw a new minefield to the global WINDOW, GameWin */
void Draw_Grid(int Width, int Height)
{
	/* Print the new minefield to GameWin*/
	register int x,y;
	
	GameWin = newwin(Height+2,Width*2+1,0,0);
	box(GameWin,0,0);	
	for(x=0;x<Width;x++)
		for(y=0;y<Height;y++)
			mvwaddch(GameWin,Y,X,UNKNOWN | COLOR_PAIR(1) | A_BOLD);
}


/* Function to draw the "flags remaining" to global WINDOW FlagsWin */
void Draw_FlagsWin(int Width, int Height)
{
	FlagsWin = newwin(3,29,Height+1,(Width*2-27)/2);
	box(FlagsWin,0,0);
}


/* Nuke that window... just to be sure. */
void KillWin(WINDOW *Window)
{
	werase(Window);
	wborder(Window, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	wrefresh(Window);
	delwin(Window);
}


/* Create Help Window, in center of GameWin */
WINDOW *Draw_HelpWin(int Width, int Height)
{
	WINDOW *Help_Win;
	Help_Win = newwin(14,31,Height/2-6,(Width*2-29)/2);
	box(Help_Win,0,0);
	return Help_Win;
}


/* Create ChangeDifficulty Window, in center of GameWin */
WINDOW *Draw_ChangeDifficultyWin(int Width, int Height)
{
	WINDOW *Change_Difficulty;
	Change_Difficulty = newwin(10,25,Height/2-4,(Width*2-23)/2);
	box(Change_Difficulty,0,0);
	return Change_Difficulty;
}

/* Create HighScore Window, in center of GameWin  */
WINDOW *Draw_HighScoreWin(int Width, int Height)
{
	WINDOW *High_Score;
	High_Score = newwin(16,31,Height/2-7,(Width*2-29)/2);
	box(High_Score,0,0);
	return High_Score;
}

/* Create Winner/Loser Window, in center of GameWin */
WINDOW *Draw_WinnerWin(int Width, int Height)
{
	WINDOW *Winner_Win;
	Winner_Win = newwin(9,29,Height/2-4,(Width*2-27)/2);
	box(Winner_Win,0,0);
	return Winner_Win;
}
