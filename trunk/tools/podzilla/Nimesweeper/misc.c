/* misc.c */
/* misc.c part of Nimesweeper */
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
#include <string.h>
#include <signal.h>

/* This could be better... :-) 
   This function returns the difficulty level,
   depending on command line args */
int Args(int Argc, char **Argv)
{
	if( Argc == 1 )
		return DEFAULT_LEVEL;

	if( Argc == 2 )
	{
		if( strncmp(*(Argv+1),"--newbie",8) == 0 ||
		    strncmp(*(Argv+1),"-d1",3) == 0)
		    	return 1;
		if( strncmp(*(Argv+1),"--easy",6) == 0 || 
		    strncmp(*(Argv+1),"-d2",3) == 0 )
			return 2;
		if( strncmp(*(Argv+1),"--medium,",8) == 0 ||
		    strncmp(*(Argv+1),"-d3",3) == 0 )
			return 3;
		if( strncmp(*(Argv+1),"--hard",6) == 0 || 
		    strncmp(*(Argv+1),"-d4",3) == 0 )
			return 4;
		if( strncmp(*(Argv+1),"--version",9) == 0 ||
		    strncmp(*(Argv+1),"-v",2) == 0 )
		{
		    	ShowVersion(*Argv);
			return FALSE;
		}
		if( strncmp(*(Argv+1),"--new",5) == 0 ||
		    strncmp(*(Argv+1),"-n",2) == 0 )
		{
			CreateScoresFile();
			return FALSE;
		}
	}

	fprintf(stderr,"Usage: %s [OPTION]\n",Argv[0]);
	fprintf(stderr,"-d[level]\tSet difficulty level:\n");
	fprintf(stderr,"  -d1, --newbie\tDifficulty level is newbie:\n");
	fprintf(stderr,"  \t\tMinefield - 16x16, Mines - 40.\n");
	fprintf(stderr,"  -d2, --easy\tDifficulty level is easy:\n");
	fprintf(stderr,"  \t\tMinefield - 16x24, Mines - 75. (Default)\n");
	fprintf(stderr,"  -d3, --medium\tDifficulty level is medium:\n");
	fprintf(stderr,"  \t\tMinefield - 30x16, Mines - 99.\n");
	fprintf(stderr,"  -d4, --hard\tDifficulty level is hard:\n");
	fprintf(stderr,"  \t\tMinefield - 38x20, Mines - 160.\n");
	fprintf(stderr,"-h, --help\tShow this help\n");
	fprintf(stderr,"-n, --new\t(Maybe as root) Create a new scores file:\n");
	fprintf(stderr,"\t\t(%s)\n",HIGHSCORES);
	fprintf(stderr,"-v, --version\tShow version information\n");
	fprintf(stderr,"-Game Controls-\n");
	fprintf(stderr,"Move Cursor:           Arrow keys.\n");
	fprintf(stderr,"Flag/Unflag a square:  %c\n",KEY__FLAG);
	fprintf(stderr,"Uncover a square:      Space\n");
	fprintf(stderr,"Pause/Unpause game:    %c\n",KEY__PAUSE);
	fprintf(stderr,"Show help:             %c, F1\n",KEY__HELP);
	fprintf(stderr,"Show best times:       %c, F2\n",KEY__TIMES);
	fprintf(stderr,"Change difficulty:     %c, F3\n",KEY__DIFF);
	fprintf(stderr,"Restart game:          %c, F5\n",KEY__RESTART);
	fprintf(stderr,"Quit:                  %c, Esc\n",KEY__QUIT);
	fprintf(stderr,"\nReport bugs to d4n13l@lycos.com\n");

	return FALSE;
}


/* To handle signals */
void CleanUp(int signal)
{
#ifdef USE_NCURSES
	clear();
	refresh();
	endwin();
#endif

	fprintf(stderr,"\n*** Caught Signal ");

	switch(signal)
	{
		case SIGTERM:
                        fprintf(stderr,"SIGTERM");
                        break;
                case SIGHUP:
                        fprintf(stderr,"SIGHUP");
                        break;
                case SIGILL:
                        fprintf(stderr,"SIGILL");
                        break;
                case SIGSEGV:
                        fprintf(stderr,"SIGSEGV");
                        break;
                case SIGKILL:
                        fprintf(stderr,"SIGKILL");
                        break;
                case SIGQUIT:
                        fprintf(stderr,"SIGQUIT");
                        break;
                case SIGBUS:
                        fprintf(stderr,"SIGBUS");
			break;
                default:
                        fprintf(stderr,"Unspecified");
                        break;
	}
	fprintf(stderr," (%d) ***\n",signal);
	exit(6);
}

/* Show version information */
void ShowVersion(char *Exec)
{
	fprintf(stdout,"%s Version %s by Daniel Burnett\n",Exec,VERSION);
	fprintf(stdout,"Last updated: %s\n",DATE);
	fprintf(stdout,"%s comes with no warranty.\n",Exec);
	fprintf(stdout,"Report bugs to %s\n",EMAIL);
}

/* Function that opens Highscores file with given Mode.
   Returns either a valid FILE pointer to the HIGHSCORES
   file, or NULL on failure. */
FILE *OpenFile(char *Mode)
{
	FILE *File;
	
	if( (File = fopen(HIGHSCORES,Mode)) == NULL )
	{
#ifdef USE_NCURSES
		beep();
		def_prog_mode();
		endwin();
#endif
		perror(HIGHSCORES);
		sleep(2);
#ifdef USE_NCURSES
		reset_prog_mode();
#endif
		return NULL;
	}
	else
		return File;
}

/* Prints error message if fread or fwrite fail */
void FileError(FILE *File, char *Type)
{
#ifdef USE_NCURSES
	beep();
	def_prog_mode();
	endwin();
#endif
	fprintf(stderr,"A %s error has occured\n",Type);
	perror(HIGHSCORES);
	clearerr(File);
	fclose(File);
	sleep(2);
#ifdef USE_NCURSES
	reset_prog_mode();
#endif
}

/* To create and initialise a new Highscores file.
   Only called if -n, or --new, is given on the command line. */
void CreateScoresFile(void)
{
	FILE *ScoresFile;
	int ch = 'N';
	int i;
	HighScores HighArray[ENTRIES];
	HighScores *HighPtr;
	
	HighPtr = HighArray;

#ifdef USE_NCURSES
	printf("Create new highscores file (%s) [y/N]?",HIGHSCORES);
	fflush(stdin);	
	ch = fgetc(stdin);
#else
	ch = 'y';
#endif
	if( ch == 'Y' || ch == 'y' )
	{
		if( (ScoresFile = fopen(HIGHSCORES,"wb")) == NULL )
		{
#ifdef USE_NCURSES
			perror(HIGHSCORES);
			exit(7);
#else
			fprintf(stderr,"%s has not been created.\n",HIGHSCORES);
			return;
#endif
		}

		for(i=0;i<ENTRIES;i++)
		{
			HighPtr->Time = 999;
			HighPtr->Name[0] = 'A';
			HighPtr->Name[1] = 'A';
			HighPtr->Name[2] = 'A';
			HighPtr->Name[3] = '\0';
			/* For CheckScoresFile() to be happy */
			HighPtr->Name[4] = '\0';
			HighPtr->Name[5] = '\0';
			HighPtr->Name[6] = '\0';
			HighPtr->Name[7] = '\0';
			HighPtr->Name[8] = '\0';
			HighPtr->Name[9] = '\0';
			HighPtr->Name[10] = '\0';
			HighPtr++;
		}
	
		if( fwrite(&HighArray,sizeof(HighArray),1,ScoresFile) != 1 )
		{
			fprintf(stderr,"A write error occured.\n");
			exit(8);
		}
		fclose(ScoresFile);
		printf("%s has been created.\n",HIGHSCORES);
		printf("Now check the permissions are suitable.\n");
	}
	else
	{
		fprintf(stderr,"%s has not been created.\n",HIGHSCORES);
	}
}


/* To check integrity of HIGHSCORES file */
int CheckScoresFile(void)
{
	FILE *ScoresFile;
	HighScores HighArray[40];
        HighScores *HighPtr;
	register int i,j;
	
        HighPtr = HighArray;

	if( (ScoresFile = OpenFile("rb")) == NULL )
		return FALSE;

	if( fseek(ScoresFile,0L,SEEK_END) != 0)
	{
		fclose(ScoresFile);
#ifdef USE_NCURSES
		beep();
		def_prog_mode();
		endwin();
#endif
		fprintf(stderr,"Could not access end of %s\n",HIGHSCORES);
		sleep(2);
#ifdef USE_NCURSES
		reset_prog_mode();
#endif
		return FALSE;
	}

	if( ftell(ScoresFile) != (sizeof(HighScores)) * 40 )
	{
		fclose(ScoresFile);
#ifdef USE_NCURSES
		beep();
		def_prog_mode();
		endwin();
#endif
		fprintf(stderr,"%s is corrupt\n",HIGHSCORES);
		sleep(2);
#ifdef USE_NCURSES
		reset_prog_mode();
#endif
		return FALSE;
	}
	
	/* Seek beginning */
	if( fseek(ScoresFile,0L,SEEK_SET) != 0 )
	{
		FileError(ScoresFile,"seek");
		return FALSE;
	}
	
	/* Read all entries (ENTRIES) into an array */
	if( fread(&HighArray,sizeof(HighArray),1,ScoresFile) != 1 )
	{
		FileError(ScoresFile,"read");
		return FALSE;
	}
	
	fclose(ScoresFile);
	
	/* Inspect each entry for suspicious data */
	for(i=0;i<ENTRIES;i++)
	{
		/* greater than a 72 Hour best time? Unless they were stoned maybe... */
		if( HighPtr->Time > 259200 || HighPtr->Time < 0 )
		{
#ifdef USE_NCURSES
			beep();
			def_prog_mode();
			endwin();
#endif
			fprintf(stderr,"%s looks like it is corrupt\n",HIGHSCORES);
			fprintf(stderr,"The time \"%ld\" is too far out.\n",HighPtr->Time);
			sleep(2);
#ifdef USE_NCURSES
			reset_prog_mode();
#endif
			return FALSE;
		}
		for(j=0;j<NAMELEN;j++)
		{
			/* Check chars are valid */
			if( NameCharIsOk(HighPtr->Name[j]) || HighPtr->Name[j] == '\0' )
			{
			    	continue;
			}
			else
			{
#ifdef USE_NCURSES
				beep();
				def_prog_mode();
				endwin();
#endif
				fprintf(stderr,"%s looks like it is corrupt.\n",HIGHSCORES);
				fprintf(stderr,"The name \"%s\" has invalid characters.\n",HighPtr->Name);
				sleep(2);
#ifdef USE_NCURSES
				reset_prog_mode();
#endif
				return FALSE;
			} 
		}
		if( HighPtr->Name[NAMELEN] != '\0' )
		{
#ifdef USE_NCURSES
			beep();
			def_prog_mode();
			endwin();
#endif
			fprintf(stderr,"%s looks like it is corrupt\n",HIGHSCORES);
			fprintf(stderr,"No terminating null value found for name.\n");
			sleep(2);
#ifdef USE_NCURSES
			reset_prog_mode();
#endif
			return FALSE;
		}
			
		HighPtr++;
	}
	return TRUE;
}

