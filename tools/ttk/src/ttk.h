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
#define TTK_POD_PP5022    0700
#define TTK_POD_PP502X    0770

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

#ifdef SDL
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_gfxPrimitives.h"
#include "SDL_rotozoom.h"
typedef Uint32 ttk_color;
#ifndef NO_TF
#include "SDL_ttf.h"
#endif
#ifndef NO_SF
#include "SFont.h"
#endif
struct SFont_Font;
struct Bitmap_Font;
struct _TTF_Font;
typedef SDL_Surface *ttk_surface;
typedef struct _ttk_font {
#ifndef NO_SF
    struct SFont_Font *sf; 
    struct SFont_Font *sfi; // inverted
#endif
    struct Bitmap_Font *bf;
#ifndef NO_TF
    struct _TTF_Font *tf;
#endif
    void (*draw)(struct _ttk_font *, ttk_surface, int, int, ttk_color, const char *);
    int (*width)(struct _ttk_font *, const char *);
    void (*free)(struct _ttk_font *);
    int height;
    int ofs;
    struct ttk_fontinfo *fi;
} ttk_font;
typedef struct ttk_point { int x, y; } ttk_point;
typedef struct _ttk_gc { int fg, bg, usebg, xormode; ttk_font font; } *ttk_gc;
#else
#define MWINCLUDECOLORS
#include "nano-X.h"
typedef unsigned int TTK_ID;
typedef GR_COLOR    ttk_color;
typedef GR_POINT    ttk_point;
typedef TTK_ID      ttk_gc;
typedef TTK_ID      ttk_surface;
typedef struct 
{
    GR_FONT_ID f;
    GR_FONT_INFO inf;
    int ofs;
    struct ttk_fontinfo *fi;
} ttk_font;
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
    /* private */ char inbuf[32]; // circular buffer
    /* private */ int inbuf_start, inbuf_end;
    /* private */ int onscreen;
    int data;
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

TWindow *ttk_init();
int ttk_run();
void ttk_quit();
int ttk_get_podversion();
void ttk_get_screensize (int *w, int *h, int *bpp);
void ttk_click(); // the "click" sound
void ttk_set_emulation (int w, int h, int bpp);

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

// -- Implemented by GFX driver --

ttk_color ttk_makecol (int r, int g, int b);
ttk_color ttk_makecol_ex (int r, int g, int b, ttk_surface srf);
void ttk_unmakecol (ttk_color col, int *r, int *g, int *b);
void ttk_unmakecol_ex (ttk_color col, int *r, int *g, int *b, ttk_surface srf);

/* Implemented by TTK core: */ ttk_color ttk_mapcol (ttk_color col, ttk_surface src, ttk_surface dst);

ttk_gc ttk_new_gc();
ttk_gc ttk_copy_gc (ttk_gc other);
void ttk_gc_set_foreground (ttk_gc gc, ttk_color fgcol);
void ttk_gc_set_background (ttk_gc gc, ttk_color bgcol);
void ttk_gc_set_font (ttk_gc gc, ttk_font font);
void ttk_gc_set_usebg (ttk_gc gc, int flag);
void ttk_gc_set_xormode (ttk_gc gc, int flag);
void ttk_free_gc (ttk_gc gc);

void ttk_pixel (ttk_surface srf, int x, int y, ttk_color col);
void ttk_pixel_gc (ttk_surface srf, ttk_gc gc, int x, int y);

void ttk_line (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col);
void ttk_line_gc (ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2);

void ttk_rect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col);
void ttk_rect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h);
void ttk_fillrect (ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col);
void ttk_fillrect_gc (ttk_surface srf, ttk_gc gc, int x, int y, int w, int h);

void ttk_poly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col);
void ttk_poly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v);
void ttk_fillpoly (ttk_surface srf, int nv, short *vx, short *vy, ttk_color col);
void ttk_fillpoly_gc (ttk_surface srf, ttk_gc gc, int n, ttk_point *v);

void ttk_ellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col);
void ttk_ellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry);
void ttk_fillellipse (ttk_surface srf, int x, int y, int rx, int ry, ttk_color col);
void ttk_fillellipse_gc (ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry);

// Font funcs. Implemented in core TTK:
ttk_fontinfo *ttk_get_fontinfo (const char *name, int size);
ttk_font ttk_get_font (const char *name, int size);
void ttk_done_fontinfo (ttk_fontinfo *fi);
void ttk_done_font (ttk_font f);
// GFX driver:
void ttk_text (ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char *str);
void ttk_text_gc (ttk_surface srf, ttk_gc gc, int x, int y, const char *str);
int ttk_text_width (ttk_font fnt, const char *str);
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
#else
#define   CKEY   255, 255, 255
#endif


// -- Private --
#define TTK_NO_EVENT     0
#define TTK_BUTTON_DOWN  1
#define TTK_BUTTON_UP    2
#define TTK_SCROLL       3

void ttk_gfx_init();
void ttk_gfx_update (ttk_surface srf);
void ttk_load_font (ttk_fontinfo *fi, const char *fname, int size);
int ttk_get_event (int *arg); /* ret=>ev code, see above; arg=>button pressed */
int ttk_get_rawevent (int *arg);
int ttk_getticks();
void ttk_delay (int ms);

#include "appearance.h"

#endif
