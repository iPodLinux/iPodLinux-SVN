#ifndef _MENU_H_
#define _MENU_H_

#include "bootloader.h"

void menu_init(void);
void menu_additem(char *text);
void menu_redraw(uint16 *fb,uint32 selectedItem);

#endif
