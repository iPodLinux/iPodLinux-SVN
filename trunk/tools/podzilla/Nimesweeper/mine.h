/* mine.h */
/* mine.h is part of Nimesweeper */
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
#ifndef __MINE_H__
#define __MINE_H__
#include <ctype.h>
#ifdef USE_NCURSES
#include <ncurses.h>
#else
#define MWINCLUDECOLORS
#include <nano-X.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

/* Misc */
#define VERSION    ("1.0")
#define DATE       ("17th November 2002")
#define EMAIL      ("d4n13l@lycos.com")
#ifdef USE_NCURSES
#define HIGHSCORES ("/usr/share/nimesweeper/HighScores.dat")
#else
#define HIGHSCORES (".minesweeper")
#endif
/* HIGHSCORES is a binary file, 40 items of HighScores,
   a top ten for each difficulty level */
/* For HighScores structure */
#define MAXNAME (11)
#define NAMELEN (MAXNAME-1)
#define LEVELS  (4)
#define ENTRIES (LEVELS * 10)

/* Minefield Symbols */
#define CLEAR	(' ')
#define FLAG	('P')
#define MINE	('X')
#define UNKNOWN ('-')

/* Status Flags */
#define TRUE 	1
#define FALSE	0
#define MINED	(9)
#define FLAGGED	(2)

/* Default difficulty level if not specified on command line */
#define DEFAULT_LEVEL (2)	

/* Gameplay Keys */
#define KEY__DIFF	('C')  /* To Change Difficulty */
#define KEY__FLAG	('F')  /* To Flag a square */
#define KEY__UNCOVER	(' ')  /* Spacebar - to Unover a Square */
#define KEY__PAUSE	('P')  /* To Pause Game and Timer */
#define KEY__RESTART	('R')  /* To Restart the Game */
#define KEY__TIMES	('T')  /* To Show best times */
#define KEY__HELP	('H')  /* To Show on Screen Help */
#define KEY__QUIT	('Q')  /* To Quit */

#define SQSIZE 12
int ylen;
int xlen;
int xoff;
int yoff;
int nummines;

/* Screen Coordinates relative to the grid coordinates */
#ifdef USE_NCURSES
#define X x*2+1
#define Y y+1
#else
#define X x*SQSIZE+(xoff+3)-(ylen*xlen)*(int)(x/xlen)
#define Y (yoff+11)+SQSIZE*y
#endif

/* Structure for storing dynamic game data */
typedef struct _Stats
{
	int Height;	/* Height of game board */
	int Width;	/* Width of game board */
	int MinesSet;	/* Number of mines set on board */
	int Guesses;	/* Number of flags used */
	int Correct;	/* Number of flags placed correctly */
	int x;		/* Posistion of cursor on x axis */
	int y;		/* Posistion of cursor on y axis */
	long int Timer;	/* To store game time, in seconds */
	int Difficulty;	/* Difficulty level, 1-4 */
} GameStats;

/* Structure for Highscores file */
typedef struct _HighScores
{
	long int Time;	/* Time taken to clear mine field */
	char Name[MAXNAME];	/* Name of player */
} HighScores;


/* Mine Field is just a 2 Dimensional grid of short int's 
   Use 0 (FALSE) for a "clear" square, 9 (MINED) for a mined square,
   or 1-7 depending on number of surrounding mines */
short int grid[50][20];

/* Create am identical array to store status of 
   Covered/Uncovered/Flagged squares.
   Use FLAGGED for a square Flagged with KEY__FLAG
   Use FALSE for an UNKNOWN square
   Use TRUE for a square uncovered with KEY__UNCOVER */
short int flags[50][20];

/* Global WINDOWS */
#ifdef USE_NCURSES
WINDOW *GameWin;
WINDOW *FlagsWin;
#else
extern GR_WINDOW_ID mines_wid;
extern GR_GC_ID mines_gc;
extern GR_SCREEN_INFO screen_info;
#endif


/* Various initialisation type functions, in init.c */
void Init_ncurses(GameStats *Game, char *Argv0);
void Init_GameStats(GameStats *Game);
void Init_Grid(GameStats *Game);
void Init_Game(GameStats *Game,struct timeval *start,char *Argv,unsigned int *restart);


/* Game Play functions, in gameplay.c */
int  ChangeDifficultyLevel(GameStats *Game);
void ClearPath(register int x, register int y, int width, int height);
int  FlagSquare(GameStats *Game);
void MarkSquare(register int x, register int y);
void MoveUp(GameStats *Game);
void MoveDown(GameStats *Game);
void MoveRight(GameStats *Game);
void MoveLeft(GameStats *Game);
int  Pause(GameStats *Game, struct timeval *start, struct timeval *stop);
void ShowHelp(int Width, int Height);
void ShowRemainingFlags(GameStats *Game);
int  Uncover(int x, int y, int width, int height);
void UncoverBoundary(register int x, register int y, int width, int height);
int  Winner(GameStats *Game, int winner);

#ifdef USE_NCURSES
/* WINDOW related functions, in windows.c */
void KillWin(WINDOW *Window);
void Draw_Grid(int Width, int Height);
void Draw_FlagsWin(int Width, int Height);
WINDOW *Draw_WinnerWin(int Width, int Height);
WINDOW *Draw_ChangeDifficultyWin(int Width,int Height);
WINDOW *Draw_HelpWin(int Width, int Height);
WINDOW *Draw_HighScoreWin(int Width, int Height);
#endif

/* High Score functions, in scores.c */
int  IsHighScore(long int time, int difficulty);
void NewHighScore(GameStats *Game);
int  WriteNewRecord(long int Timer,int Difficulty,char *Name);
int  ShowHighScores(GameStats *Game, int Position);
int  NameCharIsOk(char ch);
/* Information functions (not using Ncurses), in misc.c */
int  Args(int Argc, char **Argv);
void ShowVersion(char *Exec);

/* Signal Handler, in misc.c */
void CleanUp(int signal);

/* File related functions, in misc.c */
FILE *OpenFile(char *Mode);
void FileError(FILE *File, char *Type);
int  CheckScoresFile(void);
void CreateScoresFile(void);


/* For... cheaters :) */
/* #define CHEATERS_R_US */

#endif

