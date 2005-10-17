/*
 *	ipodmikmod
 * 
 *	a nano-x interface for mikmod to be used from Podzilla
 *
 *   Interface and port by 
 *          Scott Lawrence (BleuLlama / Yorgle)
 *          ipod@umlautllama.com
 *	    http://www.umlautllama.com
 *
 *    Ported March, April, June 2005
 */

/*
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


#include <strings.h>	/* for strcasecmp */
#include <string.h>	/* for strncpy */
#include <stdio.h>	/* for printf */

#ifdef __linux__
#include <linux/soundcard.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../ipod.h"


#ifdef IPOD
#include "mikmod.h"
#include "mikmodux.h"
#include "mmio.h"
#endif
#include "../pz.h"
#include "../vectorfont.h"

/* these are for the 'loading' screen */
/* a line with no content "" ends the loop */
char *cprt1[] = {
  /* ************************** */
    "       iPod Mikmod",
    " w/Mikmod For Unix v3.0.3",
    "by Jean-Paul Mikkers, Jake",
    " Stine, Chris Conn, Peter",
    " Amstutz & Steve McIntyre",
    ""
};

char *cprt1MINI[] = {
  /* ********************** */
    " iPod Mikmod (v3.0.3)",
    " by Jean-Paul Mikkers",
    "Jake Stine, Chris Conn",
    "    Peter Amstutz,",
    "    Steve McIntyre",
    ""
};


/* File types ***********************************************************/ 

int is_mikmod_playlist_type(char *extension)
{
    return 	   strcasecmp(extension, ".mik") == 0
                || strcasecmp(extension, ".mikmod") == 0
                || strcasecmp(extension, ".mik_playlist") == 0;
}

int is_mikmod_song_type(char *extension)
{
    return 	   strcasecmp(extension, ".669") == 0
		|| strcasecmp(extension, ".dsm") == 0
		|| strcasecmp(extension, ".far") == 0
		|| strcasecmp(extension, ".it") == 0
		|| strcasecmp(extension, ".m15") == 0
		|| strcasecmp(extension, ".med") == 0
		|| strcasecmp(extension, ".mod") == 0
		|| strcasecmp(extension, ".mtm") == 0
		|| strcasecmp(extension, ".s3m") == 0
		|| strcasecmp(extension, ".stm") == 0
		|| strcasecmp(extension, ".ult") == 0
		|| strcasecmp(extension, ".uni") == 0
		|| strcasecmp(extension, ".xm") == 0;
}

/* Mikmod Stuff *********************************************************/ 

#define VOLUME_STEP (4)

#ifdef IPOD
static PLAYLIST playlist;
int next = 0;
#endif

#ifdef IPOD
static char filename[255];
static char archive[255];
static UNIMOD *mf=NULL;
static char * playfile = NULL;
static int cfg_maxchn=64; /* probably should be like 12 */
#endif
static int mikmod_quit = 0;

#ifdef __linux__
static int mikmod_mixer_fd;
static int mikmod_dsp_vol;
#endif

//static int mikmod_autoloop_kill = 1;

static void mikmod_init( void )
{
#ifdef IPOD
    static int mikmod_initialized = 0;
    mikmod_quit = 0;

    if( !mikmod_initialized )
    {
	mikmod_initialized = 1;

	playlist.numused	= 0;
	md_mixfreq		= 44100; /* standard mixing freq */
	md_dmabufsize		= 32768; /* standard dma buf size (max 32000) */
	md_device		= 0;	/* standard device: autodetect */
	md_volume		= 128;	/* driver volume (max 128) */
	md_musicvolume		= 128;	/* music volume (max 128) */
	md_sndfxvolume		= 128;	/* sound effects volume (max 128) */
	md_pansep		= 45;	/* panning separation (0 = mono, 128 = full stereo) */
	md_reverb		= 0;	/* Reverb (max 15) */
	md_mode = DMODE_16BITS |
		    DMODE_STEREO |
		    DMODE_SOFT_MUSIC;  /* default mixing mode */
	/*   md_stereodelay  = 10;	// Stereo Delay (max 15) */

	/* Register the loaders we want to use..  */
	MikMod_RegisterAllLoaders();

	/* Register the drivers we want to use: */
	MikMod_RegisterAllDrivers();

	/* MikMod_RegisterDriver(drv_wav); */ /* wav file output */
	/* MikMod_RegisterDriver(drv_raw); */ /* raw file output */
	/* MikMod_RegisterDriver(drv_nos); */ /* no sound driver */
    }

    /* initialize the playlist */
    PL_InitList(&playlist);

    MikMod_Reset();
#endif
}


#define MIKMOD_STATE_UNDEFINED	(-1)
#define MIKMOD_STATE_INIT 	(0)
#define MIKMOD_STATE_PREP	(1)
#define MIKMOD_STATE_PLAYING	(2)
#define MIKMOD_STATE_ENDING	(3)
#define MIKMOD_STATE_QUIT	(4)

static int mikmod_state = MIKMOD_STATE_INIT;

#define SPINNER_VOLUME	(0)
#define SPINNER_SCRUB	(1)
#define SPINNER_OFF	(2)

static int spinner_mode = SPINNER_OFF;


//static GR_TIMER_ID	mikmod_timer = 0;
static GR_GC_ID		mikmod_gc = 0;
static GR_WINDOW_ID	mikmod_bufwid = 0;
static GR_WINDOW_ID	mikmod_wid = 0;
static GR_SCREEN_INFO	mikmod_screen_info;
static int 		mikmod_height = 0;

static char mikmod_playlist_filename[128];

static void mikmod_shutdown( void )
{
    if( mikmod_state == MIKMOD_STATE_UNDEFINED ) return;
    //GrDestroyTimer( mikmod_timer );
#ifdef IPOD
    MikMod_Exit();
#endif
    GrDestroyGC( mikmod_gc );
    pz_close_window( mikmod_wid );
    GrDestroyWindow( mikmod_bufwid );
    mikmod_state = MIKMOD_STATE_UNDEFINED;
}



static void mikmod_handle_playback( void )
{
#ifdef IPOD
    switch( mikmod_state )
    {
    case( MIKMOD_STATE_INIT ):
	mikmod_state = MIKMOD_STATE_PREP;
	break;

    case( MIKMOD_STATE_PREP ):
	memset(filename,0,sizeof(filename));
	memset(archive,0,sizeof(archive));
	mikmod_quit=( ipod_get_setting( SHUFFLE )?
		(PL_GetRandom(&playlist,filename,archive))!=0 :
		(PL_GetNext(&playlist,filename,archive))!=0 );
	if(filename[0]==0) {mikmod_quit=1;}
	if( mikmod_quit ) {
	    mikmod_state = MIKMOD_STATE_QUIT;
	} else {
	    /* load the module */
	    playfile=MA_dearchive(archive,filename);
	    if((mf=MikMod_LoadSong(playfile,cfg_maxchn)) == NULL)
	    {
		free( playfile );
	    } else {
		mf->extspd  = 1;	/* extended speed enable */
		mf->panflag = 0;	/* panning enable */

		if( ipod_get_setting( REPEAT ) == 1)
		{
		    mf->loop    = 1;	/* autoloop enable */
		} else {
		    mf->loop    = 0;	/* autoloop disable */
		}

		Player_Start(mf);

		next = 0;
		mikmod_quit = 0;
	    }
	    mikmod_state = MIKMOD_STATE_PLAYING;
	}
	spinner_mode = SPINNER_OFF;

	break;

    case( MIKMOD_STATE_PLAYING ):
	if(Player_Active() && !next && mf->numpos<256)
	{
	    MikMod_Update();
	} else {
	    mikmod_state = MIKMOD_STATE_ENDING;
	}

	break;

    case( MIKMOD_STATE_ENDING ):
	Player_Stop();		/* stop playing */
	MikMod_FreeSong(mf);	/* and free the module */
	mikmod_state = MIKMOD_STATE_PREP;
	break;

    case( MIKMOD_STATE_QUIT ):
	mikmod_shutdown();
	break;

    default:
	break;
    }
#endif
}

/* displays "loading" or "unsupported" */
void mikmod_display_info_screen( int loading )
{
	int bx, xpos, x2;
	char * theText;

	int LOAD_Y = mikmod_height - 35;
#define LOAD_TEXT "loading..."
#define UNSUP_TEXT "UNSUPPORTED";

	if( loading ) theText = LOAD_TEXT;
	else theText = UNSUP_TEXT;

	xpos = (screen_info.cols>>1) 
		- (vector_string_pixel_width( theText, 1, 2 )>>1);

	/* display somethign other than a blank screen... */
	GrSetGCForeground( mikmod_gc, LTGRAY );
	vector_render_string(  mikmod_bufwid, mikmod_gc,
			theText, 1, 2, xpos+1, LOAD_Y+8 );
	vector_render_string(  mikmod_bufwid, mikmod_gc,
			theText, 1, 2, xpos, LOAD_Y+8 );

	GrSetGCForeground( mikmod_gc, BLACK );
	vector_render_string(  mikmod_bufwid, mikmod_gc,
			theText, 1, 2, xpos, LOAD_Y+7 );

	/* top line... */
	GrLine( mikmod_bufwid, mikmod_gc,
		0, LOAD_Y - 3, screen_info.cols, LOAD_Y - 3 );

	bx = vector_pixel_height( 2 );

	/* bottom line... */
/*
	GrLine( mikmod_bufwid, mikmod_gc,
		0, LOAD_Y + 4 + bx,
		screen_info.cols, LOAD_Y + 4 + bx );
*/

	/* more informational text */
	GrSetGCForeground( mikmod_gc, LTGRAY );
	if( screen_info.cols < 160 )
	{
	    for( x2=0 ; cprt1MINI[x2][0] != '\0' ; x2++ )
		vector_render_string(  mikmod_bufwid, mikmod_gc,
			    cprt1MINI[x2], 1, 1, 3, 1+(x2*11) );
	} else {
	    for( x2=0 ; cprt1[x2][0] != '\0' ; x2++ )
		vector_render_string(  mikmod_bufwid, mikmod_gc,
				cprt1[x2], 1, 1,
				2 + ((screen_info.cols > 160)?35:0),
				8+(x2*11) );
	}
}

typedef struct {
    int patno;
    int patpos;
    int numrow;
    int sngpos;
    int numpos;
    int sngspd;
    int bpm;
} SP;

#ifdef IPOD
static SP sp = {1, 1, 1, 1, 1};
#endif


int mikmod_voices_playing();


static void mikmod_redraw( void )
{
    static int entries = 0;
#ifdef IPOD
    int xpos, x2, x3;
    char * fnf;
    int width;
    static int last_patpos = -1;
    static int last_sngpos = -1;
    static char posbuf[128];
    char buf[128];
    char arc[255];
#endif
    /* glue the sliders to the bottom of the screen */
    int VOL_Y = mikmod_height - 35; /* volume */
    int PAT_Y = mikmod_height - 20; /* pattern in song */
    int POS_Y = mikmod_height - 8; /* position in pattern */



    entries++;

    // draw stuff.
    GrSetGCForeground( mikmod_gc, WHITE );
    GrFillRect( mikmod_bufwid, mikmod_gc, 0, 0, 
		screen_info.cols, mikmod_height );

#ifdef IPOD
    if( mikmod_state == MIKMOD_STATE_PLAYING )
    {
	GrSetGCForeground( mikmod_gc, BLACK );

	buf[0] = arc[0] = '\0';

	if( playlist.numused )
	    PL_GetCurrent(&playlist,buf,arc);

	fnf = buf;
	for( x2 = 0 ; x2<128 && (buf[x2] != '\0') ; x2++ )
	{
	    if(    (buf[x2] == '\\')
		|| (buf[x2] == '/') )
	    fnf = &buf[x2+1];
	}

	/* filename */
	vector_render_string( mikmod_bufwid, mikmod_gc,
			  fnf, 1, 1, 0, 2 );

	/* song title */
	vector_render_string( mikmod_bufwid, mikmod_gc,
			  mf->songname, 1, 1, 0, 18 );

	/* song position */
        if(    ( last_patpos != mf->patpos )
            || ( last_sngpos != mf->sngpos ) )
        {
	    //if( mikmod_autoloop_kill && ( mf->sngpos < last_sngpos ))
	    //{
	//	next=1;
	 //   }

            last_patpos = mf->patpos;
            last_sngpos = mf->sngpos;

	    /* put in the latency buffer here */

	    sp.patno  = mf->positions[mf->sngpos];
	    sp.patpos = (mf->patpos == 65535)?0:mf->patpos;
	    sp.numrow = mf->numrow;
	    sp.sngpos = mf->sngpos;
	    sp.numpos = mf->numpos;
	    sp.sngspd = mf->sngspd;
	    sp.bpm    = mf->bpm;

	    sprintf( posbuf, " %02d/%02d %02d %3d:%d  %d/%d",
		    sp.sngpos, sp.numpos-1,
		    sp.patno,
		    sp.patpos, sp.numrow-1,
		    sp.bpm, sp.sngspd );
        }
	vector_render_string( mikmod_bufwid, mikmod_gc,
		      posbuf, 1, 1, 0, 36 );

	/* voices used: */ /* put in the top right */
	sprintf( buf, "%3d/%d", mikmod_voices_playing(), md_numchn );
	vector_render_string_right( mikmod_bufwid, mikmod_gc,
		      buf, 1, 1, screen_info.cols, 2 );
/*

This uses the following code added to virtch.c:

+133:
int mikmod_voices_playing( void )
{
    int t, count = 0;

    if( !vinf || vc_softchn == 0) return( 0 );
    for(t=0; t<vc_softchn; t++) {
        if( vinf[t].active == 1 ) count++;
    }
    return( count );
}

*/

/*  don't really need to have the spinner mode spelled out...
	if( spinner_mode == SPINNER_SCRUB )
	    vector_render_string( mikmod_bufwid, mikmod_gc,
		      "Spinner: Scrub", 1, 1, 0, 48 );
	else if( spinner_mode == SPINNER_VOLUME )
	    vector_render_string( mikmod_bufwid, mikmod_gc,
		      "Spinner: Volume", 1, 1, 0, 48 );
	else if( spinner_mode == SPINNER_OFF )
	    vector_render_string( mikmod_bufwid, mikmod_gc,
		      "Spinner: -", 1, 1, 0, 48 );
*/

	/* draw the volume slider */
	if( spinner_mode == SPINNER_VOLUME )
	{
	    GrSetGCForeground( mikmod_gc, LTGRAY );

	    GrRect( mikmod_bufwid, mikmod_gc, 
				0, VOL_Y-6, screen_info.cols, 12 );
	}

	xpos = ((mikmod_dsp_vol & 0x0ff) * (screen_info.cols-18))/100;

	/* track */
	GrSetGCForeground( mikmod_gc, BLACK );
	GrLine( mikmod_bufwid, mikmod_gc,
				6, VOL_Y-1, screen_info.cols-7, VOL_Y-1 );
	GrLine( mikmod_bufwid, mikmod_gc, 
				6, VOL_Y+1, screen_info.cols-7, VOL_Y+1 );
	GrPoint( mikmod_bufwid, mikmod_gc, 6, VOL_Y  );
	GrPoint( mikmod_bufwid, mikmod_gc, screen_info.cols-7, VOL_Y );

	/* handle */
	if( spinner_mode == SPINNER_VOLUME )
	    GrSetGCForeground( mikmod_gc, LTGRAY );
	GrFillRect( mikmod_bufwid, mikmod_gc, 6+xpos, VOL_Y-6, 6, 12 );
	GrSetGCForeground( mikmod_gc, BLACK );
	GrRect( mikmod_bufwid, mikmod_gc, 6+xpos, VOL_Y-6, 6, 12 );


	/* pattern position */
	xpos = ( sp.sngpos * (screen_info.cols-12)) / (sp.numpos);
	width = (screen_info.cols-18)/(sp.numpos-1);
	if( width < 1 ) width = 1;

	/* container*/
	GrSetGCForeground( mikmod_gc, GRAY );
	GrLine( mikmod_bufwid, mikmod_gc,
				6, PAT_Y-6, screen_info.cols-7, PAT_Y-6 );
	GrLine( mikmod_bufwid, mikmod_gc,
				6, PAT_Y+6, screen_info.cols-7, PAT_Y+6 );
	
#define R_W 	(2)
#define	R_H	(6)
	/* line */
	GrSetGCForeground( mikmod_gc, LTGRAY );
	GrLine( mikmod_bufwid, mikmod_gc, 6, PAT_Y, 6+xpos, PAT_Y);
	/* indicator */
	GrSetGCForeground( mikmod_gc, BLACK );
	GrFillRect( mikmod_bufwid, mikmod_gc, 6+xpos, PAT_Y-5, width, 11 );

	if( spinner_mode == SPINNER_SCRUB )
	{
	    GrSetGCForeground( mikmod_gc, LTGRAY );
	    GrFillRect( mikmod_bufwid, mikmod_gc, 6+xpos, PAT_Y-4, width, 9 );
	}

	GrSetGCForeground( mikmod_gc, BLACK );
	GrLine( mikmod_bufwid, mikmod_gc, 6+xpos-1, PAT_Y-4,
					  6+xpos-1, PAT_Y+4);
	GrLine( mikmod_bufwid, mikmod_gc, 6+xpos+width, PAT_Y-4,
					  6+xpos+width, PAT_Y+4);


	/* current position in the pattern */
	xpos = ( ((mf->patpos == 65535)?0:mf->patpos) *
			(screen_info.cols-12)) / (mf->numrow);
	width = (screen_info.cols-18)/(mf->numrow-1);
	if( width < 1 ) width = 1;


	/* indicator */
	/* line */
	GrSetGCForeground( mikmod_gc, LTGRAY );
	GrLine( mikmod_bufwid, mikmod_gc, 6, POS_Y, 6+xpos, POS_Y);

	/* container ticks */
	GrSetGCForeground( mikmod_gc, LTGRAY );
	for( x2 = 0; x2 < 5 ; x2++ )
	{
	    x3 = ( (screen_info.cols - 13) * x2 )>>2;
	    GrLine( mikmod_bufwid, mikmod_gc, 
				6 + x3, POS_Y-2, 6 + x3, POS_Y+2  );
	}

	/* handle */
	GrSetGCForeground( mikmod_gc, BLACK );
	GrFillRect( mikmod_bufwid, mikmod_gc, 6+xpos, POS_Y-5, width, 11 );
	GrLine( mikmod_bufwid, mikmod_gc, 6+xpos-1, POS_Y-4,
					  6+xpos-1, POS_Y+4);
	GrLine( mikmod_bufwid, mikmod_gc, 6+xpos+width, POS_Y-4,
					  6+xpos+width, POS_Y+4);

	/* container */
	GrSetGCForeground( mikmod_gc, GRAY );
	GrLine( mikmod_bufwid, mikmod_gc,
				6, POS_Y+6, screen_info.cols-7, POS_Y+6 );


	/* draw the scrub indicator if we're in that mode... */
	if( spinner_mode == SPINNER_SCRUB )
	{
	    GrSetGCForeground( mikmod_gc, LTGRAY );
	    GrRect( mikmod_bufwid, mikmod_gc, 
				//6, PAT_Y-6, screen_info.cols-12, 25 );
				0, PAT_Y-6, screen_info.cols, 25 );
	}

    } else {
	mikmod_display_info_screen( 1 );
    }
#else
    mikmod_display_info_screen( 0 );
#endif

    /* blit it into place */
    GrCopyArea( mikmod_wid,
                   mikmod_gc,
                   0, 0,
                   screen_info.cols, mikmod_height,
                   mikmod_bufwid,
                   0, 0,
                   MWROP_SRCCOPY);
}


static void mikmod_do_draw( void )
{
    pz_draw_header( "MikMod" );
    mikmod_redraw();
}


void mikmod_volume_down( void )
{
#ifdef __linux__
	if( mikmod_mixer_fd >= 0 ) {
	    int vol = mikmod_dsp_vol & 0xff;
	    if( vol > 0 ) {
		vol--;
		vol = mikmod_dsp_vol = vol << 8 | vol;
	    }
	}
#endif
}


void mikmod_volume_up( void )
{
#ifdef __linux__
	if( mikmod_mixer_fd >= 0 ) {
	    int vol = mikmod_dsp_vol & 0xff;
	    if( vol < 100 ) {
		    vol++;
		    vol = mikmod_dsp_vol = vol << 8 | vol;
	    }
	}
#endif
}


static int mikmod_handle_event( GR_EVENT * event )
{
    switch( event->type )
    {
	case( GR_EVENT_TYPE_TIMER ):
	    break;

	case( GR_EVENT_TYPE_KEY_DOWN ): 
	    switch( event->keystroke.ch )
	    {
		case( IPOD_BUTTON_ACTION ): /* wheel button */
		    /* cycle spinner mode */
		    if( spinner_mode == SPINNER_VOLUME )
			spinner_mode = SPINNER_SCRUB;
		    else if( spinner_mode == SPINNER_SCRUB )
			spinner_mode = SPINNER_OFF;
		    else if( spinner_mode == SPINNER_OFF )
			spinner_mode = SPINNER_VOLUME;
		    break;

		case( IPOD_BUTTON_PLAY ): /* play/pause */
		case( IPOD_REMOTE_PLAY ): /* play/pause on the remote */
#ifdef IPOD
		    Player_TogglePause();
#endif
		    break;

		case( IPOD_BUTTON_REWIND ): /* rewind */
		case( IPOD_REMOTE_REWIND ): /* rewind on the remote */
#ifdef IPOD
		    PL_GetPrev(&playlist,filename,archive);
		    PL_GetPrev(&playlist,filename,archive);
		    next=1;
		    spinner_mode = SPINNER_OFF;
#endif
		    break;

		case( IPOD_BUTTON_FORWARD ): /* forward */
		case( IPOD_REMOTE_FORWARD ): /* forward on the remote */
#ifdef IPOD
		    next=1;
		    spinner_mode = SPINNER_OFF;
#endif
		    break;

		case( IPOD_WHEEL_CLOCKWISE ): /* wheel clockwise */
		    if( spinner_mode == SPINNER_SCRUB ) {
#ifdef IPOD
			MP_NextPosition(mf);
#endif
		    } else if( spinner_mode == SPINNER_VOLUME ) {
			mikmod_volume_up();
		    }
		    break;


		case( IPOD_WHEEL_ANTICLOCKWISE ): /* wheel anticlockwise */
		    if( spinner_mode == SPINNER_SCRUB ) {
#ifdef IPOD
			/* this following doesn't always work... */
			//if( sp.sngpos != 0 ) /* go only back to start of song */
			    MP_PrevPosition(mf);
#endif
		    } else if( spinner_mode == SPINNER_VOLUME ) {
			mikmod_volume_down();
		    }
		    break;


		case( IPOD_REMOTE_VOL_UP ):	/* volume up on the remote */
		    mikmod_volume_up();
		    break;

		case( IPOD_REMOTE_VOL_DOWN ):/* volume down on the remote */
		    mikmod_volume_down();
		    break;


		case( IPOD_BUTTON_MENU ): /* menu button */
		    mikmod_state = MIKMOD_STATE_QUIT;
		    return( 1 );

		default:
		    break;
	    }
	    break;

	default:
	    break;
    }
    return( 1 );
}


static void mikmod_playLoop( void )
{
    int pollit = 1;
    GR_EVENT event;

    while(   (mikmod_state != MIKMOD_STATE_QUIT) 
	  && (mikmod_state != MIKMOD_STATE_UNDEFINED)
	  && !mikmod_quit )
    {
	pollit = 1;
	if( GrPeekEvent( &event ) )
	{
	    GrGetNextEventTimeout( &event, 60 );
	    if( event.type != GR_EVENT_TYPE_TIMEOUT )
	    {
		pz_event_handler( &event );
		pollit = 0;
	    }
	}

	if( pollit && !mikmod_quit )
	{
	    if( !mikmod_quit )
		mikmod_redraw( );

#ifdef __linux__
	    if( mikmod_mixer_fd >= 0 ) {
		ioctl( mikmod_mixer_fd, SOUND_MIXER_WRITE_PCM, &mikmod_dsp_vol);
	    }
#endif
	    mikmod_handle_playback( );
	}
    }
    mikmod_shutdown();
}

void new_mikmod_player(char * mik_file)
{
    char buf[80];
    FILE * fp;

    /* set up the OSS mixer, if we're on linux */
#ifdef __linux__
    mikmod_mixer_fd = open( "/dev/mixer", O_RDWR );
    if( mikmod_mixer_fd >= 0 ) {
	ioctl( mikmod_mixer_fd, SOUND_MIXER_READ_PCM, &mikmod_dsp_vol );
    }
#endif

    strncpy( mikmod_playlist_filename, mik_file, 128 );

    fp = fopen( mik_file, "r" );
    if( !fp )
    {
	sprintf( buf, "Mikmod: %s\nUnable to open file.", mik_file );
	new_message_window( buf );
	return;
    }

    GrGetScreenInfo( &mikmod_screen_info );
    mikmod_gc = GrNewGC();
    mikmod_height = screen_info.rows - (HEADER_TOPLINE + 1);

    mikmod_bufwid = GrNewPixmap( screen_info.cols, mikmod_height, NULL );
    mikmod_wid = pz_new_window(      0, HEADER_TOPLINE + 1,
                                     screen_info.cols,
                                     mikmod_height,
                                     mikmod_do_draw,
                                     mikmod_handle_event );

    GrSelectEvents( mikmod_wid,
                         GR_EVENT_MASK_KEY_DOWN
                       | GR_EVENT_MASK_KEY_UP
		       | GR_EVENT_MASK_TIMER
                       | GR_EVENT_MASK_EXPOSURE );

    GrMapWindow( mikmod_wid );

    mikmod_do_draw();

    mikmod_init();
#ifdef IPOD
    PL_Load( &playlist, mik_file );
#endif

//    mikmod_timer = GrCreateTimer( mikmod_wid, 10 );
    mikmod_state = MIKMOD_STATE_INIT;
    spinner_mode = SPINNER_OFF;

    mikmod_playLoop();
#ifdef __linux__
    close( mikmod_mixer_fd );
#endif
}

void new_mikmod_player_song( char * filename )
{
    new_message_window( "Unsupported" );
}

void new_mikmod_window(void)
{   
    new_mikmod_player( "/default.mik" );
}
