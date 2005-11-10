#define TTK_ICON_WIDTH   8
#define TTK_ICON_HEIGHT  13
extern unsigned char ttk_icon_sub[], ttk_icon_spkr[], ttk_icon_play[], ttk_icon_pause[], ttk_icon_exe[],
    ttk_icon_battery[], ttk_icon_charging[], ttk_icon_hold[];

void ttk_draw_icon (unsigned char *icon, ttk_surface srf, int x, int y, int inv);
