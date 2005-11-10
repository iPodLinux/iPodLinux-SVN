#ifndef _TTK_SLIDER_H_
#define _TTK_SLIDER_H_

TWidget *ttk_new_slider_widget (int x, int y, int w, int min, int max, int *val, const char *title);
void ttk_slider_set_bar (TWidget *this, ttk_surface empty, ttk_surface full);
void ttk_slider_set_callback (TWidget *this, void (*cb)(int cdata, int val), int cdata);

void ttk_slider_draw (TWidget *this, ttk_surface srf);
int ttk_slider_scroll (TWidget *this, int dir);
int ttk_slider_down (TWidget *this, int button);
void ttk_slider_free (TWidget *this);

TWindow *ttk_mh_slider (struct ttk_menu_item *this);
void *ttk_md_slider (int w, int min, int max, int *val);

#endif
