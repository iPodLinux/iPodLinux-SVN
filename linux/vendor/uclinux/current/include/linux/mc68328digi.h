/*----------------------------------------------------------------------------*/
/*
 * linux/drivers/char/mc68328digi.h - Touch screen driver.
 *                                    Header file.
 *
 * Author: Philippe Ney <philippe.ney@smartdata.ch>
 * Copyright (C) 2001 SMARTDATA <www.smartdata.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Thanks to:
 *   Kenneth Albanowski for is first work on a touch screen driver.
 *   Alessandro Rubini for is "Linux device drivers" book.
 *   Ori Pomerantz for is "Linux Kernel Module Programming" guide.
 *
 * Updates:
 *   2001-03-12 Pascal bauermeister
 *              - Modified the semantics of ts_drv_params;
 *              - Added check for version in ioctl()
 *              - included a sample code in the end of this file as comment
 *
 *----------------------------------------------------------------------------
 * Known bugs:
 *   - read() returns zero
 *
 *----------------------------------------------------------------------------
 * HOW TO SETUP THE PARAMETERS FOR COORDINATES SCALING
 *
 * The driver does scaling as follows, in this order:
 *   o for x and y raw samples:
 *       - raw position is multiplied by ratio_num
 *       - result is divided by ratio_den
 *       - offset is added to the result
 *       - result is bounded by min and max
 *   o x and y results are swapped if xy_swap is non-zero
 *
 * Appropriate parameter combinations allow things like:
 *   o rotations in multiples of 90 degrees
 *   o coordinate mirroring
 *   o negative coordinates
 *   o etc.
 *
 * How to calculate the parameters:
 *   o Depending on your hardware, you may need to have x and y swapped. If
 *     so, set xy_swap to 1, always.
 *   o Calibration phase:
 *     choose two physical points, preferrably diagonally opposite corners of
 *     the touch panel. Measure one first time with num=den=1 and offset=0,
 *     i.e. do an ioctl() to set these parameters, and some read() to read
 *     these points.
 *
 *   o Let mx1 be the value measured at the left edge of your touchscreen
 *         mx2 be the value measured at the right edge of your touchscreen
 *         ux1 be the value you want your userland programs to see for mx1
 *         ux2 be the value you want your userland programs to see for mx2
 *   o For X, calculate:
 *         x_ratio_num = ux1 - ux2
 *         x_ratio_den = mx1 - mx2
 *         x_offset = ux1 - mx1*x_ratio_num/x_ratio_den  (or the same with x2)
 *   o Do the same for Y
 *   o Then set the min and max according to the bounds you desire for userland
 *     values
 *
 * Notes:
 *   o We assume linearity between the touchscreen coordinates space and the
 *     userland coordinate space
 *   o During calibration, the points may be any pair of points, but best
 *     results are obtained if they are as distant as possible
 *  
 *
 *----------------------------------------------------------------------------
 *
 * Event generated:
 *                __________      ______________
 *   1) /PENIRQ            |     |
 *                         -------
 *
 *                         |     |
 *                         |     +-> generate PEN_UP
 *                         |
 *                         +-------> generate PEN_DOWN
 *
 *                __________               ______________
 *   2) /PENIRQ            |              |
 *                         ------#####-----
 *
 *                         |     |||||    |
 *                         |     |||||    +-> generate PEN_UP
 *                         |     |||||
 *                         |     ||||+------> generate PEN_MOVE
 *                         |     |||+-------> generate PEN_MOVE
 *                         |     ||+--------> generate PEN_MOVE
 *                         |     |+---------> generate PEN_MOVE
 *                         |     +----------> generate PEN_MOVE
 *                         |
 *                         -----------------> generate PEN_DOWN
 *
 */
/*----------------------------------------------------------------------------*/

#ifndef _MC68328DIGI_H
#define _MC68328DIGI_H

#ifdef __KERNEL__
#include <linux/time.h>   /* for timeval struct */
#include <linux/ioctl.h>  /* for the _IOR macro to define the ioctl commands  */
#endif

/*----------------------------------------------------------------------------*/
/* Used to check the driver vs. userland program.
 *
 * If they have the same number, it means that 'struct ts_pen_info' and
 * 'struct ts_drv_params' have compatible semantics on both side. Otherwise
 * one side is outdated.
 *
 * The number has to be incremented if and only if updates in the driver lead
 * to any change in the semantics of these interface data structures (and not
 * if some internal mechanism of the driver are changed).
 */

#define MC68328DIGI_VERSION 2

/* Userland programs can get this version number in params.version after
 * ioctl(fd, TS_PARAMS_GET, &params). Userland programs must specify their
 * version compatibility by setting version_req to MC68328DIGI_VERSION before
 * doing  ioctl(fd, TS_PARAMS_SET, &params).
 *
 * If version_req does not match, ioctl(fd, TS_PARAMS_SET, &params) would return
 * EBADRQC ('Invalid request code').
 *
 * Note:
 *   it is not possible to check the compatibility for 'struct ts_pen_info'
 *   without doing an ioctl(fd, TS_PARAMS_SET, &params). So, please, do an
 *   ioctl(fd, TS_PARAMS_SET, &params)!
 */

/*----------------------------------------------------------------------------*/

/* Pen events */
#define EV_PEN_DOWN    0
#define EV_PEN_UP      1
#define EV_PEN_MOVE    2

/* Pen states */
/* defined through the 2 lsb of an integer variable. If an error occure,
 * the driver will recover the state PEN_UP and the error bit will be set.
 */
#define ST_PEN_DOWN    (0<<0)   /* bit 0 at 0 = the pen is down            */
#define ST_PEN_UP      (1<<0)   /* bit 0 at 1 = the pen is up              */
#define ST_PEN_ERROR   (1<<1)   /* bit 1 at 1 means that an error occure   */

/* Definition for the ioctl of the driver */
/* Device is type misc then major=10 */
#define MC68328DIGI_MAJOR  10

#define TS_PARAMS_GET _IOR(MC68328DIGI_MAJOR, 0, struct ts_drv_params)
#define TS_PARAMS_SET _IOR(MC68328DIGI_MAJOR, 1, struct ts_drv_params)
 
/*----------------------------------------------------------------------------*/

/* Available info from pen position and status */
struct ts_pen_info {
  int x,y;    /* pen position                                      */
  int dx,dy;  /* delta move from last position                     */
  int event;  /* event from pen (DOWN,UP,CLICK,MOVE)               */
  int state;  /* state of pen (DOWN,UP,ERROR)                      */
  int ev_no;  /* no of the event                                   */
  unsigned long ev_time;  /* time of the event (ms) since ts_open  */
};

/* Structure that define touch screen parameters */
struct ts_drv_params {
  int version;     /* version number for check, returned by TS_PARAMS_GET
		    */
  int version_req; /* version number for check, that MUST be set to
		    * MC68328DIGI_VERSION prior to do TS_PARAMS_SET,
		    * or to -1 to bypass checking (do not do this usually)
		    */
  int x_ratio_num; /*                        */
  int x_ratio_den; /*                        */
  int y_ratio_num; /*                        */
  int y_ratio_den; /*                        */
  int x_offset;    /*                        */
  int y_offset;    /*                        */
  int xy_swap;     /*                        */
  int x_min;       /*                        */
  int y_min;       /*                        */
  int x_max;       /*                        */
  int y_max;       /*                        */
  int mv_thrs;     /* minimum delta move to considere the pen start to move */
  int follow_thrs; /* minimum delta move to follow the pen move when the pen
                    * is moving
		    */
  int sample_ms;   /* time between sampling (ms) */
  int deglitch_on; /* whether to filter glitches at pen down */
  int event_queue_on; /* switch on and off the event queue */
};


/*------------------------------------------------------------------------------
** The following sample code illustrates the usage of the ts driver:
** 
** #include <string.h>
** #include <errno.h>
** #include <fcntl.h>
** #include <stdio.h>
** #include <linux/mc68328digi.h>
** 
** 
**
** 
** const char *ts_device_name = "/dev/ts";
** 
**
**
**
** 
** 
** void tsdrv_params_fprint(FILE *f, struct ts_drv_params *p)
** {
**   fprintf(f, "version     : %d\n",    p->version);
**   fprintf(f, "version_req : %d\n",    p->version_req);
**   fprintf(f, "x ratio     : %d/%d\n", p->x_ratio_num, p->x_ratio_den);
**   fprintf(f, "y ratio     : %d/%d\n", p->y_ratio_num, p->y_ratio_den);
**   fprintf(f, "x limits    : %d/%d\n", p->x_min, p->x_max);
**   fprintf(f, "y limits    : %d/%d\n", p->y_min, p->y_max);
**   fprintf(f, "x offset    : %d\n",    p->x_offset);
**   fprintf(f, "y offset    : %d\n",    p->y_offset);
**   fprintf(f, "invert xy   : %d\n",    p->xy_swap);
**   fprintf(f, "mv thrs     : %d\n",    p->mv_thrs);
**   fprintf(f, "follow thrs : %d\n",    p->follow_thrs);
**   fprintf(f, "sample ms   : %d\n",    p->sample_ms);
**   fprintf(f, "dglitch on  : %d\n",    p->deglitch_on);
**   fprintf(f, "evt Q on    : %d\n",    p->event_queue_on);
** }
** 
** 
** int tsdrv_init(int argc, char *argv[])
** {
**   int ts_fd;
**   int err;
**   struct ts_drv_params  drv_params;
**   int mx1, mx2, my1, my2;
**   int ux1, ux2, uy1, uy2;
** 
**   ts_fd = open(ts_device_name, O_RDWR);
**   if(ts_fd==0) {
**     fprintf(stderr, "%s: error: could not open device %s\n",
** 	    argv[0], ts_device_name);
**     return 0;
**   }
** 
**   err = ioctl(ts_fd, TS_PARAMS_GET, &drv_params);
**   if(err)  {
**     fprintf(stderr, "%s: ioctl TS_PARAMS_GET error: %s\n",
** 	    argv[0], strerror(errno));
**     close(ts_fd);
**     return 0;
**   }
** 
**   printf("\nDefault driver settings:\n");
**   tsdrv_params_fprint(stdout, &drv_params);
** 
** 
**   drv_params.version_req    = MC68328DIGI_VERSION;
**   drv_params.event_queue_on = 1;
**   drv_params.deglitch_on    = 0;
**   drv_params.sample_ms      = 10;
**   drv_params.follow_thrs    = 0;
**   drv_params.mv_thrs        = 2;
**   drv_params.y_max          = 159 + 66; 
**   drv_params.y_min          = 0;
**   drv_params.x_max          = 159;
**   drv_params.x_min          = 0;
**   drv_params.xy_swap        = 0;
** 
**  
**  
**   mx1 = 508; ux1 =   0;
**   my1 = 508; uy1 =   0;
**   mx2 = 188; ux2 = 159;
**   my2 = 188; uy2 = 159;
** 
**  
**   drv_params.x_ratio_num    = ux1 - ux2;
**   drv_params.x_ratio_den    = mx1 - mx2;
**   drv_params.x_offset       =
**     ux1 - mx1 * drv_params.x_ratio_num / drv_params.x_ratio_den;
** 
**   drv_params.y_ratio_num    = uy1 - uy2;
**   drv_params.y_ratio_den    = my1 - my2;
**   drv_params.y_offset       =
**     uy1 - my1 * drv_params.y_ratio_num / drv_params.y_ratio_den;
** 
** 
**   printf("\nNew driver settings:\n");
**   tsdrv_params_fprint(stdout, &drv_params);
** 
**   err = ioctl(ts_fd, TS_PARAMS_SET, &drv_params);
**   if(err)  {
**     fprintf(stderr, "%s: ioctl TS_PARAMS_SET error: %s\n",
** 	    argv[0], strerror(errno));
**     close(ts_fd);
**     return 0;
**   }
** 
**   return ts_fd;
** }
** 
**
**
**
** 
** main(int argc, char *argv[])
** {
**   int                   ts_fd = 0;
**   struct ts_pen_info    pen_info;
**   int                   bytes_transfered = 0;
** 
** 
**   ts_fd = tsdrv_init(argc, argv);
**   if(ts_fd==0)
**     exit(1);
** 
** 
**   while(1) {
**     char whats_up;
**     int x, y;
**     bytes_transfered = read(ts_fd, (void*)&pen_info, sizeof(pen_info));
**     x = pen_info.x;
**     y = pen_info.y;
** 
**     switch(pen_info.event) {
** 
**     case EV_PEN_UP:
**       whats_up = 'U';
**       break;
** 
**     case EV_PEN_DOWN:
**       whats_up = 'D';
**       break;
** 
**     case EV_PEN_MOVE:
**       whats_up = 'M';
**       break;
** 
**     default:
**       whats_up = '?';
**     }
** 
**     printf("%c(%i,%i) ", whats_up, x, y);
**     printf("ev_no:%d ", pen_info.ev_no);
**     printf("ev_usec:%ld ", pen_info.ev_time);
**     printf("bytes_transfered:%d ", bytes_transfered);
**     printf("\n");
**   }
** 
**  
**   close(ts_fd);
**   exit(0);
** }
** 
**
** 
** <<< End of example ----------------------------------------------------------
*/


#endif /* _MC68328DIGI_H */
