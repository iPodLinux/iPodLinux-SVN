/*
 * fb.c - Frame-buffer driver for iPod
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <asm/io.h>

#include <video/fbcon.h>
#ifdef CONFIG_FBCON_MFB
#include <video/fbcon-mfb.h>
#endif
#ifdef CONFIG_FBCON_CFB2
#include <video/fbcon-cfb2.h>
#endif

#define IPOD_LCD_BASE	0xc0001000
#define IPOD_RTC	0xcf001110
#define IPOD_LCD_WIDTH	160
#define IPOD_LCD_HEIGHT	128

/* get current usec counter */
static int
timer_get_current()
{
	return inl(IPOD_RTC);
}

/* check if number of useconds has past */
static int
timer_check(int clock_start, int usecs)
{
	if ( (inl(IPOD_RTC) - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for LCD with timeout */
static void
lcd_wait_write()
{
	if ( (inl(IPOD_LCD_BASE) & 0x1) != 0 ) {
		int start = timer_get_current();

		do {
			if ( (inl(IPOD_LCD_BASE) & (unsigned int)0x8000) == 0 ) break;
		} while ( timer_check(start, 1000) == 0 );
	}
}


/* send LCD data */
static void
lcd_send_data(int data_lo, int data_hi)
{
	lcd_wait_write();
	outl(data_lo, 0xc0001010);
	lcd_wait_write();
	outl(data_hi, 0xc0001010);
}

/* send LCD command */
static void
lcd_prepare_cmd(int cmd)
{
	lcd_wait_write();
	outl(0x0, 0xc0001008);
	lcd_wait_write();
	outl(cmd, 0xc0001008);
}

/* send LCD command and data */
static void
lcd_cmd_and_data(int cmd, int data_lo, int data_hi)
{
	lcd_prepare_cmd(cmd);

	lcd_send_data(data_lo, data_hi);
}

static int lcd_inited;

/* initialise LCD hardware? */
static void
init_lcd()
{
	outl(inl(0xcf005000) | 0x800, 0xcf005000);
	outl(inl(0xcf005000) & ~0x400, 0xcf005000);

	outl(inl(0xcf005030) | 0x400, 0xcf005030);
	udelay(15);
	outl(inl(0xcf005030) & ~0x400, 0xcf005030);

	outl(inl(IPOD_LCD_BASE) & ~0x4, IPOD_LCD_BASE);
	udelay(15);
	outl(inl(IPOD_LCD_BASE) | 0x4, IPOD_LCD_BASE);

	udelay(15);

	outl((inl(IPOD_LCD_BASE) & ~0x10) | 0x4001, IPOD_LCD_BASE);

	lcd_inited = 1;
}

/* reset the LCD */
static void
reset_lcd()
{
	if ( lcd_inited == 0 ) {
		init_lcd();

		lcd_inited = 1;
	}

#if 1
	lcd_cmd_and_data(0x0, 0x0, 0x1);

	/* delay 10000 usecs */
	udelay(10000);

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
	lcd_cmd_and_data(0x4, 0x4, 0x5c);

	lcd_cmd_and_data(0x7, 0x0, 0x9);
#else
	lcd_cmd_and_data(0x0, 0x0, 0x1);

	/* delay 10000 usecs */
	udelay(10000);

	lcd_cmd_and_data(0x3, 0x15, 0x0);
	lcd_cmd_and_data(0x3, 0x15, 0xc);

	// if (backlight_off)
	lcd_cmd_and_data(0x7, 0x0, 0x9);
	// else
	//lcd_cmd_and_data(0x7, 0x0, 0x11);

	lcd_cmd_and_data(0x5, 0x0, 0x14);
#endif
}

/* allow for 2bpp */
static char ipod_scr[IPOD_LCD_HEIGHT * (IPOD_LCD_WIDTH/4)];

int
backlight_on_off(int on)
{
        int lcd_state = inl(IPOD_LCD_BASE);

        if ( on ) {
                lcd_state = lcd_state | 0x2;

		lcd_cmd_and_data(0x7, 0x0, 0x11);
        }
        else {
                lcd_state = lcd_state & ~0x2;

		lcd_cmd_and_data(0x7, 0x0, 0x9);
        }

        outl(lcd_state, IPOD_LCD_BASE);

        return 0x0;
}

struct ipodfb_info {
	/*
	 *  Choose _one_ of the two alternatives:
	 *
	 *    1. Use the generic frame buffer operations (fbgen_*).
	 */
	struct fb_info_gen gen;

#if 0
	/*
	 *    2. Provide your own frame buffer operations.
	 */
	struct fb_info info;
#endif

	/* Here starts the frame buffer device dependent part */
	/* You can use this to store e.g. the board number if you support */
	/* multiple boards */
};


struct ipodfb_par {
	/*
	 *  The hardware specific data in this structure uniquely defines a video
	 *  mode.
	 *
	 *  If your hardware supports only one video mode, you can leave it empty.
	 */
};

/* XXX ok, this is not good */
unsigned char reverse_byte(unsigned char c)
{
	return ((c & 1) << 7) | ((c & 2) << 5) | ((c & 4) << 3) | ((c & 8) << 1) | ((c & 16) >> 1) | ((c & 32) >> 3) | ((c & 64) >> 5) | ((c & 128) >> 7);
}

static void ipod_update_display(struct display *p)
{
	unsigned short cursor_pos;
	unsigned short y;

	for ( y = 0, cursor_pos = 32; y < IPOD_LCD_HEIGHT; y++ ) {
		unsigned char *img_data;
		unsigned char r6;

		// move the cursor
		lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);

		// setup for printing
		lcd_prepare_cmd(0x12);

		lcd_send_data(0x0, 0x0);

		// cursor pos * image data width
		img_data = &ipod_scr[y * p->line_length];

		// 160/8 -> 20 == loops 20 times
		for ( r6 = 0; r6 < (IPOD_LCD_WIDTH / 8); r6++ ) {
			if ( p->var.bits_per_pixel == 2 ) {
				unsigned char c1 = 0, c2 = 0;
				c1 = reverse_byte(*(img_data + 0));
				c2 = reverse_byte(*(img_data + 1));
				lcd_send_data(c1, c2);

				// display a character
				// lcd_send_data(*img_data, *(img_data + 1));
				// lcd_send_data(*(img_data + 1), *(img_data + 0));
				img_data += 2;
			}
			else {
				unsigned short c1 = *img_data++;
				unsigned short c2 = 0;
				int i;

				for ( i = 0; i < 8; i ++ ) {
					unsigned short t;
					
					t = (c1 & (1 << i));
					c2 |= (t << i);
					c2 |= (t << (i+1));
				}
				lcd_send_data((c2 >> 8) & 0xff, c2 & 0xff);
			}
		}

		// update cursor pos counter
		cursor_pos += 32;
	}
}

void ipod_mfb_setup(struct display *p)
{
	switch (p->var.bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
		case 1:
			fbcon_mfb.setup(p);
			break;
#endif

#ifdef CONFIG_FBCON_CFB2
		case 2:
			fbcon_cfb2.setup(p);
			break;
#endif
	}
}

void ipod_mfb_bmove(struct display *p, int sy, int sx, int dy, int dx,
		     int height, int width)
{
	switch (p->var.bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
		case 1:
			fbcon_mfb.bmove(p, sy, sx, dy, dx, height, width);
			break;
#endif

#ifdef CONFIG_FBCON_CFB2
		case 2:
			fbcon_cfb2.bmove(p, sy, sx, dy, dx, height, width);
			break;
#endif
	}

	ipod_update_display(p);
}

void ipod_mfb_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		     int height, int width)
{
	switch (p->var.bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
		case 1:
			fbcon_mfb.clear(conp, p, sy, sx, height, width);
			break;
#endif

#ifdef CONFIG_FBCON_CFB2
		case 2:
			fbcon_cfb2.clear(conp, p, sy, sx, height, width);
			break;
#endif
	}

	ipod_update_display(p);
}

void ipod_mfb_putc(struct vc_data *conp, struct display *p, int c, int yy,
		    int xx)
{
	switch (p->var.bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
		case 1:
			fbcon_mfb.putc(conp, p, c, yy, xx);
			break;
#endif

#ifdef CONFIG_FBCON_CFB2
		case 2:
			fbcon_cfb2.putc(conp, p, c, yy, xx);
			break;
#endif
	}

	ipod_update_display(p);
}

void ipod_mfb_putcs(struct vc_data *conp, struct display *p, 
		     const unsigned short *s, int count, int yy, int xx)
{
	switch (p->var.bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
		case 1:
			fbcon_mfb.putcs(conp, p, s, count, yy, xx);
			break;
#endif

#ifdef CONFIG_FBCON_CFB2
		case 2:
			fbcon_cfb2.putcs(conp, p, s, count, yy, xx);
			break;
#endif
	}

	ipod_update_display(p);
}

void ipod_mfb_revc(struct display *p, int xx, int yy)
{
	switch (p->var.bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
		case 1:
			fbcon_mfb.revc(p, xx, yy);
			break;
#endif

#ifdef CONFIG_FBCON_CFB2
		case 2:
			fbcon_cfb2.revc(p, xx, yy);
			break;
#endif
	}

	ipod_update_display(p);
}

void ipod_mfb_clear_margins(struct vc_data *conp, struct display *p,
			     int bottom_only)
{
	switch (p->var.bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
		case 1:
			fbcon_mfb.clear_margins(conp, p, bottom_only);
			break;
#endif

/* CFB2 doesnt have a clear_margins */
#if 0
#ifdef CONFIG_FBCON_CFB2
		case 2:
			fbcon_cfb2.clear_margins(conp, p, bottom_only);
			break;
#endif
#endif
	}

	ipod_update_display(p);
}


    /*
     *  `switch' for the low level operations
     */

struct display_switch fbcon_ipod = {
    setup:		ipod_mfb_setup,
    bmove:		ipod_mfb_bmove,
    clear:		ipod_mfb_clear,
    putc:		ipod_mfb_putc,
    putcs:		ipod_mfb_putcs,
    revc:		ipod_mfb_revc,
    clear_margins:	ipod_mfb_clear_margins,
    fontwidthmask:	FONTWIDTH(8)
};

static struct ipodfb_info fb_info;
static struct ipodfb_par current_par;
static int current_par_valid = 0;
static struct display disp;

static struct fb_var_screeninfo default_var;

int ipodfb_init(void);
int ipodfb_setup(char*);

/* ------------------- chipset specific functions -------------------------- */

static void ipod_get_par(struct ipodfb_par *, const struct fb_info *);
static int ipod_encode_var(struct fb_var_screeninfo *, struct ipodfb_par *, const struct fb_info *);

static void ipod_detect(void)
{
	/*
	 *  This function should detect the current video mode settings and store
	 *  it as the default video mode
	 */

	struct ipodfb_par par;

	ipod_get_par(&par, NULL);
	ipod_encode_var(&default_var, &par, NULL);
}

static int ipod_encode_fix(struct fb_fix_screeninfo *fix, struct ipodfb_par *par,
			  const struct fb_info *info)
{
	/*
	 *  This function should fill in the 'fix' structure based on the values
	 *  in the `par' structure.
	 */

	memset(fix, 0x0, sizeof(*fix));

	strcpy(fix->id, "iPod");
	fix->type= FB_TYPE_PACKED_PIXELS;

#ifdef CONFIG_FBCON_CFB2
	fix->visual = FB_VISUAL_PSEUDOCOLOR;	/* fixed visual */
	fix->line_length = IPOD_LCD_WIDTH >> 2;	/* cfb2 default */
#endif

#ifdef CONFIG_FBCON_MFB
	fix->visual = FB_VISUAL_MONO10;	/* fixed visual */
	fix->line_length = IPOD_LCD_WIDTH >> 3;	/* mfb default */
#endif

	fix->xpanstep = 0;	/* no hardware panning */
	fix->ypanstep = 0;	/* no hardware panning */
	fix->ywrapstep = 0;	/* */

	fix->accel = FB_ACCEL_NONE;

	return 0;
}

static int ipod_decode_var(struct fb_var_screeninfo *var, struct ipodfb_par *par,
			  const struct fb_info *info)
{
	/*
	 *  Get the video params out of 'var'. If a value doesn't fit, round it up,
	 *  if it's too big, return -EINVAL.
	 *
	 *  Suggestion: Round up in the following order: bits_per_pixel, xres,
	 *  yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
	 *  bitfields, horizontal timing, vertical timing.
	 */

	if ( var->xres > IPOD_LCD_HEIGHT ||
		var->yres > IPOD_LCD_WIDTH ||
		var->xres_virtual != var->xres ||
		var->yres_virtual != var->yres ||
		var->xoffset != 0 ||
		var->yoffset != 0 ) {
		return -EINVAL;
	}

	if ( var->bits_per_pixel != 1 ||
		var->bits_per_pixel != 2 ) {
		return -EINVAL;
	}

	return 0;
}

static int ipod_encode_var(struct fb_var_screeninfo *var, struct ipodfb_par *par,
			  const struct fb_info *info)
{
	/*
	 *  Fill the 'var' structure based on the values in 'par' and maybe other
	 *  values read out of the hardware.
	 */

	var->xres = IPOD_LCD_WIDTH;
	var->yres = IPOD_LCD_HEIGHT;
	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres;
	var->xoffset = 0;
	var->yoffset = 0;

#ifdef CONFIG_FBCON_CFB2
	var->bits_per_pixel = 2;
#endif
#ifdef CONFIG_FBCON_MFB
	var->bits_per_pixel = 1;
#endif
	var->grayscale = 0;

	return 0;
}

static void ipod_get_par(struct ipodfb_par *par, const struct fb_info *info)
{
	/*
	 *  Fill the hardware's 'par' structure.
	 */

	if ( current_par_valid ) {
		*par = current_par;
	}
	else {
		/* ... */
	}
}

static void ipod_set_par(struct ipodfb_par *par, const struct fb_info *info)
{
	/*
	 *  Set the hardware according to 'par'.
	 */

	current_par = *par;
	current_par_valid = 1;

	/* ... */
}

static int ipod_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			 unsigned *blue, unsigned *transp,
			 const struct fb_info *info)
{
	/*
	 *  Read a single color register and split it into colors/transparent.
	 *  The return values must have a 16 bit magnitude.
	 *  Return != 0 for invalid regno.
	 */

	/* ... */
	return 0;
}

static int ipod_setcolreg(unsigned regno, unsigned red, unsigned green,
			 unsigned blue, unsigned transp,
			 const struct fb_info *info)
{
	/*
	 *  Set a single color register. The values supplied have a 16 bit
	 *  magnitude.
	 *  Return != 0 for invalid regno.
	 */

	/* ... */
	return 0;
}

static int ipod_pan_display(struct fb_var_screeninfo *var,
			   struct ipodfb_par *par, const struct fb_info *info)
{
	/*
	 *  Pan (or wrap, depending on the `vmode' field) the display using the
	 *  `xoffset' and `yoffset' fields of the `var' structure.
	 *  If the values don't fit, return -EINVAL.
	 */

	/* ... */
	return -EINVAL;
}

static int ipod_blank(int blank_mode, const struct fb_info *info)
{
	/*
	 *  Blank the screen if blank_mode != 0, else unblank. If blank == NULL
	 *  then the caller blanks by setting the CLUT (Color Look Up Table) to all
	 *  black. Return 0 if blanking succeeded, != 0 if un-/blanking failed due
	 *  to e.g. a video mode which doesn't support it. Implements VESA suspend
	 *  and powerdown modes on hardware that supports disabling hsync/vsync:
	 *    blank_mode == 2: suspend vsync
	 *    blank_mode == 3: suspend hsync
	 *    blank_mode == 4: powerdown
	 */

	/* ... */
	return 0;
}

static void ipod_set_disp(const void *par, struct display *disp,
			 struct fb_info_gen *info)
{
	/*
	 *  Fill in a pointer with the virtual address of the mapped frame buffer.
	 *  Fill in a pointer to appropriate low level text console operations (and
	 *  optionally a pointer to help data) for the video mode `par' of your
	 *  video hardware. These can be generic software routines, or hardware
	 *  accelerated routines specifically tailored for your hardware.
	 *  If you don't have any appropriate operations, you must fill in a
	 *  pointer to dummy operations, and there will be no text output.
	 */

	disp->screen_base = ipod_scr;

	switch (disp->var.bits_per_pixel) {
#ifdef CONFIG_FBCON_MFB
		case 1:
			disp->dispsw = &fbcon_ipod;
			break;
#endif

#ifdef CONFIG_FBCON_CFB2
		case 2:
			disp->dispsw = &fbcon_ipod;
			break;
#endif

		default:
#if !defined(CONFIG_FBCON_MFB) && !defined(CONFIG_FBCON_CFB2)
#warning "No FB console handler!"
#endif
			disp->dispsw = &fbcon_dummy;
			break;
	}
}


/* ------------ Interfaces to hardware functions ------------ */


struct fbgen_hwswitch ipod_switch = {
	detect:		ipod_detect,
	encode_fix:	ipod_encode_fix,
	decode_var:	ipod_decode_var,
	encode_var:	ipod_encode_var,
	get_par:	ipod_get_par,
	set_par:	ipod_set_par,
	getcolreg:	ipod_getcolreg,
	setcolreg:	ipod_setcolreg,
	pan_display:	ipod_pan_display,
	blank:		ipod_blank,
	set_disp:	ipod_set_disp,
};


/* ------------------------------------------------------------------------- */


    /*
     *  Frame buffer operations
     */

static int ipod_fp_open(const struct fb_info *info, int user)
{
	reset_lcd();
	return 0;
}

    /*
     *  In most cases the `generic' routines (fbgen_*) should be satisfactory.
     *  However, you're free to fill in your own replacements.
     */

static struct fb_ops ipodfb_ops = {
	owner:		THIS_MODULE,
	fb_open:	ipod_fp_open,
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	fbgen_set_cmap,
	fb_pan_display:	fbgen_pan_display,
};


/* ------------ Hardware Independent Functions ------------ */


    /*
     *  Initialization
     */

int __init ipodfb_init(void)
{
	fb_info.gen.fbhw = &ipod_switch;

	fb_info.gen.fbhw->detect();

	strcpy(fb_info.gen.info.modename, "iPod");

	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.node = -1;
	fb_info.gen.info.fbops = &ipodfb_ops;
	fb_info.gen.info.disp = &disp;
	fb_info.gen.info.switch_con = &fbgen_switch;
	fb_info.gen.info.updatevar = &fbgen_update_var;
	fb_info.gen.info.blank = &fbgen_blank;
	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;

	/* This should give a reasonable default video mode */
	fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
	fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
	fbgen_set_disp(-1, &fb_info.gen);
	fbgen_install_cmap(0, &fb_info.gen);

	if ( register_framebuffer(&fb_info.gen.info) < 0 ) {
		return -EINVAL;
	}

	printk(KERN_INFO "fb%d: %s frame buffer device\n", GET_FB_IDX(fb_info.gen.info.node), fb_info.gen.info.modename);

	/* uncomment this if your driver cannot be unloaded */
	/* MOD_INC_USE_COUNT; */
	return 0;
}


    /*
     *  Cleanup
     */

void ipodfb_cleanup(struct fb_info *info)
{
	/*
	 *  If your driver supports multiple boards, you should unregister and
	 *  clean up all instances.
	 */

	unregister_framebuffer(info);
	/* ... */
}


    /*
     *  Setup
     */

int __init ipodfb_setup(char *options)
{
	/* Parse user speficied options (`video=ipodfb:') */
	return 0;
}



/* ------------------------------------------------------------------------- */


    /*
     *  Modularization
     */

#ifdef MODULE
MODULE_LICENSE("GPL");
int init_module(void)
{
    return ipodfb_init();
}

void cleanup_module(void)
{
    ipodfb_cleanup(void);
}
#endif /* MODULE */
