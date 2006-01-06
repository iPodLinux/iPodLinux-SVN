/* lithp - main wrapper interface
 *
 *      (c)2006 Scott Lawrence   yorgle@gmail.com
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

#include <stdlib.h>  /* for malloc */
#include "lithp.h"


lithp_burrito * burritoNew( void )
{
	lithp_burrito * lb;

	lb = (lithp_burrito *)malloc( sizeof( lithp_burrito ));
	if( !lb )  return( NULL );

	lb->list = NULL;
	lb->mainVarList = NULL;
	lb->defunList = NULL;

	lb->inFile = NULL;
	lb->inString = NULL;
	lb->pos = 0;

	return( lb );
}

void burritoDelete( lithp_burrito * lb )
{
	if( !lb )  return;

	leWipe( lb->list );
	variableFree( lb->mainVarList );
	variableFree( lb->defunList );
	free( lb );
}

