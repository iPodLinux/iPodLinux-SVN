
#include "tools.h"

#define IPOD_PP5002_LCD_BASE    0xc0001000
#define IPOD_PP5002_RTC         0xcf001110

#define IPOD_PP5020_LCD_BASE    0x70003000
#define IPOD_PP5020_RTC         0x60005010

#define LCD_DATA		0x10
#define LCD_CMD			0x08

#define IPOD_STD_LCD_WIDTH      160
#define IPOD_STD_LCD_HEIGHT     128

#define IPOD_MINI_LCD_WIDTH	138
#define IPOD_MINI_LCD_HEIGHT	110

#define HW_REV_MINI		4
#define HW_REV_4G		5


static unsigned long ipod_rtc_reg;
static unsigned long lcd_base;

static unsigned long lcd_width;
static unsigned long lcd_height;

extern int ipod_ver;

/* find out which ipod revision we're running on */
void
get_ipod_rev()
{
	unsigned long rev = inl(0x2084) >> 16;

	lcd_base = IPOD_PP5002_LCD_BASE;
	ipod_rtc_reg = IPOD_PP5002_RTC;
	lcd_width = IPOD_STD_LCD_WIDTH;
	lcd_height = IPOD_STD_LCD_HEIGHT;

	if (rev > 3) {
		lcd_base = IPOD_PP5020_LCD_BASE;
		ipod_rtc_reg = IPOD_PP5020_RTC;
	}

	if (rev == HW_REV_MINI) {
		lcd_width = IPOD_MINI_LCD_WIDTH;
		lcd_height = IPOD_MINI_LCD_HEIGHT;
	}

	ipod_ver = rev;
}

/* get current usec counter */
int
timer_get_current()
{
	return inl(ipod_rtc_reg);
}

/* check if number of seconds has past */
int
timer_check(int clock_start, int usecs)
{
	if ((inl(ipod_rtc_reg) - clock_start) >= usecs) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for r0 useconds */
int
wait_usec(int usecs)
{
	int start = inl(ipod_rtc_reg);

	while (timer_check(start, usecs) == 0) {
		// empty
	}

	return 0;
}


/* wait for LCD with timeout */
void
lcd_wait_write()
{
	if ((inl(lcd_base) & 0x8000) != 0) {
		int start = timer_get_current();

		do {
			if ((inl(lcd_base) & 0x8000) == 0) break;
		} while (timer_check(start, 1000) == 0);
	}
}


/* send LCD data */
void
lcd_send_data(int data_lo, int data_hi)
{
	lcd_wait_write();
	outl(data_lo, lcd_base + LCD_DATA);
	lcd_wait_write();
	outl(data_hi, lcd_base + LCD_DATA);
}

/* send LCD command */
void
lcd_prepare_cmd(int cmd)
{
	lcd_wait_write();
	outl(0x0, lcd_base + LCD_CMD);
	lcd_wait_write();
	outl(cmd, lcd_base + LCD_CMD);
}

/* send LCD command and data */
void
lcd_cmd_and_data(int cmd, int data_lo, int data_hi)
{
	lcd_prepare_cmd(cmd);

	lcd_send_data(data_lo, data_hi);
}

static unsigned char patterns[] = {
	0x00,
	0x03,
	0x0c,
	0x0f,
	0x30,
	0x33,
	0x3c,
	0x3f,
	0xc0,
	0xc3,
	0xcc,
	0xcf,
	0xf0,
	0xf3,
	0xfc,
	0xff
};

void
display_image(img *img, int draw_bg)
{
	unsigned int height_off_diff, width_off_diff, vert_space;
	unsigned short cursor_pos;
	unsigned char r7;

	if ( img == 0x0 ) return;

	height_off_diff = img->height - img->offy;
	width_off_diff = img->width - img->offx;

	// center the image vertically
	vert_space = (lcd_height/2) - (height_off_diff / 2);

	for ( cursor_pos = 0; vert_space > cursor_pos; cursor_pos = (cursor_pos + 1) & 0xff ) {
		int bg = 0;
		unsigned char r6;

		// move the cursor
		lcd_cmd_and_data(0x11, (cursor_pos << 5) >> 8, (cursor_pos << 5) & 0xff);

		// setup for print command
		lcd_prepare_cmd(0x12);

		// use a background pattern?
		if ( draw_bg != 0 ) {
			// background pattern
			if ( cursor_pos & 1 ) {
				bg = 0x33;
			} else {
				bg = 0xcc;
			}
		}

		/* print out line line of background */
		for ( r6 = 0; r6 < lcd_width; r6 += 8 ) {
			// display background character
			lcd_send_data(bg, bg);
		}
	}

	/* top half background is now drawn/cleared */

	cursor_pos = (vert_space << 5) & 0xffff;

	for ( r7 = 0; r7 < height_off_diff; r7++ ) {
		int bg = 0;
		unsigned char *img_data;
		unsigned char r6;

		// move the cursor
		lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);

		// setup for printing
		lcd_prepare_cmd(0x12);

		// use a background pattern?
		if ( draw_bg != 0 ) {
			// background pattern
			if ( r7 & 1 ) {
				bg = 0x33;
			} else {
				bg = 0xcc;
			}
		}

		for ( r6 = 0; r6 < (lcd_width/2) - (width_off_diff / 2); r6 += 8 ) {
			// display background character
			lcd_send_data(bg, bg);
		}

		// cursor pos * image data width
		img_data = &img->data[r7 * img->data_width];

		for ( r6 = 0; r6 < ((width_off_diff + 7) / 8); r6++ ) {

			if ( img->img_type == 1 ) {
				// display a character
				lcd_cmd_and_data(0x12, patterns[*img_data >> 4], patterns[*img_data & 0xf]);

				img_data++;
			}
			else if ( img->img_type == 2 ) {
				// display a character
				lcd_cmd_and_data(0x12, *img_data, *(img_data + 1));

				img_data += 2;
			}

		}

		for ( r6 = (lcd_width/2) + (width_off_diff / 2); r6 <= lcd_width; r6 += 8 ) {
			// display background character
			lcd_send_data(bg, bg);
		}

		// update cursor pos counter
		cursor_pos += 32;
	}

	/* image is drawn */

	/* background the bottom half */

	for ( cursor_pos = 64 + (height_off_diff / 2); cursor_pos <= lcd_height; cursor_pos++ ) {
		int bg = 0;
		unsigned char r6;

		// move the cursor
		lcd_cmd_and_data(0x11, (cursor_pos << 5) >> 8, (cursor_pos << 5) & 0xff);

		// setup for printing
		lcd_prepare_cmd(0x12);

		// use a background pattern?
		if ( draw_bg != 0 ) {

			// background pattern
			if ( cursor_pos & 1 ) {
				bg = 0x33;
			} else {
				bg = 0xcc;
			}

		}

		/* print out a line of background */
		for ( r6 = 0; r6 <= lcd_width; r6 += 8 ) {
			// display background character
			lcd_send_data(bg, bg);
		}
	}
	lcd_cmd_and_data(0x11, 0, 0);
	lcd_send_data(0xff, 0xff);
	wait_usec(15);
}

