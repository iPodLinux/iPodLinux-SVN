#include "bootloader.h"
#include "console.h"
#include "fb.h"
#include "menu.h"

#define MAX_ITEMS 4

static struct {
  uint32  numItems;
  char   *string[MAX_ITEMS];
} menu;

void menu_init(void) {
  menu.numItems = 0;
}

void menu_additem(char *text) {
  if(menu.numItems < MAX_ITEMS)
    menu.string[menu.numItems++] = text;
}

void menu_redraw(uint16 *fb,uint32 selectedItem) {
  int i;

  fb_cls(fb,0x1F << 11);

  console_putsXY(0,100+selectedItem*16,"*");

  for(i=0;i<menu.numItems;i++) {
    console_putsXY(10,100+i*16,menu.string[i]);
  }
}
