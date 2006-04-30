#ifndef _MENU_H_
#define _MENU_H_

#include "bootloader.h"

#define MAX_MENU_ITEMS 16 // 5G model can show much more than 10 items

void menu_cls(uint16 *fb);
void menu_init();
void menu_additem(char *text);
void menu_redraw(uint16 *fb, int selectedItem, char *title, char *countDown);
void menu_drawprogress(uint16 *fb, uint8 completed);

void menu_drawrect(uint16 *fb,uint16 x1,uint16 y1,uint16 x2,uint16 y2,uint16 color);
void menu_hline (uint16 *fb, uint16 x1, uint16 x2, uint16 y, uint16 color);
void menu_vline (uint16 *fb, uint16 x, uint16 y1, uint16 y2, uint16 color);
void menu_frame (uint16 *fb, uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint16 color);

#endif
