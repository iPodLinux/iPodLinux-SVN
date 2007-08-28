/*
 *  linux/arch/frionommu/kernel/traps.c
 *
 *  Copyright (C) 1993, 1994 by Hamish Macdonald
 *
 * Adapted for BlackFin (ADI) by Ted Ma <mated@sympatico.ca>
 * Copyright (c) 2002 Arcturus Networks Inc. (www.arcturusnetworks.com)
 *		-BlackFin/FRIO uses S/W interrupt 15 for the system calls
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/*
 * Sets up all exception vectors
 */

/* Note: 
 *    1. ILLEGAL USE PROTECTED RESOURCE ( EXCAUSE = 0x2E)
 *            illegal execution supervisor mode only instruction
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/a.out.h>
#include <linux/user.h>
#include <linux/string.h>
#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/setup.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/traps.h>
#include <asm/pgtable.h>
#include <asm/machdep.h>
#include <asm/siginfo.h>
//#include <asm/hw_irq.h>	// don't think I need this ...MaTed---

/* frio nisa specific include headers */
#include <asm/blackfin.h>

/* assembler routines */
asmlinkage void excpt_handle(void);
asmlinkage void system_call(void);
asmlinkage void trap(void);
asmlinkage void inthandler(void);
asmlinkage void nmihandler(void);
asmlinkage void emu_handle(void);
asmlinkage void rst_handle(void);
asmlinkage void nmi_handler(void);
asmlinkage void ivhw_handle(void);
asmlinkage void ivtmr_handle(void);
asmlinkage void ivg7_handle(void);

extern int kprintf(char *, ...);

/* Declare exception handler: For detail, look into gcc nisa.
 */

void excpt_handle(void)
{ }

/* Frio has 64 exceptions, and 0-15 can be user invoked	
 * this table provide seperated handler for different exception
 */
e_vector vectors[64] = {
	system_call, excpt_handle, excpt_handle, excpt_handle,
	excpt_handle, excpt_handle, excpt_handle, excpt_handle,
	excpt_handle, excpt_handle, excpt_handle, excpt_handle,
	excpt_handle, excpt_handle, excpt_handle, excpt_handle,
	trap, trap, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, trap, trap, trap, trap, trap, trap, trap,
	trap, trap, trap, trap, trap, trap, trap, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};


enum {
#undef REG
#define REG(name, pos) reg_##name,
#include <asm/regs.h>
};

#define EXITCODE  *(char volatile *)0xFF7EEEEE

static void __init nisa_trap_init (void)
{
    asm("p0.l = 0x2000; p0.h = 0xffe0;
	 p1.h = trap;
	 p1.l = trap;
	 [p0+(4*3)] = p1;     // Vector event 03 ----- Exception handler (trap) in entry.S
	 p1.l = system_call;
	 p1.h = system_call;
	 [p0+(4*15)] = p1;"); // Vector event 15 ----- System call handler(system_call) in entry.S

}

/* Initiate the event table handler  ---- called from init/main.c start_kernel()
 */
void __init trap_init (void)
{

	nisa_trap_init();
}



static char *vec_names[] = {
	"SYSTEM CALL" , "EXCPT 1" , "EXCPT 2" , "EXCPT 3",
        "EXCPT 4" , "EXCPT 5" , "EXCPT 6" , "EXCPT 7",
        "EXCPT 8" , "EXCPT 9" , "EXCPT 10", "EXCPT 11",
        "EXCPT 12", "EXCPT 13", "EXCPT 14", "EXCPT 15",
	"SINGLE STEP", "EMULATION TRACE OVERFLOW", "UNASSIGNED RESERVED 18",
	"UNASSIGNED RESERVED 19", "UNASSIGNED RESERVED 20", "UNASSIGNED RESERVED 21",
	"UNASSIGNED RESERVED 22", "UNASSIGNED RESERVED 23", "UNASSIGNED RESERVED 24",
	"UNASSIGNED RESERVED 25", "UNASSIGNED RESERVED 26", "UNASSIGNED RESERVED 27",
	"UNASSIGNED RESERVED 28", "UNASSIGNED RESERVED 29", "UNASSIGNED RESERVED 30",
	"UNASSIGNED RESERVED 31", "UNASSIGNED RESERVED 32",
	"UNDEFINED INSTRUCTION", "ILLEGAL INSTRUCTION", "CPLB PROTECTION VIOLATION",
	"MISALIGNED DATA ACCESS", "UNRECOVERABLE EVENT", "CPLB MISS",
	"MULTIPLE CPLB HITS", "EMULATION WATCHPOINT MATCH", "INSTRUCTION ACCESS VIOLATION",
	"MISALIGNED INSTRUCTION FETCH", "INSTRUCTION CPLB PROTECTION VIOLATION",
	"INSTRUCTION CPLB MISS", "INSTRUCTION MULTIPLE CPLB HITS",
	"ILLEGAL USE PROTECTED RESOURCE", "UNASSIGNED RESERVED 47", "UNASSIGNED RESERVED 48",
	"UNASSIGNED RESERVED 49", "UNASSIGNED RESERVED 50", "UNASSIGNED RESERVED 51",
	"UNASSIGNED RESERVED 52", "UNASSIGNED RESERVED 53", "UNASSIGNED RESERVED 54",
	"UNASSIGNED RESERVED 55", "UNASSIGNED RESERVED 56", "UNASSIGNED RESERVED 57",
	"UNASSIGNED RESERVED 58", "UNASSIGNED RESERVED 59", "UNASSIGNED RESERVED 60",
	"UNASSIGNED RESERVED 61", "UNASSIGNED RESERVED 62", "UNASSIGNED RESERVED 63"
	};

void die_if_kernel(char *,struct pt_regs *,int);
asmlinkage int do_page_fault(struct pt_regs *regs, unsigned long address,
                             unsigned long error_code);

asmlinkage void trap_c(struct pt_regs *fp);

int kstack_depth_to_print = 48;

/* MODULE_RANGE is a guess of how much space is likely to be
   vmalloced.  */
#define MODULE_RANGE (8*1024*1024)

asmlinkage void trap_c(struct pt_regs *fp)
{
	int sig;
	siginfo_t info;

	/* send the appropriate signal to the user program */
	switch (fp->seqstat & 0x3f) {
	    case VEC_STEP:
		info.si_code = TRAP_STEP;
		sig = SIGTRAP;
		break;
	    case VEC_UNDEF_I:
		info.si_code = ILL_ILLOPC;
		sig = SIGILL;
		break;
	    case VEC_OVFLOW:
		info.si_code = TRAP_TRACEFLOW;
		sig = SIGTRAP;
		break;
	    case VEC_ILGAL_I:
		info.si_code = ILL_ILLPARAOP;
		sig = SIGILL;
		break;
	    case VEC_ILL_RES:
		info.si_code = ILL_PRVOPC;
		sig = SIGILL;
                break;

	    case VEC_MISALI_D:
	    case VEC_MISALI_I:
		info.si_code = BUS_ADRALN;
		sig = SIGBUS;
		break;

	    case VEC_UNCOV:
		info.si_code = ILL_ILLEXCPT;
		sig = SIGILL;
		break;

	    case VEC_WATCH:
		info.si_code = TRAP_WATCHPT;
		sig = SIGTRAP;
		break;

	    case VEC_ISTRU_VL:
		info.si_code = BUS_OPFETCH;
		sig = SIGBUS;
                break;

	    case VEC_CPLB_I_VL:
	    case VEC_CPLB_VL:
		info.si_code = ILL_CPLB_VI;
		sig = SIGILL;
                break;

	    case VEC_CPLB_I_M:
	    case VEC_CPLB_M:
		info.si_code = IlL_CPLB_MISS;
		sig = SIGILL;
                break;

	    case VEC_CPLB_I_MHIT:
	    case VEC_CPLB_MHIT:
		info.si_code = ILL_CPLB_MULHIT;
		sig = SIGILL;
                break;

	    default:
		info.si_code = TRAP_ILLTRAP;
		sig = SIGTRAP;
		break;
	}
	info.si_signo = sig;
	info.si_errno = 0;
	info.si_addr = (void *) fp->pc;
	force_sig_info (sig, &info, current);
}

void show_trace_task(struct task_struct *tsk)
{
	/* XXX: we can do better, need a proper stack dump */
	printk("STACK ksp = 0x%lx, usp = 0x%lx\n", tsk->thread.ksp, tsk->thread.usp);
}

void die_if_kernel (char *str, struct pt_regs *fp, int nr)
{
	if (!(fp->seqstat & PS_S))
		return;

	console_verbose();
	printk("%s: %08x\n",str,nr);
	printk("PC: [<%08lu>]    SEQSTAT: %04lu\n",
	       fp->pc, fp->seqstat);
	printk("r0: %08lx    r1: %08lx    r2: %08lx    r3: %08lx\n",
	       fp->r0, fp->r1, fp->r2, fp->r3);
	printk("r4: %08lx    r5: %08lx    r6: %08lx    r7: %08lx\n",
	       fp->r4, fp->r5, fp->r6, fp->r7);
	printk("p0: %08lx    p1: %08lx    p2: %08lx    p3: %08lx\n",
	       fp->p0, fp->p1, fp->p2, fp->p3);
	printk("p4: %08lx    p5: %08lx    fp: %08lx\n",
	       fp->p4, fp->p5, fp->fp);
	printk("aow: %08lx    a0.x: %08lx    a1w: %08lx    a1.x: %08lx\n",
	       fp->a0w, fp->a0x, fp->a1w, fp->a1x);

	printk("Process %s (pid: %d, stackpage=%08lx)\n",
		current->comm, current->pid, PAGE_SIZE+(unsigned long)current);
	do_exit(SIGSEGV);
}


/* Typical exception handling routines	*/

