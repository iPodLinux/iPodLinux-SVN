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
* ported to TTK/pz2 by Courtney Cavin
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

#define LINES 1
#define AALINES 2
#define SOLID 4
#define HIDDEN 8


static PzModule *module;

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
static int	solid=SOLID;
static int zoom_out=0;
static int zoom_in=0;
static int paused=0;
static int photo=0;

typedef struct {
       int place;
       long avg;
} zsort;

struct point_3D {
	long x, y, z;
};

struct point_2D {
	long x, y;
};

struct line {
	int v1, v2;
};

static const struct line lines[12] =
{
	{0,1},{1,2},{2,3},{3,0},
	{4,5},{5,6},{6,7},{7,4},
	{0,5},{1,4},{2,7},{3,6}
};

static const int points[6][4] = {
	{0,1,2,3}, {4,5,6,7},
	{0,5,6,3}, {1,4,7,2},
	{2,3,6,7}, {0,1,4,5}
};

static struct point_3D sommet[8];
static struct point_3D point3D[8];
static struct point_2D point2D[8];

static long matrice[3][3];

static int nb_points = 8;

static zsort z_avgs_f[6];
static int x_off;
static int y_off;
static int z_off = 0;

/* Precalculated sine and cosine * 16384 (fixed point 18.14) */
static const short sin_table[91] =
{
            0,   285,   571,   857,  1142,  1427,  1712,  1996,  2280,  2563,
	 2845,  3126,  3406,  3685,  3963,  4240,  4516,  4790,  5062,  5334,
	 5603,  5871,  6137,  6401,  6663,  6924,  7182,  7438,  7691,  7943,
	 8191,  8438,  8682,  8923,  9161,  9397,  9630,  9860, 10086, 10310,
	10531, 10748, 10963, 11173, 11381, 11585, 11785, 11982, 12175, 12365,
	12550, 12732, 12910, 13084, 13254, 13420, 13582, 13740, 13894, 14043,
	14188, 14329, 14466, 14598, 14725, 14848, 14967, 15081, 15190, 15295,
	15395, 15491, 15582, 15668, 15749, 15825, 15897, 15964, 16025, 16082,
	16135, 16182, 16224, 16261, 16294, 16321, 16344, 16361, 16374, 16381,
	16384
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
	matrice[0][0] = (cza * cya) >> 14;
	matrice[1][0] = (sza * cya) >> 14;
	matrice[2][0] = -sya;

	matrice[0][1] = (((cza * sya) >> 14) * sxa - sza * cxa) >> 14;
	matrice[1][1] = (((sza * sya) >> 14) * sxa + cxa * cza) >> 14;
	matrice[2][1] = (sxa * cya) >> 14;

	matrice[0][2] = (((cza * sya) >> 14) * cxa + sza * sxa) >> 14;
	matrice[1][2] = (((sza * sya) >> 14) * cxa - cza * sxa) >> 14;
	matrice[2][2] = (cxa * cya) >> 14;

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
		point2D[i].x=(point3D[i].x << 8) /
			(point3D[i].z + (z_off << 14)) + x_off;
		point2D[i].y = (point3D[i].y << 8) /
			(point3D[i].z + (z_off << 14)) + y_off;
	}
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

static int z_cmp(const void *a, const void *b)
{
	return ((zsort *)b)->avg - ((zsort *)a)->avg;
}

static void cube_draw(PzWidget *wid, ttk_surface srf)
{
	int i, j;
	ttk_color col = ttk_ap_getx("window.fg")->color;
	int colors[] = {
		0xff0000, 0x00ff00, 0x0000ff,
		0xffff00, 0xff00ff, 0x00ffff,
		0xa0a0a0, 0xa0a0a0, 0x505050,
		0x505050, 0x000000, 0x000000
	};
#define FACE(a,b) point2D[points[a][b]]
#define XFACES(a) \
	FACE(a,0).x, FACE(a,1).x, FACE(a,2).x, FACE(a,3).x, FACE(a,0).x
#define YFACES(a) \
	FACE(a,0).y, FACE(a,1).y, FACE(a,2).y, FACE(a,3).y, FACE(a,0).y
	short xfaces[6][5] = {
		{XFACES(0)}, {XFACES(1)}, {XFACES(2)},
		{XFACES(3)}, {XFACES(4)}, {XFACES(5)}
	};
	short yfaces[6][5] = {
		{YFACES(0)}, {YFACES(1)}, {YFACES(2)},
		{YFACES(3)}, {YFACES(4)}, {YFACES(5)}
	};

#define ZFACE(a,b) point3D[points[a][b]].z
#define ZAVG(a) ((ZFACE(a,0) + ZFACE(a,1) + ZFACE(a,2) + ZFACE(a,3)) / 4)
	if (solid & SOLID) {
		for (i = 0; i < 6; i++) {
			z_avgs_f[i].place = i;
		}
		z_avgs_f[0].avg = ZAVG(0);
		z_avgs_f[1].avg = ZAVG(1);
		z_avgs_f[2].avg = ZAVG(2);
		z_avgs_f[3].avg = ZAVG(3);
		z_avgs_f[4].avg = ZAVG(4);
		z_avgs_f[5].avg = ZAVG(5);
		qsort(z_avgs_f, 6, sizeof(zsort), z_cmp);
		for (i = 3; i < 6; i++) {
			j = colors[z_avgs_f[i].place + (photo ? 0 : 6)];
			ttk_fillpoly(srf, 5, xfaces[z_avgs_f[i].place],
					yfaces[z_avgs_f[i].place],
					ttk_makecol((j&0xff0000) >> 16,
						(j&0xff00) >> 8, j&0xff));
		}
	}
	if (solid & LINES) {
		for (i = 0; i < 12; i++) {
			ttk_line(srf, point2D[lines[i].v1].x,
				      point2D[lines[i].v1].y,
				      point2D[lines[i].v2].x,
				      point2D[lines[i].v2].y, col);
		}
	}
	if (solid & AALINES) {
		if (!(solid & SOLID)) {
			for (i = 0; i < 6; i++) {
				z_avgs_f[i].place = i;
			}
			z_avgs_f[0].avg = ZAVG(0);
			z_avgs_f[1].avg = ZAVG(1);
			z_avgs_f[2].avg = ZAVG(2);
			z_avgs_f[3].avg = ZAVG(3);
			z_avgs_f[4].avg = ZAVG(4);
			z_avgs_f[5].avg = ZAVG(5);
			qsort(z_avgs_f, 6, sizeof(zsort), z_cmp);
		}
		for (i = 0; i < 12; i++) {
			ttk_aaline(srf, point2D[lines[i].v1].x,
					point2D[lines[i].v1].y,
					point2D[lines[i].v2].x,
					point2D[lines[i].v2].y, col);
		}
	}
	
	if (t_disp) {
		int h = ttk_text_height(ttk_textfont);
		ttk_fillrect(srf, 0, wid->h - (h + 4), wid->w, wid->h,
				ttk_ap_getx("window.bg")->color);
		ttk_line(srf, 0, wid->h - (h + 4), wid->w, wid->h - (h + 4),
			ttk_ap_getx("window.fg")->color);
		snprintf(t_buffer, 48, "%s:%d %s:%d %s:%d  |  d:%d ",
			t_ax[0], xs, t_ax[1], ys, t_ax[2], zs, z_off/10);
		ttk_text(srf, ttk_textfont, 2, wid->h - (h + 2),
				ttk_ap_getx("window.fg")->color, t_buffer);
	}
}

static int cube_loop(struct TWidget *this)
{
	if (paused)
		return 0;
	cube_rotate(xa,ya,za);
	if (zoom_out)
		cube_zoom_out();
	if (zoom_in)
		cube_zoom_in();
	cube_viewport();
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
	this->dirty = 1;
	return 0;
}

#define NMIN(x,y) if (x < y) x = y
#define NMAX(x,y) if (x > y) x = y
#define BOUNDS(x,y,z) NMIN(x,y);  NMAX(x,z)

static int cube_handle_event(PzEvent *e)
{
	int ret = 0;
	switch( e->type ) {
	case PZ_EVENT_BUTTON_DOWN:
		switch( e->arg ) {
		case PZ_BUTTON_ACTION:
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
			break;
		case PZ_BUTTON_PLAY:
			if (solid == SOLID) {
				if (ttk_screen->bpp >= 16) {
					solid = SOLID | AALINES;
				} else {
					solid = LINES;
					t_disp = 1;
				}
			}
			else if (solid == LINES) {
				solid = SOLID;
				t_disp = 0;
			}
			else if (solid == (AALINES | SOLID)) {
				solid = AALINES;
				t_disp = 1;
			}
			else if (solid == AALINES) {
				solid = SOLID;
				t_disp = 0;
				break;
			}
			break;
		case PZ_BUTTON_NEXT:
			zoom_in = 1;
			break;
		case PZ_BUTTON_PREVIOUS:
			zoom_out = 1;
			break;
		case PZ_BUTTON_HOLD:
			paused = 1;
			break;
		case PZ_BUTTON_MENU:
			pz_close_window( e->wid->win );
			break;
		}
		break;
	case PZ_EVENT_SCROLL:
		switch( xy_or_z ) {
		case 'x':
			xs+=e->arg;
			BOUNDS(xs, -10, 10);
			break;
		case 'y':
			ys+=e->arg;
			BOUNDS(ys, -10, 10);
			break;
		case 'z':
			zs+=e->arg;
			BOUNDS(zs, -10, 10);
			break;
		}
		break;
	case PZ_EVENT_BUTTON_UP:
		switch( e->arg ) {
		case PZ_BUTTON_NEXT:
			zoom_in = 0;
			break;
		case PZ_BUTTON_PREVIOUS:
			zoom_out = 0;
			break;
		case PZ_BUTTON_HOLD:
			paused = 0;
			break;
		}
		break;
	default:
		ret |= TTK_EV_UNUSED;
		break;
	}
	return ret;
}

static void cube_init(void)
{
       /* Original 3D-position of cube's corners */
       sommet[0].x = -dist;  sommet[0].y = -dist;      sommet[0].z = -dist;
       sommet[1].x =  dist;  sommet[1].y = -dist;      sommet[1].z = -dist;
       sommet[2].x =  dist;  sommet[2].y =      dist;  sommet[2].z = -dist;
       sommet[3].x = -dist;  sommet[3].y =      dist;  sommet[3].z = -dist;
       sommet[4].x =  dist;  sommet[4].y = -dist;      sommet[4].z =  dist;
       sommet[5].x = -dist;  sommet[5].y = -dist;      sommet[5].z =  dist;
       sommet[6].x = -dist;  sommet[6].y =      dist;  sommet[6].z =  dist;
       sommet[7].x =  dist;  sommet[7].y =      dist;  sommet[7].z =  dist;
}


PzWindow *new_cube_window( void )
{
	PzWindow *ret;
	PzWidget *wid;

	if (ttk_screen->w == 220) {
		if (!z_off) z_off = 400;
	} else if (ttk_screen->w == 138) {
		if (!z_off) z_off = 800;
	} else {
		if (!z_off) z_off = 600;
	}
	photo = (ttk_screen->bpp >= 16); /* for any color ipod */

	ret = pz_new_window(_("Cube"), PZ_WINDOW_NORMAL);
	wid = pz_add_widget(ret, cube_draw, cube_handle_event);

	ttk_widget_set_timer(wid, 100); /* ms */
	wid->timer = cube_loop;

	x_off = ttk_screen->w/2;
	y_off = (ttk_screen->h - ttk_screen->wy)/2;

	cube_init();

	return pz_finish_window(ret);
}

static void init_cube( void )
{
	module = pz_register_module("cube", NULL);
	pz_menu_add_action("/Extras/Stuff/Cube", new_cube_window);
}

PZ_MOD_INIT(init_cube)
