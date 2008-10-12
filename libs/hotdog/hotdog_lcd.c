#include "hotdog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <linux/kd.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define inl(p) (*(volatile unsigned long *) (p))
#define outl(v,p) (*(volatile unsigned long *) (p) = (v))
#define inw(a) (*(volatile unsigned short *) (a))
#define outw(a,b) (*(volatile unsigned short *) (b) = (a))

static int hw_ver, lcd_type, lcd_width, lcd_height;
static unsigned int ipod_rtc, ipod_lcd_base;

#define LCD_CMD  0x8
#define LCD_DATA 0x10

/* get current usec counter - any type */
static int timer_get_current(void)
{
	return inl(ipod_rtc);
}

/* check if number of useconds has past - any type */
static int timer_check (int clock_start, int usecs)
{
	return ((timer_get_current() - clock_start) >= usecs);
}

/* wait for LCD with timeout - type 0 or 1 */
static void lcd_wait_write_01(void)
{
	if ((inl(0x70008A0C) & 0x80000000) != 0) {
		int start = timer_get_current();
			
		do {
			if ((inl(0x70008A0C) & 0x80000000) == 0) 
				break;
		} while ((timer_get_current() - start) < 1000);
	}
}

// Sends "lo" part - type 0 or 1
static void lcd_send_lo(int v)
{
	lcd_wait_write_01();
	outl(v | 0x80000000, 0x70008A0C);
}

// Sends "hi" part - type 0 or 1
static void lcd_send_hi(int v)
{
	lcd_wait_write_01();
	outl(v | 0x81000000, 0x70008A0C);
}

// Sends command + data - type 0 or 1
static void lcd_cmd_data(int cmd, int data)
{
	if (lcd_type == 0) {
		lcd_send_lo(cmd);
		lcd_send_lo(data);
	} else {
		lcd_send_lo(0);
		lcd_send_lo(cmd);
		lcd_send_hi((data >> 8) & 0xff);
		lcd_send_hi(data & 0xff);
	}
}

// Waits for the LCD to become available - type 2 or 3
static void lcd_wait_write_23 (void) 
{
	int start = timer_get_current();
	do {
		if ((inl (ipod_lcd_base) & 0x8000) == 0) break;
	} while (timer_check (start, 1000) == 0);
}

// Sends data to the LCD - type 2
static void lcd_send_data_2 (int data_lo, int data_hi) 
{
	lcd_wait_write_23();
	outl (data_lo, ipod_lcd_base + LCD_DATA);
	lcd_wait_write_23();
	outl (data_hi, ipod_lcd_base + LCD_DATA);
}

// Sends a command to the LCD - type 2
static void lcd_prepare_cmd_2 (int cmd) 
{
	lcd_wait_write_23();
	outl(0x0, ipod_lcd_base + LCD_CMD);
	lcd_wait_write_23();
	outl(cmd, ipod_lcd_base + LCD_CMD);
}

// Sends data to the LCD - type 3
static void lcd_send_data_3 (int data_lo, int data_hi) 
{
	lcd_wait_write_23();
	outl ((inl (0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
	outl (data_hi | (data_lo << 8) | 0x760000, 0x70003008);
}

// Sends a command to the LCD - type 3
static void lcd_prepare_cmd_3 (int cmd) 
{
	lcd_wait_write_23();
	outl((inl(0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
	outl(cmd | 0x740000, 0x70003008);
}

// Sends command + data - type 2 or 3
static void lcd_cmd_and_data (int cmd, int data_lo, int data_hi, int type)
{
	if (type == 2) {
		lcd_prepare_cmd_2 (cmd);
		lcd_send_data_2 (data_lo, data_hi);
	} else if (type == 3) {
		lcd_prepare_cmd_3 (cmd);
		lcd_send_data_3 (data_lo, data_hi);
	}
}

static void lcd_update_mono_display (uint8 *fb, int sx, int sy, int width, int height) 
{
	int linelen = (lcd_width + 3) / 4;
	int cursor_pos, y, x;
	int mx = sx + width - 1, my = sy + height - 1;
	int type = lcd_type;

	sx >>= 3;
	mx >>= 3;

	cursor_pos = sx + (sy << 5);

	uint8 *img_data = fb + (sy * linelen);
	for (y = sy; y <= my; y++) {
		// move the cursor
		lcd_cmd_and_data (0x11, cursor_pos >> 8, cursor_pos & 0xff, type);

		// display one line
		if (type == 3) {
			lcd_prepare_cmd_3 (0x12);
			for (x = sx; x <= mx; x++) {
				// display 8 pixels
				lcd_send_data_3 (img_data[2*x+1], img_data[2*x]);
			}
		} else {
			lcd_prepare_cmd_2 (0x12);
			for (x = sx; x <= mx; x++) {
				// display 8 pixels
				lcd_send_data_2 (img_data[2*x+1], img_data[2*x]);
			}
		}

		// update cursor pos counter
		cursor_pos += 0x20;
		img_data += linelen;
	}
}

static void lcd_update_color_display (uint16 *fb, int sx, int sy, int width, int height)
{
	int rect1, rect2, rect3, rect4;
	uint16 *addr = fb;

	if (width & 1) width++;

	/* calculate the drawing region */
	if (hw_ver != 0x6) {
		rect1 = sx;
		rect2 = sy;
		rect3 = (sx + width) - 1;
		rect4 = (sy + height) - 1;
	} else {
		rect1 = sy;
		rect2 = (lcd_width - 1) - sx;
		rect3 = (sy + height) - 1;
		rect4 = (rect2 - width) + 1;
	}

	/* setup the drawing region */
	if (lcd_type == 0) {
		lcd_cmd_data(0x12, rect1 & 0xff);
		lcd_cmd_data(0x13, rect2 & 0xff);
		lcd_cmd_data(0x15, rect3 & 0xff);
		lcd_cmd_data(0x16, rect4 & 0xff);
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
		lcd_cmd_data(0x44, (rect3 << 8) | rect1);
		/* max vert << 8 | start vert */
		lcd_cmd_data(0x45, (rect4 << 8) | rect2);

		if (hw_ver == 0x6) {
			rect2 = rect4;
		}

		/* position cursor (set AD0-AD15) */
		/* start vert << 8 | start horiz */
		lcd_cmd_data(0x21, (rect2 << 8) | rect1);

		/* start drawing */
		lcd_send_lo(0x0);
		lcd_send_lo(0x22);
	}

	addr += sx + sy * lcd_width;

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

				if (lcd_type == 0)
					two_pixels = (addr[1] << 16) | addr[0];
				else
					two_pixels = ( ((addr[0]&0xFF)<<8) | ((addr[0]&0xFF00)>>8) ) | 
						((((addr[1]&0xFF)<<8) | ((addr[1]&0xFF00)>>8) )<<16);

				addr += 2;

                                while ((inl(0x70008A20) & 0x1000000) == 0);
                                outl(two_pixels, 0x70008B00);
			}

			addr += lcd_width - width;
		}

                while ((inl(0x70008A20) & 0x4000000) == 0);
                outl(0x0, 0x70008A24);
                
                height = height - h;
	}
}

#define LCD_FB_BASE_REG (*(volatile unsigned long *)(0xc2000028))
static void lcd_update_sansa_display(uint16 *fb, int sx, int sy, int width, int height)
{
	const uint16 *src = fb;
	uint16 *dst = (uint16 *)(LCD_FB_BASE_REG & 0x0fffffff);
	memcpy(dst, src, 220 * 176 * sizeof(uint16));
}

static long iPod_GetGeneration() 
{
	static long gen = 0;
	if (gen == 0) {
		int i;
		char cpuinfo[256];
		char *ptr;
		FILE *file;
		
		if ((file = fopen("/proc/cpuinfo", "r")) != NULL) {
			while (fgets(cpuinfo, sizeof(cpuinfo), file) != NULL)
				if (strncmp(cpuinfo, "Revision", 8) == 0)
					break;
			fclose(file);
		}
		for (i = 0; !isspace(cpuinfo[i]); i++);
		for (; isspace(cpuinfo[i]); i++);
		ptr = cpuinfo + i + 2;
		
		gen = strtol (ptr, NULL, 16);
	}
	return gen;
}

void HD_LCD_GetInfo (int *hwv, int *lw, int *lh, int *lt) 
{
	if (hwv) *hwv = hw_ver;
	if (lw) *lw = lcd_width;
	if (lh) *lh = lcd_height;
	if (lt) *lt = lcd_type;
}

void HD_LCD_Init() 
{
	hw_ver = iPod_GetGeneration() >> 16;
	switch (hw_ver) {
	case 0x0: // Sansa e200
		lcd_width = 220;
		lcd_height = 176;
		lcd_type = 4;
		// lcd_base and rtc not used for now, but here anyway
		//ipod_lcd_base = 0xc0001000;
		//ipod_rtc = 0xcf001110;
		break;
	case 0xB: // video
		lcd_width = 320;
		lcd_height = 240;
		lcd_type = 5;
		break;
	case 0xC: // nano
		lcd_width = 176;
		lcd_height = 132;
		lcd_type = 1;
		break;
	case 0x6: // photo, color
		lcd_width = 220;
		lcd_height = 176;

		if (iPod_GetGeneration() == 0x60000) {
			lcd_type = 0; // photo
		} else { // color
			int gpio_a01, gpio_a04;
			gpio_a01 = (inl(0x6000D030) & 0x2) >> 1;
			gpio_a04 = (inl(0x6000D030) & 0x10) >> 4;
			if (((gpio_a01 << 1) | gpio_a04) == 0 || ((gpio_a01 << 1) | gpio_a04) == 2) {
				lcd_type = 0;
			} else {
				lcd_type = 1;
			}
		}
		break;
	case 0x7: // mini2g
	case 0x4: // mini1g
		lcd_width = 138;
		lcd_height = 110;
		if (hw_ver == 0x7) // mini2g
			lcd_type = 3;
		else {
			lcd_type = 2;
			ipod_lcd_base = 0x70003000;
			ipod_rtc = 0x60005010;
		}
		break;
	case 0x5: // 4g
		ipod_lcd_base = 0x70003000;
		ipod_rtc = 0x60005010;
		lcd_width = 160;
		lcd_height = 128;
		lcd_type = 2;
		break;
	case 0x3: // 3g
	case 0x2: // 2g
	case 0x1: // 1g
		ipod_lcd_base = 0xc0001000;
		ipod_rtc = 0xcf001110;
		lcd_width = 160;
		lcd_height = 128;
		lcd_type = 2;
		break;
	default:
		fprintf (stderr, "Unsupported LCD\n");
		exit (1);
	}

	int fd = open("/dev/console", O_NONBLOCK);
	ioctl(fd, KDSETMODE, KD_GRAPHICS);
	close(fd);
}

extern void _HD_ARM_Update5G (uint16 *fb, int x, int y, int w, int h);
extern void _HD_ARM_UpdatePhoto (uint16 *fb, int x, int y, int w, int h, int lcd_type);
void HD_LCD_Update (void *fb, int x, int y, int w, int h) 
{
	switch (lcd_type) {
	case 0: // photo
	case 1: // color, nano
#if 1
		lcd_update_color_display (fb, x, y, w, h);
#else
		_HD_ARM_UpdatePhoto (fb, x, y, w, h, lcd_type);
#endif
		break;
	case 2: // mono
	case 3: // new mono (mini2g)
		lcd_update_mono_display (fb, x, y, w, h);
		break;
	case 4: // Sansa e200
		lcd_update_sansa_display (fb, x, y, w, h);
		break;
	case 5: // video
		_HD_ARM_Update5G (fb, x, y, w, h);
		break;
	}
}

void HD_LCD_Quit() 
{
	int fd = open("/dev/console", O_NONBLOCK);
	ioctl(fd, KDSETMODE, KD_TEXT);
	close(fd);
}

/*
 * Local Variables:
 * indent-tabs-mode: t
 * c-basic-offset: 8
 * End:
 */
