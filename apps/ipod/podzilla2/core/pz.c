/*
 * Copyright (c) 2005 Joshua Oreman, Bernard Leach, and Courtney Cavin
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h> /*d*/
#include "pz.h"
#ifdef IPOD
#include "ucdl.h"
#endif
#define _GNU_SOURCE
#include <getopt.h>

const char *PZ_Developers[] = {
    "Bernard Leach",
    "Joshua Oreman",
    "David Carne",
    "Courtney Cavin",
    "Scott Lawrence",
    "James Jacobsson",
    "Adam Johnston",
    "Alastair Stuart",
    "Rebecca Bettencourt",
    0
};

FILE *errout;

void ____Spurious_references_to_otherwise_unreferenced_symbols() 
{
    TWidget *(*tnivw)(int,int,ttk_surface) = ttk_new_imgview_widget; (void) tnivw;
    /* Add anything else *in TTK only* that's unrefed and
     * needed by a module.
     */

#ifndef IPOD
    /* And now for the div funcs and such in libgcc...
     * rand() is so the compiler can't do constant
     * optimization.
     */
    unsigned long ul = 1337 + rand();
    unsigned long long ull = 42424242 + rand();
    long l = ul; long long ll = ull;
    float f = 3.14159 + (float)rand();
    double d = 2.718281828 + (float)rand();

#define DO_STUFF_INT(x) { x += 5; x *= 27; x %= 420; x /= 42; x <<= 4; x -= 3; x >>= 20; }
#define DO_STUFF_FP(x) { x += 5.0; x *= 27.0; x /= 42.0; x -= 3.0; }
    DO_STUFF_INT(ul); DO_STUFF_INT(ull);
    DO_STUFF_INT(l);  DO_STUFF_INT(ll);
    DO_STUFF_FP(f);   DO_STUFF_FP(d);
#endif
}

/* compat globals */
t_GR_SCREEN_INFO screen_info;
long hw_version;
ttk_gc pz_root_gc;

/* static stuff */
static ttk_timer connection_timer = 0;
static int bl_forced_on = 0;

/* Is something connected? */
int usb_connected = 0;
int fw_connected = 0;

/* Set to tell ipod.c not to actually set the wheel debounce,
 * since we don't want it set *while we're setting it*.
 */
int pz_setting_debounce = 0;

/* Is hold on? */
int pz_hold_is_on = 0;

/* The global config. */
PzConfig *pz_global_config;

static void check_connection() 
{
    int this_usb_conn = pz_ipod_usb_is_connected();
    int this_fw_conn  = pz_ipod_fw_is_connected();
    int show_popup    = pz_get_int_setting (pz_global_config, USB_FW_POPUP);

    if (show_popup && this_usb_conn && !usb_connected &&
        pz_dialog (_("USB Connect"), _("Go to disk mode?"), 2, 10, "No", "Yes"))
        pz_ipod_go_to_diskmode();

    if( pz_ipod_get_hw_version() < 0x000B0000 ) {
        if (show_popup && this_fw_conn && !fw_connected &&
            pz_dialog (_("FireWire Connect"), _("Go to disk mode?"), 2, 10, "No", "Yes"))
            pz_ipod_go_to_diskmode();
    }

    usb_connected = this_usb_conn;
    fw_connected = this_fw_conn;
    connection_timer = ttk_create_timer (1000, check_connection);
}

int usb_fw_connected()
{
    return !!(usb_connected + fw_connected);
}

static ttk_timer bloff_timer = 0;

static void backlight_off() { if (!bl_forced_on) pz_ipod_set (BACKLIGHT, 0); bloff_timer = 0; }
static void backlight_on()  { pz_ipod_set (BACKLIGHT, 1); }

void pz_set_backlight_timer (int sec) 
{
    static int last = PZ_BL_OFF;
    if (sec != PZ_BL_RESET) last = sec;

    if (last == PZ_BL_OFF) {
	if (bloff_timer) ttk_destroy_timer (bloff_timer);
	bloff_timer = 0;
	backlight_off();
    } else if (last == PZ_BL_ON) {
	if (bloff_timer) ttk_destroy_timer (bloff_timer);
	bloff_timer = 0;
	backlight_on();
    } else {
	if (bloff_timer) ttk_destroy_timer (bloff_timer);
	bloff_timer = ttk_create_timer (1000*last, backlight_off);
	backlight_on();
    }
}

void backlight_toggle() 
{
    bl_forced_on = !bl_forced_on;
    pz_handled_hold ('m');
}

static int held_times[128] = { ['m'] = 500 }; // key => ms
static void (*held_handlers[128])() = { ['m'] = backlight_toggle };
static int (*unused_handlers[128])(int, int);

static int held_ignores[128]; // set a char to 1 = ignore its UP event once.
static ttk_timer held_timers[128]; // the timers

void pz_register_global_hold_button (unsigned char ch, int ms, void (*handler)()) 
{
    held_times[ch] = ms;
    held_handlers[ch] = handler;
}
void pz_unregister_global_hold_button (unsigned char ch) 
{
    held_times[ch] = 0; held_handlers[ch] = 0;
}

void pz_register_global_unused_handler (unsigned char ch, int (*handler)(int, int)) 
{
    unused_handlers[ch] = handler;
}
void pz_unregister_global_unused_handler (unsigned char ch) 
{
    unused_handlers[ch] = 0;
}

void pz_handled_hold (unsigned char ch)
{
    held_timers[ch] = 0;
    held_ignores[ch]++;
}

int pz_event_handler (int ev, int earg, int time)
{
    static int vtswitched = 0;
    int reset_backlight = 1;
    int retval = 0;

    /* unset setting_debounce if we're not anymore */
    if (pz_setting_debounce && (ttk_windows->w->focus->draw != ttk_slider_draw)) {
	pz_setting_debounce = 0;
	pz_ipod_set (WHEEL_DEBOUNCE, pz_get_int_setting (pz_global_config, WHEEL_DEBOUNCE));
    }

    switch (ev) {
    case TTK_BUTTON_DOWN:
	switch (earg) {
	case TTK_BUTTON_HOLD:
	    pz_hold_is_on = 1;
	    pz_header_fix_hold();
	    reset_backlight = 0; // turning on hold does not trigger backlight
	    break;
	case TTK_BUTTON_MENU:
	    vtswitched = 0;
	    break;
	}
	if (held_times[earg] && held_handlers[earg]) {
	    if (held_timers[earg]) ttk_destroy_timer (held_timers[earg]);
	    held_timers[earg] = ttk_create_timer (held_times[earg], held_handlers[earg]);
	}
	break;
    case TTK_BUTTON_UP:
	if (held_timers[earg]) {
	    ttk_destroy_timer (held_timers[earg]);
	    held_timers[earg] = 0;
	}
	if (held_ignores[earg]) {
	    held_ignores[earg] = 0;
	    retval = 1;
	}

	switch (earg) {
	case TTK_BUTTON_HOLD:
	    pz_hold_is_on = 0;
	    pz_header_fix_hold();
	    break;
	case TTK_BUTTON_PREVIOUS:
	    if (pz_get_int_setting (pz_global_config, ENABLE_VTSWITCH) &&
		ttk_button_pressed (TTK_BUTTON_MENU) && ttk_button_pressed (TTK_BUTTON_PLAY)) {
		// vt switch code <<
		printf ("VT SWITCH <<\n");
		vtswitched = 1;
		retval = 1;
	    } else if (pz_get_int_setting (pz_global_config, ENABLE_VTSWITCH) &&
		       ttk_button_pressed (TTK_BUTTON_MENU) && ttk_button_pressed (TTK_BUTTON_NEXT)) {
		// vt switch code [0]
		printf ("VT SWITCH 0 (N-P)\n");
		vtswitched = 1;
		retval = 1;
	    } else if (pz_get_int_setting (pz_global_config, ENABLE_WINDOWMGMT) &&
                       ttk_button_pressed (TTK_BUTTON_MENU) && !vtswitched) {
		TWindowStack *lastwin = ttk_windows;
		while (lastwin->next) lastwin = lastwin->next;
		if (lastwin->w != ttk_windows->w) {
		    ttk_move_window (lastwin->w, 0, TTK_MOVE_ABS);
		    printf ("WINDOW CYCLE >>\n");
		} else
		    printf ("WINDOW CYCLE >> DIDN'T\n");
		retval = 1;
	    }
	    break;
	case TTK_BUTTON_NEXT:
	    if (pz_get_int_setting (pz_global_config, ENABLE_VTSWITCH) &&
		ttk_button_pressed (TTK_BUTTON_MENU) && ttk_button_pressed (TTK_BUTTON_PLAY)) {
		// vt switch code >>
		printf ("VT SWITCH >>\n");
		vtswitched = 1;
		retval = 1;
	    } else if (pz_get_int_setting (pz_global_config, ENABLE_VTSWITCH) &&
		       ttk_button_pressed (TTK_BUTTON_MENU) && ttk_button_pressed (TTK_BUTTON_PREVIOUS)) {
		// vt switch code [0]
		printf ("VT SWITCH 0 (P-N)\n");
		vtswitched = 1;
		retval = 1;

	    } else if (pz_get_int_setting (pz_global_config, ENABLE_WINDOWMGMT) &&
                       ttk_button_pressed (TTK_BUTTON_MENU) && !vtswitched) {
		printf ("WINDOW CYCLE <<\n");
		if (ttk_windows->next) {
		    ttk_move_window (ttk_windows->w, 0, TTK_MOVE_END);
		    retval = 1;
		}
	    }
	    break;
	case TTK_BUTTON_PLAY:
	    if (pz_get_int_setting (pz_global_config, ENABLE_WINDOWMGMT) &&
                ttk_button_pressed (TTK_BUTTON_MENU) && !vtswitched) {
		printf ("WINDOW MINIMIZE\n");
		if (ttk_windows->next) {
		    ttk_windows->minimized = 1;
		    retval = 1;
		}
	    }
	}
	break;
    }

    if( reset_backlight ) {
	pz_set_backlight_timer (PZ_BL_RESET);
    }
    pz_reset_idle_timer();

    return retval; // keep event if 0
}


int pz_unused_handler (int ev, int earg, int time) 
{
    switch (ev) {
    case TTK_BUTTON_UP:
	if (unused_handlers[earg])
	    return unused_handlers[earg](earg, time);
	break;
    }
    return 0;
}


void pz_set_time_from_file(void)
{
#ifdef IPOD
	struct timeval tv_s;
	struct stat statbuf;

	/* find the last modified time of the settings file */
	stat( "/etc/podzilla/podzilla.conf", &statbuf );

	/* convert timespec to timeval */
	tv_s.tv_sec  = statbuf.st_mtime;
	tv_s.tv_usec = 0;

	settimeofday( &tv_s, NULL );
#endif
}

void pz_touch_settings(void) 
{
#ifdef IPOD
	close (open ("/etc/podzilla/podzilla.conf", O_WRONLY));
#endif
}


void pz_uninit() 
{
	ttk_quit();
	pz_touch_settings();
	pz_modules_cleanup();
        fclose (errout);
}

#ifdef IPOD
void
decode_instr (FILE *f, unsigned long *iaddr)
{
    unsigned long instr = *iaddr;
    fprintf (f, "%8lx:\t%08lx\t", (unsigned long)iaddr, instr);
    if ((instr & 0xff000000) == 0xeb000000) {
        unsigned long field = instr & 0xffffff;
        fprintf (f, "bl\tf=%lx", field);
        unsigned long uoffs = field << 2;
        if (uoffs & (1 << 25)) uoffs |= 0xfc000000;
        long offs = uoffs;
        fprintf (f, "  o=%lx", offs);
        unsigned long addr = (unsigned long)iaddr + 8 + offs;
        fprintf (f, "  a=%08lx", addr);
        const char *modfile, *symname;
        symname = uCdl_resolve_addr (addr, &offs, &modfile);
        fprintf (f, " <%s+%lx> from %s\n", symname, offs, modfile);
    } else {
        fprintf (f, "???\n");
    }
}

void
debug_handler (int sig) 
{
    unsigned long PC, *FP;
    unsigned long retaddr, off;
    const char *modfile = "Unknown";
    int i;
    FILE *f = fopen ("podzilla.oops", "w");

    asm ("mov %0, r11" : "=r" (FP) : );

    /* arm_fp field of struct sigcontext is 0x3c bytes
     * above our FP. The regs are in order, so PC is
     * 0x10 bytes above FP.
     */
    PC = *(unsigned long  *)((char *)FP + 0x4c);
    FP = *(unsigned long **)((char *)FP + 0x3c);
    fprintf (f, "FP = 0x%lx\n", (unsigned long)FP);

    ttk_quit();
    fprintf (stderr, "Fatal signal %d\n", sig);
    fprintf (stderr, "Trying to gather debug info. If this freezes, reboot.\n\n");

    const char *func = uCdl_resolve_addr (PC, &off, &modfile);
    fprintf (f, "#0  %08lx <%s+%lx> from %s\n", PC, func, off, modfile);
    decode_instr (f, (unsigned long *)PC - 3);

    for (i = 1; i < 10; i++) {
        retaddr = *(FP - 1);
        fprintf (f, "#%d  %08lx <%s+%lx> from %s\n", i, retaddr, uCdl_resolve_addr (retaddr, &off, &modfile), off, modfile);
        decode_instr (f, (unsigned long *)retaddr - 1);
        FP = (unsigned long *) *(FP - 3);
    }
    fclose (f);

    fprintf (stderr, "Saved: podzilla.oops\n");
    pz_touch_settings();
    pz_modules_cleanup();
    fprintf (stderr, "Letting original sig go - expect crash\n");
    signal (sig, SIG_DFL);
    return;
}
#endif

void
usage( char * exename )
{
	fprintf( stderr, "Usage: %s [options...]\n", exename );
	fprintf( stderr, "Options:\n" );
	fprintf( stderr, "  -g <gen>  set simulated ipod generation.\n" );
	fprintf( stderr, "            <gen> can be one of:\n" );
	fprintf( stderr, "                1g, 2g, 3g, 4g, 5g, 6g, scroll,\n" );
	fprintf( stderr, "                scroll, dock, mini,\n" );
	fprintf( stderr, "                nano, nano1g, nano2g, nano3g,\n" );
	fprintf( stderr, "                photo, color, video,\n" );
	fprintf( stderr, "                classic, touch\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "  -2 WxH     \tuse a screen W by H, 2bpp (monochrome)\n" );
	fprintf( stderr, "  -mono WxH  \tuse a screen W by H, 2bpp (monochrome)\n" );
	fprintf( stderr, "  -16 WxH    \tuse a screen W by H, 16bpp (color)\n" );
	fprintf( stderr, "  -color WxH \tuse a screen W by H, 16bpp (color)\n" );
	fprintf( stderr, "  -rotate    \tRotate the screen 90 degrees\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "  -l <path> set module loading path.\n" ); 
	fprintf( stderr, "            <path> should be a colon seperated list\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "  -errout <file> redirect all errors to <file>\n");
	fprintf( stderr, "                   (not fully implemented)\n");
	fprintf( stderr, "\n" );

	fprintf( stderr, "default resolution and color are for color/photo iPod\n" );

	exit(2);
}

static const struct {
	const char *name;
	int width, height, bpp;
} generations[] = {
	{"1g",      160, 128,  2},
	{"2g",      160, 128,  2},
	{"3g",      160, 128,  2},
	{"4g",      160, 128,  2},
	{"5g",      320, 240, 16},
	{"6g",      320, 240, 16},
	{"scroll",  160, 128,  2},
	{"dock",    160, 128,  2},
	{"mini",    138, 110,  2},
	{"photo",   220, 176, 16},
	{"color",   220, 176, 16},
	{"video",   320, 240, 16},
	{"classic", 320, 240, 16},
	{"nano",    176, 132, 16},
	{"nano1g",  176, 132, 16},
	{"nano2g",  320, 240, 16},
	{"nano3g",  320, 240, 16},
	{"touch",   320, 480, 16},
	{0, 0, 0, 0}
};

int
main(int argc, char **argv)
{
	TWindow *first;
	char *modulepath = NULL;
	int width = 220, height = 176, bpp = 16, rotate = 0;
	int initialContrast = ipod_get_contrast();
	if( initialContrast < 1 ) initialContrast = 96;

        if (!TTK_VERSION_CHECK()) {
        	fprintf (stderr, "Version mismatch; exiting.\n");
        	return 1;
        }

#ifdef IPOD
        signal (SIGBUS, debug_handler);

#define SCHEMESDIR "/usr/share/schemes/"
#else
#define SCHEMESDIR "schemes/"
#endif

        errout = stderr;

	for (;;) {
		int c;
		int oindex = 0;
		static struct option long_options[] = {
			{"mono",   1, 0, 'm'},
			{"color",  1, 0, 'c'},
			{"16",     1, 0, 'c'},
			{"errout", 0, 0, 'e'},
			{"gen",    1, 0, 'g'},
			{"rotate", 0, 0, 'r'}
		};

		c = getopt_long_only(argc, argv, "l:g:2:", long_options, &oindex);
		if (c == -1)
			break;
		switch (c) {
		case 'g':
			for (c = 0; generations[c].name != 0; ++c) {
				if (!strcmp(generations[c].name, optarg)) {
					width = generations[c].width;
					height = generations[c].height;
					bpp = generations[c].bpp;
					break;
				}
			}
			break;
		case '2':
		case 'm':
			bpp = 2;
			if (sscanf(optarg, "%dx%d", &width, &height) != 2)
				usage(argv[0]);
			break;
		case 'c':
			bpp = 16;
			if (sscanf(optarg, "%dx%d", &width, &height) != 2)
				usage(argv[0]);
			break;
		case 'l':
			modulepath = optarg;
			break;
		case 'e':
			if (!(errout = fopen(optarg, "a"))) {
				perror(optarg);
				exit(3);
			}
			break;
		case 'r':
			rotate = 1;
			break;
		case '?':
		default:
			usage(argv[0]);
			break;
		}
	}

#ifndef IPOD
	if( rotate ) {
		width ^= height;
		height ^= width;
		width ^= height;
	}
	ttk_set_emulation(width, height, bpp);
#endif

#ifdef IPOD
#define CONFIG_FILE "/etc/podzilla/podzilla.conf"
#else
#define CONFIG_FILE "config/podzilla.conf"
#endif

	pz_global_config = pz_load_config (CONFIG_FILE);

	/* backwards compat with old symlink method */
	if (access(SCHEMESDIR "default.cs", R_OK) == 0) {
		char ltarget[256];
		if (readlink(SCHEMESDIR "default.cs", ltarget, 256) < 0)
			perror("default.cs broken");
		else
			pz_set_string_setting(pz_global_config,
					COLORSCHEME, ltarget);
		unlink(SCHEMESDIR "default.cs");
	}

	if ((first = ttk_init()) == 0) {
		fprintf(stderr, _("ttk_init failed\n"));
		exit(1);
	}
	ttk_hide_window (first);
	atexit (pz_uninit);

#ifdef IPOD
        char exepath[256];
        if (argv[0][0] == '/')
            strcpy (exepath, argv[0]);
        else
            sprintf (exepath, "/bin/%s", argv[0]);

	if (uCdl_init (exepath) == 0) {
		ttk_quit();
		fprintf (stderr, _("uCdl_init failed: %s\n"), uCdl_error());
		exit (0);
	}
#endif

	ttk_set_global_event_handler (pz_event_handler);
	ttk_set_global_unused_handler (pz_unused_handler);

#ifdef LOCALE
	setlocale(LC_ALL, "");
	bindtextdomain("podzilla", LOCALEDIR);
	textdomain("podzilla");
#endif

	pz_root_gc = ttk_new_gc();
	ttk_gc_set_usebg(pz_root_gc, 0);
	ttk_gc_set_foreground(pz_root_gc, ttk_makecol (0, 0, 0));
	t_GrGetScreenInfo(&screen_info);

	hw_version = pz_ipod_get_hw_version();

	if( hw_version && hw_version < 0x30000 ) { /* 1g/2g only */
		pz_set_time_from_file();
	}

	/* Set some sensible defaults */
#define SET(x) pz_get_setting(pz_global_config,x)
	if (!SET(WHEEL_DEBOUNCE))   pz_ipod_set (WHEEL_DEBOUNCE, 10);
	if (!SET(CONTRAST))         pz_ipod_set (CONTRAST, initialContrast);
	if (!SET(CLICKER))          pz_ipod_set (CLICKER, 1);
	if (!SET(DSPFREQUENCY))     pz_ipod_set (DSPFREQUENCY, 0);
	if (!SET(SLIDE_TRANSIT))    pz_ipod_set (SLIDE_TRANSIT, 2);
	if (!SET(BACKLIGHT))        pz_ipod_set (BACKLIGHT, 1);
	if (!SET(BACKLIGHT_TIMER))  pz_ipod_set (BACKLIGHT_TIMER, 3);
	if (!SET(COLORSCHEME))
		pz_set_string_setting (pz_global_config,COLORSCHEME, "mono.cs");
	pz_save_config (pz_global_config);
	pz_ipod_fix_settings (pz_global_config);


	char colorscheme[256];
	snprintf(colorscheme, 256, SCHEMESDIR "%s",
			pz_get_string_setting(pz_global_config, COLORSCHEME));
	ttk_ap_load(colorscheme);

	/* load some fonts */
	/* NOTE: we should probably do something if these fail! */
	pz_load_font (&ttk_textfont, "Espy Sans", TEXT_FONT, pz_global_config);
	pz_load_font (&ttk_menufont, "Chicago",   MENU_FONT, pz_global_config);

	/* set up the menus and initialize the modules */
	pz_menu_init();
	pz_modules_init(modulepath);
	pz_header_init();

	/* sort the menus */
	pz_menu_sort ("/Extras/Demos");
	pz_menu_sort ("/Extras/Games");
	pz_menu_sort ("/Extras/Utilities");
	//pz_menu_sort ("/Extras/Applications");
	if( pz_get_int_setting( pz_global_config, GROUPED_MENUS )) {
		pz_menu_sort ("/Extras");
		pz_menu_sort ("/");
	}


	/* finish up GUI stuff */
	pz_menuconf_init();
	ttk_show_window (pz_menu_get());

	/* Start timers and flags */
	connection_timer = ttk_create_timer (1000, check_connection);
	usb_connected = pz_ipod_usb_is_connected();
	fw_connected = pz_ipod_fw_is_connected();

	return ttk_run();
}
