#include "bootloader.h"

#define IPOD_PP5020_RTC 0x60005010

static uint32 ipod_rtc,lcd_base,lcd_busy_mask,lcd_width,lcd_height;

/* get current usec counter */
static int video_timer_get_current(void)
{
  return inl(ipod_rtc);
}

/* check if number of useconds has past */
static int video_timer_check(int clock_start, int usecs)
{
  unsigned long clock;
  clock = inl(ipod_rtc);
        
  if ((clock - clock_start) >= usecs) {
    return 1;
  } else {
    return 0;
  }
}

static void video_lcd_wait_write(void)
{
  if ((inl(lcd_base) & lcd_busy_mask) != 0) {
    int start = video_timer_get_current();
                        
    do {
      if ((inl(lcd_base) & lcd_busy_mask) == 0)
        break;
    } while (video_timer_check(start, 1000) == 0);
  }
}
static void video_lcd_cmd_data(int cmd, int data)
{
  video_lcd_wait_write();
  outl(cmd | 0x80000000, 0x70008a0c);

  video_lcd_wait_write();
  outl(data | 0x80000000, 0x70008a0c);
}


void fb_bitblt(unsigned short * x, int sx, int sy, int mx, int my) {
  int startY = sy;
  int startX = sx;

  int diff = 0;
  int height;
  int width;      
        
  unsigned short *addr;

  if (my > lcd_height) {
    my = lcd_height;
  }

  if (mx > lcd_width) {
    diff = (lcd_width - mx) * 2;
    mx = lcd_width;
  }       

  height = (my - sy);
  width = (mx - sx);

  addr = x;

  /* start X and Y */
  video_lcd_cmd_data(0x12, (startY & 0xff));
  video_lcd_cmd_data(0x13, (((lcd_width - 1) - startX) & 0xff));

  /* max X and Y */
  video_lcd_cmd_data(0x15, (((startY + height) - 1) & 0xff));
  video_lcd_cmd_data(0x16, (((((lcd_width - 1) - startX) - width) + 1) & 0xff));

  addr += startY * 220 + startX;

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
    for (y = 0x0; y < h; y++) {
      /* for each column */
      for (x = 0; x < width; x += 2) {
        unsigned two_pixels;

        two_pixels = addr[0] | (addr[1] << 16);
        addr += 2;

        while ((inl(0x70008a20) & 0x1000000) == 0);

        /* output 2 pixels */
        outl(two_pixels, 0x70008b00);
      }

      addr += diff;
      //addr += lcd_width - width;
    }

    while ((inl(0x70008a20) & 0x4000000) == 0);

    outl(0x0, 0x70008a24);

    height = height - h;
  }
}

void fb_init(int hw_ver) {
  
  switch(hw_ver) {
  case 6:
    lcd_base      = 0x70008a0c;
    lcd_busy_mask = 0x80000000;
    lcd_width     = 220;
    lcd_height    = 176;
    ipod_rtc      = IPOD_PP5020_RTC;
    break;
  }
}
