#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "bootloader.h"
#include "fontmedium.h"

void console_init(uint16 *fb);
void console_putchar(char ch);
void console_puts(volatile char *str);
void console_putsXY(int x,int y,volatile char *str);
void console_printf (const char *format, ...);

void console_setcolor(uint16 fg,uint16 bg,uint8 transparent);

#endif
