/*
 * fb.c - Frame-buffer driver for iPod
 *
 * Copyright (c) 2003,2004 Bernard Leach (leachbj@bouncycastle.org)
 *
 * The LCD uses the HD66753 controller from Hitachi (now owned by Renesas).
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
#include <linux/ioctl.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb2.h>

/* the ID returned by the controller */
#define HD66753_ID	0x5307

#define IPOD_LCD_BASE	0xc0001000
#define IPOD_RTC	0xcf001110

#define IPOD_LCD_WIDTH	160
#define IPOD_LCD_HEIGHT	128

/* allow for 2bpp */
static char ipod_scr[IPOD_LCD_HEIGHT * (IPOD_LCD_WIDTH/4)];


/* get current usec counter */
static int
timer_get_current(void)
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
lcd_wait_write(void)
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

static unsigned
get_contrast(void)
{
	unsigned data_lo, data_hi;

	/* data_lo has the scan line */
	lcd_wait_write();
	data_lo = inl(0xc0001008);
	/* data_hi has the contrast */
	lcd_wait_write();
	data_hi = inl(0xc0001008);

	return data_hi & 0xff;
}

static void
set_contrast(int contrast)
{
	lcd_cmd_and_data(0x4, 0x4, contrast);
}

static int
get_backlight(void)
{
	return inl(IPOD_LCD_BASE) & 0x2 ? 1 : 0;
}

static void
set_backlight(int on)
{
	int lcd_state = inl(IPOD_LCD_BASE);

	if (on) {
		lcd_state = lcd_state | 0x2;
		outl(lcd_state, IPOD_LCD_BASE);

		/* display control (1 00 0 1) */
		/* GSH=01 -> 2/3 level grayscale control */
		/* GSL=00 -> 1/4 level grayscale control */
		/* REV=0 -> don't reverse */
		/* D=1 -> display on */
		lcd_cmd_and_data(0x7, 0x0, 0x11 | 0x2);
	}
	else {
		lcd_state = lcd_state & ~0x2;
		outl(lcd_state, IPOD_LCD_BASE);

		/* display control (10 0 1) */
		/* GSL=10 -> 2/4 level grayscale control */
		/* REV=0 -> don't reverse */
		/* D=1 -> display on */
		lcd_cmd_and_data(0x7, 0x0, 0x9);
	}
}

static unsigned
read_controller_id(void)
{
	unsigned data_lo, data_hi;

	/* read the Start Osciallation register -> it gives us a id */
	lcd_prepare_cmd(0x0);

	lcd_wait_write();
	data_lo = inl(0xc0001010);
	lcd_wait_write();
	data_hi = inl(0xc0001010);

	return ((data_hi & 0xff) << 8) | (data_lo & 0xff);
}

/* initialise the LCD */
static void
init_lcd(void)
{
	if ( read_controller_id() != HD66753_ID )  {
		printk(KERN_ERR "Unknown LCD controller ID: 0x%x id?\n", read_controller_id());
	}

	/* driver output control - 168x128 -> we use 160x128 */
	/* CMS=0, SGS=1 */
	lcd_cmd_and_data(0x1, 0x1, 0xf);

	/* ID=1 -> auto decrement address counter */
	/* AM=00 -> data is continuously written in parallel */
	/* LG=00 -> no logical operation */
	lcd_cmd_and_data(0x5, 0x0, 0x10);
}


void contrast_up(void)
{
	int contrast = get_contrast();
	if ( contrast < 0xff ) {
		contrast++; 
		lcd_cmd_and_data(0x4, 0x4, contrast);

#if 0
		printk("ctrst=0x%x\n", get_contrast());
#endif
	}
}

void contrast_down(void)
{
	int contrast = get_contrast();
	if ( contrast > 0 ) {
		contrast--; 
		lcd_cmd_and_data(0x4, 0x4, contrast);

#if 0
		printk("ctrst=0x%x\n", get_contrast());
#endif
	}
}

static void ipod_update_display(struct display *p, int sx, int sy, int mx, int my)
{
	unsigned short cursor_pos;
	unsigned short y;

	cursor_pos = sx + (sy * fontheight(p) * 0x20);

	for ( y = sy * fontheight(p); y < my * fontheight(p); y++ ) {
		unsigned char *img_data;
		unsigned char x;

		/* move the cursor */
		lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);

		/* setup for printing */
		lcd_prepare_cmd(0x12);

		/* cursor pos * image data width */
		img_data = &ipod_scr[y * p->line_length + sx * 2];

		/* 160/8 -> 20 == loops 20 times */
		for ( x = sx; x < mx; x++ ) {
			/* display a character */
			lcd_send_data(*(img_data + 1), *img_data);

			img_data += 2;
		}

		/* update cursor pos counter */
		cursor_pos += 0x20;
	}
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


void ipod_fb_setup(struct display *p)
{
	fbcon_cfb2.setup(p);
}

void ipod_fb_bmove(struct display *p, int sy, int sx, int dy, int dx,
		     int height, int width)
{
	fbcon_cfb2.bmove(p, sy, sx, dy, dx, height, width);
	ipod_update_display(p, 0, 0, IPOD_LCD_WIDTH/8, IPOD_LCD_HEIGHT/fontheight(p));
}

void ipod_fb_clear(struct vc_data *conp, struct display *p, int sy, int sx,
		     int height, int width)
{
	fbcon_cfb2.clear(conp, p, sy, sx, height, width);
	ipod_update_display(p, sx, sy, sx+width, sy+height);
}

void ipod_fb_putc(struct vc_data *conp, struct display *p, int c, int yy,
		    int xx)
{
	fbcon_cfb2.putc(conp, p, c, yy, xx);
	ipod_update_display(p, xx, yy, xx+1, yy+1);
}

void ipod_fb_putcs(struct vc_data *conp, struct display *p, 
		     const unsigned short *s, int count, int yy, int xx)
{
	fbcon_cfb2.putcs(conp, p, s, count, yy, xx);
	ipod_update_display(p, xx, yy, xx+count, yy+1);
}

void ipod_fb_revc(struct display *p, int xx, int yy)
{
	fbcon_cfb2.revc(p, xx, yy);
	ipod_update_display(p, xx, yy, xx+1, yy+1);
}


/*
 *  `switch' for the low level operations
 */

struct display_switch fbcon_ipod = {
	setup:		ipod_fb_setup,
	bmove:		ipod_fb_bmove,
	clear:		ipod_fb_clear,
	putc:		ipod_fb_putc,
	putcs:		ipod_fb_putcs,
	revc:		ipod_fb_revc,
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

	fix->visual = FB_VISUAL_PSEUDOCOLOR;	/* fixed visual */
	fix->line_length = IPOD_LCD_WIDTH >> 2;	/* cfb2 default */

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

	if ( var->xres > IPOD_LCD_WIDTH ||
		var->yres > IPOD_LCD_HEIGHT ||
		var->xres_virtual != var->xres ||
		var->yres_virtual != var->yres ||
		var->xoffset != 0 ||
		var->yoffset != 0 ) {
		return -EINVAL;
	}

	if ( var->bits_per_pixel != 2 ) {
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

	var->bits_per_pixel = 2;
	var->grayscale = 1;

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
	switch (blank_mode) {
	case VESA_NO_BLANKING:
		/* printk(KERN_ERR "VESA_NO_BLANKING\n"); */

		/* start oscillation
		 * wait 10ms
		 * cancel standby
		 * turn on LCD power
		 */
		lcd_cmd_and_data(0x0, 0x0, 0x1);
		udelay(10000);
		lcd_cmd_and_data(0x3, 0x15, 0x0);
		lcd_cmd_and_data(0x3, 0x15, 0xc);
		lcd_cmd_and_data(0x7, 0x0, 0x11);
		break;

	case VESA_VSYNC_SUSPEND:
	case VESA_HSYNC_SUSPEND:
		/* printk(KERN_ERR "VESA_XSYNC_BLANKING\n"); */

		/* go to SLP = 1 */
		/* 10101 00001100 */
		lcd_cmd_and_data(0x3, 0x15, 0x0);
		lcd_cmd_and_data(0x3, 0x15, 0x2);
		break;

	case VESA_POWERDOWN:
		/* printk(KERN_ERR "VESA_POWERDOWN\n"); */

		/* got to standby */
		lcd_cmd_and_data(0x3, 0x15, 0x1);
		break;

	default:
		/* printk(KERN_ERR "unknown blank value %d\n", blank_mode); */
		return -EINVAL;
	}

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
	disp->dispsw = &fbcon_ipod;
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
	return 0;
}

#define FBIOGET_CONTRAST	_IOR('F', 0x22, int)
#define FBIOPUT_CONTRAST	_IOW('F', 0x23, int)

#define FBIOGET_BACKLIGHT	_IOR('F', 0x24, int)
#define FBIOPUT_BACKLIGHT	_IOW('F', 0x25, int)

#define IPOD_MIN_CONTRAST 0
#define IPOD_MAX_CONTRAST 0x7f

static int ipod_fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
	u_long arg, int con, struct fb_info *info)

{
	int val;

	switch (cmd) {
	case FBIOGET_CONTRAST:
		val = get_contrast();
		if (put_user(val, (int *)arg))
			return -EFAULT;
		break;

	case FBIOPUT_CONTRAST:
		val = (int)arg;
		if (val < IPOD_MIN_CONTRAST || val > IPOD_MAX_CONTRAST)
			return -EINVAL;
		set_contrast(val);
		break;

	case FBIOGET_BACKLIGHT:
		val = get_backlight();
		if (put_user(val, (int *)arg))
			return -EFAULT;
		break;

	case FBIOPUT_BACKLIGHT:
		val = (int)arg;
		set_backlight(val);
		break;

	default:
		return -EINVAL;
	}

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
	fb_ioctl:	ipod_fb_ioctl,
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

	init_lcd();

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

