/* gameplay.c */
/* gameplay.c part of Nimesweeper */
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

/* Function that will clear a path when a "clear" square
   has been uncovered. It recursively calls itself when 
   searching paths, printing the CLEAR char in each "clear" square.
   Starts at 12 O'clock, then works clockwise.
   Bounds testing so that the grid and flags arrays are accessed within
   their limits. */
void ClearPath(register int x, register int y, int width, int height)
{
	mvwaddch(GameWin,Y,X,CLEAR | COLOR_PAIR(3));
	flags[x][y] = TRUE;
	
	/* Uncover any surrounding numbered squares */
	UncoverBoundary(x,y,width,height);
	
	/* Test top */
	if( y > 0 )
	{
		if( grid[x][y-1] == FALSE && flags[x][y-1] == FALSE )
			ClearPath(x,y-1,width,height);
	}
	/* Test top right */
	if( x < width-1 && y > 0 )
	{
		if( grid[x+1][y-1] == FALSE && flags[x+1][y-1] == FALSE )
			ClearPath(x+1,y-1,width,height);
	}
	/* Test right */
	if( x < width-1 )
	{
		if( grid[x+1][y] == FALSE && flags[x+1][y] == FALSE )
			ClearPath(x+1,y,width,height);
	}
	/* Test bottom right */
	if( x < width-1 && y < height-1 )
	{
		if( grid[x+1][y+1] == FALSE && flags[x+1][y+1] == FALSE )
			ClearPath(x+1,y+1,width,height);
	}
	/* Test bottom */
	if( y < height-1 )
	{
		if( grid[x][y+1] == FALSE && flags[x][y+1] == FALSE )
			ClearPath(x,y+1,width,height);
	}
	/* Test bottom left */
	if( x > 0 && y < height-1 )
	{
		if( grid[x-1][y+1] == FALSE && flags[x-1][y+1] == FALSE )
			ClearPath(x-1,y+1,width,height);
	}
	/* Test left */
	if( x > 0 )
	{
		if( grid[x-1][y] == FALSE && flags[x-1][y] == FALSE )
			ClearPath(x-1,y,width,height);
	}
	/* Test top left */
	if( x > 0 && y > 0 )
	{
		if( grid[x-1][y-1] == FALSE && flags[x-1][y-1] == FALSE )
			ClearPath(x-1,y-1,width,height);
	}
}


/* Function to show the number of unused flags */
void ShowRemainingFlags(GameStats *Game)
{
	mvwprintw(FlagsWin,1,2,"Flags remaining (");
	waddch(FlagsWin,FLAG | COLOR_PAIR(8) | A_BOLD);
	wprintw(FlagsWin,") - ");
	if( (Game->MinesSet - Game->Guesses) <= FALSE )
	{
		/* If zero flags left, print "0" as red. */
		wattron(FlagsWin, COLOR_PAIR(2) | A_BOLD);
	}
	else
	{
		/* ...Else white. */
		wattron(FlagsWin, COLOR_PAIR(7) | A_BOLD);
	}
	wprintw(FlagsWin,"%3d",Game->MinesSet - Game->Guesses);
	wattroff(FlagsWin, COLOR_PAIR(2) | COLOR_PAIR(7) | A_BOLD);
	wrefresh(FlagsWin);
}

/* Function to Flag/Unflag a square. */
void MarkSquare(register int x, register int y)
{
	switch(flags[x][y])
	{
	/* If the sqaure is unflagged: flag it.*/
	case FALSE:
		mvwaddch(GameWin,Y,X,FLAG | COLOR_PAIR(8) | A_BOLD);
		flags[x][y] = FLAGGED;
		break;
	/* If the square is flagged: unflag it. */
	case FLAGGED:
		
                mvwaddch(GameWin,Y,X,UNKNOWN | COLOR_PAIR(1) | A_BOLD);
                flags[x][y] = FALSE;
		break;
        }
}


/* Function to flag a square and work out the number of 
   correctly flagged squares. Returns TRUE, if they 
   are a winner, returns FALSE otherwise. */
int FlagSquare(GameStats *Game)
{
	/* If no flags are left and square is UNKNOWN, then warn. */
	if( (Game->MinesSet - Game->Guesses) <= FALSE
	    && flags[Game->x][Game->y] == FALSE )
	{
		flash();
		beep();
		return FALSE;
	}
		
	/* If it is not a mine... */
	if( grid[Game->x][Game->y] != MINED )
	{
		/* if it is marked */
		if( flags[Game->x][Game->y] == FLAGGED )
			Game->Guesses--;
		/* if it is unmarked */
		else
			if( Game->Guesses < Game->MinesSet
			    && flags[Game->x][Game->y] != TRUE )
				Game->Guesses++;
	}
	
	/* And if it is a mine... */
	else
	{
		/* if it is an unmarked mine */
		if( flags[Game->x][Game->y] == FALSE )
		{
			if( Game->Guesses < Game->MinesSet )
			{
				Game->Correct++;
				Game->Guesses++;
			}
		}
		/* if it is a marked mine */
		else
		{
				Game->Correct--;
				Game->Guesses--;
		}
	}

	/* Flag square */
	MarkSquare(Game->x,Game->y);
	
	/* Test for a winner */
	if( Game->Correct >= Game->MinesSet )
		return TRUE;
	else
		return FALSE;
}


/* Moves cursor Up */
void MoveUp(GameStats *Game)
{
	if( --Game->y < 0 )
		Game->y = Game->Height-1;
}
/* Moves cursor down */
void MoveDown(GameStats *Game)
{
	if( ++Game->y > Game->Height-1 )
		Game->y = 0;
}
/* Moves cursor left */
void MoveLeft(GameStats *Game)
{
	if( --Game->x < 0 )
		Game->x = Game->Width-1;
}
/* Moves cursor right */
void MoveRight(GameStats *Game)
{
	if( ++Game->x > Game->Width-1 )
		Game->x = 0;
}


/* Help function: displays on-screen help */
void ShowHelp(int Width, int Height)
{
	WINDOW *HelpWin;

	HelpWin = Draw_HelpWin(Width,Height);
	
	curs_set(0);
	wattron(HelpWin,COLOR_PAIR(7));
	mvwaddstr(HelpWin,2,2,"<Action>           <Key>");
	mvwaddstr(HelpWin,4,2,"Flag a square:     ");
	waddch(HelpWin,KEY__FLAG | A_BOLD);
	mvwprintw(HelpWin,5,2,"Pause/Unpause:     ");
	waddch(HelpWin,KEY__PAUSE | A_BOLD);
	mvwaddstr(HelpWin,6,2,"Uncover a square:  ");
	wattron(HelpWin,A_BOLD);
	waddstr(HelpWin,"Space");
	wattroff(HelpWin,A_BOLD);
	mvwaddstr(HelpWin,7,2,"Show this help:    ");
	waddch(HelpWin,KEY__HELP | A_BOLD);
	waddstr(HelpWin,", ");
	waddch(HelpWin,'F' | A_BOLD);
	waddch(HelpWin,'1' | A_BOLD);
	mvwaddstr(HelpWin,8,2,"Show best times:   ");
	waddch(HelpWin,KEY__TIMES | A_BOLD);
	waddstr(HelpWin,", ");
	waddch(HelpWin,'F' | A_BOLD);
	waddch(HelpWin,'2' | A_BOLD);
	mvwaddstr(HelpWin,9,2,"Change difficulty: ");
	waddch(HelpWin,KEY__DIFF | A_BOLD);
	waddstr(HelpWin,", ");
	waddch(HelpWin,'F' | A_BOLD);
	waddch(HelpWin,'3' | A_BOLD);
	mvwaddstr(HelpWin,10,2,"Restart game:      ");
	waddch(HelpWin,KEY__RESTART | A_BOLD);
	waddstr(HelpWin,", ");
	waddch(HelpWin,'F' | A_BOLD);
	waddch(HelpWin,'5' | A_BOLD);
	mvwaddstr(HelpWin,11,2,"Quit game:         ");
	waddch(HelpWin,KEY__QUIT | A_BOLD);
	waddstr(HelpWin,", ");
	wattron(HelpWin,A_BOLD);
	waddstr(HelpWin,"Esc");
	wattroff(HelpWin,COLOR_PAIR(7));
	wrefresh(HelpWin);
	getch();
	KillWin(HelpWin);
	redrawwin(GameWin);
	curs_set(1);
}


/* Function to uncover numbers surrounding a "clear" square.
   Only calls uncover() if square is:
   1) Within the grid bounds
   2) Not a mine.
   3) Not a "clear" square.
   4) Not flagged */
void UncoverBoundary(register int x, register int y, int width, int height)
{
	if( y > 0 )
		if( grid[x][y-1] != MINED && grid[x][y-1] != FALSE )
			if( flags[x][y-1] == FALSE )
				Uncover(x,y-1,width,height);
	if( x < width-1 && y > 0 )
		if( grid[x+1][y-1] != MINED && grid[x+1][y-1] != FALSE )
			if( flags[x+1][y-1] == FALSE )
				Uncover(x+1,y-1,width,height);
	if( x < width-1 )
		if( grid[x+1][y] != MINED && grid[x+1][y] != FALSE )
			if( flags[x+1][y] == FALSE )
				Uncover(x+1,y,width,height);
	if( x < width-1 && y < height-1 )
		if(grid[x+1][y+1] != MINED && grid[x+1][y+1] != FALSE )
			if( flags[x+1][y+1] == FALSE )
				Uncover(x+1,y+1,width,height);
	if( y < height-1 )
		if( grid[x][y+1] != MINED && grid[x][y+1] != FALSE )
			if( flags[x][y+1] == FALSE )
				Uncover(x,y+1,width,height);
	if( x > 0 && y < height-1 )
		if( grid[x-1][y+1] != MINED && grid[x-1][y+1] != FALSE )
			if( flags[x-1][y+1] == FALSE )
				Uncover(x-1,y+1,width,height);
	if( x > 0 )
		if( grid[x-1][y] != MINED && grid[x-1][y] != FALSE )
			if( flags[x-1][y] == FALSE )
				Uncover(x-1,y,width,height);
	if( x > 0 && y > 0 )
		if( grid[x-1][y-1] != MINED && grid[x-1][y-1] != FALSE )
			if( flags[x-1][y-1] == FALSE )
				Uncover(x-1,y-1,width,height);
}


/* Function to uncover a square
   Returns 1 on a mine, 0 otherwise */
int Uncover(int x, int y, int width, int height)
{

flags[x][y] = TRUE;
switch(grid[x][y])
{
	/* Oops, someone uncovered a mine */
	case MINED:
		return TRUE;

	/* Not a mine, so ClearPath() if a "clear" square,
   	or print number of mines surrounding the square. */
	case 8:
	case 7:
	case 6: /* Bold magenta */
		mvwaddch(GameWin,Y,X, ('0'+grid[x][y]) | COLOR_PAIR(5) | A_BOLD);
		break;
	case 5:
		/* Magenta */
		mvwaddch(GameWin,Y,X, ('0'+grid[x][y]) | COLOR_PAIR(5));
		break;
	case 4:
		/* Red */
		mvwaddch(GameWin,Y,X, ('0'+grid[x][y]) | COLOR_PAIR(2));
		break;
	case 3:
		/* Bold red */
		mvwaddch(GameWin,Y,X, ('0'+grid[x][y]) | COLOR_PAIR(2) | A_BOLD);
		break;
	case 2:
		/* Yellow */
		mvwaddch(GameWin,Y,X, ('0'+grid[x][y]) | COLOR_PAIR(4));
		break;
	case 1:
		/* Bold blue */
		mvwaddch(GameWin,Y,X, ('0'+grid[x][y]) | COLOR_PAIR(6) | A_BOLD);
		break;
	case 0:
		ClearPath(x,y,width,height);
		break;
	
	/* This "shouldn't" ever happen, but... */
	default:
		endwin();
		fprintf(stderr,"Uncover::grid[x][y] is out of range\n");
		exit(1);
}
return 0;
}

/* Pauses game, "stops/starts" timer. */
int Pause(GameStats *Game, struct timeval *start, struct timeval *stop)
{
	int ch;
	
	gettimeofday(stop,NULL);
	Game->Timer+= (stop->tv_sec - start->tv_sec);
	curs_set(0);
	mvwaddstr(FlagsWin,1,2,"*********PAUSED**********");
	wrefresh(FlagsWin);
	do
	{
		fflush(stdin);
		ch = toupper(getch());
		if( ch != KEY__PAUSE )
		{
			flash();
			beep();
		}
		if( ch == 27 || ch == KEY__QUIT )
			return KEY__QUIT;
	}while( ch != KEY__PAUSE && ch != '-' );
	curs_set(1);
	gettimeofday(start,NULL);
	return TRUE;
}


/* Function that changes the difficulty, 
   returns TRUE is it is changed, FALSE otherwise */
int ChangeDifficultyLevel(GameStats *Game)
{
	WINDOW *ChangeDifficulty;
	int Choice, RetVal = -1;
	
	ChangeDifficulty = Draw_ChangeDifficultyWin(Game->Width,Game->Height);
	curs_set(0);
	wattron(ChangeDifficulty,COLOR_PAIR(7));
	mvwaddstr(ChangeDifficulty,1,4,"Change Difficulty");
	mvwaddstr(ChangeDifficulty,2,4,"      Level");
	mvwaddch(ChangeDifficulty,4,5,'(');
	waddch(ChangeDifficulty,'1' | A_BOLD);
	waddstr(ChangeDifficulty,")   -   Newbie");
	mvwaddch(ChangeDifficulty,5,5,'(');
	waddch(ChangeDifficulty,'2' | A_BOLD);
	waddstr(ChangeDifficulty,")   -   Easy");
	mvwaddch(ChangeDifficulty,6,5,'(');
	waddch(ChangeDifficulty,'3' | A_BOLD);
	waddstr(ChangeDifficulty,")   -   Medium");
	mvwaddch(ChangeDifficulty,7,5,'(');
	waddch(ChangeDifficulty,'4' | A_BOLD);
	waddstr(ChangeDifficulty,")   -   Hard");
	mvwaddch(ChangeDifficulty,8,8,'(');
	waddch(ChangeDifficulty,'D' | A_BOLD);
	waddstr(ChangeDifficulty,")ismiss");
	switch(Game->Difficulty)
	{
		case 1:
			mvwaddch(ChangeDifficulty,4,6,'1');
			break;
		case 2: 
			mvwaddch(ChangeDifficulty,5,6,'2');
			break;
		case 3: 
			mvwaddch(ChangeDifficulty,6,6,'3');
			break;
		case 4: 
			mvwaddch(ChangeDifficulty,7,6,'4');
			break;
		default:
			endwin();
			fprintf(stderr,
			"ChangeDifficultyLevel:: Game->Difficulty out of range\n");
			exit(2);
	}
	wrefresh(ChangeDifficulty);
	wattroff(ChangeDifficulty,COLOR_PAIR(7));
	
	do
	{
		fflush(stdin);
		Choice = toupper(getch());
		if( Game->Difficulty == (Choice-'0') )
			RetVal = FALSE;
		switch(Choice)
		{
			case '1':
				Game->Difficulty = 1;
				RetVal = TRUE;
				break;
			case '2':
				Game->Difficulty = 2;
				RetVal = TRUE;
				break;
			case '3':
				Game->Difficulty = 3;
				RetVal = TRUE;
				break;
			case '4':
				Game->Difficulty = 4;
				RetVal = TRUE;
				break;
			case KEY_F(3):
			case 'd':
			case 'D':
				RetVal = FALSE;
				break;
			case 27:
			case KEY__QUIT:
				RetVal = KEY__QUIT;
				break;
			default:
				beep();
				flash();
				break;
		}
	}while(RetVal != TRUE && RetVal != KEY__QUIT && RetVal != FALSE);
	
	KillWin(ChangeDifficulty);
	redrawwin(GameWin);
	curs_set(1);
	return RetVal;
}



/* Function to deal with a win or loss */
int Winner(GameStats *Game, int winner)
{
	register int x_coord,y_coord;
	char ch;
	WINDOW *WinnerWin;
	
	if( winner == TRUE )
	{
		mvwaddstr(FlagsWin,1,1,"           Clear          ");
		WinnerWin = Draw_WinnerWin(Game->Width,Game->Height);
		wattron(WinnerWin,COLOR_PAIR(1) | A_BOLD);
		mvwprintw(WinnerWin,2,9,"Well Done!");
		wattroff(WinnerWin,COLOR_PAIR(1) | A_BOLD);
		wattron(WinnerWin,COLOR_PAIR(7));
		mvwprintw(WinnerWin,4,5,"Time  %4ld seconds",Game->Timer);
		if( CheckScoresFile() == TRUE )
		{
			if( IsHighScore(Game->Timer,Game->Difficulty) == TRUE )
			{
				mvwaddstr(FlagsWin,1,1,"       A New Record!      ");
				wrefresh(FlagsWin);
				NewHighScore(Game);
				redrawwin(WinnerWin);
			}
		}
	}
	else
	{
		flash();
		mvwaddstr(FlagsWin,1,1,"                          ");
		/* Show all mines that are still covered */
		for(x_coord = 0;x_coord<Game->Width;x_coord++)
               		for(y_coord=0;y_coord<Game->Height;y_coord++)
                        	if( grid[x_coord][y_coord] == MINED 
				    && flags[x_coord][y_coord] != FLAGGED )
                                	mvwaddch(GameWin,y_coord+1,x_coord*2+1,MINE | COLOR_PAIR(2));
	        mvwaddch(GameWin,Game->Y,Game->X,MINE | A_BLINK | A_BOLD | COLOR_PAIR(2));
		wrefresh(GameWin);
	        WinnerWin = Draw_WinnerWin(Game->Width,Game->Height);
		wattron(WinnerWin,A_BLINK | A_BOLD | COLOR_PAIR(2));
		mvwprintw(WinnerWin,3,5,"You trod on a mine!");
		wattroff(WinnerWin,A_BLINK | A_BOLD);
	}

	curs_set(0);
	wattron(WinnerWin,COLOR_PAIR(7));
	mvwaddstr(WinnerWin,6,2,"(");
	waddch(WinnerWin,'P' | A_BOLD);
	waddstr(WinnerWin,")lay again, or (");
	waddch(WinnerWin,'Q' | A_BOLD);
	waddstr(WinnerWin,")uit?");
	wattroff(WinnerWin,COLOR_PAIR(7));
	wrefresh(WinnerWin);
	wrefresh(FlagsWin);

	fflush(stdin);
	ch = toupper(getch());
	
	KillWin(WinnerWin);
	curs_set(1);
	
	if( ch == 'Q' || ch == 27 )
		return FALSE;
	else
		return TRUE;
}



