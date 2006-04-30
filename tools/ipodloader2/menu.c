#include "bootloader.h"
#include "console.h"
#include "fb.h"
#include "menu.h"
#include "minilibc.h"
#include "ipodhw.h"

static struct {
  ipod_t *ipod;
  int    numItems;
  char   *string[MAX_MENU_ITEMS];
  int    x,y,w,h,fh;
} menu;

/* Clears the screen to a nice black to blue gradient */
void menu_cls(uint16 *fb) {
  uint16 x,y;
  if (menu.ipod->lcd_is_grayscale) {
    fb_cls (fb, 0);
  } else {
    for(y=0;y<(menu.ipod->lcd_height);y++) {
      for(x=0;x<(menu.ipod->lcd_width);x++) {
        fb[y * menu.ipod->lcd_width + x] = fb_rgb (0, 0, (y*255) / menu.ipod->lcd_height);
      }
    }
  }
}

void menu_drawrect(uint16 *fb,uint16 x1,uint16 y1,uint16 x2,uint16 y2,uint16 color) {
  int x,y;
  for(y=y1;y<=y2;y++) {
    for(x=x1;x<=x2;x++) {
      fb[y * menu.ipod->lcd_width + x] = color;
    }
  }
}

void menu_hline (uint16 *fb, uint16 x1, uint16 x2, uint16 y, uint16 color) {
  int x;
  for (x=x1; x<=x2; x++) {
    fb[y * menu.ipod->lcd_width + x] = color;
  }
}

void menu_vline (uint16 *fb, uint16 x, uint16 y1, uint16 y2, uint16 color) {
  int y;
  for (y=y1; y<=y2; y++) {
    fb[y * menu.ipod->lcd_width + x] = color;
  }
}

void menu_frame (uint16 *fb, uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint16 color) {
  menu_hline(fb,x1,x2,y1,color);
  menu_hline(fb,x1,x2,y2,color);
  menu_vline(fb,x1,y1,y2,color);
  menu_vline(fb,x2,y1,y2,color);
}

static void menu_recenter() {
  int i;

  menu.w = 0;
  for(i = 0; i < menu.numItems; i++) {
    if(menu.w < (mlc_strlen (menu.string[i]) << 3))
      menu.w = mlc_strlen (menu.string[i]) << 3;
  }
  menu.w += 6;
  menu.fh = 16;
  menu.h = menu.numItems * 20;
  if(menu.h > (menu.ipod->lcd_height - 50)) {
    menu.fh = 8;
    menu.h = menu.numItems * 12;
  }
  if(menu.w < (menu.ipod->lcd_width*3/5))
    menu.w = menu.ipod->lcd_width*3/5;
  if(menu.h < (menu.ipod->lcd_height*2/5))
    menu.h = menu.ipod->lcd_height*2/5;
  menu.x = (menu.ipod->lcd_width - menu.w) >> 1;
  menu.y = ((menu.ipod->lcd_height - menu.h - (menu.fh + 6)) >> 1) + menu.fh + 6;
}

void menu_init() {
  menu.ipod = ipod_get_hwinfo();
  menu.numItems = 0;
}

void menu_additem(char *text) {
  if(menu.numItems < MAX_MENU_ITEMS)
    menu.string[menu.numItems++] = text;
  menu_recenter();
}

void menu_drawprogress(uint16 *fb,uint8 completed) {
  uint16 pbarWidth;
  static char tmpBuff[40];

  pbarWidth = (menu.ipod->lcd_width - 20);

  menu_cls(fb);

  menu_drawrect( fb,10,(menu.ipod->lcd_height>>1)-5,
		 10+pbarWidth,(menu.ipod->lcd_height>>1)+5,
                 WHITE );
  menu_drawrect( fb,10,(menu.ipod->lcd_height>>1)-5,
                 10+(completed*pbarWidth)/255,(menu.ipod->lcd_height>>1)+5,
                 (menu.ipod->lcd_is_grayscale? BLACK : GREEN) );

  console_putsXY(1,1,tmpBuff);
  fb_update(fb);
}

void menu_redraw(uint16 *fb, int selectedItem, char *title, char *countDown) {
  int i;
  int line_height = menu.fh + 4;
  uint16 fg, bg;
  uint8 tp;

  const uint8 *menu_font = (menu.fh == 16)? font_large : font_medium;
  console_setfont (menu_font);

  /*fb_cls(fb,0x1F << 11);*/
  menu_cls(fb);

  console_getcolor(&fg, &bg, &tp);
  console_setcolor(BLACK, WHITE, 1);

  console_putsXY(2,2,title);
  console_putsXY(menu.ipod->lcd_width-2-mlc_strlen(countDown)*font_width,2,countDown);
  menu_hline(fb, 2, menu.ipod->lcd_width-2, menu.fh+2, BLACK);
  menu_frame(fb, menu.x-2, menu.y-2, menu.x+menu.w+1, menu.y+menu.h+1, BLACK);

  for(i=0;i<menu.numItems;i++) {
    if( i==selectedItem ) {
      if( menu.ipod->hw_rev < 0x60000 || (menu.ipod->hw_rev>>16 == 0x7) ) {
        console_setcolor (WHITE, BLACK, 0);
        menu_drawrect(fb, menu.x, menu.y+i*line_height, menu.x+menu.w-1, menu.y+(i+1)*line_height-1, BLACK);
      } else {
        console_setcolor(BLACK, menu.ipod->lcd_is_grayscale?WHITE:MENU_HILIGHT, 0);
        menu_drawrect(fb, menu.x, menu.y+i*line_height, menu.x+menu.w-1, menu.y+(i+1)*line_height-1, MENU_HILIGHT);
      }
    } else {
      console_setcolor(BLACK, WHITE, 1);
    }
    console_putsXY(menu.x+2, menu.y+i*line_height+2, menu.string[i]);
  }

  console_setcolor(fg, bg, tp);
}
