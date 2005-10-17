#include "ttk.h"

char thestring[256];
int inputting = 0;

int i_key (TWidget *this, int key) 
{
    ttk_input_char (key);
    return 0;
}

void i_draw (TWidget *this, ttk_surface srf) 
{
    ttk_text (srf, ttk_textfont, this->x + 3,
	      this->y + (this->h - ttk_text_height (ttk_textfont)) / 2,
	      ttk_makecol (BLACK),
	      "Type to input, ENTER when done.");
}

TWidget *new_basic_input() 
{
    TWidget *ret = ttk_new_widget (0, 0);
    ret->w = ttk_screen->w;
    ret->h = 18;
    ret->draw = i_draw;
    ret->down = i_key;
    ret->focusable = 1;
    ret->keyrepeat = 1;
    ret->rawkeys = 1;
    ret->dirty = 1;
    return ret;
}

void t_draw (TWidget *this, ttk_surface srf) 
{
    if (inputting && !strlen (thestring))
	ttk_text (srf, ttk_menufont, this->x, this->y, ttk_makecol (BLACK), "Nothing yet.");
    else if (inputting)
	ttk_text (srf, ttk_textfont, this->x, this->y, ttk_makecol (BLACK), thestring);
    else
	ttk_text (srf, ttk_menufont, this->x, this->y, ttk_makecol (BLACK), "Action to input."); 
}

int t_down (TWidget *this, int button) 
{
    this->dirty++;
    switch (button) {
    case TTK_BUTTON_ACTION:
	ttk_input_start (new_basic_input());
	inputting = 1;
	strcpy (thestring, "_");
	return TTK_EV_CLICK;
    case TTK_BUTTON_MENU:
	ttk_input_end();
	return TTK_EV_DONE;
    }
    return TTK_EV_UNUSED;
}

int t_input (TWidget *this, int ch)
{
    char s[3];

    this->dirty++;
    switch (ch) {
    case TTK_INPUT_ENTER:
	ttk_input_end();
	inputting = 0;
	break;
    case TTK_INPUT_END:
	inputting = 0;
	break;
    case TTK_INPUT_BKSP:
	if (strlen (thestring) >= 2)
	    strcpy (thestring + strlen (thestring) - 2, "_");
	break;
    case TTK_INPUT_LEFT:
    case TTK_INPUT_RIGHT:
	printf ("Can't deal with cursor movement");
	break;
    default:
	s[0] = ch;
	s[1] = '_';
	s[2] = 0;
	strcpy (thestring + strlen (thestring) - 1, s);
	break;
    }
    return 0;
}

int main (int argc, char *argv[]) 
{
    TWindow *win = ttk_init();
    
    ttk_menufont = ttk_get_font ("Chicago", 12);
    ttk_textfont = ttk_get_font ("Espy Sans", 10);

    TWidget *wid = ttk_new_widget (20, 20);
    wid->w = 160;
    wid->h = 100;
    wid->focusable = 1;
    wid->draw = t_draw;
    wid->down = t_down;
    wid->input = t_input;
    ttk_add_widget (win, wid);
    ttk_window_title (win, "Text Input Test");
    ttk_run();
    ttk_quit();
    return 0;
}
