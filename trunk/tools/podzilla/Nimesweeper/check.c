/* check.c */
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA  */

/* Simple program to dump contents of Highscores file */

#include "mine.h"

#define SCOREFILE ((argc==1) ? HIGHSCORES : *(argv+1))

int main(int argc, char **argv)
{
	
	FILE *fp;
	HighScores HighArray[ENTRIES];
	register int i;
	char *Levels[] = {"Newbie","Easy","Medium","Hard" };

	if( argc != 1 && argc != 2 )
	{
		fprintf(stderr,"Usage: %s [file]\n",*argv);
		return 1;
	}
	
	if( (fp = fopen(SCOREFILE,"rb")) == NULL )
	{
		perror(SCOREFILE);
		return 2;
	}
	
	if( fread(&HighArray,sizeof(HighArray),1,fp) != 1 )
	{
		fprintf(stderr,"%s: error reading file %s\n",*argv,SCOREFILE );
		fclose(fp);
		return 3;
	}
	fclose(fp);
	
	printf("\n|        %s         |",Levels[0]);
	printf("         %s          |",Levels[1]);
	printf("        %s         |",Levels[2]);
        printf("         %s          |\n\n",Levels[3]);

	for(i=0;i<10;i++)
	{
		printf("| %2d - %-10s  %4ld | "
			,i+1,HighArray[i].Name,HighArray[i].Time);
		printf("%2d - %-10s  %4ld | "
			,i+1,HighArray[i+10].Name,HighArray[i+10].Time);
		printf("%2d - %-10s  %4ld | "
			,i+1,HighArray[i+20].Name,HighArray[i+20].Time);
		printf("%2d - %-10s  %4ld | \n"
			,i+1,HighArray[i+30].Name,HighArray[i+30].Time);
	}
	return 0;
}
