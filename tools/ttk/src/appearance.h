#ifndef _TTK_APPEARANCE_H_
#define _TTK_APPEARANCE_H_

#define TTK_AP_COLOR   1
#define TTK_AP_IMAGE   2
#define TTK_AP_SPACING 4

typedef struct TApItem
{
    char *name;
    int type; // bitwise OR of TTK_AP_* values
    ttk_color color;
    ttk_surface img;
    int spacing;
    struct TApItem *next; // used in some cases
} TApItem;

void ttk_ap_load (const char *filename);
TApItem *ttk_ap_get (const char *prop);
TApItem *ttk_ap_getx (const char *prop);
void ttk_ap_hline (ttk_surface srf, TApItem *ap, int x1, int x2, int y);
void ttk_ap_vline (ttk_surface srf, TApItem *ap, int x, int y1, int y2);
void ttk_ap_dorect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2, int filled);
void ttk_ap_rect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2);
void ttk_ap_fillrect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2);

#endif
