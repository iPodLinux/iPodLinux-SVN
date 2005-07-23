/*
 * Copyright (C) 2005 Adam Johnston
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

#include <stdlib.h>
#ifdef IPOD
#include <string.h>
#include <unistd.h>
#endif

#include "pz.h"
#include "mlist.h"

#define outl(a, b) (*(volatile unsigned int *)(b) = (a))
#define inl(a) (*(volatile unsigned int *)(a))



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


int usb_is_connected(void)
{
#ifdef IPOD
	if (hw_version>=40000)
	{	
		static int r0;
		r0 = usb_test_core(0x22fA05);
		return r0;
	} 
#endif
	return 0;
}


static void goto_diskmode()
{
#ifdef IPOD
	unsigned char * storage_ptr = (unsigned char *)0x40017F00;
	char * diskmode = "diskmode\0";
	char * hotstuff = "hotstuff\0";

	memcpy(storage_ptr, diskmode, 9);
	storage_ptr = (unsigned char *)0x40017f08;
	memcpy(storage_ptr, hotstuff, 9);
	outl(1, 0x40017F10);	
	outl(inl(0x60006004) | 0x4, 0x60006004);
#endif
}


static GR_WINDOW_ID usb_wid;
static GR_GC_ID usb_gc;
static menu_st *usb_menu;
static int usb_ww, usb_wh, usb_mh;

static void usb_menu_quit()
{
	usb_menu = menu_destroy(usb_menu);
	pz_close_window(usb_wid);
	GrDestroyGC(usb_gc);
}


static int usb_menu_keystroke(GR_EVENT * event)
{
	int ret = 0;
	switch (event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch(event->keystroke.ch) {
		case '\r':	
		case '\n':
			usb_menu = menu_handle_item(usb_menu, usb_menu->sel);
			ret |= KEY_CLICK;
			break;
		case 'l':
			if (menu_shift_selected(usb_menu, -1)) {
				menu_draw(usb_menu);
				ret |= KEY_CLICK;
			}
			break;
		case 'r':
			if (menu_shift_selected(usb_menu, 1)) {
				menu_draw(usb_menu);
				ret |= KEY_CLICK;
			}
			break;
		case 'm':
			usb_menu_quit();	
			ret |= KEY_CLICK;
			break;
		default:
			ret |= KEY_UNUSED;
			break;
		}
	default:
		ret |= EVENT_UNUSED;
		break;
	}
	return ret;
}


static item_st usb_menu_items[] = {
	{"Return to Podzilla", usb_menu_quit, ACTION_MENU},
	{"Reboot to Diskmode", goto_diskmode, ACTION_MENU},
	{0}
};

static void usb_draw()
{
	char *str = "USB connected";
	GR_SIZE w, h, b;
	int y = (usb_wh - usb_mh) + 1;

	GrSetGCForeground(usb_gc, appearance_get_color(CS_FG));
	GrRect(usb_wid, usb_gc, 1, 1, usb_ww - 2, usb_wh - 2);
	GrLine(usb_wid, usb_gc, 1, y, usb_ww - 2, y);

	GrGetGCTextSize(usb_gc, str, -1, GR_TFASCII, &w, &h, &b);

	GrText(usb_wid, usb_gc, (usb_ww - w) / 2, 3 + 2, str,
			-1, GR_TFASCII | GR_TFTOP);
}

static void usb_do_draw()
{
	if (usb_wid == GrGetFocus()) {
		menu_draw(usb_menu);
		usb_draw();
	}
}
void usb_check_goto_diskmode()
{
	GR_SIZE w, h, b;
	int i, x, y, width;
	
	if (hw_version < 40000)
		return;

	usb_gc = pz_get_gc(1);
	GrSetGCForeground(usb_gc, appearance_get_color(CS_FG));
	
	for (i = 0, width = 0; usb_menu_items[i].text != 0; i++) {
		GrGetGCTextSize(usb_gc, usb_menu_items[i].text, -1,
				GR_TFASCII, &w, &h, &b);
		if (width < w)
			width = w;
	}
	usb_ww = width + 8*2 + 6;
	usb_mh = (h + 4)*i + 6;
	usb_wh = usb_mh + 3 + h + 4;
	x = (screen_info.cols - usb_ww)/2;
	y = (screen_info.rows - usb_wh)/2;
		
	usb_wid = pz_new_window(x, y, usb_ww, usb_wh, usb_do_draw,
			usb_menu_keystroke);
	GrSelectEvents(usb_wid, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_KEY_UP |
		GR_EVENT_MASK_KEY_DOWN | GR_EVENT_MASK_TIMER);
	GrSetWindowBackgroundColor(usb_wid, appearance_get_color(CS_BG));
	usb_menu = menu_init(usb_wid, "Diskmode", 3, usb_wh - usb_mh + 3,
			usb_ww - 6, usb_mh - 6, NULL, usb_menu_items, ASCII);
	GrMapWindow(usb_wid);
}

