#include "bootloader.h"

#define IPOD_PP5020_RTC         0x60005010

static struct {
  uint32 hw_version;
  uint32 lcd_base,lcd_busy_mask,lcd_type;
  uint32 rtc;

  uint32 lcd_width,lcd_height;
} fb;

/* get current usec counter */
static int timer_get_current(void) {
  return inl(fb.rtc);
}

/* check if number of useconds has past */
static int timer_check(int clock_start, int usecs) {
  unsigned long clock;
  clock = inl(fb.rtc);
  
  if ( (clock - clock_start) >= usecs ) {
    return 1;
  } else {
    return 0;
  }
}


/* wait for LCD with timeout */
static void lcd_wait_write(void) {
  if ((inl(fb.lcd_base) & fb.lcd_busy_mask) != 0) {
    int start = timer_get_current();
    
    do {
      if ((inl(fb.lcd_base) & fb.lcd_busy_mask) == 0) break;
    } while (timer_check(start, 1000) == 0);
  }
}

static void lcd_send_lo(int v) {
  lcd_wait_write();
  outl(v | 0x80000000, 0x70008A0C);
}

static void lcd_send_hi(int v) {
  lcd_wait_write();
  outl(v | 0x81000000, 0x70008A0C);
}

static void lcd_cmd_data(int cmd, int data) {
  if( fb.lcd_type == 0) {
    lcd_send_lo(cmd);
    lcd_send_lo(data);
  } else {
    lcd_send_lo(0x0);
    lcd_send_lo(cmd);
    lcd_send_hi((data >> 8) & 0xff);
    lcd_send_hi(data & 0xff);
  }
}

static void fb_bitblt(uint16 *x, int sx, int sy, int mx, int my) {
  int startx = sy;
  int starty = sx;
  int height = (my - sy);
  int width  = (mx - sx);
  int rect1, rect2, rect3, rect4;
  
  unsigned short *addr = x;
  
  /* calculate the drawing region */
  if( (fb.hw_version>>16) != 0x6) {
    rect1 = starty;                 /* start horiz */
    rect2 = startx;                 /* start vert */
    rect3 = (starty + width) - 1;   /* max horiz */
    rect4 = (startx + height) - 1;  /* max vert */
  } else {
    rect1 = startx;                 /* start vert */
    rect2 = (fb.lcd_width - 1) - starty;       /* start horiz */
    rect3 = (startx + height) - 1;  /* end vert */
    rect4 = (rect2 - width) + 1;            /* end horiz */
  }
  
  /* setup the drawing region */
  if( fb.lcd_type == 0) {
    lcd_cmd_data(0x12, rect1);      /* start vert */
    lcd_cmd_data(0x13, rect2);      /* start horiz */
    lcd_cmd_data(0x15, rect3);      /* end vert */
    lcd_cmd_data(0x16, rect4);      /* end horiz */
  } else {
    /* swap max horiz < start horiz */
    if (rect3 < rect1) {
      int t;
      t = rect1;
      rect1 = rect3;
      rect3 = t;
    }
    
    /* swap max vert < start vert */
    if (rect4 < rect2) {
      int t;
      t = rect2;
      rect2 = rect4;
      rect4 = t;
    }

    /* max horiz << 8 | start horiz */
    lcd_cmd_data(0x44, (rect3 << 8) | rect1);
    /* max vert << 8 | start vert */
    lcd_cmd_data(0x45, (rect4 << 8) | rect2);
    
    if( (fb.hw_version>>16) == 0x6) {
      /* start vert = max vert */
      rect2 = rect4;
    }
    
    /* position cursor (set AD0-AD15) */
    /* start vert << 8 | start horiz */
    lcd_cmd_data(0x21, (rect2 << 8) | rect1);
    
    /* start drawing */
    lcd_send_lo(0x0);
    lcd_send_lo(0x22);
  }
  
  addr += startx * (fb.lcd_width*2) + starty;
  
  while (height > 0) {
    int x, y;
    int h, pixels_to_write;
    
    pixels_to_write = (width * height) * 2;
    
    /* calculate how much we can do in one go */
    h = height;
    if (pixels_to_write > 64000) {
      h = (64000/2) / width;
      pixels_to_write = (width * h) * 2;
    }
    
    outl(0x10000080, 0x70008a20);
    outl((pixels_to_write - 1) | 0xc0010000, 0x70008a24);
    outl(0x34000000, 0x70008a20);
    
    /* for each row */
    for (x = 0; x < h; x++) {
      /* for each column */
      for (y = 0; y < width; y += 2) {
	unsigned two_pixels;
	
	two_pixels = addr[0] | (addr[1] << 16);
	addr += 2;
	
	while ((inl(0x70008a20) & 0x1000000) == 0);
	
	/* output 2 pixels */
	outl(two_pixels, 0x70008b00);
      }
      
      addr += fb.lcd_width - width;
    }
    
    while ((inl(0x70008a20) & 0x4000000) == 0);
    
    outl(0x0, 0x70008a24);
    
    height = height - h;
  }
}

void fb_update(uint16 *x) {
  fb_bitblt(x,0,0,fb.lcd_width,fb.lcd_height);
}

void fb_cls(uint16 *x,uint16 val) {
  uint32 i;

  for(i=0;i<(fb.lcd_width*fb.lcd_height*2);i++) {
    x[i] = val;
  }
}

void fb_init(uint32 hw_ver) {

  fb.hw_version = hw_ver;
  
  switch(hw_ver>>16) {
  case 0xC:
    fb.lcd_base      = 0x70008a0c;
    fb.lcd_busy_mask = 0x80000000;
    fb.lcd_width     = 176;
    fb.lcd_height    = 132;
    fb.lcd_type      = 1;
    fb.rtc           = IPOD_PP5020_RTC;
    break;
  case 0x6:
    if (fb.hw_version == 0x60000) {
      fb.lcd_type = 0;
    } else {
      int gpio_a01, gpio_a04;
      
      /* A01 */
      gpio_a01 = (inl(0x6000D030) & 0x2) >> 1;
      /* A04 */
      gpio_a04 = (inl(0x6000D030) & 0x10) >> 4;
      
      if (((gpio_a01 << 1) | gpio_a04) == 0 || ((gpio_a01 << 1) | gpio_a04) == 2) {
	fb.lcd_type = 0;
      } else {
	fb.lcd_type = 1;
      }
    }

    fb.lcd_width     = 220;
    fb.lcd_height    = 176;
    fb.lcd_base      = 0x70008a0c;
    fb.lcd_busy_mask = 0x80000000;
    fb.rtc           = IPOD_PP5020_RTC;
    break;
  }
}
