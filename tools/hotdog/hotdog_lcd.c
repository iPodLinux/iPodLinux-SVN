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

/* get current usec counter */
static int timer_get_current(void)
{
	return inl(0x60005010);
}

/* wait for LCD with timeout */
static void lcd_wait_write(void)
{
	if ((inl(0x70008A0C) & 0x80000000) != 0) {
		int start = timer_get_current();
			
		do {
			if ((inl(0x70008A0C) & 0x80000000) == 0) 
				break;
		} while ((timer_get_current() - start) < 1000);
	}
}

static void lcd_send_lo(int v)
{
	lcd_wait_write();
	outl(v | 0x80000000, 0x70008A0C);
}

static void lcd_send_hi(int v)
{
	lcd_wait_write();
	outl(v | 0x81000000, 0x70008A0C);
}

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

static void lcd_update_display(uint16 *fb, int sx, int sy, int width, int height)
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

static long iPod_GetGeneration() 
{
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
	
	return strtol(ptr, NULL, 16);
}

void HD_LCD_Init() 
{
	hw_ver = iPod_GetGeneration() >> 16;
	switch (hw_ver) {
	case 0xB:
		lcd_width = 320;
		lcd_height = 240;
		lcd_type = 5;
		break;
	case 0xC:
		lcd_width = 176;
		lcd_height = 132;
		lcd_type = 1;
		break;
	case 0x6:
		lcd_width = 220;
		lcd_height = 176;

		int gpio_a01, gpio_a04;
		gpio_a01 = (inl(0x6000D030) & 0x2) >> 1;
		gpio_a04 = (inl(0x6000D030) & 0x10) >> 4;
		if (((gpio_a01 << 1) | gpio_a04) == 0 || ((gpio_a01 << 1) | gpio_a04) == 2) {
			lcd_type = 0;
		} else {
			lcd_type = 1;
		}		
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
void HD_LCD_Update (uint16 *fb, int x, int y, int w, int h) 
{
	switch (lcd_type) {
	case 0:
	case 1:
		lcd_update_display (fb, x, y, w, h);
		break;
	case 5:
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
