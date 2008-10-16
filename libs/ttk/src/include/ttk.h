/*
 * Copyright (c) 2005 Joshua Oreman
 *
 * This file is a part of TTK.
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

#ifndef _TTK_H_
#define _TTK_H_

#define TTK_POD_X11       0
#define TTK_POD_1G        01
#define TTK_POD_2G        02
#define TTK_POD_3G        04
#define TTK_POD_PP5002    07
#define TTK_POD_4G        010
#define TTK_POD_MINI_1G   020
#define TTK_POD_PHOTO     040
#define TTK_POD_PP5020    070
#define TTK_POD_MINI_2G   0100
#define TTK_POD_MINI      0120
#define TTK_POD_NANO      0200
#define TTK_POD_VIDEO     0400
#define TTK_POD_PP5022    0700 /* also includes 5021 */
#define TTK_POD_SANSA     01000
#define TTK_POD_PP5024    07000
#define TTK_POD_PP502X    07770

#define TTK_BUTTON_ACTION   '\n'
#define TTK_BUTTON_PREVIOUS 'w'
#define TTK_BUTTON_NEXT     'f'
#define TTK_BUTTON_MENU     'm'
#define TTK_BUTTON_PLAY     'd'
#define TTK_BUTTON_HOLD     'h'

#define TTK_MOVE_ABS      0
#define TTK_MOVE_REL      1
#define TTK_MOVE_END      2

#define TTK_DIRTY_HEADER     1
#define TTK_DIRTY_WINDOWAREA 2
#define TTK_DIRTY_SCREEN     4
#define TTK_DIRTY_INPUT      8
#define TTK_FILTHY           15
extern int ttk_dirty;

#define TTK_INPUT_ENTER  '\r'
#define TTK_INPUT_BKSP   '\b'
#define TTK_INPUT_LEFT   '\1'
#define TTK_INPUT_RIGHT  '\2'
#define TTK_INPUT_END    '\0'

struct TWidget;
struct TWindow;
struct TWidgetList;

#define TTK_NO_BACKGROUND -1

typedef struct _ttk_timer
{
    int started;
    int delay;
    void (*fn)();
    struct _ttk_timer *next;
} *ttk_timer;

typedef unsigned short uc16;
#ifdef SDL
#include "SDL.h"
#include "SDL_image.h"
#include "ttk/SDL_gfxPrimitives.h"
#include "ttk/SDL_rotozoom.h"
typedef Uint32 ttk_color;
#ifndef NO_TF
#include "SDL_ttf.h"
#endif
#ifndef NO_SF
#include "SFont.h"
#endif
struct Bitmap_Font;
struct _TTF_Font;
typedef SDL_Surface *ttk_surface;
typedef struct _ttk_font {
#ifndef NO_SF
    SFont_Font *sf; 
    SFont_Font *sfi; // inverted
#endif
    struct Bitmap_Font *bf;
#ifndef NO_TF
    struct _TTF_Font *tf;
#endif
    void (*draw)(struct _ttk_font *, ttk_surface, int, int, ttk_color, const char *);
    void (*draw_lat1)(struct _ttk_font *, ttk_surface, int, int, ttk_color, const char *);
    void (*draw_uc16)(struct _ttk_font *, ttk_surface, int, int, ttk_color, const uc16 *);
    int (*width)(struct _ttk_font *, const char *);
    int (*width_lat1)(struct _ttk_font *, const char *);
    int (*width_uc16)(struct _ttk_font *, const uc16 *);
    void (*free)(struct _ttk_font *);
    int height;
    int ofs;
    struct ttk_fontinfo *fi;
} * ttk_font;
typedef struct ttk_point { int x, y; } ttk_point;
typedef struct _ttk_gc
{
	int fg, bg, usebg, xormode;
	ttk_font font;
} *ttk_gc;
#elif defined(MWIN)
#include "nano-X.h"
typedef unsigned int TTK_ID;
typedef GR_COLOR    ttk_color;
typedef GR_POINT    ttk_point;
typedef TTK_ID      ttk_gc;
typedef TTK_ID      ttk_surface;
typedef struct _ttk_font
{
    GR_FONT_ID f;
    GR_FONT_INFO inf;
    int ofs;
    struct ttk_fontinfo *fi;
} * ttk_font;
#else /* Hotdog */
#include "hotdog.h"
typedef uint32 ttk_color;
typedef hd_surface ttk_surface;
typedef hd_point ttk_point;
typedef struct _ttk_font 
{
    hd_font *f;
    int ofs;
    struct ttk_fontinfo *fi;
} * ttk_font;
typedef struct _ttk_gc {
    ttk_color fg, bg;
    int usebg, xormode;
    ttk_font font;
} *ttk_gc;
#endif

typedef struct TWidgetList 
{
    struct TWidget *v;
    struct TWidgetList *next;
} TWidgetList;

struct TApItem;

typedef struct TWindow
{
    const char *title;
    TWidgetList *widgets;
    int x, y, w, h, color;
    ttk_surface srf;
    int titlefree;
    int dirty;
    struct TApItem *background;
    /* readonly */ struct TWidget *focus;
    /* private */ struct TWidget *input;
    /* private */ int show_header;
    /* private */ int epoch;
    /* private */ int inbuf[32]; // circular buffer
    /* private */ int inbuf_start, inbuf_end;
    /* private */ int onscreen;
    int data;
    void *data2;
} TWindow;

typedef struct TWindowStack {
    TWindow *w;
    int minimized;
    struct TWindowStack *next;
} TWindowStack;

#define TTK_EV_CLICK   1
#define TTK_EV_UNUSED  2
#define TTK_EV_DONE    4
#define TTK_EV_RET(x)  ((x)<<8)

/**TWidget**/
typedef struct TWidget
{
    int x, y, w, h;
    int focusable;
    int dirty;
    int holdtime;
    int keyrepeat;
    int rawkeys;
    /* readonly */ TWindow *win;
    /* private */ int framelast, framedelay;
    /* private */ int timerlast, timerdelay;
    
    void (*draw) (struct TWidget *, ttk_surface);
    int (*scroll) (struct TWidget *, int);
    int (*stap) (struct TWidget *, int);
    int (*button) (struct TWidget *, int, int);
    int (*down) (struct TWidget *, int);
    int (*held) (struct TWidget *, int);
    int (*input) (struct TWidget *, int);
    int (*frame) (struct TWidget *);
    int (*timer) (struct TWidget *);
    void (*destroy) (struct TWidget *);

    void *data;
    void *data2;
} TWidget;

typedef struct ttk_fontinfo {
    char file[64];   /* font file, relative to /usr/share/fonts on iPod, fonts/ on X11 */
    char name[64];   /* font name */
    int size;        /* font size (px) */
    ttk_font f;      /* the font itself */
    int loaded;
    int good;
    int offset;
    int refs;
    struct ttk_fontinfo *next;
} ttk_fontinfo;

typedef struct ttk_screeninfo {
    int w, h, bpp;
    int wx, wy;
    ttk_surface srf;
} ttk_screeninfo;

//--//

extern TWindowStack *ttk_windows;
extern ttk_font ttk_menufont, ttk_textfont;
extern ttk_screeninfo *ttk_screen;
extern ttk_fontinfo *ttk_fonts;
extern int ttk_epoch;

//--//

int ttk_version_check (int yourver);

TWindow *ttk_init();
int ttk_run();
void ttk_quit();
int ttk_get_podversion();
void ttk_get_screensize (int *w, int *h, int *bpp);
void ttk_set_emulation (int w, int h, int bpp);
void ttk_click_ex(int period, int duration); // the "click" sound
void ttk_click(); // the "click" sound

TWindow *ttk_new_window();
void ttk_free_window (TWindow *win);
void ttk_show_window (TWindow *win);
void ttk_draw_window (TWindow *win);
void ttk_popup_window (TWindow *win);
void ttk_move_window (TWindow *win, int offset, int whence);
int ttk_hide_window (TWindow *win);
void ttk_set_popup (TWindow *win);
#define ttk_popdown_window(w) ttk_hide_window(w)
void ttk_window_title (TWindow *win, const char *str);
#define ttk_window_set_title(w,s) ttk_window_title(w,s)
void ttk_window_show_header (TWindow *win);
void ttk_window_hide_header (TWindow *win);

enum ttk_justification {
        TTK_TEXT_CENTER = 0,
        TTK_TEXT_LEFT,
        TTK_TEXT_RIGHT
};
void ttk_header_set_text_position( int x );
void ttk_header_set_text_justification( enum ttk_justification j );

TWidget *ttk_new_widget (int x, int y);
void ttk_free_widget (TWidget *wid);
TWindow *ttk_add_widget (TWindow *win, TWidget *wid); // returns win
int ttk_remove_widget (TWindow *win, TWidget *wid);
void ttk_widget_set_fps (TWidget *wid, int fps);
void ttk_widget_set_inv_fps (TWidget *wid, int fps_m1);
void ttk_widget_set_timer (TWidget *wid, int ms);

void ttk_add_header_widget (TWidget *wid);
void ttk_remove_header_widget (TWidget *wid);

void ttk_set_global_event_handler (int (*fn)(int, int, int));
void ttk_set_global_unused_handler (int (*fn)(int, int, int));
int ttk_button_pressed (int button);

void ttk_update_lcd (int xstart, int ystart, int xend, int yend, unsigned char *data);
void ttk_start_cop (void (*fn)());
void ttk_stop_cop();
void ttk_sleep_cop();
void ttk_wake_cop();

ttk_timer ttk_create_timer (int ms, void (*fn)());
void ttk_destroy_timer (ttk_timer tim);

void ttk_set_transition_frames (int frames);
void ttk_set_clicker (void (*fn)());
void ttk_set_scroll_multiplier (int num, int denom);

int ttk_input_start_for (TWindow *win, TWidget *inmethod);
void ttk_input_move_for (TWindow *win, int x, int y);
void ttk_input_size_for (TWindow *win, int *w, int *h);

int ttk_input_start (TWidget *inmethod);
void ttk_input_move (int x, int y);
void ttk_input_size (int *w, int *h);
void ttk_input_char (int ch);
void ttk_input_end();

// -- Implemented by GFX driver --

#if !defined(SDL) && !defined(MWIN) /* Hotdog */
#define TTK_ACOLOR(r,g,b,a) HD_RGBA(r,g,b,a)
#define TTK_COLOR(r,g,b) HD_RGB(r,g,b)
#else
#define TTK_ACOLOR(r,g,b,a) ttk_makecol(r,g,b)
#define TTK_COLOR(r,g,b) ttk_makecol(r,g,b)
#endif

ttk_color ttk_makecol (int r, int g, int b);
ttk_color ttk_makecol_ex (int r, int g, int b, ttk_surface srf);
void ttk_unmakecol (ttk_color col, int *r, int *g, int *b);
void ttk_unmakecol_ex (ttk_color col, int *r, int *g, int *b, ttk_surface srf);

/* Implemented by TTK core: */ ttk_color ttk_mapcol (ttk_color col, ttk_surface src, ttk_surface dst);

ttk_gc ttk_new_gc();
ttk_gc ttk_copy_gc (ttk_gc other);
ttk_color ttk_gc_get_foreground (ttk_gc gc);
ttk_color ttk_gc_get_background (ttk_gc gc);
ttk_font ttk_gc_get_font (ttk_gc gc);
void ttk_gc_set_foreground (ttk_gc gc, ttk_color fgcol);
void ttk_gc_set_background (ttk_gc gc, ttk_color bgcol);
void ttk_gc_set_font (ttk_gc gc, ttk_font font);
void ttk_gc_set_usebg (ttk_gc gc, int flag);
void ttk_gc_set_xormode (ttk_gc gc, int flag);
void ttk_free_gc (ttk_gc gc);

ttk_color ttk_getpixel (ttk_surface srf, int x, int y);
void ttk_pixel (ttk_surface srf, int x, int y, ttk_color col);
void ttk_pixel_gc (ttk_surface srf, ttk_gc gc, int x, int y);

void ttk_line (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col);
void ttk_line_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2);
void ttk_aaline (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col);
void ttk_aaline_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2);

void ttk_rect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col);
void ttk_rect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h);
void ttk_fillrect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col);
void ttk_fillrect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h);
void ttk_hgradient( ttk_surface srf, int x1, int y1, int x2, int y2,
                        ttk_color left, ttk_color right );
void ttk_vgradient(ttk_surface srf, int x1, int y1, int x2, int y2,
                        ttk_color top, ttk_color bottom );

void ttk_poly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col);
void ttk_poly_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col);
void ttk_poly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v);
void ttk_aapoly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col);
void ttk_aapoly_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col);
void ttk_aapoly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v);
void ttk_fillpoly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col);
void ttk_fillpoly_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col);
void ttk_fillpoly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v);
#if !defined(SDL) && !defined(MWIN) /* Hotdog */
void ttk_polyline (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col);
void ttk_polyline_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col);
void ttk_polyline_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v);
void ttk_aapolyline (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col);
void ttk_aapolyline_pt (ttk_surface srf, ttk_point *v, int n, ttk_color col);
void ttk_aapolyline_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v);
#endif

void ttk_ellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col);
void ttk_ellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry);
void ttk_aaellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col);
void ttk_aaellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry);
void ttk_fillellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col);
void ttk_fillellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry);
void ttk_aafillellipse (ttk_surface srf, int xc, int yc, int rx, int ry, ttk_color col);
void ttk_aafillellipse_gc (ttk_surface srf, ttk_gc gc, int xc, int yc, int rx, int ry);
void ttk_bezier (ttk_surface srf, int x1, int y1, int x2, int y2,
		int x3, int y3, int x4, int y4, int level, ttk_color col);
void ttk_bezier_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2,
		int x3, int y3, int x4, int y4, int level);
void ttk_aabezier (ttk_surface srf, int x1, int y1, int x2, int y2,
		int x3, int y3, int x4, int y4, int level, ttk_color col);
void ttk_aabezier_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2,
		int x3, int y3, int x4, int y4, int level);

void ttk_bitmap (ttk_surface srf, int x, int y, int w, int h, unsigned short *data, ttk_color col);
void ttk_bitmap_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h, unsigned short *data);

// Font funcs. Implemented in core TTK:
ttk_fontinfo *ttk_get_fontinfo (const char *name, int size);
ttk_font ttk_get_font (const char *name, int size);
void ttk_done_fontinfo (ttk_fontinfo *fi);
void ttk_done_font (ttk_font f);
// GFX driver:
void ttk_text (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *str); /* UTF-8 */
void ttk_textf (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *fmt, ...);
void ttk_text_lat1 (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *str);
void ttk_text_uc16 (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const uc16 *str);
void ttk_text_gc (ttk_surface srf, ttk_gc gc, int x, int y, const char *str);
int ttk_text_width (ttk_font fnt, const char *str); /* UTF-8 */
int ttk_text_width_lat1 (ttk_font fnt, const char *str);
int ttk_text_width_uc16 (ttk_font fnt, const uc16 *str);
int ttk_text_width_gc (ttk_gc gc, const char *str);
int ttk_text_height (ttk_font fnt);
int ttk_text_height_gc (ttk_gc gc);

ttk_surface ttk_load_image (const char *path);
void ttk_free_image (ttk_surface img);
void ttk_blit_image (ttk_surface src, ttk_surface dst, int dx, int dy);
void ttk_blit_image_ex (ttk_surface src, int sx, int sy, int sw, int sh,
                               ttk_surface dst, int dx, int dy);

ttk_surface ttk_new_surface (int w, int h, int bpp);
ttk_surface ttk_scale_surface (ttk_surface srf, float factor);
void ttk_surface_get_dimen (ttk_surface srf, int *w, int *h);
void ttk_free_surface (ttk_surface srf);

#ifdef IPOD
#define TTK_SCROLLMOD(dir,n) \
  do { \
    static int sofar = 0; \
    sofar += dir; \
    if (sofar > -n && sofar < n) return 0; \
    dir = sofar / n; \
    sofar -= dir * n; \
  } while (0)
#else
#define TTK_SCROLLMOD(dir,n)
#endif

// -- Colors --
#undef WHITE
#undef GREY
#undef DKGREY
#undef BLACK

#define  WHITE   255, 255, 255
#define   GREY   160, 160, 160
#define DKGREY    80,  80,  80
#define  BLACK     0,   0,   0
#ifdef SDL
#define   CKEY   255,   0, 255
#elif defined(MWIN)
#define   CKEY   255, 255, 255
#else /* Hotdog */
#define   CKEY   -1, -1, 0x00000000 /* zero-alpha */
#endif

#undef MIN
#undef MAX
#define MIN(x,y)	(((x)<(y))?(x):(y))
#define MAX(x,y)	(((x)>(y))?(x):(y))

/* Steve Brown's scroll wheel acceleration macro, version 2-poit-oh
 * Amplifies scroll events with a timeout
 * arg1: (variable) event to be amplified
 * arg2: (constant int) slow accel by this factor
 * arg3: (constant int) maximum acceleration factor
 */
	/* TODO: these timers need to be fine-tuned on a real iPod */
#define TTK_ACCEL_L	150 /* events below this delay are accel'ed */
#define TTK_ACCEL_H	250 /* events between the two are decel'ed */
#define TTK_SCROLL_ACCEL(ttk_accel_event, ttk_accel_slow, ttk_accel_max)		\
do { 			\
	static int ttk_accel_throttle = 0;			\
	static int ttk_accel_timer = 0;			\
	static int ttk_accel_now = 0;			\
	ttk_accel_now = ttk_getticks() - ttk_accel_timer;			\
	if(ttk_accel_now > TTK_ACCEL_H || ttk_accel_throttle * ttk_accel_event < 0) {			\
		ttk_accel_throttle = 0; /* user changed direction or timed out; stop accel */			\
	} else if(ttk_accel_now > TTK_ACCEL_L && abs(ttk_accel_throttle) > abs(ttk_accel_event)) {/* slow the accel */			\
		ttk_accel_throttle -= ttk_accel_event;			\
	} else { /* increase accel coefficient up to max */			\
		ttk_accel_throttle += ttk_accel_event; /* count can be negative, meaning backwards direction*/			\
	}			\
	ttk_accel_event *= MIN(abs(ttk_accel_throttle / ttk_accel_slow) + 1, ttk_accel_max);			\
	ttk_accel_timer += ttk_accel_now;			\
} while (0)

// -- Private --
#define TTK_NO_EVENT     0
#define TTK_BUTTON_DOWN  1
#define TTK_BUTTON_UP    2
#define TTK_SCROLL       3
#define TTK_TOUCH        4
#define TTK_LIFT         5
#define TTK_TAP          6

void ttk_gfx_init();
void ttk_gfx_update (ttk_surface srf);
void ttk_load_font (ttk_fontinfo *fi, const char *fname, int size);
int ttk_get_event (int *arg); /* ret=>ev code, see above; arg=>button pressed */
int ttk_get_rawevent (int *arg);
int ttk_getticks();
void ttk_delay (int ms);

#include "appearance.h"

#endif
