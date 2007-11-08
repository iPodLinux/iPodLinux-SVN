/*
 * Copyright (c) 2006 Courtney Cavin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "pz.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static PzModule *module;
#define inl(a) (*(volatile unsigned long *) (a))
#define outl(a,b) (*(volatile unsigned long *) (b) = (a))

#define CLOCK_POLICY   0x60006020
#define CLOCK_SCALER  0x60006034
#define CPU_CONTROL	0x60007004
#define PLL_CONTROL   0x70000020

#define KEYPAD 0x7000c104
#define LCD_CONTROL 0x70003000
#define CLOCK_CONTROL 0x6000600c

static void ipod_goto_sleep(void)
{
#ifndef IPOD
	puts("Putting your computer to sleep...  Just kidding.");
#else
	long hw_ver = pz_ipod_get_hw_version() >> 16;
	if (hw_ver < 0x4 || hw_ver == 0x5 || hw_ver == 0x6)
		return;
	int kbfd = open("/dev/misc/wheel", O_RDONLY | O_NONBLOCK);
	unsigned long control = inl(CPU_CONTROL);
	unsigned long pll_power = inl(PLL_CONTROL);
	unsigned long clock_scaler = inl(CLOCK_SCALER);
	unsigned long clock_policy = inl(CLOCK_POLICY);
	unsigned long lcd_control = inl(LCD_CONTROL);
	unsigned long clock_control = inl(CLOCK_CONTROL);
	outl(0x0, CPU_CONTROL);
	outl(pll_power | (1 << 30), PLL_CONTROL);
	outl(0xf0000000, CLOCK_POLICY);
	/* 32kHz = (32kHz / 1) * 1 */
	outl(0xaa000000, CLOCK_SCALER);
	if (hw_ver == 0x4 || hw_ver == 0x7)
		outl(0x100 << 3, 0x6000d824);
	} else if (hw_ver == 0xb || hw_ver == 0xc) {
		outl(0x100 << 3, 0x6000d824);
		outl(0x100 << 7, 0x6000d12c);
	}

	unsigned short ev;
	while (read(kbfd, &ev, 2) < 2);

	if (hw_ver == 0x04 || hw_ver == 0x7)
		outl(0x101 << 3, 0x6000d824);
	} else if (hw_ver == 0xb || hw_ver == 0xc) {
		outl(0x101 << 3, 0x6000d824);
		outl(0x101 << 7, 0x6000d12c);
	}

	outl(control, CPU_CONTROL);
	outl(pll_power, PLL_CONTROL);
	outl(clock_policy, CLOCK_POLICY);
	outl(clock_scaler, CLOCK_SCALER);
	close(kbfd);
#endif
}


static char mpdc_is_playing(void)
{
	char (* local_mpdc_is_playing)(void);
	local_mpdc_is_playing = pz_module_softdep("mpdc", "mpdc_is_playing");
	return (local_mpdc_is_playing && local_mpdc_is_playing());
}


static int idle_handler(void *d)
{
	PzWindow *(* local_mpd_currently_playing)(void);
	char (* local_mcp_active)(void);


	if (mpdc_is_playing()) {
		local_mcp_active = pz_module_softdep("mpdc", "mcp_active");
		if (!local_mcp_active || local_mcp_active())
			return 0;
		if ((local_mpd_currently_playing = pz_module_softdep("mpdc",
				"mpd_currently_playing")))
			ttk_show_window(local_mpd_currently_playing());
	}
	else { 
		ipod_goto_sleep();	
	}
	return 0;
}

static void stop_mpdc(void)
{
	void (* local_mpdc_playpause)(void);

	if (mpdc_is_playing()) {	
		local_mpdc_playpause = pz_module_softdep("mpdc", 
							"mpdc_playpause");

		/* pause the music if we're currently playing */
		if (local_mpdc_playpause)
			local_mpdc_playpause();
	}
}



static void sleep_forced(void)
{
	/* kill the timer + handle the button first
	 * because we'll be out for a while */
	pz_handled_hold (PZ_BUTTON_PLAY); 

	/* take down mpd if its currently playing */
	stop_mpdc();

	/* sleep it! */
	ipod_goto_sleep();

}
static void init_sleep()
{

	module = pz_register_module("sleep", NULL);

	/* register us as an idle handler */
	pz_register_idle(idle_handler, NULL);

	/* register the play button as the sleep key */
	pz_register_global_hold_button(PZ_BUTTON_PLAY, 5000, sleep_forced);
}

PZ_MOD_INIT(init_sleep)
