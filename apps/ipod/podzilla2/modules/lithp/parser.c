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
#include <ctype.h>	/* for isspace */
#include <stdlib.h>	/* for malloc / free */
#include "lithp.h"


/*
	This parser works by reading in from a file or buffer, and it
	categorizes the caracters as they're read in.  Most of the lithp
	'tokens' are single character. ( ) " ' \n ; #.  

	One exception is that that a word can either be wrapped in
	"quotation" 'marks', or can contain the worde [quote] before it.
	there is a heuristic in the parser for this case.
	Another exception is that numbers and words being parsed can
	be returned as well.  These are T_WORD tokens.

	These tokens are then used to determine the nesting and content
	to be generated into the lists contained in the burrito.

	The burrito also contains the information as to whether we are
	working with a string buffer or a file for the input data.
*/
enum tokenname {
        T_NONE,
        T_OPENPAREN,
        T_CLOSEPAREN,
        T_QUOTE,
        T_NEWLINE,
        T_WHITESPACE,
	T_COMMENT,
        T_WORD,
        T_EOF
};



/* ungets a character from the file or buffer (as defined in the burrito) */
static void myungetc( lithp_burrito * lb, int c )
{
	if( lb->inFile ) {
		/* working with the file */
		ungetc( c, lb->inFile );

	} else if( lb->inString ) {
		/* working with the buffer */
		lb->pos--;
		if( lb->pos < 0 ) lb->pos = 0;
	}
}

/* gets a character from the file or buffer (as defined in the burrito) */
static int mygetc( lithp_burrito * lb )
{
	int ret = EOF; /* insurance policy */

	if( lb->inFile ) {
		/* getting from the file */
		ret = getc( lb->inFile );

	} else if( lb->inString ) {
		/* getting from the buffer */
		if( lb->inString[ lb->pos ] == '\0' ) {
			ret = EOF;
		} else {
			ret = lb->inString[ lb->pos ];
			lb->pos++;
		}
	}

	return( ret );
}


/* a simple routine to deterimine which category a character falls into */
static enum tokenname categorizeToken( int c )
{
	/* categorize the character, if possible */
	switch( c ) {
	case( '(' ): 	return( T_OPENPAREN );
	case( ')' ): 	return( T_CLOSEPAREN );
	case( '\"' ): 	return( T_QUOTE );
	case( '\'' ): 	return( T_QUOTE );
	case( '\n' ): 	return( T_NEWLINE );
	case( ';' ): 	return( T_COMMENT );
	case( '#' ): 	return( T_COMMENT );
	case( EOF ): 	return( T_EOF );
	case( '\0' ): 	return( T_EOF );
	}

	/* check for other space characters */
	if( isspace( c ) )  return( T_WHITESPACE );

	/* otherwise, assume it's a alphanumeric */
	return( T_WORD );
}

/* this retrieves a token from the input file/buffer.  A token can either
** be a word, or a keyword, as defined in the above enum
*/
static char * snagAToken( lithp_burrito * lb, enum tokenname * t )
{
	unsigned int pos = 0;
	int c;
	int quoted = 0;
	char temp[128]; 	/* BUG: it's bad if the string is >128 long */

	/* start at a known */
	*t = T_EOF;

	if (!lb->inFile && !lb->inString) {
		return (NULL);
	}

	/* chew space to next token */
	while (1) {
		c = mygetc( lb );
		*t = categorizeToken( c );
	
		/* munch comments */
		if( *t == T_COMMENT ) {
			do {
				c = mygetc( lb );
				*t = categorizeToken( c );
			} while( *t != T_NEWLINE );
			
		}

		if(    (*t == T_OPENPAREN) 
		    || (*t == T_CLOSEPAREN)
		    || (*t == T_QUOTE)
		    || (*t == T_NEWLINE)
		    || (*t == T_WORD)
		    || (*t == T_EOF) )
			break;
	}
	*t = categorizeToken( c );

	/* if it's not a string, the passed in 't' will already contain the
	** correct token id value, so we can just return NULL for the string
	*/
	switch( *t ) {
	case( T_OPENPAREN ):
	case( T_CLOSEPAREN ):
	case( T_QUOTE ):
	case( T_NEWLINE ):
	case( T_EOF ):
		return( NULL );
	default:
		break;
	}

	/* oh well. it looks like a string.  snag to the next whitespace. */

	if( *t == T_QUOTE ) {
		quoted = 1;
		c = mygetc( lb );
		*t = categorizeToken( c );
	}

	while (1) {
		*t = categorizeToken( c );
		temp[pos++] = (char) c;

		/* if it's quoted, keep accumulating until we hit a newline,
		    end of file, or closequote */
		if( quoted ) {
			switch( *t ) {
			case( T_NEWLINE ):
			case( T_EOF ):
				myungetc( lb, c ); /* oops. restore it */
				/* fall through... and return */
			case( T_QUOTE ):
				temp[ pos-1 ] = '\0';
				*t = T_WORD;
				return( strdup( temp ));
			default:
				break;
			}
		} else {
			/* not quoted */
			switch( *t ) {
			case( T_OPENPAREN ):
			case( T_CLOSEPAREN ):
			case( T_COMMENT ):
			case( T_WHITESPACE ):
			case( T_NEWLINE ):
			case( T_EOF ):
				myungetc(lb, c);
				/* fall through... */
			case( T_QUOTE ):
				temp[pos - 1] = '\0';

				/* check for the "quote" keyword */
				/* hardcoded heuristic */
				if( !strcmp( temp, "quote" )) {
					*t = T_QUOTE;
					return( NULL );
				}

				/* otherwise, return the word */
				*t = T_WORD;
				return (strdup(temp));
			default:
				break;
			}
		}

		c = mygetc( lb );
	}
	return (NULL);
}


/* this actually handles the main outer loop of the parsing */
struct le * parseIn( lithp_burrito * lb, le * list )
{
	char *temp = NULL;
	enum tokenname  tokenid = T_NONE;
	int  isquoted = 0;

	if (!lb->inFile && !lb->inString) return (NULL);

	while (1) {
		temp = snagAToken(lb, &tokenid);

		switch (tokenid) {
		case( T_QUOTE ):
			isquoted = 1;
			break;

		case( T_OPENPAREN ):
			list = leAddBranchElement( 	list,
							parseIn( lb, NULL ),
							isquoted );
			isquoted = 0;
			break;

		case( T_NEWLINE ):
			isquoted = 0;
			break;

		case( T_WORD ):
			list = leAddDataElement( 	list,
							temp,
							isquoted );
			free(temp);
			isquoted = 0;
			break;

		case( T_CLOSEPAREN ):
		case( T_EOF ):
			isquoted = 0;
			return( list );

		/* the following should never occur, but added for -Wall */
		default:
			break;
		}
	}
}


/* reset some of the burrito for when we start out */
static void reset_parse_burrito( lithp_burrito * lb )
{
	lb->inFile = NULL;
	lb->inString = NULL;
	lb->pos = 0;
}

/* this parses the file as defined by [[inputfilename]] */
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

/* this parses the character string as defined by [[theString]] */
int Lithp_parseInString( lithp_burrito * lb, char * theString )
{
	reset_parse_burrito( lb );
	lb->inString = theString;

	lb->list = parseIn( lb, NULL );
	if( lb->list ) return( 0 );
	return( -2 );
}
