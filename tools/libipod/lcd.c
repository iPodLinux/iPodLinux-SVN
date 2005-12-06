/*
 * lcd.c - iPod lcd routines
 *
 * Copyright (C) 2005 Bernard Leach, David Carne
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

unsigned short * ipod_alloc_fb(ipod_lcd_info *lcd)
{
	if (lcd == 0)
		return 0;

	if (lcd->bpp == 2)
		lcd->fb = malloc(lcd->width * (lcd->height/4));
	else if (lcd->bpp == 16)
		lcd->fb = malloc(lcd->width * (lcd->height*2));
	
	return lcd->fb;
}

void ipod_free_fb(ipod_lcd_info *lcd)
{
	if (lcd == 0 || lcd->fb != 0)
		return;
	
	free(lcd->fb);
}

ipod_lcd_info * ipod_get_lcd_info(void)
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
			lcd->bpp = 16;
			break;
	}
	
	if (generation >= 0x40000) {
		lcd->base = IPOD_NEW_LCD_BASE;
		lcd->rtc = IPOD_NEW_LCD_RTC;
	} else {
		lcd->base = IPOD_OLD_LCD_BASE;
		lcd->rtc = IPOD_OLD_LCD_RTC;
	}
	
	lcd->generation = generation;
	
	return lcd;
}

/*** The following LCD code is taken from Linux kernel uclinux-2.4.24-uc0-ipod2,
	 file arch/armnommu/mach-ipod/fb.c. A few modifications have been made. ***/

/* get current usec counter */
static int M_timer_get_current(ipod_lcd_info *lcd)
{
	return inl(lcd->rtc);
}

/* check if number of useconds has past */
static int M_timer_check(ipod_lcd_info *lcd, int clock_start, int usecs)
{
	unsigned long clock;
	clock = inl(lcd->rtc);
	
	if ( (clock - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for LCD with timeout */
static void M_lcd_wait_write(ipod_lcd_info *lcd)
{
	int start = M_timer_get_current(lcd);
	
	do {
		if ( (inl(lcd->base) & (unsigned int)0x8000) == 0 ) 
			break;
	} while ( M_timer_check(lcd, start, 1000) == 0 );
}


/* send LCD data */
static void M_lcd_send_data(ipod_lcd_info *lcd, int data_lo, int data_hi)
{
	M_lcd_wait_write(lcd);
	if (lcd->generation >= 0x70000) {
			outl ((inl (0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
			outl (data_hi | (data_lo << 8) | 0x760000, 0x70003008);
		} else {
			outl(data_lo, lcd->base + LCD_DATA);
			M_lcd_wait_write(lcd);
			outl(data_hi, lcd->base + LCD_DATA);
		}
}

/* send LCD command */
static void
M_lcd_prepare_cmd(ipod_lcd_info *lcd, int cmd)
{
	M_lcd_wait_write(lcd);
		if (lcd->generation > 0x70000) {
			outl ((inl (0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
			outl (cmd | 0x74000, 0x70003008);
		} else {
			outl(0x0, lcd->base + LCD_CMD);
			M_lcd_wait_write(lcd);
			outl(cmd, lcd->base + LCD_CMD);
		}
}

/* send LCD command and data */
static void M_lcd_cmd_and_data(ipod_lcd_info *lcd, int cmd, int data_lo, int data_hi)
{
	M_lcd_prepare_cmd(lcd, cmd);

	M_lcd_send_data(lcd, data_lo, data_hi);
}

// Copied from uW
void ipod_update_mono_lcd(ipod_lcd_info *lcd, int sx, int sy, int mx, int my)
{
	int y;
	int cursor_pos;
	int linelen = (lcd->width + 3) / 4;

	sx >>= 3;
	mx >>= 3;

	cursor_pos = sx + (sy << 5);

	for ( y = sy; y <= my; y++ ) {
		int x;
		unsigned char *img_data;

		/* move the cursor */
		M_lcd_cmd_and_data(lcd, 0x11, cursor_pos >> 8, cursor_pos & 0xff);

		/* setup for printing */
		M_lcd_prepare_cmd(lcd, 0x12);

		img_data = (unsigned char *)lcd->fb + (sx << 1) + (y * linelen);

		/* loops up to 20 times */
		for ( x = sx; x <= mx; x++ ) {
			/* display eight pixels */
			M_lcd_send_data(lcd, *(img_data + 1), *img_data);

			img_data += 2;
		}

		/* update cursor pos counter */
		cursor_pos += 0x20;
	}
}

/* get current usec counter */
static int C_timer_get_current(void)
{
	return inl(0x60005010);
}

/* check if number of useconds has past */
static int C_timer_check(int clock_start, int usecs)
{
	unsigned long clock;
	clock = inl(0x60005010);
	
	if ( (clock - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for LCD with timeout */
static void C_lcd_wait_write(void)
{
	if ((inl(0x70008A0C) & 0x80000000) != 0) {
		int start = C_timer_get_current();
			
		do {
			if ((inl(0x70008A0C) & 0x80000000) == 0) 
				break;
		} while (C_timer_check(start, 1000) == 0);
	}
}

static void C_lcd_send_lo (int v) 
{
	C_lcd_wait_write();
	outl (v | 0x80000000, 0x70008A0C);
}

static void C_lcd_send_hi (int v) 
{
	C_lcd_wait_write();
	outl (v | 0x81000000, 0x70008A0C);
}

static void C_lcd_cmd_data(ipod_lcd_info *lcd, int cmd, int data)
{
	if (lcd->type == 0) {
		C_lcd_send_lo (cmd);
		C_lcd_send_lo (data);
	} else {
		C_lcd_send_lo (0);
		C_lcd_send_lo (cmd);
		C_lcd_send_hi ((data >> 8) & 0xff);
		C_lcd_send_hi (data & 0xff);
	}
}

void ipod_update_colour_lcd(ipod_lcd_info *lcd, int sx, int sy, int mx, int my)
{
	int height = (my - sy) + 1;
	int width = (mx - sx) + 1;
	int rect1, rect2, rect3, rect4;

	unsigned short *addr = lcd->fb;

	if (width & 1) width++;

	/* calculate the drawing region */
	if ((lcd->generation >> 16) != 0x6) {
		rect1 = sx;
		rect2 = sy;
		rect3 = (sx + width) - 1;
		rect4 = (sy + height) - 1;
	} else {
		rect1 = sy;
		rect2 = (lcd->width - 1) - sx;
		rect3 = (sy + height) - 1;
		rect4 = (rect2 - width) + 1;
	}

	/* setup the drawing region */
	if (lcd->type == 0) {
		C_lcd_cmd_data(lcd, 0x12, rect1 & 0xff);
		C_lcd_cmd_data(lcd, 0x13, rect2 & 0xff);
		C_lcd_cmd_data(lcd, 0x15, rect3 & 0xff);
		C_lcd_cmd_data(lcd, 0x16, rect4 & 0xff);
	} else {
		if (rect3 < rect1) {
			int t;
			t = rect1;
			rect1 = rect3;
			rect3 = t;
		}

		if (rect4 < rect2) {
			int t;
			t = rect2;
			rect2 = rect4;
			rect4 = t;
		}

		/* max horiz << 8 | start horiz */
		C_lcd_cmd_data(lcd, 0x44, (rect3 << 8) | rect1);
		/* max vert << 8 | start vert */
		C_lcd_cmd_data(lcd, 0x45, (rect4 << 8) | rect2);

		if ((lcd->generation >> 16) == 0x6) {
			rect2 = rect4;
		}

		/* position cursor (set AD0-AD15) */
		/* start vert << 8 | start horiz */
		C_lcd_cmd_data(lcd, 0x21, (rect2 << 8) | rect1);

		/* start drawing */
		C_lcd_send_lo(0x0);
		C_lcd_send_lo(0x22);
	}

		addr += sy * lcd->width + sx;
		while (height > 0) {
		int h, x, y, pixels_to_write;

		pixels_to_write = (width * height) * 2;

		/* calculate how much we can do in one go */
		h = height;
		if (pixels_to_write > 64000) {
			h = (64000/2) / width;
			pixels_to_write = (width * h) * 2;
		}

		outl(0x10000080, 0x70008A20);
		outl((pixels_to_write - 1) | 0xC0010000, 0x70008A24);
		outl(0x34000000, 0x70008A20);

		/* for each row */
		for (y = 0; y < h; y++)
		{
			/* for each column */
			for (x = 0; x < width; x += 2) {
				unsigned two_pixels;

				two_pixels = addr[0] | (addr[1] << 16);
				addr += 2;

				while ((inl(0x70008A20) & 0x1000000) == 0);

				if (lcd->type != 0) {
					unsigned t = (two_pixels & ~0xFF0000) >> 8;
					two_pixels = two_pixels & ~0xFF00;
					two_pixels = t | (two_pixels << 8);
				}

				/* output 2 pixels */
				outl(two_pixels, 0x70008B00);
			}

			addr += lcd->width - width;
		}

		while ((inl(0x70008A20) & 0x4000000) == 0);

		outl(0x0, 0x70008A24);

		height = height - h;
	}
}



