/******************************************************************/
/*                                                                */
/* Module:       jb_jtag.c                                        */
/*                                                                */
/* Descriptions: Manages JTAG State Machine (JSM), loading of     */
/*               JTAG instructions                                */
/*                                                                */
/* Revisions:    0.1 02/12/07                                     */
/*                                                                */
/******************************************************************/
/* $Id: jb_jtag.c,v 1.1 2002/09/09 15:02:53 gerg Exp $ */

//#include <stdio.h>
//#include <string.h>
#include <asm/MC68VZ328.h>
#include "jb_jtag.h"
#include "jb_io.h"

#define MAX_JS_CHAR_COUNT 10
#define JSM_RESET_COUNT 5

/* JTAG Configuration Signals */
#define J_SIG_TCK  0 /* TCK */
#define J_SIG_TMS  1 /* TMS */
#define J_SIG_TDI  2 /* TDI */
#define J_SIG_TDO  3 /* TDO */

/* States of JTAG State Machine */
#define JS_RESET         0
#define JS_RUNIDLE       1
#define JS_SELECT_IR     2
#define JS_CAPTURE_IR    3
#define JS_SHIFT_IR      4
#define JS_EXIT1_IR      5
#define JS_PAUSE_IR      6
#define JS_EXIT2_IR      7
#define JS_UPDATE_IR     8
#define JS_SELECT_DR     9
#define JS_CAPTURE_DR    10
#define JS_SHIFT_DR      11
#define JS_EXIT1_DR      12
#define JS_PAUSE_DR      13
#define JS_EXIT2_DR      14
#define JS_UPDATE_DR     15
#define JS_UNDEFINE      16

/* JTAG State Machine */
const int JSM[16][2] = {
/*-State-      -mode= '0'-    -mode= '1'- */
/*RESET     */ {JS_RUNIDLE,   JS_RESET    },
/*RUNIDLE   */ {JS_RUNIDLE,   JS_SELECT_DR},
/*SELECTIR  */ {JS_CAPTURE_IR,JS_RESET    },
/*CAPTURE_IR*/ {JS_SHIFT_IR,  JS_EXIT1_IR },
/*SHIFT_IR  */ {JS_SHIFT_IR,  JS_EXIT1_IR },
/*EXIT1_IR  */ {JS_PAUSE_IR,  JS_UPDATE_IR},
/*PAUSE_IR  */ {JS_PAUSE_IR,  JS_EXIT2_IR },
/*EXIT2_IR  */ {JS_SHIFT_IR,  JS_UPDATE_IR},
/*UPDATE_IR */ {JS_RUNIDLE,   JS_SELECT_DR},
/*SELECT_DR */ {JS_CAPTURE_DR,JS_SELECT_IR},
/*CAPTURE_DR*/ {JS_SHIFT_DR,  JS_EXIT1_DR },
/*SHIFT_DR  */ {JS_SHIFT_DR,  JS_EXIT1_DR },
/*EXIT1_DR  */ {JS_PAUSE_DR,  JS_UPDATE_DR},
/*PAUSE_DR  */ {JS_PAUSE_DR,  JS_EXIT2_DR },
/*EXIT2_DR  */ {JS_SHIFT_DR,  JS_UPDATE_DR},
/*UPDATE_DR */ {JS_RUNIDLE,   JS_SELECT_DR}
};

/* JTAG Instructions */
const int JI_EXTEST       = 0x000;
const int JI_PROGRAM      = 0x002;
const int JI_STARTUP      = 0x003;
const int JI_CHECK_STATUS = 0x004;
const int JI_SAMPLE       = 0x005;
const int JI_IDCODE       = 0x006;
const int JI_USERCODE     = 0x007;
const int JI_INTEST       = 0x008;
const int JI_REGSCAN      = 0x009;
const int JI_USER0        = 0x00C;
const int JI_USER1        = 0x00E;
const int JI_BYPASS       = 0x3FF;

const char JS_NAME[][MAX_JS_CHAR_COUNT+1] = {
"RESET",
"RUN/IDLE",
"SELECT_IR",
"CAPTURE_IR",
"SHIFT_IR",
"EXIT1_IR",
"PAUSE_IR",
"EXIT2_IR",
"UPDATE_IR",
"SELECT_DR",
"CAPTURE_DR",
"SHIFT_DR",
"EXIT1_DR",
"PAUSE_DR",
"EXIT2_DR",
"UPDATE_DR",
"UNDEFINE" };

struct states
{
	int state;
} jtag;

/******************************************************************/
/* Name:         Js_Reset                                         */
/*                                                                */
/* Parameters:   None.                                            */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Reset the JSM by issuing JSM_RESET_COUNT of clock*/
/*               with the TMS at HIGH.                            */
/*                                                                */
/******************************************************************/
void Js_Reset()
{
	int i;

	for(i=0;i<JSM_RESET_COUNT;i++)
		AdvanceJSM(1);
}

/******************************************************************/
/* Name:         Runidle                                          */
/*                                                                */
/* Parameters:   None.                                            */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: If the current JSM is not at UPDATE_DR or        */
/*               UPDATE_IR state, RESET JSM and move to RUNIDLE,  */
/*               if it is, clock once with TMS LOW and move to    */
/*               RUNIDLE.                                         */
/*                                                                */
/******************************************************************/
void Js_Runidle()
{
	int i=0;
	
	/* If the current state is not UPDATE_DR or UPDATE_IR, reset the JSM and move to RUN/IDLE */
	if(jtag.state!=JS_UPDATE_IR && jtag.state!=JS_UPDATE_DR)
	{
		for(i=0;i<JSM_RESET_COUNT;i++)
			AdvanceJSM(1);
	}

	AdvanceJSM(0);
}

/******************************************************************/
/* Name:         AdvanceJSM                                       */
/*                                                                */
/* Parameters:   mode                                             */
/*               -the input mode to JSM.                          */
/*                                                                */
/* Return Value: The current JSM state.                           */
/*               		                                          */
/* Descriptions: Function that keep track of the JSM state. It    */
/*               drives out signals to TMS associated with a      */
/*               clock pulse at TCK and updates the current state */
/*               variable.                                        */
/*                                                                */
/******************************************************************/
__inline__ int AdvanceJSM(int mode)
{
	PBDATA = mode ? (PBDATA | 0x20) : (PBDATA & 0xdf);
	PDDATA |= 0x02;
	PDDATA &= 0xfd;
	//DriveSignal(J_SIG_TMS,mode,1,1);

	jtag.state = JSM[jtag.state][mode];

	return (jtag.state);
}

/******************************************************************/
/* Name:         PrintJS                                          */
/*                                                                */
/* Parameters:   None.                                            */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Print the current state of the JSM.              */
/*                                                                */
/******************************************************************/
void PrintJS()
{
	//char state[MAX_JS_CHAR_COUNT+1];

	//strcpy(state, JS_NAME[jtag.state]);

	low_printf("Info: JSM: %s\r\n", JS_NAME[jtag.state]);
}

/******************************************************************/
/* Name:         SetupChain                                       */
/*                                                                */
/* Parameters:   dev_count,dev_seq,ji_info,action                 */
/*               -device_count is the total device in chain       */
/*               -dev_seq is the device sequence in chain         */
/*               -ji_info is the pointer to an integer array that */
/*                contains the JTAG instruction length for the    */
/*                devices in chain.                               */
/*               -action is the JTAG instruction to load          */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Move the JSM to SHIFT_IR. Issue the JTAG         */
/*               instruction, "action" to the target device and   */
/*               BYPASS to the rest of the devices. Then, move    */
/*               the JSM to UPDATE_IR.                            */
/*                                                                */
/******************************************************************/
void SetupChain(int dev_count,int dev_seq,int* ji_info,int action)
{
	int i,record=0;
	/* Move Jtag State Machine (JSM) to RUN/IDLE */
	if(jtag.state!=JS_RUNIDLE && jtag.state!=JS_RESET)
		Js_Runidle();

	/* Move JSM to SHIFT_IR */
	AdvanceJSM(0);
	AdvanceJSM(1);
	AdvanceJSM(1);
	AdvanceJSM(0);
	AdvanceJSM(0);

	for(i=dev_count-1;i>=0;i--)
	{
		if(i==dev_seq-1)
			record = ReadTDO(ji_info[i],action,(i==0)? 1:0);
		else
			record = ReadTDO(ji_info[i],JI_BYPASS,(i==0)? 1:0);
	}

	/* Move JSM to UPDATE_IR */
	AdvanceJSM(1);
	AdvanceJSM(1);
}

/******************************************************************/
/* Name:         LoadJI                                           */
/*                                                                */
/* Parameters:   action,dev_count,ji_info                         */
/*               -action is the JTAG instruction to load          */
/*               -dev_count is the maximum number of devices in   */
/*                chain.                                          */
/*               -ji_info is the pointer to an integer array that */
/*                contains the JTAG instruction length for the    */
/*                devices in chain.                               */
/*                                                                */
/* Return Value: 1 if contains error;0 if not.                    */
/*               		                                          */
/* Descriptions: Move the JSM to SHIFT_IR. Load in the JTAG       */
/*               instruction to all devices in chain. Then        */
/*               advance the JSM to UPDATE_IR. Irrespective of    */
/*                                                                */
/******************************************************************/
__inline__ int LoadJI(int action,int dev_count,int* ji_info)
{
	int i,record=0,error=0;

	/* Move Jtag State Machine (JSM) to RUN/IDLE */
	if(jtag.state!=JS_RUNIDLE && jtag.state!=JS_RESET)
		Js_Runidle();

	/* Move JSM to SHIFT_IR */
	AdvanceJSM(0);
	AdvanceJSM(1);
	AdvanceJSM(1);
	AdvanceJSM(0);
	AdvanceJSM(0);

	for(i=0;i<dev_count;i++)
	{
		record = ReadTDO(ji_info[i],action,(i==(dev_count-1))? 1:0);

		if(record!=0x155)
		{
			error=1;
			low_printf("Error: JTAG chain broken!\r\nError: Bits unloaded: 0x%X\r\n", record);
			return error;
		}
	}

	/* Move JSM to UPDATE_IR */
	AdvanceJSM(1);
	AdvanceJSM(1);

	return error;
}

/******************************************************************/
/* Name:         Js_Shiftdr                                       */
/*                                                                */
/* Parameters:   None.                                            */
/*                                                                */
/* Return Value: 1 if the current state is not UPDATE_DR or       */
/*               UPDATE_IR. 0 if the opeation is successful.      */
/*               		                                          */
/* Descriptions: Move the JSM to SHIFT_DR. The current state is   */
/*               expected to be UPDATE_DR or UPDATE_IR.           */
/*                                                                */
/******************************************************************/
int Js_Shiftdr()
{
	/* The current JSM state must be in UPDATE_IR or UPDATE_DR */
	if(jtag.state!=JS_UPDATE_DR && jtag.state!=JS_UPDATE_IR)
	{
		if(jtag.state!=JS_RESET && jtag.state!=JS_RUNIDLE)
			return (1);
		else
		{
			AdvanceJSM(0);
			AdvanceJSM(0);
			AdvanceJSM(1);
			AdvanceJSM(0);
			AdvanceJSM(0);

			return (0);
		}
	}

	AdvanceJSM(1);
	AdvanceJSM(0);
	AdvanceJSM(0);

	return (0);
}

/******************************************************************/
/* Name:         Js_Updatedr                                      */
/*                                                                */
/* Parameters:   None.                                            */
/*                                                                */
/* Return Value: 1 if the current state is not SHIFT_DR;0 if the  */
/*               operation is successful.                         */
/*               		                                          */
/* Descriptions: Move the JSM to UPDATE_DR. The current state is  */
/*               expected to be SHIFT_DR                          */
/*                                                                */
/******************************************************************/
int Js_Updatedr()
{
	/* The current JSM state must be in UPDATE_IR or UPDATE_DR */
	if(jtag.state!=JS_SHIFT_DR)
		return (1);

	AdvanceJSM(1);
	AdvanceJSM(1);

	return (0);
}

int Ji_Extest(int dev_count,int* ji_info)
{
	return LoadJI(JI_EXTEST,dev_count,ji_info);
}

int Ji_Program(int dev_count,int* ji_info)
{
	return LoadJI(JI_PROGRAM,dev_count,ji_info);
}

int Ji_Startup(int dev_count,int* ji_info)
{
	return LoadJI(JI_STARTUP,dev_count,ji_info);
}

int Ji_Checkstatus(int dev_count,int* ji_info)
{
	return LoadJI(JI_CHECK_STATUS,dev_count,ji_info);
}

int Ji_Sample(int dev_count,int* ji_info)
{
	return LoadJI(JI_SAMPLE,dev_count,ji_info);
}

int Ji_Idcode(int dev_count,int* ji_info)
{
	return LoadJI(JI_IDCODE,dev_count,ji_info);
}

int Ji_Usercode(int dev_count,int* ji_info)
{
	return LoadJI(JI_USERCODE,dev_count,ji_info);
}

int Ji_Intest(int dev_count,int* ji_info)
{
	return LoadJI(JI_INTEST,dev_count,ji_info);
}

int Ji_Regscan(int dev_count,int* ji_info)
{
	return LoadJI(JI_REGSCAN,dev_count,ji_info);
}

int Ji_User0(int dev_count,int* ji_info)
{
	return LoadJI(JI_USER0,dev_count,ji_info);
}

int Ji_User1(int dev_count,int* ji_info)
{
	return LoadJI(JI_USER1,dev_count,ji_info);
}

int Ji_Bypass(int dev_count,int* ji_info)
{
	return LoadJI(JI_BYPASS,dev_count,ji_info);
}
