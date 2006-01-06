/* lithp - eval
 *
 *      evaluate (run) a lithp branch
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

#ifndef __LITHP_H__
#define __LITHP_H__

#include <stdio.h>


/* the list element defines a node of the list */
typedef struct le{
	/* either data or a branch */
	struct le * branch;
	char * data;
	int quoted;
	int tag;

	/* for the previous/next in the list in the current parenlevel */
	struct le * list_prev;
	struct le * list_next;
} le;

/* the burrito holds everything needed together.  AKA "context" */
typedef struct _lithp_burrito {
	struct le *list;
	struct le *mainVarList;
	struct le *defunList;

	/* for reading */
	FILE * inFile;
	char * inString;
	int pos;
} lithp_burrito;


/*****************************************
** List element stuff
**/
le * leNew(char * text);
void leDelete(le * element);
void leWipe(le * list);

le * leAddHead(le * list, le * element);
le * leAddTail(le * list, le * element);

le * leAddBranchElement( le * list, le * branch, int quoted );
le * leAddDataElement( le * list, char * data, int quoted );
le * leDup( le * list );

void leClearTag( le * list );
void leTagData(le * list, char * data, int tagval);
void leTagReplace(le * list, int tagval, le * newinfo);

void leDump( le * list, int indent );
void leDumpReformat( FILE * of, le * tree);

void leDumpEval( lithp_burrito * lb, le * list, int indent );
void leDumpEvalTree( lithp_burrito * lb, le * list, int indent );


/*****************************************
** Variables stuff
**/

le * variableFind( le * varlist, char * key );

#define variableFree( L ) \
                leWipe( (L) );

le * variableSet( le * varlist, char * key, le * value );
le * variableSetString( le * varlist, char * key, char * value );
le * variableGet( le * varlist, char * key );
char * variableGetString( le * varlist, char * key );

void variableDump( le * varlist );


/*****************************************
** Evaluation stuff
**/

typedef le * (*eval_cb) ( lithp_burrito *lb, const int argc, le * branch );

typedef struct evalLookupNode {
	char    * word;
	eval_cb   callback;
} evalLookupNode;

le * evaluateBranch( lithp_burrito *lb, le * trybranch);
le * evaluateNode( lithp_burrito *lb, le * node);
le * evaluateDefun(  lithp_burrito *lb, le * fcn, le * params );

int countNodes( le * branch);

int evalCastLeToInt( const le * levalue );
le * evalCastIntToLe( int intvalue );

le * eval_cb_nothing( lithp_burrito *lb, const int argc, le * branch );

enum cumefcn { C_NONE, C_ADD, C_SUBTRACT, C_MULTIPLY, C_DIVIDE };

int eval_cume_helper( enum cumefcn function, lithp_burrito *lb, int value, le * branch) ;

le * eval_cb_add( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_subtract( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_multiply( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_divide( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_oneplus( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_oneminus( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_modulus( lithp_burrito *lb, const int argc, le * branch );

le * eval_cb_lt( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_lt_eq( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_gt( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_gt_eq( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_eqsign( lithp_burrito *lb, const int argc, le * branch );

le * eval_cb_and( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_or( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_not( lithp_burrito *lb, const int argc, le * branch );

le * eval_cb_atom( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_car( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_cdr( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_cons( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_list( lithp_burrito *lb, const int argc, le * branch );
int eval_cb_lists_same( le * list1, le * list2 );
le * eval_cb_equal( lithp_burrito *lb, const int argc, le * branch );

le * eval_cb_if( lithp_burrito *lb, const int argc, le * branch );

enum whenunless { WU_WHEN, WU_UNLESS };

le * eval_cb_whenunless_helper(
		enum whenunless which,
		lithp_burrito *lb,
		const int argc, 
		le * branch);

le * eval_cb_unless( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_when( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_cond( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_select( lithp_burrito *lb, const int argc, le * branch );

le * eval_cb_princ( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_terpri( lithp_burrito *lb, const int argc, le * branch );

le * eval_cb_eval( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_prog( lithp_burrito *lb, const int argc, le * branch, int returnit );
le * eval_cb_prog1( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_prog2( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_progn( lithp_burrito *lb, const int argc, le * branch );

enum setfcn { S_SET, S_SETQ };

le * eval_cb_set_helper( enum setfcn function, lithp_burrito *lb, const int argc, le * branch);

le * eval_cb_set( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_setq( lithp_burrito *lb, const int argc, le * branch );
le * eval_cb_enum( lithp_burrito *lb, const int argc, le * branch );

le * eval_cb_defun( lithp_burrito *lb, const int argc, le * branch );



/*****************************************
** context, primary stuff
*/

lithp_burrito * burritoNew( void );
void burritoDelete( lithp_burrito * lb );

int Lithp_parseInFile( lithp_burrito * lb, char * inputfilename );
int Lithp_parseInString( lithp_burrito * lb, char * theString );

#endif
