/* lithp - eval
 *
 * 	evaluate (run) a lithp branch
 *
 *	(c)2001/2006 Scott Lawrence   yorgle@gmail.com
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lithp.h"

evalLookupNode evalTable[] =
{
	{ "+" 		, eval_cb_add		},
	{ "-" 		, eval_cb_subtract	},
	{ "*" 		, eval_cb_multiply	},
	{ "/" 		, eval_cb_divide	},

	{ "1+" 		, eval_cb_oneplus	},
	{ "1-" 		, eval_cb_oneminus	},

	{ "%" 		, eval_cb_modulus	},

	{ "<" 		, eval_cb_lt		},
	{ "<=" 		, eval_cb_lt_eq		},
	{ ">" 		, eval_cb_gt		},
	{ ">=" 		, eval_cb_gt_eq		},
	{ "=" 		, eval_cb_eqsign	},

	{ "and" 	, eval_cb_and		},
	{ "or" 		, eval_cb_or		},
	{ "not" 	, eval_cb_not		},
	{ "null" 	, eval_cb_not		},

	{ "atom" 	, eval_cb_atom		},
	{ "car" 	, eval_cb_car		},
	{ "cdr" 	, eval_cb_cdr		},
	{ "cons" 	, eval_cb_cons		},
	{ "list" 	, eval_cb_list		},
	{ "equal"	, eval_cb_equal		},

	{ "if"		, eval_cb_if		},
	{ "unless"	, eval_cb_unless	},
	{ "when"	, eval_cb_when		},
	{ "cond"	, eval_cb_cond		},
	{ "select"	, eval_cb_select	},

	{ "princ"	, eval_cb_princ		},
	{ "terpri"	, eval_cb_terpri	},

	{ "eval"	, eval_cb_eval		},
	{ "prog1"	, eval_cb_prog1		},
	{ "prog2"	, eval_cb_prog2		},
	{ "progn"	, eval_cb_progn		},

	{ "set"		, eval_cb_set		},
	{ "setq"	, eval_cb_setq		},
	{ "setf"	, eval_cb_setq		},
	{ "enum"	, eval_cb_enum		},

	{ "defun"	, eval_cb_defun		},

	{ "gc"		, eval_cb_nothing	},
	{ "garbage-collect" , eval_cb_nothing	},

	/* now, the additional functions for graphics renderings */

	{ "Rand"		, eval_gfx_Rand			},
	{ "RandomOf"		, eval_gfx_RandomOf		},
	{ "DrawPen"		, eval_gfx_DrawPen		},
	{ "DrawPen2"		, eval_gfx_DrawPen2		},
	{ "DrawPixel"		, eval_gfx_DrawPixel		},
	{ "DrawLine"		, eval_gfx_DrawLine		},
	{ "DrawAALine"		, eval_gfx_DrawAALine		},
	{ "DrawRect"		, eval_gfx_DrawRect		},
	{ "DrawFillRect"	, eval_gfx_DrawFillRect		},
	{ "DrawClear"		, eval_gfx_DrawClear		},
	{ "DrawVGradient"	, eval_gfx_DrawVGradient	},
	{ "DrawhGradient"	, eval_gfx_DrawHGradient	},
	{ "DrawEllipse"		, eval_gfx_DrawEllipse		},
	{ "DrawAAEllipse"	, eval_gfx_DrawAAEllipse	},
	{ "DrawFillEllipse"	, eval_gfx_DrawFillEllipse	},
	{ "DrawVectorText"	, eval_gfx_DrawVectorText	},
	{ "DrawVectorTextCentered"	, eval_gfx_DrawVectorTextCentered	},
/*
	{ "DrawText"		, eval_gfx_DrawText		},
	{ "DrawPoly"		, eval_gfx_DrawPoly		},
	{ "DrawFillPoly"	, eval_gfx_DrawFillPoly		},
*/

	{ NULL			, NULL				}
};

le * evaluateBranch( lithp_burrito *lb, le * trybranch)
{
	le * keyword;
	int tryit = 0;
	if (!trybranch) return( NULL );

	if (trybranch->branch) {
		keyword = evaluateBranch(lb, trybranch->branch);
	} else 
	    	keyword = leNew( trybranch->data );

	if (!keyword->data) {
		leWipe( keyword );
		return( leNew( "NIL" ));
	}

	for ( tryit=0 ; evalTable[tryit].word ; tryit++) {
		if (!strcmp(evalTable[tryit].word, keyword->data)) {
			leWipe( keyword );
			return( evalTable[tryit].callback(
					lb,
					countNodes( trybranch ), 
					trybranch) );
		}
	}

	leWipe( keyword );
	return( evaluateNode( lb, trybranch ));
}
    

le * evaluateNode( lithp_burrito * lb, le * node)
{
	le * temp;
	le * value;

	if (node->branch) {
		if( node->quoted ) {
			value = leDup( node->branch );
		} else {
			value = evaluateBranch( lb, node->branch );
		}
	} else {
		temp = variableGet( lb->defunList, node->data );

		if (temp) {
			value = evaluateDefun( lb, temp, node->list_next );
		} else {
			temp = variableGet( lb->mainVarList, node->data );
			if (temp) {
				value = leDup( temp );
			} else {
				value = leNew( node->data );
			}
		}
	}

	return( value );
}


le * evaluateDefun( lithp_burrito *lb, le * fcn, le * params )
{
	le * function;
	le * thisparam;
	le * result;
	int count;

	/* make sure both lists exist */
	if (!fcn)  return( leNew( "NIL" ));

	/* check for the correct number of parameters */
	if (countNodes(fcn->branch) > countNodes(params))
	    return( leNew( "NIL" ));

	/* allocate another function definition, since we're gonna hack it */
	function = leDup(fcn);

	/* pass 1:  tag each node properly. 
		    for each parameter: (fcn)
		    - look for it in the tree, tag those with the value
	*/
	count = 0;
	thisparam = fcn->branch;
	while (thisparam)
	{
		leTagData(function, thisparam->data, count);
		thisparam = thisparam->list_next;
		count++;
	}

	/* pass 2:  replace
		    for each parameter: (param)
		    - evaluate the passed in value
		    - replace it in the tree
	*/
	count = 0;
	thisparam = params;
	while (thisparam)
	{
		result = evaluateNode( lb, thisparam );
		leTagReplace(function, count, result);
		thisparam = thisparam->list_next;
		leWipe(result);
		count++;
	}

	/* then evaluate the resulting tree */
	result = evaluateBranch( lb, function->list_next );
	
	/* free any space allocated */
	leWipe( function );

	/* return the evaluation */
	return( result );
}


void Lithp_callDefun( lithp_burrito *lb, char * fname )
{
	le * fcn;
	le * temp = variableGet( lb->defunList, fname );

	if( !temp ) return;
	fcn = leNew( fname );

	(void)evaluateNode( lb, fcn );

	leWipe( fcn );
}
 

int countNodes(le * branch)
{
	int count = 0;

	while (branch)
	{
		count++;
		branch = branch->list_next;
	}
	return( count );
}


int evalCastLeToInt( const le * levalue )
{
	if (!levalue) return( 0 );
	if (!levalue->data) return( 0 );
	
	return( atoi(levalue->data) );
}


le * evalCastIntToLe( int intvalue )
{
	char buffer[80];
	sprintf (buffer, "%d", intvalue);

	return( leNew(buffer) );
}


/* the functions that do the work... */

le * eval_cb_nothing( lithp_burrito * lb, const int argc, le * branch )
{
	return( leNew( "T" ));
}
    
int eval_cume_helper( enum cumefcn function, lithp_burrito * lb, int value, le * branch) 
{
	int newvalue = 0;
	le * temp = branch;
	le * value_le;

	if (!branch) return( 0 );

	while (temp) {
		value_le = evaluateNode(lb, temp); 
		newvalue = evalCastLeToInt(value_le);
		leWipe(value_le);

		switch(function) {
		case( C_ADD ):
			value += newvalue;
			break;

		case( C_SUBTRACT ):
			value -= newvalue;
			break;

		case( C_MULTIPLY ):
			value *= newvalue;
			break;

		case( C_DIVIDE ):
			value /= newvalue;
			break;

		case( C_NONE ):
			break;
		}

		temp = temp->list_next;
	}

	return( value );
}

    
le * eval_cb_add( lithp_burrito * lb, const int argc, le * branch )
{
	if (!branch || argc < 2) return( leNew( "NIL" ) );

	return( evalCastIntToLe( 
			eval_cume_helper( C_ADD, lb, 0, branch->list_next) ));
}

le * eval_cb_subtract( lithp_burrito * lb, const int argc, le * branch )
{
	int firstitem = 0;
	le * lefirst;

	if (!branch || argc < 2) return( leNew( "NIL" ) );

	lefirst = evaluateNode( lb, branch->list_next );
	firstitem = evalCastLeToInt( lefirst );
	leWipe( lefirst );

	if (argc == 2) {
		return( evalCastIntToLe( -1 * firstitem) );
	}
	
	return( evalCastIntToLe( eval_cume_helper( C_SUBTRACT, lb, firstitem, 
				    branch->list_next->list_next)));
}

    
le * eval_cb_multiply( lithp_burrito * lb, const int argc, le * branch )
{
	if (!branch || argc < 2) return( leNew( "NIL" ) );

	return( evalCastIntToLe(
		    eval_cume_helper( C_MULTIPLY, lb, 1, branch->list_next)));
}

    
le * eval_cb_divide( lithp_burrito * lb, const int argc, le * branch )
{
	int firstitem = 0;
	le * lefirst;
	if (!branch || argc < 2) return( leNew( "NIL" ) );

	lefirst = evaluateNode( lb, branch->list_next );
	firstitem = evalCastLeToInt( lefirst );
	leWipe( lefirst );

	return( evalCastIntToLe(
			eval_cume_helper( C_DIVIDE, lb, firstitem, 
				    branch->list_next->list_next)));
}


le * eval_cb_oneplus( lithp_burrito * lb, const int argc, le * branch )
{
	le * retle;
	int value;

	if (!branch || argc < 2) return( leNew( "NIL" ) );

	retle = evaluateNode( lb, branch->list_next ); 
	value = evalCastLeToInt( retle );
	leWipe( retle );

	return( evalCastIntToLe(value + 1) );
}


le * eval_cb_oneminus( lithp_burrito * lb, const int argc, le * branch )
{
	le * retle;
	int value;

	if (!branch || argc < 2) return( leNew( "NIL" ) );

	retle = evaluateNode( lb, branch->list_next ); 
	value = evalCastLeToInt( retle );
	leWipe( retle );

	return( evalCastIntToLe(value - 1) );

}


le * eval_cb_modulus( lithp_burrito * lb, const int argc, le * branch )
{
	le * letemp;
	int value1, value2;

	if (!branch || argc != 3) return( leNew( "NIL" ) );

	letemp = evaluateNode( lb, branch->list_next );
	value1 = evalCastLeToInt( letemp );
	leWipe( letemp );

	letemp = evaluateNode( lb, branch->list_next->list_next );
	value2 = evalCastLeToInt( letemp );
	leWipe( letemp );

	return( evalCastIntToLe ( value1 % value2 ) );
}


    
le * eval_cb_lt( lithp_burrito * lb, const int argc, le * branch )
{
	le * letemp;
        int value1, value2;

	if (!branch || argc != 3 ) return( leNew( "NIL" ) );

        letemp = evaluateNode( lb, branch->list_next );
        value1 = evalCastLeToInt( letemp );
        leWipe( letemp );

        letemp = evaluateNode( lb, branch->list_next->list_next );
        value2 = evalCastLeToInt( letemp );
        leWipe( letemp );

        return( leNew ( (value1 < value2 )?"T":"NIL" ) );
}
    
le * eval_cb_lt_eq( lithp_burrito * lb, const int argc, le * branch )
{
	le * letemp;
        int value1, value2;

	if (!branch || argc != 3 ) return( leNew( "NIL" ) );

        letemp = evaluateNode( lb, branch->list_next );
        value1 = evalCastLeToInt( letemp );
        leWipe( letemp );

        letemp = evaluateNode( lb, branch->list_next->list_next );
        value2 = evalCastLeToInt( letemp );
        leWipe( letemp );

        return( leNew ( (value1 <= value2 )?"T":"NIL" ) );
}
    
le * eval_cb_gt( lithp_burrito * lb, const int argc, le * branch )
{
	le * letemp;
        int value1, value2;

	if (!branch || argc != 3 ) return( leNew( "NIL" ) );

        letemp = evaluateNode( lb, branch->list_next );
        value1 = evalCastLeToInt( letemp );
        leWipe( letemp );

        letemp = evaluateNode( lb, branch->list_next->list_next );
        value2 = evalCastLeToInt( letemp );
        leWipe( letemp );

        return( leNew ( (value1 > value2 )?"T":"NIL" ) );
}

le * eval_cb_gt_eq( lithp_burrito * lb, const int argc, le * branch )
{
	le * letemp;
        int value1, value2;

	if (!branch || argc != 3 ) return( leNew( "NIL" ) );

        letemp = evaluateNode( lb, branch->list_next );
        value1 = evalCastLeToInt( letemp );
        leWipe( letemp );

        letemp = evaluateNode( lb, branch->list_next->list_next );
        value2 = evalCastLeToInt( letemp );
        leWipe( letemp );

        return( leNew ( (value1 >= value2 )?"T":"NIL" ) );
}

le * eval_cb_eqsign( lithp_burrito * lb, const int argc, le * branch )
{
	le * letemp;
        int value1, value2;

	if (!branch || argc != 3 ) return( leNew( "NIL" ) );

        letemp = evaluateNode( lb, branch->list_next );
        value1 = evalCastLeToInt( letemp );
        leWipe( letemp );

        letemp = evaluateNode( lb, branch->list_next->list_next );
        value2 = evalCastLeToInt( letemp );
        leWipe( letemp );

        return( leNew ( (value1 == value2 )?"T":"NIL" ) );
}

le * eval_cb_and( lithp_burrito * lb, const int argc, le * branch )
    {
	le * temp;
	le * result = NULL;
	if (!branch || argc < 2 ) return( leNew( "NIL" ));

	temp = branch->list_next;
	while( temp )
	{
		if( result )  leWipe( result );

		result = evaluateNode(lb, temp);
		if (result->data) {
			if (!strcmp ( result->data, "NIL" )) {
				return( result );
			}
		}
		temp = temp->list_next;
	}
	return( result );
}

le * eval_cb_or( lithp_burrito * lb, const int argc, le * branch )
{
	le * temp;
	le * result = NULL;
	if (!branch || argc < 2 ) return( leNew( "NIL" ));

	temp = branch->list_next;
	while( temp ) {
		if( result )  leWipe( result );

		result = evaluateNode(lb, temp);
		if (result->data) {
			if (strcmp ( result->data, "NIL" )) {
			    return( result );
			}
		}
		temp = temp->list_next;
	}
	return( result );
}

le * eval_cb_not( lithp_burrito * lb, const int argc, le * branch )
{
	le * result = NULL;
	if (!branch || argc != 2 ) return( leNew( "NIL" ));

	result = evaluateNode( lb, branch->list_next );

	if (result->data) {
		if (!strcmp (result->data, "NIL" )) {
			leWipe( result );
			return( leNew( "T" ) );
		} else {
			leWipe( result );
			return( leNew( "NIL" ) );
		}
	} else if (result->branch) {
		leWipe( result );
		return( leNew( "NIL" ) );
	}
	
	leWipe( result );
	return( leNew( "T" ));
}

le * eval_cb_atom( lithp_burrito * lb, const int argc, le * branch )
{
	le * result = NULL;
	if (!branch || argc != 2 ) return( leNew( "NIL" ));

	result = evaluateNode( lb, branch->list_next );

	if (countNodes(result) == 1) {
		leWipe( result );
		return( leNew( "T" ) );
	}
	return( leNew( "NIL" ) );
}
    
le * eval_cb_car( lithp_burrito * lb, const int argc, le * branch )
{
	le * result = NULL;
	le * temp = NULL;
	if (!branch || argc != 2 ) return( leNew( "NIL" ));

	result = evaluateNode(lb, branch->list_next);

	if( result == NULL )  return( leNew( "NIL" ) );

	if (countNodes(result) <= 1) {
		if (result->branch) {
			temp = result;
			result = result->branch;
			temp->branch = NULL;
			leWipe( temp );
		}
		return( result );
	}

	result->list_next->list_prev = NULL;
	leWipe( result->list_next );
	result->list_next = NULL;

	if (result->branch) {
		temp = result;
		result = result->branch;
		temp->branch = NULL;
		leWipe( temp );
	}

	return( result );
}

le * eval_cb_cdr( lithp_burrito * lb, const int argc, le * branch )
{
	le * result = NULL;
	le * temp = NULL;
	if (!branch || argc != 2 ) return( leNew( "NIL" ));

	result = evaluateNode(lb, branch->list_next);

	if( result == NULL )  return( leNew( "NIL" ) );

	if (result == NULL  || countNodes(result) == 1) {
		return( leNew( "NIL" ) );
	}

	temp = result;
	temp->list_next->list_prev = NULL;
	result = result->list_next;

	temp->list_next = NULL;
	leWipe( temp );

	return( result );
}

le * eval_cb_cons( lithp_burrito * lb, const int argc, le * branch )
{
	le * result1 = NULL;
	le * result2 = NULL;

	if (!branch || argc != 3 ) return( leNew( "NIL" ));

	result1 = evaluateNode(lb, branch->list_next);
	if ( result1 == NULL ) return( leNew( "NIL" ));

	result2 = evaluateNode(lb, branch->list_next->list_next);
	if ( result2 == NULL ) {
		leWipe( result1 );
		return( leNew( "NIL" ));
	}

	if ( countNodes(result1) > 1 ) {
		le * temp = leNew( NULL );
		temp->branch = result1;
		result1 = temp;
	} 
	result1->list_next = result2;
	result2->list_prev = result1;

	return( result1 );
}

le * eval_cb_list( lithp_burrito * lb, const int argc, le * branch )
{
	le * currelement = NULL;
	le * finaltree = NULL;
	le * lastadded = NULL;
	le * result = NULL;

	if (!branch) return( leNew( "NIL" ));

	currelement = branch->list_next;
	while (currelement)
	{
		result = evaluateNode(lb, currelement);
		if ( result == NULL ) {
			leWipe( finaltree );
			return( leNew( "NIL" ));
		}

		if( countNodes(result) > 1) {
			le * temp = leNew( NULL );
			temp->branch = result;
			result = temp;
		}
	
		if (!finaltree) {
			finaltree = result;
			lastadded = result;
		} else {
			lastadded->list_next = result;
			result->list_prev    = lastadded;
			lastadded = result;
		}
		
		currelement = currelement->list_next;
	}

	if (!finaltree) {
		return( leNew( "NIL" ));
	}
	return( finaltree );
}
    
int eval_cb_lists_same( le * list1, le * list2 )
{
	if (!list1 && !list2)    return( 1 );

	while( list1 ) {
		/* if list2 ended prematurely, fail */
		if (list2 == NULL) return( 0 );

		/* if one has data and the other doesn't, fail */
		if (   (list1->data && ! list2->data)
		    || (list2->data && ! list1->data))
		    	return( 0 );

		/* if the data is different, fail */
		if (list1->data && list2->data) {
		    if (strcmp( list1->data, list2->data )) {
			    return( 0 );
		    }
		}

		/* if one is quoted and the other isn't, fail */
		if (list1->quoted != list2->quoted) return( 0 );

		/* if their branches aren't the same, fail */
		if (!eval_cb_lists_same( list1->branch, list2->branch ))
			return( 0 );

		/* try the next in the list */
		list1 = list1->list_next;
		list2 = list2->list_next;
	}

	/* if list2 goes on, fail */
	if (list2) return( 0 );

	return( 1 );
}

le * eval_cb_equal( lithp_burrito * lb, const int argc, le * branch )
{
        le * list1 = NULL;
	le * list2 = NULL;
	int retval = 0;

	if (!branch || argc != 3 ) return( leNew( "NIL" ) );

        list1 = evaluateNode( lb, branch->list_next );
        list2 = evaluateNode( lb, branch->list_next->list_next );

	retval = eval_cb_lists_same( list1, list2 );

        leWipe( list1 );
        leWipe( list2 );

        return( leNew ( (retval == 1) ? "T" : "NIL" ) );
}

    
le * eval_cb_if( lithp_burrito * lb, const int argc, le * branch )
{
	le * retcond = NULL;

	if (!branch || argc < 3 || argc > 4) return( leNew( "NIL" ));

	/* if */
	retcond = evaluateNode(lb, branch->list_next);

	if (!strcmp ( retcond->data, "NIL" ))
	{
	    if (argc == 3) /* no else */
		    return( retcond );

	    leWipe( retcond );
	    return( evaluateNode( lb, branch->list_next->list_next->list_next ) );
	}

	/* then */
	leWipe( retcond );
	return( evaluateNode(lb, branch->list_next->list_next) );
}

le * eval_cb_whenunless_helper(
	    enum whenunless which,
	    lithp_burrito * lb, const int argc, 
	    le * branch
) {
	le * retval = NULL;
	le * trythis = NULL;

	if (!branch || argc < 3 ) return( leNew( "NIL" ));

	/* conditional */
	retval = evaluateNode(lb, branch->list_next);

	if ( which == WU_UNLESS )
	{
		/* unless - it wasn't true... bail */
		if ( strcmp( retval->data, "NIL" )) {
			leWipe( retval );
			return( leNew( "NIL" ) );
		}
	} else {
		/* when:   it wasn't false... bail */
		if ( !strcmp( retval->data, "NIL" ))
			    return( retval );
	}

	trythis = branch->list_next->list_next;
	while( trythis ) {
		if (retval)  leWipe( retval );

		retval = evaluateNode(lb, trythis);
		trythis = trythis->list_next;
	}
	return( retval );
}
    
le * eval_cb_unless( lithp_burrito * lb, const int argc, le * branch )
{
	return( eval_cb_whenunless_helper( WU_UNLESS, lb, argc, branch ) );
}

le * eval_cb_when( lithp_burrito * lb, const int argc, le * branch )
{
	return( eval_cb_whenunless_helper( WU_WHEN, lb, argc, branch ) );
}

le * eval_cb_cond( lithp_burrito * lb, const int argc, le * branch )
{
	le * retval = NULL;
	le * retblock = NULL;
	le * trythis = NULL;
	le * tryblock = NULL;
	int newargc;

	if (!branch || argc < 2 ) return( leNew( "NIL" ));

	trythis = branch->list_next;
	while (trythis) {
		newargc = countNodes( trythis->branch );
		if (newargc == 0)  continue;

		/* conditional */
		if (retval)  leWipe(retval);
		retval = evaluateNode(lb, trythis->branch);

		if ( strcmp(retval->data, "NIL" )) {
			if (newargc == 1) return( retval );

			tryblock = trythis->branch->list_next;
			while (tryblock) {
				if (retblock)  leWipe(retblock);
				retblock = NULL;

				retblock = evaluateNode(lb, tryblock);
				tryblock = tryblock->list_next;
			}
			return( retblock );
		}
		    
		trythis = trythis->list_next;
	}
	return( retval );
}

le * eval_cb_select( lithp_burrito * lb, const int argc, le * branch )
{
	le * result;

	if (argc < 2)  return( leNew( "NIL" ));

	branch = branch->list_next;
	result = evaluateNode(lb, branch);

	branch = branch->list_next;
	while( branch ) {
		if( branch->branch ) {
			le * check = branch->branch;
			if (check && check->data
			    && (!strcmp( check->data, result->data ))) {
				/* we're in the right place, evaluate and return */
				le * computelist = check->list_next;
				while( computelist )
				{
				    leWipe( result );
				    result = evaluateNode( lb, computelist );
				    computelist = computelist->list_next;
				}
				return( result );
			}
		}
		branch = branch->list_next;
	}

	return( result );
}

le * eval_cb_princ( lithp_burrito * lb, const int argc, le * branch )
{
	le * thisnode;
	le * retblock = NULL;
	if (!branch || argc < 1 ) return( leNew( "NIL" ));

	thisnode = branch->list_next;
	while (thisnode) {
		if (retblock)  leWipe( retblock );
		retblock = evaluateNode(lb, thisnode);
		leDumpReformat(stdout, retblock);

		thisnode = thisnode->list_next;
	}
	return( retblock );
}

le * eval_cb_terpri( lithp_burrito * lb, const int argc, le * branch )
{
	if (!branch || argc != 1 ) return( leNew( "NIL" ));

	printf( "\n" );
	fflush( stdout );
	return( leNew( "NIL" ) );
}

le * eval_cb_eval( lithp_burrito * lb, const int argc, le * branch )
{
	le * temp;
	le * retval;
	if (!branch || argc != 2 ) return( leNew( "NIL" ));

	temp = evaluateNode(lb, branch->list_next);
	retval = evaluateBranch(lb, temp);
	leWipe( temp );
	return( retval );
}

le * eval_cb_prog( lithp_burrito * lb, const int argc, le * branch, int returnit )
{
	le * curr;
	le * retval = NULL;
	le * tempval = NULL;
	int current = 0;

	if (!branch || argc < (returnit +1) ) return( leNew( "NIL" ));

	curr = branch->list_next;
	while (curr)
	{
		++current;

		if ( tempval ) leWipe (tempval);
		tempval = evaluateNode( lb, curr );

		if (current == returnit) retval = leDup( tempval );
		
		curr = curr->list_next;
	}

	if (!retval)  retval = tempval;

	return( retval );
}
    
le * eval_cb_prog1( lithp_burrito * lb, const int argc, le * branch )
{
	return( eval_cb_prog( lb, argc, branch, 1 ));
}
    
le * eval_cb_prog2( lithp_burrito * lb, const int argc, le * branch )
{
	return( eval_cb_prog( lb, argc, branch, 2 ));
}
    
le * eval_cb_progn( lithp_burrito * lb, const int argc, le * branch )
{
	return( eval_cb_prog( lb, argc, branch, -1 ));
}

le * eval_cb_set_helper( 
		enum setfcn function,
		lithp_burrito * lb, const int argc, 
		le * branch
		)
{
	le * newkey;
	le * newvalue;
	le * current;

	if (!branch || argc < 3)  return( leNew( "NIL" ) );

	current = branch->list_next;
	while ( current ) {
		if (!current->list_next) {
			newvalue = leNew( "NIL" );
		} else {
			newvalue = evaluateNode(lb, current->list_next);
		}

		if ( function == S_SET ) newkey = evaluateNode(lb, current);

		lb->mainVarList = variableSet( 
				lb->mainVarList,
		    ( function == S_SET )? newkey->data : current->data,
				newvalue
				);

		if ( function == S_SET ) leWipe(newkey);

		if (!current->list_next) {
		    current = NULL;
		} else {
		    current = current->list_next->list_next;
		}
	}
	return( leDup(newvalue) );
}


le * eval_cb_set( lithp_burrito * lb, const int argc, le * branch )
{
	return( eval_cb_set_helper( S_SET, lb, argc, branch ) );
}

    
le * eval_cb_setq( lithp_burrito * lb, const int argc, le * branch )
{
	return( eval_cb_set_helper( S_SETQ, lb, argc, branch ) );
}
    
le * eval_cb_enum( lithp_burrito * lb, const int argc, le * branch )
{
	le * current;
	int count = -1;
	char value[16];

	if (!branch || argc < 2)  return( leNew( "NIL" ) );

	current = branch->list_next;
	while ( current ) {
		if (current->data) {
			sprintf( value, "%d", ++count);

			lb->mainVarList = variableSetString( 
				    lb->mainVarList,
				    current->data,
				    value
				    );
		}
		current = current->list_next;
	}

	if (count == -1) return( leNew( "NIL" ) );
	else 		 return( evalCastIntToLe(count) );
}

le * eval_cb_defun( lithp_burrito * lb, const int argc, le * branch )
{
	if (!branch || argc < 4 ) return( leNew( "NIL" ));

	if ( !branch->list_next->data )  return( leNew( "NIL" ));

	lb->defunList = variableSet( 
			lb->defunList,
			branch->list_next->data,
			branch->list_next->list_next
			);

	return( leNew( branch->list_next->data ));
}

/*****************************************************************************/
/*  graphics / podzilla addons */

le * eval_gfx_Rand ( lithp_burrito * lb, const int argc, le * branch )
{
        le * retle;
        int value;
	int r;

        if (!branch || argc < 2) return( leNew( "NIL" ) );

        retle = evaluateNode( lb, branch->list_next );
        value = evalCastLeToInt( retle );
        leWipe( retle );

        r = (int)((float)value * rand() / (RAND_MAX + 1.0));

        return( evalCastIntToLe( r ) );
}

/* this is to be called like:  (RandomOf A B C D)
** although, it probably should be (RandomOf (list A B C D))
** this should work fine for now though
*/
le * eval_gfx_RandomOf( lithp_burrito * lb, const int argc, le * branch )
{
        le * ptr;
        int value;
	int r;

        if (!branch) return( leNew( "NIL" ) );
        value = (int)((float)(argc-1) * rand() / (RAND_MAX + 1.0));

	ptr = branch->list_next;
	for( r=0 ; r<value ; r++ ) ptr = ptr->list_next;

        return( leNew( ptr->data ));
}


/*
static le * eval_getint_1( lithp_burrito *lb, le * branch, int *a )
{
	le * retle = evaluateNode( lb, branch->list_next );
        *a = evalCastLeToInt( retle );
	return( branch->list_next->list_next );
}
*/

static le * eval_getint_2( lithp_burrito *lb, le * branch, int *a, int *b )
{
	le * retle = evaluateNode( lb, branch->list_next );
        *a = evalCastLeToInt( retle );
	retle = evaluateNode( lb, branch->list_next->list_next );
        *b = evalCastLeToInt( retle );
	return( branch->list_next->list_next->list_next );
}

static le * eval_getint_3( lithp_burrito *lb, le * branch, int *a, int *b, int *c )
{
	le * retle = evaluateNode( lb, branch->list_next );
        *a = evalCastLeToInt( retle );
	retle = evaluateNode( lb, branch->list_next->list_next );
        *b = evalCastLeToInt( retle );
	retle = evaluateNode( lb, branch->list_next->list_next->list_next );
        *c = evalCastLeToInt( retle );
	return( branch->list_next->list_next->list_next->list_next );
}

static le * eval_getint_4( lithp_burrito *lb, le * branch, int *a, int *b, int *c, int *d )
{
	le * retle = evaluateNode( lb, branch->list_next );
        *a = evalCastLeToInt( retle );
	retle = evaluateNode( lb, branch->list_next->list_next );
        *b = evalCastLeToInt( retle );
	retle = evaluateNode( lb, branch->list_next->list_next->list_next );
        *c = evalCastLeToInt( retle );
	retle = evaluateNode( lb, branch->list_next->list_next->list_next->list_next );
        *d = evalCastLeToInt( retle );
	return( branch->list_next->list_next->list_next->list_next->list_next );
}

static ttk_color eval_make_color( 
			lithp_burrito *lb, int r, int g, int b, char *m)
{
	ttk_color c;
	if( lb->isMono ) {
		if( !strcmp( m, "WHITE" ))         c = ttk_makecol( WHITE );
		else if ( !strcmp( m, "GREY" ))    c = ttk_makecol( GREY );
		else if ( !strcmp( m, "DKGREY" ))  c = ttk_makecol( DKGREY );
		else 				   c = ttk_makecol( BLACK );
	} else {
		c = ttk_makecol( r, g, b );
	}
	return( c );
}

le * eval_gfx_DrawPen ( lithp_burrito * lb, const int argc, le * branch )
{
	int r,g,b;
	le *mono, *result;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));
	
	mono = eval_getint_3( lb, branch, &r, &g, &b );
	result = evaluateNode( lb, mono );
	lb->pen1 = eval_make_color( lb, r, g, b, result->data );

	return( leNew( "T" ));
}

le * eval_gfx_DrawPen2 ( lithp_burrito * lb, const int argc, le * branch )
{
	int r,g,b;
	le *mono, *result;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));
	
	mono = eval_getint_3( lb, branch, &r, &g, &b );
	result = evaluateNode( lb, mono );
	lb->pen1 = eval_make_color( lb, r, g, b, result->data );
	return( leNew( "T" ));
}

le * eval_gfx_DrawPixel ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y;
	if( !lb || !branch || argc != 3 ) return( leNew( "NIL" ));

	eval_getint_2( lb, branch, &x, &y );
	ttk_pixel( lb->srf, x, y, lb->pen1 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawLine ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_line( lb->srf, x, y, a, b, lb->pen1 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawAALine ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_aaline( lb->srf, x, y, a, b, lb->pen1 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawRect ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_rect( lb->srf, x, y, a, b, lb->pen1 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawFillRect ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_fillrect( lb->srf, x, y, a, b, lb->pen1 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawVGradient ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_vgradient( lb->srf, x, y, a, b, lb->pen1, lb->pen2 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawHGradient ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_hgradient( lb->srf, x, y, a, b, lb->pen1, lb->pen2 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawPoly ( lithp_burrito * lb, const int argc, le * branch )
{
	return( leNew( "T" ));
}

le * eval_gfx_DrawFillPoly ( lithp_burrito * lb, const int argc, le * branch )
{
	return( leNew( "T" ));
}

le * eval_gfx_DrawEllipse ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_ellipse( lb->srf, x, y, a, b, lb->pen1 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawAAEllipse ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_aaellipse( lb->srf, x, y, a, b, lb->pen1 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawFillEllipse ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,a,b;
	if( !lb || !branch || argc != 5 ) return( leNew( "NIL" ));

	eval_getint_4( lb, branch, &x, &y, &a, &b );
	ttk_fillellipse( lb->srf, x, y, a, b, lb->pen1 );
	return( leNew( "T" ));
}

le * eval_gfx_DrawText ( lithp_burrito * lb, const int argc, le * branch )
{
/*
	le * t;
	le * t2;
	int x,y;
	if( !lb || !branch || argc < 5 ) return( leNew( "NIL" ));

	t = eval_getint_2( lb, branch, &x, &y );
	t2 = evaluateNode( lb, t );
	ttk_text( lb->srf, (font) x, y, lb->pen1, t2->data );

*/
	return( leNew( "T" ));
}

le * eval_gfx_DrawVectorText ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,w,h;
	le *t, *t2;

	if( !lb || !branch || argc < 6 ) return( leNew( "NIL" ));
	t = eval_getint_4( lb, branch, &x, &y, &w, &h );
	t2 = evaluateNode( lb, t );

	pz_vector_string( lb->srf, t2->data, x, y, w, h, 1, lb->pen1 );

	return( leNew( "T" ));
}

le * eval_gfx_DrawVectorTextCentered ( lithp_burrito * lb, const int argc, le * branch )
{
	int x,y,w,h;
	le *t, *t2;

	if( !lb || !branch || argc < 6 ) return( leNew( "NIL" ));
	t = eval_getint_4( lb, branch, &x, &y, &w, &h );
	t2 = evaluateNode( lb, t );

	pz_vector_string_center( lb->srf, t2->data, x, y, w, h, 1, lb->pen1 );

	return( leNew( "T" ));
}

le * eval_gfx_DrawClear ( lithp_burrito * lb, const int argc, le * branch )
{
	ttk_fillrect( lb->srf, 0, 0, lb->srf->w, lb->srf->h, lb->pen1 );
	return( leNew( "T" ));
}
