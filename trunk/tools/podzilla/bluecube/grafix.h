/*
 * BlueCube - just another tetris clone 
 * Copyright (C) 2003  Sebastian Falbesoner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

/* File: grafix.h === gfx Interface */

#define MWINCLUDECOLORS
#include <nano-X.h>

GR_WINDOW_ID tetris_wid;
GR_GC_ID tetris_gc;
GR_SCREEN_INFO screen_info;

void InitWindow(void);
void PutRect(int x, int y, int w, int h, int color);
void DrawValues(void);
void youlose(void);
