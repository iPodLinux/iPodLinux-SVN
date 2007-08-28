/*
 *  linux/include/linux/de2fpga.h -- FPGA driver for the DragonEngine
 *
 *	Copyright (C) 2003 Georges Menie
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _DE2FPGA_H
#define _DE2FPGA_H

#define DE2FPGA_PARAMS_GET _IOR('F', 0, struct de2fpga_info)
#define DE2FPGA_RESET _IOR('F', 1, struct de2fpga_info)

#define DE2FPGA_MAX_TAGLEN 128

struct de2fpga_info {
	int status;
	int file;
	int part;
	int date;
	int time;
	char buf[DE2FPGA_MAX_TAGLEN];
};

#define DE2FPGA_STS_UNKNOWN 0
#define DE2FPGA_STS_OK 1
#define DE2FPGA_STS_DRV_ERROR -1 /* driver internal error */
#define DE2FPGA_STS_HDR_ERROR -2 /* the bitstream header is not recognized */
#define DE2FPGA_STS_TGL_ERROR -3 /* unknown tag letter in the bitstream */
#define DE2FPGA_STS_TGC_ERROR -4 /* tag content length too long */
#define DE2FPGA_STS_NSZ_ERROR -5 /* bitstream size is zero */
#define DE2FPGA_STS_PRG_ERROR -6 /* error starting the programming mode */
#define DE2FPGA_STS_EPR_ERROR -7 /* error ending the programming mode */
#define DE2FPGA_STS_FIL_ERROR -8 /* bitstream format error */
#define DE2FPGA_STS_REL_ERROR -9 /* unexpected end of bitstream */

#endif
