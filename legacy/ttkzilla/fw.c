/*
 * Copyright (C) 2005 Adam Johnston
 * 0.1 Modified for FireWire support by copying usb.c - Jeffrey Nelson
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
#include "pz.h"

#define outl(a, b) (*(volatile unsigned int *)(b) = (a))
#define inl(a) (*(volatile unsigned int *)(a))

extern void reboot_ipod();
int fw_is_connected(void);
static void goto_diskmode();


int fw_is_connected(void)
{
#ifdef IPOD
	if (hw_version<40000)
	{	
		if ((inl(0xcf000038) & (1<<4)))
			return 0;
		else
			return 1;  // we're connected - yay!
	} 
#endif
	return 0;
}


static void goto_diskmode()
{
#ifdef IPOD
	reboot_ipod();
#endif
}


void fw_check_goto_diskmode()
{
	if (DIALOG_MESSAGE_T2(_("FireWire Connect"), 
					_("Reboot iPod?"),
					_("No"), _("Yes"), 10)==1)
		goto_diskmode();	
}
