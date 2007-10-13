/*
 * Copyright (c) 2005 Joshua Oreman
 *
 * This file is a part of TTK.
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

#ifndef _ALL_TTK_H_
#define _ALL_TTK_H_

/* version is 0xMMMmm  MAJOR minor */
/* so, 0x10117 is v101.17 */
/* Major will change when there are structural changes */
/* Major bumps will require software to be recompiled */
/* Minor will change when there are implementation/minor chages */

#define TTK_API_VERSION  0x10303
#define TTK_VERSION_CHECK() ttk_version_check(TTK_API_VERSION)

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdlib.h>
#include <ttk/ttk.h>
#include <ttk/menu.h>
#include <ttk/icons.h>
#include <ttk/slider.h>
#include <ttk/imgview.h>
#include <ttk/gradient.h>
#include <ttk/textarea.h>
# include <ttk/mwin-emu.h>
#include <ttk/appearance.h>
#ifdef __cplusplus
}
#include <ttk/ttkmm.h>
#endif

#endif
