/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |         _  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id: cube.c,v 1.6 2005/08/02 21:14:22 courtc Exp $
*
* Copyright (C) 2002 Damien Teney
* modified to use int instead of float math by Andreas Zwirtes
* ported to iPod Linux/podzilla and solid rendering by Alastair S (coob)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

#include "pz.h"
#include "ipod.h"

static GR_WINDOW_ID		cube_wid;
static GR_GC_ID			cube_gc;
static GR_TIMER_ID		cube_timer;
static GR_WINDOW_ID		temp_pixmap;

static int dist=70;

static int xa=0;
static int ya=0;
static int za=0;
	
static int xs=1;
static int ys=3;
static int zs=1;

static int	t_disp=0;
static char	t_buffer[48];
static char t_ax[3][4] = {"[x]", "y", "z"};
static char	xy_or_z = 'x';
static int	solid=1;
static int zoom_out=0;
static int zoom_in=0;
static int paused=0;
static int photo=0;

struct point_3D {
	long x, y, z;
};

struct point_2D {
	long x, y;
};

typedef struct {
	int place;
	long avg;
} zsort;

typedef struct {
	int v1, v2;
} line_2D;

static struct point_3D sommet[8];
static struct point_3D point3D[8];
static struct point_2D point2D[8];

static long matrice[3][3];

static int nb_points = 8;

static int x_off;
static int y_off;
static int z_off = 0;

/* Precalculated sine and cosine * 10000 (four digit fixed point math) */
static int sin_table[91] = 
{
	0, 174, 348, 523, 697,
	871,1045,1218,1391,1564,
	1736,1908,2079,2249,2419,
	2588,2756,2923,3090,3255,
	3420,3583,3746,3907,4067,
	4226,4383,4539,4694,4848,
	5000,5150,5299,5446,5591,
	5735,5877,6018,6156,6293,
	6427,6560,6691,6819,6946,
	7071,7193,7313,7431,7547,
	7660,7771,7880,7986,8090,
	8191,8290,8386,8480,8571,
	8660,8746,8829,8910,8987,
	9063,9135,9205,9271,9335,
	9396,9455,9510,9563,9612,
	9659,9702,9743,9781,9816,
	9848,9876,9902,9925,9945,
	9961,9975,9986,9993,9998,
	10000
};


static long fisin(int val)
{
	/* Speed improvement through sukzessive lookup */
	if (val<181)
	{
		if (val<91)
		{
			/* phase 0-90 degree */
			return (long)sin_table[val];
		}
		else
		{
			/* phase 91-180 degree */
			return (long)sin_table[180-val];
		}
	}
	else
	{
		if (val<271)
		{
			/* phase 181-270 degree */
			return (-1L)*(long)sin_table[val-180];
		}
		else
		{
			/* phase 270-359 degree */
			return (-1L)*(long)sin_table[360-val];
		}
	}
	return 0;
}

static long ficos(int val)
{
	/* Speed improvement through sukzessive lookup */
	if (val<181)
	{
		if (val<91)
		{
			/* phase 0-90 degree */
			return (long)sin_table[90-val];
		}
		else
		{
			/* phase 91-180 degree */
			return (-1L)*(long)sin_table[val-90];
		}
	}
	else
	{
		if (val<271)
		{
			/* phase 181-270 degree */
			return (-1L)*(long)sin_table[270-val];
		}
		else
		{
			/* phase 270-359 degree */
			return (long)sin_table[val-270];
		}
	}
	return 0;
}


static void cube_rotate(int xa, int ya, int za)
{
	int i;

	/* Just to prevent unnecessary lookups */
	long sxa,cxa,sya,cya,sza,cza;
	sxa=fisin(xa);
	cxa=ficos(xa);
	sya=fisin(ya);
	cya=ficos(ya);
	sza=fisin(za);
	cza=ficos(za);

	/* calculate overall translation matrix */
	matrice[0][0] = cza*cya/10000L;
	matrice[1][0] = sza*cya/10000L;
	matrice[2][0] = -sya;

	matrice[0][1] = cza*sya/10000L*sxa/10000L - sza*cxa/10000L;
	matrice[1][1] = sza*sya/10000L*sxa/10000L + cxa*cza/10000L;
	matrice[2][1] = sxa*cya/10000L;

	matrice[0][2] = cza*sya/10000L*cxa/10000L + sza*sxa/10000L;
	matrice[1][2] = sza*sya/10000L*cxa/10000L - cza*sxa/10000L;
	matrice[2][2] = cxa*cya/10000L;

	/* apply translation matrix to all points */
	for(i=0;i<nb_points;i++)
	{
		point3D[i].x = matrice[0][0]*sommet[i].x + matrice[1][0]*sommet[i].y
			+ matrice[2][0]*sommet[i].z;
		
		point3D[i].y = matrice[0][1]*sommet[i].x + matrice[1][1]*sommet[i].y
			+ matrice[2][1]*sommet[i].z;
		
		point3D[i].z = matrice[0][2]*sommet[i].x + matrice[1][2]*sommet[i].y
			+ matrice[2][2]*sommet[i].z;
	}
}

static void cube_viewport(void)
{
	int i;

	/* Do viewport transformation for all points */
	for(i=0;i<nb_points;i++)
	{
		point2D[i].x=(((point3D[i].x)<<8)/10000L)/
			(point3D[i].z/10000L+z_off)+x_off;
		point2D[i].y=(((point3D[i].y)<<8)/10000L)/
			(point3D[i].z/10000L+z_off)+y_off;
	}
}

void cube_init(void)
{
	/* Original 3D-position of cube's corners */
	sommet[0].x = -dist;  sommet[0].y = -dist;	sommet[0].z = -dist;
	sommet[1].x =  dist;  sommet[1].y = -dist;	sommet[1].z = -dist;
	sommet[2].x =  dist;  sommet[2].y =	 dist;	sommet[2].z = -dist;
	sommet[3].x = -dist;  sommet[3].y =	 dist;	sommet[3].z = -dist;
	sommet[4].x =  dist;  sommet[4].y = -dist;	sommet[4].z =  dist;
	sommet[5].x = -dist;  sommet[5].y = -dist;	sommet[5].z =  dist;
	sommet[6].x = -dist;  sommet[6].y =	 dist;	sommet[6].z =  dist;
	sommet[7].x =  dist;  sommet[7].y =	 dist;	sommet[7].z =  dist;
}

static void cube_draw_line(int a, int b)
{
	GrLine(temp_pixmap, cube_gc, point2D[a].x, point2D[a].y, point2D[b].x, point2D[b].y);
}

static int compfunc(const void * a, const void * b)
{
	return ((zsort *)b)->avg - ((zsort *)a)->avg;
}

static void cube_zoom_out( void )
{
	if (z_off < 2000)
		z_off+=20;
}

static void cube_zoom_in( void )
{
	if (z_off > 300)
		z_off-=20;
}

static void cube_draw(void)
{
	int i;
	GR_COLOR colour; // limey alert!
	GR_POINT faces[6][5] = {
		{
			{point2D[0].x, point2D[0].y},
			{point2D[1].x, point2D[1].y},
			{point2D[2].x, point2D[2].y},
			{point2D[3].x, point2D[3].y},
			{point2D[0].x, point2D[0].y},
		},
		{
			{point2D[4].x, point2D[4].y},
			{point2D[5].x, point2D[5].y},
			{point2D[6].x, point2D[6].y},
			{point2D[7].x, point2D[7].y},
			{point2D[4].x, point2D[4].y},
		},
		{
			{point2D[0].x, point2D[0].y},
			{point2D[5].x, point2D[5].y},
			{point2D[6].x, point2D[6].y},
			{point2D[3].x, point2D[3].y},
			{point2D[0].x, point2D[0].y},
		},
		{
			{point2D[1].x, point2D[1].y},
			{point2D[4].x, point2D[4].y},
			{point2D[7].x, point2D[7].y},
			{point2D[2].x, point2D[2].y},
			{point2D[1].x, point2D[1].y},
		},
		{
			{point2D[2].x, point2D[2].y},
			{point2D[3].x, point2D[3].y},
			{point2D[6].x, point2D[6].y},
			{point2D[7].x, point2D[7].y},
			{point2D[2].x, point2D[2].y},
		},
		{
			{point2D[0].x, point2D[0].y},
			{point2D[1].x, point2D[1].y},
			{point2D[4].x, point2D[4].y},
			{point2D[5].x, point2D[5].y},
			{point2D[0].x, point2D[0].y}
		}
	};
	zsort z_avgs_f[6];
	line_2D lines[12] = {
		{0,1}, {1,2}, {2,3}, {3,0},
		{4,5}, {5,6}, {6,7}, {7,4},
		{0,5}, {1,4}, {2,7}, {3,6}
	};
	
	if (solid) {
		for (i = 0; i < 6; i++) {
			z_avgs_f[i].place = i;
		}
		z_avgs_f[0].avg =
			(point3D[0].z + point3D[1].z + point3D[2].z + point3D[3].z)/4;
		z_avgs_f[1].avg =
			(point3D[4].z + point3D[5].z + point3D[6].z + point3D[7].z)/4;
		z_avgs_f[2].avg =
			(point3D[0].z + point3D[5].z + point3D[6].z + point3D[3].z)/4;
		z_avgs_f[3].avg =
			(point3D[1].z + point3D[4].z + point3D[7].z + point3D[2].z)/4;
		z_avgs_f[4].avg =
			(point3D[2].z + point3D[3].z + point3D[6].z + point3D[7].z)/4;
		z_avgs_f[5].avg =
			(point3D[0].z + point3D[1].z + point3D[4].z + point3D[5].z)/4;
		qsort(z_avgs_f, 6, sizeof(zsort), compfunc);
		for (i = 3; i < 6; i++) { /* we can only see the front 3 faces... */
			switch(z_avgs_f[i].place) {
				case 0:
					colour = (photo==0) ? LTGRAY : RED;
					break;
				case 1:
					colour = (photo==0) ? LTGRAY : GREEN;
					break;
				case 2:
					colour = (photo==0) ? GRAY : BLUE;
					break;
				case 3:
					colour = (photo==0) ? GRAY : YELLOW;
					break;
				case 4:
					colour = (photo==0) ? BLACK : MAGENTA;
					break;
				case 5:
					colour = (photo==0) ? BLACK : CYAN;
					break;
			}
			GrSetGCForeground(cube_gc, colour);
			GrFillPoly(temp_pixmap, cube_gc, 5, faces[z_avgs_f[i].place]);
		}
	} else {
		/* polygons not used so hidden lines can be gray in future
		 *  - once i work out how to do it, possibly with GrRegion stuff */
		GrSetGCForeground(cube_gc, BLACK);
		for (i = 0; i < 12; i++) {
			cube_draw_line(lines[i].v1, lines[i].v2);
		}
	}
	
	if (t_disp) {
		GrSetGCForeground(cube_gc, WHITE);
		GrFillRect(temp_pixmap, cube_gc,
			0, screen_info.rows-(HEADER_TOPLINE + 1)-14, screen_info.cols, 14);
		GrSetGCForeground(cube_gc, BLACK);
		GrLine(temp_pixmap, cube_gc,
			0, screen_info.rows-(HEADER_TOPLINE + 1)-14,
			screen_info.cols, screen_info.rows-(HEADER_TOPLINE + 1)-14);
		snprintf(t_buffer, 48, "%s:%d %s:%d %s:%d  |  d:%d ",
			t_ax[0], xs, t_ax[1], ys, t_ax[2], zs, z_off/10);
		GrText(temp_pixmap, cube_gc, 2,
			screen_info.rows-(HEADER_TOPLINE + 1)-2, t_buffer, -1, GR_TFASCII);
	}
	GrCopyArea(cube_wid, cube_gc, 0, 0,
		screen_info.cols, (screen_info.rows - (HEADER_TOPLINE + 1)),
		temp_pixmap, 0, 0, MWROP_SRCCOPY);
}

static void cube_clear_screen( void )
{
	GrSetGCForeground(cube_gc, WHITE);
	GrFillRect(temp_pixmap, cube_gc,
		0, 0, screen_info.cols, (screen_info.rows - (HEADER_TOPLINE + 1)));
}

static void cube_do_draw( void )
{
	pz_draw_header(_("Cube"));
}

static void cube_loop( void )
{
	cube_clear_screen();
	cube_rotate(xa,ya,za);
	if (zoom_out)
		cube_zoom_out();
	if (zoom_in)
		cube_zoom_in();
	cube_viewport();
	cube_draw();
	xa+=xs;
	if (xa>359)
		xa-=360;
	if (xa<0)
		xa+=360;
	ya+=ys;
	if (ya>359)
		ya-=360;
	if (ya<0)
		ya+=360;
	za+=zs;
	if (za>359)
		za-=360;
	if (za<0)
		za+=360;
}

static int cube_handle_event(GR_EVENT * event)
{
	int ret = 0;
	switch( event->type )
	{
		case( GR_EVENT_TYPE_TIMER ):
			if (!paused) cube_loop();
			break;
		case( GR_EVENT_TYPE_KEY_DOWN ):
			switch( event->keystroke.ch )
			{
				case '\r':
				case '\n': /* action */
					switch( xy_or_z ) {
						case 'x':
							xy_or_z ='y';
							strncpy(t_ax[0], "x", 4);
							strncpy(t_ax[1], "[y]", 4);
							break;
						case 'y':
							xy_or_z ='z';
							strncpy(t_ax[1], "y", 4);
							strncpy(t_ax[2], "[z]", 4);
							break;
						case 'z':
							xy_or_z ='x';
							strncpy(t_ax[2], "z", 4);
							strncpy(t_ax[0], "[x]", 4);
							break;
					}
					ret |= KEY_CLICK;
					break;
				case 'p': /* play/pause */
				case 'd': /*or this */
					if (t_disp) { t_disp=0; } else { t_disp=1; }
					if (solid) { solid=0; } else { solid=1; }
					break;
				case 'f': /* >>| */
					zoom_in = 1;
					break;
				case 'w': /* |<< */
					zoom_out = 1;
					break;
				case 'l': /* CCW spin */
					switch( xy_or_z )
					{
						case 'x':
							xs-=1;
							if (xs<-10) { xs=-10;}
							break;
						case 'y':
							ys-=1;
							if (ys<-10) { ys=-10;}
							break;
						case 'z':
							zs-=1;
							if (zs<-10) { zs=-10;}
							break;
					}
					break;
				case 'r': /* CW spin */
					switch( xy_or_z )
					{
						case 'x':
							xs+=1;
							if (xs>10) { xs=10;}
							break;
						case 'y':
							ys+=1;
							if (ys>10) { ys=10;}
							break;
						case 'z':
							zs+=1;
							if (zs>10) { zs=10;}
							break;
					}
					break;
				case 'h': /* hold */
					paused=1;
					break;
				case 'm':
					GrDestroyTimer( cube_timer );
					GrDestroyGC( cube_gc );
					pz_close_window( cube_wid );
					ret |= KEY_CLICK;
					break;
			}
			break;
		case( GR_EVENT_TYPE_KEY_UP ):
			switch( event->keystroke.ch )
			{
				case 'f': /* >>| */
					zoom_in = 0;
					break;
				case 'w': /* |<< */
					zoom_out = 0;
					break;
				case 'h': /* hold */
					paused = 0;
					break;
			}
			break;
	}
	return ret;
}

void new_cube_window( void )
{
	if (screen_info.cols==220) {
		photo = 1;
		if (!z_off) z_off = 400;
	} else if (screen_info.cols==138) {
		if (!z_off) z_off = 800;
	} else {
		if (!z_off) z_off = 600;
	}

	cube_gc = pz_get_gc(1);
	GrSetGCUseBackground(cube_gc, GR_FALSE);
	GrSetGCForeground(cube_gc, BLACK);

	cube_wid = pz_new_window(0, HEADER_TOPLINE + 1, 
	screen_info.cols, screen_info.rows - (HEADER_TOPLINE + 1), 
	cube_do_draw, cube_handle_event);

	GrSelectEvents( cube_wid, GR_EVENT_MASK_TIMER|
	GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_UP|GR_EVENT_MASK_KEY_DOWN);

    cube_timer = GrCreateTimer( cube_wid, 100 );

	GrMapWindow( cube_wid );

	x_off = screen_info.cols/2;
	y_off = (screen_info.rows - (HEADER_TOPLINE + 1))/2;

	temp_pixmap = GrNewPixmap(screen_info.cols,
								(screen_info.rows - (HEADER_TOPLINE + 1)),
						 		NULL);
	
	cube_init();	
}
