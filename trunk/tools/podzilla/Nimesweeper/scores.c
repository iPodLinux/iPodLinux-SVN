/* scores.c */
/* scores.c part of Nimesweeper */
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

#include <string.h>
#include "mine.h"
#include "../pz.h"

#define HOF_XOFF ((screen_info.cols/2)/2)+(screen_info.cols/2)-40

/* Gets name of player and adds a new high score to relevant
   position in HIGHSCORES file through WriteNewRecord(). */
void NewHighScore(GameStats *Game)
{
#ifdef USE_NCURSES
	WINDOW *HighScoreWin;
#endif
	char name[MAXNAME];
	int Position = FALSE;

#ifdef USE_NCURSES
	unsigned int i,Input = FALSE;
	echo();
	HighScoreWin = Draw_HighScoreWin(Game->Width,Game->Height);
	wattron(HighScoreWin,A_BOLD | COLOR_PAIR(7));
	mvwaddstr(HighScoreWin,4,7,"Congratulations!");
	wattroff(HighScoreWin,A_BOLD);
	mvwaddstr(HighScoreWin,6,3,"You Have Made it into The");
	mvwaddstr(HighScoreWin,7,3,"Nimesweeper Hall of Fame!");
	mvwaddstr(HighScoreWin,9,7,"Enter Your Name:");
	wrefresh(HighScoreWin);
	do
	{
		Input = FALSE;
		fflush(stdin);
		mvwgetnstr(HighScoreWin,11,10,name,NAMELEN);
		name[NAMELEN] = '\0';
		/* Only accept chars A-Z,a-z,
		   Space and '-', for the name. */
		for(i=0;i<MAXNAME;i++)
		{
			if( (name[i] == '\0' || name[i] == '\n')
			     && i != 0 )
			{
				Input = TRUE;
				break;
			}

			if( NameCharIsOk(name[i]) )
				Input = TRUE;
			else
			{
				beep();
				flash();
				curs_set(0);
				mvwaddstr(HighScoreWin,11,8,"Invalid input.");
				mvwaddstr(HighScoreWin,12,6,"Accepted chars: A-Z,");
				mvwaddstr(HighScoreWin,13,6,"a-z, 0-9, Space or -");
				wrefresh(HighScoreWin);
				sleep(3);
				mvwaddstr(HighScoreWin,11,8,"              ");
				mvwaddstr(HighScoreWin,12,6,"                    ");
				mvwaddstr(HighScoreWin,13,6,"                    ");
				wrefresh(HighScoreWin);
				Input = FALSE;
				curs_set(1);
				break;
			}
		}
	}
	while( Input == FALSE );

	/* So the file chars are 100% known */
	for(i=0;i<MAXNAME;i++)
	{
		if( NameCharIsOk(name[i]) )
		    	continue;
		else
			name[i] = '\0';
	}

	noecho();
	werase(HighScoreWin);
#else
	strcpy(name, "You");
#endif

	/* Write new record to file */
	if( (Position = WriteNewRecord(Game->Timer,Game->Difficulty,name)) == FALSE )
	{
#ifdef USE_NCURSES
		/* if it fails, clean up windows */
		KillWin(HighScoreWin);
		redrawwin(GameWin);
		wrefresh(GameWin);
#else
		new_message_window("Highscores failed");
#endif
	}
	else
	{
		ShowHighScores(Game,Position);
	}
}

/* Checks input for players name for inserting into
   the HIGHSCORES file.
   Must be between: A-Z; a-z; 0-9. Space or a '-'. */
int  NameCharIsOk(char ch)
{
	if( (ch >= 'A' && ch <= 'Z') ||
	    (ch >= 'a' && ch <= 'z') ||
	    (ch >= '0' && ch <= '9') ||
	    (ch == ' ' || ch == '-') )
	    	return TRUE;
	else
		return FALSE;
}

/* Shows top ten times for given difficulty
   Position is used to highlight name, passed FALSE
   if not a new entry. */
int  ShowHighScores(GameStats *Game, int Position)
{
	FILE *ScoresFile;
#ifdef USE_NCURSES
	WINDOW *HighScoreWin;
#endif
	HighScores HighArray[10];
	int i;

#ifdef USE_NCURSES
	char *Levels[] = {"Newbie","Easy","Medium","Hard"};
	HighScoreWin = Draw_HighScoreWin(Game->Width,Game->Height);
	curs_set(0);
#endif
	if( (ScoresFile = OpenFile("rb")) == NULL)
	{
#ifdef USE_NCURSES
		curs_set(1);
		KillWin(HighScoreWin);
		redrawwin(GameWin);
		wrefresh(GameWin);
#else
		new_message_window("Read failed");
#endif
		return FALSE;
	}

	/* Locate "top ten" times, using difficulty as the offset */
	if( fseek(ScoresFile,sizeof(HighScores)*(Game->Difficulty-1)*10,SEEK_SET) != 0 )
	{
#ifdef USE_NCURSES
		curs_set(1);
		FileError(ScoresFile,"seek");
		KillWin(HighScoreWin);
		redrawwin(GameWin);
		wrefresh(GameWin);
#else
		new_message_window("Seek failed");
#endif
		return FALSE;
	}
	/* read top ten entries for given difficulty into an array */
	if( fread(&HighArray,sizeof(HighArray),1,ScoresFile) != 1 )
	{
#ifdef USE_NCURSES
		curs_set(1);
		FileError(ScoresFile,"read");
		KillWin(HighScoreWin);
		redrawwin(GameWin);
		wrefresh(GameWin);
#else
		new_message_window("Read failed");
#endif
		return FALSE;
	}

	fclose(ScoresFile);

	/* Print the array, highlighting new entry (if there is one) */
#ifdef USE_NCURSES
	wattron(HighScoreWin, COLOR_PAIR(7));
	mvwprintw(HighScoreWin,1,3,"Nimesweeper Hall of Fame");
	mvwprintw(HighScoreWin,3,7,"Difficulty %6s",Levels[Game->Difficulty-1]);
#else
	GrText(mines_wid, mines_gc, HOF_XOFF, 12, "Hall of Fame:",  -1, GR_TFASCII);
#endif
	for(i=0;i<7;i++)
	{
		if( Position == i+1 )
		{
#ifdef USE_NCURSES
			wattron(HighScoreWin,A_BOLD);
			mvwprintw(HighScoreWin,5+i,5,"%-2d  %-10s  %-5ld",
			  i+1,HighArray[i].Name,HighArray[i].Time);
			wattroff(HighScoreWin,A_BOLD);
#else
			char scorestring[128];

			sprintf(scorestring," %-2d  %-5s  %-4ld", i+1, HighArray[i].Name, HighArray[i].Time);
			GrText(mines_wid, mines_gc, HOF_XOFF, 26+i*12,scorestring,  -1, GR_TFASCII);
#endif
		}
		else
		{
#ifdef USE_NCURSES
			mvwprintw(HighScoreWin,5+i,5,"%-2d  %-10s  %-5ld",
			  i+1,HighArray[i].Name,HighArray[i].Time);
#else
			char scorestring[128];

			sprintf(scorestring, " %-2d  %-5s  %-4ld", i+1, HighArray[i].Name, HighArray[i].Time);
			GrText(mines_wid, mines_gc, HOF_XOFF, 26+i*12,scorestring,  -1, GR_TFASCII);
#endif
		}
	}

#ifdef USE_NCURSES
	wrefresh(HighScoreWin);
	getch();
	curs_set(1);
	KillWin(HighScoreWin);
	redrawwin(GameWin);
	wrefresh(GameWin);
#endif
	return TRUE;
}


/* Write new high score to file.
   Returns "position" of entry if successful, FALSE on error. */
int WriteNewRecord(long int Timer,int Difficulty,char *Name)
{
	FILE *ScoresFile;
	HighScores HighArray[10];

	int position,i;

	if( (ScoresFile = OpenFile("rb")) == NULL )
		return FALSE;

	/* Locate "top ten" times, using difficulty as the offset */
	if( fseek(ScoresFile,sizeof(HighScores)*(Difficulty-1)*10,SEEK_SET) != 0 )
	{
		FileError(ScoresFile,"seek");
		return FALSE;
	}
	/* Read top ten entries for given difficulty into an array */
	if( fread(&HighArray,sizeof(HighArray),1,ScoresFile) != 1 )
	{
		FileError(ScoresFile,"read");
		return FALSE;
	}
	fclose(ScoresFile);

	/* Locate 'position' in top 10 in which to insert the new record */
	for(position=0;position<10;position++)
		if( Timer <= HighArray[position].Time )
			break;

	/* From 'position', copy each record down one place*/
	for(i=9;i>position;i--)
		HighArray[i] = HighArray[i-1];

	/* Copy time taken to array */
	HighArray[position].Time = Timer;

	/* Copy Name to Array */
	for(i=0;i<MAXNAME;i++)
		HighArray[position].Name[i] = *(Name+i);

	/* Make *Sure* that Name ends with NULL, just to be safe */
	HighArray[position].Name[NAMELEN] = '\0';

	if( (ScoresFile = OpenFile("rb+")) == NULL )
		return FALSE;

	/* Re-seek what we got earlier */
	if( fseek(ScoresFile,sizeof(HighScores)*(Difficulty-1)*10,SEEK_SET) != 0 )
	{
		FileError(ScoresFile,"seek");
		return FALSE;
	}
	/* Write the array to file */
	if( fwrite(&HighArray,sizeof(HighArray),1,ScoresFile) != 1 )
	{
		FileError(ScoresFile,"write");
		return FALSE;
	}
	fclose(ScoresFile);

	/* Remember the array starts with 0... */
	return position+1;
}

/* Checks if time taken will make it into top ten,
   returns TRUE or FALSE. */
int  IsHighScore(long int time, int difficulty)
{
	FILE *ScoresFile;
	HighScores HighArray[40];

	if( (ScoresFile = OpenFile("rb")) == NULL )
		return FALSE;

	/* Read entire file into an array */
	if( fread(&HighArray,sizeof(HighArray),1,ScoresFile) != 1 )
	{
		FileError(ScoresFile,"read");
		return FALSE;
	}

	fclose(ScoresFile);

	switch(difficulty)
	{
		case 1:
			if( HighArray[9].Time < time )
				return FALSE;
			else
				return TRUE;
			break;
		case 2:
			if( HighArray[19].Time < time )
				return FALSE;
			else
				return TRUE;
			break;
		case 3:
			if( HighArray[29].Time < time )
				return FALSE;
			else
				return TRUE;
			break;
		case 4:
			if( HighArray[39].Time < time )
				return FALSE;
			else
				return TRUE;
			break;
		default:
			/* Just in case */
#ifdef USE_NCURSES
			endwin();
#endif
			fprintf(stderr,"IsHighScore:: difficulty is invalid\n");
			exit(9);
			break;
	}

	return FALSE;
}
