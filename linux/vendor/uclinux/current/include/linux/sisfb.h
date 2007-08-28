#ifndef _LINUX_SISFB
#define _LINUX_SISFB

#include <linux/spinlock.h>

#include <asm/ioctl.h>
#include <asm/types.h>

/* TW: vbflags */
#define CRT2_DEFAULT            0x00000001
#define CRT2_LCD                0x00000002  /* TW: Never change the order of the CRT2_XXX entries */
#define CRT2_TV                 0x00000004  /*     (see SISCycleCRT2Type())                       */
#define CRT2_VGA                0x00000008
#define CRT2_ENABLE		(CRT2_LCD | CRT2_TV | CRT2_VGA)
#define VB_DISPTYPE_DISP2	CRT2_ENABLE
#define VB_DISPTYPE_CRT2	CRT2_ENABLE
#define TV_NTSC                 0x00000010
#define TV_PAL                  0x00000020
#define TV_HIVISION             0x00000040
#define TV_HIVISION_LV          0x00000080
#define TV_TYPE                 (TV_NTSC | TV_PAL | TV_HIVISION | TV_HIVISION_LV)
#define TV_AVIDEO               0x00000100
#define TV_SVIDEO               0x00000200
#define TV_SCART                0x00000400
#define TV_INTERFACE            (TV_AVIDEO | TV_SVIDEO | TV_SCART | TV_CHSCART | TV_CHHDTV)
#define VB_USELCDA		0x00000800
#define TV_PALM                 0x00001000
#define TV_PALN                 0x00002000
#define TV_CHSCART              0x00008000
#define TV_CHHDTV               0x00010000
#define VGA2_CONNECTED          0x00040000
#define VB_DISPTYPE_CRT1	0x00080000  	/* CRT1 connected and used */
#define VBDISPTYPE_DISP1	VB_DISPTYPE_CRT1
#define VB_301                  0x00100000	/* Video bridge type */
#define VB_301B                 0x00200000
#define VB_302B                 0x00400000
#define VB_30xBDH		0x00800000      /* 30xB DH version (w/o LCD support) */
#define VB_LVDS                 0x01000000
#define VB_CHRONTEL             0x02000000
#define VB_301LV                0x04000000  	
#define VB_302LV                0x08000000  	
#define VB_301C			0x10000000     
#define VB_VIDEOBRIDGE		(VB_301|VB_301B|VB_301C|VB_302B|VB_301LV|VB_302LV| \
				 VB_LVDS|VB_CHRONTEL)
#define VB_SISBRIDGE            (VB_301|VB_301B|VB_302B|VB_301LV|VB_302LV)
#define VB_SINGLE_MODE          0x20000000   	 /* CRT1 or CRT2; determined by VB_DISPTYPE_CRTx */
#define VB_DISPMODE_SINGLE	VB_SINGLE_MODE 	 
#define VB_MIRROR_MODE		0x40000000   	 /* CRT1 + CRT2 identical (mirror mode) */
#define VB_DISPMODE_MIRROR	VB_MIRROR_MODE   
#define VB_DUALVIEW_MODE	0x80000000   	 /* CRT1 + CRT2 independent (dual head mode) */
#define VB_DISPMODE_DUAL	VB_DUALVIEW_MODE 
#define VB_DISPLAY_MODE         (VB_SINGLE_MODE | VB_MIRROR_MODE | VB_DUALVIEW_MODE) 


/* entries for disp_state - deprecated as of 1.6.02 */
#define DISPTYPE_CRT1       0x00000008L
#define DISPTYPE_CRT2       0x00000004L
#define DISPTYPE_LCD        0x00000002L
#define DISPTYPE_TV         0x00000001L
#define DISPTYPE_DISP1      DISPTYPE_CRT1
#define DISPTYPE_DISP2      (DISPTYPE_CRT2 | DISPTYPE_LCD | DISPTYPE_TV)
#define DISPMODE_SINGLE	    0x00000020L
#define DISPMODE_MIRROR	    0x00000010L
#define DISPMODE_DUALVIEW   0x00000040L

/* Deprecated as of 1.6.02 - use vbflags instead */
#define HASVB_NONE      	0x00
#define HASVB_301       	0x01
#define HASVB_LVDS      	0x02
#define HASVB_TRUMPION  	0x04
#define HASVB_LVDS_CHRONTEL	0x10
#define HASVB_302       	0x20
#define HASVB_303       	0x40
#define HASVB_CHRONTEL  	0x80

/* TW: *Never* change the order of the following enum */
typedef enum _SIS_CHIP_TYPE {
	SIS_VGALegacy = 0,
	SIS_300,
	SIS_630,
	SIS_540,
	SIS_730, 
	SIS_315H,
	SIS_315,
	SIS_315PRO,
	SIS_550,
	SIS_650,
	SIS_740,
	SIS_330,
	SIS_660,
	SIS_760,
	MAX_SIS_CHIP
} SIS_CHIP_TYPE;

typedef enum _VGA_ENGINE {
	UNKNOWN_VGA = 0,
	SIS_300_VGA,
	SIS_315_VGA,
} VGA_ENGINE;

typedef enum _TVTYPE {
	TVMODE_NTSC = 0,
	TVMODE_PAL,
	TVMODE_HIVISION,
	TVMODE_TOTAL
} SIS_TV_TYPE;

typedef enum _TVPLUGTYPE {
	TVPLUG_Legacy = 0,
	TVPLUG_COMPOSITE,
	TVPLUG_SVIDEO,
	TVPLUG_SCART,
	TVPLUG_TOTAL
} SIS_TV_PLUG;

struct sis_memreq {
	unsigned long offset;
	unsigned long size;
};

struct mode_info {
	int    bpp;
	int    xres;
	int    yres;
	int    v_xres;
	int    v_yres;
	int    org_x;
	int    org_y;
	unsigned int  vrate;
};

struct ap_data {
	struct mode_info minfo;
	unsigned long iobase;
	unsigned int  mem_size;
	unsigned long disp_state;  /* deprecated */  	
	SIS_CHIP_TYPE chip;
	unsigned char hasVB;
	SIS_TV_TYPE TV_type;	   /* deprecated */
	SIS_TV_PLUG TV_plug;	   /* deprecated */
	unsigned long version;
	unsigned long vbflags;	   /* replaces deprecated entries above */
	unsigned long currentvbflags;
	char reserved[248];
};

struct video_info {
	int           chip_id;
	unsigned int  video_size;
	unsigned long video_base;
	char  *       video_vbase;
	unsigned long mmio_base;
	char  *       mmio_vbase;
	unsigned long vga_base;
	unsigned long mtrr;
	unsigned long heapstart;

	int    video_bpp;
	int    video_cmap_len;
	int    video_width;
	int    video_height;
	int    video_vwidth;
	int    video_vheight;
	int    org_x;
	int    org_y;
	int    video_linelength;
	unsigned int refresh_rate;

	unsigned long disp_state;		/* DEPRECATED */
	unsigned char hasVB;			/* DEPRECATED */
	unsigned char TV_type;			/* DEPRECATED */
	unsigned char TV_plug;			/* DEPRECATED */

	SIS_CHIP_TYPE chip;
	unsigned char revision_id;

        unsigned short DstColor;		/* TW: For 2d acceleration */
	unsigned long  SiS310_AccelDepth;
	unsigned long  CommandReg;

	spinlock_t     lockaccel;

        unsigned int   pcibus;
	unsigned int   pcislot;
	unsigned int   pcifunc;

	int 	       accel;

	unsigned short subsysvendor;
	unsigned short subsysdevice;
	
	unsigned long  vbflags;			/* Replacing deprecated stuff from above */
	unsigned long  currentvbflags;
	
	int    current_bpp;
	int    current_width;
	int    current_height;
	int    current_htotal;
	int    current_vtotal;
	__u32  current_pixclock;
	int    current_refresh_rate;

	char reserved[200];
};


/* TW: Addtional IOCTL for communication sisfb <> X driver                 */
/*     If changing this, vgatypes.h must also be changed (for X driver)    */

/* TW: ioctl for identifying and giving some info (esp. memory heap start) */
#define SISFB_GET_INFO	  	_IOR('n',0xF8,sizeof(__u32))

#define SISFB_GET_VBRSTATUS  	_IOR('n',0xF9,sizeof(__u32))

/* TW: Structure argument for SISFB_GET_INFO ioctl  */
typedef struct _SISFB_INFO sisfb_info, *psisfb_info;

struct _SISFB_INFO {
	unsigned long sisfb_id;         /* for identifying sisfb */
#ifndef SISFB_ID
#define SISFB_ID	  0x53495346    /* Identify myself with 'SISF' */
#endif
 	int    chip_id;			/* PCI ID of detected chip */
	int    memory;			/* video memory in KB which sisfb manages */
	int    heapstart;               /* heap start (= sisfb "mem" argument) in KB */
	unsigned char fbvidmode;	/* current sisfb mode */
	
	unsigned char sisfb_version;
	unsigned char sisfb_revision;
	unsigned char sisfb_patchlevel;

	unsigned char sisfb_caps;	/* Sisfb capabilities */

	int    sisfb_tqlen;		/* turbo queue length (in KB) */

	unsigned int sisfb_pcibus;      /* The card's PCI ID */
	unsigned int sisfb_pcislot;
	unsigned int sisfb_pcifunc;

	unsigned char sisfb_lcdpdc;	/* PanelDelayCompensation */
	
	unsigned char sisfb_lcda;	/* Detected status of LCDA for low res/text modes */
	
	unsigned long sisfb_vbflags;
	unsigned long sisfb_currentvbflags;

	int sisfb_scalelcd;
	unsigned long sisfb_specialtiming;

	char reserved[219]; 		/* for future use */
};

#ifdef __KERNEL__
extern struct video_info ivideo;

extern void sis_malloc(struct sis_memreq *req);
extern void sis_free(unsigned long base);
extern void sis_dispinfo(struct ap_data *rec);
#endif
#endif
