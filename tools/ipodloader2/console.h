#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "bootloader.h"
#include "fontlarge.h"
#include "fontmedium.h"
#include "fontsmall.h"

void console_init(uint16 *fb);
void console_putchar(char ch);
void console_puts(volatile char *str);
void console_putsXY(int x,int y,volatile char *str);
void console_printf (const char *format, ...);

void console_setcolor(uint16 fg, uint16 bg, uint8 transparent);
void console_getcolor(uint16 *fg, uint16 *bg, uint8 *transparent);

void console_setfont(const uint8 *font);
void console_home();
void console_clear();
int console_suppress_fbupdate (int modify);

extern int font_width, font_height;
extern const uint8 *fontdata;
extern int console_printcount;

#endif
