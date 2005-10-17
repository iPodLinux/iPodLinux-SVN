/*
 * Copyright (C) 2005 Adam Johnston <agjohnst@uiuc.edu>
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

#ifndef __VIDEOVARS_H_
#define __VIDEOVARS_H_

#define VIDEO_FILEBUFFER_COUNT	8 
#define VIDEO_FILEBUFFER_SIZE	1548800
#define FRAMES_PER_BUFFER	20
#define NUM_FRAMES_BUFFER	20


#define VAR_COP_HANDLER			(0x4001501C)
#define VAR_COP_STATE			(VAR_COP_HANDLER+4)
#define VAR_VIDEO_ON			(VAR_COP_STATE+4)
#define VAR_VIDEO_COP_HANDLER		(VAR_VIDEO_ON+4) 
#define VAR_VIDEO_CURBUFFER_PLAYING 	(VAR_VIDEO_COP_HANDLER+4) 
#define VAR_VIDEO_CURFRAME_PLAYING 	(VAR_VIDEO_CURBUFFER_PLAYING+4) 
#define VAR_VIDEO_MODE			(VAR_VIDEO_CURFRAME_PLAYING+4) 
#define VAR_VIDEO_FRAME_WIDTH		(VAR_VIDEO_MODE+4)
#define VAR_VIDEO_FRAME_HEIGHT		(VAR_VIDEO_FRAME_WIDTH+4) 
#define VAR_VIDEO_MICROSECPERFRAME	(VAR_VIDEO_FRAME_HEIGHT+4) 
#define VAR_VIDEO_FRAMES_PER_BUFFER	(VAR_VIDEO_MICROSECPERFRAME+4) 
#define VAR_VIDEO_BUFFER_READY		(VAR_VIDEO_FRAMES_PER_BUFFER+4) 
#define VAR_VIDEO_BUFFER_DONE		(VAR_VIDEO_BUFFER_READY+0x20) 
#define VAR_VIDEO_FRAMES		(VAR_VIDEO_BUFFER_DONE+0x20)

#endif
