#include <stdlib.h>
#include <string.h>
#include "pz.h"

static const char *YESNO[] = {N_("Off"), N_("On")};
static PzConfig *_conf;

typedef struct mod {
	unsigned int uid;
	void *(* on)(void);
	void (* off)(void *);
	void *data;
	ttk_menu_item *item;
	struct mod *next;
} mod_t;

static mod_t *modlist;

static mod_t *mod_new(void *(* on)(void), void (* off)(void *),
		unsigned int uid, ttk_menu_item *item)
{
	mod_t *mod = malloc(sizeof(mod_t));
	mod->item = item;
	mod->uid = uid;
	mod->on = on;
	mod->off = off;

	mod->next = modlist;
	modlist = mod;
	return mod;
}

static void set_activation(ttk_menu_item *item, int cdata)
{
	mod_t *mod = (mod_t *)item->data;
	printf("choice = %d\n", item->choice);
	if (item->choice == 1)
		mod->data = mod->on();
	else
		mod->off(mod->data);
	pz_set_int_setting(_conf, mod->uid, item->choice);
}

static int get_activation(ttk_menu_item *item, int cdata)
{
	mod_t *mod = (mod_t *)item->data;
	return pz_get_int_setting(_conf, mod->uid);
}

static void mod_add(const char *name, void *(* on)(), void (* off)(void *),
		unsigned int uid)
{
	ttk_menu_item *item;
	char text[256];

	sprintf(text, "/Settings/Experimental/%s", name);

	item = pz_menu_add_option(text, YESNO);
	item->data = mod_new(on, off, uid, item);
	if ((item->choice = pz_get_int_setting(_conf, uid)) == 1)
		((mod_t *)item->data)->data = on();
	item->choicechanged = set_activation;
	item->choiceget = get_activation;
}

static void destroy()
{
	mod_t *o, *mod = modlist;
	while (mod) {
		if (pz_get_int_setting(_conf, mod->uid) == 1)
			mod->off(mod->data);
		//ttk_menu_remove_by_ptr(mod->item->menu, mod->item);
		//free(mod->item);

		o = mod->next;
		free(mod);
		mod = o;
	}
	pz_save_config(_conf);
	pz_free_config(_conf);
	_conf = 0;
}

extern void *quickmenu_on(void);
extern void  quickmenu_off(void *);

static void init()
{
	PzModule *module = pz_register_module("experimental", destroy);
	_conf = pz_load_config(pz_module_get_cfgpath(module, "config"));

	/* add mods here */
	mod_add("QuickMenu", quickmenu_on, quickmenu_off, 0xfa57ba4);
}

PZ_MOD_INIT(init)
