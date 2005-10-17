#ifndef _TTK_TEXTAREA_H_
#define _TTK_TEXTAREA_H_

TWidget *ttk_new_textarea_widget (int w, int h, const char *text, ttk_font font, int baselineskip);

void ttk_textarea_draw (TWidget *this, ttk_surface srf);
int ttk_textarea_scroll (TWidget *this, int dir);
int ttk_textarea_down (TWidget *this, int button);
void ttk_textarea_free (TWidget *this);

TWindow *ttk_mh_textarea (struct ttk_menu_item *this);
void *ttk_md_textarea (char *text, int baselineskip);

#endif

