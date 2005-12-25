#include "bootloader.h"
#include "console.h"
#include "fb.h"
#include "menu.h"
#include "minilibc.h"
#include "ipodhw.h"

#define MAX_ITEMS 10

static struct {
  ipod_t *ipod;
  uint32  numItems;
  char   *string[MAX_ITEMS];
} menu;

/* Clears the screen to a nice black to blue gradient */
static void menu_cls(uint16 *fb) {
  uint32 x,y,tmp;
  
  for(y=0;y<(menu.ipod->lcd_height);y++) {
    for(x=0;x<(menu.ipod->lcd_width);x++) {
      tmp  = y;
      tmp /= (menu.ipod->lcd_height/29);
      fb[y * menu.ipod->lcd_width + x] = (y*31) / menu.ipod->lcd_height;  /* BRG * */
    }
  }
}

static void menu_drawrect(uint16 *fb,uint32 x1,uint32 y1,uint32 x2,uint32 y2,uint16 color) {
  uint32 x,y;
  
  for(y=y1;y<=y2;y++) {
    for(x=x1;x<=x2;x++) {
      fb[y * menu.ipod->lcd_width + x] = color;
    }
  }
}

void menu_init(void) {
  menu.ipod = ipod_get_hwinfo();
  menu.numItems = 0;
}

void menu_additem(char *text) {
  if(menu.numItems < MAX_ITEMS)
    menu.string[menu.numItems++] = text;
}

void menu_drawprogress(uint16 *fb,uint8 completed) {
  uint32 pbarWidth;
  static char tmpBuff[40];

  pbarWidth = (menu.ipod->lcd_width - 20);

  menu_cls(fb);

  menu_drawrect(fb,10,(menu.ipod->lcd_height>>1)-5,
		   10+pbarWidth,(menu.ipod->lcd_height>>1)+5,0x0000);
  menu_drawrect(fb,10,(menu.ipod->lcd_height>>1)-5,
		   10+(completed*pbarWidth)/255,(menu.ipod->lcd_height>>1)+5,63<<5);

  console_putsXY(1,1,tmpBuff);
  fb_update(fb);
}

void menu_redraw(uint16 *fb,uint32 selectedItem) {
  int i;

  /*fb_cls(fb,0x1F << 11);*/
  menu_cls(fb);

  console_setcolor(0xFFFF,0x0000,0x1);

  console_putsXY(0,0,"iPL Loader 2.0");

  for(i=0;i<menu.numItems;i++) {
    if( i==selectedItem ) {
      console_setcolor(0xFFFF,26,0x0);
      console_putsXY(10,20+i*font_height,menu.string[i]);
    } else {
      console_setcolor(0xFFFF,0x0000,0x1);
      console_putsXY(10,20+i*font_height,menu.string[i]);
    }
  }
}
