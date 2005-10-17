/*

Name:
MARCHIVE.C

Description:
These routines are used to detect different archive/compression formats and
decompress/de-archive the mods from them if necessary

Portability:
All systems - all compilers

Steve McIntyre <stevem@chiark.greenend.org.uk>

Feel free to use/modify this code if you like, let me know if you improve
it! 

HISTORY
=======

v1.00 (06/12/96) - first "versioned" version
v1.01 (03/01/97) - Changed from #defined archiver markers and commands
	to an array to be stepped through. Should be easier for users to add
	their own commands later.
v1.02 (10/01/97) - minor changes for compilation with -Wall
v1.03 (09/02/97) - now loads supplied file from supplied archive...
			a little kludgy, but works (Peter Amstutz)
v1.04 (20/04/97) - de-kludged it :)  Now works somewhat better.  Pass
			archive & file you want out of it, and it passes back the
			filename to find the decompressed file
v1.05 (24/11/97) - Stephan Kanthak: SGI workaround for fnmatch
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>

#ifdef NEED_UNISTD
#include <unistd.h>
#endif /* NEED_UNISTD */

#include "mikmod.h"
#include "mikmodux.h"

/* 
	==================================================================
	Use similar signature idea to "file" to see what format we have...
	==================================================================
*/

ARCHIVE MA_archiver[]={
/* 
location marker  command 
*/

    {0,	"PK",	"unzip -p",	"unzip -l", 27},	/* PKZIP archive */
    {2,	"-l",	"lha pq",	"lha v", 65},				/* LHA/LZH archive */
    {1,	"ê",	"unarj c",	"unarj l", 0},				/* ARJ */
    {0,	"",	"gzip -dc",	"gzip -l", 27},		/* GZIP */
    {0,	"ZOO",	"zoo xpq",	"zoo -list", 50},	/* ZOO */
    {0, NULL,   NULL}		/* needed to end the array... */
};

int *MA_identify(char *filename, int header_location, char *header_string)
{
	FILE *fp;
	char id[16]; /* hopefully should be big enough for "signature" */

	if((fp=fopen(filename,"rb"))==NULL){
		return NULL;
	}
	modfp=fp;
	_mm_rewind(modfp);
	if(header_location)
		_mm_fseek(modfp,header_location,SEEK_SET);
	if(!fread(id,strlen(header_string),1,modfp)) 
	{
		fclose(fp);
		return 0;
	}
	if(!memcmp(id,(char *)header_string,strlen(header_string))) 
	{
		fclose(fp);
		 return (int *) 1;
	}
	fclose(fp);

	return 0;
}

char *MA_dearchive(char *arc, char *file)
{
	int t;
	char *tmp_file;
	char command_buff[512];
	char *vc;

	int archive=0;
	tmp_file=(char *)malloc(256);

	if(arc==NULL || arc[0]==0) 
	{
		memcpy(tmp_file, file, 256);
		return tmp_file;
	}

	if ((vc=getenv("HOME"))) {
      		strcpy(tmp_file, vc);
	} else {
      		strcpy(tmp_file, "/tmp");
	}
 	strcat(tmp_file, "/.modXXXXX");

	for(t=0; MA_archiver[t].command!=NULL; t++)
	  	{
		if(MA_identify(arc,MA_archiver[t].location,MA_archiver[t].marker))
	   		{
			sprintf(command_buff,"%s %s \"%s\" >%s 2>/dev/null",MA_archiver[t].command,arc,file,tmp_file);
			archive=1;
			break;
			}
		}

	if(archive){
		/* display "loading" message, as this may take some time... */
#ifndef IPOD
		display_version();
		display_extractbanner();
#endif
		system(command_buff);
	}

	return (char *) tmp_file;
}

void MA_FindFiles(PLAYLIST *pl, char *filename)
{
	int t;
	//char command_buff[512];
	int archive=0;
	char string[255];
	FILE *file;
#if (defined(SGI))
	char *pointpos;
#endif

	for(t=0; MA_archiver[t].command!=NULL; t++)
	  	{
		if(MA_identify(filename,MA_archiver[t].location,MA_archiver[t].marker))
	   		{
			archive=t+1;
			break;
			}
		}

	if(archive--)
		{
		sprintf(string,"%s %s", MA_archiver[archive].listcmd, filename);
		file=popen(string, "r");
		fgets(string,255,file);
		while(!feof(file))
		{
			string[strlen(string)-1]=0;
			if(MA_archiver[archive].nameoffset==0)
			{
				for(t=0; string[t]!=' '; t++);
				string[t]=0;
			}
#if (defined(SGI))
		        pointpos = strrchr(string+MA_archiver[archive].nameoffset, '.');
			if ((pointpos != NULL) &&
			    ((strcasecmp(pointpos+1, "MOD") == 0) ||
			     (strcasecmp(pointpos+1, "S3M") == 0) ||
			     (strcasecmp(pointpos+1, "STM") == 0) ||
			     (strcasecmp(pointpos+1, "UNI") == 0) ||
			     (strcasecmp(pointpos+1, "MTM") == 0) ||
			     (strcasecmp(pointpos+1, "ULT") == 0) ||
			     (strcasecmp(pointpos+1, "IT") == 0) ||
			     (strcasecmp(pointpos+1, "XM") == 0) ||
			     (strncasecmp(string+MA_archiver[archive].nameoffset, "MOD", 3) == 0) ||
			     (strncasecmp(string+MA_archiver[archive].nameoffset, "S3M", 3) == 0) ||
			     (strncasecmp(string+MA_archiver[archive].nameoffset, "STM", 3) == 0) ||
			     (strncasecmp(string+MA_archiver[archive].nameoffset, "UNI", 3) == 0) ||
			     (strncasecmp(string+MA_archiver[archive].nameoffset, "MTM", 3) == 0) ||
			     (strncasecmp(string+MA_archiver[archive].nameoffset, "ULT", 3) == 0) ||
			     (strncasecmp(string+MA_archiver[archive].nameoffset, "IT", 2) == 0) ||
			     (strncasecmp(string+MA_archiver[archive].nameoffset, "XM", 2) == 0)))
#else
			if(!fnmatch("*.[Mm][Oo][Dd]",(string+MA_archiver[archive].nameoffset),0) ||
				!fnmatch("*.[Ss]3[Mm]",(string+MA_archiver[archive].nameoffset),0) ||
				!fnmatch("*.[Ss][Tt][Mm]",(string+MA_archiver[archive].nameoffset),0) ||
				!fnmatch("*.[Un][Nn][Ii]",(string+MA_archiver[archive].nameoffset),0) ||
				!fnmatch("*.[Mm][Tt][Mm]",(string+MA_archiver[archive].nameoffset),0) ||
				!fnmatch("*.[Uu][Ll][Tt]",(string+MA_archiver[archive].nameoffset),0) ||
				!fnmatch("*.[Ii][Tt]",(string+MA_archiver[archive].nameoffset),0) ||
				!fnmatch("*.[Xx][Mm]",(string+MA_archiver[archive].nameoffset),0))
#endif
			{
				PL_Add(pl,(string+MA_archiver[archive].nameoffset),filename);
			}
			fgets(string,255,file);
		}
		pclose(file);
	}
	else PL_Add(pl, filename, NULL);
}
