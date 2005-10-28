/*
 * Copyright (C) 2004 Bernard Leach
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

#include <fcntl.h>
#ifdef IPOD
#include <linux/fb.h>
#endif
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "pz.h"

#define FBIOGET_CONTRAST	_IOR('F', 0x22, int)
#define FBIOSET_CONTRAST	_IOW('F', 0x23, int)

#define FBIOGET_BACKLIGHT	_IOR('F', 0x24, int)
#define FBIOSET_BACKLIGHT	_IOW('F', 0x25, int)

#define FB_DEV_NAME		"/dev/fb0"
#define FB_DEVFS_NAME		"/dev/fb/0"

#define inb(a) (*(volatile unsigned char *) (a))
#define outb(a,b) (*(volatile unsigned char *) (b) = (a))

#define inl(a) (*(volatile unsigned long *) (a))
#define outl(a,b) (*(volatile unsigned long *) (b) = (a))

extern int pz_setting_debounce;

static int ipod_ioctl(int request, int *arg)
{
#ifdef IPOD
	int fd;

	fd = open(FB_DEV_NAME, O_NONBLOCK);
	if (fd < 0) fd = open(FB_DEVFS_NAME, O_NONBLOCK);
	if (fd < 0) {
		return -1;
	}
	if (ioctl(fd, request, arg) < 0) {
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
#else
	return -1;
#endif
}

static int ipod_constrain( int min, int max, int val )
{
	if( val > max ) val = max;
	if( val < min ) val = min;
	return( val );
}

static int ipod_get_contrast(void)
{
	int contrast;

	if (ipod_ioctl(FBIOGET_CONTRAST, &contrast) < 0) {
		return -1;
	}

	return contrast;
}

static int ipod_set_contrast(int contrast)
{
	contrast = ipod_constrain( 0, 128, contrast ); /* just in case */
	if (ipod_ioctl(FBIOSET_CONTRAST, (int *)(long)contrast) < 0) {
		return -1;
	}

	return 0;
}

static int ipod_get_backlight(void)
{
	int backlight;

	if (ipod_ioctl(FBIOGET_BACKLIGHT, &backlight) < 0) {
		return -1;
	}

	return backlight;
}

static int ipod_set_backlight(int backlight)
{
	if (ipod_ioctl(FBIOSET_BACKLIGHT, (int *)(long)backlight) < 0) {
		return -1;
	}

	return 0;
}

static int ipod_set_backlight_timer(int timer)
{
	int times[] = {-2, 1, 2, 5, 10, 30, 60, 0};
	pz_set_backlight_timer (times[timer]);
	pz_ipod_set (BACKLIGHT, timer ? 1 : 0);
	return 0;
}

static void ipod_set_wheelspeed (int value) 
{
    int   num[] = {1, 1, 1, 1, 2, 1, 3, 2, 3, 4, 1, 3, 4, 2, 5, 3, 7, 4, 9, 5};
    int denom[] = {6, 5, 4, 3, 5, 2, 5, 3, 4, 5, 1, 2, 3, 1, 2, 1, 2, 1, 2, 1};

    ttk_set_scroll_multiplier (num[value], denom[value]);
}

static void fix_setting (int setting, int value)
{
	if (value <= 0) {
		value = 0;
	}

	switch (setting) {
	case CONTRAST:
		ipod_set_contrast(value);
		break;	
	case BACKLIGHT:
		ipod_set_backlight(value);
		break;
	case BACKLIGHT_TIMER:
		ipod_set_backlight_timer(value);
		break;
	case DECORATIONS:
		ttk_dirty |= TTK_DIRTY_HEADER;
		//		appearance_set_decorations(value);
		break;
	case DISPLAY_LOAD:
		ttk_dirty |= TTK_DIRTY_HEADER;
		pz_header_init();
		break;
	case CLICKER:
		if (value)
			ttk_set_clicker (ttk_click);
		else
			ttk_set_clicker (0);
		break;
	case WHEEL_DEBOUNCE:
		if (!pz_setting_debounce)
			ipod_set_wheelspeed (value);
		break;
	case SLIDE_TRANSIT:
		switch (value) {
		case 0:
			ttk_set_transition_frames (1);
			break;
		case 1:
			ttk_set_transition_frames (16);
			break;
		case 2:
			ttk_set_transition_frames (8);
			break;
		}
		break;
	}
}

void pz_ipod_set (int sid, int value) 
{
    fix_setting (sid, value);
    //	set_int_setting(setting, value);
}

/*
 * 0: screen on
 * 1,2: screen off
 * 3: screen power down
 */
int ipod_set_blank_mode(int blank)
{
#ifdef IPOD
	if (ipod_ioctl(FBIOBLANK, (int *) blank) < 0) {
		return -1;
	}

#endif
	return 0;
}

void ipod_beep(void)
{
#ifdef IPOD
	if (hw_version >= 40000) {
		int i, j;
		outl(inl(0x70000010) & ~0xc, 0x70000010);
		outl(inl(0x6000600c) | 0x20000, 0x6000600c);    /* enable device */
		for (j = 0; j < 10; j++) {
			for (i = 0; i < 0x888; i++ ) {
				outl(0x80000000 | 0x800000 | i, 0x7000a000); /* set pitch */
			}
		}
		outl(0x0, 0x7000a000);    /* piezo off */
	} else {
		static int fd = -1; 
		static char buf;

		if (fd == -1 && (fd = open("/dev/ttyS1", O_WRONLY)) == -1
				&& (fd = open("/dev/tts/1", O_WRONLY)) == -1) {
			return;
		}
    	
		write(fd, &buf, 1);
	}
#else
	if (isatty(1)) {
		printf("\a");
	}
#endif
}

static int ipod_read_apm(int *battery, int *charging)
{
#ifdef IPOD
	FILE *file;
	int ac_line_status = 0xff;
	int battery_status = 0xff;
	int battery_flag = 0xff;
	int percentage = -1;
	int time_units = -1;

	if ((file = fopen("/proc/apm", "r")) != NULL) {
		fscanf(file, "%*s %*d.%*d 0x%*02x 0x%02x 0x%02x 0x%02x %d%% %d", &ac_line_status, &battery_status, &battery_flag, &percentage, &time_units);
		fclose(file);

		if (battery) {
			*battery = time_units;
		}
		if (charging) {
			*charging = (battery_status != 0xff && battery_status & 0x3) ? 1 : 0;
		}

		return 0;
	}
	if (battery) *battery = 512;
	if (charging) *charging = 0;
#else
	int t = time (0);
	int u = t/2, v = u/4;
	if (battery) {
	    if ((u % 4) == 0) {
		*battery = 512;
	    } else {
		srand ((unsigned)time (0));
		*battery = rand() % 512;
	    }
	}
	if (charging) {
	    if ((v % 2) == 0) {
		*charging = 1;
	    } else {
		*charging = 0;
	    }
	}
#endif
	return 0;
}

int pz_ipod_get_battery_level(void)
{
	int battery;

	ipod_read_apm(&battery, 0);
	return battery;
}

int pz_ipod_is_charging(void)
{
	int charging;

	ipod_read_apm(0, &charging);
	return charging;
}

long pz_ipod_get_hw_version(void)
{
#ifdef IPOD
	int i;
	char cpuinfo[512];
	char *ptr;
	FILE *file;
	static int hw_ver = -1;

	if (hw_ver != -1)
	    return hw_ver;

	if ((file = fopen("/proc/cpuinfo", "r")) != NULL) {
		while (fgets(cpuinfo, sizeof(cpuinfo), file) != NULL)
			if (strncmp(cpuinfo, "Revision", 8) == 0)
				break;
		fclose(file);
	} else {
		return 0;
	}
	for (i = 0; !isspace(cpuinfo[i]); i++);
	for (; isspace(cpuinfo[i]); i++);
	ptr = cpuinfo + i + 2;

	return (hw_ver = strtol(ptr, NULL, 16));
#else
	return 0;
#endif
}

void pz_ipod_go_to_diskmode()
{
#ifdef IPOD
	unsigned char * storage_ptr = (unsigned char *)0x40017F00;
	char * diskmode = "diskmode\0";
	char * hotstuff = "hotstuff\0";

	if (pz_ipod_get_hw_version() < 0x40000) {
	    pz_ipod_reboot();
	    return;
	}

	memcpy(storage_ptr, diskmode, 9);
	storage_ptr = (unsigned char *)0x40017f08;
	memcpy(storage_ptr, hotstuff, 9);
	outl(1, 0x40017F10);	
	outl(inl(0x60006004) | 0x4, 0x60006004);
	sleep (1);
	pz_ipod_reboot();
#endif
}

void pz_ipod_powerdown(void)
{
#ifdef IPOD
    pz_uninit();
    printf("\nPowering down.\nPress action to power on.\n");
    execl("/bin/poweroff", "poweroff", NULL);
    
    printf("No poweroff binary available.  Rebooting.\n");
    execl("/bin/reboot", "reboot", NULL);
    exit(0);
#else
    pz_error ("I don't think you want to reboot your desktop...");
#endif
}

void pz_ipod_reboot(void)
{
#ifdef IPOD
    pz_uninit();
    execl("/bin/reboot", "reboot", NULL);
    exit(0);
#else
    pz_warning ("I don't think you want me to reboot your desktop...");
#endif
}

#ifdef IPOD
static int usb_check_connection()
{
	if ((inl(0xc50001A4) & 0x800)==0)
		return 0;
	else
		return 1;  // we're connected - yay!
}


static int usb_get_usb2d_ident_reg()
{
	return inl(0xc5000000);
}

static int setup_usb(int arg0)
{
	int r0, r1;
	static int usb_inited = 0;

	if (!usb_inited)
	{
		outl(inl(0x70000084) | 0x200, 0x70000084);
		if (arg0==0)
		{
			outl(inl(0x70000080) & ~0x2000, 0x70000080);
			return 0;
		} 
	
		outl(inl(0x7000002C) | 0x3000000, 0x7000002C);
		outl(inl(0x6000600C) | 0x400000, 0x6000600C);
	
		outl(inl(0x60006004) | 0x400000, 0x60006004);   // reset usb start
		outl(inl(0x60006004) & ~0x400000, 0x60006004);  // reset usb end

		outl(inl(0x70000020) | 0x80000000, 0x70000020);

		while ((inl(0x70000028) & 0x80) == 0);
	
		outl(inl(0xc5000184) | 0x100, 0xc5000184);


		while ((inl(0xc5000184) & 0x100) != 0);


		outl(inl(0xc50001A4) | 0x5F000000, 0xc50001A4);

		if ((inl(0xc50001A4) & 0x100) == 0)
		{
			outl(inl(0xc50001A8) & ~0x3, 0xc50001A8);
			outl(inl(0xc50001A8) | 0x2, 0xc50001A8);
 			outl(inl(0x70000028) | 0x4000, 0x70000028);
			outl(inl(0x70000028) | 0x2, 0x70000028);
		} else {

			outl(inl(0xc50001A8) | 0x3, 0xc50001A8);
			outl(inl(0x70000028) &~0x4000, 0x70000028);
			outl(inl(0x70000028) | 0x2, 0x70000028);
		}
		outl(inl(0xc5000140) | 0x2, 0xc5000140);
		while((inl(0xc5000140) & 0x2) != 0);
		r0 = inl(0xc5000184);
	///////////////THIS NEEDS TO BE CHANGED ONCE THERE IS KERNEL USB
		outl(inl(0x70000020) | 0x80000000, 0x70000020);
		outl(inl(0x6000600C) | 0x400000, 0x6000600C);
		while ((inl(0x70000028) & 0x80) == 0);
		outl(inl(0x70000028) | 0x2, 0x70000028);
	
		//wait_usec(0x186A0);
		usleep(0x186A0);	
	/////////////////////////////////////////////////////////////////
		r0 = r0 << 4;
		r1 = r0 >> 30;
		usb_inited = 1;	
		return r1; // if r1>2 or r1 < 0 then it returns crap - not sure what
	} else { 
		return 0;
	}
}



static int usb_test_core(int arg0)
{
	int r0;
	setup_usb(1);
	r0 = usb_get_usb2d_ident_reg();
	if (r0!=arg0)
	{
		///// USB2D_IDENT iS BAD
		return 0;
	} else {

		if (!usb_check_connection())
		{
			/// we're not connected to anything
			return 0;	
		} else {
		
			return 1;	
			// display id from r0
		}	
	}
}
#endif


int pz_ipod_usb_is_connected(void)
{
#ifdef IPOD
	if (pz_ipod_get_hw_version() >= 0x40000)
	{	
		static int r0;
		r0 = usb_test_core(0x22fA05);
		return r0;
	} 
#endif
	return 0;
}


int pz_ipod_fw_is_connected(void)
{
#ifdef IPOD
	if (pz_ipod_get_hw_version() < 0x40000)
	{	
		if ((inl(0xcf000038) & (1<<4)))
			return 0;
		else
			return 1;  // we're connected - yay!
	} 
#endif
	return 0;
}
