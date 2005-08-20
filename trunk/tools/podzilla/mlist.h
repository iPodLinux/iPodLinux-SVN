#ifndef _MLIST_H_
#define _MLIST_H_

#define NOSETTING	0

/* Menu item options */
#define STUB_MENU	0
#define LONG_ITEM	1
#define SUB_MENU_HEADER	2
#define ACTION_MENU	4
#define BOOLEAN_MENU	8
#define OPTION_MENU	16
#define SUB_MENU_PREV	32
#define SETTING_ITEM	64
#define ARROW_MENU	128
#define EXECUTE_MENU	256

/* Menu options */
#define UTF8		1		
#define ASCII		2
#define UC16		4
#define TRANSLATE	8

typedef struct _item_st {
	char *text;	/* Menu item text to display */
	void *action;	/* Optional procedure pointer */
	unsigned int op;/* Options to pass */
	int setting;	/* The setting that this item is associated with */
	int item_count;	/* Number of option items */
	int orig_pos;	/* Retains the original	pos of the item */
	int sel_option;	/* Selected Option */
	int text_width; /* The width of the text on the screen */
} item_st;

typedef struct _menu_st {
	char *title;		/* Title, duh */
	item_st *items;		/* List of items */
	int num_items, alloc_items;	/* total items and total space
					 * allocated for items */
	int top_item, lastsel, sel;	/* item at top of screen; last selected
					 * and currently selected items */
	int init;		/* if the menu has been initailized */
	int screen_items;	/* how many items will fit on the screen */
	struct _menu_st *parent;/* parent menu */
	int x, y, w, h;		/* geometry */
	int scrollbar;		/* if there are enough items to have
				 * a scrollbar */
	int *pixmap_pos;	/* position of the pixmaps on the screen */
	GR_WINDOW_ID *pixmaps;	/* pixmaps for the items */
	GR_GC_ID menu_gc;
	GR_WINDOW_ID menu_wid;

	GR_WINDOW_ID transition;

	GR_SIZE width, height, base;	/* height contains the height
					 * of the items */
	GR_TIMER_ID timer;	/* scroll timer */
	GR_FONT_ID font;	/* font the pixmaps were drawn with */
	int op;			/* menu options */
	int timer_step;
	int scheme_no;		/* which scheme the pixmaps were drawn with */
} menu_st;

/* does the drawing, safe(and recommended) to use {in, as} an exposure event */
void menu_draw(menu_st *menulist);

/* selects specified item */
void menu_select_item(menu_st *menulist, int sel);

/* selection changer, for the lazy */
int menu_shift_selected(menu_st *menulist, int num);

void menu_draw_timer(menu_st *menulist);

void menu_handle_timer(menu_st *menulist, int force);

menu_st *menu_handle_item(menu_st *menulist, int num);

void menu_update_menu(menu_st *menulist);

void menu_add_item(menu_st *menulist, char *text, void *action, int count,
		unsigned int op);
void menu_delete_item(menu_st *menulist, int num);

/* sorts items alphabetically and indexes them */
void menu_sort_menu(menu_st *menulist);

/* flash currently selected item */
void menu_flash_selected(menu_st * menulist);

menu_st *menu_init(GR_WINDOW_ID menu_wid, char *title, int x, int y, int w,
		int h, menu_st *parent, item_st *items, int op);
menu_st *menu_destroy(menu_st *menulist);

/* destroy all humans. err.. menus */
void menu_destroy_all(menu_st *menulist);

/* made public so mpdc can use it */
void quicksort(item_st a[], int lower, int upper);

#endif /* _MLIST_H_ */

