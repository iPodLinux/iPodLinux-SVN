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

void lcd_5g_write(unsigned R0, unsigned R1, unsigned R2, unsigned R3) {
  unsigned LR, R12, R4, R5;
  unsigned data[4] = {R3, R4, LR, 0};     
  R5 = R3;
  LR = R3 &~ 0x1; //clear bit 1 of r3
  R12 = (R0 & 0x3) | (R3 & ~0x1);  
  if (R12!=0)
    return;
  if (R3!=0)
    {
      LR = 0x30050000;
      R3 = 0x30040000;
      R12 = 0x30070000;
    } else {
      LR = 0x30010000;
      R12 = 0x30030000;
      R3 = 0x30000000;
    }
  
  R4 = R2 >> 2;
  outw(R0, LR);
  R0 = R0 >> 16;
  outw(R0, LR);
  
  // data on short boundary and bytes to write > 16
  if( ((R1 & 0x1) != 0) && (R2 >= 0x10))
    { 
      R0 = R2 >> 31; //ASR
      R0 = R2 + R0 >> 30; //LSR
      R2 = R0 >> 2; // ASR 
      while (R2 > 0)
	{
	  while (inw(R12) & 0x2 == 0);
	  R0 = inw(R1);
	  outw(R0, R3);
	  R0 = R1 + 2;
	  R1 = inw(R1+2);
	  outw(R1, R3);
	  R1 = R0 + 2;
	  R2--;
	}
    } else {
      char R9, R10;
      char * ptr1;    
      R2 = 0;
      ptr1 = R1;      
      while (R2 < R4)
	{
	  
	  R5 = *ptr1 | (*(ptr1+1)  << 8);
	  ptr1 += 2;      
	  while (inw(R12) & 0x2 == 0);
	  
	  outw(R5, R3);
	  R5 = *ptr1 | (*(ptr1+1)  << 8);
	  ptr1 += 2;      
	  
	  outw(R5, R3);
	  R1 = R1 + 4;
	  R2 = R2 + 1;
	}               
    } 
}

void lcd_5g_read(R0, R1, R2, R3)
{
  unsigned R12, LR, R4, R5;
  unsigned data[2];
  unsigned outbuf;
  outbuf = R1;
  
  LR = R3 &~1;
  R12 = R1 & 0x3;
  R12 = (R1 & 0x3) | (R3 & ~0x1);
  if (R12 != 0)
    return;
  if (R3!=0)
    {
      LR = 0x30060000;
      R3 = 0x30040000;
      R12 = 0x30070000;
    } else {
      LR = 0x30020000;
      R3 = 0x30000000;
      R12 = 0x30030000;
    }
  R5 = R2 >> 2;
  
  
  while (inw(LR) & 1 == 0);
  outw(R1, LR);
  R1 = R1 >> 16;
  outw(R1, LR);
  
#if 1
  if (R0 & 1 != 0) // short boundary?
    {
      
      char * ptr = outbuf;
      R1 = &data; //SP;
      R2 = 0;
      while (R2 < R5)
	{
	  while (inw(R12) & 0x10 == 0);

	  
	  R4 = inw(R3);
	  *ptr = R4 & 0xF;
	  *(ptr+1) = (R4 >> 8) & 0xF;
	  ptr+=2;
	  R4 = inw(R3);
	  *ptr = R4 & 0xF;
	  *(ptr+1) = (R4 >> 8) & 0xF;
	  ptr+=2; 
	  R2++;
	  /*      R4 = inh(R3);
		  data[0]= R4;
		  R4 = inb(R1);
		  outb(R4, R0);
		  R4 = inb(R1+1);
		  outb(R4, R0+1); 
		  R4 = inh(R3);
		  data[0] = R4;
		  R4 = inb(R1);
		  R2+=1;
		  outb(R4, R0+0x2);
		  R4 = inb(R1+1);
		  outb(R4, R0+0x3);
		  R0 += 4; */             
	  
	}
    } else {
      R1 = 0;
      
      while (R1 < R5)
	{
	  
	  while (inw(R12) & 0x10 == 0);
	  
	  R2 = inw(R3);
	  outw(R2, R0);
	  R2 = inw(R3);
	  outw(R2, R0+2);
	  R0+=4;
	  R1+=1;
	}
    }
#endif
  R0 = inw(LR);
  data[0] = R0;
  R0 = 0;
}


void lcd_5g_finishup(void) {
  unsigned data[2]; 
  outw(0x31, 0x30030000); 
  lcd_5g_read(data, 0x1FC, 4, 0);
  
  lcd_5g_read(&data[1], 0x1F8, 4, 0);
  while( (data[1]==0xFFFA0005) || (data[1]==0xFFFF) ) {
    lcd_5g_read(&data[1], 0x1F8, 4, 0);
  }  
  lcd_5g_read(data, 0x1FC, 4, 0);
}


void lcd_5g_setup_rect(unsigned cmd, unsigned start_horiz, unsigned start_vert,unsigned max_horiz, unsigned max_vert, unsigned count) {
        
  unsigned data[8] = {0xFFFA0005, cmd, start_horiz, start_vert, 
		      max_horiz, max_vert, count, 0}; 
  lcd_5g_write(0x1F8, &data, 4, 0);
  lcd_5g_write(0xE0000, &data[1], 4, 0);
  lcd_5g_write(0xE0004, &data[2], 4, 0);
  lcd_5g_write(0xE0008, &data[3], 4, 0);
  lcd_5g_write(0xE000C, &data[4], 4, 0);
  lcd_5g_write(0xE0010, &data[5], 4, 0);
  lcd_5g_write(0xE0014, &data[6], 4, 0);
  lcd_5g_write(0xE0018, &data[6], 4, 0);
  lcd_5g_write(0xE001C, &data[7], 4, 0);
}


void lcd_5g_draw_2pixels(unsigned two_pixels, unsigned y) {
  unsigned data[3] = {two_pixels, y, 0};
  unsigned cmd = 0xE0020 + (y << 2);
  lcd_5g_write(cmd, data, 4, 0); 
}


/* send LCD command */
static void lcd_prepare_cmd(int cmd) {
  lcd_wait_write();
  if(ipod->hw_rev >= 70000) {
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
  if(ipod->hw_rev >= 70000) {
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

typedef struct {
  union {
   uint8 p1 : 2,
	 p2 : 2,
	 p3 : 2,	  
	 p4 : 2;
   uint8 aggregate;
  } pix;
} bpp_t;

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
      bpp_t pixels[2];

      /* RGB565 to 2BPP downsampling */

      pixels[0].pix.p1 = ((fb[y*ipod->lcd_width+x]>>15)&1) +
	                 ((fb[y*ipod->lcd_width+x]>>10)&1) +
	                 ((fb[y*ipod->lcd_width+x]>>4)&1);
      pixels[0].pix.p2 = ((fb[y*ipod->lcd_width+x+1]>>15)&1) +
	                 ((fb[y*ipod->lcd_width+x+1]>>10)&1) +
	                 ((fb[y*ipod->lcd_width+x+1]>>4)&1);
      pixels[0].pix.p3 = ((fb[y*ipod->lcd_width+x+2]>>15)&1) +
	                 ((fb[y*ipod->lcd_width+x+2]>>10)&1) +
	                 ((fb[y*ipod->lcd_width+x+2]>>4)&1);
      pixels[0].pix.p4 = ((fb[y*ipod->lcd_width+x+3]>>15)&1) +
	                 ((fb[y*ipod->lcd_width+x+3]>>10)&1) +
	                 ((fb[y*ipod->lcd_width+x+3]>>4)&1);
      pixels[1].pix.p1 = ((fb[y*ipod->lcd_width+x+4]>>15)&1) +
	                 ((fb[y*ipod->lcd_width+x+4]>>10)&1) +
	                 ((fb[y*ipod->lcd_width+x+4]>>4)&1);
      pixels[1].pix.p2 = ((fb[y*ipod->lcd_width+x+5]>>15)&1) +
	                 ((fb[y*ipod->lcd_width+x+5]>>10)&1) +
	                 ((fb[y*ipod->lcd_width+x+5]>>4)&1);
      pixels[1].pix.p3 = ((fb[y*ipod->lcd_width+x+6]>>15)&1) +
	                 ((fb[y*ipod->lcd_width+x+6]>>10)&1) +
	                 ((fb[y*ipod->lcd_width+x+6]>>4)&1);
      pixels[1].pix.p4 = ((fb[y*ipod->lcd_width+x+7]>>15)&1) +
	                 ((fb[y*ipod->lcd_width+x+7]>>10)&1) +
	                 ((fb[y*ipod->lcd_width+x+7]>>4)&1);

      // display a character
      lcd_send_data(pixels[1].pix.aggregate,pixels[0].pix.aggregate);
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
    unsigned count;
    count = (width * height) << 1;
    lcd_5g_setup_rect(0x34, rect1, rect2, rect3, rect4, count);
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
	  lcd_5g_draw_2pixels(two_pixels,curpixel);

	  curpixel++;
	}
      }
      
      addr += ipod->lcd_width - width;
    }

    if( ipod->lcd_type != 0xB ) {
      while ((inl(0x70008a20) & 0x4000000) == 0);
      
      outl(0x0, 0x70008a24);
    
      height = height - h;
    } else {
      height = 0;
    }
  }

  if( ipod->lcd_type == 0xB )
    lcd_5g_finishup();
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
