/*
 *  linux/include/linux/de2ts.h -- Touchscreen driver for the DragonEngine
 *
 *	Copyright (C) 2003 Georges Menie
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _DE2TS_H
#define _DE2TS_H

/* Pen events */
#define EV_PEN_DOWN    1
#define EV_PEN_MOVE    2
#define EV_PEN_UP      3

#define DE2TS_VERSION 2

#define DE2TS_DRV_PARAMS_GET _IOR('T', 0, struct de2ts_drv_params)
#define DE2TS_DRV_PARAMS_SET _IOR('T', 1, struct de2ts_drv_params)
#define DE2TS_CAL_PARAMS_GET _IOR('T', 2, struct de2ts_cal_params)
#define DE2TS_CAL_PARAMS_SET _IOR('T', 3, struct de2ts_cal_params)

/* Available info from pen position and status */
struct de2ts_event {
	int x, y;					/* calculated pen position               */
	int xraw, yraw;				/* raw pen position                      */
	int event;					/* event from pen (DOWN,UP,MOVE)         */
	int ev_no;					/* no of the event                       */
	unsigned long ev_time;		/* time of the event (ms) since ts_open  */
};

/* Structures that define touch screen parameters */
struct de2ts_drv_params {
	short version;				/* version number for check, returned by xx_PARAMS_GET */
								/* you have to set it to DE2TS_VERSION when calling    */
								/* xx_PARAMS_SET                                       */

	short max_samples_x;		/* maximum number of samples before giving up, for x   */
	short max_samples_y;		/* and for y conversion                                */
	short min_equals_x;			/* validate the position when this cconsecutive number */
	short min_equals_y;			/* of samples on x and y has been reached              */
	short max_error;			/* below this level, two consecutive samples are equal */
	short max_speed;			/* max speed allowed. To remove spikes while moving    */

	short xysw;					/* swap x and y coord.    */
};

struct de2ts_cal_params {
	short version;				/* version number for check, returned by xx_PARAMS_GET */
								/* you have to set it to DE2TS_VERSION when calling    */
								/* xx_PARAMS_SET                                       */

	short xoff;					/* calibration data       */
	short xden;					/*                        */
	short yoff;					/*                        */
	short yden;					/*                        */
	short xrng;					/* output range coord.    */
	short yrng;					/*                        */
};

#endif
