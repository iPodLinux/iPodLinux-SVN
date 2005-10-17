#include "ttk.h"
#include "menu.h"
#include "slider.h"
#include "textarea.h"
#include <stdlib.h>

char TEST_Instructions[] = "Lorem-ipsum-dolor-sit-amet,\nconsectetur-adipisicing-elit,\nsed-do-eiusmod-tempor-incididunt-ut-labore-et-dolore-magna-aliqua.\n\nReallyreallyreallyreallyreallylonglonglonglonglongwordwordwordwordwordwwwwwwwwwwwwwwwwwwwwwwthisthisthisthisthis mujunmdsvjsoidfhnvosfdnv.\n\nUt enim ad minim veniam,\nquis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.\n\nDuis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.\n\nExcepteur sint occaecat cupidatat non proident,\nsunt in culpa qui officia deserunt mollit anim id est laborum.";


ttk_menu_item menu_music[] = {
    { "Artist 1", {0}, 0, 0 },
    { "Artist 2", {0}, 0, 0 },
    { "Artist 3", {0}, 0, 0 },
    { "Artist 4", {0}, 0, 0 },
    { "Artist 5", {0}, 0, 0 },
    { "Artist 6", {0}, 0, 0 },
    { "Artist 7", {0}, 0, 0 },
    { "Artist 8", {0}, 0, 0 },
    { "Some artist who has a really really, really really, ultra-extremely long name", {0}, TTK_MENU_ICON_SUB, 0 },
    { "Artist 9", {0}, 0, 0 },
    { "Artist 10", {0}, 0, 0 },
    { "Artist 11", {0}, 0, 0 },
    { "Artist 12", {0}, 0, 0 },
    { "Artist 13", {0}, 0, 0 },
    { "Artist 14", {0}, 0, 0 },
    { "Artist 15", {0}, 0, 0 },
    { "Artist 16", {0}, 0, 0 },
    { "Artist 17", {0}, 0, 0 },
    { "Artist 18", {0}, 0, 0 },
    { "Artist 19", {0}, 0, 0 },
    { "Artist 20", {0}, 0, 0 },
    { "Artist 21", {0}, 0, 0 },
    { "Artist 22", {0}, 0, 0 },
    { "Artist 23", {0}, 0, 0 },
    { "Artist 24", {0}, 0, 0 },
    { "Artist 25", {0}, 0, 0 },
    { "Artist 26", {0}, 0, 0 },
    { "Artist 27", {0}, 0, 0 },
    { "Artist 28", {0}, 0, 0 },
    { "Artist 29", {0}, 0, 0 },
    { "Artist 30", {0}, 0, 0 },
    { "Artist 31", {0}, 0, 0 },
    { "Artist 32", {0}, 0, 0 },
    { "Artist 33", {0}, 0, 0 },
    { "Artist 34", {0}, 0, 0 },
    { "Artist 35", {0}, 0, 0 },
    { "Artist 36", {0}, 0, 0 },
    { "Artist 37", {0}, 0, 0 },
    { "Artist 38", {0}, 0, 0 },
    { "Artist 39", {0}, 0, 0 },
    { "Artist 40", {0}, 0, 0 },
    { "Artist 41", {0}, 0, 0 },
    { "Artist 42", {0}, 0, 0 },
    { "Artist 43", {0}, 0, 0 },
    { "Artist 44", {0}, 0, 0 },
    { "Artist 45", {0}, 0, 0 },
    { "Artist 46", {0}, 0, 0 },
    { "Artist 47", {0}, 0, 0 },
    { "Artist 48", {0}, 0, 0 },
    { "Artist 49", {0}, 0, 0 },
    { "Artist 50", {0}, 0, 0 },
    { "Artist 51", {0}, 0, 0 },
    { "Artist 52", {0}, 0, 0 },
    { "Artist 53", {0}, 0, 0 },
    { "Artist 54", {0}, 0, 0 },
    { "Artist 55", {0}, 0, 0 },
    { "Artist 56", {0}, 0, 0 },
    { "Artist 57", {0}, 0, 0 },
    { "Artist 58", {0}, 0, 0 },
    { "Artist 59", {0}, 0, 0 },
    { 0 }
};

ttk_menu_item menu_extras[] = {
    { "Games", {0}, 0, 0 },
    { "Voice Memos", {0}, 0, 0 },
    { "and more!", {0}, 0, 0 },
    { 0 }
};

int wheel_sens = 3;

const char *backlight_settings[] = { "Off", "1/2", "On", 0 };

int main (int argc, char **argv)
{
ttk_menu_item menu_settings[] = {
    { "Backlight", {0}, 0, 0, backlight_settings, 0 },
    { "Wheel Sensitivity", {ttk_mh_slider}, TTK_MENU_ICON_SUB, ttk_md_slider (100, 1, 5, &wheel_sens) },
    { "and more!", {0}, 0, 0 },
    { 0 }
};

ttk_menu_item menu[] = {
    { "Instructions", {ttk_mh_textarea}, 0, ttk_md_textarea (TEST_Instructions, 14) },
    { "Music", {ttk_mh_sub}, TTK_MENU_ICON_SUB, ttk_md_sub (menu_music) },
    { "Extras", {ttk_mh_sub}, 0, ttk_md_sub (menu_extras) },
    { "Settings", {ttk_mh_sub}, 0, ttk_md_sub (menu_settings) },
    { 0 }
};

    TWindow *mainwindow;
    TWidget *menuwidget;
    mainwindow = ttk_init();
    ttk_menufont = ttk_get_font ("Chicago", 12);
    ttk_textfont = ttk_get_font ("Espy Sans", 10);

    menuwidget = ttk_new_menu_widget (menu, ttk_textfont, mainwindow->w, mainwindow->h);
    ttk_add_widget (mainwindow, menuwidget);
    ttk_window_title (mainwindow, "Menu Test");
    ttk_run();
}
