/*
 * Copyright (C) 2005 Adam Johnston <agjohnst@uiuc.edu>
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


#ifdef IPOD

#define outl(a,b) (*(volatile unsigned int *)(b) = (a))
#define inl(a) (*(volatile unsigned int *)(a))

#include "videovars.h"

#define IPOD_STD_LCD_BUSY_MASK	0x8000

#define IPOD_STD_LCD_WIDTH	160
#define IPOD_STD_LCD_HEIGHT	128
#define IPOD_MINI_LCD_WIDTH	138
#define IPOD_MINI_LCD_HEIGHT	110

#define IPOD_NANO_LCD_WIDTH	176	
#define IPOD_NANO_LCD_HEIGHT	132	


#define IPOD_PP5002_RTC		0xcf001110
#define IPOD_PP5002_LCD_BASE	0xc0001000
#define IPOD_PP5020_RTC		0x60005010
#define IPOD_PP5020_LCD_BASE	0x70003000

#define LCD_CMD 0x8
#define LCD_DATA 0x10

static unsigned long ipod_rtc = 0x60005010;
static unsigned long lcd_base = 0x70008a0c;
static unsigned long lcd_busy_mask = 0x80000000;

static unsigned long lcd_width = 220;
static unsigned long lcd_height = 176;
static unsigned int lcd_type = 0x0;
extern int hw_version;
extern unsigned int full_hw_version;


static void video_setup_display(long hw_ver)
{
	int gpio_a01, gpio_a04;
	switch (hw_ver)
	{
	case 0xc:
		lcd_base = 0x70008a0c;
		lcd_busy_mask = 0x80000000;
		lcd_width = IPOD_NANO_LCD_WIDTH;
		lcd_height = IPOD_NANO_LCD_HEIGHT;
		ipod_rtc = IPOD_PP5020_RTC;
		lcd_type = 2;
		break;
	case 6:	
		lcd_base = 0x70008a0c;
		lcd_busy_mask = 0x80000000;
		lcd_width = 220;
		lcd_height = 176;
		ipod_rtc = IPOD_PP5020_RTC;
		if (full_hw_version == 0x60000) {
			lcd_type = 0;
		} else {
			gpio_a01 = (inl(0x6000D030) & 0x2) >> 1;
			gpio_a04 = (inl(0x6000D030) & 0x10) >> 4;
			if (((gpio_a01 << 1) | gpio_a04) == 0 || ((gpio_a01 < 1) | gpio_a04) == 2) {
				lcd_type = 0;
			} else {
				lcd_type = 1;
			}
		}
		break;
	case 5:
		lcd_base = IPOD_PP5020_LCD_BASE;
		lcd_busy_mask = IPOD_STD_LCD_BUSY_MASK;
		lcd_width = IPOD_STD_LCD_WIDTH;
		lcd_height = IPOD_STD_LCD_HEIGHT;
		ipod_rtc = IPOD_PP5020_RTC;
		break;	
	case 7:	
	case 4:
		lcd_width = IPOD_MINI_LCD_WIDTH;
		lcd_height = IPOD_MINI_LCD_HEIGHT;
		lcd_base = IPOD_PP5020_LCD_BASE;
		lcd_busy_mask = IPOD_STD_LCD_BUSY_MASK;
		ipod_rtc = IPOD_PP5020_RTC;
		break;	
	case 3:
	case 2:
	case 1:	
		lcd_width = IPOD_STD_LCD_WIDTH;
		lcd_height = IPOD_STD_LCD_HEIGHT;
		lcd_base = IPOD_PP5002_LCD_BASE;
		ipod_rtc = IPOD_PP5002_RTC;
		lcd_busy_mask = IPOD_STD_LCD_BUSY_MASK;
		break;	
	} 	
}

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

/* wait for LCD with timeout */
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

static void video_lcd_send_data(int data_lo, int data_hi)
{
	video_lcd_wait_write();
	if (hw_version == 0x7) {
		outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
		outl(data_hi | (data_lo << 8) | 0x760000, 0x70003008);
	}
	else {
		outl(data_lo, lcd_base + LCD_DATA);
		video_lcd_wait_write();
		outl(data_hi, lcd_base + LCD_DATA);
	}
}

static void video_lcd_prepare_cmd(int cmd)
{
	video_lcd_wait_write();
	if (hw_version == 0x7) {
		outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
		outl(cmd | 0x740000, 0x70003008);
	}
	else {
		outl(0x0, lcd_base + LCD_CMD);
		video_lcd_wait_write();
		outl(cmd, lcd_base + LCD_CMD);
	}
}

static void video_lcd_send_lo(int v)
{
	video_lcd_wait_write();
	outl(v | 0x80000000, 0x70008A0C);
}
static void video_lcd_send_hi(int v)
{
	video_lcd_wait_write();
	outl(v | 0x81000000, 0x70008A0C);
}

static void video_lcd_cmd_data(int cmd, int data)
{
	if (lcd_type == 0) {
		video_lcd_send_lo(cmd);
		video_lcd_send_lo(data);
	} else {
		video_lcd_send_lo(0x0);
		video_lcd_send_lo(cmd);
		video_lcd_send_hi((data >> 8) & 0xff);
		video_lcd_send_hi(data & 0xff);
	}
}

static inline void video_lcd_cmd_and_data(int cmd, int data_lo, int data_hi)
{
	video_lcd_prepare_cmd(cmd);
	video_lcd_send_data(data_lo, data_hi);
}

static void video_ipod_update_display(char * addr, int sx, int sy, int mx, int my)
{
	unsigned short y;
	unsigned short cursor_pos;
	unsigned char *img_data;
	unsigned width;
	unsigned short diff = 0;

	img_data = addr;

	if (my > lcd_height) {
		my = lcd_height;
	}

	if (mx > lcd_width) {
		diff = (lcd_width - mx) / 4;
		mx = lcd_width;
	}

	width = mx-sx;
	width = width / 8;

	cursor_pos = sx + (sy * 0x20);
	for (y = sy; y <= my; y++) {
		unsigned char x;
		video_lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);
		video_lcd_prepare_cmd(0x12);
		for (x = 0; x < width; x++) {
			video_lcd_send_data(*(img_data+1), *img_data);
			img_data += 2;
		}

		img_data += diff;
		cursor_pos += 0x20;
	}
}

static void video_ipod_update_photo(unsigned short * x, int sx, int sy, int mx, int my)
{

#if 0
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
#else


	int startx = sx; //sy * fontheight(p);
	int starty = sy; //sx * fontwidth(p);
	int height = (my - sy); // * fontheight(p);
	int width = (mx - sx); // * fontwidth(p);
	int rect1, rect2, rect3, rect4;

	unsigned short *addr = x; //(unsigned short *)ipod_scr;
	
	/* calculate the drawing region */
	if (hw_version != 0x6) {
		rect1 = startx;			/* start horiz */
		rect2 = starty;			/* start vert */
		rect3 = (startx + width) - 1;	/* max horiz */
		rect4 = (starty + height) - 1;	/* max vert */
	} else {
		rect1 = starty;			/* start vert */
		rect2 = (lcd_width - 1) - startx;	/* start horiz */
		rect3 = (starty + height) - 1;	/* end vert */
		rect4 = (rect2 - width) + 1;		/* end horiz */
	}

	/* setup the drawing region */
	if (lcd_type == 0) {
		video_lcd_cmd_data(0x12, rect1 & 0xff);	/* start vert */
		video_lcd_cmd_data(0x13, rect2 & 0xff);	/* start horiz */
		video_lcd_cmd_data(0x15, rect3 & 0xff);	/* end vert */
		video_lcd_cmd_data(0x16, rect4 & 0xff);	/* end horiz */
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
		video_lcd_cmd_data(0x44, (rect3 << 8) | rect1);
		/* max vert << 8 | start vert */
		video_lcd_cmd_data(0x45, (rect4 << 8) | rect2);

		if (hw_version == 0x6) {
			/* start vert = max vert */
			rect2 = rect4;
		}

		/* position cursor (set AD0-AD15) */
		/* start vert << 8 | start horiz */
		video_lcd_cmd_data(0x21, (rect2 << 8) | rect1);

		/* start drawing */
		video_lcd_send_lo(0x0);
		video_lcd_send_lo(0x22);
	}

	addr += startx * lcd_width + starty;

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
				if (lcd_type != 0)
				{
					unsigned t = (two_pixels & ~0xFF0000) >> 8;
					two_pixels = two_pixels & ~0xFF00;
					two_pixels = t | (two_pixels << 8);
				}
				/* output 2 pixels */
				outl(two_pixels, 0x70008b00);
			}

			//addr += lcd_width - width;
		}

		while ((inl(0x70008a20) & 0x4000000) == 0);

		outl(0x0, 0x70008a24);

		height = height - h;
	}

#endif
}

void
ipod_handle_video()
{
	unsigned int curframe;
	unsigned int x = 0;
	unsigned int numFrames;
	unsigned int frameWidth;
	unsigned int frameHeight;
	unsigned int uSecPerFrame;	
	unsigned int curBuffer;
	unsigned int tstart;

	curBuffer = 0;

	outl(0, VAR_VIDEO_CURBUFFER_PLAYING);
	outl(0, VAR_VIDEO_BUFFER_READY);
	outl(0, VAR_VIDEO_MODE);
	
	video_setup_display(hw_version);

	while (inl(VAR_VIDEO_ON)) {
		curBuffer = inl(VAR_VIDEO_CURBUFFER_PLAYING);	
		while ((inl(VAR_VIDEO_BUFFER_READY + sizeof(unsigned int)* curBuffer)==0) || (inl(VAR_VIDEO_MODE)==0)) {
			if (!inl(VAR_VIDEO_ON))
				return;
		}

		curBuffer = inl(VAR_VIDEO_CURBUFFER_PLAYING);
		x = 0;	

		numFrames = inl(VAR_VIDEO_BUFFER_READY + sizeof(unsigned int) * curBuffer);	
		frameWidth = inl(VAR_VIDEO_FRAME_WIDTH);
		frameHeight = inl(VAR_VIDEO_FRAME_HEIGHT);
		uSecPerFrame = inl(VAR_VIDEO_MICROSECPERFRAME);

		while (x < numFrames) {
			tstart = video_timer_get_current();	
			if (inl(VAR_VIDEO_MODE) == 0) {
				break;
			}

			curframe = inl(VAR_VIDEO_FRAMES + curBuffer * sizeof(unsigned int) * FRAMES_PER_BUFFER + x * sizeof(unsigned int));	
			if (hw_version==0x6 || hw_version==0xc) {	
				video_ipod_update_photo((unsigned short *)curframe, 0, 0, frameWidth, frameHeight);
			} else {
				video_ipod_update_display((unsigned char *)curframe, 0,0, frameWidth, frameHeight);
			}

			x++;	
			while (video_timer_check(tstart, uSecPerFrame) == 0);
		}

		outl(0x0, VAR_VIDEO_BUFFER_READY + sizeof(unsigned int) * curBuffer);
		outl(0x1, VAR_VIDEO_BUFFER_DONE + sizeof(unsigned int) * curBuffer); 	

		curBuffer = (curBuffer + 1) % VIDEO_FILEBUFFER_COUNT;	
		outl(curBuffer, VAR_VIDEO_CURBUFFER_PLAYING);
	}
}
#endif
