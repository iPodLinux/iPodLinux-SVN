#include "bootloader.h"
#include "fb.h"
#include "console.h"
#include "minilibc.h"
#include "ipodhw.h"

#include "vga_font.h"

static struct {
  struct {
    uint32 x,y;
  } cursor;
  struct {
    uint32 w,h;
  } dimensions;
  uint16 *fb;

  uint16  fgcolor,bgcolor;
  uint8   transparent;

  ipod_t *ipod;
} console;

void console_setcolor(uint16 fg,uint16 bg,uint8 transparent) {
  console.fgcolor     = fg;
  console.bgcolor     = bg;
  console.transparent = transparent;
}

void console_blitchar(int x,int y,char ch) {
  int r,c;

  for(r=0;r<16;r++) {
    for(c=0;c<8;c++) {
      if( (uint8)font8x16[(uint8)ch][r] & (1<<(8-c)) ) {  /* Pixel set */
	console.fb[(y+r)*console.dimensions.w+x+c] = console.fgcolor;
      } else { /* Pixel clear */
	if( !console.transparent )
	  console.fb[(y+r)*console.dimensions.w+x+c] = console.bgcolor;
      }
    }
  }
}

void console_putchar(char ch) {
  int32 x,y;

  x = console.cursor.x * VGA_FONT_WIDTH;
  y = console.cursor.y * VGA_FONT_HEIGHT;

  if(ch == '\n') { 
    console.cursor.x = 0;
    console.cursor.y++;

#if THIS_CORRUPTS_MEMORY_SOMEHOW
    /* Check if we need to scroll the display up */
    if(console.cursor.y > (console.dimensions.h/VGA_FONT_HEIGHT) ) {
      mlc_memcpy(console.fb,
		 console.fb+(console.dimensions.w*VGA_FONT_HEIGHT*2),
		 (console.dimensions.w*console.dimensions.h*2) - 
		 (console.dimensions.w*VGA_FONT_HEIGHT*2) );
    }
    fb_update(console.fb);
    return;
#else
    if( (y+VGA_FONT_HEIGHT) >= console.dimensions.h ) {
      console.cursor.x = 0;
      console.cursor.y = 0;
      x = 0;
      y = 0;
    }

#endif
  }
  if(ch == '\r') { console.cursor.x = 0; return; }

  console_blitchar(x,y,ch);

  console.cursor.x += 1;

  if( console.cursor.x > (console.dimensions.w/VGA_FONT_WIDTH) ) {
    console.cursor.x = 0; 
    console.cursor.y++;
  }

}

void console_putcharX(char ch) {
  int32 r,c,x,y;

  x = console.cursor.x * VGA_FONT_WIDTH;
  y = console.cursor.y * VGA_FONT_HEIGHT;

  if(ch == '\n') { 
    console.cursor.x = 0;
    console.cursor.y++;

#if 1
    /* Check if we need to scroll the display up */
    if(console.cursor.y > (console.dimensions.h/VGA_FONT_HEIGHT) ) {
      mlc_memcpy(console.fb,
		 console.fb+(console.dimensions.w*VGA_FONT_HEIGHT*2),
		 (console.dimensions.w*console.dimensions.h*2) - 
		 (console.dimensions.w*VGA_FONT_HEIGHT*2) );
    }
#endif
    return;
  }
  if(ch == '\r') { console.cursor.x = 0; return; }

  /* !!! Assumes RGB565 */

  for(r=0;r<16;r++) {
    for(c=0;c<8;c++) {
      if( (uint8)font8x16[(uint8)ch][r] & (1<<(8-c)) ) {  /* Pixel set */
	console.fb[(y+r)*console.dimensions.w+x+c] = 0xFFFF;
      } else { /* Pixel clear */
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
}

void console_puts(volatile char *str) {
  while(*str != 0) {
    console_putchar(*str);
    str++;
  }
}

void console_putsXY(int x,int y,volatile char *str) {
  while(*str != 0) {
    console_blitchar(x,y,*str);
    x += VGA_FONT_WIDTH;
    str++;
  }
}

void console_init(uint16 *fb) {

  console.ipod = ipod_get_hwinfo();

  console.cursor.x = 0;
  console.cursor.y = 0;
  console.dimensions.w = console.ipod->lcd_width;
  console.dimensions.h = console.ipod->lcd_height;

  console.fgcolor     = 0xFFFF;
  console.bgcolor     = 0x0000;
  console.transparent = 0x0;

  console.fb = fb;
}
