#include "bootloader.h"
#include "ipodhw.h"

static ipod_t *ipod;

/* get current usec counter */
static int timer_get_current(void) {
  return inl(ipod->rtc);
}

/* check if number of useconds has past */
static int timer_check(int clock_start, int usecs) {
  unsigned long clock;
  clock = inl(ipod->rtc);
  
  if ( (clock - clock_start) >= usecs ) {
    return 1;
  } else {
    return 0;
  }
}


/* wait for LCD with timeout */
static void lcd_wait_write(void) {
  if ((inl(ipod->lcd_base) & ipod->lcd_busy_mask) != 0) {
    int start = timer_get_current();
    
    do {
      if ((inl(ipod->lcd_base) & ipod->lcd_busy_mask) == 0) break;
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
  if( ipod->lcd_type == 0) {
    lcd_send_lo(cmd);
    lcd_send_lo(data);
  } else {
    lcd_send_lo(0x0);
    lcd_send_lo(cmd);
    lcd_send_hi((data >> 8) & 0xff);
    lcd_send_hi(data & 0xff);
  }
}

/* send LCD command */
static void lcd_prepare_cmd(int cmd) {
  lcd_wait_write();
  if(ipod->hw_rev >= 0x70000) {
    outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
    outl(cmd | 0x740000, 0x70003008);
  }
  else {
    outl(0x0, ipod->lcd_base + 0x8);
    lcd_wait_write();
    outl(cmd, ipod->lcd_base + 0x8);
  }
}

/* send LCD data */
static void lcd_send_data(int data_lo, int data_hi) {
  lcd_wait_write();
  if(ipod->hw_rev >= 0x70000) {
    outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
    outl(data_hi | (data_lo << 8) | 0x760000, 0x70003008);
  }
  else {
    outl(data_lo, ipod->lcd_base + 0x10);
    lcd_wait_write();
    outl(data_hi, ipod->lcd_base + 0x10);
  }
}

/* send LCD command and data */
static void lcd_cmd_and_data(int cmd, int data_lo, int data_hi) {
  lcd_prepare_cmd(cmd);

  lcd_send_data(data_lo, data_hi);
}

static void lcd_bcm_write32(unsigned address, unsigned value) {
	/* write out destination address as two 16bit values */
	outw(address, 0x30010000);
	outw((address >> 16), 0x30010000);

	/* wait for it to be write ready */
	while ((inw(0x30030000) & 0x2) == 0);

	/* write out the value low 16, high 16 */
	outw(value, 0x30000000);
	outw((value >> 16), 0x30000000);
}

static void lcd_bcm_setup_rect(unsigned cmd, unsigned start_horiz, unsigned start_vert, unsigned max_horiz, unsigned max_vert, unsigned count) {
	lcd_bcm_write32(0x1F8, 0xFFFA0005);
	lcd_bcm_write32(0xE0000, cmd);
	lcd_bcm_write32(0xE0004, start_horiz);
	lcd_bcm_write32(0xE0008, start_vert);
	lcd_bcm_write32(0xE000C, max_horiz);
	lcd_bcm_write32(0xE0010, max_vert);
	lcd_bcm_write32(0xE0014, count);
	lcd_bcm_write32(0xE0018, count);
	lcd_bcm_write32(0xE001C, 0);
}

static unsigned lcd_bcm_read32(unsigned address) {
	while ((inw(0x30020000) & 1) == 0);

	/* write out destination address as two 16bit values */
	outw(address, 0x30020000);
	outw((address >> 16), 0x30020000);

	/* wait for it to be read ready */
	while ((inw(0x30030000) & 0x10) == 0);

	/* read the value */
	return inw(0x30000000) | inw(0x30000000) << 16;
}

static void lcd_bcm_finishup(void) {
	unsigned data; 

	outw(0x31, 0x30030000); 

	lcd_bcm_read32(0x1FC);

	do {
		data = lcd_bcm_read32(0x1F8);
	} while (data == 0xFFFA0005 || data == 0xFFFF);

	lcd_bcm_read32(0x1FC);
}

inline uint8 LUMA565(uint16 val) {
  uint32 calc; 

  calc  = (val>>11)<<3;
  calc += ((val&(0x3F<<5))>>5)<<2;
  calc += (val&(0x1F))<<3;

  return(calc/3);
}

static void fb_2bpp_bitblt(uint16 *fb, int sx, int sy, int mx, int my) {
  int cursor_pos;
  int y;
  
  //ADDR8   addr = psd->addr;
  
  /* only update the ipod if we are writing to the screen */
  //if (!(psd->flags & PSF_SCREEN)) return;
  
  //assert (addr != 0);
  
  sx >>= 3;
  //mx = (mx+7)>>3;
  mx >>= 3;
  
  cursor_pos = sx + (sy << 5);
  
  for ( y = sy; y <= my; y++ ) {
    //ADDR8 img_data;
    int x;
    
    // move the cursor
    lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);
    
    // setup for printing
    lcd_prepare_cmd(0x12);
    
    //img_data = addr + (sx<<1) + (y * psd->linelen);
    
    // 160/8 -> 20 == loops 20 times
    // make sure we loop at least once
    for ( x = sx; x <= mx; x++ ) {
      uint8 pix[2];

      /* RGB565 to 2BPP downsampling */
      pix[0]  = (LUMA565( fb[y*ipod->lcd_width+x*8+0] ) >> 6) << 6;
      pix[0] |= (LUMA565( fb[y*ipod->lcd_width+x*8+1] ) >> 6) << 4;
      pix[0] |= (LUMA565( fb[y*ipod->lcd_width+x*8+2] ) >> 6) << 2;
      pix[0] |= (LUMA565( fb[y*ipod->lcd_width+x*8+3] ) >> 6) << 0;
      pix[1]  = (LUMA565( fb[y*ipod->lcd_width+x*8+4] ) >> 6) << 6;
      pix[1] |= (LUMA565( fb[y*ipod->lcd_width+x*8+5] ) >> 6) << 4;
      pix[1] |= (LUMA565( fb[y*ipod->lcd_width+x*8+6] ) >> 6) << 2;
      pix[1] |= (LUMA565( fb[y*ipod->lcd_width+x*8+7] ) >> 6) << 0;
      
      // display a character
      lcd_send_data(pix[0],pix[1]);
    }
    
    // update cursor pos counter
    cursor_pos += 0x20;
  }
}

static void fb_565_bitblt(uint16 *x, int sx, int sy, int mx, int my) {
  int startx = sy;
  int starty = sx;
  int height = (my - sy);
  int width  = (mx - sx);
  int rect1, rect2, rect3, rect4;
  
  unsigned short *addr = x;
  
  /* calculate the drawing region */
  if( (ipod->hw_rev>>16) != 0x6) {
    rect1 = starty;                 /* start horiz */
    rect2 = startx;                 /* start vert */
    rect3 = (starty + width) - 1;   /* max horiz */
    rect4 = (startx + height) - 1;  /* max vert */
  } else {
    rect1 = startx;                 /* start vert */
    rect2 = (ipod->lcd_width - 1) - starty;       /* start horiz */
    rect3 = (startx + height) - 1;  /* end vert */
    rect4 = (rect2 - width) + 1;            /* end horiz */
  }
  
  /* setup the drawing region */
  if( ipod->lcd_type == 0) {
    lcd_cmd_data(0x12, rect1);      /* start vert */
    lcd_cmd_data(0x13, rect2);      /* start horiz */
    lcd_cmd_data(0x15, rect3);      /* end vert */
    lcd_cmd_data(0x16, rect4);      /* end horiz */
  } else if( ipod->lcd_type != 5 ) {
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
    
    if( (ipod->hw_rev>>16) == 0x6) {
      /* start vert = max vert */
      rect2 = rect4;
    }
    
    /* position cursor (set AD0-AD15) */
    /* start vert << 8 | start horiz */
    lcd_cmd_data(0x21, (rect2 << 8) | rect1);
    
    /* start drawing */
    lcd_send_lo(0x0);
    lcd_send_lo(0x22);
  } else { // 5G
    unsigned count = (width * height) << 1;
    lcd_bcm_setup_rect(0x34, rect1, rect2, rect3, rect4, count);
  }
  
  addr += startx * (ipod->lcd_width*2) + starty;
  
  while (height > 0) {
    int x, y;
    int h, pixels_to_write;
    uint32 curpixel;
    
    curpixel = 0;

    if( ipod->lcd_type != 5 ) {
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
    } else {
      h = height;
    }

    /* for each row */
    for (x = 0; x < h; x++) {
      /* for each column */
      for (y = 0; y < width; y += 2) {
	unsigned two_pixels;
	
	two_pixels = addr[0] | (addr[1] << 16);
	addr += 2;
	
	if( ipod->lcd_type != 5 ) {
	  while ((inl(0x70008a20) & 0x1000000) == 0);
	  
	  /* output 2 pixels */
	  outl(two_pixels, 0x70008b00);
	} else {
          /* output 2 pixels */
          lcd_bcm_write32(0xE0020 + (curpixel << 2), two_pixels);
          curpixel++;	
	}
      }
      
      addr += ipod->lcd_width - width;
    }

    if (ipod->lcd_type != 5) {
      while ((inl(0x70008a20) & 0x4000000) == 0);
      
      outl(0x0, 0x70008a24);
    
      height = height - h;
    } else {
      height = 0;
    }
  }

  if (ipod->lcd_type == 5) {
    lcd_bcm_finishup();
  }
}

void fb_update(uint16 *x) {
  if( ipod->lcd_format == IPOD_LCD_FORMAT_RGB565 ) 
    fb_565_bitblt(x,0,0,ipod->lcd_width,ipod->lcd_height);
  else
    fb_2bpp_bitblt(x,0,0,ipod->lcd_width,ipod->lcd_height);
}

void fb_cls(uint16 *x,uint16 val) {
  uint32 i;

  for(i=0;i<(ipod->lcd_width*ipod->lcd_height*2);i++) {
    x[i] = val;
  }
}

void fb_init(void) {

  ipod = ipod_get_hwinfo();

}
