#include "bootloader.h"
#include "ipodhw.h"

static ipod_t *ipod;

static void lcd_send_lo(int v) {
  lcd_wait_write();
  outl(v | 0x80000000, ipod->lcd_base);
}

static void lcd_send_hi(int v) {
  lcd_wait_write();
  outl(v | 0x81000000, ipod->lcd_base);
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

uint8 LUMA565(uint16 val) {
  uint32 calc; 

  calc  = (val>>11)<<3;
  calc += ((val&(0x3F<<5))>>5)<<2;
  calc += (val&(0x1F))<<3;

  return(calc/3);
}

static void fb_2bpp_bitblt(uint16 *fb, int sx, int sy, int mx, int my) {
  int cursor_pos;
  int y;

  sx >>= 3;
  mx >>= 3;
  
  cursor_pos = 0; //sx + (sy << 5); //+1;
  
  for ( y = sy; y <= my; y++ ) {
    int x;
    
    /* move the cursor */
    lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);
    
    /* setup for printing */
    lcd_prepare_cmd(0x12);
    
    /*
     * 160/8 -> 20 == loops 20 times
     * make sure we loop at least once
     */
    for ( x = sx; x <= mx; x++ ) {
      uint8 pix[2];

      /* RGB565 to 2BPP downsampling */
      pix[0]  = (LUMA565( fb[y*ipod->lcd_width+x*8+7] ) >> 6) << 6;
      pix[0] |= (LUMA565( fb[y*ipod->lcd_width+x*8+6] ) >> 6) << 4;
      pix[0] |= (LUMA565( fb[y*ipod->lcd_width+x*8+5] ) >> 6) << 2;
      pix[0] |= (LUMA565( fb[y*ipod->lcd_width+x*8+4] ) >> 6) << 0;
      pix[1]  = (LUMA565( fb[y*ipod->lcd_width+x*8+3] ) >> 6) << 6;
      pix[1] |= (LUMA565( fb[y*ipod->lcd_width+x*8+2] ) >> 6) << 4;
      pix[1] |= (LUMA565( fb[y*ipod->lcd_width+x*8+1] ) >> 6) << 2;
      pix[1] |= (LUMA565( fb[y*ipod->lcd_width+x*8+0] ) >> 6) << 0;
      
      /* display a character */
      lcd_send_data(pix[0],pix[1]);
    }
    
    /* update cursor pos counter */
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
  } else { /* 5G */
    unsigned count = (width * height) << 1;
    lcd_bcm_setup_rect(0x34, rect1, rect2, rect3, rect4, count);
  }
  
  addr += startx * ipod->lcd_width + starty;
  
  while (height > 0) {
    int x, y;
    int h, pixels_to_write;

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
      outw ((0xE0020 & 0xffff), 0x30010000);
      outw ((0xE0020 >> 16), 0x30010000);
      while ((inw (0x30030000) & 0x2) == 0);
    }

    /* for each row */
    for (y = 0; y < h; y++) {
      /* for each column */
      for (x = 0; x < width; x += 2) {
	unsigned two_pixels;

	if( ipod->lcd_type != 5 ) {
	  two_pixels = ( ((addr[0]&0xFF)<<8) | ((addr[0]&0xFF00)>>8) ) | 
	               ((((addr[1]&0xFF)<<8) | ((addr[1]&0xFF00)>>8) )<<16);
	} else {
	  two_pixels = (addr[1]<<16) | addr[0];
	}
	
	if( ipod->lcd_type != 5 ) {

	  while ((inl(0x70008a20) & 0x1000000) == 0);
	  
	  /* output 2 pixels */
	  outl(two_pixels, 0x70008b00);
          addr += 2;
	} else {
	  /*two_pixels = ((two_pixels&0xFF000000)>>24) | ((two_pixels&0xFF0000)>>8) | 
	    ((two_pixels&0xFF00) << 8) | ((two_pixels&0xFF)<<24);*/


          /* output 2 pixels */
          outw (*addr++, 0x30000000);
          outw (*addr++, 0x30000000);
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
    fb_2bpp_bitblt(x,0,0,ipod->lcd_width-1,ipod->lcd_height-1);
}

void fb_cls(uint16 *x,uint16 val) {
  uint32 i;

  for(i=0;i<(ipod->lcd_width*ipod->lcd_height);i++) {
    x[i] = val;
  }
}

void fb_init(void) {

  int hw_ver;
  ipod = ipod_get_hwinfo();
  hw_ver = ipod->hw_rev >> 16;

  if (hw_ver == 0x4 || hw_ver == 0x7) {
    /* driver output control - 160x112 (ipod mini) */
    lcd_cmd_and_data(0x1, 0x0, 0xd);
  }
  else if (hw_ver < 0x4 || hw_ver == 0x5) {
    /* driver output control - 160x128 */
    lcd_cmd_and_data(0x1, 0x1, 0xf);
  }
  
  /* ID=1 -> auto decrement address counter */
  /* AM=00 -> data is continuously written in parallel */
  /* LG=00 -> no logical operation */
  if (hw_ver < 0x6 || hw_ver == 0x7) {
    lcd_cmd_and_data(0x5, 0x0, 0x10);
  }

#if YOU_WANT_TO_SCREW_UP_THE_COLORS_IN_RETAILOS
  if( ((ipod->hw_rev>>16) == 0x6) && (ipod->lcd_type == 0) ) {
    lcd_cmd_data(0xef,0x0);
    lcd_cmd_data(0x1,0x0);
    lcd_cmd_data(0x80,0x1);
    lcd_cmd_data(0x10,0x8);
    lcd_cmd_data(0x18,0x6);
    lcd_cmd_data(0x7e,0x4);
    lcd_cmd_data(0x7e,0x5);
    lcd_cmd_data(0x7f,0x1);
  }
#endif

}
