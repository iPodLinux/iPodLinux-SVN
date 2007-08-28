/******************************************************************/
/*                                                                */
/* Module:       jb_jtag.h                                        */
/*                                                                */
/* Descriptions: Manages JTAG State Machine (JSM), loading of     */
/*               JTAG instructions                                */
/*                                                                */
/* Revisions:    0.1 02/12/07                                     */
/*                                                                */
/******************************************************************/
/* $Id: jb_jtag.h,v 1.1 2002/09/09 15:02:53 gerg Exp $ */

#ifndef JB_JTAG_H
#define JB_JTAG_H
__inline__ int  AdvanceJSM(int);
void PrintJS(void);
void SetupChain(int, int, int *,int);
__inline__ int  LoadJI(int,int,int *);
int  Ji_Extest(int, int *);
int  Ji_Program(int,int *);
int  Ji_Startup(int, int *);
int  Ji_Checkstatus(int,int *);
int  Ji_Sample(int, int *);
int  Ji_Idcode(int, int *);
int  Ji_Usercode(int, int *);
int  Ji_Intest(int, int *);
int  Ji_Regscan(int, int *);
int  Ji_User0(int, int *);
int  Ji_User1(int, int *);
int  Ji_Bypass(int, int *);
void Js_Reset(void);
void Js_Runidle(void);
int  Js_Shiftdr(void);
int  Js_Updatedr(void);
#endif
