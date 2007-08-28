/****************************************************************************
*
*	Name:			plldef.h
*
*	Description:	
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
*
*****************************************************************************

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA

****************************************************************************
*  $Author: davidm $
*  $Revision: 1.1 $
*  $Modtime: 6/26/02 8:56a $
****************************************************************************/

#ifndef PLLDEF_H
#define PLLDEF_H

/* Setup the PLL defines  */
#define PLL_F		0x350068
#define PLL_B		0x35006C

/* PLL bit definitions */
#define P52_PLL_CR_SLOW  BIT28
#define P52_PLL_CLK_RATE (BIT25 | BIT24)

#define RUSHMORE_PLL_CR_SLOW     BIT3
#define RUSHMORE_PLL_CLK_RATE    (BIT2 | BIT1 | BIT0)

/* P52 FCLK/UCLK defines */
/* ARM and USB */
#define P52_PLL_FAST_168MHZ		0x03dc8859
#define P52_PLL_FAST_144MHZ 	0x02D874DF  /* Divide by 3, F=144Mhz, U=72Mhz */
#define P52_PLL_FAST_120MHZ   0x01D46164  /* Divide by 3, F=120Mhz, U=60Mhz */
#define P52_PLL_FAST_96MHZ    0x0095BD38  /* Divide by 4, F=96Mhz,  U=48Mhz */

#define P52_PLL_SLOW_144MHZ 	0x12D874DF  /* Slow down FCLK, divide by 3, F=72Mhz, U=72Mhz */
#define P52_PLL_SLOW_120MHZ   0x11D46164  /* Slow down FCLK, divide by 3, F=60Mhz, U=60Mhz */
#define P52_PLL_SLOW_96MHZ    0x1095BD38  /* Slow down FCLK, divide by 4, F=48Mhz, U=48Mhz */

/* BCLK/PCLK defines */
/* ASB and APB */
#define P52_PLL_FAST_125MHZ 	0x02D53AC8  /* Divide by 3, B=125Mhz, P=62.5Mhz */
#define P52_PLL_FAST_100MHZ 	0x0196A51A  /* Divide by 4, B=100Mhz, P=50Mhz   */
#define P52_PLL_FAST_75MHZ    0x0090FBD4  /* Divide by 4, B=75Mhz,  P=37.5Mhz */

#define P52_PLL_SLOW_125MHZ 	0x12D53AC8  /* Divide by 3, B=62.5Mhz, P=31.25Mhz */
#define P52_PLL_SLOW_100MHZ 	0x1196A51A  /* Divide by 4, B=50Mhz,   P=25Mhz   */

/* Rushmore FCLK/UCLK defines */
/* ARM and USB */
#define RUSHMORE_PLL_FAST_168MHZ	0x7
#define RUSHMORE_PLL_FAST_144MHZ    0x5  /* Divide by 3, F=144Mhz, U=48Mhz */
#define RUSHMORE_PLL_FAST_120MHZ    0x4  /* Divide by 3, F=120Mhz, U=48Mhz */
#define RUSHMORE_PLL_FAST_96MHZ     0x3  /* Divide by 3, F=96Mhz,  U=48Mhz */

/* BCLK/PCLK defines */
/* ASB and APB */
#define RUSHMORE_PLL_FAST_125MHZ    0x4  /* Divide by 3, B=125Mhz, P=62.5Mhz */
#define RUSHMORE_PLL_FAST_100MHZ 	0x3  /* Divide by 3, B=100Mhz, P=50Mhz   */
#define RUSHMORE_PLL_FAST_75MHZ     0x2  /* Divide by 3, B=75Mhz,  P=37.5Mhz */

#endif	/* PLLDEF_H */