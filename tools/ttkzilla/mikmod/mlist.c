/*

MLIST.C

Description:
Functions to handle the playlist.
Note: ironically, the tcl/tk interface will replace these functions
to interact with the playlist held in the frontend, not with mikmod.


Peter Amstutz <amstpi@freenet.tlh.fl.us>


HISTORY 
======

v1.00 (20/01/97) - first version

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "mikmod.h"
#include "mikmodux.h"

void PL_InitList(PLAYLIST *pl)
{
	pl->numused=0;
	pl->current=-1;
	pl->file=NULL;
	pl->archive=NULL;
}

void PL_ClearList(PLAYLIST *pl)
{
	int i;
	for(i=0;i<pl->numused;i++)
	{
		free(pl->file[i]);
		pl->file=NULL;
		free(pl->archive[i]);
		pl->archive[i]=NULL;
	}
	if(pl->file!=NULL) {
		free(pl->file);
		pl->file=NULL;
	}
	if(pl->archive!=NULL) {
		free(pl->archive);
		pl->archive=NULL;
	}
	pl->current=0;
	pl->numused=0;
}

int PL_GetCurrent(PLAYLIST *pl, char *retfile, char *retarc)
{
	if(pl->numused==0) return 1;
	strcpy(retfile, pl->file[pl->current>0 ? pl->current : 0]);
	if(pl->archive[pl->current>0 ? pl->current : 0]!=NULL) 
		strcpy(retarc,pl->archive[pl->current>0 ? pl->current : 0]);
	else strcpy(retarc,"");
	if(pl->current==(pl->numused-1)) return 1;
	else return 0;
}

int PL_GetNext(PLAYLIST *pl, char *retfile, char *retarc)
{
	int ret=0;

	if(pl->numused==0) return 1;
	pl->current++;
	if(pl->current==pl->numused) {pl->current=pl->numused-1; ret=1;}

	if(retfile!=NULL) strcpy(retfile, pl->file[pl->current>-1 ? pl->current : 0]);
	if(pl->archive[pl->current>-1 ? pl->current : 0]!=NULL && retarc!=NULL) strcpy(retarc, pl->archive[pl->current>-1 ? pl->current : 0]);
		else strcpy(retarc, "");

	return ret;
}

int PL_GetPrev(PLAYLIST *pl, char *retfile, char *retarc)
{
	if(pl->numused==0) return 1;
	pl->current--;
	if(pl->current<-1) pl->current=-1;

	if(retfile!=NULL) strcpy(retfile, pl->file[pl->current>-1 ? pl->current : 0]);
	if(pl->archive[pl->current>-1 ? pl->current : 0]!=NULL && retarc!=NULL) 
		strcpy(retarc, pl->archive[pl->current>-1 ? pl->current : 0]);

	return 0;
}

int PL_GetRandom(PLAYLIST *pl, char *retfile, char *retarc)
{
	if(pl->numused==0) return 1;
	srandom(time(NULL));
	pl->current=random() % pl->numused;
	strcpy(retfile,pl->file[pl->current]);
	if(pl->archive[pl->current]!=NULL)
		strcpy(retarc,pl->archive[pl->current]);
	else strcpy(retarc, "");
	return 0;
}

int PL_DelCurrent(PLAYLIST *pl)
{
	int i;
	if(pl->numused==0) return 1;
	free(pl->file[pl->current>-1 ? pl->current : 0]);
	free(pl->archive[pl->current>-1 ? pl->current : 0]);

	for(i=pl->current>-1 ? pl->current : 0;i<pl->numused;i++)
	{
		pl->file[i]=pl->file[i+1];
		pl->archive[i]=pl->archive[i+1];
	}
	pl->numused--;
	pl->current--;
	pl->file=realloc(pl->file,pl->numused*sizeof(char *));
	pl->archive=realloc(pl->archive,pl->numused*sizeof(char *));
	return 0;
}

void PL_Add(PLAYLIST *pl, char *file, char *arc)
{
	pl->numused++;
	pl->file=realloc(pl->file,pl->numused*sizeof(char *));
	pl->archive=realloc(pl->archive,pl->numused*sizeof(char *));

	pl->file[pl->numused-1]=malloc(strlen(file)+1);
	strcpy(pl->file[pl->numused-1],file);
	if(arc!=NULL) {
		pl->archive[pl->numused-1]=malloc(strlen(arc)+1);
		strcpy(pl->archive[pl->numused-1],arc);
	} else pl->archive[pl->numused-1]=NULL;
}

/* the following two functions are untested... */
void PL_Load(PLAYLIST *pl, char *filename)
{
	FILE *f;
	char file[255];
	char *arc;

	if((f=fopen(filename,"r"))==NULL) {
		perror("Error loading playlist\n");
		return;
	}

	while(!feof(f))
	{
		fgets(file,255,f);
		if(file[strlen(file)-1]=='\n') file[strlen(file)-1]=0;
		arc=strchr(file,'|');
		if(arc==NULL)
		{
			MA_FindFiles(pl, file);
		}
		else 
		{
			*arc=0;
			arc++;
			PL_Add(pl, arc, file);
		}
	}
	fclose(f);
}

void PL_Save(PLAYLIST *pl, char *savefile)
{

	FILE *f;
	int i;

	if((f=fopen(savefile,"w"))==NULL) {
		perror("Error saving playlist\n");
		return;
	}

	for(i=0;i<pl->numused;i++)
	{
		if(pl->archive!=NULL) fprintf(f,"%s|%s\n",pl->archive[i], pl->file[i]);
		else fprintf(f,"%s\n",pl->file[i]);
	}
	fclose(f);
}
