#include "bootloader.h"
#include "fb.h"
#include "console.h"
#include "minilibc.h"
#include "ipodhw.h"

/*#include "vga_font.h"*/

int font_height, font_width;
const uint8 *fontdata;

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

void console_home() 
{
  console.cursor.x = 0;
  console.cursor.y = 0;
}

void console_setfont(const uint8 *font) {
  font_width  = font[0];
  font_height = font[1];
  fontdata    = font + 2;
}

void console_blitchar(int x,int y,char ch) {
  int r,c;

  for(r=0;r<font_height;r++) {
    for(c=0;c<font_width;c++) {
      if( (uint8)fontdata[(uint8)ch * font_height + r] & (1<<(8-c)) ) {  /* Pixel set */
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

  x = console.cursor.x * font_width;
  y = console.cursor.y * font_height;

  if(ch == '\n') { 
    console.cursor.x = 0;
    console.cursor.y++;

    /* Check if we need to scroll the display up */
    if(console.cursor.y >= (console.dimensions.h/font_height) ) {
#if 0
      int i;
      mlc_memcpy(console.fb,
		 console.fb+(console.dimensions.w*font_height),
		 (console.dimensions.w*console.dimensions.h*2) - 
		 (console.dimensions.w*font_height*2) );
      for (i = console.dimensions.w*(console.dimensions.h-font_height); i < console.dimensions.w*console.dimensions.h; i++) {
          console.fb[i] = console.bgcolor;
      }
      
      console.cursor.y--;
#else
      console.cursor.y = 0;
      x = y = 0;
      fb_cls (console.fb, console.bgcolor);
#endif
    }
    fb_update(console.fb);
#ifdef MSGDELAY
    unsigned int start = inl (0x60005010);
    while (inl (0x60005010) < start + MSGDELAY)
        ;
#endif
    return;
  }
  if(ch == '\r') { console.cursor.x = 0; return; }

  console_blitchar(x,y,ch);

  console.cursor.x += 1;

  if( console.cursor.x > (console.dimensions.w/font_width) ) {
    console.cursor.x = 0; 
    console.cursor.y++;
  }

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
    x += font_width;
    str++;
  }
}

void console_init(uint16 *fb) {
  const uint8 *font;

  console.ipod = ipod_get_hwinfo();

  console.cursor.x = 0;
  console.cursor.y = 0;
  console.dimensions.w = console.ipod->lcd_width;
  console.dimensions.h = console.ipod->lcd_height;

  if (console.dimensions.w < 300)
    font = font_medium;
  else
    font = font_large;

  font_width = font[0];
  font_height = font[1];
  fontdata = font + 2;

  console.fgcolor     = 0xFFFF;
  console.bgcolor     = 0x0018;
  console.transparent = 0x0;

  console.fb = fb;
}
