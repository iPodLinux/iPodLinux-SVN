/*
 *  Tone Dialer (DTMF)
 * Copyright (C) 2005 Scott Lawrence
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


/* these correspond to the table below... */
#define F_0	(0)

#define F_697	(1)
#define F_770	(2)
#define F_852	(3)
#define F_941	(4)

#define F_1209	(5)
#define F_1336	(6)
#define F_1477	(7)
#define F_1633	(8)

#define F_2600	(9)
#define F_1850	(10)
#define F_1700	(11)
#define F_2200	(12)

typedef struct wftable {
	int frequency;
	int nsamples;
	int samples[255];
} wftable;

/*
    2: 	1633 -9
	1336 -12
	1209 +28

    3: 	697 +43
	770 -18
	852 -9

    4:	697 -15
	1850 -163

    5:	941 -62
	1477 -86
*/

static wftable waveforms[] = {
        {     0,  0, { 0 }},

        {   697, 190, { 128, 135, 143, 151, 158, 166, 172, 179, 185, 190, 195, 
                       199, 202, 204, 206, 207, 207, 207, 206, 204, 201, 197, 
                       193, 188, 183, 176, 170, 163, 156, 148, 140, 133, 125, 
                       117, 109, 101,  94,  87,  80,  74,  68,  63,  59,  55, 
                        52,  50,  48,  48,  48,  48,  50,  52,  56,  59,  64, 
                        69,  75,  81,  87,  95, 102, 110, 117, 125, 133, 141, 
                       149, 156, 164, 171, 177, 183, 188, 193, 197, 201, 204, 
                       206, 207, 207, 207, 206, 204, 202, 198, 194, 189, 184, 
                       178, 172, 165, 158, 150, 143, 135, 127, 119, 111, 103, 
                        96,  89,  82,  76,  70,  65,  60,  56,  53,  50,  49, 
                        48,  48,  48,  50,  52,  55,  58,  62,  67,  73,  79, 
                        86,  93, 100, 107, 115, 123, 131, 139, 147, 154, 162, 
                       169, 175, 181, 187, 192, 196, 200, 203, 205, 207, 207, 
                       207, 206, 205, 202, 199, 195, 191, 186, 180, 174, 167, 
                       160, 152, 145, 137, 129, 121, 113, 105,  98,  91,  84, 
                        77,  72,  66,  61,  57,  54,  51,  49,  48,  48,  48, 
                        49,  51,  54,  57,  61,  66,  71,  77,  84,  91,  98, 
                       105, 113, 121 }},
        {   770, 172, { 128, 136, 145, 153, 161, 169, 176, 183, 189, 194, 199, 
                       202, 205, 207, 207, 207, 206, 204, 201, 197, 192, 187, 
                       181, 174, 166, 159, 150, 142, 133, 124, 116, 107,  99, 
                        91,  83,  76,  70,  64,  59,  55,  52,  49,  48,  48, 
                        48,  50,  52,  55,  59,  64,  70,  77,  84,  91,  99, 
                       108, 116, 125, 134, 143, 151, 159, 167, 175, 181, 187, 
                       193, 198, 201, 204, 206, 207, 207, 207, 205, 202, 198, 
                       194, 189, 183, 176, 169, 161, 153, 144, 135, 127, 118, 
                       109, 101,  93,  85,  78,  71,  65,  60,  56,  52,  50, 
                        48,  48,  48,  49,  51,  54,  58,  63,  69,  75,  82, 
                        89,  97, 105, 114, 123, 131, 140, 149, 157, 165, 173, 
                       180, 186, 192, 196, 200, 204, 206, 207, 207, 207, 205, 
                       203, 199, 195, 190, 184, 178, 171, 163, 155, 146, 138, 
                       129, 120, 112, 103,  95,  87,  80,  73,  67,  62,  57, 
                        53,  50,  49,  48,  48,  49,  50,  53,  57,  62,  67, 
                        73,  80,  87,  95, 103, 112, 120 }},
        {   852, 155, { 128, 137, 147, 156, 165, 173, 181, 188, 194, 199, 202, 
                       205, 207, 207, 207, 205, 202, 198, 193, 187, 180, 172, 
                       164, 155, 146, 136, 126, 117, 107,  98,  89,  81,  73, 
                        67,  61,  56,  52,  49,  48,  48,  48,  50,  53,  58, 
                        63,  69,  76,  84,  92, 101, 111, 120, 130, 139, 149, 
                       158, 167, 175, 182, 189, 195, 200, 203, 206, 207, 207, 
                       207, 204, 201, 197, 192, 185, 178, 170, 162, 153, 143, 
                       134, 124, 114, 105,  96,  87,  79,  72,  65,  60,  55, 
                        51,  49,  48,  48,  49,  51,  54,  59,  64,  71,  78, 
                        86,  94, 103, 113, 122, 132, 142, 151, 160, 169, 177, 
                       184, 191, 196, 201, 204, 206, 207, 207, 206, 204, 200, 
                       196, 190, 184, 176, 168, 160, 151, 141, 131, 122, 112, 
                       103,  94,  85,  77,  70,  64,  58,  54,  51,  49,  48, 
                        48,  49,  52,  55,  60,  66,  72,  80,  88,  96, 106, 
                       115 }},
        {   941, 234, { 128, 138, 149, 159, 168, 177, 185, 192, 198, 202, 205, 
                       207, 207, 206, 204, 200, 195, 188, 181, 172, 163, 153, 
                       143, 132, 121, 111, 101,  91,  82,  73,  66,  60,  55, 
                        51,  48,  48,  48,  50,  53,  58,  64,  71,  79,  88, 
                        97, 108, 118, 129, 140, 150, 160, 170, 178, 186, 193, 
                       198, 203, 206, 207, 207, 206, 203, 199, 194, 187, 180, 
                       171, 162, 152, 141, 131, 120, 109,  99,  89,  80,  72, 
                        65,  59,  54,  50,  48,  48,  48,  50,  54,  59,  65, 
                        72,  80,  89,  99, 109, 120, 130, 141, 151, 161, 171, 
                       179, 187, 194, 199, 203, 206, 207, 207, 206, 203, 199, 
                       193, 186, 179, 170, 161, 150, 140, 129, 119, 108,  98, 
                        88,  79,  71,  64,  58,  53,  50,  48,  48,  48,  51, 
                        54,  59,  66,  73,  81,  90, 100, 110, 121, 132, 142, 
                       153, 163, 172, 180, 188, 194, 200, 204, 206, 207, 207, 
                       205, 202, 198, 192, 185, 178, 169, 159, 149, 139, 128, 
                       117, 107,  97,  87,  78,  70,  63,  57,  53,  50,  48, 
                        48,  49,  51,  55,  60,  66,  74,  82,  92, 101, 112, 
                       122, 133, 144, 154, 164, 173, 181, 189, 195, 200, 204, 
                       206, 207, 207, 205, 202, 197, 191, 184, 176, 168, 158, 
                       148, 137, 127, 116, 105,  95,  86,  77,  69,  62,  57, 
                        52,  49,  48,  48,  49,  52,  56,  61,  67,  75,  83, 
                        93, 103, 113 }},

        {  1209, 73, { 128, 141, 155, 167, 178, 188, 196, 202, 206, 207, 207, 
                       203, 198, 190, 181, 170, 158, 144, 131, 117, 104,  91, 
                        79,  69,  61,  54,  50,  48,  48,  51,  56,  63,  72, 
                        82,  94, 107, 121, 135, 148, 161, 173, 184, 193, 200, 
                       204, 207, 207, 205, 201, 194, 186, 175, 164, 151, 137, 
                       124, 110,  97,  85,  74,  64,  57,  51,  48,  48,  49, 
                        53,  59,  67,  77,  88, 101, 114 }},
        {  1336, 66, { 128, 143, 157, 171, 183, 193, 200, 205, 207, 207, 203, 
                       197, 188, 177, 164, 150, 135, 120, 105,  91,  78,  67, 
                        58,  52,  48,  48,  50,  55,  62,  72,  84,  98, 112, 
                       127, 142, 157, 171, 183, 193, 200, 205, 207, 207, 203, 
                       197, 188, 177, 164, 150, 135, 120, 105,  91,  78,  67, 
                        58,  52,  48,  48,  50,  55,  62,  72,  84,  97, 112 }},
        {  1477, 209, { 128, 144, 160, 175, 187, 197, 204, 207, 207, 203, 196, 
                       186, 174, 159, 143, 126, 110,  94,  79,  67,  57,  51, 
                        48,  48,  52,  59,  69,  82,  97, 113, 130, 147, 162, 
                       177, 189, 198, 204, 207, 207, 203, 195, 185, 172, 157, 
                       141, 124, 107,  92,  77,  66,  56,  50,  48,  48,  53, 
                        61,  71,  84,  99, 115, 132, 149, 164, 178, 190, 199, 
                       205, 207, 206, 202, 194, 183, 170, 155, 138, 122, 105, 
                        89,  76,  64,  55,  50,  48,  49,  54,  62,  73,  86, 
                       101, 118, 135, 151, 167, 180, 192, 200, 206, 207, 206, 
                       201, 192, 181, 168, 152, 136, 119, 103,  87,  74,  63, 
                        54,  49,  48,  49,  55,  63,  75,  88, 104, 120, 137, 
                       153, 169, 182, 193, 201, 206, 207, 205, 200, 191, 180, 
                       166, 150, 134, 117, 100,  85,  72,  61,  53,  49,  48, 
                        50,  56,  65,  76,  90, 106, 123, 139, 156, 171, 184, 
                       194, 202, 206, 207, 205, 199, 190, 178, 164, 148, 131, 
                       114,  98,  83,  70,  60,  52,  48,  48,  51,  57,  66, 
                        78,  92, 108, 125, 142, 158, 173, 185, 196, 203, 207, 
                       207, 204, 198, 188, 176, 161, 146, 129, 112,  96,  81, 
                        69,  59,  52,  48,  48,  51,  58,  68,  80,  95, 111 }},
        {  1633, 27, { 128, 146, 163, 179, 192, 201, 206, 207, 204, 197, 186, 
                       172, 155, 137, 118, 100,  84,  69,  58,  51,  48,  49, 
                        54,  63,  76,  91, 109 }},

        {  2600, 254, { 128, 156, 181, 199, 207, 204, 191, 169, 142, 112,  85, 
                        63,  50,  48,  56,  74, 100, 129, 158, 182, 200, 207, 
                       204, 190, 168, 141, 111,  84,  63,  50,  48,  57,  75, 
                       101, 130, 159, 183, 200, 207, 204, 190, 167, 139, 110, 
                        83,  62,  50,  48,  57,  76, 102, 131, 160, 184, 201, 
                       207, 203, 189, 166, 138, 109,  82,  61,  50,  48,  58, 
                        77, 103, 132, 161, 185, 201, 207, 203, 188, 165, 137, 
                       108,  81,  61,  49,  49,  58,  78, 104, 133, 162, 186, 
                       202, 207, 203, 188, 164, 136, 107,  80,  60,  49,  49, 
                        59,  79, 105, 134, 163, 186, 202, 207, 202, 187, 163, 
                       135, 106,  79,  59,  49,  49,  60,  80, 106, 135, 164, 
                       187, 202, 207, 202, 186, 162, 134, 105,  78,  59,  49, 
                        49,  60,  81, 107, 137, 165, 188, 203, 207, 201, 185, 
                       161, 133, 103,  77,  58,  48,  49,  61,  81, 108, 138, 
                       166, 189, 203, 207, 201, 184, 160, 132, 102,  77,  58, 
                        48,  50,  62,  82, 109, 139, 167, 189, 203, 207, 200, 
                       184, 159, 130, 101,  76,  57,  48,  50,  62,  83, 110, 
                       140, 168, 190, 204, 207, 200, 183, 158, 129, 100,  75, 
                        57,  48,  50,  63,  84, 112, 141, 169, 191, 204, 207, 
                       200, 182, 157, 128,  99,  74,  56,  48,  50,  63,  85, 
                       113, 142, 170, 191, 204, 207, 199, 181, 156, 127,  98, 
                        73,  56,  48,  51,  64,  86, 114, 143, 171, 192, 205, 
                       207, 198, 180, 155, 126,  97,  72,  55,  48,  51,  65, 
                        87 }},
        {  1850, 143, { 128, 148, 168, 184, 197, 205, 207, 205, 196, 183, 166, 
                       147, 126, 105,  86,  69,  57,  50,  48,  51,  60,  73, 
                        90, 110, 131, 152, 171, 187, 199, 206, 207, 204, 194, 
                       181, 163, 143, 122, 102,  83,  67,  56,  49,  48,  52, 
                        62,  76,  93, 113, 134, 155, 173, 189, 200, 206, 207, 
                       202, 193, 178, 160, 140, 119,  99,  80,  65,  54,  48, 
                        48,  53,  63,  78,  96, 117, 138, 158, 176, 191, 201, 
                       207, 207, 201, 190, 175, 157, 137, 116,  96,  78,  63, 
                        53,  48,  49,  55,  66,  81,  99, 120, 141, 161, 179, 
                       193, 203, 207, 206, 200, 188, 173, 154, 133, 112,  92, 
                        75,  61,  52,  48,  49,  56,  68,  84, 103, 123, 144, 
                       164, 181, 195, 204, 207, 206, 198, 186, 170, 151, 130, 
                       109,  89,  72,  59,  51,  48,  50,  58,  70,  87, 106 }},
        {  1700, 233, { 128, 147, 165, 181, 193, 202, 207, 207, 202, 193, 180, 
                       164, 146, 127, 108,  90,  74,  61,  52,  48,  48,  53, 
                        62,  75,  91, 109, 129, 148, 166, 181, 194, 203, 207, 
                       207, 202, 192, 179, 163, 145, 126, 107,  89,  73,  61, 
                        52,  48,  48,  53,  63,  76,  92, 110, 130, 149, 167, 
                       182, 195, 203, 207, 207, 201, 192, 179, 162, 144, 125, 
                       106,  88,  72,  60,  52,  48,  49,  54,  64,  77,  93, 
                       112, 131, 150, 168, 183, 195, 203, 207, 206, 201, 191, 
                       178, 161, 143, 124, 105,  87,  71,  59,  51,  48,  49, 
                        54,  64,  78,  94, 113, 132, 151, 169, 184, 196, 204, 
                       207, 206, 200, 190, 177, 160, 142, 122, 103,  86,  71, 
                        59,  51,  48,  49,  55,  65,  79,  95, 114, 133, 152, 
                       170, 185, 196, 204, 207, 206, 200, 190, 176, 159, 141, 
                       121, 102,  85,  70,  58,  51,  48,  49,  55,  66,  80, 
                        96, 115, 134, 153, 171, 185, 197, 204, 207, 206, 200, 
                       189, 175, 158, 140, 120, 101,  84,  69,  58,  50,  48, 
                        49,  56,  66,  80,  97, 116, 135, 154, 172, 186, 198, 
                       205, 207, 206, 199, 188, 174, 157, 138, 119, 100,  83, 
                        68,  57,  50,  48,  50,  56,  67,  81,  98, 117, 136, 
                       155, 172, 187, 198, 205, 207, 205, 199, 188, 173, 156, 
                       137, 118,  99,  82,  68,  57,  50,  48,  50,  57,  68, 
                        82,  99 }},
        {  2200, 241, { 128, 152, 174, 192, 204, 207, 204, 192, 175, 153, 128, 
                       103,  81,  63,  52,  48,  51,  62,  80, 102, 126, 151, 
                       173, 191, 203, 207, 204, 193, 176, 154, 129, 105,  82, 
                        64,  52,  48,  51,  62,  79, 101, 125, 150, 173, 191, 
                       203, 207, 204, 194, 177, 155, 130, 106,  83,  65,  52, 
                        48,  50,  61,  78, 100, 124, 149, 172, 190, 202, 207, 
                       205, 194, 178, 156, 132, 107,  84,  65,  53,  48,  50, 
                        60,  77,  98, 123, 148, 171, 189, 202, 207, 205, 195, 
                       179, 157, 133, 108,  85,  66,  53,  48,  50,  60,  76, 
                        97, 122, 147, 170, 188, 201, 207, 205, 196, 180, 158, 
                       134, 109,  86,  67,  54,  48,  50,  59,  75,  96, 120, 
                       145, 169, 188, 201, 207, 206, 196, 180, 159, 135, 110, 
                        87,  68,  54,  48,  49,  58,  74,  95, 119, 144, 168, 
                       187, 201, 207, 206, 197, 181, 160, 136, 111,  88,  68, 
                        55,  48,  49,  58,  73,  94, 118, 143, 166, 186, 200, 
                       207, 206, 197, 182, 161, 137, 113,  89,  69,  55,  48, 
                        49,  57,  72,  93, 117, 142, 165, 185, 200, 207, 206, 
                       198, 183, 163, 139, 114,  90,  70,  56,  48,  49,  57, 
                        72,  92, 116, 141, 164, 185, 199, 207, 206, 199, 184, 
                       164, 140, 115,  91,  71,  56,  48,  48,  56,  71,  91, 
                       115, 140, 163, 184, 199, 206, 207, 199, 185, 165, 141, 
                       116,  92,  72,  57,  49,  48,  56,  70,  90, 113 }}
};


/* Frequencies: (hz)	box color	function
	2600 		Blue		trunk on/off	(obsolete)
	1850				Tasi Lock
	1700 + 2200	Red		(+66ms  )x1 = nickel
					(+66 _66)x2 = dime
					(+33 _33)x5 = quarter
	(table)		Silver/White

                1209    1336    1477    1633
        697     1       2       3       A
        770     4       5       6       B
        852     7       8       9       C
        941     *       0       #       D
*/

#include <stdlib.h>
#include "pz.h"
#ifdef __linux__
#include "oss.h"
#else
#define dsp_st int
#endif


static GR_WINDOW_ID dialer_wid;
static GR_GC_ID dialer_gc;

#ifdef __linux__
static dsp_st dspz;
#else
static int dspz;
#endif

static int xlocal,ylocal,lastxlocal,lastylocal;
static char digitdisplay[16];
static int numfull, littr;

static int current_dialer_button = 0;
static int last_bouvet_item;
char *dialerpad[]={ 
		"1","2","3","A",
		"4","5","6","B",
		"7","8","9","C",
		"*","0","#","D"};

static int cxgap = 5, cygap = 4;
static int cbw = 25, cbh = 16;

#define CHEIGHT	((5*cbh)+(4*cygap))
#define CWIDTH	((4*cbw)+(3*cxgap))
#define CXOFF	((screen_info.cols-CWIDTH)/2)
#define CYOFF	(((screen_info.rows-(HEADER_TOPLINE+1))-CHEIGHT)/2)
#define CBYOFF	(CYOFF+cbh+cygap)

static void draw_dialer() {
	GR_SIZE width, height, base;

	GrSetGCUseBackground(dialer_gc, GR_FALSE);
	GrSetGCForeground(dialer_gc, BLACK);
	GrGetGCTextSize(dialer_gc, digitdisplay, -1, GR_TFASCII, &width, &height, &base);
	/*
	GrRect(dialer_wid, dialer_gc, CXOFF, CYOFF, CWIDTH, cbh);
	GrSetGCForeground(dialer_gc, WHITE);
	GrFillRect(dialer_wid, dialer_gc, CXOFF+1, CYOFF+1, CWIDTH-2, cbh-2);
	GrSetGCForeground(dialer_gc, BLACK);
	GrText(dialer_wid, dialer_gc, CXOFF+(CWIDTH-width-4),
		CYOFF+((cbh/2)-(height/2)), digitdisplay, -1, GR_TFASCII|GR_TFTOP);
	*/

	/* range check */
	if(current_dialer_button < 0)
		current_dialer_button = 15;
	else if(current_dialer_button >= 16)
		current_dialer_button = 0;
		
	/* compute button sizings */
	xlocal=CXOFF+((cbw+cxgap)*(current_dialer_button%4));
	ylocal=CBYOFF+((cbh+cygap)*(current_dialer_button/4));
	lastxlocal=CXOFF+((cbw+cxgap)*(last_bouvet_item%4));
	lastylocal=CBYOFF+((cbh+cygap)*(last_bouvet_item/4));
	
	GrSetGCForeground(dialer_gc, WHITE);
	GrFillRect(dialer_wid, dialer_gc, lastxlocal+1, lastylocal+1, cbw-2, cbh-2);
	GrSetGCForeground(dialer_gc, BLACK);
	GrGetGCTextSize(dialer_gc, dialerpad[last_bouvet_item], -1, GR_TFASCII,
	                &width, &height, &base);
	GrText(dialer_wid, dialer_gc,
	       lastxlocal+((cbw/2)-(width/2)), lastylocal+((cbh/2)-(height/2)),
	       dialerpad[last_bouvet_item], -1, GR_TFASCII|GR_TFTOP);
	GrFillRect(dialer_wid, dialer_gc, xlocal, ylocal, cbw, cbh);
	GrSetGCForeground(dialer_gc, WHITE);
	GrGetGCTextSize(dialer_gc, dialerpad[current_dialer_button], -1, GR_TFASCII,
	                &width, &height, &base);
	GrText(dialer_wid, dialer_gc,
	       xlocal+((cbw/2)-(width/2)), ylocal+((cbh/2)-(height/2)),
	       dialerpad[current_dialer_button], -1, GR_TFASCII|GR_TFTOP);
	GrSetGCUseBackground(dialer_gc, GR_FALSE);
	GrSetGCMode(dialer_gc, GR_MODE_SET);
}

void dtmf_play( dsp_st *dspz, char key )
{
	int bp;
	int f1p = 0;
	int f2p = 0;
	int freq1 = F_0;
	int freq2 = F_0;
	int ontime = 66;
	int nsamps = 4410;
	int ntimes = 1;
	int c;
	short * buf;

	buf = (short *)malloc( sizeof( short ) * nsamps );


	/* freq 1 */
	switch( key ) {
	case( '1' ): case( '2' ): case( '3' ): case( 'A' ): 
		freq1 = F_697; break;
	case( '4' ): case( '5' ): case( '6' ): case( 'B' ): 
		freq1 = F_770; break;
	case( '7' ): case( '8' ): case( '9' ): case( 'C' ): 
		freq1 = F_852; break;
	case( '*' ): case( '0' ): case( '#' ): case( 'D' ): 
		freq1 = F_941; break;
	case( 's' ): 
		freq1 = F_2600; break;
	case( 't' ): 
		freq1 = F_1850; break;
	case( 'n' ): case( 'd' ): case( 'q' ): 
		freq1 = F_1700; break;
	}

	/* freq 2 */
	switch( key ) {
	case( '1' ): case( '4' ): case( '7' ): case( '*' ): 
		freq2 = F_1209; break;
	case( '2' ): case( '5' ): case( '8' ): case( '0' ): 
		freq2 = F_1336; break;
	case( '3' ): case( '6' ): case( '9' ): case( '#' ): 
		freq2 = F_1477; break;
	case( 'A' ): case( 'B' ): case( 'C' ): case( 'D' ): 
		freq2 = F_1633; break;
	case( 's' ): 
		freq2 = F_0; break;
	case( 't' ): 
		freq2 = F_0; break;
	case( 'n' ): case( 'd' ): case( 'q' ): 
		freq2 = F_2200; break;
	}

	/* adjust timings */
	if( key == 'Q' ) ontime = 33;
	if( key == ',' ) ontime = 500;	/* pause */

	/* adjust repeats */
	if( key == 'Q' ) ntimes = 5;
	if( key == 'D' ) ntimes = 2;

	for( c=0 ; c<ntimes ; c++ )
	{
		// play the two tones
		for( bp = 0 ; bp < nsamps ; bp++ )
		{
			/* copy over and add the two waveforms */
			buf[bp] = (waveforms[freq1].samples[f1p++])
				+ (waveforms[freq2].samples[f2p++]);
			buf[bp] = buf[bp] << 6;

			/* adjust sample pointers */
			if( f1p >= waveforms[freq1].nsamples )  f1p = 0;
			if( f2p >= waveforms[freq2].nsamples )  f2p = 0;
		}
#ifdef __linux__
		dsp_write( dspz, buf, nsamps*sizeof(short));
#endif

		/* zero it out */
		for( bp=0 ; bp < nsamps ; bp++ )  buf[bp] = 0;

#ifdef __linux__
		dsp_write( dspz, buf, nsamps*sizeof(short));
#endif
	}

	free( buf );
}

static void dialer_press_button(int pos) {
	dtmf_play( &dspz, dialerpad[pos][0] );
}

static void dialer_do_draw() {
	int i;
	GR_SIZE width, height, base;
	pz_draw_header("DTMF Dialer");
	digitdisplay[0]='\0';
	numfull = 0;
	littr = 0;
	GrSetGCForeground(dialer_gc, BLACK);
	for(i=0; i<=15; i++) {
		GrGetGCTextSize(dialer_gc, dialerpad[i], -1, GR_TFASCII,
		                &width, &height, &base);
		GrRect(dialer_wid, dialer_gc,CXOFF+((cbw+cxgap)*(i%4)), 
		       CBYOFF+((cbh+cygap)*(i/4)), cbw, cbh);
		GrText(dialer_wid, dialer_gc,
		       CXOFF+((cbw+cxgap)*(i%4))+((cbw/2)-(width/2)),
		       CBYOFF+((cbh+cygap)*(i/4))+((cbh/2)-(height/2)),
		       dialerpad[i], -1, GR_TFASCII|GR_TFTOP);
	}
	draw_dialer();
}

static int dialer_do_keystroke(GR_EVENT * event) {
	int ret = 0;

	switch(event->type) {
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch) {
		case '\r':
		case '\n':
			dialer_press_button(current_dialer_button);
			draw_dialer();
			ret |= KEY_CLICK;
			break;

		case 'l':
			last_bouvet_item = current_dialer_button;
			current_dialer_button--;
			draw_dialer();
			ret |= KEY_CLICK;
			break;

		case 'r':
			last_bouvet_item = current_dialer_button;
			current_dialer_button++;
			draw_dialer();
			ret |= KEY_CLICK;
			break;

		case 'm':
		case 'q':
#ifdef __linux__
                        dsp_close(&dspz);
#endif
			GrDestroyGC(dialer_gc);
			pz_close_window(dialer_wid);
			ret |= KEY_CLICK;
			break;

		default:
			ret |= KEY_UNUSED;
		}
		break;

	default:
		ret |= EVENT_UNUSED;
		break;
	}
	return ret;
}

/* the entry point for the menu system */
void new_dialer_window() {
	int ret = 0;

#ifdef __linux__
        ret = dsp_open(&dspz, DSP_LINEOUT);
#endif
        if (ret < 0) {
                pz_perror("/dev/dsp");
                return;
        }
#ifdef __linux__
	ret = dsp_setup(&dspz, 1, 44100);
#endif

	dialer_gc = pz_get_gc(1);
	GrSetGCUseBackground(dialer_gc, GR_FALSE);
	GrSetGCForeground(dialer_gc, BLACK);

	if (screen_info.cols < 160) { //mini
		cxgap = 3; cygap = 2;
		cbw = 22; cbh = 15;
	}

	dialer_wid = pz_new_window(0, HEADER_TOPLINE + 1, 
				screen_info.cols, 
				screen_info.rows - (HEADER_TOPLINE + 1), 
				dialer_do_draw, dialer_do_keystroke);

	GrSelectEvents(dialer_wid, GR_EVENT_MASK_EXPOSURE |
			GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(dialer_wid);
}

/* an entry point for an address book */
void dialer_dial( char * number )
{
	int x = 0;
	int ret;
	dsp_st dspz;

	if( !number ) return;

#ifdef __linux__
	ret = dsp_open(&dspz, DSP_LINEOUT);
	if( ret < 0 ) {
		pz_error( "/dev/dsp" );
		return;
	}
	ret = dsp_setup(&dspz, 1, 44100);
#endif

	while ( number[x] != '\0' )
	{
		dtmf_play( &dspz, number[x++] );
	}

#ifdef __linux__
	dsp_close(&dspz);
#endif
}


/* here's the code that generated the tone waveforms:*/
#ifdef SNOWING_IN_HELL
#include <stdio.h>

void gen_entry( int freq, int cycles )
{
	int rate = 44100;
	int sample;
	double dsamples;
	int samples;
	double ts;
	double Pi = 3.14159;
	double sampleperiod = 1.0/rate;
	int per = 0;
	int cd;

	if( freq==0 ) {
		dsamples = 1;
	} else {
		dsamples = ((double)rate / (double) freq) * (double)cycles;
		dsamples += 0.5;
	}
	samples = (int) dsamples;


/* -- for doing statistical analysis, uncomment this section.
	cd = (samples * freq) / cycles;
	printf( "%d -- %d %f  -> %d %s%d \n", 
		freq, samples, dsamples, cd, 
		(cd-rate)>0?"+":"", cd-rate ); 
	return;

	/* we need to fit within the [255] element range, and at the same
	    time, we want things to be reasonably accurate.  We basically go
	    for waveforms that generate with the lowest |cd-rate| for the 
	    available space.  For some of the tones (the regular number pad
	    rows and columns) I basically went with frequencies within 
	    tolerances that seem to work well on my personal telephone.
	*/
*/

	printf( "\t{ %5d, %2d, { ", freq, samples );


	freq *= 2*Pi;

	for (sample=0; sample<samples; sample++)
	{
		ts = 128.0 + 80 * sin(freq*sample*sampleperiod);

		if( sample>0 ) printf( ", " );
		if( per++ > 10 )
		{
			per = 1;
			printf( "\n\t\t       " );
		}
		printf( "%3d", (int)ts );
	}
	if( samples==0 ) printf( "0" );
	printf( " }},\n");
}


int main( int argc, char ** argv )
{
	int cyc = 1;

	if( argc > 1 )   cyc = atoi( argv[1] );

	gen_entry( 0, 1 );

	gen_entry( 697, cyc );
	gen_entry( 770, cyc );
	gen_entry( 852, cyc );
	gen_entry( 941, cyc );

	gen_entry( 1209, cyc );
	gen_entry( 1336, cyc );
	gen_entry( 1477, cyc );
	gen_entry( 1633, cyc );

	gen_entry( 2600, cyc );
	gen_entry( 1850, cyc );
	gen_entry( 1700, cyc );
	gen_entry( 2200, cyc );

	return( 0 );
}
#endif
