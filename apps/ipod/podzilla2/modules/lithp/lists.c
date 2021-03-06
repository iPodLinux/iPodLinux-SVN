/* lithp - lists
 *
 *      keeps track of the lists
 *      
 *      (c)2001/2006 Scott Lawrence   yorgle@gmail.com
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <stdlib.h>	/* for malloc/free */
#include "lithp.h"

    
le * leNew(char * text)
{
	le * new = (le *) malloc (sizeof(le));
	if (new)
	{
		new->branch = NULL;
		new->data = (text)?strdup(text):NULL;
		new->quoted = 0;
		new->tag = -1;
		new->list_prev = NULL;
		new->list_next = NULL;
	}
	return( new );
}
    
void leDelete(le * element)
{
	if (element) {
		if (element->data) free( element->data );
		free(element);
	}
}

void leWipe(le * list)
{
	if (list) {
		/* free descendants */
		leWipe(list->branch);
		leWipe(list->list_next);

		/* free ourself */
		if (list->data) free( list->data );
		free( list );
	}
}

le * leAddHead(le * list, le * element)
{
	if (!element)  return( list );

	element->list_next = list;
	if (list) list->list_prev = element;
	return( element );
}

le * leAddTail(le * list, le * element)
{
	le * temp = list;

	/* if neither element or list don't 
	   exist return the 'new' list */
	if (!element) return( list );
	if (!list)  return( element );

	/* find the end element of the list */
	while (temp->list_next) temp = temp->list_next;

	/* tack ourselves on */
	temp->list_next = element;
	element->list_prev = temp;

	/* return the list */
	return( list );
}

    
le * leAddBranchElement( le * list, le * branch, int quoted )
{
	le * temp = leNew(NULL);
	temp->branch = branch;
	temp->quoted = quoted;
	return leAddTail(list, temp);
}

le * leAddDataElement( le * list, char * data, int quoted )
{
	le * newdata = leNew(data);
	if (newdata) {
		newdata->quoted = quoted;
		return leAddTail(list, newdata);
	}
	return( NULL );
}

le * leDup( le * list )
{
	le * temp;
	if (!list) return( NULL );

	
	temp = leNew(list->data);
	temp->branch = leDup(list->branch);
	temp->list_next = leDup(list->list_next);

	if (temp->list_next) temp->list_next->list_prev = temp;

	return( temp );
}


void leClearTag( le * list )
{
	if (!list) return;
	list->tag = -1;
	leClearTag( list->branch );
	leClearTag( list->list_next );
}
    
void leTagData(le * list, char * data, int tagval)
{
	if (!data || !list) return;

	while (list) {
		if( list->data ) {
			if (!strcmp( list->data, data )) {
			    list->tag = tagval;
			}
		}
		leTagData( list->branch, data, tagval );

		list = list->list_next;
	}
}

void leTagReplace(le * list, int tagval, le * newinfo)
{
	if (!list || !newinfo) return;

	while (list)
	{
		if( list->tag == tagval ) {
			/* free any existing stuff */
			if ( list->data ) {
				free( list->data );
				list->data = NULL;
			}

			/* NOTE: This next comparison might be flawed */
			if ( newinfo->list_next || newinfo->branch ) {
				list->branch = leDup( newinfo );
				list->quoted = 1;
			} else if ( newinfo->data ) {
				list->data = strdup( newinfo->data );
			}

		}
		leTagReplace( list->branch, tagval, newinfo );

		list = list->list_next;
	}
}


void leDump( le * list, int indent )
{
	int c;
	le * temp = list;

	while (temp) {
		if (temp->data) {
			for( c=0 ; c<indent ; c++ ) printf( " " );
			printf( "%s %s\n", 
				temp->data,
				(temp->quoted == 1) ? "quoted" : ""
				);
		} else {
			leDump(temp->branch, indent + 4);
		}

		temp=temp->list_next;
	}
}

void leDumpEvalTree( lithp_burrito *lb, le * list, int indent )
{
	int c;
	le * temp = list;

	while (temp) {
		for( c=0 ; c<indent ; c++ ) printf( " " );
		if (temp->data) {
			printf( "%s %s\n", 
				temp->data,
				(temp->quoted == 1) ? "quoted" : ""
				);
		} else {
			le * le_value = evaluateBranch( lb, temp->branch );
			printf( "B: %s", (temp->quoted) ? "quoted " : "" );
			leDumpReformat( stdout, le_value );
			printf( "\n" );
			leWipe(le_value);

			leDump(temp->branch, indent + 4);
		}

		temp=temp->list_next;
	}
}


void Lithp_evaluateBurrito( lithp_burrito * lb )
{
	le * temp = lb->list;
	le * le_value = NULL;

	while( temp ) {
		if( temp->branch ) {
			le_value = evaluateBranch( lb, temp->branch );
			leWipe(le_value);
		}
		temp=temp->list_next;
	}
}

void leDumpEval( lithp_burrito * lb, le * list, int indent )
{
	le * temp = list;
	le * le_value = NULL;

	while (temp) {
		if (temp->branch) {
			printf ("\n");
			leDumpReformat( stdout, temp->branch );

			printf ("\n==> ");
			le_value = evaluateBranch( lb, temp->branch );
			leDumpReformat( stdout, le_value );
			leWipe(le_value);
			printf ("\n");
		}

		temp=temp->list_next;
	}
	printf("=======\n");
}

void leDumpReformat(FILE * of, le * tree)
{
	le * treetemp = tree;
	int notfirst = 0;

	if (!tree) return;

	fprintf( of, "(" );
	while (treetemp) {
		if (treetemp->data) {
			fprintf( of, "%s%s", notfirst?" ":"", treetemp->data);
			notfirst++;
		}

		if (treetemp->branch) {
			fprintf( of, " %s", (treetemp->quoted)? "\'":"");
			leDumpReformat( of, treetemp->branch );
		}

		treetemp = treetemp->list_next;
	}
	fprintf( of, ")" );
}
