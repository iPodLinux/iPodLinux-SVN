#include "bootloader.h"
#include "fb.h"
#include "console.h"
#include "minilibc.h"

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

  if(ch == '\n') { 
    console.cursor.x = 0;
    console.cursor.y++;

#if 1
    if(console.cursor.y > (console.dimensions.h/VGA_FONT_HEIGHT) ) { // Scrolling needed
      mlc_memcpy(console.fb,
		 console.fb+(console.dimensions.w*VGA_FONT_HEIGHT*2),
		 (console.dimensions.w*console.dimensions.h*2) - 
		 (console.dimensions.w*VGA_FONT_HEIGHT*2) );
    }
#endif
    return;
  }
  if(ch == '\r') { console.cursor.x = 0; return; }

  // !!! Assumes RGB565

  for(r=0;r<16;r++) {
    for(c=0;c<8;c++) {
      if( (uint8)font8x16[(uint8)ch][r] & (1<<(8-c)) ) {  // Pixel set
	console.fb[(y+r)*console.dimensions.w+x+c] = 0xFFFF;
      } else { // Pixel clear
	console.fb[(y+r)*console.dimensions.w+x+c] = 0;
      }
    }
  }

  if( console.cursor.x > (console.dimensions.w/VGA_FONT_WIDTH) ) {
    console.cursor.x = 0; 
    console.cursor.y++;
  } else {
    console.cursor.x += 1;
  }

  if(ch == '\n')
    fb_update(console.fb);

  //fb_update(console.fb);
  //fb_bitblt(console.fb,0,0,220,176);
}

void console_puts(volatile char *str) {
  while(*str != 0) {
    console_putchar(*str);
    str++;
  }
}

void console_init(uint16 *fb,uint32 hw_ver) {

  switch(hw_ver>>16) {
  case 0x6:
    console.dimensions.w = 220;// / VGA_FONT_WIDTH;
    console.dimensions.h = 176;// / VGA_FONT_HEIGHT;
    console.cursor.x = 0;
    console.cursor.y = 0;
    break;
  case 0xC: // 0xC doesnt work!?
    console.dimensions.w = 176;// / VGA_FONT_WIDTH;
    console.dimensions.h = 132;// / VGA_FONT_HEIGHT;
    console.cursor.x = 0;
    console.cursor.y = 0;
    break;
  }
  console.fb = fb;
}