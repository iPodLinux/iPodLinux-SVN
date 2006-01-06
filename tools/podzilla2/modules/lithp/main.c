/* lithp - main
 *
 *      an example to show how to use lithp
 *      
 *      (c)2001/2006 Scott Lawrence   yorgle@gmail.com
 *
 *  NOTE: This was originally based on LITHP 0.6, which is available 
 *        at http://yorgle.cis.rit.edu/Software/lithp
 *        This version has been changed to remove the .nw dependancy,
 *        and adds the concept of the burrito, removing all globals.
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
#include "lithp.h"


int main( int argc, char* argv[] )
{
	int fileno;
	lithp_burrito *lb = burritoNew();

	printf( "lithp sample executable by jsl.lithp@absynth.com\n" );
	printf( "Version 0.7  January 2006\n" );

	if (argc <= 1) {
		fprintf(stderr, "ERROR: You must specify a .lsp file!\n");
		return __LINE__;
	}

	for (fileno = 0 ; fileno < argc-1 ; fileno ++)
	{
		/* parse in the file */
		printf("==== File %02d: %s\n", fileno, argv[fileno+1]);

		lb = burritoNew();
		Lithp_parseInFile( lb, argv[fileno+1] );
		/*Lithp_parseInString( lb, "(+ 5 4)(- 9 10)" );*/

		/* evaluate the read-in lists and free */
		//leDumpEval( lb, lb->list, 0); 
		leDumpEval( lb, lb->list, 0); 

		/* display the variables and free */
		printf("Variables:\n");
		variableDump( lb->mainVarList );

		/* display the user-defined functions and free */
		printf("defun's:\n");
		variableDump( lb->defunList );

		burritoDelete( lb );
	}
	return 0;
}
