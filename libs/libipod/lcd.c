/*
 * lcd.c - iPod lcd routines
 *
 * Copyright (C) 2005 Bernard Leach, David Carne, Alastair Stuart,
 *                    Benjamin Eriksson & Mattias Pierre.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <string.h>
#ifdef IPOD
#include <linux/kd.h>
#endif

#include "ipod.h"
#include "ipodhardware.h"

static inline int constrain(int min, int max, int val)
{
	if (val > max) val = max;
	if (val < min) val = min;
	return (val);
}

#ifdef IPOD
static int ipod_ioctl(int request, int *arg)
{
	int fd;

	fd = open(FB_DEV_NAME, O_NONBLOCK);
	if (fd < 0) fd = open(FB_DEVFS_NAME, O_NONBLOCK);
	if (fd < 0) {
		return -1;
	}
	if (ioctl(fd, request, arg) < 0) {
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}
#endif

int ipod_get_contrast(void)
{
	int contrast = 0;
#ifdef IPOD
	if (ipod_ioctl(FBIOGET_CONTRAST, &contrast) < 0)
		return -1;
#endif
	return contrast;
}

int ipod_set_contrast(int contrast)
{
	contrast = constrain(0, 128, contrast); /* just in case */
#ifdef IPOD
	if (ipod_ioctl(FBIOSET_CONTRAST, (int *)(long)contrast) < 0)
		return -1;
#endif
	return 0;
}

int ipod_get_backlight(void)
{
	int backlight = 0;
#ifdef IPOD
	if (ipod_ioctl(FBIOGET_BACKLIGHT, &backlight) < 0)
		return -1;
#endif
	return backlight;
}

int ipod_set_backlight(int backlight)
{
#ifdef IPOD
	if (ipod_ioctl(FBIOSET_BACKLIGHT, (int *)(long)backlight) < 0)
		return -1;
#endif
	return 0;
}

int ipod_set_blank_mode(int blank)
{
#ifdef IPOD
	if (ipod_ioctl(FBIOBLANK, (int *) blank) < 0)
		return -1;
#endif
	return 0;
}

void * ipod_lcd_alloc_fb(ipod_lcd_info *lcd)
{
	if (lcd == 0)
		return 0;
	
	lcd->fb = malloc(lcd->fblen);
	
	return lcd->fb;
}

void ipod_lcd_free_fb(ipod_lcd_info *lcd)
{
	if (lcd == 0 || lcd->fb != 0)
		return;
	
	free(lcd->fb);
}

ipod_lcd_info * ipod_lcd_get_info(void)
{
	long generation = ipod_get_hw_version();
	ipod_lcd_info *lcd;
	
	lcd = malloc(sizeof(ipod_lcd_info));
	memset(lcd, 0, sizeof(ipod_lcd_info));
	
	switch (generation >> 16) {
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x5:
			/* 1-4g */
			lcd->width = 160;
			lcd->height = 128;
			lcd->bpp = 2;
			break;
		case 0x4:
		case 0x7:
			/* mini */
			lcd->width = 138;
			lcd->height = 110;
			lcd->bpp = 2;
			break;
		case 0x6:
			/* photo */
			lcd->width = 220;
			lcd->height = 176;
			lcd->bpp = 16;
			if (generation == 0x60000) {
				lcd->type = 0;
			} else {
#ifdef IPOD
				int gpio_a01, gpio_a04;
	
				gpio_a01 = (inl (0x6000D030) & 0x2) >> 1;
				gpio_a04 = (inl (0x6000D030) & 0x10) >> 4;
				if (((gpio_a01 << 1) | gpio_a04) == 0 ||
					((gpio_a01 << 1) | gpio_a04) == 2) {
					lcd->type = 0;
				} else {
					lcd->type = 1;   
				}
#else
				lcd->type = 1;
#endif
			}
			break;
		case 0xc:
			/* nano */
			lcd->width = 176;
			lcd->height = 132;
			lcd->bpp = 16;
			lcd->type = 1;
			break;
		case 0xb:
			/* video */
			lcd->width = 320;
			lcd->height = 240;
			lcd->type = 5;
			lcd->bpp = 16;
			break;
	}
	
	if (lcd->bpp == 2) {
		lcd->busy_mask = IPOD_STD_LCD_BUSY_MASK;
		lcd->fblen = lcd->width * (lcd->height/4);
	} else if (lcd->bpp == 16) {
		lcd->busy_mask = IPOD_COL_LCD_BUSY_MASK;
		lcd->fblen = lcd->width * (lcd->height*2);
	}
	
	if (generation >= 0x40000) {
		lcd->base = IPOD_NEW_LCD_BASE;
		lcd->rtc = IPOD_NEW_LCD_RTC;
	} else {
		lcd->base = IPOD_OLD_LCD_BASE;
		lcd->rtc = IPOD_OLD_LCD_RTC;
	}
	
	lcd->gen = generation >> 16;
	
	return lcd;
}

/* An amalgamation of all the hw specific lcd writing code follows. */
 
#ifdef IPOD

static int ipod_lcd_timer_get_current(ipod_lcd_info * lcd)
{
	return inl(lcd->rtc);
}

static int ipod_lcd_timer_check(ipod_lcd_info * lcd, int clock_start, int usecs)
{
	unsigned long clock;
	clock = inl(lcd->rtc);
	
	return (clock - clock_start) >= usecs;
}

static void ipod_lcd_wait_write(ipod_lcd_info * lcd)
{
	if ((inl(lcd->base) & lcd->busy_mask) != 0) {
		int start = ipod_lcd_timer_get_current(lcd);
			
		do {
			if ((inl(lcd->base) & lcd->busy_mask) == 0)
				break;
		} while (ipod_lcd_timer_check(lcd, start, 1000) == 0);
	}
}

static void ipod_lcd_send_data(ipod_lcd_info * lcd, int data_lo, int data_hi)
{
	ipod_lcd_wait_write(lcd);
	if (lcd->gen == 0x7) {
		outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
		outl(data_hi | (data_lo << 8) | 0x760000, 0x70003008);
	}
	else {
		outl(data_lo, lcd->base + LCD_DATA);
		ipod_lcd_wait_write(lcd);
		outl(data_hi, lcd->base + LCD_DATA);
	}
}

static void ipod_lcd_prepare_cmd(ipod_lcd_info * lcd, int cmd)
{
	ipod_lcd_wait_write(lcd);
	if (lcd->gen == 0x7) {
		outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
		outl(cmd | 0x740000, 0x70003008);
	}
	else {
		outl(0x0, lcd->base + LCD_CMD);
		ipod_lcd_wait_write(lcd);
		outl(cmd, lcd->base + LCD_CMD);
	}
}

static void ipod_lcd_send_lo(ipod_lcd_info * lcd, int v)
{
	ipod_lcd_wait_write(lcd);
	outl(v | 0x80000000, 0x70008a0c);
}

static void ipod_lcd_send_hi(ipod_lcd_info * lcd, int v)
{
	ipod_lcd_wait_write(lcd);
	outl(v | 0x81000000, 0x70008a0c);
}

static void ipod_lcd_cmd_data(ipod_lcd_info * lcd, int cmd, int data)
{
	if (lcd->type == 0)
	{
		ipod_lcd_send_lo(lcd, cmd);
		ipod_lcd_send_lo(lcd, data);
	} else {
		/* todo add support for old shit (?) */
		ipod_lcd_send_lo(lcd, 0);
		ipod_lcd_send_lo(lcd, cmd);
		ipod_lcd_send_hi(lcd, (data >> 8) & 0xff);
		ipod_lcd_send_hi(lcd,  data       & 0xff);
	}
}

static inline void ipod_lcd_cmd_and_data(ipod_lcd_info * lcd,
                                         int cmd, int data_lo, int data_hi)
{
	ipod_lcd_prepare_cmd(lcd, cmd);
	ipod_lcd_send_data(lcd, data_lo, data_hi);
}

static void ipod_lcd_bcm_write32(unsigned address, unsigned value) {
	/* write out destination address as two 16bit values */
	outw(address, 0x30010000);
	outw((address >> 16), 0x30010000);

	/* wait for it to be write ready */
	while ((inw(0x30030000) & 0x2) == 0);

	/* write out the value low 16, high 16 */
	outw(value, 0x30000000);
	outw((value >> 16), 0x30000000);
}

static void ipod_lcd_bcm_setup_rect(unsigned cmd, unsigned start_horiz, unsigned start_vert, unsigned max_horiz, unsigned max_vert, unsigned count) {
	ipod_lcd_bcm_write32(0x1F8, 0xFFFA0005);
	ipod_lcd_bcm_write32(0xE0000, cmd);
	ipod_lcd_bcm_write32(0xE0004, start_horiz);
	ipod_lcd_bcm_write32(0xE0008, start_vert);
	ipod_lcd_bcm_write32(0xE000C, max_horiz);
	ipod_lcd_bcm_write32(0xE0010, max_vert);
	ipod_lcd_bcm_write32(0xE0014, count);
	ipod_lcd_bcm_write32(0xE0018, count);
	ipod_lcd_bcm_write32(0xE001C, 0);
}

static unsigned ipod_lcd_bcm_read32(unsigned address) {
	while ((inw(0x30020000) & 1) == 0);

	/* write out destination address as two 16bit values */
	outw(address, 0x30020000);
	outw((address >> 16), 0x30020000);

	/* wait for it to be read ready */
	while ((inw(0x30030000) & 0x10) == 0);

	/* read the value */
	return inw(0x30000000) | inw(0x30000000) << 16;
}

static void ipod_lcd_bcm_finishup(void) {
	unsigned data; 

	outw(0x31, 0x30030000); 

	ipod_lcd_bcm_read32(0x1FC);

	do {
		data = ipod_lcd_bcm_read32(0x1F8);
	} while (data == 0xFFFA0005 || data == 0xFFFF);

	ipod_lcd_bcm_read32(0x1FC);
}


static void ipod_lcd_update_16bpp(ipod_lcd_info * lcd, int sx, int sy, int mx, int my)
{
	int startx = sy;
	int starty = sx;
	int height = (my - sy);
	int width  = (mx - sx);
	int rect1, rect2, rect3, rect4;
	
	unsigned short *addr = lcd->fb;
	
	/* calculate the drawing region */
	if (lcd->gen != 0x6) {
		rect1 = starty;                 /* start horiz */
		rect2 = startx;                 /* start vert */
		rect3 = (starty + width) - 1;   /* max horiz */
		rect4 = (startx + height) - 1;  /* max vert */
	} else {
		rect1 = startx;                          /* start vert */
		rect2 = (lcd->width - 1) - starty;       /* start horiz */
		rect3 = (startx + height) - 1;           /* end vert */
		rect4 = (rect2 - width) + 1;             /* end horiz */
	}
	
	/* setup the drawing region */
	if (lcd->type == 0) {
		ipod_lcd_cmd_data(lcd, 0x12, rect1);      /* start vert */
		ipod_lcd_cmd_data(lcd, 0x13, rect2);      /* start horiz */
		ipod_lcd_cmd_data(lcd, 0x15, rect3);      /* end vert */
		ipod_lcd_cmd_data(lcd, 0x16, rect4);      /* end horiz */
	} else if( lcd->type != 5 ) {
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
		ipod_lcd_cmd_data(lcd, 0x44, (rect3 << 8) | rect1);
		/* max vert << 8 | start vert */
		ipod_lcd_cmd_data(lcd, 0x45, (rect4 << 8) | rect2);
	
		if( lcd->gen == 0x6) {
			/* start vert = max vert */
			rect2 = rect4;
		}
		
		/* position cursor (set AD0-AD15) */
		/* start vert << 8 | start horiz */
		ipod_lcd_cmd_data(lcd, 0x21, (rect2 << 8) | rect1);
	
	/* start drawing */
		ipod_lcd_send_lo(lcd, 0x0);
		ipod_lcd_send_lo(lcd, 0x22);
	} else { /* 5G */
		unsigned count = (width * height) << 1;
		ipod_lcd_bcm_setup_rect(0x34, rect1, rect2, rect3, rect4, count);
	}
	
	addr += startx * (lcd->width*2) + starty;
  
	while (height > 0) {
		int x, y;
		int h, pixels_to_write;
		unsigned int curpixel;
		
		curpixel = 0;
		
		if (lcd->type != 5) {
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
				
				two_pixels = ( ((addr[0]&0xFF)<<8) | ((addr[0]&0xFF00)>>8) ) | 
				          ((((addr[1]&0xFF)<<8) | ((addr[1]&0xFF00)>>8) )<<16);
				
				addr += 2;
	
				if (lcd->type != 5) {
					while ((inl(0x70008a20) & 0x1000000) == 0);
						/* output 2 pixels */
						outl(two_pixels, 0x70008b00);
				} else {
					/* output 2 pixels */
					ipod_lcd_bcm_write32(0xE0020 + (curpixel << 2), two_pixels);
					curpixel++;
				}
			}
	
		addr += lcd->width - width;
		}
	
		if (lcd->type != 5) {
			while ((inl(0x70008a20) & 0x4000000) == 0);
				outl(0x0, 0x70008a24);
			height = height - h;
		} else {
			height = 0;
		}
	}
	
	if (lcd->type == 5) {
		ipod_lcd_bcm_finishup();
	}

}


static void ipod_lcd_update_2bpp(ipod_lcd_info * lcd, int sx, int sy, int mx, int my)
{
	unsigned short  y;
	unsigned short  cursor_pos;
	unsigned int    width;
	unsigned short  diff = 0;
	unsigned int    rowh = (lcd->width/4);
	unsigned char * img_data = (unsigned char *) lcd->fb;
	
	// Truncate the height of the data if it falls outside the LCD-area.
	if (my > lcd->height)
		my = lcd->height;

	// Truncate the width of the data if it falls outside the LCD-area.
	// Save the amount of data being cropped so one can skip that redundant
	// information when blitting.
	if (mx > lcd->width)
	{
		diff = (lcd->width - mx) / 4;
		mx   = lcd->width;
	}
	
	// The width of the blit-rect in bytes.
	width = (mx - sx) / 8; 
	
	// Set the cursor position to where the blit-rect starts.
	// NOTE: 0x20 = 32, 32*4 = 256, hence the width of one display row is 128px.
	cursor_pos = sx + (sy * rowh);
	
	// Blit the data to the screen. This is done, as one would expect, first
	// horizontally, then vertically. 
	for (y = sy; y <= my; y++)
	{
		unsigned char x;
		ipod_lcd_cmd_and_data(lcd, 0x11, cursor_pos >> 8, cursor_pos & 0xff);
		ipod_lcd_prepare_cmd(lcd, 0x12);
		
		// This is the horizontal plot.
		for (x = 0; x < width; x++)
		{
			// Send two bytes at once (?).
			ipod_lcd_send_data(lcd, *(img_data+1), *img_data);
			img_data += 2;
		}
		
		// Skip any data that got truncated before.
		img_data += diff;
		
		// Skip to the next row.
		cursor_pos += rowh;
	}
}

void ipod_lcd_update(ipod_lcd_info *lcd, int sx, int sy, int mx, int my)
{
	if (lcd->bpp == 2)
		ipod_lcd_update_2bpp(lcd, sx, sy, mx, my);
	else
		ipod_lcd_update_16bpp(lcd, sx, sy, mx, my);
}

void ipod_fb_video(void)
{
	int fd = open("/dev/console", O_NONBLOCK);
	ioctl(fd, KDSETMODE, KD_GRAPHICS);
	close(fd);	
}

void ipod_fb_text(void)
{
	int fd = open("/dev/console", O_NONBLOCK);
	ioctl(fd, KDSETMODE, KD_TEXT);
	close(fd);
}


#endif

