/*
 * Copyright (C) 2005 Rebecca G. Bettencourt
 *
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

#include "pz.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Structures for PodPaint UI Objects */

typedef struct podpaint_tool {
	ttk_surface icon;
	ttk_surface selicon;
	ttk_surface cursor;
	void (* click)(ttk_surface srf, int x, int y, ttk_color c);
	void (* drag)(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c);
	void (* dragClick)(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c);
	void (* dragContinue)(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c);
	void (* dragClickContinue)(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c);
	int (* visible)();
	int autoClose;
} podpaint_tool;

typedef struct podpaint_menu {
	char * title;
	int titlex1;
	int titlex2;
	int width;
	int height;
	int numitems;
	char ** items;
	int (** actions)(TWidget * wid, ttk_surface srf);
} podpaint_menu;

#define ceildiv(a,b) (((a)+(b)-1)/(b))
#define floordiv(a,b) ((a)/(b))
#define ISEXT(a,b,c,d) ( (a[0]=='.') && (a[1]==(b) || a[1]==((b)+32)) && (a[2]==(c) || a[2]==((c)+32)) && (a[3]==(d) || a[3]==((d)+32)) )

/* Dependent on Text Input Module */

typedef struct TiBuffer {
	char * text;
	int asize; /* allocated size */
	int usize; /* used size */
	int cpos; /* cursor position */
	int idata[4];
	void * data;
	int (*callback)(TWidget *, char *); /* callback function */
} TiBuffer;
extern TiBuffer * ti_create_buffer(int size);
extern void ti_destroy_buffer(TiBuffer * buf);
extern void ti_buffer_input(TiBuffer * buf, int ch);
extern void ti_multiline_text(ttk_surface srf, ttk_font fnt, int x, int y, int w, int h, ttk_color col, const char *t, int cursorpos, int scroll, int * lc, int * sl, int * cb);
extern TWidget * ti_new_standard_text_widget(int x, int y, int w, int h, int absheight, char * dt, int (*callback)(TWidget *, char *));
extern int ti_widget_start(TWidget * wid);

/* PodPaint Variables & Crap */

static PzModule * podpaint_module;

static ttk_surface podpaint_arrowcur;

static int podpaint_inited = 0;

static int podpaint_toolcount = 0;
static int podpaint_vtoolcount = 0;
static int podpaint_menucount = 0;
static int podpaint_colorcount = 0;

static podpaint_tool ** podpaint_toolbox = 0;
static podpaint_menu ** podpaint_menubar = 0;
static ttk_color * podpaint_palette = 0;

static int podpaint_sel_tool = 0;
static int podpaint_sel_menu = -1;
static ttk_color podpaint_sel_color;

static int podpaint_menubar_height;
static int podpaint_toolbox_width;
static int podpaint_toolbox_height;
static int podpaint_palette_height;

static int podpaint_toolsx;
static int podpaint_toolsy;

static int podpaint_colorsx;
static int podpaint_colorsy;

static int podpaint_last_x = 0;
static int podpaint_last_y = 0;
static int podpaint_dragging = 0;
static int podpaint_cursorstate = 0;

static int podpaint_fillmode = 0;
static int podpaint_aamode = 0;

static int podpaint_area_made = 0;
static ttk_surface podpaint_area;
static ttk_surface podpaint_area_temp;
static ttk_surface podpaint_area_undo;

static int podpaint_scrollx = 0;
static int podpaint_scrolly = 0;
static int podpaint_maxscrollx = 0;
static int podpaint_maxscrolly = 0;
static int podpaint_scrollsavex = 0;
static int podpaint_scrollsavey = 0;

static int podpaint_text_x;
static int podpaint_text_y;
static ttk_color podpaint_text_col;
static TiBuffer * podpaint_text = 0;

static TWidget * podpaint_controller = 0;
static PzWindow * podpaint_window = 0;
static ttk_menu_item podpaint_fbx;

static int (*podpaint_colorpicker)(const char *, ttk_color *) = 0;

/* Dependent on Mouse Emulation Module */

typedef struct ttk_mousedata {
	int x;
	int y;
	int scrollaxis;
	ttk_surface cursor;
	int hidecursor;
	void (*draw)(TWidget * wid, ttk_surface srf);
	void (*mouseDown)(TWidget * wid, int x, int y);
	void (*mouseStillDown)(TWidget * wid, int x, int y);
	void (*mouseUp)(TWidget * wid, int x, int y);
	void (*mouseMove)(TWidget * wid, int x, int y);
	int mouseDownHappened;
} ttk_mousedata;
extern ttk_surface ttk_get_cursor(char * n);
extern TWidget * ttk_new_mouse_widget_with(int x, int y, int w, int h, ttk_surface cursor,
	void (*d)(TWidget * wid, ttk_surface srf),
	void (*md)(TWidget * wid, int x, int y),
	void (*msd)(TWidget * wid, int x, int y),
	void (*mu)(TWidget * wid, int x, int y),
	void (*mm)(TWidget * wid, int x, int y)
);

/* PodPaint UI Object Manipulation */

podpaint_tool * new_podpaint_tool(ttk_surface i, ttk_surface si, ttk_surface cu,
	void (* c)(ttk_surface srf, int x, int y, ttk_color c),
	void (* d)(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c),
	void (* dk)(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c),
	void (* dc)(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c),
	void (* dck)(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c),
	int (* v)(),
	int ac
)
{
	podpaint_tool * t = (podpaint_tool *)calloc(1, sizeof(podpaint_tool));
	if (t) {
		t->icon = i;
		t->selicon = si;
		t->cursor = cu;
		t->click = c;
		t->drag = d;
		t->dragClick = dk;
		t->dragContinue = dc;
		t->dragClickContinue = dck;
		t->visible = v;
		t->autoClose = ac;
	}
	return t;
}

void podpaint_add_tool(podpaint_tool * t)
{
	int p = podpaint_toolcount++;
	int n = podpaint_toolcount;
	if (podpaint_toolbox) {
		podpaint_tool ** t = (podpaint_tool **)realloc(podpaint_toolbox, n * sizeof(podpaint_tool *));
		if (t) podpaint_toolbox = t;
	} else {
		podpaint_toolbox = (podpaint_tool **)malloc(n * sizeof(podpaint_tool *));
	}
	if (t->visible) {
		if (t->visible()) {
			podpaint_vtoolcount++;
		}
	} else {
		podpaint_vtoolcount++;
	}
	if (podpaint_toolbox) podpaint_toolbox[p] = t;
}

void podpaint_free_tools()
{
	while (podpaint_toolcount) {
		free(podpaint_toolbox[--podpaint_toolcount]);
	}
	free(podpaint_toolbox);
	podpaint_toolbox = 0;
	podpaint_vtoolcount = 0;
}

podpaint_menu * new_podpaint_menu(char * title)
{
	podpaint_menu * m = (podpaint_menu *)calloc(1, sizeof(podpaint_menu));
	if (m) m->title = title;
	return m;
}

void podpaint_add_menuitem(podpaint_menu * m, char * name, int (* act)(TWidget * wid, ttk_surface srf))
{
	int p = m->numitems++;
	int n = m->numitems;
	if (m->items) {
		char ** t = (char **)realloc(m->items, n * sizeof(char *));
		if (t) m->items = t;
	} else {
		m->items = (char **)malloc(n * sizeof(char *));
	}
	if (m->actions) {
		int (** t)(TWidget *, ttk_surface);
		t = (int (**)(TWidget *, ttk_surface))realloc(m->actions, n * sizeof(int (*)(TWidget *, ttk_surface)));
		if (t) m->actions = t;
	} else {
		m->actions = (int (**)(TWidget *, ttk_surface))malloc(n * sizeof(int (*)(TWidget *, ttk_surface)));
	}
	if (m->items) m->items[p] = name;
	if (m->actions) m->actions[p] = act;
	if (ttk_text_width(ttk_textfont, name) > m->width) {
		m->width = ttk_text_width(ttk_textfont, name);
	}
	m->height = n*ttk_text_height(ttk_textfont);
}

void podpaint_add_menu(podpaint_menu * m)
{
	int p = podpaint_menucount++;
	int n = podpaint_menucount;
	if (podpaint_menubar) {
		podpaint_menu ** t = (podpaint_menu **)realloc(podpaint_menubar, n * sizeof(podpaint_menu *));
		if (t) podpaint_menubar = t;
	} else {
		podpaint_menubar = (podpaint_menu **)malloc(n * sizeof(podpaint_menu *));
	}
	if (podpaint_menubar) podpaint_menubar[p] = m;
	if (p>0) {
		m->titlex1 = podpaint_menubar[p-1]->titlex2;
	} else {
		m->titlex1 = 0;
	}
	m->titlex2 = m->titlex1 + ttk_text_width(ttk_textfont, m->title);
}

void podpaint_free_menus()
{
	while (podpaint_menucount) {
		free(podpaint_menubar[--podpaint_menucount]);
	}
	free(podpaint_menubar);
	podpaint_menubar = 0;
}

void podpaint_add_color(ttk_color c)
{
	int p = podpaint_colorcount++;
	int n = podpaint_colorcount;
	if (podpaint_palette) {
		ttk_color * t = (ttk_color *)realloc(podpaint_palette, n * sizeof(ttk_color));
		if (t) podpaint_palette = t;
	} else {
		podpaint_palette = (ttk_color *)malloc(n * sizeof(ttk_color));
	}
	if (podpaint_palette) podpaint_palette[p] = c;
}

void podpaint_free_colors()
{
	podpaint_colorcount = 0;
	free(podpaint_palette);
	podpaint_palette = 0;
}

/* PodPaint Special Drawing Functions / Tool Handlers */

void podpaint_paintbrush(ttk_surface srf, int x, int y, ttk_color c)
{
	ttk_pixel(srf, x, y, c);
	ttk_pixel(srf, x+1, y, c);
	ttk_pixel(srf, x, y+1, c);
	ttk_pixel(srf, x+1, y+1, c);
	ttk_pixel(srf, x, y-1, c);
	ttk_pixel(srf, x+1, y-1, c);
	ttk_pixel(srf, x, y+2, c);
	ttk_pixel(srf, x+1, y+2, c);
	ttk_pixel(srf, x-1, y, c);
	ttk_pixel(srf, x-1, y+1, c);
	ttk_pixel(srf, x+2, y, c);
	ttk_pixel(srf, x+2, y+1, c);
}

void podpaint_eraser(ttk_surface srf, int x, int y, ttk_color c)
{
	ttk_fillrect(srf, x, y, x+8, y+8, ttk_makecol(WHITE));
}

void podpaint_spraypaint(ttk_surface srf, int x, int y, ttk_color c)
{
	int pts[] = {
		6, 0,	4, 1,	8, 2,	1, 3,	3, 3,	6, 3,	7, 4,	10, 4,
		0, 5,	2, 5,	5, 5,	8, 6,	11, 6,	1, 7,	3, 7,
		6, 7,	8, 8,	10, 8,	2, 9,	5, 9,	8, 10,	6, 11
	};
	int n = 44;
	while (n) {
		ttk_pixel(srf, x+pts[n-2], y+pts[n-1], c);
		n -= 2;
	}
}

static int podpaint_fillstack[1024][4];
static int podpaint_fillstackptr;

static int colcmp(ttk_color c1, ttk_color c2)
{
	int a, b, c, d, e, f;
	ttk_unmakecol(c1, &a, &b, &c);
	ttk_unmakecol(c2, &d, &e, &f);
	return (a==d && b==e && c==f);
}

static void podpaint_fillpush(int y, int xleft, int xright, int dy, int ph)
{
	if ((y+dy) >= 0 && (y+dy) < ph) {
		podpaint_fillstack[podpaint_fillstackptr][0] = y;
		podpaint_fillstack[podpaint_fillstackptr][1] = xleft;
		podpaint_fillstack[podpaint_fillstackptr][2] = xright;
		podpaint_fillstack[podpaint_fillstackptr][3] = dy;
		podpaint_fillstackptr++;
	}
}

static void podpaint_fillpop(int * y, int * xleft, int * xright, int * dy)
{
	podpaint_fillstackptr--;
	*dy = podpaint_fillstack[podpaint_fillstackptr][3];
	*y = podpaint_fillstack[podpaint_fillstackptr][0] + *dy;
	*xleft = podpaint_fillstack[podpaint_fillstackptr][1];
	*xright = podpaint_fillstack[podpaint_fillstackptr][2];
}

static void podpaint_dofill(int x, int y, ttk_color fill, ttk_color whil, ttk_surface pic, int pw, int ph)
{
	/* I don't remember where I got this algorithm from, though */
	/* it has its origins in Graphics Gems vol. 1 pp. 275-277.  */
	/* Do not ask me what the heck this is doing; I don't know. */
	int x1, x2, start, dy;
	if (x < 0 || x >= pw || y < 0 || y >= ph || !colcmp(ttk_getpixel(pic,x,y),whil) || colcmp(ttk_getpixel(pic,x,y),fill)) return;
	podpaint_fillstackptr = 0;
	podpaint_fillpush(y, x, x, 1, ph);
	podpaint_fillpush(y+1, x, x, -1, ph);
	while (podpaint_fillstackptr > 0) {
		podpaint_fillpop(&y, &x1, &x2, &dy);
		for (x = x1; x >= 0 && colcmp(ttk_getpixel(pic,x,y), whil); x--) {
			ttk_pixel(pic, x, y, fill);
		}
		if (x >= x1) goto skip;
		start = x+1;
		if (start < x1) {
			podpaint_fillpush(y, start, x1-1, -dy, ph);
		}
		x = x1+1;
		do {
			while (x < pw && colcmp(ttk_getpixel(pic,x,y), whil)) {
				ttk_pixel(pic, x, y, fill);
				x++;
			}
			podpaint_fillpush(y, start, x-1, dy, ph);
			if (x > x2+1) {
				podpaint_fillpush(y, x2+1, x-1, -dy, ph);
			}
			skip:
			for (x++; x <= x2 && !colcmp(ttk_getpixel(pic,x,y), whil); x++);
			start = x;
			if (podpaint_fillstackptr > 1020) {
				pz_message(_("Area is too complex to fill completely."));
				return;
			}
		} while (x <= x2);
		if (podpaint_fillstackptr > 1020) {
			pz_message(_("Area is too complex to fill completely."));
			return;
		}
	}
}

void podpaint_fill(ttk_surface srf, int x, int y, ttk_color c)
{
	int w, h;
	ttk_surface_get_dimen(srf, &w, &h);
	podpaint_dofill(x, y, c, ttk_getpixel(srf, x, y), srf, w, h);
}

void podpaint_eyedropper(ttk_surface srf, int x, int y, ttk_color c)
{
	podpaint_sel_color = ttk_getpixel(srf, x, y);
}

void podpaint_text_tool(ttk_surface srf, int x, int y, ttk_color c)
{
	podpaint_text_x = x;
	podpaint_text_y = y-2;
	podpaint_text_col = c;
	if (podpaint_text) {
		ti_destroy_buffer(podpaint_text);
		podpaint_text = 0;
	}
	podpaint_text = ti_create_buffer(0);
	pz_start_input_for(podpaint_window);
}

#define ABS(n) (((n)<0)?(-(n)):(n))

void podpaint_ellipse(ttk_surface srf, int x, int y, int rx, int ry, ttk_color c) { ttk_ellipse(srf, x, y, ABS(rx-x), ABS(ry-y), c); }
void podpaint_aaellipse(ttk_surface srf, int x, int y, int rx, int ry, ttk_color c) { ttk_aaellipse(srf, x, y, ABS(rx-x), ABS(ry-y), c); }
void podpaint_fillellipse(ttk_surface srf, int x, int y, int rx, int ry, ttk_color c) { ttk_fillellipse(srf, x, y, ABS(rx-x), ABS(ry-y), c); }
void podpaint_aafillellipse(ttk_surface srf, int x, int y, int rx, int ry, ttk_color c) { ttk_aafillellipse(srf, x, y, ABS(rx-x), ABS(ry-y), c); }

void podpaint_grabd(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color c)
{
	podpaint_scrollx = (podpaint_scrollsavex + x1 - x2);
	podpaint_scrolly = (podpaint_scrollsavey + y1 - y2);
	if (podpaint_scrollx > podpaint_maxscrollx) podpaint_scrollx = podpaint_maxscrollx;
	if (podpaint_scrollx < 0) podpaint_scrollx = 0;
	if (podpaint_scrolly > podpaint_maxscrolly) podpaint_scrolly = podpaint_maxscrolly;
	if (podpaint_scrolly < 0) podpaint_scrolly = 0;
}

void podpaint_grabc(ttk_surface srf, int x, int y, ttk_color c)
{
	podpaint_scrollsavex = podpaint_scrollx;
	podpaint_scrollsavey = podpaint_scrolly;
}

static int podpaint_bezier_x[4];
static int podpaint_bezier_y[4];
static int podpaint_bezier_curpoint = 0;

void podpaint_bezierstart(ttk_surface srf, int x, int y, ttk_color c)
{
	podpaint_bezier_x[0] = x;
	podpaint_bezier_y[0] = y;
	podpaint_bezier_curpoint = 1;
}

void podpaint_bezier(ttk_surface srf, int px, int py, int x, int y, ttk_color c)
{
	podpaint_bezier_x[podpaint_bezier_curpoint] = x;
	podpaint_bezier_y[podpaint_bezier_curpoint] = y;
	switch (podpaint_bezier_curpoint+1) {
	case 4:
		ttk_bezier(srf, podpaint_bezier_x[0], podpaint_bezier_y[0], podpaint_bezier_x[1], podpaint_bezier_y[1],
			podpaint_bezier_x[2], podpaint_bezier_y[2], podpaint_bezier_x[3], podpaint_bezier_y[3], 50, c);
		ttk_line(srf, podpaint_bezier_x[2], podpaint_bezier_y[2], podpaint_bezier_x[3], podpaint_bezier_y[3], c);
	case 3:
		ttk_line(srf, podpaint_bezier_x[1], podpaint_bezier_y[1], podpaint_bezier_x[2], podpaint_bezier_y[2], c);
	case 2:
		ttk_line(srf, podpaint_bezier_x[0], podpaint_bezier_y[0], podpaint_bezier_x[1], podpaint_bezier_y[1], c);
	}
}

void podpaint_bezierclick(ttk_surface srf, int px, int py, int x, int y, ttk_color c)
{
	podpaint_bezier_x[podpaint_bezier_curpoint] = x;
	podpaint_bezier_y[podpaint_bezier_curpoint] = y;
	podpaint_bezier_curpoint++;
	switch (podpaint_bezier_curpoint) {
	case 4:
		podpaint_dragging = 0;
		podpaint_bezier_curpoint = 0;
		ttk_bezier(srf, podpaint_bezier_x[0], podpaint_bezier_y[0], podpaint_bezier_x[1], podpaint_bezier_y[1],
			podpaint_bezier_x[2], podpaint_bezier_y[2], podpaint_bezier_x[3], podpaint_bezier_y[3], 50, c);
		break;
	}
}

void podpaint_aabezier(ttk_surface srf, int px, int py, int x, int y, ttk_color c)
{
	podpaint_bezier_x[podpaint_bezier_curpoint] = x;
	podpaint_bezier_y[podpaint_bezier_curpoint] = y;
	switch (podpaint_bezier_curpoint+1) {
	case 4:
		ttk_aabezier(srf, podpaint_bezier_x[0], podpaint_bezier_y[0], podpaint_bezier_x[1], podpaint_bezier_y[1],
			podpaint_bezier_x[2], podpaint_bezier_y[2], podpaint_bezier_x[3], podpaint_bezier_y[3], 50, c);
		ttk_aaline(srf, podpaint_bezier_x[2], podpaint_bezier_y[2], podpaint_bezier_x[3], podpaint_bezier_y[3], c);
	case 3:
		ttk_aaline(srf, podpaint_bezier_x[1], podpaint_bezier_y[1], podpaint_bezier_x[2], podpaint_bezier_y[2], c);
	case 2:
		ttk_aaline(srf, podpaint_bezier_x[0], podpaint_bezier_y[0], podpaint_bezier_x[1], podpaint_bezier_y[1], c);
	}
}

void podpaint_aabezierclick(ttk_surface srf, int px, int py, int x, int y, ttk_color c)
{
	podpaint_bezier_x[podpaint_bezier_curpoint] = x;
	podpaint_bezier_y[podpaint_bezier_curpoint] = y;
	podpaint_bezier_curpoint++;
	switch (podpaint_bezier_curpoint) {
	case 4:
		podpaint_dragging = 0;
		podpaint_bezier_curpoint = 0;
		ttk_aabezier(srf, podpaint_bezier_x[0], podpaint_bezier_y[0], podpaint_bezier_x[1], podpaint_bezier_y[1],
			podpaint_bezier_x[2], podpaint_bezier_y[2], podpaint_bezier_x[3], podpaint_bezier_y[3], 50, c);
		break;
	}
}

#define PODPAINT_MAX_POLY 128
static short podpaint_poly_x[PODPAINT_MAX_POLY];
static short podpaint_poly_y[PODPAINT_MAX_POLY];
static short podpaint_poly_cur = 0;

void podpaint_polystart(ttk_surface srf, int x, int y, ttk_color c)
{
	podpaint_poly_x[0] = x;
	podpaint_poly_y[0] = y;
	podpaint_poly_cur = 1;
}

void podpaint_poly(ttk_surface srf, int px, int py, int x, int y, ttk_color c)
{
	int cur = podpaint_poly_cur;
	podpaint_poly_x[cur] = x;
	podpaint_poly_y[cur] = y;
	cur++;
	while (--cur)
	{
		ttk_line(srf, podpaint_poly_x[cur-1], podpaint_poly_y[cur-1], podpaint_poly_x[cur], podpaint_poly_y[cur], c);
	}
}

void podpaint_polyclick(ttk_surface srf, int px, int py, int x, int y, ttk_color c)
{
	int cur = podpaint_poly_cur;
	podpaint_poly_x[cur] = x;
	podpaint_poly_y[cur] = y;
	podpaint_poly_cur++; cur++;
	if ((px == x && py == y) || cur >= PODPAINT_MAX_POLY) {
		ttk_fillpoly(srf, podpaint_poly_cur, podpaint_poly_x, podpaint_poly_y, c);
		podpaint_dragging = 0;
	} else while (--cur) {
		ttk_line(srf, podpaint_poly_x[cur-1], podpaint_poly_y[cur-1], podpaint_poly_x[cur], podpaint_poly_y[cur], c);
	}
}

void podpaint_aapoly(ttk_surface srf, int px, int py, int x, int y, ttk_color c)
{
	int cur = podpaint_poly_cur;
	podpaint_poly_x[cur] = x;
	podpaint_poly_y[cur] = y;
	cur++;
	while (--cur)
	{
		ttk_aaline(srf, podpaint_poly_x[cur-1], podpaint_poly_y[cur-1], podpaint_poly_x[cur], podpaint_poly_y[cur], c);
	}
}

void podpaint_aapolyclick(ttk_surface srf, int px, int py, int x, int y, ttk_color c)
{
	int cur = podpaint_poly_cur;
	podpaint_poly_x[cur] = x;
	podpaint_poly_y[cur] = y;
	podpaint_poly_cur++; cur++;
	if ((px == x && py == y) || cur >= PODPAINT_MAX_POLY) {
		ttk_fillpoly(srf, podpaint_poly_cur, podpaint_poly_x, podpaint_poly_y, c);
		podpaint_dragging = 0;
	} else while (--cur) {
		ttk_aaline(srf, podpaint_poly_x[cur-1], podpaint_poly_y[cur-1], podpaint_poly_x[cur], podpaint_poly_y[cur], c);
	}
}

/* BMP Writing Helper Functions */

static int write_word(FILE * fp, unsigned short w)
{
	       putc(w     , fp);
	return putc(w >> 8, fp);
}

static int write_dword(FILE * fp, unsigned int dw)
{
	       putc(dw      , fp);
	       putc(dw >> 8 , fp);
	       putc(dw >> 16, fp);
	return putc(dw >> 24, fp);
}

static int write_long(FILE * fp, int l)
{
	       putc(l      , fp);
	       putc(l >> 8 , fp);
	       putc(l >> 16, fp);
	return putc(l >> 24, fp);
}

/* PodPaint Menu Handlers */

int podpaint_save_callback(TWidget * wid, char * txt)
{
	FILE * fp;
	int w, h, x, y, z;
	char ext[4] = {0, 0, 0, 0};
	
	pz_close_window(wid->win);
	if (!txt[0]) return 0;
	if (strlen(txt)>4) {
		strncpy(ext, txt+strlen(txt)-4, 4);
	}
	fp = fopen(txt, "w");
	if (!fp) {
		pz_error(_("Could not save file."));
		return 0;
	}
	ttk_surface_get_dimen(podpaint_area, &w, &h);
	if (ISEXT(ext,'P','P','M')) {
		fprintf(fp, "P3\n");
		fprintf(fp, "#generated by podpaint/podzilla\n");
		fprintf(fp, "%d %d 255\n", w, h);
		for (x=0; x<(w*h); x++) {
			int r, g, b;
			ttk_color c = ttk_getpixel(podpaint_area, (x % w), (x / w));
			ttk_unmakecol(c, &r, &g, &b);
			fprintf(fp, "%d %d %d\n", r, g, b);
		}
	} else {
		/* default is bmp */
		putc(66, fp);
		putc(77, fp);
		write_dword(fp, 54+(w*h*24)/8);
		write_dword(fp, 0);
		write_dword(fp, 54);
		write_dword(fp, 40);
		write_long(fp, w);
		write_long(fp, h);
		write_word(fp, 1);
		write_word(fp, 24);
		write_dword(fp, 0);
		write_dword(fp, 0);
		write_long(fp, 0);
		write_long(fp, 0);
		write_dword(fp, 0);
		write_dword(fp, 0);
		for (y=h-1; y>=0; y--) {
			for (x=0; x<w; x++) {
				int r, g, b;
				ttk_color c = ttk_getpixel(podpaint_area, x, y);
				ttk_unmakecol(c, &r, &g, &b);
				putc(b, fp);
				putc(g, fp);
				putc(r, fp);
			}
			for (z=w%4; z>0; z--) {
				putc(0, fp);
			}
		}
	}
	fclose(fp);
	
	return 0;
}

int podpaint_save(TWidget * wxd, ttk_surface srf)
{
	PzWindow * ret;
	TWidget * wid;
	ret = pz_new_window(_("Save to..."), PZ_WINDOW_NORMAL);
	wid = ti_new_standard_text_widget(10, 40, ret->w-20, 10+ttk_text_height(ttk_textfont), 0, "/Images/untitled.bmp", podpaint_save_callback);
	ttk_add_widget(ret, wid);
	pz_show_window(pz_finish_window(ret));
	ti_widget_start(wid);
	return 0;
}

int podpaint_undo(TWidget * wid, ttk_surface srf)
{
	ttk_surface t = podpaint_area_undo;
	podpaint_area_undo = podpaint_area;
	podpaint_area = t;
	ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
	wid->dirty = 1;
	return 0;
}

int podpaint_clear(TWidget * wid, ttk_surface srf)
{
	int w, h;
	ttk_surface_get_dimen(srf, &w, &h);
	ttk_fillrect(srf, 0, 0, w, h, ttk_makecol(WHITE));
	return 1;
}

int podpaint_quit(TWidget * wid, ttk_surface srf)
{
	pz_close_window(wid->win);
	return 0;
}

int podpaint_switchaa(TWidget * wid, ttk_surface srf)
{
	podpaint_aamode = !podpaint_aamode;
	wid->dirty = 1;
	return 0;
}

int podpaint_switchfill(TWidget * wid, ttk_surface srf)
{
	podpaint_fillmode = !podpaint_fillmode;
	wid->dirty = 1;
	return 0;
}

/* Tool Visibility Functions */

/* Pod Paint Tool Visibility [[Anti] Aliased] [{Filled|Hollow}] */
static int PPTV_AA() { return podpaint_aamode; }
static int PPTV_A() { return !podpaint_aamode; }
static int PPTV_F() { return podpaint_fillmode; }
static int PPTV_H() { return !podpaint_fillmode; }
static int PPTV_AAF() { return podpaint_aamode && podpaint_fillmode; }
static int PPTV_AF() { return !podpaint_aamode && podpaint_fillmode; }
static int PPTV_AAH() { return podpaint_aamode && !podpaint_fillmode; }
static int PPTV_AH() { return !podpaint_aamode && !podpaint_fillmode; }

/* PodPaint Environment Initialization */

#define BIC(n) (ttk_get_cursor(n ".png"))
#define TLI(n) (ttk_load_image(pz_module_get_datapath(podpaint_module, n ".png")))
#define TSI(n) (ttk_load_image(pz_module_get_datapath(podpaint_module, n "i.png")))

void podpaint_free_ui()
{
	podpaint_free_menus();
	podpaint_free_tools();
	podpaint_free_colors();
	podpaint_inited = 0;
}

void podpaint_init_ui()
{
	podpaint_free_ui();
	
	/* add menus */
	/*
	 * We put spaces before and after all menu titles and menu item names,
	 * since the rendering routines do not space things out for us.
	 * It is more efficient to do it this way, I think.
	 * I believe GS/OS on the Apple IIgs did it this way too (at least
	 * for window titles).
	 */
	podpaint_menu * mFile = new_podpaint_menu(_(" File "));
	podpaint_add_menu(mFile);
	podpaint_add_menuitem(mFile, _(" Save... "), podpaint_save);
	podpaint_add_menuitem(mFile, _(" Quit "), podpaint_quit);
	podpaint_menu * mEdit = new_podpaint_menu(_(" Edit "));
	podpaint_add_menu(mEdit);
	podpaint_add_menuitem(mEdit, _(" Undo "), podpaint_undo);
	podpaint_add_menuitem(mEdit, _(" Clear "), podpaint_clear);
	podpaint_menu * mOptions = new_podpaint_menu(_(" Options "));
	podpaint_add_menu(mOptions);
	podpaint_add_menuitem(mOptions, _(" Anti-Aliased "), podpaint_switchaa);
	podpaint_add_menuitem(mOptions, _(" Filled "), podpaint_switchfill);
	
	/* add tools */
	podpaint_add_tool(new_podpaint_tool(
		TLI("eyedropper"), TSI("eyedropper"), BIC("eyedropper"),
		podpaint_eyedropper, 0, 0, 0, 0, 0, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("pencil"), TSI("pencil"), BIC("pencil"),
		ttk_pixel, 0, 0, 0, 0, 0, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("brush"), TSI("brush"), BIC("brush"),
		podpaint_paintbrush, 0, 0, 0, 0, 0, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("spraypaint"), TSI("spraypaint"), BIC("spraypaint"),
		podpaint_spraypaint, 0, 0, 0, 0, 0, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("fill"), TSI("fill"), BIC("bucket"),
		podpaint_fill, 0, 0, 0, 0, 0, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("eraser"), TSI("eraser"), BIC("eraser2"),
		podpaint_eraser, 0, 0, 0, 0, 0, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("line"), TSI("line"), BIC("cross"),
		0, ttk_line, ttk_line, 0, 0, PPTV_A, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("lineaa"), TSI("lineaa"), BIC("cross"),
		0, ttk_aaline, ttk_aaline, 0, 0, PPTV_AA, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("rect"), TSI("rect"), BIC("cross"),
		0, ttk_rect, ttk_rect, 0, 0, PPTV_H, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("rectf"), TSI("rectf"), BIC("cross"),
		0, ttk_fillrect, ttk_fillrect, 0, 0, PPTV_F, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("oval"), TSI("oval"), BIC("cross"),
		0, podpaint_ellipse, podpaint_ellipse, 0, 0, PPTV_AH, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("ovalf"), TSI("ovalf"), BIC("cross"),
		0, podpaint_fillellipse, podpaint_fillellipse, 0, 0, PPTV_AF, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("ovalaa"), TSI("ovalaa"), BIC("cross"),
		0, podpaint_aaellipse, podpaint_aaellipse, 0, 0, PPTV_AAH, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("ovalfaa"), TSI("ovalfaa"), BIC("cross"),
		0, podpaint_aafillellipse, podpaint_aafillellipse, 0, 0, PPTV_AAF, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("poly"), TSI("poly"), BIC("cross"),
		0, 0, 0, ttk_line, ttk_line, PPTV_AH, 1 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("polyaa"), TSI("polyaa"), BIC("cross"),
		0, 0, 0, ttk_aaline, ttk_aaline, PPTV_AAH, 1 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("polyf"), TSI("polyf"), BIC("cross"),
		podpaint_polystart, podpaint_poly, podpaint_polyclick, podpaint_poly, podpaint_polyclick, PPTV_AF, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("polyfaa"), TSI("polyfaa"), BIC("cross"),
		podpaint_polystart, podpaint_aapoly, podpaint_aapolyclick, podpaint_aapoly, podpaint_aapolyclick, PPTV_AAF, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("bezier"), TSI("bezier"), BIC("cross"),
		podpaint_bezierstart, podpaint_bezier, podpaint_bezierclick, podpaint_bezier, podpaint_bezierclick, PPTV_A, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("bezieraa"), TSI("bezieraa"), BIC("cross"),
		podpaint_bezierstart, podpaint_aabezier, podpaint_aabezierclick, podpaint_aabezier, podpaint_aabezierclick, PPTV_AA, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("text"), TSI("text"), BIC("ibeam"),
		podpaint_text_tool, 0, 0, 0, 0, 0, 0 ));
	podpaint_add_tool(new_podpaint_tool(
		TLI("hand"), TSI("hand"), BIC("hand"),
		podpaint_grabc, podpaint_grabd, podpaint_grabd, 0, 0, 0, 0 ));
	
	/* add colors */
	switch (ttk_screen->bpp) {
	case 1:
		podpaint_add_color(ttk_makecol(WHITE));
		podpaint_add_color(ttk_makecol(BLACK));
		break;
	case 2:
		podpaint_add_color(ttk_makecol(WHITE));
		podpaint_add_color(ttk_makecol(GREY));
		podpaint_add_color(ttk_makecol(DKGREY));
		podpaint_add_color(ttk_makecol(BLACK));
		break;
	case 4:
		podpaint_add_color(ttk_makecol(BLACK));
		podpaint_add_color(ttk_makecol(DKGREY));
		podpaint_add_color(ttk_makecol(128, 0,   0  ));
		podpaint_add_color(ttk_makecol(128, 128, 0  ));
		podpaint_add_color(ttk_makecol(0,   128, 0  ));
		podpaint_add_color(ttk_makecol(0,   128, 128));
		podpaint_add_color(ttk_makecol(0,   0,   128));
		podpaint_add_color(ttk_makecol(128, 0,   128));
		podpaint_add_color(ttk_makecol(WHITE));
		podpaint_add_color(ttk_makecol(GREY));
		podpaint_add_color(ttk_makecol(255, 0,   0  ));
		podpaint_add_color(ttk_makecol(255, 255, 0  ));
		podpaint_add_color(ttk_makecol(0,   255, 0  ));
		podpaint_add_color(ttk_makecol(0,   255, 255));
		podpaint_add_color(ttk_makecol(0,   0,   255));
		podpaint_add_color(ttk_makecol(255, 0,   255));
		break;
	default:
		podpaint_add_color(ttk_makecol(BLACK));
		podpaint_add_color(ttk_makecol(DKGREY));
		podpaint_add_color(ttk_makecol(128, 0,   0  ));
		podpaint_add_color(ttk_makecol(128, 128, 0  ));
		podpaint_add_color(ttk_makecol(0,   128, 0  ));
		podpaint_add_color(ttk_makecol(0,   128, 128));
		podpaint_add_color(ttk_makecol(0,   0,   128));
		podpaint_add_color(ttk_makecol(128, 0,   128));
		podpaint_add_color(ttk_makecol(128, 128, 64 ));
		podpaint_add_color(ttk_makecol(0,   64,  64 ));
		podpaint_add_color(ttk_makecol(0,   128, 255));
		podpaint_add_color(ttk_makecol(0,   64,  128));
		podpaint_add_color(ttk_makecol(64,  0,   255));
		podpaint_add_color(ttk_makecol(128, 64,  0  ));
		podpaint_add_color(ttk_makecol(255, 0,   26 ));
		podpaint_add_color(ttk_makecol(242, 101, 33 ));
		podpaint_add_color(ttk_makecol(171, 102, 56 ));
		podpaint_add_color(ttk_makecol(0,   191, 0  ));
		podpaint_add_color(ttk_makecol(153, 153, 255));
		podpaint_add_color(ttk_makecol(108, 0,   215));
		podpaint_add_color(ttk_makecol(255, 102, 204));
		podpaint_add_color(ttk_makecol(228, 198, 255));
		podpaint_add_color(ttk_makecol(199, 150, 105));
		podpaint_add_color(ttk_makecol(WHITE));
		podpaint_add_color(ttk_makecol(GREY));
		podpaint_add_color(ttk_makecol(255, 0,   0  ));
		podpaint_add_color(ttk_makecol(255, 255, 0  ));
		podpaint_add_color(ttk_makecol(0,   255, 0  ));
		podpaint_add_color(ttk_makecol(0,   255, 255));
		podpaint_add_color(ttk_makecol(0,   0,   255));
		podpaint_add_color(ttk_makecol(255, 0,   255));
		podpaint_add_color(ttk_makecol(255, 255, 128));
		podpaint_add_color(ttk_makecol(0,   255, 128));
		podpaint_add_color(ttk_makecol(128, 255, 255));
		podpaint_add_color(ttk_makecol(128, 128, 255));
		podpaint_add_color(ttk_makecol(255, 0,   128));
		podpaint_add_color(ttk_makecol(255, 128, 64 ));
		podpaint_add_color(ttk_makecol(255, 128, 128));
		podpaint_add_color(ttk_makecol(247, 148, 28 ));
		podpaint_add_color(ttk_makecol(255, 222, 0  ));
		podpaint_add_color(ttk_makecol(0,   255, 156));
		podpaint_add_color(ttk_makecol(204, 204, 255));
		podpaint_add_color(ttk_makecol(195, 126, 255));
		podpaint_add_color(ttk_makecol(255, 176, 242));
		podpaint_add_color(ttk_makecol(255, 213, 248));
		podpaint_add_color(ttk_makecol(255, 214, 170));
		/* This is about all we can add; if we add any more, the swatches will  */
		/* be too small on the nano (the smallest of the color-screened iPods). */
		break;
	}
	
	/* initialize some stuff */
	podpaint_inited = 1;
	podpaint_sel_tool = 0;
	podpaint_sel_menu = -1;
	podpaint_menubar_height = ttk_text_height(ttk_textfont)+1;
	podpaint_colorsy = (podpaint_colorcount<16)?1:2;
	podpaint_colorsx = ceildiv(podpaint_colorcount,podpaint_colorsy);
	podpaint_palette_height = 15;
	if (podpaint_window) {
		podpaint_toolbox_height = podpaint_window->h - podpaint_menubar_height - podpaint_palette_height;
		podpaint_toolsy = floordiv(podpaint_toolbox_height+1, 15);
		podpaint_toolsx = ceildiv(podpaint_vtoolcount, podpaint_toolsy);
		podpaint_toolbox_width = podpaint_toolsx*15 + 1;
	}
}

void podpaint_free_area()
{
	if (podpaint_area_made) {
		ttk_free_surface(podpaint_area);
		ttk_free_surface(podpaint_area_temp);
		ttk_free_surface(podpaint_area_undo);
		podpaint_area_made = 0;
	}
}

ttk_surface podpaint_duplicate_surface(ttk_surface srf)
{
	int w, h, bpp;
	ttk_surface nsrf;
	ttk_surface_get_dimen(srf, &w, &h);
	bpp = ttk_screen->bpp;
	nsrf = ttk_new_surface(w, h, bpp);
	ttk_blit_image(srf, nsrf, 0, 0);
	return nsrf;
}

void podpaint_init_area_from(ttk_surface srf)
{
	int w, h, bpp;
	podpaint_free_area();
	ttk_surface_get_dimen(srf, &w, &h);
	bpp = ttk_screen->bpp;
	podpaint_area = srf;
	podpaint_area_temp = podpaint_duplicate_surface(srf);
	podpaint_area_undo = podpaint_duplicate_surface(srf);
	podpaint_area_made = 1;
	podpaint_scrollx = 0;
	podpaint_scrolly = 0;
	if (podpaint_controller) {
		podpaint_maxscrollx = w - (podpaint_controller->w - podpaint_toolbox_width);
		podpaint_maxscrolly = h - (podpaint_toolbox_height);
	} else {
		podpaint_maxscrollx = w - (ttk_screen->w - podpaint_toolbox_width);
		podpaint_maxscrolly = h - (podpaint_toolbox_height);
	}
	if (podpaint_maxscrollx < 0) podpaint_maxscrollx = 0;
	if (podpaint_maxscrolly < 0) podpaint_maxscrolly = 0;
}

void podpaint_init_area(int w, int h, int bpp)
{
	podpaint_free_area();
	podpaint_area = ttk_new_surface(w,h,bpp);
	podpaint_area_temp = ttk_new_surface(w,h,bpp);
	podpaint_area_undo = ttk_new_surface(w,h,bpp);
	ttk_fillrect(podpaint_area, 0, 0, w, h, ttk_makecol(255,255,255));
	ttk_fillrect(podpaint_area_temp, 0, 0, w, h, ttk_makecol(255,255,255));
	ttk_fillrect(podpaint_area_undo, 0, 0, w, h, ttk_makecol(255,255,255));
	podpaint_area_made = 1;
	podpaint_scrollx = 0;
	podpaint_scrolly = 0;
	if (podpaint_controller) {
		podpaint_maxscrollx = w - (podpaint_controller->w - podpaint_toolbox_width);
		podpaint_maxscrolly = h - (podpaint_toolbox_height);
	} else {
		podpaint_maxscrollx = w - (ttk_screen->w - podpaint_toolbox_width);
		podpaint_maxscrolly = h - (podpaint_toolbox_height);
	}
	if (podpaint_maxscrollx < 0) podpaint_maxscrollx = 0;
	if (podpaint_maxscrolly < 0) podpaint_maxscrolly = 0;
}

/* Rendering Routines */

void podpaint_render_menu(TWidget * wid, ttk_surface srf, podpaint_menu * m)
{
	int i;
	int mx1 = wid->x + m->titlex1;
	int my1 = wid->y + podpaint_menubar_height - 1;
	int mx2 = mx1 + m->width + 2;
	int my2 = my1 + m->height + 2;
	ttk_fillrect(srf, wid->x+m->titlex1, wid->y, wid->x+m->titlex2, wid->y+podpaint_menubar_height-1, ttk_makecol(BLACK));
	ttk_text(srf, ttk_textfont, wid->x+m->titlex1, wid->y, ttk_makecol(WHITE), m->title);
	ttk_fillrect(srf, mx1, my1, mx2, my2, ttk_makecol(WHITE));
	ttk_rect(srf, mx1, my1, mx2, my2, ttk_makecol(BLACK));
	ttk_line(srf, mx1+2, my2, mx2, my2, ttk_makecol(BLACK));
	ttk_line(srf, mx2, my1, mx2, my2, ttk_makecol(BLACK));
	for (i=0; i<m->numitems; i++) {
		ttk_text(srf, ttk_textfont, mx1+1, my1+1+(i * ttk_text_height(ttk_textfont)), ttk_makecol(BLACK), m->items[i]);
	}
}

void podpaint_render_menubar(TWidget * wid, ttk_surface srf)
{
	int i;
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+podpaint_menubar_height, ttk_makecol(WHITE));
	ttk_line(srf, wid->x, wid->y+podpaint_menubar_height-1, wid->x+wid->w, wid->y+podpaint_menubar_height-1, ttk_makecol(BLACK));
	for (i=0; i<podpaint_menucount; i++) {
		ttk_text(srf, ttk_textfont, wid->x+podpaint_menubar[i]->titlex1, wid->y, ttk_makecol(BLACK), podpaint_menubar[i]->title);
	}
	if (podpaint_sel_menu >= 0) {
		podpaint_render_menu(wid, srf, podpaint_menubar[podpaint_sel_menu]);
	}
}

void podpaint_render_toolbox(TWidget * wid, ttk_surface srf)
{
	int by = wid->y + podpaint_menubar_height-1;
	int x = wid->x;
	int y = wid->y + podpaint_menubar_height-1;
	int i;
	ttk_fillrect(srf, x, y, x+podpaint_toolbox_width, y+podpaint_toolbox_height, ttk_makecol(WHITE));
	for (i=0; i<podpaint_toolcount; i++) {
		if (podpaint_toolbox[i]->visible) {
			if (!podpaint_toolbox[i]->visible()) {
				continue;
			}
		}
		if (i == podpaint_sel_tool) {
			ttk_blit_image(podpaint_toolbox[i]->selicon, srf, x, y);
		} else {
			ttk_blit_image(podpaint_toolbox[i]->icon, srf, x, y);
		}
		y += 15;
		if ((y-by+15) >= podpaint_toolbox_height) {
			y = by;
			x += 15;
		}
	}
	ttk_line(srf, wid->x + podpaint_toolbox_width-1, by, wid->x + podpaint_toolbox_width-1, by + podpaint_toolbox_height, ttk_makecol(BLACK));
}

void podpaint_render_palette(TWidget * wid, ttk_surface srf)
{
	int by = wid->y + wid->h - podpaint_palette_height + 1;
	int x, x2;
	int y, y2;
	int i;
	ttk_fillrect(srf, wid->x, by, wid->x+20, wid->y+wid->h, podpaint_sel_color);
	for (i=0; i<podpaint_colorcount; i++) {
		x = 20 + ((wid->w - 20) * (i % podpaint_colorsx)) / podpaint_colorsx;
		y = by + (podpaint_palette_height * (i / podpaint_colorsx)) / podpaint_colorsy;
		x2 = 20 + ((wid->w - 20) * (i % podpaint_colorsx + 1)) / podpaint_colorsx;
		y2 = by + (podpaint_palette_height * (i / podpaint_colorsx + 1)) / podpaint_colorsy;
		ttk_fillrect(srf, x, y, x2, y2, podpaint_palette[i]);
	}
	ttk_line(srf, wid->x, by-1, wid->x + wid->w, by-1, ttk_makecol(BLACK));
}

void podpaint_render(TWidget * wid, ttk_surface srf)
{
	ttk_fillrect(srf, wid->x, wid->y, wid->x+wid->w, wid->y+wid->h, ttk_makecol(WHITE));
	ttk_blit_image(podpaint_area_temp, srf, wid->x+podpaint_toolbox_width-podpaint_scrollx, wid->y+podpaint_menubar_height-podpaint_scrolly);
	podpaint_render_toolbox(wid, srf);
	podpaint_render_palette(wid, srf);
	podpaint_render_menubar(wid, srf);
}

/* Click Routines */

void podpaint_change_cursor(ttk_surface c)
{
	((ttk_mousedata *)podpaint_controller->data)->cursor = c;
}

int podpaint_input(TWidget * wid, int ch)
{
	int w, h;
	ttk_surface_get_dimen(podpaint_area, &w, &h);
	if (ch == TTK_INPUT_END) {
		ttk_blit_image(podpaint_area, podpaint_area_undo, 0, 0);
		ti_multiline_text(podpaint_area, ttk_textfont, podpaint_text_x, podpaint_text_y, w-podpaint_text_x, h-podpaint_text_y, podpaint_text_col,
			podpaint_text->text, -1, 0, 0, 0, 0);
		ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
		wid->dirty = 1;
		ti_destroy_buffer(podpaint_text);
		podpaint_text = 0;
		ttk_input_end();
	} else {
		ti_buffer_input(podpaint_text, ch);
		ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
		ti_multiline_text(podpaint_area_temp, ttk_textfont, podpaint_text_x, podpaint_text_y, w-podpaint_text_x, h-podpaint_text_y, podpaint_text_col,
			podpaint_text->text, podpaint_text->cpos, 0, 0, 0, 0);
		wid->dirty = 1;
	}
	return TTK_EV_CLICK;
}

void podpaint_mmove(TWidget * wid, int x, int y)
{
	if (podpaint_dragging) {
		podpaint_tool * t = podpaint_toolbox[podpaint_sel_tool];
		int rx = x - podpaint_toolbox_width + podpaint_scrollx;
		int ry = y - podpaint_menubar_height + podpaint_scrolly;
		if (podpaint_sel_menu >= 0) { podpaint_sel_menu = -1; wid->dirty = 1; }
		ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
		switch (podpaint_dragging) {
		case 1:
			if (t->drag) {
				t->drag(podpaint_area_temp, podpaint_last_x, podpaint_last_y, rx, ry, podpaint_sel_color);
				return;
			}
		case 2:
			if (t->dragContinue) {
				t->dragContinue(podpaint_area_temp, podpaint_last_x, podpaint_last_y, rx, ry, podpaint_sel_color);
			}
		}
		wid->dirty = 1;
	} else if (y < podpaint_menubar_height || podpaint_sel_menu >= 0 || x < podpaint_toolbox_width || y > (wid->h - podpaint_palette_height)) {
		if (podpaint_cursorstate) {
			podpaint_change_cursor(podpaint_arrowcur);
			podpaint_cursorstate = 0;
		}
	} else {
		if (!podpaint_cursorstate) {
			podpaint_change_cursor(podpaint_toolbox[podpaint_sel_tool]->cursor);
			podpaint_cursorstate = 1;
		}
	}
}

void podpaint_click(TWidget * wid, int x, int y)
{
	int i;
	if (podpaint_dragging) {
		/* in the middle of dragging a tool */
		podpaint_tool * t = podpaint_toolbox[podpaint_sel_tool];
		int rx = x - podpaint_toolbox_width + podpaint_scrollx;
		int ry = y - podpaint_menubar_height + podpaint_scrolly;
		if (podpaint_sel_menu >= 0) { podpaint_sel_menu = -1; wid->dirty = 1; }
		if (podpaint_dragging > 1 && podpaint_last_x == rx && podpaint_last_y == ry && t->autoClose) {
			podpaint_dragging = 0;
			return;
		}
		ttk_blit_image(podpaint_area, podpaint_area_undo, 0, 0);
		switch (podpaint_dragging) {
		case 1:
			if (t->dragClick) {
				t->dragClick(podpaint_area, podpaint_last_x, podpaint_last_y, rx, ry, podpaint_sel_color);
				ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
				wid->dirty = 1;
				if (podpaint_dragging && (t->dragContinue || t->dragClickContinue)) {
					podpaint_last_x = rx;
					podpaint_last_y = ry;
					podpaint_dragging = 2;
				} else {
					podpaint_dragging = 0;
				}
				return;
			} else if (t->drag) {
				t->drag(podpaint_area, podpaint_last_x, podpaint_last_y, rx, ry, podpaint_sel_color);
				ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
				wid->dirty = 1;
				if (podpaint_dragging && (t->dragContinue || t->dragClickContinue)) {
					podpaint_last_x = rx;
					podpaint_last_y = ry;
					podpaint_dragging = 2;
				} else {
					podpaint_dragging = 0;
				}
				return;
			}
		case 2:
			if (t->dragClickContinue) {
				t->dragClickContinue(podpaint_area, podpaint_last_x, podpaint_last_y, rx, ry, podpaint_sel_color);
			} else if (t->dragContinue) {
				t->dragContinue(podpaint_area, podpaint_last_x, podpaint_last_y, rx, ry, podpaint_sel_color);
			}
		}
		ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
		wid->dirty = 1;
		if (podpaint_dragging && (t->dragContinue || t->dragClickContinue)) {
			podpaint_last_x = rx;
			podpaint_last_y = ry;
			podpaint_dragging = 2;
		} else {
			podpaint_dragging = 0;
		}
	} else if (y < podpaint_menubar_height) {
		/* click on the menubar */
		podpaint_sel_menu = -1;
		for (i=0; i<podpaint_menucount; i++) {
			if (x >= podpaint_menubar[i]->titlex1 && x < podpaint_menubar[i]->titlex2) {
				podpaint_sel_menu = i;
				break;
			}
		}
		wid->dirty = 1;
	} else if (podpaint_sel_menu >= 0
		&& x >= podpaint_menubar[podpaint_sel_menu]->titlex1
		&& x <= (podpaint_menubar[podpaint_sel_menu]->titlex1 + podpaint_menubar[podpaint_sel_menu]->width)
		&& y < (podpaint_menubar_height + podpaint_menubar[podpaint_sel_menu]->height)
	) {
		/* click on a menu item */
		int mi = (y - podpaint_menubar_height)/(ttk_text_height(ttk_textfont));
		int (* act)(TWidget *, ttk_surface srf);
		while (mi >= podpaint_menubar[podpaint_sel_menu]->numitems) mi--;
		while (mi < 0) mi++;
		act = podpaint_menubar[podpaint_sel_menu]->actions[mi];
		podpaint_sel_menu = -1;
		wid->dirty = 1;
		if (act) {
			if (act(wid, podpaint_area)) {
				ttk_blit_image(podpaint_area_temp, podpaint_area_undo, 0, 0);
				ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
				wid->dirty = 1;
			}
		}
	} else if (y > (wid->h - podpaint_palette_height)) {
		/* click on the color palette */
		if (x >= 20) {
			int ci = ((x-20) * podpaint_colorsx / (wid->w-20)) + podpaint_colorsx * ((y - podpaint_menubar_height - podpaint_toolbox_height - 1) * podpaint_colorsy / (podpaint_palette_height-1));
			if (podpaint_sel_menu >= 0) { podpaint_sel_menu = -1; wid->dirty = 1; }
			if (ci >= 0 && ci < podpaint_colorcount) {
				podpaint_sel_color = podpaint_palette[ci];
				wid->dirty = 1;
			}
		} else {
			if (podpaint_colorpicker) {
				podpaint_colorpicker(_("Paint Color"), &podpaint_sel_color);
				wid->dirty = 1;
			}
		}
	} else if (x < podpaint_toolbox_width) {
		/* click on the toolbox */
		int i, j;
		int ti = (x / 15) * podpaint_toolsy + ((y-podpaint_menubar_height) / 15);
		if (podpaint_sel_menu >= 0) { podpaint_sel_menu = -1; wid->dirty = 1; }
		for (i=0, j=0; i<podpaint_toolcount; i++) {
			if (podpaint_toolbox[i]->visible) {
				if (!podpaint_toolbox[i]->visible()) {
					continue;
				}
			}
			if (j == ti) {
				podpaint_sel_tool = i;
				wid->dirty = 1;
				break;
			}
			j++;
		}
	} else {
		/* click on the image area */
		podpaint_tool * t = podpaint_toolbox[podpaint_sel_tool];
		int rx = x - podpaint_toolbox_width + podpaint_scrollx;
		int ry = y - podpaint_menubar_height + podpaint_scrolly;
		if (podpaint_sel_menu >= 0) { podpaint_sel_menu = -1; wid->dirty = 1; }
		if (t->click) {
			ttk_blit_image(podpaint_area, podpaint_area_undo, 0, 0);
			t->click(podpaint_area, rx, ry, podpaint_sel_color);
			ttk_blit_image(podpaint_area, podpaint_area_temp, 0, 0);
			wid->dirty = 1;
		}
		if (t->drag || t->dragClick || t->dragContinue || t->dragClickContinue) {
			podpaint_last_x = rx;
			podpaint_last_y = ry;
			podpaint_dragging = 1;
		}
	}
}

/* Master Initialization */

void podpaint_free()
{
	podpaint_free_ui();
	podpaint_free_area();
	ttk_free_surface(podpaint_arrowcur);
}

PzWindow * new_podpaint_window_with_file(char * fn)
{
	PzWindow * win = pz_new_window(_("PodPaint"), PZ_WINDOW_NORMAL);
	TWidget * wid = ttk_new_mouse_widget_with(0, 0, win->w, win->h, podpaint_arrowcur, podpaint_render, 0, 0, podpaint_click, podpaint_mmove);
	podpaint_window = win;
	podpaint_controller = wid;
	
	if (!podpaint_inited) podpaint_init_ui();
	podpaint_sel_menu = -1;
	podpaint_sel_tool = 0;
	podpaint_sel_color = ttk_makecol(BLACK);
	podpaint_last_x = 0;
	podpaint_last_y = 0;
	podpaint_dragging = 0;
	podpaint_cursorstate = 0;
	podpaint_fillmode = 0;
	podpaint_aamode = 0;
	podpaint_init_area_from(ttk_load_image(fn));
	
	wid->input = podpaint_input;
	
	ttk_add_widget(win, wid);
	return pz_finish_window(win);
}

PzWindow * new_podpaint_window()
{
	PzWindow * win = pz_new_window(_("PodPaint"), PZ_WINDOW_NORMAL);
	TWidget * wid = ttk_new_mouse_widget_with(0, 0, win->w, win->h, podpaint_arrowcur, podpaint_render, 0, 0, podpaint_click, podpaint_mmove);
	podpaint_window = win;
	podpaint_controller = wid;
	
	if (!podpaint_inited) podpaint_init_ui();
	podpaint_sel_menu = -1;
	podpaint_sel_tool = 0;
	podpaint_sel_color = ttk_makecol(BLACK);
	podpaint_last_x = 0;
	podpaint_last_y = 0;
	podpaint_dragging = 0;
	podpaint_cursorstate = 0;
	podpaint_fillmode = 0;
	podpaint_aamode = 0;
	podpaint_init_area(320, 240, ttk_screen->bpp); /* 16 bpp doesn't blit well to a 2bpp screen ;) */
	
	wid->input = podpaint_input;
	
	ttk_add_widget(win, wid);
	return pz_finish_window(win);
}

int podpaint_openable(const char * filename)
{
	char ext[4];
	struct stat st;
	stat(filename, &st);
	if (S_ISDIR(st.st_mode)) return 0;
	strncpy(ext, &filename[strlen(filename)-4], 4);
	if (ext[0] != '.') return 0;
	if (ISEXT(ext, 'P', 'N', 'G')) return 1;
	if (ISEXT(ext, 'J', 'P', 'G')) return 1;
	if (ISEXT(ext, 'G', 'I', 'F')) return 1;
	if (ISEXT(ext, 'B', 'M', 'P')) return 1;
	if (ISEXT(ext, 'P', 'P', 'M')) return 1;
	if (ISEXT(ext, 'P', 'G', 'M')) return 1;
	if (ISEXT(ext, 'P', 'B', 'M')) return 1;
	if (ISEXT(ext, 'P', 'N', 'M')) return 1;
	return 0;
}

PzWindow * podpaint_open_handler(ttk_menu_item * item)
{
	return new_podpaint_window_with_file(item->data);
}

void podpaint_mod_init()
{
	podpaint_module = pz_register_module("podpaint", podpaint_free);
	podpaint_arrowcur = ttk_get_cursor("arrow.png");
	pz_menu_add_action("/Extras/Utilities/PodPaint", new_podpaint_window);
	
	podpaint_fbx.name = _("Open with PodPaint");
	podpaint_fbx.makesub = podpaint_open_handler;
	pz_browser_add_action (podpaint_openable, &podpaint_fbx);
	
	podpaint_colorpicker = pz_module_softdep("colorpicker", "pz_colorpicker");
}

PZ_MOD_INIT(podpaint_mod_init)
