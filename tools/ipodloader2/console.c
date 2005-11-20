#include "bootloader.h"
#include "fb.h"
#include "console.h"

#include "vga_font.h"

static struct {
  struct {
    uint32 x,y;
  } cursor;
  struct {
    uint32 w,h;
  } dimensions;
  uint16 *fb;
} console;

void console_putchar(char ch) {
  int32 r,c,x,y;

  x = console.cursor.x * VGA_FONT_WIDTH;
  y = console.cursor.y * VGA_FONT_HEIGHT;

  if(ch == '\n') { console.cursor.x = 0; console.cursor.y++; return; }
  if(ch == '\r') { console.cursor.x = 0; return; }

  // !!! Assumes RGB565

  for(r=0;r<16;r++) {
    for(c=0;c<8;c++) {
      if( (uint8)font8x16[(uint8)ch][r] & (1<<(8-c)) ) {  // Pixel set
	console.fb[(y+r)*220+x+c] = 0xFFFF;
      } else { // Pixel clear
	console.fb[(y+r)*220+x+c] = 0;
      }
    }
  }

  if( console.cursor.x > (220/VGA_FONT_WIDTH) ) {
    console.cursor.x = 0; 
    console.cursor.y++;
  } else {
    console.cursor.x += 1;
  }

  if(ch == '\n')
    fb_bitblt(console.fb,0,0,220,176);
}

void console_puts(volatile char *str) {
  while(*str != 0) {
    console_putchar(*str);
    str++;
  }
}

void console_init(uint16 *fb) {
  console.dimensions.w = 220 / VGA_FONT_WIDTH;
  console.dimensions.h = 176 / VGA_FONT_HEIGHT;
  console.cursor.x = 0;
  console.cursor.y = 0;

  console.fb = fb;
}
