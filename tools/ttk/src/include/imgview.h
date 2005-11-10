#ifndef _TTK_IMGVIEW_H_
#define _TTK_IMGVIEW_H_

TWidget *ttk_new_imgview_widget (int w, int h, ttk_surface img);

void ttk_imgview_draw (TWidget *this, ttk_surface srf);
int ttk_imgview_scroll (TWidget *this, int dir);
int ttk_imgview_down (TWidget *this, int button);
void ttk_imgview_free (TWidget *this);

TWindow *ttk_mh_imgview (struct ttk_menu_item *this);
void *ttk_md_imgview (ttk_surface srf);

#endif
