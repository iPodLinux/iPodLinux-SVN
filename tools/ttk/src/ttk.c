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

#include <ttk.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

TWindowStack *ttk_windows = 0;
ttk_font ttk_menufont, ttk_textfont;
ttk_screeninfo *ttk_screen = 0;
ttk_fontinfo *ttk_fonts = 0;
TWidgetList *ttk_header_widgets = 0;
int ttk_button_presstime[128];
int ttk_button_holdsent[128];
int ttk_button_erets[128];
TWidget *ttk_button_pressedfor[128];
int ttk_first_stap, ttk_last_stap, ttk_last_stap_time, ttk_ignore_stap;
int ttk_dirty = 0;
int (*ttk_global_evhandler)(int, int, int);
int (*ttk_global_unusedhandler)(int, int, int);
int ttk_epoch = 0;
static ttk_timer ttk_timers = 0;
static int ttk_transit_frames = 16;
static void (*ttk_clicker)() = ttk_click;

static int ttk_started = 0;
static int ttk_podversion = -1;
static int ttk_scroll_num = 1, ttk_scroll_denom = 1;

#ifdef IPOD
#define outl(datum,addr) (*(volatile unsigned long *)(addr) = (datum))
#define inl(addr) (*(volatile unsigned long *)(addr))
#endif

void ttk_widget_nodrawing (TWidget *w, ttk_surface s) {}
int ttk_widget_noaction_2 (TWidget *w, int i1, int i2) { return TTK_EV_UNUSED; }
int ttk_widget_noaction_1 (TWidget *w, int i) { return TTK_EV_UNUSED; }
int ttk_widget_noaction_i0 (TWidget *w) { return 0; }
void ttk_widget_noaction_0 (TWidget *w) {}

#ifdef IPOD
#define FONTSDIR "/usr/share/fonts"
#define SCHEMESDIR "/usr/share/schemes"
#else
#define FONTSDIR "fonts"
#define SCHEMESDIR "schemes"
#endif

int ttk_version_check (int otherver) 
{
    int myver = TTK_API_VERSION;
    /* version completely equal - OK */
    if (myver == otherver)
        return 1;

    /* version less - some stuff may be missing, won't work */
    if (myver < otherver) {
        fprintf (stderr, "Error: I was compiled with TTK headers version %x but "
                 "linked with library version %x.\n", otherver, myver);
        return 0;
    }
    /* minor version more - only stuff added, OK */
    if (((myver & ~0xff) == (otherver & ~0xff)) && ((myver & 0xff) >= (otherver & 0xff))) {
        fprintf (stderr, "Warning: I was compiled with TTK headers version %x but "
                 "linked with library version %x.\n         "
                 "This will probably be OK, but you should soon recompile this "
                 "program completely to fix the problem.\n", otherver, myver);
        return 1;
    }
    /* major version more - won't work */
    fprintf (stderr, "Error: I was compiled with TTK headers version %x but "
             "linked with library version %x.\n       "
             "There have been major changes between those versions, so you must "
             "recompile this program.\n", otherver, myver);
    return 0;
}

static void ttk_parse_fonts_list() 
{
    char buf[128];
    ttk_fontinfo *current = 0;
    FILE *fp = fopen (FONTSDIR "/fonts.lst", "r");

    if (!fp) {
	fprintf (stderr, "No fonts list. Make one.\n");
	ttk_quit();
	exit (1);
    }
    while (fgets (buf, 128, fp)) {
	if (buf[0] == '#') {
	    continue;
	}
	if (buf[strlen (buf) - 1] == '\n')
	    buf[strlen (buf) - 1] = 0;
	if (!strlen (buf)) {
	    continue;
	}
	
	if (!strchr (buf, '[') || !strchr (buf, ']') || !strchr (buf, '(') || !strchr (buf, ')') ||
	    !strchr (buf, '<') || !strchr (buf, '>')) {
	    fprintf (stderr, "Invalid fonts.lst (bad line: |%s|)\n", buf);
	    ttk_quit();
	    exit (1);
	}

	if (!ttk_fonts) {
	    ttk_fonts = current = malloc (sizeof(ttk_fontinfo));
	} else {
	    if (!current) {
		current = ttk_fonts;
		while (current->next) current = current->next;
	    }
	    current->next = malloc (sizeof(ttk_fontinfo));
	    current = current->next;
	}

	strncpy (current->file, strchr (buf, '[') + 1, 63);
	strncpy (current->name, strchr (buf, '(') + 1, 63);
	current->file[63] = current->name[63] = 0;
	*strchr (current->file, ']') = 0;
	*strchr (current->name, ')') = 0;
	current->size = atoi (strchr (buf, '<') + 1);
	if (strchr (buf, '{')) {
	    char *p = strchr (buf, '{') + 1;
	    int sign = 1;
	    if (*p == '-') {
		sign = -1;
		p++;
	    } else if (*p == '+') {
		p++;
	    }
	    current->offset = atoi (p) * sign;
	} else {
	    current->offset = 0;
	}

	current->refs = 0;
	current->loaded = 0;
	current->next = 0;
    }
}

// Get the font [name] sized closest to [size].
ttk_fontinfo *ttk_get_fontinfo (const char *name, int size)
{
    ttk_fontinfo *current = ttk_fonts;
    ttk_fontinfo *bestmatch = 0;
    int havematch = 0;
    int bestmatchsize = -1;

    while (current) {
	if (strcmp (current->name, name) == 0 && (!current->loaded || current->good)) {
	    if (current->size == 0) { // scalable font, name match --> go
		bestmatch = current;
		bestmatchsize = size;
	    }

	    if ((bestmatchsize < size && current->size > bestmatchsize) ||
		(bestmatchsize > size && current->size < bestmatchsize)) {
		bestmatch = current;
		bestmatchsize = current->size;
	    }
	}
	current = current->next;
    }

    if (!bestmatch) { // no name matches, try ALL fonts close to that size.
	current = ttk_fonts;
	while (current) {
	    if (!current->loaded || current->good) {
		if (current->size == 0) { // scalable font --> go
		    bestmatch = current;
		    bestmatchsize = size;
		}
		
		if ((bestmatchsize < size && current->size > bestmatchsize) ||
		    (bestmatchsize > size && current->size < bestmatchsize)) {
		    bestmatch = current;
		    bestmatchsize = current->size;
		}
	    }
            current = current->next;
	}
    }

    if (!bestmatch->loaded) {
	char tmp[256];
	
	strcpy (tmp, FONTSDIR);
	strcat (tmp, "/");
	strcat (tmp, bestmatch->file);

	bestmatch->good = 1; // will be unset if bad
	ttk_load_font (bestmatch, tmp, bestmatch->size);
	bestmatch->f.ofs = bestmatch->offset;
	bestmatch->f.fi = bestmatch;
	bestmatch->loaded = 1;
	if (!bestmatch->good) return ttk_get_fontinfo ("Any Font", 0);
    }
    bestmatch->refs++;

    return bestmatch;
}

ttk_font ttk_get_font (const char *name, int size) 
{
    return ttk_get_fontinfo (name, size) -> f;
}

void ttk_done_fontinfo (ttk_fontinfo *fi) 
{
    fi->refs--;
    if (fi->refs <= 0) {
	ttk_unload_font (fi);
	fi->loaded = 0;
    }
}
void ttk_done_font (ttk_font f) 
{
    ttk_done_fontinfo (f.fi);
}

TWindow *ttk_init() 
{
    TWindow *ret;

    if (!ttk_screen) {
	int ver;

	ttk_screen = malloc (sizeof(ttk_screeninfo));
#ifdef IPOD
	ver = ttk_get_podversion();
	if (ver & (TTK_POD_1G|TTK_POD_2G|TTK_POD_3G|TTK_POD_4G)) {
	    ttk_screen->w = 160;
	    ttk_screen->h = 128;
	    ttk_screen->bpp = 2;
	} else if (ver & (TTK_POD_MINI_1G|TTK_POD_MINI_2G)) {
	    ttk_screen->w = 138;
	    ttk_screen->h = 110;
	    ttk_screen->bpp = 2;
	} else if (ver & TTK_POD_PHOTO) {
	    ttk_screen->w = 220;
	    ttk_screen->h = 176;
	    ttk_screen->bpp = 16;
        } else if (ver & TTK_POD_NANO) {
            ttk_screen->w = 176;
            ttk_screen->h = 132;
            ttk_screen->bpp = 16;
        } else if (ver & TTK_POD_VIDEO) {
            ttk_screen->w = 320;
            ttk_screen->h = 240;
            ttk_screen->bpp = 16;
	} else {
	    fprintf (stderr, "Couldn't determine your iPod version (v=0%o)\n", ver);
	    free (ttk_screen);
	    exit (1);
	}
#else
	ttk_screen->w = 160;
	ttk_screen->h = 128;
	ttk_screen->bpp = 2;
#endif

	ttk_screen->wx = 0;
	if (ttk_screen->bpp == 16) {
            if (ttk_screen->h == 176 || ttk_screen->h == 132)
                ttk_screen->wy = 21; /* Photo & Nano: 22px header, 22px menu items */
            else if (ttk_screen->h == 240)
                ttk_screen->wy = 23; /* Video: 24px header, 24px menu items */
            else
                ttk_screen->wy = 22; /* sensible default for the unknown */
	} else {
	    ttk_screen->wy = 19; /* Reg/Mini: 20px header, 18px menu items */
	}
    }

    ttk_gfx_init();
    ttk_ap_load (SCHEMESDIR "/default.cs");
    ttk_fillrect (ttk_screen->srf, 0, 0, ttk_screen->w, ttk_screen->h, ttk_makecol (255, 255, 255));
    ttk_fillellipse (ttk_screen->srf, ttk_screen->w/2 - 35, ttk_screen->h/2, 5, 5, ttk_makecol (0, 0, 0));
    ttk_fillellipse (ttk_screen->srf, ttk_screen->w/2, ttk_screen->h/2, 5, 5, ttk_makecol (0, 0, 0));
    ttk_fillellipse (ttk_screen->srf, ttk_screen->w/2 + 35, ttk_screen->h/2, 5, 5, ttk_makecol (0, 0, 0));
    ttk_gfx_update (ttk_screen->srf);

    ttk_parse_fonts_list();
    
    ret = ttk_new_window();

    ttk_windows = (TWindowStack *)malloc (sizeof(TWindowStack));
    ttk_windows->w = ret;
    ttk_windows->minimized = 0;
    ttk_windows->next = 0;
    ret->onscreen++;

    return ret;
}


void ttk_draw_window (TWindow *win) 
{
    TApItem b, *bg;
    ttk_screeninfo *s = ttk_screen;
    
    if (win->background) {
	memcpy (&b, win->background, sizeof(TApItem));
    } else {
	memcpy (&b, ttk_ap_getx ("window.bg"), sizeof(TApItem));
    }
    b.spacing = 0;
    b.type |= TTK_AP_SPACING;
    
    if (win->x > s->wx+2 || win->y > s->wy+2)
	b.spacing = ttk_ap_getx ("window.border") -> spacing;
    bg = &b;
    
    ttk_ap_fillrect (s->srf, bg, win->x, win->y, win->x + win->w, win->y + win->h);
    ttk_blit_image_ex (win->srf, 0, 0, win->w, win->h, s->srf, win->x, win->y);
    if (win->x > s->wx || win->y > s->wy) { // popup window
	ttk_ap_rect (s->srf, ttk_ap_get ("window.border"), win->x, win->y,
		     win->x + win->w, win->y + win->h);
    }

    ttk_dirty |= TTK_DIRTY_WINDOWAREA | TTK_DIRTY_SCREEN;
}


int ttk_button_pressed (int button) 
{
    return ttk_button_presstime[button];
}


int ttk_run()
{
    ttk_screeninfo *s = ttk_screen;
    TWindow *win;
    TWidgetList *cur;
    TWidget *evtarget; // usually win->focus
    int tick, i;
    int ev, earg, eret;
    int in, st, touch;
    int iter = 0;
    const char *keys = "mfwd\n", *p;
    static int initd = 0;
    int local, global;
    static int sofar = 0;
    int time = 0, hs;
    ttk_timer ctim;
    TWidget *pf;

    ttk_started = 1;

    if (!initd) {
	for (i = 0; i < 128; i++) {
	    ttk_button_presstime[i] = 0;
	    ttk_button_holdsent[i] = 0;
            ttk_button_erets[i] = 0;
	}
	
	if (!ttk_windows) {
	    fprintf (stderr, "Run with no windows\n");
	    ttk_quit();
	    exit (1);
	}
	initd = 1;
    }

    ttk_dirty |= TTK_FILTHY;

    while(1) {
	if (!ttk_windows)
	    return 0;

	// If all windows are minimized, this'll loop however-many times
	// and eventually one of the minimized flags will get reset to 0
	// by ttk_move_window().
	if (ttk_windows->minimized)
	    ttk_move_window (ttk_windows->w, 0, TTK_MOVE_END);
	    
	win = ttk_windows->w;

	if (win->input) evtarget = win->input;
	else            evtarget = win->focus;

	tick = ttk_getticks();

	if (win->epoch < ttk_epoch) {
	    ttk_dirty |= TTK_FILTHY;
	    win->dirty++;
	    win->epoch = ttk_epoch;
	}

	// Do hwid, timers
	if (ttk_header_widgets) {
	    cur = ttk_header_widgets;
	    while (cur) {
		if (cur->v->timer && cur->v->timerdelay && (cur->v->timerlast + cur->v->timerdelay <= tick)) {
		    cur->v->timerlast = tick + 1;
		    cur->v->timer (cur->v);
		}
		if (cur->v->dirty && win->show_header) {
		    ttk_ap_fillrect (s->srf, ttk_ap_get ("header.bg"), cur->v->x, cur->v->y,
				     cur->v->x + cur->v->w, cur->v->y + cur->v->h);
		    cur->v->dirty = 0;
		    cur->v->draw (cur->v, s->srf);
		    ttk_dirty |= TTK_DIRTY_SCREEN;
		}
		cur = cur->next;
	    }
	}

	/*** Do timers ***/
	ctim = ttk_timers;
	while (ctim) {
	    if (tick > (ctim->started + ctim->delay)) {
		ttk_timer next = ctim->next;
                void (*fn)() = ctim->fn;
		ttk_destroy_timer (ctim);
		ctim = next;
                // We delay the call of fn in case it itself
                // calls ttk_run() (e.g. for a dialog).
                fn();
		continue;
	    }
	    ctim = ctim->next;
	}

	/*** Draw ***/

	if ((ttk_dirty & TTK_DIRTY_HEADER) && win->show_header) {
	    /* Clear it */
	    ttk_ap_fillrect (s->srf, ttk_ap_get ("header.bg"), 0, 0, s->w, s->wy + ttk_ap_getx ("header.line") -> spacing);

	    if (ttk_header_widgets) {
		/* Draw hwid */
		cur = ttk_header_widgets;
		while (cur) {
		    cur->v->draw (cur->v, s->srf);
		    cur = cur->next;
		}
	    }

	    /* Draw title */
	    ttk_text (s->srf, ttk_menufont,
		      (s->w - ttk_text_width (ttk_menufont, win->title)) / 2,
		      (s->wy - ttk_text_height (ttk_menufont)) / 2,
		      ttk_ap_getx ("header.fg") -> color,
		      win->title);

	    /* Draw line */
	    ttk_ap_hline (s->srf, ttk_ap_get ("header.line"), 0, s->w, s->wy);

	    ttk_dirty &= ~TTK_DIRTY_HEADER;
	    ttk_dirty |= TTK_DIRTY_SCREEN;
	}

	if (win->dirty) {
	    cur = win->widgets;
            ttk_ap_fillrect (win->srf, ttk_ap_get ("window.bg"), 0, 0, win->w, win->h);

	    while (cur) {
		cur->v->dirty = 0;
		cur->v->draw (cur->v, win->srf);
		cur = cur->next;
	    }
	    
	    ttk_dirty |= TTK_DIRTY_WINDOWAREA;
	    win->dirty = 0;
	}

	if (ttk_dirty & TTK_DIRTY_WINDOWAREA) {
	    TApItem b, *bg;
	    if (win->background) {
		memcpy (&b, win->background, sizeof(TApItem));
	    } else {
		memcpy (&b, ttk_ap_getx ("window.bg"), sizeof(TApItem));
	    }
	    b.spacing = 0;
	    b.type |= TTK_AP_SPACING;
	    
	    if (win->x > s->wx+2 || win->y > s->wy+2)
		b.spacing = ttk_ap_getx ("window.border") -> spacing;
	    bg = &b;

	    ttk_ap_fillrect (s->srf, bg, win->x, win->y, win->x + win->w, win->y + win->h);
	    ttk_blit_image_ex (win->srf, 0, 0, win->w, win->h, s->srf, win->x, win->y);
	    if (win->x > s->wx+2 || win->y > s->wy+2) { // popup window
		ttk_ap_rect (s->srf, ttk_ap_get ("window.border"), win->x, win->y,
			     win->x + win->w, win->y + win->h);
	    }
	    ttk_dirty &= ~TTK_DIRTY_WINDOWAREA;
	    ttk_dirty |= TTK_DIRTY_SCREEN;
	}

	/*** Events ***/
	eret = 0;

	/* check text input stuff */
	if (win->input) {
	    if (win->input->frame && win->input->framedelay && (win->input->framelast + win->input->framedelay <= tick)) {
		win->input->framelast = tick;
		eret |= win->input->frame (win->input) & ~TTK_EV_UNUSED;
	    }

	    if (win->input->timer && win->input->timerdelay && (win->input->timerlast + win->input->timerdelay <= tick)) {
		win->input->timerlast = tick + 1;
		eret |= win->input->timer (win->input) & ~TTK_EV_UNUSED;
	    }

	    if (win->input->dirty) {
		ttk_dirty |= TTK_DIRTY_INPUT;
	    }
	}

	while (win->inbuf_start != win->inbuf_end) {
	    if (win->focus) // NOT evtarget, that's probably the TI method
		eret |= win->focus->input (win->focus, win->inbuf[win->inbuf_start]) & ~TTK_EV_UNUSED;
	    win->inbuf_start++;
	    win->inbuf_start &= 0x1f;
	}

	if ((ttk_dirty & TTK_DIRTY_INPUT) && win->input) {
	    ttk_ap_fillrect (s->srf, ttk_ap_get ("window.bg"), win->input->x, win->input->y,
			     win->input->x + win->input->w, win->input->y + win->input->h);

	    if (ttk_ap_get ("window.border")) {
		TApItem border;
		memcpy (&border, ttk_ap_getx ("window.border"), sizeof(TApItem));
		border.spacing = -1;
		
		ttk_ap_rect (s->srf, &border, win->input->x, win->input->y,
			     win->input->x + win->input->w, win->input->y + win->input->h);
	    }

	    win->input->draw (win->input, s->srf);

	    ttk_dirty &= ~TTK_DIRTY_INPUT;
            ttk_dirty |= TTK_DIRTY_SCREEN;
	}

	/* check fps + draw individual dirties */
	cur = win->widgets;
	while (cur) {
	    if (cur->v->frame && cur->v->framedelay && (cur->v->framelast + cur->v->framedelay <= tick)) {
		cur->v->framelast = tick;
		eret |= cur->v->frame (cur->v) & ~TTK_EV_UNUSED;
	    }
	    
	    if (cur->v->timer && cur->v->timerdelay && (cur->v->timerlast + cur->v->timerdelay <= tick)) {
		cur->v->timerlast = tick + 1;
		eret |= cur->v->timer (cur->v) & ~TTK_EV_UNUSED;
	    }

	    if (cur->v->dirty) {
                ttk_ap_fillrect (win->srf, ttk_ap_get ("window.bg"), cur->v->x, cur->v->y,
                                 cur->v->x + cur->v->w, cur->v->y + cur->v->h);
		cur->v->dirty = 0;
		cur->v->draw (cur->v, win->srf);
		ttk_dirty |= TTK_DIRTY_WINDOWAREA;
	    }

	    cur = cur->next;
	}

	/* check new events (down, button, scroll) */
	earg = 0;
	if (evtarget && evtarget->rawkeys)
	    ev = ttk_get_rawevent (&earg);
	else
	    ev = ttk_get_event (&earg);

	local = global = 1;

	if (!ev)
	    local = global = 0;

	if (!ttk_global_evhandler)
	    global = 0;
	if (ev == TTK_BUTTON_DOWN) {
	    if (!((ttk_button_pressedfor[earg] == 0 || ttk_button_pressedfor[earg] == evtarget) &&
		  (ttk_button_presstime[earg] == 0 || ttk_button_presstime[earg] == tick))) // key rept
		global = 0;
	}

	if (!evtarget) local = 0;

	if (global) {
	    local &= !ttk_global_evhandler (ev, earg, tick - ttk_button_presstime[earg]);
	}

	if (ev == TTK_SCROLL) {
	    if (ttk_scroll_denom > 1) {
		sofar += earg;
		if (sofar > -ttk_scroll_denom && sofar < ttk_scroll_denom) local = 0;
		else if (sofar < 0) {
		    while (sofar <= -ttk_scroll_denom) sofar += ttk_scroll_denom;
		} else {
		    while (sofar >= ttk_scroll_denom) sofar -= ttk_scroll_denom;
		}
	    }
	    earg *= ttk_scroll_num;
	}

	/* pass event to local, if we should; update event
	   metadata in all cases. */
	switch (ev) {
	case TTK_BUTTON_DOWN:
	    if (!ttk_button_presstime[earg] || !ttk_button_pressedfor[earg]) { // don't reset with key-repted buttons
		ttk_button_presstime[earg] = tick;
		ttk_button_pressedfor[earg] = evtarget;
		ttk_button_holdsent[earg] = 0;
	    }
	    
	    // Don't send different parts of same keyt-rept to different widgets:
	    if (local && (ttk_button_pressedfor[earg] == evtarget) &&
		((ttk_button_presstime[earg] == tick) || evtarget->keyrepeat))
	    {
                int er = evtarget->down (evtarget, earg);
                ttk_button_erets[earg] |= er; eret |= er & ~TTK_EV_UNUSED;
	    }
	    break;
	case TTK_BUTTON_UP:
	    time = tick - ttk_button_presstime[earg];
	    pf = ttk_button_pressedfor[earg];
            hs = ttk_button_holdsent[earg];
	    
	    // Need to be before, in case button() launches its own ttk_run().
	    ttk_button_presstime[earg] = 0;
	    ttk_button_holdsent[earg] = 0;
	    ttk_button_pressedfor[earg] = 0;
	    
	    if (evtarget == pf && local && !hs) {
		int er = evtarget->button (evtarget, earg, time);
                // If *both* down and button returned unused, do unused. Otherwise,
                // don't.
                eret |= er;
                if (!((er & TTK_EV_UNUSED) && (ttk_button_erets[earg] & TTK_EV_UNUSED))) {
                    eret &= ~TTK_EV_UNUSED;
                } else {
                    eret |= TTK_EV_UNUSED;
                }
            }
	    break;
	case TTK_SCROLL:
	    if (local)
		eret |= evtarget->scroll (evtarget, earg) & ~TTK_EV_UNUSED;
	    break;
	}
	
	if ((eret & TTK_EV_UNUSED) && ttk_global_unusedhandler)
	    eret |= ttk_global_unusedhandler (ev, earg, time);

	/* check old events (stap, held) */
	if (evtarget) {
	    // held
	    for (p = keys; *p; p++) {
		if (ttk_button_presstime[*p] && (tick - ttk_button_presstime[*p] >= evtarget->holdtime) && !ttk_button_holdsent[*p] && (evtarget->held != ttk_widget_noaction_1)) {
                    int er = evtarget->held (evtarget, *p);
                    if (!(er & TTK_EV_UNUSED)) {
                        eret |= er;
                        ttk_button_holdsent[*p] = 1;
                    }
		}

		// If user pushes a button on clickwheel, it'll also register as a tap.
		// Counter that.
		if (ttk_button_presstime[*p] && (ttk_get_podversion() & TTK_POD_PP5020) &&
		    ttk_last_stap_time) ttk_ignore_stap = 1;
	    }
	    // stap
#ifdef IPOD
	    if (ttk_get_podversion() & TTK_POD_PP5020) {
		in = inl (0x7000C140);
		st = (in & 0x40000000);
		touch = (in & 0x007F0000) >> 16;
		if (st) {
		    if (!ttk_last_stap_time) {
			ttk_first_stap = touch;
			ttk_last_stap_time = tick;
		    }
		    ttk_last_stap = touch;
		} else if (ttk_last_stap_time) {
		    // Heuristic: finger moved <1/20 of wheel, held for <2/5 sec.
		    if ((abs(ttk_last_stap - ttk_first_stap) <= 5) &&
			((tick - ttk_last_stap_time) <= 400) &&
			!ttk_ignore_stap)
		    {
			eret |= evtarget->stap (evtarget, ttk_first_stap);
		    }
		    // Even if it didn't match, reset it. Otherwise, no taps will be
		    // recorded once you scroll.
		    ttk_last_stap_time = 0;
		    ttk_ignore_stap = 0;
		}
	    }
#else
	    if (ev == TTK_BUTTON_UP) {
		switch (earg) {
		case '7': eret |= evtarget->stap (evtarget, 84); break;
		case '8': eret |= evtarget->stap (evtarget, 0); break;
		case '9': eret |= evtarget->stap (evtarget, 12); break;
		case '6': eret |= evtarget->stap (evtarget, 24); break;
		case '3': eret |= evtarget->stap (evtarget, 36); break;
		case '2': eret |= evtarget->stap (evtarget, 48); break;
		case '1': eret |= evtarget->stap (evtarget, 60); break;
		case '4': eret |= evtarget->stap (evtarget, 72); break;
		default: break;
		}
	    }
#endif

	    if (eret & TTK_EV_CLICK)
		(*ttk_clicker)();
	    if (eret & TTK_EV_DONE)
		return (eret >> 8);

	    if (ttk_dirty & TTK_DIRTY_SCREEN) {
		ttk_gfx_update (ttk_screen->srf);
		ttk_dirty &= ~TTK_DIRTY_SCREEN;
	    }
	}
#ifndef IPOD
	ttk_delay(30);
#endif
    }
}


void ttk_click()
{
#ifdef IPOD
    if (ttk_get_podversion() & (TTK_POD_PP5020 | TTK_POD_PP5022)) {
	int i, j;
	outl(inl(0x70000010) & ~0xc, 0x70000010);
	outl(inl(0x6000600c) | 0x20000, 0x6000600c);    /* enable device */
	for (j = 0; j < 10; j++) {
	    for (i = 0; i < 0x888; i++ ) {
		outl(0x80000000 | 0x800000 | i, 0x7000a000); /* set pitch */
	    }
	}
	outl(0x0, 0x7000a000);    /* piezo off */
    } else {
	static int fd = -1; 
	static char buf;
	
	if (fd == -1 && (fd = open("/dev/ttyS1", O_WRONLY)) == -1
	    && (fd = open("/dev/tts/1", O_WRONLY)) == -1) {
	    return;
	}
    	
	write(fd, &buf, 1);
    }
#endif
}


void ttk_quit() 
{
    ttk_gfx_quit();
    ttk_stop_cop();
}


static long iPod_GetGeneration() 
{
    int i;
    char cpuinfo[256];
    char *ptr;
    FILE *file;
    
    if ((file = fopen("/proc/cpuinfo", "r")) != NULL) {
	while (fgets(cpuinfo, sizeof(cpuinfo), file) != NULL)
	    if (strncmp(cpuinfo, "Revision", 8) == 0)
		break;
	fclose(file);
    }
    for (i = 0; !isspace(cpuinfo[i]); i++);
    for (; isspace(cpuinfo[i]); i++);
    ptr = cpuinfo + i + 2;
    
    return strtol(ptr, NULL, 16);
}

static int ttk_get_podversion_raw() 
{
#ifdef IPOD
    static int ver;
    if (!ver) ver = iPod_GetGeneration();
    switch (ver >> 16) {
    case 0x0: return 0;
    case 0x1: return TTK_POD_1G;
    case 0x2: return TTK_POD_2G;
    case 0x3: return TTK_POD_3G;
    case 0x4: return TTK_POD_MINI_1G;
    case 0x5: return TTK_POD_4G;
    case 0x6: return TTK_POD_PHOTO;
    case 0x7: return TTK_POD_MINI_2G;
    case 0xB: return TTK_POD_VIDEO;
    case 0xC: return TTK_POD_NANO;
    default: return 0;
    }
#else
    return TTK_POD_X11;
#endif
}

int ttk_get_podversion() 
{
    if (ttk_podversion == -1)
	ttk_podversion = ttk_get_podversion_raw();
    return ttk_podversion;
}


void ttk_get_screensize (int *w, int *h, int *bpp)
{
    if (!ttk_screen) return;
    
    if (w) *w = ttk_screen->w;
    if (h) *h = ttk_screen->h;
    if (bpp) *bpp = ttk_screen->bpp;
}


void ttk_set_emulation (int w, int h, int bpp) 
{
#ifndef IPOD
    if (!ttk_screen)
	ttk_screen = malloc (sizeof(struct ttk_screeninfo));
    
    ttk_screen->w = w;
    ttk_screen->h = h;
    ttk_screen->bpp = bpp;

    ttk_screen->wx = 0;
    if (ttk_screen->bpp == 16) {
	ttk_screen->wy = 22; /* Photo: 22px header, 22px menu items */
    } else {
	ttk_screen->wy = 20; /* Reg/Mini: 20px header, 18px menu items */
    }
#endif
}


ttk_fontinfo *ttk_get_fontlist() 
{
    return ttk_fonts;
}


TWindow *ttk_new_window() 
{
    TWindow *ret = calloc (1, sizeof(TWindow));
    ret->show_header = 1;
    ret->titlefree = 0;
    ret->widgets = 0;
    ret->x = ttk_screen->wx;
    ret->y = ttk_screen->wy;
    ret->w = ttk_screen->w - ttk_screen->wx;
    ret->h = ttk_screen->h - ttk_screen->wy;
    ret->color = (ttk_screen->bpp == 16);
    ret->srf = ttk_new_surface (ttk_screen->w, ttk_screen->h, ret->color? 16 : 2);
    ret->background = 0;
    ret->focus = ret->input = 0;
    ret->dirty = 0;
    ret->epoch = ttk_epoch;
    ret->inbuf_start = ret->inbuf_end = 0;
    ret->onscreen = 0;

    if (ttk_windows) {
	ret->title = strdup (ttk_windows->w->title);
	ret->titlefree = 1;
    } else {
	ret->title = "TTK";
    }

    ttk_ap_fillrect (ret->srf, ttk_ap_get ("window.bg"), 0, 0, ret->w, ret->h);

    return ret;
}


void ttk_window_show_header (TWindow *win) 
{
    if (!win->show_header) {
	win->show_header = 1;
	ttk_dirty |= TTK_FILTHY;
	if (win->focus) {
	    win->focus->h -= 20;
	    win->focus->dirty++;
	}
        win->x = ttk_screen->wx;
        win->y = ttk_screen->wy;
        win->w = ttk_screen->w - win->x;
        win->h = ttk_screen->h - win->y;
    }
}


void ttk_window_hide_header (TWindow *win) 
{
    if (win->show_header) {
	win->show_header = 0;
	ttk_dirty |= TTK_FILTHY;
	if (win->focus) {
	    win->focus->h += ttk_screen->wy;
	    win->focus->dirty++;
	}
        win->x = 0;
        win->y = 0;
        win->w = ttk_screen->w;
        win->h = ttk_screen->h;
    }
}


void ttk_free_window (TWindow *win) 
{
    ttk_hide_window (win);
    
    if (win->widgets) {
	TWidgetList *cur = win->widgets, *next;
	while (cur) {
	    next = cur->next;
	    cur->v->win = 0;
	    ttk_free_widget (cur->v);
	    free (cur);
	    cur = next;
	}
    }
    ttk_free_surface (win->srf);
    if (win->titlefree) free ((void *)win->title);
    free (win);
}


void ttk_show_window (TWindow *win) 
{
    if (!win->onscreen) {
	TWindow *oldwindow = ttk_windows? ttk_windows->w : 0;
	TWindowStack *next = ttk_windows;
        TWidgetList *cur;
	ttk_windows = malloc (sizeof(struct TWindowStack));
	ttk_windows->w = win;
	ttk_windows->minimized = 0;
	ttk_windows->next = next;
	win->onscreen++;

	if (ttk_started && oldwindow &&
	    oldwindow->w == win->w && oldwindow->h == win->h &&
	    oldwindow->x == ttk_screen->wx && oldwindow->y == ttk_screen->wy) {
	    
	    int i;
	    int jump = win->w / ttk_transit_frames;
	    
            // Render the stuff in the new window
            cur = win->widgets;
            while (cur) {
                ttk_ap_fillrect (win->srf, ttk_ap_get ("window.bg"), 0, 0, win->w, win->h);
                cur->v->draw (cur->v, win->srf);
                cur = cur->next;
            }

	    for (i = 0; i < ttk_transit_frames; i++) {
	    	ttk_ap_fillrect (ttk_screen->srf, ttk_ap_get ("window.bg"), ttk_screen->wx,
	    	                 ttk_screen->wy, ttk_screen->w, ttk_screen->h);
	    	ttk_blit_image_ex (oldwindow->srf, i * jump, 0, oldwindow->w - i*jump, oldwindow->h,
	    	                   ttk_screen->srf, ttk_screen->wx, ttk_screen->wy);
	    	ttk_blit_image_ex (win->srf, 0, 0, i * jump, oldwindow->h,
	    	                   ttk_screen->srf, oldwindow->w - i*jump + ttk_screen->wx,
	    	                   ttk_screen->wy);
	    	ttk_gfx_update (ttk_screen->srf);
#ifndef IPOD
	    	ttk_delay (10);
#endif
	    }
	    
	    ttk_blit_image (win->srf, ttk_screen->srf, ttk_screen->wx, ttk_screen->wy);
	    ttk_gfx_update (ttk_screen->srf);
	}
    } else {
	ttk_move_window (win, 0, TTK_MOVE_ABS);
	ttk_windows->minimized = 0;
    }

    ttk_dirty |= TTK_DIRTY_WINDOWAREA | TTK_DIRTY_HEADER;
    if (ttk_windows->w->input)
        ttk_dirty |= TTK_DIRTY_INPUT;
}


void ttk_set_popup (TWindow *win)
{
    int minx, miny, maxx, maxy;
    TWidgetList *cur = win->widgets;

    minx = miny = maxx = maxy = 0;

    while (cur) {
	if (cur->v->x < minx) minx = cur->v->x;
	if (cur->v->y < miny) miny = cur->v->y;
	if (cur->v->x + cur->v->w > maxx) maxx = cur->v->x + cur->v->w;
	if (cur->v->y + cur->v->h > maxy) maxy = cur->v->y + cur->v->h;
	
	cur = cur->next;
    }

    // Move widgets to upper-left corner of new window
    cur = win->widgets;
    while (cur) {
	cur->v->x -= minx;
	cur->v->y -= miny;

	cur = cur->next;
    }

    // Center the window
    win->x = ((ttk_screen->w - ttk_screen->wx) - (maxx - minx)) / 2 + ttk_screen->wx;
    win->y = ((ttk_screen->h - ttk_screen->wy) - (maxy - miny)) / 2 + ttk_screen->wy;
    win->w = maxx - minx;
    win->h = maxy - miny;
}


void ttk_popup_window (TWindow *win) 
{
    ttk_set_popup (win);
    ttk_show_window (win);
}


int ttk_hide_window (TWindow *win) 
{
    TWindowStack *current = ttk_windows, *last = 0;
    int ret = 0;
    
    if (!current) return 0;

    while (current) {
	if (current->w == win) {
	    if (last)
		last->next = current->next;
	    else
		ttk_windows = current->next;
	    
	    ttk_dirty |= TTK_DIRTY_WINDOWAREA | TTK_DIRTY_HEADER;

	    free (current);
	    current = last;
	    win->onscreen = 0;
	    ret++;
	}

	last = current;
	if (current) current = current->next;
    }

    if (ret && ttk_windows) {
	TWindow *newwindow = ttk_windows->w;
	
	if (newwindow->w == win->w && newwindow->h == win->h &&
	    newwindow->x == ttk_screen->wx && newwindow->y == ttk_screen->wy) {
 	    
	    int i;
	    int jump = win->w / ttk_transit_frames;

	    // Render stuff in the new window
            TWidgetList *cur = win->widgets;
            while (cur) {
                ttk_ap_fillrect (win->srf, ttk_ap_get ("window.bg"), 0, 0, win->w, win->h);
                cur->v->draw (cur->v, win->srf);
                cur = cur->next;
            }

	    for (i = ttk_transit_frames - 1; i >= 0; i--) {
	    	ttk_ap_fillrect (ttk_screen->srf, ttk_ap_get ("window.bg"), ttk_screen->wx,
	    	                 ttk_screen->wy, ttk_screen->w, ttk_screen->h);
	    	ttk_blit_image_ex (newwindow->srf, i * jump, 0, win->w - i*jump, win->h,
	    	                   ttk_screen->srf, ttk_screen->wx, ttk_screen->wy);
	    	ttk_blit_image_ex (win->srf, 0, 0, i * jump, win->h,
	    	                   ttk_screen->srf, win->w - i*jump + ttk_screen->wx,
	    	                   ttk_screen->wy);
	    	ttk_gfx_update (ttk_screen->srf);
#ifndef IPOD
	    	ttk_delay (10);
#endif
	    }
	    
	    ttk_blit_image (newwindow->srf, ttk_screen->srf, ttk_screen->wx, ttk_screen->wy);
	    ttk_gfx_update (ttk_screen->srf);
	}
    }

    return ret;
}


void ttk_move_window (TWindow *win, int offset, int whence)
{
    TWindowStack *cur = ttk_windows, *last = 0;
    int oidx, idx = -1, nitems = 0, i = 0;
    int minimized;
    if (!cur) return;

    while (cur) {
	if ((cur->w == win) && (idx == -1)) {
	    idx = nitems;

	    if (last)
		last->next = cur->next;
	    else
		ttk_windows = cur->next;

	    minimized = cur->minimized;
	    last = cur;
	    cur = cur->next;
	    free (last);
	}
	nitems++;
	last = cur;
	if (cur) cur = cur->next;
    }

    if (idx == -1) return;

    oidx = idx;

    switch (whence) {
    case TTK_MOVE_ABS:
	idx = offset;
	break;
	
    case TTK_MOVE_REL:
	idx -= offset;
	break;

    case TTK_MOVE_END:
	idx = 32767; // obscenely high number
	break;
    }

    if (idx < 0) idx = 0;
    
    cur = ttk_windows;
    last = 0;
    
    while (cur) {
	if (idx == i) {
	    TWindowStack *s = malloc (sizeof(TWindowStack));
	    if (last)
		last->next = s;
	    else
		ttk_windows = s;
	    s->next = cur;
	    s->w = win;
	    s->minimized = minimized;
	    break;
	}

	if (oidx == i)
	    i++; // double-increment, since something used to
	         // be here and [idx] is still based on that. :TRICKY:

	last = cur;
	cur = cur->next;
	i++;
    }

    if (!cur && ttk_windows) { // index was past end, put it on end
	cur = ttk_windows;
	while (cur->next) cur = cur->next;
	cur->next = malloc (sizeof(TWindowStack));
	cur->next->w = win;
	cur->next->minimized = minimized;
	cur->next->next = 0;
    }

    if (ttk_windows)
        ttk_windows->minimized = 0;
    ttk_dirty |= TTK_FILTHY;
}


void ttk_window_title (TWindow *win, const char *str)
{
    if (win->titlefree)
	free ((void *)win->title);

    win->title = malloc (strlen (str) + 1);
    strcpy ((char *)win->title, str);
    win->titlefree = 1;

    if (ttk_windows && ttk_windows->w == win) {
	ttk_dirty |= TTK_DIRTY_HEADER;
    }
}


void ttk_add_header_widget (TWidget *wid)
{
    TWidgetList *current;
    
    wid->win = 0;

    if (!ttk_header_widgets) {
	ttk_header_widgets = current = malloc (sizeof(TWidgetList));
    } else {
	current = ttk_header_widgets;
	while (current->next) current = current->next;
	current->next = malloc (sizeof(TWidgetList));
	current = current->next;
    }

    current->v = wid;
    current->next = 0;
}


void ttk_remove_header_widget (TWidget *wid)
{
    TWidgetList *current = ttk_header_widgets, *last = 0;
    if (!current) return;
    
    while (current) {
	if (current->v == wid) {
	    if (last) last->next = current->next;
	    else      ttk_header_widgets = current->next;
	    free (current);
	    if (last) current = last->next;
	    else      current = ttk_header_widgets;
	} else {
	    if (current) last = current, current = current->next;
	}
    }
}


TWidget *ttk_new_widget (int x, int y) 
{
    TWidget *ret = malloc (sizeof(TWidget));
    int i;
    if (!ret) return 0;

    ret->x = x; ret->y = y; ret->w = 0; ret->h = 0;
    ret->focusable = 0; ret->keyrepeat = 0; ret->rawkeys = 0;
    ret->framelast = 0; ret->framedelay = 0;
    ret->timerlast = 0; ret->timerdelay = 0;
    ret->holdtime = 1000; ret->dirty = 1;

    ret->draw = ttk_widget_nodrawing;
    ret->button = ttk_widget_noaction_2;
    ret->down = ret->held = ret->scroll = ret->stap = ttk_widget_noaction_1;
    ret->frame = ret->timer = ttk_widget_noaction_i0;
    ret->destroy = ttk_widget_noaction_0;
    ret->data = 0;

    return ret;
}


void ttk_free_widget (TWidget *wid)
{
    if (!wid) return;
    
    wid->destroy (wid);
    if (wid->win)
	ttk_remove_widget (wid->win, wid);
    free (wid);
}


TWindow *ttk_add_widget (TWindow *win, TWidget *wid)
{
    TWidgetList *current;
    if (!wid || !win) return win;

    if (!win->widgets) {
	win->widgets = current = malloc (sizeof(TWidgetList));
    } else {
	current = win->widgets;
	while (current->next) current = current->next;
	current->next = malloc (sizeof(TWidgetList));
	current = current->next;
    }

    if (wid->focusable)
	win->focus = wid;

    wid->dirty++;
    wid->win = win;
    
    current->v = wid;
    current->next = 0;

    return win;
}


int ttk_remove_widget (TWindow *win, TWidget *wid)
{
    TWidgetList *current, *last = 0;
    int count = 0;
    if (!win || !wid || !win->widgets) return 0;

    if (wid == win->focus) win->focus = 0;

    current = win->widgets;
    
    while (current) {
	if (current->v == wid) {
	    if (last) last->next = current->next;
	    else      win->widgets = current->next;
	    free (current);
	    if (last) current = last->next;
	    else      current = win->widgets;
	    count++;
	} else {
	    if (current && current->v->focusable) win->focus = current->v;
	    if (current) last = current, current = current->next;
	}
    }

    win->dirty++;
    wid->win = 0;

    return count;
}


void ttk_widget_set_fps (TWidget *wid, int fps) 
{
    if (fps) {
	wid->framelast = ttk_getticks();
	wid->framedelay = 1000/fps;
    } else {
	wid->framelast = wid->framedelay = 0;
    }
}


void ttk_widget_set_inv_fps (TWidget *wid, int fps_m1)
{
    wid->framelast = ttk_getticks();
    wid->framedelay = 1000*fps_m1; // works for 0 to unset
}


void ttk_widget_set_timer (TWidget *wid, int ms) 
{
    wid->timerlast = ttk_getticks();
    wid->timerdelay = ms; // works for 0 to unset
}


void ttk_set_global_event_handler (int (*fn)(int ev, int earg, int time)) 
{
    ttk_global_evhandler = fn;
}
void ttk_set_global_unused_handler (int (*fn)(int ev, int earg, int time)) 
{
    ttk_global_unusedhandler = fn;
}


ttk_color ttk_mapcol (ttk_color col, ttk_surface src, ttk_surface dst) 
{
    int r, g, b;
    if (!src) src = ttk_screen->srf;
    if (!dst) dst = ttk_screen->srf;
    if (src == dst) return col;
    
    ttk_unmakecol_ex (col, &r, &g, &b, src);
    return ttk_makecol_ex (r, g, b, dst);
}


#ifdef IPOD
/*** The following LCD code is taken from Linux kernel uclinux-2.4.24-uc0-ipod2,
     file arch/armnommu/mach-ipod/fb.c. A few modifications have been made. ***/

#define LCD_DATA          0x10
#define LCD_CMD           0x08
#define IPOD_OLD_LCD_BASE 0xc0001000
#define IPOD_OLD_LCD_RTC  0xcf001110
#define IPOD_NEW_LCD_BASE 0x70003000
#define IPOD_NEW_LCD_RTC  0x60005010

static unsigned long lcd_base = 0, lcd_rtc = 0, lcd_width = 0, lcd_height = 0;

/* get current usec counter */
static int M_timer_get_current(void)
{
	return inl(lcd_rtc);
}

/* check if number of useconds has past */
static int M_timer_check(int clock_start, int usecs)
{
	unsigned long clock;
	clock = inl(lcd_rtc);
	
	if ( (clock - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for LCD with timeout */
static void M_lcd_wait_write(void)
{
	if ( (inl(lcd_base) & 0x8000) != 0 ) {
		int start = M_timer_get_current();
			
		do {
			if ( (inl(lcd_base) & (unsigned int)0x8000) == 0 ) 
				break;
		} while ( M_timer_check(start, 1000) == 0 );
	}
}


/* send LCD data */
static void M_lcd_send_data(int data_lo, int data_hi)
{
	M_lcd_wait_write();
	
	outl(data_lo, lcd_base + LCD_DATA);
		
	M_lcd_wait_write();
	
	outl(data_hi, lcd_base + LCD_DATA);

}

/* send LCD command */
static void
M_lcd_prepare_cmd(int cmd)
{
	M_lcd_wait_write();

	outl(0x0, lcd_base + LCD_CMD);

	M_lcd_wait_write();
	
	outl(cmd, lcd_base + LCD_CMD);
	
}

/* send LCD command and data */
static void M_lcd_cmd_and_data(int cmd, int data_lo, int data_hi)
{
	M_lcd_prepare_cmd(cmd);

	M_lcd_send_data(data_lo, data_hi);
}

// Copied from uW
static void M_update_display(int sx, int sy, int mx, int my, unsigned char *data, int pitch)
{
	int y;
	unsigned short cursor_pos;

	sx >>= 3;
	mx >>= 3;

	cursor_pos = sx + (sy << 5);

	for ( y = sy; y <= my; y++ ) {
		unsigned char *img_data;
		int x;

		/* move the cursor */
		M_lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);

		/* setup for printing */
		M_lcd_prepare_cmd(0x12);

		img_data = data + (sx << 1) + (y * (lcd_width/4));

		/* loops up to 160 times */
		for ( x = sx; x <= mx; x++ ) {
		        /* display eight pixels */
			M_lcd_send_data(*(img_data + 1), *img_data);

			img_data += 2;
		}

		/* update cursor pos counter */
		cursor_pos += 0x20;
	}
}

/* get current usec counter */
static int C_timer_get_current(void)
{
	return inl(0x60005010);
}

/* check if number of useconds has past */
static int C_timer_check(int clock_start, int usecs)
{
	unsigned long clock;
	clock = inl(0x60005010);
	
	if ( (clock - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for LCD with timeout */
static void C_lcd_wait_write(void)
{
	if ((inl(0x70008A0C) & 0x80000000) != 0) {
		int start = C_timer_get_current();
			
		do {
			if ((inl(0x70008A0C) & 0x80000000) == 0) 
				break;
		} while (C_timer_check(start, 1000) == 0);
	}
}
static void C_lcd_cmd_data(int cmd, int data)
{
	C_lcd_wait_write();
	outl(cmd | 0x80000000, 0x70008A0C);

	C_lcd_wait_write();
	outl(data | 0x80000000, 0x70008A0C);
}

static void C_update_display(int sx, int sy, int mx, int my, unsigned char *data, int pitch)
{
	int height = (my - sy) + 1;
	int width = (mx - sx) + 1;

	char *addr = (char *)data;

	if (width & 1) width++;

	/* start X and Y */
	C_lcd_cmd_data(0x12, (sy & 0xff));
	C_lcd_cmd_data(0x13, (((ttk_screen->w - 1) - sx) & 0xff));

	/* max X and Y */
	C_lcd_cmd_data(0x15, (((sy + height) - 1) & 0xff));
	C_lcd_cmd_data(0x16, (((((ttk_screen->w - 1) - sx) - width) + 1) & 0xff));

	addr += sx + sy * pitch;

	while (height > 0) {
		int h, x, y, pixels_to_write;

		pixels_to_write = (width * height) * 2;

		/* calculate how much we can do in one go */
		h = height;
		if (pixels_to_write > 64000) {
			h = (64000/2) / width;
			pixels_to_write = (width * h) * 2;
		}

		outl(0x10000080, 0x70008A20);
		outl((pixels_to_write - 1) | 0xC0010000, 0x70008A24);
		outl(0x34000000, 0x70008A20);

		/* for each row */
		for (x = 0; x < h; x++)
		{
			/* for each column */
			for (y = 0; y < width; y += 2) {
				unsigned two_pixels;

				two_pixels = addr[0] | (addr[1] << 16);
				addr += 2;

				while ((inl(0x70008A20) & 0x1000000) == 0);

				/* output 2 pixels */
				outl(two_pixels, 0x70008B00);
			}

			addr += pitch - width;
		}

		while ((inl(0x70008A20) & 0x4000000) == 0);

		outl(0x0, 0x70008A24);

		height = height - h;
	}
}
/** End copied code. **/


void ttk_update_lcd (int xstart, int ystart, int xend, int yend, unsigned char *data)
{
    static int do_color = 0;
    static int pitch = 0;
    
    if (lcd_base < 0) {
	int ver = ttk_get_podversion();
	if (!ver) { // X11
	    fprintf (stderr, "No iPod. Can't ttk_update_lcd.");
	    ttk_quit();
	    exit (1);
	}
	if (ver & (TTK_POD_1G|TTK_POD_2G|TTK_POD_3G)) {
	    lcd_base = IPOD_OLD_LCD_BASE;
	    lcd_rtc = IPOD_OLD_LCD_RTC;
	} else {
	    lcd_base = IPOD_NEW_LCD_BASE;
	    lcd_rtc = IPOD_NEW_LCD_RTC;
	}

	lcd_width = ttk_screen->w;
	lcd_height = ttk_screen->h;

	if (ver & TTK_POD_PHOTO) do_color = 1;
	
	pitch = ttk_screen->w;
    }

    if (do_color) C_update_display (xstart, ystart, xend, yend, data, pitch);
    else          M_update_display (xstart, ystart, xend, yend, data, pitch);
}

#else

void ttk_update_lcd (int xstart, int ystart, int xend, int yend, unsigned char *data)
{
    fprintf (stderr, "update_lcd() skipped: not on iPod\n");
}

#endif

void ttk_start_cop (void (*fn)()) { exit (255); }
void ttk_stop_cop() {}
void ttk_sleep_cop() {}
void ttk_wake_cop() {}


ttk_timer ttk_create_timer (int ms, void (*fn)()) 
{
    ttk_timer cur;
    if (!ttk_timers) {
	cur = ttk_timers = malloc (sizeof(struct _ttk_timer));
    } else {
	cur = ttk_timers;
	while (cur->next) cur = cur->next;
	cur->next = malloc (sizeof(struct _ttk_timer));
	cur = cur->next;
    }
    cur->started = ttk_getticks();
    cur->delay = ms;
    cur->fn = fn;
    cur->next = 0;
    return cur;
}

void ttk_destroy_timer (ttk_timer tim) 
{
    ttk_timer cur = ttk_timers, last = 0;
    while (cur && (cur != tim)) {
	last = cur;
	cur = cur->next;
    }
    if (cur != tim) {
	fprintf (stderr, "Warning: didn't delete nonexistent timer %p\n", tim);
	return;
    }

    if (last) {
	last->next = cur->next;
	free (cur);
    } else {
	ttk_timers = cur->next;
	free (cur);
    }
}


void ttk_set_transition_frames (int frames) 
{
    if (frames <= 0) frames = 1;
    ttk_transit_frames = frames;
}


static void do_nothing() {}
void ttk_set_clicker (void (*fn)()) 
{
    if (!fn) fn = do_nothing;
    ttk_clicker = fn;
}


void ttk_set_scroll_multiplier (int num, int denom) 
{
    ttk_scroll_num = num;
    ttk_scroll_denom = denom;
}


int ttk_input_start_for (TWindow *win, TWidget *inmethod)
{
    inmethod->x = ttk_screen->w - inmethod->w - 1;
    inmethod->y = ttk_screen->h - inmethod->h - 1;
    
    inmethod->win = win;
    win->input = inmethod;
    return inmethod->h;
}

int ttk_input_start (TWidget *inmethod) 
{
    if (!ttk_windows || !ttk_windows->w) return -1;

    ttk_input_start_for (ttk_windows->w, inmethod);
    ttk_dirty |= TTK_DIRTY_WINDOWAREA | TTK_DIRTY_INPUT;
    return inmethod->h;
}


void ttk_input_move (int x, int y) 
{
    if (!ttk_windows || !ttk_windows->w || !ttk_windows->w->input) return;
    
    ttk_windows->w->input->x = x;
    ttk_windows->w->input->y = y;
}
void ttk_input_move_for (TWindow *win, int x, int y)
{
    win->input->x = x;
    win->input->y = y;
}


void ttk_input_size (int *w, int *h)
{
    if (!ttk_windows || !ttk_windows->w || !ttk_windows->w->input) return;
    
    if (w) *w = ttk_windows->w->input->w;
    if (h) *h = ttk_windows->w->input->h;
}
void ttk_input_size_for (TWindow *win, int *w, int *h) 
{
    if (w) *w = win->input->w;
    if (h) *h = win->input->h;
}


void ttk_input_char (int ch) 
{
    if (!ttk_windows || !ttk_windows->w) return;
    
    ttk_windows->w->inbuf[ttk_windows->w->inbuf_end++] = ch;
    ttk_windows->w->inbuf_end &= 0x1f;
}


void ttk_input_end() 
{
    if (!ttk_windows || !ttk_windows->w || !ttk_windows->w->input) return;
    
    ttk_input_char (TTK_INPUT_END);
    ttk_free_widget (ttk_windows->w->input);
    ttk_windows->w->input = 0;
}
