/* lithp - parser
 *
 *      parse a file or a buffer into lithp structures
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

#include <string.h>	/* for strcmp, strdup */
#include <stdlib.h>	/* for malloc / free */
#include "lithp.h"


enum tokenname {
        T_NONE,
        T_CLOSEPAREN,
        T_OPENPAREN,
        T_NEWLINE,
        T_QUOTE,
        T_WORD,
        T_EOF
};

static void myungetc( lithp_burrito * lb, int c )
{
	if( lb->inFile ) {
		ungetc( c, lb->inFile );
	} else if( lb->inString ) {
		lb->pos--;
		if( lb->pos < 0 ) lb->pos = 0;
	}
}

static int mygetc( lithp_burrito * lb )
{
	int ret = EOF;

	if( lb->inFile ) {
		ret = getc( lb->inFile );
	} else if( lb->inString ) {
		if( lb->inString[ lb->pos ] == '\0' ) {
			ret = EOF;
		} else {
			ret = lb->inString[ lb->pos ];
			lb->pos++;
		}
	}
	/* hack until i rewrite the parser engine */
	if( ret == '\t' )  ret = '\n';

	return( ret );
}



static char * snagAToken( lithp_burrito * lb, enum tokenname * tokenid )
{
	unsigned int pos = 0;
	int c;
	int doublequotes = 0;
	char temp[128];

	*tokenid = T_EOF;

	if (!lb->inFile && !lb->inString) {
		*tokenid = T_EOF;
		return (NULL);
	}

	/* chew space to next token */
	while (1) {
		c = mygetc( lb );

		/* munch comments */
		if ((c == '#') || (c == ';')) {
			do {
				c = mygetc( lb );
			} while (c != '\n');
		}
		if ((   (c == '(')
		     || (c == ')')
		     || (c == '\n')
		     || (c == '\"')
		     || (c == '\'')
		     || (c == EOF)
		     || (c > '-')
		     || (c <= 'z')
		     ) && (c != ' ') && (c != '\t')) {
			break;
		}
	}

	/* snag token */
	if (c == '(') {
		*tokenid = T_OPENPAREN;
		return (NULL);
	} else if (c == ')') {
		*tokenid = T_CLOSEPAREN;
		return (NULL);
	} else if (c == '\'') {
		*tokenid = T_QUOTE;
		return (NULL);
	} else if (c == '\n') {
		*tokenid = T_NEWLINE;
		return (NULL);
	} else if (c == EOF) {
		*tokenid = T_EOF;
		return (NULL);
	}
	/* oh well. it looks like a string.  snag to the next whitespace. */

	if (c == '\"') {
		doublequotes = 1;
		c = mygetc( lb );
	}
	while (1) {
		temp[pos++] = (char) c;

		if (!doublequotes) {
			if ((c == ')')
			    || (c == '(')
			    || (c == ';')
			    || (c == '#')
			    || (c == ' ')
			    || (c == '\n')
			    || (c == '\r')
			    || (c == EOF)
				) {
				myungetc(lb, c);
				temp[pos - 1] = '\0';

				if (!strcmp(temp, "quote")) {
					*tokenid = T_QUOTE;
					return (NULL);
				}
				*tokenid = T_WORD;
				return (strdup(temp));
			}
		} else {
			switch (c) {
			case ('\n'):
			case ('\r'):
			case (EOF):
				myungetc(lb, c);

			case ('\"'):
				temp[pos - 1] = '\0';
				*tokenid = T_WORD;
				return (strdup(temp));

			}
		}

		c = mygetc( lb );
	}
	return (NULL);
}

struct le * parseIn( lithp_burrito * lb, le * list )
{
	char *temp = NULL;
	enum tokenname  tokenid = T_NONE;
	int  isquoted = 0;

	if (!lb->inFile && !lb->inString) return (NULL);

	while (1) {
		temp = snagAToken(lb, &tokenid);

		switch (tokenid) {
		case (T_QUOTE):
			isquoted = 1;
			break;

		case (T_OPENPAREN):
			list = leAddBranchElement( 	list,
							parseIn( lb, NULL ),
							isquoted );
			isquoted = 0;
			break;

		case (T_NEWLINE):
			isquoted = 0;
			break;

		case (T_WORD):
			list = leAddDataElement( 	list,
							temp,
							isquoted );
			free(temp);
			isquoted = 0;
			break;

		case (T_CLOSEPAREN):
		case (T_EOF):
			isquoted = 0;
			return( list );

		case (T_NONE):
			break;
		}
	}
}



static void reset_parse_burrito( lithp_burrito * lb )
{
	lb->inFile = NULL;
	lb->inString = NULL;
	lb->pos = 0;
}

int Lithp_parseInFile( lithp_burrito * lb, char * inputfilename )
{
	reset_parse_burrito( lb );
	lb->inFile = fopen( inputfilename, "r" );
	if( lb->inFile ) {
		lb->list = parseIn( lb, NULL );
		fclose( lb->inFile );
	} else {
		return( -1 );
	}
	if( lb->list ) return( 0 );
	return( -2 );
}

int Lithp_parseInString( lithp_burrito * lb, char * theString )
{
	reset_parse_burrito( lb );
	lb->inString = theString;

	lb->list = parseIn( lb, NULL );
	if( lb->list ) return( 0 );
	return( -2 );
}
