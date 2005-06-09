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
	int samples[64];
} wftable;

static wftable waveforms[] = {
        {     0,  0, { 0 }},

        {   697, 63, { 128, 135, 143, 151, 158, 166, 172, 179, 185, 190, 195, 
                       199, 202, 204, 206, 207, 207, 207, 206, 204, 201, 197, 
                       193, 188, 183, 176, 170, 163, 156, 148, 140, 133, 125, 
                       117, 109, 101,  94,  87,  80,  74,  68,  63,  59,  55, 
                        52,  50,  48,  48,  48,  48,  50,  52,  56,  59,  64, 
                        69,  75,  81,  87,  95, 102, 110, 117 }},
        {   770, 57, { 128, 136, 145, 153, 161, 169, 176, 183, 189, 194, 199, 
                       202, 205, 207, 207, 207, 206, 204, 201, 197, 192, 187, 
                       181, 174, 166, 159, 150, 142, 133, 124, 116, 107,  99, 
                        91,  83,  76,  70,  64,  59,  55,  52,  49,  48,  48, 
                        48,  50,  52,  55,  59,  64,  70,  77,  84,  91,  99, 
                       108, 116 }},
        {   852, 51, { 128, 137, 147, 156, 165, 173, 181, 188, 194, 199, 202, 
                       205, 207, 207, 207, 205, 202, 198, 193, 187, 180, 172, 
                       164, 155, 146, 136, 126, 117, 107,  98,  89,  81,  73, 
                        67,  61,  56,  52,  49,  48,  48,  48,  50,  53,  58, 
                        63,  69,  76,  84,  92, 101, 111 }},
        {   941, 46, { 128, 138, 149, 159, 168, 177, 185, 192, 198, 202, 205, 
                       207, 207, 206, 204, 200, 195, 188, 181, 172, 163, 153, 
                       143, 132, 121, 111, 101,  91,  82,  73,  66,  60,  55, 
                        51,  48,  48,  48,  50,  53,  58,  64,  71,  79,  88, 
                        97, 108 }},

        {  1209, 36, { 128, 141, 155, 167, 178, 188, 196, 202, 206, 207, 207, 
                       203, 198, 190, 181, 170, 158, 144, 131, 117, 104,  91, 
                        79,  69,  61,  54,  50,  48,  48,  51,  56,  63,  72, 
                        82,  94, 107 }},
        {  1336, 33, { 128, 143, 157, 171, 183, 193, 200, 205, 207, 207, 203, 
                       197, 188, 177, 164, 150, 135, 120, 105,  91,  78,  67, 
                        58,  52,  48,  48,  50,  55,  62,  72,  84,  98, 112 }},
        {  1477, 29, { 128, 144, 160, 175, 187, 197, 204, 207, 207, 203, 196, 
                       186, 174, 159, 143, 126, 110,  94,  79,  67,  57,  51, 
                        48,  48,  52,  59,  69,  82,  97 }},
        {  1633, 27, { 128, 146, 163, 179, 192, 201, 206, 207, 204, 197, 186, 
                       172, 155, 137, 118, 100,  84,  69,  58,  51,  48,  49, 
                        54,  63,  76,  91, 109 }},

        {  2600, 16, { 128, 156, 181, 199, 207, 204, 191, 169, 142, 112,  85, 
                        63,  50,  48,  56,  74 }},
        {  1850, 23, { 128, 148, 168, 184, 197, 205, 207, 205, 196, 183, 166, 
                       147, 126, 105,  86,  69,  57,  50,  48,  51,  60,  73, 
                        90 }},
        {  1700, 25, { 128, 147, 165, 181, 193, 202, 207, 207, 202, 193, 180, 
                       164, 146, 127, 108,  90,  74,  61,  52,  48,  48,  53, 
                        62,  75,  91 }},
        {  2200, 20, { 128, 152, 174, 192, 204, 207, 204, 192, 175, 153, 128, 
                       103,  81,  63,  52,  48,  51,  62,  80, 102 }},
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
#ifdef __linux__
#else
	pos=pos; /* eliminate build warning */
#endif
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


/* here's the code that generated the tone waveforms:

void gen_entry( int freq )
{
	int rate = 44100;
	int sample;
	int samples = rate / freq;
	double ts;
	double Pi = 3.14159;
	double sampleperiod = 1.0/rate;
	int per = 0;

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
	gen_entry( 0 );

	gen_entry( 697 );
	gen_entry( 770 );
	gen_entry( 852 );
	gen_entry( 941 );

	gen_entry( 1209 );
	gen_entry( 1336 );
	gen_entry( 1477 );
	gen_entry( 1633 );

	gen_entry( 2600 );
	gen_entry( 1850 );
	gen_entry( 1700 );
	gen_entry( 2200 );

	return( 0 );
}

*/
