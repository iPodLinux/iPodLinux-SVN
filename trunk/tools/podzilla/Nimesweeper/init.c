/* init.c */
/* init.c is part of Nimesweeper */
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

/* Function to initialise all the game variables,
   used when restarting the game also */
void Init_Game(GameStats *Game,struct timeval *start,char *Argv,unsigned int *restart)
{
	Init_GameStats(Game);
#ifdef USE_NCURSES
	Init_ncurses(Game,Argv);
#endif
	Init_Grid(Game);
	*restart = 2;
	gettimeofday(start,NULL);
#ifdef USE_NCURSES
	fflush(stdin);
	refresh();
	Draw_Grid(Game->Width,Game->Height);
	Draw_FlagsWin(Game->Width,Game->Height);
#endif
}


/* Initialise the game variables for different
   difficulty levels */
void Init_GameStats(GameStats *Game)
{
#ifdef USE_NCURSES
	switch(Game->Difficulty)
	{
		/* --newbie	Area 256 - 6.4 Clear : 1 Mine */
		case 1: 
			Game->Height = 16;
			Game->Width  = 16;
			Game->MinesSet = 40;
			Game->x = Game->Width/2;
			Game->y = Game->Height/2;
			break;
		/* --easy	Area 384 - 5.12 Clear : 1 Mine */
		case 2:
			Game->Height = 16;
			Game->Width  = 24;
			Game->MinesSet = 75 ;
			Game->x = Game->Width/2;
			Game->y = Game->Height/2;
			break;
		/* --medium	Area 480 - 4.8 Clear : 1 Mine */
		case 3: 
			Game->Height = 16;
			Game->Width  = 30;
			Game->MinesSet = 99;
			Game->x = Game->Width/2;
			Game->y = Game->Height/2;
			break;
		/* --hard	Area 760 - 4.75 Clear : 1 Mine */
		case 4:
			Game->Height = 20;
			Game->Width  = 38;
			Game->MinesSet = 160;
			Game->x = Game->Width/2;
			Game->y = Game->Height/2;
			break;
		/* In case something got really messed up. */
		default:
			endwin();
			fprintf(stderr,"\nInit_GameStats::switch(Game->Difficulty) == default:\n");
			exit(3);
	}
#else
	Game->Height = ylen;
	Game->Width  = xlen;
	Game->MinesSet = nummines;
	Game->x = Game->Width/2;
	Game->y = Game->Height/2;
#endif
	Game->Guesses = 0;
	Game->Timer = 0;
	Game->Correct = 0;
}



/* For the initialisation of the grid and the flags */
void Init_Grid(GameStats *Game)
{
	register int x, y, total;
	int height, width, mines_set, mines, taken;
	height = Game->Height;
	width  = Game->Width;
	mines  = Game->MinesSet;

	/* Initialise both arrays */
	for(x=0;x<width;x++)
		for(y=0;y<height;y++)
			grid[x][y] = flags[x][y] = 0;

	/* Now set the mines: use 9 to denote a mine,
	   use numbers 1-8 to denote how many mines
	   surround the square */
	for(mines_set=0;mines_set<mines;mines_set++)
	{
		/* Know a better way to get a random number?
		   Answers on a postcard to d4n13l@lycos.com */
		srand(time(NULL));
		x = rand() % width;
		y = rand() % height;
		taken = TRUE;
		do
		{
			if( grid[x][y] == FALSE )
			{
				taken = FALSE;
				grid[x][y] = MINED;
			}
			else
			{
				x = rand() % width;
				y = rand() % height;
			}
		}while(taken == TRUE);
	}

	/* Now set the numbered squares */
	for(x=0;x<width;x++)
		for(y=0;y<height;y++)
		{
			if( grid[x][y] == MINED )
				continue;
			total = 0;
			if( y > 0 )
				if( grid[x][y-1] == MINED )
					total++;
			if( x < width-1 && y > 0 )
				if( grid[x+1][y-1] == MINED )
					total++;
			if( x < width-1 )
				if( grid[x+1][y] == MINED )
					total++;
			if( x < width-1 && y < height-1 )
				if(grid[x+1][y+1] == MINED )
					total++;
			if( y < height-1 )
				if( grid[x][y+1] == MINED )
					total++;
			if( x > 0 && y < height-1 )
				if( grid[x-1][y+1] == MINED )
					total++;
			if( x > 0 )
				if( grid[x-1][y] == MINED )
					total++;
			if( x > 0 && y > 0 )
				if( grid[x-1][y-1] == MINED )
					total++;
			grid[x][y] = total;
		}
}


#ifdef USE_NCURSES
/* Initialise the ncurses environment */
void Init_ncurses(GameStats *Game, char *Argv0)
{
	short int Width, Height; /* To store screen size */
	unsigned char ch;
	static int tested = FALSE;
	
	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	noecho();
	curs_set(1);
	clear();
	
	getmaxyx(stdscr,Height,Width);
	if( Width+1 < Game->Width*2+2 || Height < Game->Height+4 )
	{
		endwin();
		fprintf(stderr,"%s:: Your terminal screen is too small.\n"
				,Argv0);
		fprintf(stderr,"%s:: It must be %dx%d or bigger.\n",
				Argv0,Game->Width*2+2,Game->Height+6);
		fprintf(stderr,"%s:: Current size is %dx%d\n",
				Argv0,Width,Height);
		exit(4);
	}
	
	if( has_colors() == FALSE && tested == FALSE )
	{
		printw("%s: Hmmm... your terminal does not appear"
		       " to support colour.\n",Argv0);
		printw("Do you wish to try without colour anyway [Y/n]?");
		refresh();
		
		fflush(stdin);
		ch = getch();
		if( ch == 'N' || ch == 'n' || ch == 27 )
		{
			endwin();
			exit(5);
		}
		
		clear();
	}
	tested = TRUE;
	start_color();
	init_pair(1,COLOR_CYAN, COLOR_BLACK);
	init_pair(2,COLOR_RED,COLOR_BLACK);
	init_pair(3,COLOR_GREEN,COLOR_BLACK);
	init_pair(4,COLOR_YELLOW,COLOR_BLACK);
	init_pair(5,COLOR_MAGENTA,COLOR_BLACK);
	init_pair(6,COLOR_BLUE,COLOR_BLACK);
	init_pair(7,COLOR_WHITE,COLOR_BLACK);
	init_pair(8,COLOR_YELLOW,COLOR_RED);
}
#endif


