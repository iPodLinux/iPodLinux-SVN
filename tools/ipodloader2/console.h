#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "bootloader.h"

void console_init(uint16 *fb,uint32 hw_ver);
void console_putchar(char ch);
void console_puts(volatile char *str);
void console_printf (const char *format, ...);

#endif