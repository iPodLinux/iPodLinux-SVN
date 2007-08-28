/*
 *  linux/arch/hyperstone-nommu/kernel/traps.c
 *
 *  Copyright (C) 2002-2003,    George Thanos <george.thanos@gdt.gr>
 *                              Yannis Mitsos <yannis.mitsos@gdt.gr>
 */
#include <asm/ptrace.h>
#include <asm/signal.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>

void die_if_kernel (char *str, struct pt_regs *regs, int nr)
{
	if(user_mode(regs))
		return;

	do_exit(SIGSEGV);
}

/* FP exception number entries */
#define FCVTD   1
#define FCVT    2
#define FCMPUD  3
#define FCMPU   4
#define FCMPD   5
#define FCMP    6
#define FDIVD   7
#define FDIV    8
#define FMULD   9
#define FMUL    10
#define FSUBD   11
#define FSUB    12
#define FADDD   13
#define FADD    14

void Exception58_Entry(void);
void Exception59_Entry(void);
void Exception60_Entry(void);
void Exception63_Entry(void);
void ExceptionFP_Entry(void);

void ExceptionFCVTD_Entry(void);
void ExceptionFCVT_Entry(void);
void ExceptionFCMPUD_Entry(void);
void ExceptionFCMPU_Entry(void);
void ExceptionFCMPD_Entry(void);
void ExceptionFCMP_Entry(void);
void ExceptionFDIVD_Entry(void);
void ExceptionFDIV_Entry(void);
void ExceptionFMULD_Entry(void);
void ExceptionFMUL_Entry(void);
void ExceptionFSUBD_Entry(void);
void ExceptionFSUB_Entry(void);
void ExceptionFADDD_Entry(void);
void ExceptionFADD_Entry(void);

/* Set an entry in the Trap Entry Table.
 * Entries are mapped in the beggining of
 * memory area MEM0.
 */
void MEM0_set_exception(int num, void *handler)
{
	unsigned long *exception_ptr = (unsigned long *)((long)0x000000FC - 4*num);
	*exception_ptr = *((unsigned long*)handler);
}

void MEM0_set_fp_exception(int num, void *handler)
{
	unsigned long *exception_ptr = (unsigned long *)((long)0x0000011C + 0x10*num);
	*exception_ptr = *((unsigned long*)handler);
}

void trap_init(void) 
{ 
        /*Regular Exceptions*/
        /* 57. Trace exception is allocated by GDB */
        MEM0_set_exception( 58, Exception58_Entry );
        MEM0_set_exception( 59, Exception59_Entry );
        MEM0_set_exception( 60, Exception60_Entry );

	/* Set the FP exception */
        MEM0_set_exception( 46, ExceptionFP_Entry );

	/* Floating Point Exceptions */

	MEM0_set_fp_exception( FCVTD , ExceptionFCVTD_Entry );
	MEM0_set_fp_exception( FCVT  , ExceptionFCVT_Entry );
	MEM0_set_fp_exception( FCMPUD, ExceptionFCMPUD_Entry );
	MEM0_set_fp_exception( FCMPU,  ExceptionFCMPU_Entry );
	MEM0_set_fp_exception( FCMPD,  ExceptionFCMPD_Entry );
	MEM0_set_fp_exception( FCMP,   ExceptionFCMP_Entry );
	MEM0_set_fp_exception( FDIVD,  ExceptionFDIVD_Entry );
	MEM0_set_fp_exception( FDIV,   ExceptionFDIV_Entry );
	MEM0_set_fp_exception( FMULD,  ExceptionFMULD_Entry );
	MEM0_set_fp_exception( FMUL,   ExceptionFMUL_Entry );
	MEM0_set_fp_exception( FSUBD,  ExceptionFSUBD_Entry );
	MEM0_set_fp_exception( FSUB,   ExceptionFSUB_Entry );
	MEM0_set_fp_exception( FADDD,  ExceptionFADDD_Entry );
	MEM0_set_fp_exception( FADD,   ExceptionFADD_Entry );
}

void show_trace_task(struct task_struct *tsk) { }

