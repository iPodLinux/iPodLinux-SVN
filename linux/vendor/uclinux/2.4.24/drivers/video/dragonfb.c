/*
 * linux/drivers/video/dragonfb.c -- Frame Buffer driver for the 68EZ328 & 68VZ328
 *
 *  (C) 03 Dec 2001 by Khaled Hassounah <khassounah@mediumware.net>
 *
 *  Contributors:
 *
 *  29 March 2002 by Daniel Haensse <daniel.haensse@alumni.ethz.ch),
 *  added support for the 68vz328 and some minor changes
 *
 *  Following are few comments about this driver, if you fix any of the issues
 *  please remove them from this list:
 *
 *  1) Most of the components to support the standard frame buffer console
 *     exist, but it hasn't been tested before. So probably you'll need to
 *     do some debugging before you get working
 *
 *  2) The LCD settings (resolution, color depth, etc.) have been dealt with
 *     as configuration settings as opposed to user settings because we are
 *     with an LCD in which those settings are mostly constant
 *
 *  3) The driver has been tested on a 68EZ328 with a 320X240 monochrome
 *     LCD.
 *
 *  4) No color support yet, I'll try to work on it, if you get this version
 *     check again later (or check again ;-) and you might find it, otherwise
 *     maybe you want to make some smiles in the open source community and do
 *     it yourself ;-)
 *
 *  5) This is effort is much less than perfect, so have fun :)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

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

#include <video/fbcon.h>

#include <video/fbcon-mfb.h>

// include display controller specific stuff, be aware of the differences
#if defined(CONFIG_M68EZ328)
#include <asm/MC68EZ328.h>
#elif defined(CONFIG_M68VZ328)
#include <asm/MC68VZ328.h>
#endif

//#define DEBUG

//const int config_xres = 0;
//const int config_yres = 0;
//const int config_bpp = 0;
//const int config_buswidth = 0;

// Define diplay type
#if defined(CONFIG_UG24U01WGHT3L)
        // Display driving
        #define X_RES 240       // X-Size
        #define Y_RES 320       // Y-Size
        #define REFRATE 20    // See motorola manual LRRA
        #define PIXRATE 0    // See motorola manual LPXCD
        #define ACRATE  0x00    // See motorola manual LACDRC
        #define GRAYSCALE 0x73  // See motorola manual LGPMR
        #define POLARITY 0x00   // See motorola manual LPOLCF
        #define BUSWIDTH_LPICF 0x08 // LPICF: 4 bit interface, 1bit = 0x00, 2bit=0x04,  4bit= 0x08, 8bit= 0x0c (VZ only)
#elif define(CONFIG_UG24U03WGHT3A)
        // Display driving
        #define X_RES 320       // X-Size
        #define Y_RES 240       // Y-Size
        #define REFRATE 20    // See motorola manual LRRA
        #define PIXRATE 0    // See motorola manual LPXCD
        #define ACRATE  0x00    // See motorola manual LACDRC
        #define GRAYSCALE 0x73  // See motorola manual LGPMR
        #define POLARITY 0x00   // See motorola manual LPOLCF
        #define BUSWIDTH_LPICF 0x08 // LPICF: 4 bit interface, 1bit = 0x00, 2bit=0x04,  4bit= 0x08, 8bit= 0x0c (VZ only)
#elif define(CONFIG_UG32F24CMGF3A)
        // Display driving
        #define X_RES 960       // X-Size 320xRGB
        #define Y_RES 240       // Y-Size
        #define REFRATE 20    // See motorola manual LRRA
        #define PIXRATE 1    // See motorola manual LPXCD
        #define ACRATE  0x00    // See motorola manual LACDRC
        #define GRAYSCALE 0x73  // See motorola manual LGPMR
        #define POLARITY 0x00   // See motorola manual LPOLCF
        #define BUSWIDTH_LPICF 0x0c // LPICF: 4 bit interface, 1bit = 0x00, 2bit=0x04,  4bit= 0x08, 8bit= 0x0c (VZ only)
#endif

#if defined(CONFIG_FB_DRAGON_MONO)
	#define BPP 1
        #define GRAY_LPICF      0x00
#elif defined(CONFIG_FB_DRAGON_4GRAY)
	#define BPP 2
        #define GRAY_LPICF      0x01
#elif defined(CONFIG_FB_DRAGON_16GRAY) // Well 16GRAY does not mean that we can use 16 gray scales, we have a better resolution of the pixel density only
	#define BPP 2
        #define GRAY_LPICF      0x02
#endif






/* few useful macros */

#define LINE_LENGTH(x, bpp) (((x*bpp+31)&-32)>>3)
#define LENGTH(x, y, bpp) (x * y * bpp / 8)

/* The video ram, which should be declared in the linker script */

extern int __vram_start;

/* few structs holding default or initialization info */
static struct fb_info fb_info;
static struct display disp;
static int display_initialized[MAX_NR_CONSOLES];

static struct fb_var_screeninfo default_var = {
	xres:		X_RES,	/* until I define the mechanism for passing resolutions */
	yres:		Y_RES,	/* ditto */

	/* Dragonballs (EZ & VZ) support virtual screen, but I'm in a hurry
	   to implement it, if you need it, check out the the LVPW register
	   and add the support, it should be pretty easy, all you need is
	   to modify the dragon_set_var method to allow it */

	xres_virtual:	X_RES,	/* too early for virtual support */
	yres_virtual:	Y_RES,	/* ditto */
	xoffset:	0,		/* ditto */
	yoffset:	0,		/* ditto */
	bits_per_pixel:	BPP,	/* see xres */
	grayscale:	0,		/* now monochrome tomorrow grayscale :-) */

	red:		{0,1,0},	/* only length is needed (not true color) */
	green:		{0,1,0},	/* ditto */
	blue:		{0,1,0},	/* ditto */
	transp:		{0,0,0},	/* we don't support transparency */

	activate:	FB_ACTIVATE_NOW,
	height:		-1,		/* is this really important? might be */
	width:		-1,		/* later */

	accel_flags:	FB_ACCEL_NONE,

	vmode:		FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo current_fix = {
	id:		"Dragon",
	smem_start:	(__u32)&__vram_start, /* initially, we'll fill it later */
	smem_len:	LENGTH(X_RES, Y_RES, BPP), /* initially, we'll fill it later */
	type:		FB_TYPE_PACKED_PIXELS, /* this should always be true for dragonballs */
	visual:		FB_VISUAL_MONO10, /* this should depend on LCD used */
	line_length:	LINE_LENGTH(X_RES, BPP),
	accel:		FB_ACCEL_NONE, /* but still like a breeze ;-) */
};

static int currcon = 0;

/*int dragon_fb_init(void);
int dragon_fb_setup(char*);*/

/* ------------------- chipset specific functions -------------------------- */


static void set_color_bitfields(struct fb_var_screeninfo *var)
{
    switch (var->bits_per_pixel) {
	case 1:
	    var->red.offset = 0;
	    var->red.length = 1;
	    var->green.offset = 0;
	    var->green.length = 1;
	    var->blue.offset = 0;
	    var->blue.length = 1;
	    var->transp.offset = 0;
	    var->transp.length = 0;
	    break;
	case 8:
	    var->red.offset = 0;
	    var->red.length = 8;
	    var->green.offset = 0;
	    var->green.length = 8;
	    var->blue.offset = 0;
	    var->blue.length = 8;
	    var->transp.offset = 0;
	    var->transp.length = 0;
	    break;
	case 16:	/* RGB 565 */
	    var->red.offset = 0;
	    var->red.length = 5;
	    var->green.offset = 5;
	    var->green.length = 6;
	    var->blue.offset = 11;
	    var->blue.length = 5;
	    var->transp.offset = 0;
	    var->transp.length = 0;
	    break;
	case 24:	/* RGB 888 */
	    var->red.offset = 0;
	    var->red.length = 8;
	    var->green.offset = 8;
	    var->green.length = 8;
	    var->blue.offset = 16;
	    var->blue.length = 8;
	    var->transp.offset = 0;
	    var->transp.length = 0;
	    break;
	case 32:	/* RGBA 8888 */
	    var->red.offset = 0;
	    var->red.length = 8;
	    var->green.offset = 8;
	    var->green.length = 8;
	    var->blue.offset = 16;
	    var->blue.length = 8;
	    var->transp.offset = 24;
	    var->transp.length = 8;
	    break;
    }
    var->red.msb_right = 0;
    var->green.msb_right = 0;
    var->blue.msb_right = 0;
    var->transp.msb_right = 0;
}


static int dragon_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			 unsigned *blue, unsigned *transp,
			 struct fb_info *info)
{
#if defined(DEBUG)
	printk(KERN_DEBUG "fb%d: start: dragon_getcolreg\n",
		GET_FB_IDX(fb_info.node));
#endif

    /*
     *  Read a single color register and split it into colors/transparent.
     *  The return values must have a 16 bit magnitude.
     *  Return != 0 for invalid regno.
     */

    /* ... */
    return 0;
}

static int dragon_setcolreg(unsigned regno, unsigned red, unsigned green,
			 unsigned blue, unsigned transp,
			 struct fb_info *info)
{
   /*
     *  Set a single color register. The values supplied have a 16 bit
     *  magnitude.
     *  Return != 0 for invalid regno.
     */
#if defined(DEBUG)
	printk(KERN_DEBUG "fb%d: start: dragon_setcolreg\n",
		GET_FB_IDX(fb_info.node));
#endif

#if 0
    if (regno < 16) {
	/*
	 *  Make the first 16 colors of the palette available to fbcon
	 */
	if (is_cfb15)		/* RGB 555 */
	    ...fbcon_cmap.cfb16[regno] = ((red & 0xf800) >> 1) |
					 ((green & 0xf800) >> 6) |
					 ((blue & 0xf800) >> 11);
	if (is_cfb16)		/* RGB 565 */
	    ...fbcon_cmap.cfb16[regno] = (red & 0xf800) |
					 ((green & 0xfc00) >> 5) |
					 ((blue & 0xf800) >> 11);
	if (is_cfb24)		/* RGB 888 */
	    ...fbcon_cmap.cfb24[regno] = ((red & 0xff00) << 8) |
					 (green & 0xff00) |
					 ((blue & 0xff00) >> 8);
	if (is_cfb32)		/* RGBA 8888 */
	    ...fbcon_cmap.cfb32[regno] = ((red & 0xff00) << 16) |
					 ((green & 0xff00) << 8) |
					 (blue & 0xff00) |
					 ((transp & 0xff00) >> 8);
    }
#endif

    /* ... */
    return 0;
}

    /*
     *  Get the Colormap
     */

static int dragon_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
    if (con == currcon) /* current console? */
	return fb_get_cmap(cmap, kspc, dragon_getcolreg, info);
    else if (fb_display[con].cmap.len) /* non default colormap? */
	fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
    else
	fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
		     cmap, kspc ? 0 : 2);
    return 0;
}

    /*
     *  Set the Colormap
     */

static int dragon_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
    int err;

    if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
	if ((err = fb_alloc_cmap(&fb_display[con].cmap,
			      1<<fb_display[con].var.bits_per_pixel, 0)))
	    return err;
    }
    if (con == currcon)			/* current console? */
	return fb_set_cmap(cmap, kspc, dragon_setcolreg, info);
    else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
    return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
/*    if (con != currcon)
	return;
    if (fb_display[con].cmap.len)
	fb_set_cmap(&fb_display[con].cmap, 1, vfb_setcolreg, info);
    else
	fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), 1,
		    vfb_setcolreg, info);*/
}

/* ------------------- fbcon specific functions -------------------------- */

static int dragoncon_switch(int con, struct fb_info *info)
{
    /* Do we have to save the colormap? */
    /*if (fb_display[currcon].cmap.len)
	fb_get_cmap(&fb_display[currcon].cmap, 1, dragon_getcolreg, info);*/

    currcon = con;
    /* Install new colormap */
    do_install_cmap(con, info);
    return 0;
}


static int dragoncon_updatevar(int con, struct fb_info *info)
{
    /* Nothing */
    return 0;
}

static void dragoncon_blank(int blank_mode, struct fb_info *info)
{
#if defined(DEBUG)
	printk(KERN_DEBUG "fb%d: start: dragon_blank\n", 
		GET_FB_IDX(fb_info.node));
#endif

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

	/* The display must be driven all the time, otherwise a DC voltage builds up which destroy the crystal
           put we can disable the display (if it has that function) and switch off the backlight
         */
	if (blank_mode == 0)    // Add your code here, for normal operation
		{
#if defined(CONFIG_DRAGONIXVZ)
        PBDATA=(PBDATA&~0x80)|0x40; // Display on, backlight on#endif
#endif
                }
	else                    // add your code here to save power and save the forest
		{
#if defined(CONFIG_DRAGONIXVZ)
        PBDATA=(PBDATA&~0x40)|0x80; // Display on, backlight on#endif
#endif
                }
}

static void dragon_initialize_display(struct display *disp)
{
#if defined(DEBUG)
	printk(KERN_DEBUG "fb: start: dragon_initialize_display\n");
#endif

	disp->screen_base = fb_info.screen_base;


	switch (default_var.bits_per_pixel) { /* maybe I should pass a var and use it here */
#ifdef FBCON_HAS_MFB
	case 1:
                printk(KERN_INFO "Using fbcon_mfb\n");
		disp->dispsw = &fbcon_mfb;
		break;
#endif
#ifdef FBCON_HAS_CFB2
	case 2:
                printk(KERN_INFO "Using fbcon_CFB2\n");
		disp->dispsw = &fbcon_cfb2;
		break;
#endif
#ifdef FBCON_HAS_CFB4
	case 4:
		disp->dispsw = &fbcon_cfb4;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		disp->dispsw = &fbcon_cfb8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		disp->dispsw = &fbcon_cfb16;
		disp->dispsw_data = fbcon_cmap.cfb16;
		break;
#endif
#ifdef FBCON_HAS_CFB24
	case 24:
		disp->dispsw = &fbcon_cfb24;
		disp->dispsw_data = fbcon_cmap.cfb24;
		break;
#endif
#ifdef FBCON_HAS_CFB32
	case 32:
		disp->dispsw = &fbcon_cfb32;
		disp->dispsw_data = fbcon_cmap.cfb32;
		break;
#endif
	default:
		disp->dispsw = &fbcon_dummy;
		break;
	}

	//disp->dispsw = &fbcon_dummy;

	disp->visual = current_fix.visual;
	disp->type = current_fix.type;
   	disp->type_aux = current_fix.type_aux;
	disp->type_aux = current_fix.type_aux;
	disp->ypanstep = current_fix.ypanstep;
	disp->ywrapstep = current_fix.ywrapstep;
	disp->line_length = current_fix.line_length;
	disp->can_soft_blank = 1;
	disp->inverse = 0;

	memcpy(&disp->var, &default_var, sizeof(struct fb_var_screeninfo));
}


/* ------------ Hardware Independent Functions ------------ */





    /*
     *  Setup
     */

int __init dragon_fb_setup(char *options)
{
#if defined(DEBUG)
	printk(KERN_DEBUG "fb%d: start: dragon_fb_setup\n",
		GET_FB_IDX(fb_info.node));
#endif

    /* Parse user speficied options (`video=dragon_fb:') */

	/* no parsing yet */

	return 0;
}


/* ------------------------------------------------------------------------- */


    /*
     *  Frame buffer operations
     */

/* If all you need is that - just don't define ->fb_open */
/*static int dragon_fb_open(const struct fb_info *info, int user)
{
    return 0;
}*/

/* If all you need is that - just don't define ->fb_release */
/*static int dragon_fb_release(const struct fb_info *info, int user)
{
    return 0;
}*/


    /*
     *  Get the Fixed Part of the Display
     */

static int dragon_get_fix(struct fb_fix_screeninfo *fix, int con,
		       struct fb_info *info)
{
#if defined(DEBUG)
	printk(KERN_DEBUG "fb%d: start: dragon_get_fix\n",
		GET_FB_IDX(fb_info.node));
#endif

    /* we only have one mode, so our fix_screeninfo struct
       always has the same values, just throw it back to the user
       current_fix was initialized earlier in init */

#if defined(DEBUG)
	printk(KERN_DEBUG "fb: info: getting fix for console %d\n", con);
#endif

    memcpy(fix, &current_fix, sizeof(struct fb_fix_screeninfo));

    return 0;
}


    /*
     *  Get the User Defined Part of the Display
     */

static int dragon_get_var(struct fb_var_screeninfo *var, int con,
		       struct fb_info *info)
{
#if defined(DEBUG)
	printk(KERN_DEBUG "fb%d: start: dragon_get_var\n", 
		GET_FB_IDX(fb_info.node));

	printk(KERN_DEBUG "fb: info: getting var for console %d\n", con);
#endif

    if (con == -1)
	*var = default_var;
    else
	*var = fb_display[con].var;

    set_color_bitfields(var);

    return 0;
}


    /*
     *  Set the User Defined Part of the Display
     */

static int dragon_set_var(struct fb_var_screeninfo *var, int con,
		       struct fb_info *info)
{
    int activate = var->activate;

    struct display *display;

#if defined(DEBUG)
	printk(KERN_DEBUG "fb%d: start: dragon_set_var\n",
		GET_FB_IDX(fb_info.node));
#endif

    if (con >= 0)
    {
	display = &fb_display[con];

	/* this is the mechanism I use to initialize the display
		structures. You should have noticed that I assume
		that fbcon will call set_var at least once before
		using the display, I need to verify that */

        if (!display_initialized[con])
        {
            dragon_initialize_display(display);
            display_initialized[con] = 1;
        }
    }
    else
	display = &disp;	/* used during initialization */

    /*
     *  We are working with an LCD, which means that we
     *  can't allow for modifications of many of the screen's
     *  usually updateable parameters
     */

    /** I wonder if the first time a display is initialized, this method
        is called by fbcon to initialize the display struct, I should
        test this, VERY SERIOUS my temp solution has been to */

    if (var->xres != display->var.xres ||
        var->yres != display->var.yres ||
        var->xres_virtual != var->xres ||  /* virtual is not supported yet */
        var->yres_virtual != var->yres ||
        (var->xoffset != 0 && !(var->vmode & FB_VMODE_CONUPDATE)) ||
        (var->yoffset != 0 && !(var->vmode & FB_VMODE_CONUPDATE)) ||
        (var->bits_per_pixel != display->var.bits_per_pixel))
    {
	return -EINVAL;
    }

    set_color_bitfields(var); /* I don't really know what this does yet, I'll have to work on it */

     /* Since we don't allow anything to change anyways,
	there's no need to update the display var, makes 
	our lifes much easier :-)*/

    return 0;
}

    /*
     *  In most cases the `generic' routines (fbgen_*) should be satisfactory.
     *  However, you're free to fill in your own replacements.
     */

static struct fb_ops dragonfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	dragon_get_fix,
	fb_get_var:	dragon_get_var,
	fb_set_var:	dragon_set_var,
	fb_get_cmap:	dragon_get_cmap,
	fb_set_cmap:	dragon_set_cmap,
};

    /*
     *  Initialization
     */

void dragon_lcd_init()
{
	int i = 0;
	int translation; /* used to translate some settings to their register values */

#if defined(DEBUG)
	printk(KERN_DEBUG "fb: start: dragon_lcd_init\n");

	printk(KERN_DEBUG "fb: debug: dragon_lcd_init: Setting LSSA to %08x\n", (int) &__vram_start);
#endif

// The order the initialisation is performed, is very important. The board may crash!!!
#if defined(CONFIG_M68EZ328)
        // Set Controller io pins according to the interface used
        #if BUSWIDTH_LPICF == 0x00
                PCSEL&=~0xf1;   //
                PCPDEN&=~0xf1;  // We don't need pull-ups for those pins
        #elif BUSWIDTH_LPICF == 0x04
                PCSEL&=~0xf3;
                PCPDEN&=~0xf3;  // We don't need pull-ups for those pins
        #elif BUSWIDTH_LPICF == 0x08
                PCSEL&=~0xff;
                PCPDEN&=~0xff;  // We don't need pull-ups for those pins
        #endif

        LSSA = &__vram_start; /* the LCD Screen Starting Address Register should point at vram */
	LXMAX = X_RES;
	LYMAX = Y_RES-1;        // see 8.3.4 of the manual, size is +1

        LVPW = X_RES>>(4-GRAY_LPICF);
        LPICF = BUSWIDTH_LPICF|GRAY_LPICF;
        LPOLCF = POLARITY;
        LACDRC = ACRATE;
        LPXCD =  PIXRATE;
        LRRA = REFRATE;

	LCKCON = 0x80; // Fixme change this when you use a dedicated SRAM or ROM
        LPOSR = 0x00;
        LGPMR = GRAYSCALE;

#elif defined(CONFIG_M68VZ328)
        // Set Controller io pins according to the interface used
        #if BUSWIDTH_LPICF == 0x00
                PCSEL&=~0xf1;   //
                PCPDEN&=~0xf1;  // We don't need pull-ups for those pins
        #elif BUSWIDTH_LPICF == 0x04
                PCSEL&=~0xf3;
                PCPDEN&=~0xf3;  // We don't need pull-ups for those pins
        #elif BUSWIDTH_LPICF == 0x08
                PCSEL&=~0xff;
                PCPDEN&=~0xff;  // We don't need pull-ups for those pins
        #elif BUSWIDTH_LPICF == 0xc0
                PCSEL&=~0xff;
                PCPDEN&=~0xff;  // We don't need pull-ups for those pins
                PKSEL&=~0xf0;
                PKPUEN&=~0xf0;  // We don't need pull-ups for those pins
        #endif
        LSSA = &__vram_start; /* the LCD Screen Starting Address Register should point at vram */
	LXMAX = X_RES;
	LYMAX = Y_RES-1;        // see 8.3.4 of the manual, size is +1

        LVPW = X_RES>>(4-GRAY_LPICF);
        LPICF = BUSWIDTH_LPICF|GRAY_LPICF;
        LPOLCF = POLARITY;
        LACDRC = ACRATE;
        LPXCD =  PIXRATE;
        LRRA = REFRATE;

	LCKCON = 0x80;
        LPOSR = 0x00;
        LGPMR = GRAYSCALE;
        RMCR=0;
        DMACR=0x62;
#endif

#if defined(DEBUG)
	/* for testing purposes */
	for (i = 0; i < Y_RES; i+= 10)
	{
		memset(((char *)(&__vram_start)) + i       * X_RES * BPP / 8, 0xff, 2 * X_RES * BPP / 8);
		memset(((char *)(&__vram_start)) + (i + 2) * X_RES * BPP / 8, 0, 8 * X_RES * BPP / 8);
	}
#endif


        /* Add your custom display initialisation code here */
#if defined(CONFIG_DRAGONIXVZ)
        PBSEL|=0xc0;  // TIN/OUT is nDISPEN and PWMO1 is nBLEN
        PBPUEN|=0xc0; // Enable pullups
        PBDIR|=0xc0;  // Outputs
        PBDATA=(PBDATA&~0x80)|0x40; // Display on, backlight on
#endif
}

int __init dragon_fb_init(void)
{
#if defined(DEBUG)
	printk(KERN_DEBUG "fb: start: dragon_fb_init\n");
#endif

	/* then we initialize our lcd */
	dragon_lcd_init();

	/* then our structures */
	fb_info.changevar = NULL;
	fb_info.node = -1;
	fb_info.fbops = &dragonfb_ops;
	fb_info.disp = &disp;
	fb_info.switch_con = &dragoncon_switch;
	fb_info.updatevar = &dragoncon_updatevar;
	fb_info.blank = &dragoncon_blank;
	fb_info.flags = FBINFO_FLAG_DEFAULT;

	fb_info.screen_base = (char *)__vram_start;

	dragon_initialize_display(&disp);

	if (register_framebuffer(&fb_info) < 0) {
		return -EINVAL;
	}

	printk(KERN_INFO "fb%d: end: Virtual frame buffer device initialized\n",
		GET_FB_IDX(fb_info.node));
        printk(KERN_INFO "(C) 2001 by Khaled Hassounah <khassounah@mediumware.net>\n");

	/* uncomment this if your driver cannot be unloaded */
	/* MOD_INC_USE_COUNT; */
	return 0;
}


    /*
     *  Cleanup
     */

void dragon_fb_cleanup(struct fb_info *info)
{
#if defined(DEBUG)
	printk(KERN_DEBUG "fb%d: start: dragon_fb_cleanup\n", 
		GET_FB_IDX(fb_info.node));
#endif
    /*
     *  If your driver supports multiple boards, you should unregister and
     *  clean up all instances.
     */

    unregister_framebuffer(&fb_info);
}


/* ------------------------------------------------------------------------- */


    /*
     *  Modularization
     */

#ifdef MODULE
MODULE_LICENSE("GPL");
int init_module(void)
{
    return dragon_fb_init();
}

void cleanup_module(void)
{
    dragon_fb_cleanup(void);
}
#endif /* MODULE */

