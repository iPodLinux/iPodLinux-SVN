/******************************************************************/
/*                                                                */
/* Module:       jb_const.h                                       */
/*                                                                */
/* Descriptions: Contain USER and PROGRAM variables use by        */
/*               jblaster.c                                       */
/*                                                                */
/* Revisions:    1.0 02/22/02                                     */
/*                                                                */
/******************************************************************/
/* $Id: jb_const.h,v 1.1 2002/09/09 15:02:53 gerg Exp $ */

#ifndef JB_CONST_H
#define JB_CONST_H

/* User Variables */

#define MAX_DEVICE_ALLOW 10
#define MAX_CONFIG_COUNT 3
#define INIT_COUNT       200

/* JTAG Configuration Signals */
#define SIG_TCK  0 /* TCK */
#define SIG_TMS  1 /* TMS */
#define SIG_TDI  2 /* TDI */
#define SIG_TDO  3 /* TDO */

/* Chain Description File (CDF) records string length */
#define CDF_IDCODE_LEN 32
#define CDF_PNAME_LEN  20
#define CDF_PATH_LEN   50
#define CDF_FILE_LEN   20

extern const int JI_PROGRAM;/*      = 0x002L;*/
extern const int JI_BYPASS;/*       = 0x3FFL;*/
extern const int JI_CHECK_STATUS;/* = 0x004L;*/
extern const int JI_STARTUP;/*      = 0x003L;*/

/* Version Number */
const char VERSION[4] = "0.1";

int MAX_JTAG_INIT_CLOCK[8] = {
	30,   /* ACEX      */
	175,  /* APEX II   */
	175,  /* APEX 20K  */
	175,  /* APEX 20KC */
	175,  /* APEX 20KE */
	30,   /* FLEX 10KE */
	175,  /* MERCURY   */
	175   /* STRATIX   */
};

int device_family=0; /* Device Family, check jb_device.h for detail */

/* a structure (list) that stores the records of a device */
struct list{
	int   idcode;
	int   jseq_max;
	int   jseq_conf_done;
	char  action;
	char  partname[CDF_PNAME_LEN];
	unsigned char *buffer;
	long  buffer_len;
	char  compress;
	int   inst_len;
};

#endif
