#include "ttk.h"
#include "menu.h"
#include "textarea.h"
#include "appearance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _MAKETHIS textarea_data *data = (textarea_data *)this->data
extern ttk_screeninfo *ttk_screen;

typedef struct _textarea_data
{
    char *text;
    ttk_font font;
    int baselineskip;

    ttk_surface textsrf;
    int textsrfheight, scroll, spos, sheight, top;
    int epoch;
} textarea_data;


static void render (TWidget *this) 
{
    _MAKETHIS;
    int wid = this->w - 2;
    int line;
    char *End;

    // *wrapat[] stores pointers to the char we wrap *after* on each line.
    // (If it's a space, tab, or nl, it's eaten.)
    char *wrapat[10002];
    // Number of lines. Used in computing the size of our surface.
    int lines;
    // Current EOL (pointer in wrapat[])
    char **end;
    // Current char
    char *p;
    // Current beginning-of-word --> candidate EOL
    char *wend;
    // Current xpos, wordwidth
    int xpos, wordwidth;

 wrapit:
    lines = 0;
    end = wrapat;
    p = data->text;
    wend = 0;
    xpos = wordwidth = 0;

    // The text width functions should be very inexpensive, so we can just
    // do them per-char and be simple.

    while (*p) {
	char s[2] = { '?', 0 };
	s[0] = *p;

	if (lines > 10000) {
	    fprintf (stderr, "Too many lines; showing only first 10,000.");
	    *end++ = 0;
	    break;
	}
	
	if (*p == ' ' || *p == '\t' || *p == '-' || *p == '\n') {
	    int olines = lines;
	    
	    if (wend) {
		s[0] = *wend;
		xpos += ttk_text_width (data->font, s) * (1 + 4*(*wend == '\t'));
	    }
	    xpos += wordwidth;
	    
	    if (wend && (xpos > wid)) {
		*end++ = wend;
		lines++;
		xpos = wordwidth;
		wend = 0;
	    }
	    if (*p == '\n') {
		*end++ = p;
		lines++;
		xpos = 0;
		wend = 0;
	    }
	    
	    if (wordwidth > wid) {
		while (wordwidth > wid) {
		    int wwid = 0;
		    char *q;

		    if (!wend) wend = *(end-2);

		    for (q = wend + 1; wwid <= wid && q < p && *q; q++) {
			s[0] = *q;
			wwid += ttk_text_width (data->font, s);
		    }
		    wend = q - 2; // break after the last OK letter
		    wordwidth -= wwid;
		    *end++ = wend;
		    lines++;
		    xpos = 0;
		}
		xpos = 0;
		p--; // recheck this space/hyphen/whatever next time around
	    }
	    
	    if (olines == lines) // not NL
		wend = p;
	    
	    wordwidth = 0;
	} else {
	    wordwidth += ttk_text_width (data->font, s);
	}
	
	if (((lines * 20) > this->h) && (wid == (this->w - 2))) {
	    wid -= 11; // scrollbar
	    goto wrapit;
	}
	
	p++;
    }
    *end = 0;
    
    line = 0;
    End = data->text + strlen (data->text);

    data->textsrf = ttk_new_surface (wid, (lines + 1) * data->baselineskip, ttk_screen->bpp);

    end = wrapat;
    p = data->text;
    while (p < End) {
	if (!*end) {
	    ttk_text (data->textsrf, data->font, 0, line * data->baselineskip,
		      ttk_makecol (BLACK), p);
	    break;
	} else {
	    char *nextstart = *end + 1;
	    char svch = *nextstart;
	    *nextstart = 0;
	    ttk_text (data->textsrf, data->font, 0, line * data->baselineskip,
		      ttk_makecol (BLACK), p);
	    if ((**end != ' ') && (**end != '\t') && (**end != '\n') && (**end != '-')) { // midword brk
		ttk_pixel (data->textsrf, wid - 2,
			   line * data->baselineskip + (data->baselineskip / 2),
			   ttk_makecol (BLACK));
	    }
	    p = nextstart;
	    *nextstart = svch;
	    line++;
	    end++;
	}
    }
    
    data->textsrfheight = (lines + 1) * data->baselineskip;
    data->scroll = (data->textsrfheight > this->h);
    data->sheight = this->h * (this->h + ttk_ap_getx ("header.line") -> spacing) / data->textsrfheight;
    if (data->sheight < 3) data->sheight = 3;
    data->spos = 0;
}


TWidget *ttk_new_textarea_widget (int w, int h, const char *ctext, ttk_font font, int baselineskip) 
{
    TWidget *ret = ttk_new_widget (0, 0);
    textarea_data *data = calloc (sizeof(textarea_data), 1);
    
    ret->w = w;
    ret->h = h;
    ret->data = data;
    ret->focusable = 1;
    ret->draw = ttk_textarea_draw;
    ret->down = ttk_textarea_down;
    ret->scroll = ttk_textarea_scroll;
    ret->destroy = ttk_textarea_free;

    data->text = strdup (ctext);
    data->font = font;
    data->baselineskip = baselineskip;
    data->top = 0;
    data->epoch = ttk_epoch;

    render (ret);

    ret->dirty = 1;
    return ret;
}


void ttk_textarea_draw (TWidget *this, ttk_surface srf) 
{
    _MAKETHIS;
    int wid = this->w - 10*data->scroll;

    if (data->epoch < ttk_epoch) {
	data->font = ttk_textfont;
	data->baselineskip = ttk_text_height (ttk_textfont) + 2;
	render (this);
	data->epoch = ttk_epoch;
    }

    ttk_blit_image_ex (data->textsrf, 0, data->top, wid, this->h,
		       srf, this->x + 2, this->y);

    if (data->scroll) {
	ttk_ap_fillrect (srf, ttk_ap_get ("scroll.bg"), this->x + this->w - 10,
			 this->y + ttk_ap_getx ("header.line") -> spacing,
			 this->x + this->w - 1, this->y + this->h - 1);
	ttk_ap_rect (srf, ttk_ap_get ("scroll.box"), this->x + this->w - 10,
		     this->y + ttk_ap_getx ("header.line") -> spacing,
		     this->x + this->w - 1, this->y + this->h - 1);
	ttk_ap_fillrect (srf, ttk_ap_get ("scroll.bar"), this->x + this->w - 10,
			 this->y + ttk_ap_getx ("header.line") -> spacing + data->spos,
			 this->x + this->w - 1,
			 this->y - ttk_ap_getx ("header.line") -> spacing + data->spos + data->sheight);	
#if 0
	ttk_rect (srf, this->x + this->w - 10, this->y - 1, this->x + this->w - 1,
		  this->y + this->h - 1, ttk_makecol (BLACK));
	ttk_fillrect (srf, this->x + this->w - 8, this->y + data->spos + 1, this->x + this->w - 3,
		      this->y + data->spos + data->sheight, ttk_makecol (DKGREY));
#endif
    }
}


int ttk_textarea_scroll (TWidget *this, int dir) 
{
    _MAKETHIS;
 
    int oldtop = data->top;
   
    if (!data->scroll) return 0;

    data->top += 5*dir;

    // These two must be in this order and must not be else-ifs.
    if (data->top > (data->textsrfheight - this->h))
	data->top = data->textsrfheight - this->h;
    if (data->top < 0)
	data->top = 0;
    
    data->spos = data->top * (this->h + ttk_ap_getx ("header.line") -> spacing) / data->textsrfheight;

    this->dirty++;

    if (oldtop != data->top)
	return TTK_EV_CLICK;
    return 0;
}


int ttk_textarea_down (TWidget *this, int button) 
{
    int ret = 0;
    
    switch (button) {
    case TTK_BUTTON_MENU:
	if (ttk_hide_window (this->win) == -1) {
	    ttk_quit();
	    exit (0);
	}
	break;
    case TTK_BUTTON_PREVIOUS:
	ttk_textarea_scroll (this, -20);
	ret |= TTK_EV_CLICK;
	break;
    case TTK_BUTTON_NEXT:
	ttk_textarea_scroll (this, 20);
	ret |= TTK_EV_CLICK;
	break;
    default:
	ret |= TTK_EV_UNUSED;
	break;
    }
    return ret;
}


void ttk_textarea_free (TWidget *this) 
{
    _MAKETHIS;
    ttk_free_surface (data->textsrf);
    free (data->text);
    free (data);
}


TWindow *ttk_mh_textarea (struct ttk_menu_item *this) 
{
    _MAKETHIS;
    
    TWindow *ret = ttk_new_window();
    ttk_add_widget (ret, ttk_new_textarea_widget (this->menuwidth, this->menuheight,
						  data->text, ttk_textfont, data->baselineskip));
    ttk_window_title (ret, this->name);
    ret->widgets->v->draw (ret->widgets->v, ret->srf);
    free (data);
    return ret;
}

void *ttk_md_textarea (char *text, int baselineskip) 
{
    textarea_data *data = calloc (sizeof(textarea_data), 1);
    data->text = text;
    data->baselineskip = baselineskip;
    return (void *)data;
}
