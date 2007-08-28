/*
 * linux/drivers/video/mq200fb.c -- MQ-200 for a frame buffer device
 * based on linux/driver/video/pm2fb.c
 *
 * Copyright (C) 2000 Lineo, Japan
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/selection.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <asm/io.h>
#include "mq200fb.h"


#if 0
#define PRINTK(args...) printk(args)
#else
#define PRINTK(args...)
#endif

#define WITH_NONE 0
#define WITH_HSYNC (1 << 0)
#define WITH_VSYNC (1 << 1)


/****************************************************************
 * type definitions
 */

/****
 * PLL parameters
 */
struct pll_par {
    u16 m_par;                 /* M parameter */
    u16 n_par;                 /* N parameter */
    u16 p_par;                 /* P parameter */
};


/****
 * display control values
 */
struct disp_par {
    u16 hdt;                   /* horizontal display total;
                                * hdt >= hse + 4; val = hdt - 2 */
    u16 hde;                   /* horizontal display end */
    u16 vdt;                   /* vertical display total;
                                * vdt >= vse + 2; val = vdt - 1 */
    u16 vde;                   /* vertical display end; val = vde - 1 */

    u16 hss;                   /* horizontal sync start; hss >= hde + 4 */
    u16 hse;                   /* horizontal sync end; hse >= hss + 4 */
    u16 vss;                   /* vertical sync start; vss >= vde + 1 */
    u16 vse;                   /* vertical sync end; vse >= vss + 1 */
};


/****
 * window control values
 */
struct win_par {
    u16 hws;                   /* horizontal window start */
    u16 hww;                   /* horizontal window width;
                                * hww >= 8; val = hww - 1 */
    u16 wald;                  /* window additional line data */

    u16 vws;                   /* vertical window start */
    u16 vwh;                   /* vertical window height;
                                * vwh >= 1; val = vwh - 1 */
    s16 wst;                   /* window stride; do NOT use a negative
                                  number */
    unsigned long wsa;         /* window start address */
};

/****
 *  The hardware specific data in this structure uniquely defines a video
 *  mode.
 *
 *  If your hardware supports only one video mode, you can leave it empty.
 */
struct mq200fb_par {
    struct pll_par pll[3];     /* PLL 1, 2 and 3 */
    struct {
       struct disp_par disp;   /* display control */
       struct win_par win;     /* window control */
       struct win_par awin;    /* alternate window control */
       u16 gcd;                /* graphics color depth */
       u16 agcd;               /* alternate graphics color depth */
       u16 v_height;           /* virtual height for main window */
       u16 v_aheight;          /* virtual height for alternate window */
       u16 fd;                 /* pixclock = source_clock / fd / sd */
       u16 sd;
    } gc[2];                   /* graphics controller 1 and 2 */
};


struct mq200fb_cursor_info {
    struct {
       u16 x, y;
    } size;
    int enable;
    struct timer_list timer;
};


struct mq200fb_info {
    struct fb_info_gen gen;
    struct mq200fb_par current_par;
    int current_par_valid;
    u16 vendor_id;             /* MQ-200 vendor ID */
    u16 device_id;             /* MQ-200 device ID */
    struct {
       u32 fb_size;            /* framebuffer size */
       unsigned long rg_base;  /* physical register memory base */
       unsigned long p_fb;     /* physical address of frame buffer */
       unsigned long v_fb;     /* virtual address of frame buffer */
       unsigned long p_regs;   /* physical address of rg_base */
       unsigned long v_regs;   /* virtual address of rg_base */
    } regions;
    unsigned long pmu_base;    /* power management unit */
    unsigned long cpu_base;    /* CPU interface */
    unsigned long miu_base;    /* memory interface unit */
    unsigned long in_base;     /* interrupt controller */
    unsigned long gc_base;     /* graphics controller 1&2 */
    unsigned long ge_base;     /* graphics engine */
    unsigned long fpi_base;    /* flat panel interface */
    unsigned long cp1_base;    /* color palette 1 */
    unsigned long dc_base;     /* device configuration */
    unsigned long pci_base;    /* PCI configuration */
    unsigned long psf_base;    /* ??? */

    struct display disp;
    struct mq200fb_cursor_info cursor_info;
    spinlock_t lock;
};


/****************************************************************
 * function prototype declarations
 */

static int mq200fb_open(struct fb_info*, int);
static int mq200fb_release(struct fb_info*, int);
static void mq200fb_detect(void);
static int mq200fb_encode_fix(struct fb_fix_screeninfo*, const void*,
                             struct fb_info_gen*);
static int mq200fb_decode_var(const struct fb_var_screeninfo*, void*,
                             struct fb_info_gen*);
static int mq200fb_encode_var(struct fb_var_screeninfo*, const void*,
                             struct fb_info_gen*);
static void mq200fb_get_par(void*, struct fb_info_gen*);
static void mq200fb_set_par(const void*, struct fb_info_gen*);
static int mq200fb_getcolreg(unsigned, unsigned*, unsigned*, unsigned*,
                            unsigned*, struct fb_info*);
static int mq200fb_setcolreg(unsigned, unsigned, unsigned, unsigned,
                            unsigned, struct fb_info*);
static int mq200fb_pan_display(const struct fb_var_screeninfo*,
                              struct fb_info_gen*);
static int mq200fb_blank(int, struct fb_info_gen*);
static void mq200fb_set_disp(const void*, struct display*,
                            struct fb_info_gen*);
#ifdef FBCON_HAS_CFB8
static void mq200fb_cursor(struct display*, int, int, int);
static int mq200fb_set_font(struct display*, int, int);
static void mq200fb_clear(struct vc_data*, struct display*, int, int, int,
                         int);
static void mq200fb_clear_margins(struct vc_data*, struct display*, int);
static void mq200fb_putc(struct vc_data*, struct display*, int, int, int);
static void mq200fb_putcs(struct vc_data*, struct display*,
                         const unsigned short*, int, int, int);
#endif

static int is_color_palette_enabled(const struct mq200fb_par*);
static u32 get_line_length(const struct mq200fb_par*);
static u32 get_width(const struct mq200fb_par*);
#if 0
static void set_v_width(struct mq200fb_par*, int);
#endif
static u32 get_v_height(const struct mq200fb_par*);
static u32 get_height(const struct mq200fb_par*);
#if 0
static void set_v_height(struct mq200fb_par*, int);
#endif
static int get_bits_per_pixel(const struct mq200fb_par*);
#if 0
static void set_bits_per_pixel(struct mq200fb_par*, int);
#endif
static u32 get_right_margin(const struct mq200fb_par*);
static u32 get_left_margin(const struct mq200fb_par*);
static u32 get_lower_margin(const struct mq200fb_par*);
static u32 get_upper_margin(const struct mq200fb_par*);
static u32 get_hsync_len(const struct mq200fb_par*);
static u32 get_vsync_len(const struct mq200fb_par*);
static u32 get_xoffset(const struct mq200fb_par*);
static u32 get_yoffset(const struct mq200fb_par*);
static u32 get_pixclock(const struct mq200fb_par*);
static double get_pll_freq(const struct pll_par*);
static double get_fd_val(const struct mq200fb_par*);
static double get_sd_val(const struct mq200fb_par*);

static void power_state_transition(const struct mq200fb_info*, int);
#if 0
static void vsync(void);
static void mq200_interrupt(int, void*, struct pt_regs*);
static void mq200_enable_irq(const struct mq200fb_info*);
#endif
static int mq200fb_conf(struct mq200fb_info*);
static int mq200_probe(struct mq200fb_info*);
static void mq200_reset(const struct mq200fb_info*);
static void dc_reset(const struct mq200fb_info*);
static void miu_reset(const struct mq200fb_info*);
static void pmu_reset(const struct mq200fb_info*);
static void gc1_reset(const struct mq200fb_info*);
static void ge_reset(const struct mq200fb_info*);
static void cp1_reset(const struct mq200fb_info*);
static void set_screen(const struct mq200fb_info*,
                      const struct mq200fb_par*);
static void set_cursor_shape(const struct mq200fb_info*);
static void enable_cursor(struct mq200fb_info*);
static void disable_cursor(struct mq200fb_info*);
static int set_cursor_pos(const struct mq200fb_info*, unsigned, unsigned);
static void cursor_timer_handler(unsigned long);
static void get_color_palette(const struct mq200fb_info*, unsigned,
                             unsigned*, unsigned*, unsigned*);
static void set_color_palette(const struct mq200fb_info*, unsigned,
                             unsigned, unsigned, unsigned);
static void enable_dac(const struct mq200fb_info*);
static void disable_dac(const struct mq200fb_info*, unsigned);
static int __inline__ is_enough_cmd_fifo(const struct mq200fb_info*, int);
static int __inline__ is_enough_src_fifo(const struct mq200fb_info*, int);
static void wait_cmd_fifo(const struct mq200fb_info*, int);
static void wait_src_fifo(const struct mq200fb_info*, int);

static void ge_rect_fill(const struct mq200fb_info*, int, int, int, int,
                        u32);
static void ge_bitblt_char(const struct mq200fb_info*, int, int, u8*, int,
                          int, u32, u32);

static void add_cursor_timer(struct mq200fb_cursor_info*);


/****************************************************************
 * static variables definitions
 */

/****
 *  In most cases the `generic' routines (fbgen_*) should be satisfactory.
 *  However, you're free to fill in your own replacements.
 */
static struct fb_ops mq200fb_ops = {
#if LINUX_VERSION_CODE >= 0x020400
    owner:             THIS_MODULE,
#else
    fb_open:           mq200fb_open,
    fb_release:                mq200fb_release,
#endif
    fb_get_fix:                fbgen_get_fix,
    fb_get_var:                fbgen_get_var,
    fb_set_var:                fbgen_set_var,
    fb_get_cmap:       fbgen_get_cmap,
    fb_set_cmap:       fbgen_set_cmap,
    fb_pan_display:    fbgen_pan_display,
};


static struct mq200fb_info mq200fb_info;
static struct {
    char font[40];
    struct mq200fb_par par;
} mq200fb_options = {
    font: "",
    {
       {
           { 219, 16, 2 },     /* PLL1 = 12.288MHz * 220 / 17 / 4 = 39.76 */
           { 242, 16, 2 },     /* PLL2 = 12.288MHz * 243 / 17 / 4 = 43.91 */
           { 242, 22, 2 },     /* PLL3 = 12.288MHz * 243 / 23 / 4 = 32.46 */
       },
       {
           {                   /* graphics controller 1 */
               { 1047, 800, 624, 599, 832, 976, 601, 604 }, /* display */
               { 0, 800, 0, 0, 600, 800, 0 }, /* window */
               { },            /* alternate window */
               3,              /* gcd = 8-bpp */
               3,              /* agcd = 8-bpp */
               600,            /* virtual height for main window */
               0,              /* virtual height for alternate window */
               0,              /* fd = 1 */
               1               /* sd = 1 */
           },
           {                   /* graphics controller 2 */
           }
       },
    }
};


/****
 * Interfaces to hardware functions
 */
static struct fbgen_hwswitch mq200_switch = {
    detect: mq200fb_detect,
    encode_fix: mq200fb_encode_fix,
    decode_var: mq200fb_decode_var,
    encode_var: mq200fb_encode_var,
    get_par: mq200fb_get_par,
    set_par: mq200fb_set_par,
    getcolreg: mq200fb_getcolreg,
    setcolreg: mq200fb_setcolreg,
    pan_display: mq200fb_pan_display,
    blank: mq200fb_blank,
    set_disp: mq200fb_set_disp
};


#ifdef FBCON_HAS_CFB8
static void mq200fb_ds_setup(struct display* disp)
{
    fbcon_cfb8_setup(disp);
}

/****
 * fbcon switch for the low level operations.
 */
static struct display_switch mq200_cfb8 = {
    setup: mq200fb_ds_setup,
    bmove: fbcon_cfb8_bmove,
    clear: mq200fb_clear,
    putc: mq200fb_putc,
    putcs: mq200fb_putcs,
    cursor: mq200fb_cursor,
    set_font: mq200fb_set_font,
    clear_margins: mq200fb_clear_margins,
    fontwidthmask: FONTWIDTH(8) | FONTWIDTH(16)
};
#endif


/****************************************************************
 * functions for struct fb_ops member
 */

#if LINUX_VERSION_CODE < 0x020400
static int
mq200fb_open(struct fb_info* info,
            int user)
{
    MOD_INC_USE_COUNT;
    return 0;
}
#endif


#if LINUX_VERSION_CODE < 0x020400
static int
mq200fb_release(struct fb_info* info,
               int user)
{
    MOD_DEC_USE_COUNT;
    return 0;
}
#endif


/****************************************************************
 * functions for struct fbgen_hwswitch member
 */

/****
 *  This function should detect the current video mode settings and store
 *  it as the default video mode
 */
static void
mq200fb_detect(void)
{
}


/****
 *  This function should fill in the 'fix' structure based on the values
 *  in the `par' structure.
 */
static int
mq200fb_encode_fix(struct fb_fix_screeninfo* fix,
                  const void* par,
                  struct fb_info_gen* info)
{
    const struct mq200fb_par* mq200fb_par = par;
    struct mq200fb_info* fb_info = (struct mq200fb_info*)info;

    strcpy(fix->id, CHIPNAME);
#if LINUX_VERSION_CODE >= 0x020400
    fix->smem_start = fb_info->regions.p_fb;
    fix->mmio_start = fb_info->regions.p_regs;
#else
    fix->smem_start = (char*)fb_info->regions.p_fb;
    fix->mmio_start = (char*)fb_info->regions.p_regs;
#endif
    fix->smem_len = fb_info->regions.fb_size;
    fix->mmio_len = MQ200_REGS_SIZE;
    fix->accel = FB_ACCEL_MEDIAQ_MQ200;
    fix->type = FB_TYPE_PACKED_PIXELS;
    fix->visual = is_color_palette_enabled(mq200fb_par)
       ?  FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
    if (fb_info->current_par_valid)
       fix->line_length = get_line_length(mq200fb_par);
    else
       fix->line_length = 0;
    fix->xpanstep = 0;         /* $H$j$"$($: */
    fix->ypanstep = 1;         /* $H$j$"$($: */
    fix->ywrapstep = 0;

    return 0;
}


/****
 *  Get the video params out of 'var'. If a value doesn't fit, round it up,
 *  if it's too big, return -EINVAL.
 *
 *  Suggestion: Round up in the following order: bits_per_pixel, xres,
 *  yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *  bitfields, horizontal timing, vertical timing.
 */
static int
mq200fb_decode_var(const struct fb_var_screeninfo *var,
                  void* par,
                  struct fb_info_gen *info)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)info;
    struct mq200fb_par* fb_par = (struct mq200fb_par*)par;

    *fb_par = fb_info->current_par;
    return 0;
}


/****
 *  Fill the 'var' structure based on the values in 'par' and maybe other
 *  values read out of the hardware.
 */
static int
mq200fb_encode_var(struct fb_var_screeninfo* var,
                  const void* par,
                  struct fb_info_gen* info)
{
    const struct mq200fb_par* mq200fb_par = par;
    struct fb_var_screeninfo new_var;

    memset(&new_var, 0, sizeof new_var);

    new_var.xres_virtual = get_line_length(mq200fb_par) * 8;
    new_var.yres_virtual = get_v_height(mq200fb_par);
    new_var.xres = get_width(mq200fb_par);
    new_var.yres = get_height(mq200fb_par);
    new_var.right_margin = get_right_margin(mq200fb_par);
    new_var.hsync_len = get_hsync_len(mq200fb_par);
    new_var.left_margin = get_left_margin(mq200fb_par);
    new_var.lower_margin = get_lower_margin(mq200fb_par);
    new_var.vsync_len = get_vsync_len(mq200fb_par);
    new_var.upper_margin = get_upper_margin(mq200fb_par);
    new_var.bits_per_pixel = get_bits_per_pixel(mq200fb_par);
    switch (get_bits_per_pixel(mq200fb_par)) {
    case 1:
    case 2:
    case 4:
    case 16:
    case 24:
    case 32:
       PRINTK(CHIPNAME ": %d bpp is not supported depth.\n",
              get_bits_per_pixel(mq200fb_par));
       break;

    case 8:
       new_var.red.length = new_var.green.length = new_var.blue.length = 8;
       break;
    }
    new_var.xoffset = get_xoffset(mq200fb_par);
    new_var.yoffset = get_yoffset(mq200fb_par);
    new_var.height = new_var.width = -1;
    new_var.pixclock = get_pixclock(mq200fb_par);
    new_var.sync = 0;
    new_var.vmode = FB_VMODE_NONINTERLACED;
    new_var.activate = FB_ACTIVATE_NOW;

    *var = new_var;

    return 0;
}


/****
 *  Fill the hardware's 'par' structure.
 */
static void mq200fb_get_par(void* par,
                           struct fb_info_gen *info)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)info;

    if (! fb_info->current_par_valid) {
//     mq200_reset(fb_info);
       set_screen(fb_info, &fb_info->current_par);
       fb_info->current_par_valid = 1;
    }
    *((struct mq200fb_par*)par) = fb_info->current_par;
}


/****
 *  Set the hardware according to 'par'.
 */
static void mq200fb_set_par(const void* par,
                           struct fb_info_gen *info)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)info;
    struct mq200fb_par* fb_par = (struct mq200fb_par*)par;

    set_screen(fb_info, fb_par);
    fb_info->current_par = *fb_par;
    fb_info->current_par_valid = 1;
}


/****
 *  Read a single color register and split it into colors/transparent.
 *  The return values must have a 16 bit magnitude.
 *  Return != 0 for invalid regno.
 */
static int mq200fb_getcolreg(unsigned regno,
                            unsigned* red,
                            unsigned* green,
                            unsigned* blue,
                            unsigned* transp,
                            struct fb_info* info)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)info;

    if (regno >= 256)
       return 1;

    get_color_palette(fb_info, regno, red, green, blue);
    *red <<= 8;
    *green <<= 8;
    *blue <<= 8;
    *transp = 0;               /* right now, transparency is not
                                  implemented */

    return 0;
}


/****
 *  Set a single color register. The values supplied have a 16 bit
 *  magnitude.
 *  Return != 0 for invalid regno.
 */
static int mq200fb_setcolreg(unsigned regno,
                            unsigned red,
                            unsigned green,
                            unsigned blue,
                            unsigned transp,
                            struct fb_info *info)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)info;

    if (regno >= 256)
       return 1;

    set_color_palette(fb_info, regno, red >> 8, green >> 8, blue >> 8);

    return 0;
}


/****
 *  Pan (or wrap, depending on the `vmode' field) the display using the
 *  `xoffset' and `yoffset' fields of the `var' structure.
 *  If the values don't fit, return -EINVAL.
 */
static int mq200fb_pan_display(const struct fb_var_screeninfo* var,
                              struct fb_info_gen* info)
{
    if (var->xoffset || var->yoffset)
       return -EINVAL;
    else
       return 0;

#if 0
    if (var->xoffset <= 0 && var->yoffset <= 0) {
    }
    else if (var->xoffset > 0 && var->yoffset > 0) {
    }
    else if (var->xoffset <= 0 && var->yoffset > 0) {
    }
    else {
    }

    return 0;
#endif
}


/****
 *  Blank the screen if blank_mode != 0, else unblank. If blank == NULL
 *  then the caller blanks by setting the CLUT (Color Look Up Table) to all
 *  black. Return 0 if blanking succeeded, != 0 if un-/blanking failed due
 *  to e.g. a video mode which doesn't support it. Implements VESA suspend
 *  and powerdown modes on hardware that supports disabling hsync/vsync:
 *    blank_mode == 2: suspend vsync
 *    blank_mode == 3: suspend hsync
 *    blank_mode == 4: powerdown
 */
static int mq200fb_blank(int blank_mode,
                        struct fb_info_gen* info)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)info;

    if (! fb_info->current_par_valid)
       return 1;

    switch (blank_mode) {
    case 0:
       enable_dac(fb_info);
       enable_cursor(fb_info);
       return 0;

    case 1:
       disable_cursor(fb_info);
       disable_dac(fb_info, WITH_VSYNC | WITH_HSYNC);
       return 0;

    case 2:
       disable_cursor(fb_info);
       disable_dac(fb_info, WITH_HSYNC);
       return 0;

    case 3:
       disable_cursor(fb_info);
       disable_dac(fb_info, WITH_VSYNC);
       return 0;

    case 4:
       disable_cursor(fb_info);
       disable_dac(fb_info, WITH_NONE);
       return 0;

    default:
       printk(CHIPNAME ": unknown blank mode (%d).\n", blank_mode);
       return 1;
    }
}


/****
 *  Fill in a pointer with the virtual address of the mapped frame buffer.
 *  Fill in a pointer to appropriate low level text console operations (and
 *  optionally a pointer to help data) for the video mode `par' of your
 *  video hardware. These can be generic software routines, or hardware
 *  accelerated routines specifically tailored for your hardware.
 *  If you don't have any appropriate operations, you must fill in a
 *  pointer to dummy operations, and there will be no text output.
 */
static void mq200fb_set_disp(const void *par,
                            struct display *disp,
                            struct fb_info_gen *info)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)info;

    disp->screen_base = (char*)fb_info->regions.v_fb;
    switch (get_bits_per_pixel((struct mq200fb_par*)par)) {
#ifdef FBCON_HAS_CFB8
    case 8:
       disp->dispsw = &mq200_cfb8;
       break;
#endif
    default:
       disp->dispsw = &fbcon_dummy;
    }
}


/****************************************************************
 * functions for struct display_switch member
 */

#ifdef FBCON_HAS_CFB8
static void
mq200fb_cursor(struct display* disp,
              int mode,
              int x,
              int y)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)disp->fb_info;

    switch (mode) {
    case CM_ERASE:
       disable_cursor(fb_info);
       break;

    case CM_DRAW:
    case CM_MOVE:
       enable_cursor(fb_info);
       set_cursor_pos(fb_info, x * fontwidth(disp), y * fontheight(disp));
       break;
    }
}
#endif


#ifdef FBCON_HAS_CFB8
static int
mq200fb_set_font(struct display* disp,
                int width,
                int height)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)disp->fb_info;

    fb_info->cursor_info.size.x = width;
    fb_info->cursor_info.size.y = height;
    set_cursor_shape(fb_info);

    return 1;
}
#endif


#ifdef FBCON_HAS_CFB8
static void mq200fb_clear(struct vc_data* conp, struct display* disp,
                         int sy, int sx, int height, int width)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)disp->fb_info;
    u32 bgx;

    bgx = attr_bgcol_ec(disp, conp);
    ge_rect_fill(fb_info, sx * fontwidth(disp), sy * fontheight(disp),
                width * fontwidth(disp), height * fontheight(disp), bgx);
}
#endif


#ifdef FBCON_HAS_CFB8
static void
mq200fb_clear_margins(struct vc_data* conp,
                     struct display* disp,
                     int bottom_only)
{
    const struct mq200fb_info* fb_info = (struct mq200fb_info*)disp->fb_info;
    unsigned int right_start = conp->vc_cols * fontwidth(disp);
    unsigned int right_width = disp->var.xres - right_start;
    unsigned int bottom_start = conp->vc_rows * fontheight(disp);
    unsigned int bottom_width = disp->var.yres - bottom_start;
    u32 bgx = attr_bgcol_ec(disp, conp);

    if (! bottom_only && right_width != 0)
       ge_rect_fill(fb_info, right_start, 0, right_width,
                    disp->var.yres_virtual, bgx);
    if (bottom_width != 0)
       ge_rect_fill(fb_info, 0, disp->var.yoffset + bottom_start,
                    right_start, bottom_width, bgx);
}
#endif


#ifdef FBCON_HAS_CFB8
static void
mq200fb_putc(struct vc_data* conp,
            struct display* disp,
            int ch,
            int yy,
            int xx)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)disp->fb_info;
    u32 fgx, bgx;
    u8* chdat;

    fgx = attr_fgcol(disp, ch);
    bgx = attr_bgcol(disp, ch);
    xx *= fontwidth(disp);
    yy *= fontheight(disp);
    chdat = disp->fontdata + (ch & disp->charmask) * fontheight(disp);
    ge_bitblt_char(fb_info, xx, yy, chdat, fontwidth(disp),
                  fontheight(disp), fgx, bgx);
}
#endif


#ifdef FBCON_HAS_CFB8
static void
mq200fb_putcs(struct vc_data* conp,
             struct display* disp,
             const unsigned short* str,
             int count,
             int yy,
             int xx)
{
    struct mq200fb_info* fb_info = (struct mq200fb_info*)disp->fb_info;
    u32 fgx, bgx;

    fgx = attr_fgcol(disp, scr_readw(str));
    bgx = attr_bgcol(disp, scr_readw(str));
    xx *= fontwidth(disp);
    yy *= fontheight(disp);
    while (count-- > 0) {
       u16 ch = scr_readw(str++) & disp->charmask;
       u8* chdat = disp->fontdata + ch * fontheight(disp);

       ge_bitblt_char(fb_info, xx, yy, chdat, fontwidth(disp),
                      fontheight(disp), fgx, bgx);
       xx += fontwidth(disp);
    }
}
#endif


/****************************************************************
 * functions related to "struct mq200fb_par".
 */

/****
 * Is current window using color palette?
 */
static int
is_color_palette_enabled(const struct mq200fb_par* par)
{
    switch (par->gc[0].gcd) {
    case GCD_1_CP: case GCD_2_CP: case GCD_4_CP: case GCD_8_CP:
    case GCD_16_CP: case GCD_24_CP: case GCD_32_RGB_CP: case GCD_32_BGR_CP:
       return 1;
    default:
       return 0;
    }
}


/****
 * Return current window's virtual width in bytes.
 *
 * [NOTE]
 *     This function assumes that struct win_par.wst value is not a
 *     negative number.
 */
static u32
get_line_length(const struct mq200fb_par* par)
{
    return par->gc[0].win.wst;
}


/****
 * Return current window's width in pixels.
 */
static u32
get_width(const struct mq200fb_par* par)
{
    return par->gc[0].win.hww /* + 1 */;
}


#if 0
/****
 * set current window's virtual width.
 *
 * [NOTE]
 *     This function assumes that already depth was set.
 */
static void
set_v_width(struct mq200fb_par* par,
           int width)          /* in pixels */
{
    int depth = get_bits_per_pixel(par);
    par->gc[0].win.wst = (width * depth + 127) / 128 * 16;
}
#endif


/****
 * Return current window's virtual height.
 */
static u32
get_v_height(const struct mq200fb_par* par)
{
    return par->gc[0].v_height;
}


static u32
get_height(const struct mq200fb_par* par)
{
    return par->gc[0].win.vwh;
}


#if 0
/****
 * set current window's virtual height.
 */
static void
set_v_height(struct mq200fb_par* par,
            int height)
{
    par->gc[0].v_height = height;
}
#endif


/****
 * get current window's depth (bits / pixcel).
 */
static int
get_bits_per_pixel(const struct mq200fb_par* par)
{
    switch (par->gc[0].gcd) {
    case GCD_1_CP:
       return 1;
    case GCD_2_CP:
       return 2;
    case GCD_4_CP:
       return 4;
    case GCD_8_CP:
       return 8;
    case GCD_16_CP: case GCD_16:
       return 16;
    case GCD_24_CP: case GCD_24:
       return 24;
    default:
       return 32;
    }
}


#if 0
/****
 * set current window's depth.
 *
 * [NOTE]
 *     we do not use the following:
 *             GCD_16_CP, GCD_24_CP, GCD_32_RGB_CP, GCD_32_BGR_CP,
 *             GCD_32_BGR.
 */
static void
set_bits_per_pixel(struct mq200fb_par* par,
                  int bpp)
{
    if (bpp <= 1)
       par->gc[0].gcd = GCD_1_CP;
    else if (bpp <= 2)
       par->gc[0].gcd = GCD_2_CP;
    else if (bpp <= 4)
       par->gc[0].gcd = GCD_4_CP;
    else if (bpp <= 8)
       par->gc[0].gcd = GCD_8_CP;
    else if (bpp <= 16)
       par->gc[0].gcd = GCD_16;
    else if (bpp <= 24)
       par->gc[0].gcd = GCD_24;
    else
       par->gc[0].gcd = GCD_32_RGB;
}
#endif


static u32
get_right_margin(const struct mq200fb_par* par)
{
    return par->gc[0].disp.hss;
}


static u32
get_left_margin(const struct mq200fb_par* par)
{
    return par->gc[0].disp.hdt + 2 - par->gc[0].disp.hse;
}


static u32
get_lower_margin(const struct mq200fb_par* par)
{
    return par->gc[0].disp.vss;
}


static u32
get_upper_margin(const struct mq200fb_par* par)
{
    return par->gc[0].disp.vdt + 1 - par->gc[0].disp.vse;
}


static u32
get_hsync_len(const struct mq200fb_par* par)
{
    return par->gc[0].disp.hse - par->gc[0].disp.hss;
}


static u32
get_vsync_len(const struct mq200fb_par* par)
{
    return par->gc[0].disp.vse - par->gc[0].disp.vss;
}


static u32
get_xoffset(const struct mq200fb_par* par)
{
    return (par->gc[0].win.wsa % par->gc[0].win.wst) /
       ((get_bits_per_pixel(par) + 7) / 8);
}


static u32
get_yoffset(const struct mq200fb_par* par)
{
    return par->gc[0].win.wsa / par->gc[0].win.wst;
}


/****
 * Return pixel clock in pico seconds.
 */
static u32
get_pixclock(const struct mq200fb_par* par)
{
    double freq = get_pll_freq(&par->pll[1]) / get_fd_val(par) / get_sd_val(par);
    return (u32)(1.0e12 / freq + 0.5);
}


/****
 * Return specified PLL frequencey in MHz.
 */
static double
get_pll_freq(const struct pll_par* pll)
{
    return 12.288e6 * (pll->m_par + 1) / (pll->n_par + 1) / (1 << pll->p_par);
}


/****
 * Return FD value. FD is a value that is used to generate G1MCLK.
 */
static double
get_fd_val(const struct mq200fb_par* par)
{
    switch (par->gc[0].fd) {
    case 0:
       return 1.0;
    default:
       return par->gc[0].fd + 0.5;
    }
}


/****
 * Return SD value. SD is a value that is used to generate G1MCLK.
 */
static double
get_sd_val(const struct mq200fb_par* par)
{
    return par->gc[0].sd;
}


/****************************************************************
 * hardware specific functions.
 */

/****
 * power state transition to "state".
 */
static void
power_state_transition(const struct mq200fb_info* fb_info,
                      int state)
{
    int i;
    writel(state, PMCSR(fb_info));
    for (i = 1; ; i++) {
       udelay(100);
       if ((readl(PMCSR(fb_info)) & 0x3) == state) {
           PRINTK(CHIPNAME ": transition to D%d state (%d)\n\r", state, i);
           break;
       }
    }
}


#if 0
/****
 * vertical sync enable -- falling edge.
 */
static void
vsync()
{
}


/****
 * interrupt handler.
 */
static void
mq200_interrupt(int irq,
               void* dev_id,
               struct pt_regs* regs)
{
    u32 status;

    status = readl(IN02R(&mq200fb_info));
    writel(status, IN02R(&mq200fb_info));
    if (status & 0x02)
       vsync();
}


/****
 * enable interrupt controller.
 */
static void
mq200_enable_irq(const struct mq200fb_info* fb_info)
{
    writel(0x1F02, IN01R(fb_info)); /* unmask */
    writel(1, IN00R(fb_info)); /* interrupt enable */
}
#endif


/****
 * Configure register and framebuffer addresses.
 * If you need to get virtual addresses using ioremap(), rewrite this
 * function.
 *
 * [RETURN]
 *     1: success
 *     0: fail
 */
static int __init
mq200fb_conf(struct mq200fb_info* fb_info)
{
    /* set framebuffer related values */
    fb_info->regions.p_fb = virt_to_phys((void*)MQ200_FB_BASE);
    fb_info->regions.v_fb = MQ200_FB_BASE;
    fb_info->regions.p_regs = virt_to_phys((void*)MQ200_REGS_BASE);
    fb_info->regions.v_regs = MQ200_REGS_BASE;
    fb_info->regions.fb_size = MQ200_FB_SIZE - CURSOR_IMAGE_SIZE;

    /* set addresses of module register */
    fb_info->pmu_base = fb_info->regions.v_regs + PMU_OFFSET;
    fb_info->cpu_base = fb_info->regions.v_regs + CPU_OFFSET;
    fb_info->miu_base = fb_info->regions.v_regs + MIU_OFFSET;
    fb_info->in_base = fb_info->regions.v_regs + IN_OFFSET;
    fb_info->gc_base = fb_info->regions.v_regs + GC_OFFSET;
    fb_info->ge_base = fb_info->regions.v_regs + GE_OFFSET;
    fb_info->fpi_base = fb_info->regions.v_regs + FPI_OFFSET;
    fb_info->pci_base = fb_info->regions.v_regs + PCI_OFFSET;
    fb_info->dc_base = fb_info->regions.v_regs + DC_OFFSET;
    fb_info->cp1_base = fb_info->regions.v_regs + CP1_OFFSET;
    fb_info->psf_base = fb_info->regions.v_regs + PSF_OFFSET;

    if (! mq200_probe(fb_info)) {
       printk(CHIPNAME ": MQ-200 not found.\n");
       return 0;
    }
    PRINTK(CHIPNAME ": MQ-200 found.\n");

    /* set cursor info */
    init_timer(&fb_info->cursor_info.timer);
    fb_info->cursor_info.timer.data = (unsigned long)fb_info;
    fb_info->cursor_info.timer.function = cursor_timer_handler;
    add_cursor_timer(&fb_info->cursor_info);

    return 1;
}


/****
 * [RETURN]
 *     1: detect
 *     0: not found
 */
static int __init
mq200_probe(struct mq200fb_info* fb_info)
{
    union pc00r pc00r;

    if (readl(PMR(fb_info)) != PMR_VALUE)
       return 0;

    pc00r.whole = readl(PC00R(fb_info));
    fb_info->vendor_id = pc00r.part.vendor;
    fb_info->device_id = pc00r.part.device;

    return 1;
}


/****
 * mq-200 hardware reset.
 */
static void
mq200_reset(const struct mq200fb_info* fb_info)
{
    power_state_transition(fb_info, 0);        /* transition to D0 state */

    dc_reset(fb_info);         /* device configuration */
    miu_reset(fb_info);                /* memory interface unit */
    pmu_reset(fb_info);                /* power management unit */
    gc1_reset(fb_info);                /* graphics controller 1 */
    ge_reset(fb_info);         /* graphics engine */
    cp1_reset(fb_info);                /* color palette 1 */
}


/****
 * device configuration initialization.
 */
static void
dc_reset(const struct mq200fb_info* fb_info)
{
    union dc00r dc00r;

    dc00r.whole = 0;
    dc00r.part.osc_enbl = 1;
    dc00r.part.pll1_enbl = 1;
    dc00r.part.pll1_p_par = fb_info->current_par.pll[0].p_par;
    dc00r.part.pll1_n_par = fb_info->current_par.pll[0].n_par;
    dc00r.part.pll1_m_par = fb_info->current_par.pll[0].m_par;
    dc00r.part.fast_pwr = 1;
    dc00r.part.osc_frq = 3;    /* oscillator frequency is in the range of
                                  12 MHz to 25 MHz */
    writel(dc00r.whole, DC00R(fb_info));
    PRINTK(CHIPNAME ": DC00R = %lx\n", readl(DC00R(fb_info)));
}


/****
 * initialize memory interface unit.
 */
static void
miu_reset(const struct mq200fb_info* fb_info)
{
    union mm00r mm00r;
    union mm01r mm01r;
    union mm02r mm02r;
    union mm03r mm03r;
    union mm04r mm04r;

    /* MIU interface control 1 */
    mm00r.whole = 0;
    mm00r.part.miu_enbl = 1;
    mm00r.part.mr_dsbl = 1;
    writel(mm00r.whole, MM00R(fb_info));
    mdelay(100);
    writel(0, MM00R(fb_info));
    mdelay(50);

    /* MIU interface control 2
     * o PLL 1 output is used as memory clock source.
     */
    mm01r.whole = 0;
    mm01r.part.msr_enbl = 1;
    mm01r.part.pb_cpu = 1;
    mm01r.part.pb_ge = 1;
    mm01r.part.mr_interval = 3744; /* normal memory refresh time interval */
    writel(mm01r.whole, MM01R(fb_info));

    /* memory interface control 3 */
    mm02r.whole = 0;
    mm02r.part.bs_stnr = 1;    /* burst count for STN read is four */
    mm02r.part.bs_ge = 3;      /* burst count for graphics engine is eight */
    mm02r.part.fifo_gc1 = 2;
    mm02r.part.fifo_gc2 = 1;
    mm02r.part.fifo_stnr = 2;
    mm02r.part.fifo_stnw = 1;
    mm02r.part.fifo_ge_src = 3;
    mm02r.part.fifo_ge_dst = 3;
    writel(mm02r.whole, MM02R(fb_info));

    /* memory interface control 5 */
    mm04r.whole = 0;
    mm04r.part.latency = 1;    /* EDRAM latency 1 */
    mm04r.part.dmm_cyc = 1;
    mm04r.part.bnk_act_cls = 1;
    writel(mm04r.whole, MM04R(fb_info));

    /* memory interface control 4 */
    mm03r.whole = 0;
    mm03r.part.rd_late_req = 1;
    mm03r.part.reserved_1 = 134; /* ? */
    writel(mm03r.whole, MM03R(fb_info));
    mdelay(10);

    /* MIU interface control 1 */
    writel(mm00r.whole, MM00R(fb_info));
    mdelay(50);

    PRINTK(CHIPNAME ": MIU reset.\n");
}


/****
 * initialize power management unit.
 */
static void
pmu_reset(const struct mq200fb_info* fb_info)
{
    union pm00r pm00r;
    union pm01r pm01r;
    union pm02r pm02r;
    union pm06r pm06r;
    union pm07r pm07r;

    /* power management miscellaneous control
     * o GE is driven by PLL 1 clock.
     */
    pm00r.whole = 0;
    pm00r.part.pll2_enbl = 1;
    pm00r.part.pll3_enbl = 1;
    pm00r.part.ge_enbl = 1;
    pm00r.part.ge_bsy_gl = 1;
    pm00r.part.ge_bsy_lcl = 1;
    pm00r.part.ge_clock = 1;   /* GE is driven by PLL 1 clock */
    pm00r.part.d3_mem_rfsh = 1;
    writel(pm00r.whole, PM00R(fb_info));
    mdelay(1);
    writel(pm00r.whole & 0x6000, PM00R(fb_info));
    mdelay(1);

    /* D1 state control */
    pm01r.whole = 0;
    pm01r.part.osc_enbl = 1;
    pm01r.part.pll1_enbl = 1;
    pm01r.part.pll2_enbl = 1;
    pm01r.part.miu_enbl = 1;
    pm01r.part.mem_rfsh = 1;
    pm01r.part.ge_enbl = 1;
    pm01r.part.fpd_enbl = 1;
    pm01r.part.ctl1_enbl = 1;
    pm01r.part.awin1_enbl = 1;
    writel(pm01r.whole, PM01R(fb_info));

    /* D2 state control */
    pm02r.whole = 0;
    pm02r.part.osc_enbl = 1;
    pm02r.part.pll1_enbl = 1;
    pm02r.part.pll2_enbl = 1;
    pm02r.part.miu_enbl = 1;
    pm02r.part.mem_rfsh = 1;
    pm02r.part.ge_enbl = 1;
    writel(pm02r.whole, PM02R(fb_info));

    /* PLL 2 programming */
    pm06r.whole = 0;
    pm06r.part.p_par = fb_info->current_par.pll[1].p_par;
    pm06r.part.n_par = fb_info->current_par.pll[1].n_par;
    pm06r.part.m_par = fb_info->current_par.pll[1].m_par;
    writel(pm06r.whole, PM06R(fb_info));

    /* PLL 3 programming */
    pm07r.whole = 0;
    pm07r.part.p_par = fb_info->current_par.pll[2].p_par;
    pm07r.part.n_par = fb_info->current_par.pll[2].n_par;
    pm07r.part.m_par = fb_info->current_par.pll[2].m_par;
    writel(pm07r.whole, PM07R(fb_info));

    writel(pm00r.whole, PM00R(fb_info));

    PRINTK(CHIPNAME ": PMU reset.\n");
}


/****
 * initialize graphics controller 1.
 */
static void
gc1_reset(const struct mq200fb_info* fb_info)
{
    unsigned long flags;
    union gc00r gc00r;
    union gc01r gc01r;
    union gc02r gc02r;
    union gc03r gc03r;
    union gc04r gc04r;
    union gc05r gc05r;
    union gc08r gc08r;
    union gc09r gc09r;
    union gc0er gc0er;
    union gc11r gc11r;

    spin_lock_irqsave(&fb_info->lock, flags);

    /* graphics controller CRT control */
    gc01r.whole = 0;
    gc01r.part.dac_enbl = 1;
    gc01r.part.hsync_out = 1;
    gc01r.part.vsync_out = 1;
    writel(gc01r.whole, GC01R(fb_info));

    /* horizontal display 1 control */
    gc02r.whole = 0;
    gc02r.part.hd1t = fb_info->current_par.gc[0].disp.hdt;
    gc02r.part.hd1e = fb_info->current_par.gc[0].disp.hde;
    writel(gc02r.whole, GC02R(fb_info));

    /* vertical display 1 control */
    gc03r.whole = 0;
    gc03r.part.vd1t = fb_info->current_par.gc[0].disp.vdt;
    gc03r.part.vd1e = fb_info->current_par.gc[0].disp.vde;
    writel(gc03r.whole, GC03R(fb_info));

    /* horizontal sync 1 control */
    gc04r.whole = 0;
    gc04r.part.hs1s = fb_info->current_par.gc[0].disp.hss;
    gc04r.part.hs1e = fb_info->current_par.gc[0].disp.hse;
    writel(gc04r.whole, GC04R(fb_info));

    /* vertical sync 1 control */
    gc05r.whole = 0;
    gc05r.part.vs1s = fb_info->current_par.gc[0].disp.vss;
    gc05r.part.vs1e = fb_info->current_par.gc[0].disp.vse;
    writel(gc05r.whole, GC05R(fb_info));

    /* horizontal window 1 control */
    gc08r.whole = 0;
    gc08r.part.hw1s = fb_info->current_par.gc[0].win.hws;
    gc08r.part.hw1w = fb_info->current_par.gc[0].win.hww;
    gc08r.part.w1ald = fb_info->current_par.gc[0].win.wald;
    writel(gc08r.whole, GC08R(fb_info));

    /* vertical window 1 control */
    gc09r.whole = 0;
    gc09r.part.vw1s = fb_info->current_par.gc[0].win.vws;
    gc09r.part.vw1h = fb_info->current_par.gc[0].win.vwh;
    writel(gc09r.whole, GC09R(fb_info));

    /* alternate horizontal window 1 control */
    writel(0, GC0AR(fb_info));

    /* alternate vertical window 1 control */
    writel(0, GC0BR(fb_info));

    /* window 1 start address */
    writel(fb_info->current_par.gc[0].win.wsa, GC0CR(fb_info));

    /* alternate window 1 start address */
    writel(0, GC0DR(fb_info));

    /* window 1 stride */
    gc0er.whole = 0;
    gc0er.part.w1st = fb_info->current_par.gc[0].win.wst;
    gc0er.part.aw1st = fb_info->current_par.gc[0].awin.wst;
    writel(gc0er.whole, GC0ER(fb_info));

    /* reserved register - ??? - */
    writel(0x31f, GC0FR(fb_info));

    /* hardware cursor 1 position */
    writel(0, GC10R(fb_info));

    /* hardware cursor 1 start address and offset */
    gc11r.whole = 0;
    gc11r.part.hc1sa = CURSOR_OFFSET >> 10;
    writel(gc11r.whole, GC11R(fb_info));

    /* hardware cursor 1 foreground color */
    writel(0x00ffffff, GC12R(fb_info));

    /* hardware cursor 1 background color */
    writel(0x00000000, GC13R(fb_info));

    /* graphics controller 1 register
     * o GC1 clock source is PLL 2.
     * o hardware cursor is enabled.
     */
    gc00r.whole = 0;
    gc00r.part.ctl_enbl = 1;
    gc00r.part.iwin_enbl = 1;
    gc00r.part.gcd = fb_info->current_par.gc[0].gcd;
    gc00r.part.hc_enbl = 1;
   gc00r.part.agcd = fb_info->current_par.gc[0].agcd;
    gc00r.part.g1rclk_src = 2; /* PLL 2 */
    gc00r.part.fd = 0;         /* FD = 1 */
    gc00r.part.sd = 1;         /* SD = 1 */
    writel(gc00r.whole, GC00R(fb_info));

    spin_unlock_irqrestore(&fb_info->lock, flags);

    PRINTK(CHIPNAME ": GC1 reset.\n");
}


/****
 * initialize graphics engine.
 */
static void
ge_reset(const struct mq200fb_info* fb_info)
{
    /* drawing command register */
    writel(0, GE00R(fb_info));

    /* promary width and height register */
    writel(0, GE01R(fb_info));

    /* primary destination address register */
    writel(0, GE02R(fb_info));

    /* primary source XY register */
    writel(0, GE03R(fb_info));

    /* primary color compare register */
    writel(0, GE04R(fb_info));

    /* primary clip left/top register */
    writel(0, GE05R(fb_info));

    /* primary clip right/bottom register */
    writel(0, GE06R(fb_info));

    /* primary source and pattern offset register */
    writel(0, GE07R(fb_info));

    /* primary foreground color register/rectangle fill color depth */
    writel(0, GE08R(fb_info));

    /* source stride/offset register */
    writel(0, GE09R(fb_info));

    /* destination stride register and color depth */
    writel(0, GE0AR(fb_info));

    /* image base address register */
    writel(0, GE0BR(fb_info));

    PRINTK(CHIPNAME ": GE reset.\n");
}


/****
 * initialize Color Palette 1.
 */
static void
cp1_reset(const struct mq200fb_info* fb_info)
{
    int i;

    for (i = 0; i < 256; i++)
       writel(0, C1xxR(fb_info, i));
}


/****
 * change screen size and depth, and so on.
 * Right now, screen attributes modification is not implemeted.
 */
static void
set_screen(const struct mq200fb_info* fb_info,
          const struct mq200fb_par* par)
{
    unsigned long flags;
    union ge0ar ge0ar;

    /* set destination stride and color depth for GE */
    ge0ar.whole = 0;
    ge0ar.part.dst_strid = get_line_length(par);
    switch (get_bits_per_pixel(par)) {
    case 8:
       ge0ar.part.col_dpth = 0;
       break;
    case 16:
       ge0ar.part.col_dpth = 1;
       break;
    case 32:
       ge0ar.part.col_dpth = 3;
       break;
    default:
       printk(CHIPNAME ": unexpected %d bits/pixel\n",
              get_bits_per_pixel(par));
       break;
    }
    spin_lock_irqsave(&fb_info->lock, flags);
    writel(ge0ar.whole, GE0AR(fb_info));
    spin_unlock_irqrestore(&fb_info->lock, flags);
}


/****
 * set hardware cursor shape.
 *
 * [NOTE]
 *     This function assume that cursor width is less than or equal to 64
 *     pixels.
 */
static void
set_cursor_shape(const struct mq200fb_info* fb_info)
{
    static int already_created;
    static u32 cursor_images[2][256];
    unsigned long flags;
    u32 image;

    if (! already_created) {
       u16 and_bits[4], xor_bits[4];
       u32 transparent = 0x0000FFFF;
       int line, i;

       /*
        * set "CM_DRAW" cursor shape
        */
       image = fb_info->regions.v_fb + CURSOR_OFFSET;

       /* inverse transparency */
       for (i = 0; i < fb_info->cursor_info.size.x / 16; i++)
           and_bits[i] = xor_bits[i] = ~0;
       /* partially inverse transparency */
       and_bits[i] = ~0;
       xor_bits[i] = (u16)~0 >> (16 - fb_info->cursor_info.size.x % 16);
       /* rest is transparent */
       while (++i < 4) {
           and_bits[i] = ~0;
           xor_bits[i] = 0;
       }

       spin_lock_irqsave(&fb_info->lock, flags);
       for (line = 0; line < fb_info->cursor_info.size.y; line++)
           for (i = 0; i < 4; i++) {
               writel(((u32)xor_bits[i] << 16) | (u32)and_bits[i], image);
               image += 4;
           }

       while (line++ < 64)
           for (i = 0; i < 4; i++) {
               writel(transparent, image);
               image += 4;
           }

       /*
        * set "CM_ERASE" cursor shape
        */
       image = fb_info->regions.v_fb + CURSOR_OFFSET + 1024;
       for (line = 0; line < 64; line++)
           for (i = 0; i < 4; i++) {
               writel(transparent, image);
               image += 4;
           }

       /* save cursor images */
       image = fb_info->regions.v_fb + CURSOR_OFFSET;
       for (i = 0; i < 2; i++) {
           int j;

           for (j = 0; j < 256; j++, image += 4)
               cursor_images[i][j] = readl(image);
       }
       spin_unlock_irqrestore(&fb_info->lock, flags);

       already_created = 1;
    }
    else {
       int i;

       /* restore cursor images */
       image = fb_info->regions.v_fb + CURSOR_OFFSET;

       spin_lock_irqsave(&fb_info->lock, flags);
       for (i = 0; i < 2; i++) {
           int j;

           for (j = 0; j < 256; j++, image += 4)
               writel(cursor_images[i][j], image);
       }
       spin_unlock_irqrestore(&fb_info->lock, flags);
    }
}


/****
 * turn on hardware cursor.
 */
static void
enable_cursor(struct mq200fb_info* fb_info)
{
    unsigned long flags;
    union gc00r gc00r;

    spin_lock_irqsave(&fb_info->lock, flags);
    gc00r.whole = readl(GC00R(fb_info));
    if (gc00r.part.hc_enbl == 0) {
       set_cursor_shape(fb_info);

       gc00r.part.hc_enbl = 1;
       writel(gc00r.whole, GC00R(fb_info));
    }
    spin_unlock_irqrestore(&fb_info->lock, flags);
}


/****
 * turn off hardware cursor.
 */
static void
disable_cursor(struct mq200fb_info* fb_info)
{
    unsigned long flags;
    union gc00r gc00r;

    spin_lock_irqsave(&fb_info->lock, flags);
    gc00r.whole = readl(GC00R(fb_info));
    if (gc00r.part.hc_enbl == 1) {
       gc00r.part.hc_enbl = 0;
       writel(gc00r.whole, GC00R(fb_info));
    }
    spin_unlock_irqrestore(&fb_info->lock, flags);
}


/****
 * Set current window's cursor position.
 *
 * [RETURN]
 *     1: when cursor position has been moved.
 */
static int
set_cursor_pos(const struct mq200fb_info* fb_info,
              unsigned x,      /* horizontal pos in pixels */
              unsigned y)      /* vertical pos in pixels */
{
    int ret;
    unsigned long flags;
    union gc10r gc10r;

    spin_lock_irqsave(&fb_info->lock, flags);
    gc10r.whole = readl(GC10R(fb_info));
    if (gc10r.part.hc1s == x && gc10r.part.vc1s == y)
       ret = 0;
    else {
       gc10r.whole = 0;
       gc10r.part.hc1s = x;
       gc10r.part.vc1s = y;
       writel(gc10r.whole, GC10R(fb_info));

       ret = 1;
    }
    spin_unlock_irqrestore(&fb_info->lock, flags);

    return ret;
}


/****
 * blink cursor.
 */
static void
cursor_timer_handler(unsigned long data)
{
    unsigned long flags;
    struct mq200fb_info* fb_info = (struct mq200fb_info*)data;
    union gc11r gc11r;

    spin_lock_irqsave(&fb_info->lock, flags);
    gc11r.whole = readl(GC11R(fb_info));
    if (gc11r.part.hc1sa == CURSOR_OFFSET >> 10)
       gc11r.part.hc1sa += 1;
    else
       gc11r.part.hc1sa = CURSOR_OFFSET >> 10;
    writel(gc11r.whole, GC11R(fb_info));
    spin_unlock_irqrestore(&fb_info->lock, flags);

    add_cursor_timer(&fb_info->cursor_info);
}


static void
get_color_palette(const struct mq200fb_info* fb_info,
                 unsigned indx,
                 unsigned* red,
                 unsigned* green,
                 unsigned* blue)
{
    unsigned long flags;
    union c1xxr c1xxr;

    spin_lock_irqsave(&fb_info->lock, flags);
    c1xxr.whole = readl(C1xxR(fb_info, indx));
    spin_unlock_irqrestore(&fb_info->lock, flags);

    *red = c1xxr.part.red;
    *green = c1xxr.part.green;
    *blue = c1xxr.part.blue;
}


static void
set_color_palette(const struct mq200fb_info* fb_info,
                 unsigned indx,
                 unsigned red,
                 unsigned green,
                 unsigned blue)
{
    unsigned long flags;
    union c1xxr c1xxr;

    c1xxr.whole = 0;
    c1xxr.part.red = red;
    c1xxr.part.green = green;
    c1xxr.part.blue = blue;

    spin_lock_irqsave(&fb_info->lock, flags);
    writel(c1xxr.whole, C1xxR(fb_info, indx));
    spin_unlock_irqrestore(&fb_info->lock, flags);
}


/****
 * enable CRT DAC.
 */
static void
enable_dac(const struct mq200fb_info* fb_info)
{
    unsigned long flags;
    union gc01r gc01r;

    spin_lock_irqsave(&fb_info->lock, flags);
    gc01r.whole = readl(GC01R(fb_info));
    gc01r.part.dac_enbl = 1;
    writel(gc01r.whole, GC01R(fb_info));
    spin_unlock_irqrestore(&fb_info->lock, flags);
}


/****
 * disable CRT DAC.
 */
static void
disable_dac(const struct mq200fb_info* fb_info,
           unsigned sync_mode) /* specify SYNC that output while DAC is
                                  disabled */
{
    unsigned long flags;
    union gc01r gc01r;

    spin_lock_irqsave(&fb_info->lock, flags);
    gc01r.whole = readl(GC01R(fb_info));
    gc01r.part.dac_enbl = 0;
    gc01r.part.hsync_out = (sync_mode & WITH_HSYNC) != 0;
    gc01r.part.vsync_out = (sync_mode & WITH_VSYNC) != 0;
    writel(gc01r.whole, GC01R(fb_info));
    spin_unlock_irqrestore(&fb_info->lock, flags);
}


/****
 * Return true if command FIFO .
 */
static int __inline__
is_enough_cmd_fifo(const struct mq200fb_info* fb_info,
                  int num)
{
    union cc01r cc01r;

    cc01r.whole = readl(CC01R(fb_info));
    return cc01r.part.cmd_fifo >= num;
}


/****
 *
 */
static int __inline__
is_enough_src_fifo(const struct mq200fb_info* fb_info,
                  int num)
{
    union cc01r cc01r;

    cc01r.whole = readl(CC01R(fb_info));
    return cc01r.part.src_fifo >= num;
}


/****
 * wait until command FIFO space is enough.
 */
static void
wait_cmd_fifo(const struct mq200fb_info* fb_info,
             int num)
{
    int i;

    for (i = 0; i < 0x00100000; i++)
       if (is_enough_cmd_fifo(fb_info, num))
           return;
    printk(CHIPNAME ": Command FIFO is full.\n");
}


/****
 *
 */
static void
wait_src_fifo(const struct mq200fb_info* fb_info,
             int num)
{
    int i;

    for (i = 0; i < 0x00100000; i++)
       if (is_enough_src_fifo(fb_info, num))
           return;
    printk(CHIPNAME ": Source FIFO is full.\n");
}


/****************************************************************
 *  Frame buffer operations
 */

/****
 * rectangle fill (solid fill)
 */
static void
ge_rect_fill(const struct mq200fb_info* fb_info,
            int x,
            int y,
            int width,
            int height,
            u32 bgcol)
{
    unsigned long flags;
    union ge01r ge01r;
    union ge02r ge02r;
    union ge00r ge00r;

    ge01r.whole = 0;
    ge01r.bitblt.width = width;
    ge01r.bitblt.height = height;

    ge02r.whole = 0;
    ge02r.window.dst_x = x;
    ge02r.window.dst_y = y;

    ge00r.whole = 0;
    ge00r.part.rop = ROP_PATCOPY;
    ge00r.part.cmd_typ = 2;    /* BitBLT */
    ge00r.part.mon_ptn = 1;
    ge00r.part.sol_col = 1;

    wait_cmd_fifo(fb_info, 4);
    spin_lock_irqsave(&fb_info->lock, flags);
    writel(ge01r.whole, GE01R(fb_info));
    writel(ge02r.whole, GE02R(fb_info));
    writel(bgcol, GE42R(fb_info));
    writel(ge00r.whole, GE00R(fb_info));
    spin_unlock_irqrestore(&fb_info->lock, flags);
}


#if 0
/****
 * bitblt screen to screen.
 */
static void
ge_bitblt_scr_to_scr(const struct mq200fb_info* fb_info,
                    int src_x,
                    int src_y,
                    int dst_x,
                    int dst_y,
                    int width,
                    int height)
{
    unsigned long flags;
    union ge00r ge00r;
    union ge01r ge01r;
    union ge02r ge02r;
    union ge03r ge03r;

    ge01r.whole = 0;
    ge01r.bitblt.width = width;
    ge01r.bitblt.height = height;

    ge02r.whole = 0;
    ge02r.window.dst_x = dst_x;
    ge02r.window.dst_y = dst_y;

    ge03r.whole = 0;
    ge03r.window.src_x = src_x;
    ge03r.window.src_y = src_y;

    ge00r.whole = 0;
    ge00r.part.rop = ROP_SRCCOPY;
    ge00r.part.cmd_typ = 2;    /* BitBLT */
    if (src_x >= dst_x) {
       if (src_y < dst_y)
           ge00r.part.y_dir = 1;
    }
    else if (src_y >= dst_y)
       ge00r.part.x_dir = 1;
    else {
       ge00r.part.x_dir = 1;
       ge00r.part.y_dir = 1;
    }

    wait_cmd_fifo(fb_info, 4);
    spin_lock_irqsave(&fb_info->lock, flags);
    writel(ge01r.whole, GE01R(fb_info));
    writel(ge02r.whole, GE02R(fb_info));
    writel(ge03r.whole, GE03R(fb_info));
    writel(ge00r.whole, GE00R(fb_info));
    spin_unlock_irqrestore(&fb_info->lock, flags);
}
#endif


/****
 * draw mono text image.
 */
static void
ge_bitblt_char(const struct mq200fb_info* fb_info,
              int dst_x, int dst_y,
              u8* image,
              int width, int height,
              u32 fgcol, u32 bgcol) /* foreground and background color */
{
    unsigned long flags;
    int i;
    union ge00r ge00r;
    union ge01r ge01r;
    union ge02r ge02r;
    union ge09r ge09r;
    int offset = ((u32)image & 0x3) * 8;

    ge01r.whole = 0;
    ge01r.bitblt.width = width;
    ge01r.bitblt.height = height;

    ge02r.whole = 0;
    ge02r.window.dst_x = dst_x;
    ge02r.window.dst_y = dst_y;

    ge09r.whole = 0;
    ge09r.pack_mono.strt_bit = offset;
    ge09r.pack_mono.amount = 0;
    ge09r.pack_mono.bit_spc = 0;

    ge00r.whole = 0;
    ge00r.part.rop = ROP_SRCCOPY;
    ge00r.part.cmd_typ = 2;    /* bitblt */
    ge00r.part.src_mem = 1;
    ge00r.part.mon_src = 1;
    ge00r.part.mod_sel = 1;    /* packed mode */

    wait_cmd_fifo(fb_info, 6);
    spin_lock_irqsave(&fb_info->lock, flags);
    writel(ge01r.whole, GE01R(fb_info));
    writel(ge02r.whole, GE02R(fb_info));
    writel(fgcol, GE07R(fb_info));
   writel(bgcol, GE08R(fb_info));
    writel(ge09r.whole, GE09R(fb_info));
    writel(ge00r.whole, GE00R(fb_info));

    /* ---- */
    for (i = 0; i < height; i += 4) {
       u32 src_data = *(u32*)(image + i);

       wait_src_fifo(fb_info, 8);
       writel(src_data, PSF(fb_info));
    }
    spin_unlock_irqrestore(&fb_info->lock, flags);
}


/****************************************************************
 * Hardware Independent Functions.
 */

/****
 *  Initialization
 *
 * [RETURN]
 *         0: success
 *     minus: fail
 */
int __init
mq200fb_init(void)
{
#if 0
    int irqval;
#endif
    memset(&mq200fb_info, 0, sizeof mq200fb_info);
    mq200fb_info.current_par = mq200fb_options.par;
    if (! mq200fb_conf(&mq200fb_info))
       return -ENXIO;
    mq200_reset(&mq200fb_info);

    mq200fb_info.disp.scrollmode = SCROLL_YNOMOVE;
    mq200fb_info.gen.parsize = sizeof(struct mq200fb_par);
    mq200fb_info.gen.fbhw = &mq200_switch;
    strcpy(mq200fb_info.gen.info.modename, CHIPNAME);
    mq200fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
    mq200fb_info.gen.info.fbops = &mq200fb_ops;
    mq200fb_info.gen.info.disp = &mq200fb_info.disp;
    strcpy(mq200fb_info.gen.info.fontname, mq200fb_options.font);
    mq200fb_info.gen.info.switch_con = &fbgen_switch;
    mq200fb_info.gen.info.updatevar = &fbgen_update_var;
    mq200fb_info.gen.info.blank = &fbgen_blank;
    spin_lock_init(&mq200fb_info.lock);

    fbgen_get_var(&mq200fb_info.disp.var, -1, &mq200fb_info.gen.info);
    fbgen_do_set_var(&mq200fb_info.disp.var, 1, &mq200fb_info.gen);
    fbgen_set_disp(-1, &mq200fb_info.gen);
    fbgen_install_cmap(0, &mq200fb_info.gen);
    if (register_framebuffer(&mq200fb_info.gen.info) < 0) {
       printk(KERN_ERR DEVICE_DRIVER_NAME ": unable to register.\n");
       return -EINVAL;
    }
    printk(
       KERN_INFO "fb%d: %s, using %uK of video memory.\n",
       GET_FB_IDX(mq200fb_info.gen.info.node),
       CHIPNAME,
       (u32)(mq200fb_info.regions.fb_size >> 10)
    );

#if 0
    if ((irqval = request_irq(MQ200_IRQ, mq200_interrupt, 0, CHIPNAME,
                             NULL)) != 0) {
       printk(CHIPNAME ": unable to get IRQ %d (irqval=%d).\n",
              MQ200_IRQ, irqval);
       return -EAGAIN;
    }
    mq200_enable_irq(&mq200fb_info);
#endif

    return 0;
}


/****
 *  Cleanup
 */
void mq200fb_cleanup(struct fb_info *info)
{
    /*
     *  If your driver supports multiple boards, you should unregister and
     *  clean up all instances.
     */

    unregister_framebuffer(info);
    /* ... */
}


/****
 *  Setup
 */
int __init mq200fb_setup(char *options)
{
    /* Parse user speficied options (`video=mq200fb:') */
    return 0;
}


/****
 * add cursor timer to timer_list.
 */
static void
add_cursor_timer(struct mq200fb_cursor_info* cursor_info)
{
    cursor_info->timer.expires = jiffies + CURSOR_BLINK_RATE;
    add_timer(&cursor_info->timer);
}


/****************************************************************
 *  Modularization
 */

#ifdef MODULE

int
init_module(void)
{
    return mq200fb_init();
}


void cleanup_module(void)
{
    mq200fb_cleanup(void);
}
#endif /* MODULE */


/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */

