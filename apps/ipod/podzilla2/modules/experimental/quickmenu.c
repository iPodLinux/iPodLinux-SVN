#include <stdio.h>
#include "pz.h"

static int active;

static void calendar()
{
	PzWindow *(* l_new_calendar_window)(void);
	l_new_calendar_window = pz_module_softdep("calendar",
			"new_calendar_window");
	if (l_new_calendar_window)
		ttk_show_window(l_new_calendar_window());
}


static void clock() // 23:18 < BleuLlama> CLOCKS!
{
	PzWindow *(* l_new_local_clock_window)(void);
	l_new_local_clock_window = pz_module_softdep("clocks",
			"new_local_clock_window");
	if (l_new_local_clock_window)
		ttk_show_window(l_new_local_clock_window());
}

struct {
	char *name;
	void (* action)();
} menuitems[] = {
	{"Poweroff iPod", pz_ipod_powerdown},
	{"Clock", clock},
	{"Calendar", calendar},
	{0, 0}
};

static TWindow *do_action(ttk_menu_item *item)
{
	void (* action)() = item->data;
	active = 0;
	(* action)();
	return TTK_MENU_UPONE;
}


static TWidget *quickmenu()
{
	int i;
	TWidget *ret = ttk_new_menu_widget(NULL, ttk_menufont,
			ttk_screen->w / 2, (ttk_text_height(ttk_menufont)+4)*3);

	for (i = 0; menuitems[i].name; ++i) {
		ttk_menu_item *item = calloc(1, sizeof(ttk_menu_item));
		item->name = menuitems[i].name;
		item->data = menuitems[i].action;
		item->makesub = do_action;
		ttk_menu_append(ret, item);
	}
	return ret;
}

static TWindow *menu_create()
{
	TWindow *ret = pz_new_window("QuickMenu", PZ_WINDOW_POPUP);
	ttk_add_widget(ret, quickmenu());
	return pz_finish_window(ret);
}

static void held_handler()
{
	active = 1;
	ttk_show_window(menu_create());
}

void *quickmenu_on()
{
	pz_register_global_hold_button(PZ_BUTTON_PLAY, 500, held_handler);
	return NULL;
}


void quickmenu_off(void *null)
{
	pz_unregister_global_hold_button(PZ_BUTTON_PLAY);
}
