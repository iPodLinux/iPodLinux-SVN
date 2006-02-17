#include <sys/types.h>
#include <sys/ioctl.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>

#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <linux/fb.h>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_events_c.h"
#include "SDL_sysevents.h"
#include "SDL_ipodvideo.h"

#define _THIS SDL_VideoDevice *this

static int iPod_VideoInit (_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **iPod_ListModes (_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *iPod_SetVideoMode (_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int iPod_SetColors (_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void iPod_UpdateRects (_THIS, int nrects, SDL_Rect *rects);
static void iPod_VideoQuit (_THIS);
static void iPod_PumpEvents (_THIS);

static long iPod_GetGeneration();

static int initd = 0;
static int kbfd = -1;
static int fbfd = -1;
static int oldvt = -1;
static int curvt = -1;
static int old_kbmode = -1;
static long generation = 0;
static struct termios old_termios, cur_termios;
static int lcd_type;

FILE *dbgout;

#define LCD_DATA          0x10
#define LCD_CMD           0x08
#define IPOD_OLD_LCD_BASE 0xc0001000
#define IPOD_OLD_LCD_RTC  0xcf001110
#define IPOD_NEW_LCD_BASE 0x70003000
#define IPOD_NEW_LCD_RTC  0x60005010
#define outl(datum,addr) (*(volatile unsigned long *)(addr) = (datum))
#define inl(addr) (*(volatile unsigned long *)(addr))

static unsigned long lcd_base, lcd_rtc, lcd_width, lcd_height;

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

static int iPod_Available() 
{
    return 1;
}

static void iPod_DeleteDevice (SDL_VideoDevice *device)
{
    free (device->hidden);
    free (device);
}

void iPod_InitOSKeymap (_THIS) {}

static SDL_VideoDevice *iPod_CreateDevice (int devindex)
{
    SDL_VideoDevice *this;
    
    this = (SDL_VideoDevice *)malloc (sizeof(SDL_VideoDevice));
    if (this) {
	memset (this, 0, sizeof *this);
	this->hidden = (struct SDL_PrivateVideoData *) malloc (sizeof(struct SDL_PrivateVideoData));
    }
    if (!this || !this->hidden) {
	SDL_OutOfMemory();
	if (this)
	    free (this);
	return 0;
    }
    memset (this->hidden, 0, sizeof(struct SDL_PrivateVideoData));
    
    generation = iPod_GetGeneration();

    this->VideoInit = iPod_VideoInit;
    this->ListModes = iPod_ListModes;
    this->SetVideoMode = iPod_SetVideoMode;
    this->SetColors = iPod_SetColors;
    this->UpdateRects = iPod_UpdateRects;
    this->VideoQuit = iPod_VideoQuit;
    this->AllocHWSurface = 0;
    this->CheckHWBlit = 0;
    this->FillHWRect = 0;
    this->SetHWColorKey = 0;
    this->SetHWAlpha = 0;
    this->LockHWSurface = 0;
    this->UnlockHWSurface = 0;
    this->FlipHWSurface = 0;
    this->FreeHWSurface = 0;
    this->SetCaption = 0;
    this->SetIcon = 0;
    this->IconifyWindow = 0;
    this->GrabInput = 0;
    this->GetWMInfo = 0;
    this->InitOSKeymap = iPod_InitOSKeymap;
    this->PumpEvents = iPod_PumpEvents;
    this->free = iPod_DeleteDevice;

    return this;
}

VideoBootStrap iPod_bootstrap = {
    "ipod", "iPod Framebuffer Driver",
    iPod_Available, iPod_CreateDevice
};

//--//

static int iPod_VideoInit (_THIS, SDL_PixelFormat *vformat)
{
    if (!initd) {
	/*** Code adapted/copied from SDL fbcon driver. ***/

	static const char * const tty0[] = { "/dev/tty0", "/dev/vc/0", 0 };
	static const char * const vcs[] = { "/dev/vc/%d", "/dev/tty%d", 0 };
	int i, tty0_fd;

	dbgout = fdopen (open ("/etc/sdlpod.log", O_WRONLY | O_SYNC | O_APPEND), "a");
	if (dbgout) {
	    setbuf (dbgout, 0);
	    fprintf (dbgout, "--> Started SDL <--\n");
	}

	// Try to query for a free VT
	tty0_fd = -1;
	for ( i=0; tty0[i] && (tty0_fd < 0); ++i ) {
	    tty0_fd = open(tty0[i], O_WRONLY, 0);
	}
	if ( tty0_fd < 0 ) {
	    tty0_fd = dup(0); /* Maybe stdin is a VT? */
	}
	ioctl(tty0_fd, VT_OPENQRY, &curvt);
	close(tty0_fd);

#if 0
	tty0_fd = open("/dev/tty", O_RDWR, 0);
	if ( tty0_fd >= 0 ) {
	    ioctl(tty0_fd, TIOCNOTTY, 0);
	    close(tty0_fd);
	}
#endif

	if ( (geteuid() == 0) && (curvt > 0) ) {
	    for ( i=0; vcs[i] && (kbfd < 0); ++i ) {
		char vtpath[12];
		
		sprintf(vtpath, vcs[i], curvt);
		kbfd = open(vtpath, O_RDWR);
	    }
	}
	if ( kbfd < 0 ) {
	    if (dbgout) fprintf (dbgout, "Couldn't open any VC\n");
	    return -1;
	}
	if (dbgout) fprintf (stderr, "Current VT: %d\n", curvt);

	if (kbfd >= 0) {
	    /* Switch to the correct virtual terminal */
	    if ( curvt > 0 ) {
		struct vt_stat vtstate;
		
		if ( ioctl(kbfd, VT_GETSTATE, &vtstate) == 0 ) {
		    oldvt = vtstate.v_active;
		}
		if ( ioctl(kbfd, VT_ACTIVATE, curvt) == 0 ) {
		    if (dbgout) fprintf (dbgout, "Waiting for switch to this VT... ");
		    ioctl(kbfd, VT_WAITACTIVE, curvt);
		    if (dbgout) fprintf (dbgout, "done!\n");
		}
	    }

	    // Set terminal input mode
	    if (tcgetattr (kbfd, &old_termios) < 0) {
		if (dbgout) fprintf (dbgout, "Can't get termios\n");
		return -1;
	    }
	    cur_termios = old_termios;
	    //	    cur_termios.c_iflag &= ~(ICRNL | INPCK | ISTRIP | IXON);
	    //	    cur_termios.c_iflag |= (BRKINT);
	    //	    cur_termios.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
	    //	    cur_termios.c_oflag &= ~(OPOST);
	    //	    cur_termios.c_oflag |= (ONOCR | ONLRET);
	    cur_termios.c_lflag &= ~(ICANON | ECHO | ISIG);
	    cur_termios.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
	    cur_termios.c_cc[VMIN] = 0;
	    cur_termios.c_cc[VTIME] = 0;
	    
	    if (tcsetattr (kbfd, TCSAFLUSH, &cur_termios) < 0) {
		if (dbgout) fprintf (dbgout, "Can't set termios\n");
		return -1;
	    }
	    if (ioctl (kbfd, KDSKBMODE, K_MEDIUMRAW) < 0) {
		if (dbgout) fprintf (dbgout, "Can't set medium-raw mode\n");
		return -1;
	    }
	    if (ioctl (kbfd, KDSETMODE, KD_GRAPHICS) < 0) {
		if (dbgout) fprintf (dbgout, "Can't set graphics\n");
		return -1;
	    }
	}

	// Open the framebuffer
	if ((fbfd = open ("/dev/fb0", O_RDWR)) < 0) {
	    if ((fbfd = open ("/dev/fb/0", O_RDWR)) < 0) {
		if (dbgout) fprintf (dbgout, "Can't open framebuffer\n");
		return -1;
	    }
	}
	{
	    struct fb_var_screeninfo vinfo;
	    
	    if (dbgout) fprintf (dbgout, "Generation: 0x%lx\n", generation);
	    
	    if (generation >= 0x40000) {
		lcd_base = IPOD_NEW_LCD_BASE;
	    } else {
		lcd_base = IPOD_OLD_LCD_BASE;
	    }
	    
	    ioctl (fbfd, FBIOGET_VSCREENINFO, &vinfo);
	    close (fbfd);

	    if (lcd_base == IPOD_OLD_LCD_BASE)
		lcd_rtc = IPOD_OLD_LCD_RTC;
	    else if (lcd_base == IPOD_NEW_LCD_BASE)
		lcd_rtc = IPOD_NEW_LCD_RTC;
	    else {
		SDL_SetError ("Unknown iPod version");
		return -1;
	    }

	    lcd_width = vinfo.xres;
	    lcd_height = vinfo.yres;

	    if (dbgout) fprintf (dbgout, "LCD is %ldx%ld\n", lcd_width, lcd_height);
	}

	fcntl (kbfd, F_SETFL, O_RDWR | O_NONBLOCK);

	if ((generation >> 16) == 0x6) {
	    vformat->BitsPerPixel = 16;
	    vformat->Rmask = 0xF800;
	    vformat->Gmask = 0x07E0;
	    vformat->Bmask = 0x001F;
	    lcd_width = 220;
	    lcd_height = 176;
        } else if ((generation >> 16) == 0xc) {
	    vformat->BitsPerPixel = 16;
	    vformat->Rmask = 0xF800;
	    vformat->Gmask = 0x07E0;
	    vformat->Bmask = 0x001F;
	    lcd_width = 176;
	    lcd_height = 132;
	} else {
	    vformat->BitsPerPixel = 8;
	    vformat->Rmask = vformat->Gmask = vformat->Bmask = 0;
	}

        if (generation == 0x60000) {
            lcd_type = 0;
        } else {
            if ((generation >> 16) == 0x6) {
                int gpio_a01, gpio_a04;

                gpio_a01 = (inl (0x6000D030) & 0x2) >> 1;
                gpio_a04 = (inl (0x6000D030) & 0x10) >> 4;
                if (((gpio_a01 << 1) | gpio_a04) == 0 || ((gpio_a01 << 1) | gpio_a04) == 2) {
                    lcd_type = 0;
                } else {
                    lcd_type = 1;   
                }
            }
            else if ((generation >> 16) == 0xc) {
                lcd_type = 1;
            }      
        }

	initd = 1;
	if (dbgout) fprintf (dbgout, "Initialized.\n\n");
    }
    return 0;
}

static SDL_Rect **iPod_ListModes (_THIS, SDL_PixelFormat *format, Uint32 flags)
{
    int width, height, fd;
    static SDL_Rect r;
    static SDL_Rect *rs[2] = { &r, 0 };

    if ((fd = open ("/dev/fb0", O_RDWR)) < 0) {
	if ((fd = open ("/dev/fb/0", O_RDWR)) < 0) {
	    return 0;
	}
    }
    {
	struct fb_var_screeninfo vinfo;
	
	ioctl (fbfd, FBIOGET_VSCREENINFO, &vinfo);
	close (fbfd);
	
	width = vinfo.xres;
	height = vinfo.yres;
    }
    r.x = r.y = 0;
    r.w = width;
    r.h = height;
    return rs;
}


static SDL_Surface *iPod_SetVideoMode (_THIS, SDL_Surface *current, int width, int height, int bpp,
				       Uint32 flags)
{
    Uint32 Rmask, Gmask, Bmask;
    if (bpp > 8) {
	Rmask = 0xF800;
	Gmask = 0x07E0;
	Bmask = 0x001F;	
    } else {
	Rmask = Gmask = Bmask = 0;
    }

    current = SDL_VideoSurface = SDL_CreateRGBSurface (0, width + 4, height, (bpp<8)?8:bpp, Rmask, Gmask, Bmask, 0);
    current->w = width;
    if (bpp <= 8)
        current->pitch = (lcd_width + 3) & ~3;
    else
        current->pitch = lcd_width * 2;

    if (bpp <= 8) {
	int i, j;
	for (i = 0; i < 256; i += 4) {
	    for (j = 0; j < 4; j++) {
		int val = (j == 3)? 255 : 80*j;
		current->format->palette->colors[i+j].r = val;
		current->format->palette->colors[i+j].g = val;
		current->format->palette->colors[i+j].b = val;
	    }
	}
    }

    return current;
}

static int iPod_SetColors (_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
    if (SDL_VideoSurface && SDL_VideoSurface->format && SDL_VideoSurface->format->palette) {
	int i;
	for (i = 0; i < 256; i += 4) {
	    SDL_VideoSurface->format->palette->colors[i+0].r = 
		SDL_VideoSurface->format->palette->colors[i+0].g = 
		SDL_VideoSurface->format->palette->colors[i+0].b = 255;

	    SDL_VideoSurface->format->palette->colors[i+1].r = 
		SDL_VideoSurface->format->palette->colors[i+1].g = 
		SDL_VideoSurface->format->palette->colors[i+1].b = 160;

	    SDL_VideoSurface->format->palette->colors[i+2].r = 
		SDL_VideoSurface->format->palette->colors[i+2].g = 
		SDL_VideoSurface->format->palette->colors[i+2].b = 80;

	    SDL_VideoSurface->format->palette->colors[i+3].r = 
		SDL_VideoSurface->format->palette->colors[i+3].g = 
		SDL_VideoSurface->format->palette->colors[i+3].b = 0;
	}
    }
    return 0;
}

static void iPod_VideoQuit (_THIS)
{
    ioctl (kbfd, KDSETMODE, KD_TEXT);
    tcsetattr (kbfd, TCSAFLUSH, &old_termios);
    old_kbmode = -1;

    if (oldvt > 0)
	ioctl (kbfd, VT_ACTIVATE, oldvt);
    
    if (kbfd > 0)
	close (kbfd);

    if (dbgout) {
	fprintf (dbgout, "<-- Ended SDL -->\n");
	fclose (dbgout);
    }
    
    kbfd = -1;
}

static char iPod_SC_keymap[] = {
    0,				/* 0 - no key */
    '[' - 0x40,			/* ESC (Ctrl+[) */
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '-', '=',
    '\b', '\t',			/* Backspace, Tab (Ctrl+H,Ctrl+I) */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0,			/* Enter, Left CTRL */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\',			/* left shift, backslash */
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, '*', 0, ' ', 0,		/* right shift, KP mul, left alt, space, capslock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1-10 */
    0, 0,			/* numlock, scrollock */
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', /* numeric keypad */
    0, 0,			/* padding */
    0, 0, 0,			/* "less" (?), F11, F12 */
    0, 0, 0, 0, 0, 0, 0,	/* padding */
    '\n', 0, '/', 0, 0,	/* KP enter, Rctrl, Ctrl, KP div, PrtSc, RAlt */
    0, 0, 0, 0, 0, 0, 0, 0, 0,	/* Break, Home, Up, PgUp, Left, Right, End, Down, PgDn */
    0, 0,			/* Ins, Del */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* padding */
    0, 0,			/* RWin, LWin */
    0				/* no key */
};
    

static void iPod_keyboard() 
{
    unsigned char keybuf[128];
    int i, nread;
    SDL_keysym keysym;

    keysym.mod = 0;
    keysym.scancode = 0xff;

    nread = read (kbfd, keybuf, 128);
    for (i = 0; i < nread; i++) {
	char ascii = iPod_SC_keymap[keybuf[i] & 0x7f];

	if (dbgout) fprintf (dbgout, "@%d Key! %02x is %c %s\n", SDL_GetTicks(), keybuf[i], ascii, (keybuf[i] & 0x80)? "up" : "down");

	keysym.sym = keysym.unicode = ascii;
	if (ascii == '\n') keysym.sym = SDLK_RETURN;

	SDL_PrivateKeyboard ((keybuf[i] & 0x80)? SDL_RELEASED : SDL_PRESSED, &keysym);
    }
}

static void iPod_PumpEvents (_THIS) 
{
    fd_set fdset;
    int max_fd = 0;
    static struct timeval zero;
    int posted;

    do {
	posted = 0;

	FD_ZERO (&fdset);
	if (kbfd >= 0) {
	    FD_SET (kbfd, &fdset);
	    max_fd = kbfd;
	}
	if (select (max_fd + 1, &fdset, 0, 0, &zero) > 0) {
	    iPod_keyboard();
	    posted++;
	}
    } while (posted);
}

// enough space for 160x128x2
static char ipod_scr[160 * (128/4)];

/*** The following LCD code is taken from Linux kernel uclinux-2.4.24-uc0-ipod2,
     file arch/armnommu/mach-ipod/fb.c. A few modifications have been made. ***/

/* get current usec counter */
static int M_timer_get_current(void)
{
	return inl(lcd_rtc);
}

/* check if number of useconds has past */
static int M_timer_check(int clock_start, int usecs)
{
	unsigned long clock;
	clock = inl(lcd_rtc);
	
	if ( (clock - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

/* wait for LCD with timeout */
static void M_lcd_wait_write(void)
{
    int start = M_timer_get_current();
    
    do {
        if ( (inl(lcd_base) & (unsigned int)0x8000) == 0 ) 
            break;
    } while ( M_timer_check(start, 1000) == 0 );
}


/* send LCD data */
static void M_lcd_send_data(int data_lo, int data_hi)
{
	M_lcd_wait_write();
	if (generation >= 0x70000) {
            outl ((inl (0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
            outl (data_hi | (data_lo << 8) | 0x760000, 0x70003008);
        } else {
            outl(data_lo, lcd_base + LCD_DATA);
            M_lcd_wait_write();
            outl(data_hi, lcd_base + LCD_DATA);
        }
}

/* send LCD command */
static void
M_lcd_prepare_cmd(int cmd)
{
	M_lcd_wait_write();
        if (generation > 0x70000) {
            outl ((inl (0x70003000) & ~0x1f00000) | 0x1700000, 0x70003000);
            outl (cmd | 0x74000, 0x70003008);
        } else {
            outl(0x0, lcd_base + LCD_CMD);
            M_lcd_wait_write();
            outl(cmd, lcd_base + LCD_CMD);
        }
}

/* send LCD command and data */
static void M_lcd_cmd_and_data(int cmd, int data_lo, int data_hi)
{
	M_lcd_prepare_cmd(cmd);

	M_lcd_send_data(data_lo, data_hi);
}

// Copied from uW
static void M_update_display(int sx, int sy, int mx, int my)
{
	int y;
	int cursor_pos;
        int linelen = (lcd_width + 3) / 4;

	sx >>= 3;
	mx >>= 3;

	cursor_pos = sx + (sy << 5);

	for ( y = sy; y <= my; y++ ) {
		int x;
                unsigned char *img_data;

		/* move the cursor */
		M_lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);

		/* setup for printing */
		M_lcd_prepare_cmd(0x12);

                img_data = ipod_scr + (sx << 1) + (y * linelen);

		/* loops up to 20 times */
		for ( x = sx; x <= mx; x++ ) {
		        /* display eight pixels */
			M_lcd_send_data(*(img_data + 1), *img_data);

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

static void C_lcd_cmd_data(int cmd, int data)
{
    if (lcd_type == 0) {
        C_lcd_send_lo (cmd);
        C_lcd_send_lo (data);
    } else {
        C_lcd_send_lo (0);
        C_lcd_send_lo (cmd);
        C_lcd_send_hi ((data >> 8) & 0xff);
        C_lcd_send_hi (data & 0xff);
    }
}

static void C_update_display(int sx, int sy, int mx, int my)
{
	int height = (my - sy) + 1;
	int width = (mx - sx) + 1;
        int rect1, rect2, rect3, rect4;

	unsigned short *addr = (unsigned short *)SDL_VideoSurface->pixels;

	if (width & 1) width++;

	/* calculate the drawing region */
	if ((generation >> 16) != 0x6) {
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
		C_lcd_cmd_data(0x12, rect1 & 0xff);
		C_lcd_cmd_data(0x13, rect2 & 0xff);
		C_lcd_cmd_data(0x15, rect3 & 0xff);
		C_lcd_cmd_data(0x16, rect4 & 0xff);
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
		C_lcd_cmd_data(0x44, (rect3 << 8) | rect1);
		/* max vert << 8 | start vert */
		C_lcd_cmd_data(0x45, (rect4 << 8) | rect2);

		if ((generation >> 16) == 0x6) {
			rect2 = rect4;
		}

		/* position cursor (set AD0-AD15) */
		/* start vert << 8 | start horiz */
		C_lcd_cmd_data(0x21, (rect2 << 8) | rect1);

		/* start drawing */
		C_lcd_send_lo(0x0);
		C_lcd_send_lo(0x22);
	}

        addr += sy * lcd_width + sx;
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

				if (lcd_type != 0) {
					unsigned t = (two_pixels & ~0xFF0000) >> 8;
					two_pixels = two_pixels & ~0xFF00;
					two_pixels = t | (two_pixels << 8);
				}

				/* output 2 pixels */
				outl(two_pixels, 0x70008B00);
			}

			addr += lcd_width - width;
		}

		while ((inl(0x70008A20) & 0x4000000) == 0);

		outl(0x0, 0x70008A24);

		height = height - h;
	}
}

// Should work with photo. However, I don't have one, so I'm not sure.
static void iPod_UpdateRects (_THIS, int nrects, SDL_Rect *rects) 
{
    if (SDL_VideoSurface->format->BitsPerPixel == 16) {
	C_update_display (0, 0, lcd_width - 1, lcd_height - 1);
    } else {
#ifndef IPOD
#error Cant get here from there
#endif
	// Only works on LITTLE-ENDIAN! (which iPod is, of course)
	int i;
	Uint32 *p;
	Uint8 *q;
	p = SDL_VideoSurface->pixels;
	q = ipod_scr;

	for (i = 0; i < ((lcd_width * lcd_height + 3) & ~3) >> 2; i++, p++, q++) {
	    // Call the pixels AA, BB, CC, DD (#1, #2, #3, #4).
	    // (Uint8[4])p looks like
	    // 000000AA 000000BB 000000CC 000000DD
	    // *p looks like
	    // 000000DD 000000CC 000000BB 000000AA (remember, little-endian)
	    // We want *q like
	    // DDCCBBAA                                 |              |
	    // so we OR the following four values:      v end of word, v stuff cut off
	    // p:       ......DD......CC......BB......AA|
	    // p >> 6:  >>>>>>......DD......CC......BB..|....AA
	    // p >> 12: >>>>>>>>>>>>......DD......CC....|..BB......AA
	    // p >> 18: >>>>>>>>>>>>>>>>>>......DD......|CC......BB......AA
	    // AND 0xff:                       |^^^^^^^^|
	    // --> DDCCBBAA --> what we want

	    *q = (*p | (*p >> 6) | (*p >> 12) | (*p >> 18)) & 0xff;
	}
	
	M_update_display (0, 0, lcd_width - 1, lcd_height - 1);
    }
}
