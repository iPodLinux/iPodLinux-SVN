
#include "tools.h"


/* wait for action button to be pressed and then released */
void
wait_for_action()
{
	/* wait for press */
	do {
		inl(0xcf000030);
	} while ( (inl(0xcf000030) & 0x2) != 0 );

	/* wait for release */
	do {
		inl(0xcf000030);
	} while ( (inl(0xcf000030) & 0x2) == 0 );
}


/* get current usec counter */
int
timer_get_current()
{
	return inl(0xcf001110);
}

/* check if number of seconds has past */
int
timer_check(int clock_start, int usecs)
{
	if ( (inl(0xcf001110) - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for r0 useconds */
int
wait_usec(int usecs)
{
	int start = inl(0xcf001110);

	while ( timer_check(start, usecs) == 0 ) {
		// empty
	}

	return 0;
}


/* wait for LCD with timeout */
void
lcd_wait_write()
{
	if ( (inl(0xc0001000) & 0x1) != 0 ) {
		int start = timer_get_current();

		do {
			if ( (inl(0xc0001000) & (unsigned int)0x8000) == 0 ) break;
		} while ( timer_check(start, 1000) == 0 );
	}
}


/* send LCD data */
void
lcd_send_data(int data_lo, int data_hi)
{
	lcd_wait_write();
	outl(0xc0001010, data_lo);
	lcd_wait_write();
	outl(0xc0001010, data_hi);
}

/* send LCD command */
void
lcd_prepare_cmd(int cmd)
{
	lcd_wait_write();
	outl(0xc0001008, 0x0);
	lcd_wait_write();
	outl(0xc0001008, cmd);
}

/* send LCD command and data */
void
lcd_cmd_and_data(int cmd, int data_lo, int data_hi)
{
	lcd_prepare_cmd(cmd);

	lcd_send_data(data_lo, data_hi);
}

static int lcd_inited;
static int ipod_rev;

/* initialise LCD hardware? */
static void
init_lcd()
{
	wait_usec(15);
	outl(0xcf005000, inl(0xcf005000) | 0x800);
	wait_usec(15);
	outl(0xcf005000, inl(0xcf005000) & ~0x400);
	wait_usec(15);

	outl(0xcf005030, inl(0xcf005030) | 0x400);

	/* delay for 15 usecs */
	wait_usec(15);

	outl(0xcf005030, inl(0xcf005030) & ~0x400);
	wait_usec(15);

	outl(0xc0001000, inl(0xc0001000) & ~0x4);

	/* delay for 15 usecs */
	wait_usec(15);

	outl(0xc0001000, inl(0xc0001000) | 0x4);

	/* delay for 15 usecs */
	wait_usec(15);

	outl(0xc0001000, (inl(0xc0001000) & ~0x10) | 0x4001);
	wait_usec(15);

	lcd_inited = 1;
}

/* reset the LCD */
void
reset_lcd()
{
	if ( lcd_inited == 0 ) {
		int i;

		// 0xcf005000 and 0xc0001000 initialisation
		init_lcd();

		lcd_inited = 1;

		i = inl(0x2084);
		if ( i == 0x30000 || i == 0x30001 ) ipod_rev = 3;
		else if ( i == 0x20000 || i == 0x20001 ) ipod_rev = 2;
		else ipod_rev = 1;
	}

	lcd_cmd_and_data(0x0, 0x0, 0x1);

	/* delay 10000 usecs */
	wait_usec(10000);


	lcd_cmd_and_data(0x1, 0x0, 0xf);
	lcd_cmd_and_data(0x2, 0x0, 0x55);
	lcd_cmd_and_data(0x3, 0x15, 0xc);
	lcd_cmd_and_data(0x4, 0x0, 0x0);
	lcd_cmd_and_data(0x5, 0x0, 0x0);
	lcd_cmd_and_data(0x6, 0x0, 0x0);
	lcd_cmd_and_data(0x7, 0x0, 0x0);
	lcd_cmd_and_data(0x8, 0x0, 0x4);
	lcd_cmd_and_data(0xb, 0x0, 0x0);
	lcd_cmd_and_data(0xc, 0x0, 0x0);
	lcd_cmd_and_data(0xd, 0xff, 0x0);
	lcd_cmd_and_data(0xe, 0x0, 0x0);
	lcd_cmd_and_data(0x10, 0x0, 0x0);

	/* move the cursor */
	lcd_cmd_and_data(0x11, 0, 0);

	/* the following two should vary per board */
	if ( ipod_rev == 3 ) {
		lcd_cmd_and_data(0x4, 0x4, 0x6c);
	}
	else if ( ipod_rev == 2 ) {
		lcd_cmd_and_data(0x4, 0x4, 0x5c);
	}
	else {
		lcd_cmd_and_data(0x4, 0x4, 0x5c);
	}
	lcd_cmd_and_data(0x7, 0x0, 0x9);
}

int
backlight_on_off(int on)
{
	int lcd_state = inl(0xc0001000);

	if ( on ) {
		lcd_state = lcd_state | 0x2;
	}
	else {
		lcd_state = lcd_state & ~0x2;
	}

	outl(0xc0001000, lcd_state);

	return 0x0;
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
	// nb. screen height is 128
	vert_space = 64 - (height_off_diff / 2);

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
		for ( r6 = 0; r6 < 168; r6 += 8 ) {
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

		for ( r6 = 0; r6 < 80 - (width_off_diff / 2); r6 += 8 ) {
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

		for ( r6 = 80 + (width_off_diff / 2); r6 < 168; r6 += 8 ) {
			// display background character
			lcd_send_data(bg, bg);
		}

		// update cursor pos counter
		cursor_pos += 32;
	}

	/* image is drawn */

	/* background the bottom half */

	// screen height is 128
	for ( cursor_pos = 64 + (height_off_diff / 2); cursor_pos <= 128; cursor_pos++ ) {
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
		for ( r6 = 0; r6 < 168; r6 += 8 ) {
			// display background character
			lcd_send_data(bg, bg);
		}
	}
	wait_usec(15);
}

