/* -*- mode: asm -*-
 *
 *  linux/arch/h8300/platform/h8s/entry.S
 *
 *  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 *	fairly heavy changes to fix syscall args and signal processing
 *	by David McCullough <davidm@snapgear.com>
 */

/*
 *  entry.S
 *  include exception/interrupt gateway
 *          system call entry
 */

#include <linux/sys.h>
#include <linux/config.h>
#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/linkage.h>
	
#define PT_PTRACED	0x00000001
#define PT_TRACESYS	0x00000002
#define PT_DTRACE	0x00000004	/* delayed trace (used on m68k, i386) */
#define PT_TRACESYSGOOD	0x00000008
#define PT_PTRACE_CAP	0x00000010	/* ptracer can follow suid-exec */

ENOSYS = 38

LSIGTRAP = 5

TASK_THREAD = 616
THREAD_KSP = 0
THREAD_USP = 4
THREAD_CCR = (8+2)
	
SOFTIRQ_PENDING = 0
		
/*
 * these are offsets into the task-struct
 */
state      =  0
flags      =  4
sigpending =  8
need_resched	= 20
ptrace     = 24
counter    = 32

/* the following macro is used when enabling interrupts */

LER4	=	0
LER5	=	4
LER3	=	8
LER2	=	12
LER1	=	16
LORIG	=	20
LCCR	=	24
LER0	=	26
LVEC	=	30
LEXR	=	34
LRET	=	36

	.h8300s

/* CPU context save/restore macros. */
	
	.macro	SAVE_ALL
	mov.l	er0,@-sp

	stc	ccr,r0l				/* check kernel mode */
	orc	#0x10,ccr
	btst	#4,r0l
	bne	5f

	mov.l	sp,@SYMBOL_NAME(sw_usp)		/* user mode */
	mov.l	@sp,er0
	mov.l	@SYMBOL_NAME(sw_ksp),sp
	sub.l	#(LRET-LORIG),sp		/* allocate LORIG - LRET */ 
	stm.l	er0-er3,@-sp
	mov.l	@SYMBOL_NAME(sw_usp),er0
	mov.l	@(10:16,er0),er1		/* copy the RET addr */
	mov.l	er1,@(LRET-LER3:16,sp)
	mov.w	@(8:16,er0),r1
	mov.w	r1,@(LEXR-LER3:16,sp)		/* copy EXR */

	mov.w	e1,r1				/* e1 highbyte = ccr */
	and	#0xef,r1h			/* mask mode? flag */
	sub.w	r0,r0
	mov.b	r1h,r0l
	mov.w	r0,@(LCCR-LER3:16,sp)		/* copy ccr */
	mov.l	@(LORIG-LER3:16,sp),er0
	mov.l	er0,@(LER0-LER3:16,sp)		/* copy ER0 */
	bra	6f
5:
	mov.l	@sp,er0				/* kernel mode */
	subs	#2,sp
	stm.l	er0-er3,@-sp
	mov.w	@(LRET-LER3:16,sp),r1
	mov.b	r1h,r1l
	mov.b	#0,r1h
	mov.w	r1,@(LCCR-LER3:16,sp)
6:	
	mov.l	er5,@-sp			/* syscall arg #5 */
	mov.l	er4,@-sp			/* syscall arg #4 */
	.endm

	.macro	RESTORE_ALL
	mov.w	@(LCCR:16,sp),r0		/* check kernel mode */
	btst	#4,r0l
	bne	7f

	mov.l	@SYMBOL_NAME(sw_usp),er0
	mov.l	@(LER0:16,sp),er1		/* restore ER0 */
	mov.l	er1,@er0
	mov.w	@(LEXR:16,sp),r1		/* restore EXR */
	mov.w	r1,@(8:16,er0)
	mov.w	@(LCCR:16,sp),r1		/* restore the RET addr */
	mov.b	r1l,r1h
	mov.b	@(LRET+1:16,sp),r1l
	mov.w	r1,e1
	mov.w	@(LRET+2:16,sp),r1
	mov.l	er1,@(10:16,er0)

	mov.l	@sp+,er4
	mov.l	@sp+,er5
	ldm.l	@sp+,er2-er3
	mov.l	@sp+,er1
	add.l	#(LRET-LER1),sp			/* remove LORIG - LRET */ 
	mov.l	sp,@SYMBOL_NAME(sw_ksp)
	mov.l	er0,sp
	bra	8f
7:
	mov.l	@sp+,er4
	mov.l	@sp+,er5
	ldm.l	@sp+,er2-er3
	mov.l	@sp+,er1
	adds	#4,sp
	adds	#2,sp
8:
	mov.l	@sp+,er0
	adds	#4,sp				/* remove the sw created LVEC */
	rte
	.endm
	
#define SWITCH_STACK_SIZE (1*4+4)		/* includes return address */

	.macro	SAVE_SWITCH_STACK
	mov.l	er6,@-sp
	.endm

	.macro	RESTORE_SWITCH_STACK
	mov.l	@sp+,er6
	.endm

.globl SYMBOL_NAME(system_call)
.globl SYMBOL_NAME(ret_from_exception)
.globl SYMBOL_NAME(ret_from_signal)
.globl SYMBOL_NAME(ret_from_interrupt)
.globl SYMBOL_NAME(interrupt_redirect_table)
.globl SYMBOL_NAME(sw_ksp),SYMBOL_NAME(sw_usp)
.globl SYMBOL_NAME(resume)
.globl SYMBOL_NAME(syscall_trampoline)
.globl SYMBOL_NAME(trace_break)
.globl SYMBOL_NAME(interrupt_entry)
		
INTERRUPTS = 128
#if defined(CONFIG_ROMKERNEL)
	.section .int_redirect,"ax"
SYMBOL_NAME_LABEL(interrupt_redirect_table)
	.rept	7
	.long	0
	.endr
	jsr	@SYMBOL_NAME(interrupt_entry)	/* NMI */
	jmp	@SYMBOL_NAME(system_call)	/* TRAPA #0 (System call) */
	.long	0
	.long	0
	jmp	@SYMBOL_NAME(trace_break)	/* TRAPA #3 (breakpoint) */
	.rept	INTERRUPTS-12
	jsr	@SYMBOL_NAME(interrupt_entry)
	.endr
#endif
#if defined(CONFIG_RAMKERNEL)
.globl SYMBOL_NAME(interrupt_redirect_table)
	.section .bss
SYMBOL_NAME_LABEL(interrupt_redirect_table)
	.space	4
#endif
	
	.section .text
	.align	2
SYMBOL_NAME_LABEL(interrupt_entry)
	SAVE_ALL
	mov.w	@(LCCR,sp),r0
	btst	#4,r0l
	bne	1f
	mov.l	@SYMBOL_NAME(sw_usp),er0
	mov.l	@(4:16,er0),er0
	bra	2f
1:
	mov.l	@(LVEC:16,sp),er0
2:
#if defined(CONFIG_ROMKERNEL)
	sub.l	#SYMBOL_NAME(interrupt_redirect_table),er0
#endif
#if defined(CONFIG_RAMKERNEL)
	mov.l	@SYMBOL_NAME(interrupt_redirect_table),er1
	sub.l	er1,er0
#endif
	shlr.l	#2,er0
	dec.l	#1,er0
	mov.l	sp,er1
	jsr	@SYMBOL_NAME(process_int)
	mov.l	@SYMBOL_NAME(irq_stat)+SOFTIRQ_PENDING,er0
	beq	1f
	jsr	@SYMBOL_NAME(do_softirq)
1:
	sub.l	er0,er0
	dec.l	#1,er0
	mov.l	er0,@(LORIG:16,sp)
	jmp	@SYMBOL_NAME(ret_from_exception)

SYMBOL_NAME_LABEL(system_call)
	subs	#4,sp				/* dummy LVEC */
	SAVE_ALL
	mov.w	@(LCCR:16,sp),r1
	bset	#4,r1l
	ldc	r1l,ccr
	mov.l	er0,er3
	mov.l	#-ENOSYS,er0
	mov.l	er0,@(LER0:16,sp)

	/* save top of frame */
	mov.l	sp,er0
	mov.l	er3,@-sp
	jsr	@SYMBOL_NAME(set_esp0)
	mov.l	@sp+,er3
	cmp.l	#NR_syscalls,er3
	bcc	SYMBOL_NAME(ret_from_exception):16
	shll.l	#2,er3
	mov.l	#SYMBOL_NAME(sys_call_table),er0
	add.l	er3,er0
	mov.l	@er0,er0
	mov.l	er0,er3
	beq	SYMBOL_NAME(ret_from_exception):16	
	mov.l	@SYMBOL_NAME(_current_task),er2
	mov.b	@((flags+3-(PT_TRACESYS >> 3)):16,er2),r2l
	btst	#(PT_TRACESYS & 7),r2l
	bne	1f
	mov.l	@(LER1:16,sp),er0
	mov.l	@(LER2:16,sp),er1
	mov.l	@(LER3:16,sp),er2
	jsr	@er3
	mov.l	er0,@(LER0,sp)			/* save the return value */
#if defined(CONFIG_SYSCALL_PRINT)
	jsr	@SYMBOL_NAME(syscall_print)
#endif
	jmp	@SYMBOL_NAME(ret_from_exception)
1:
	subs	#4, sp				/* dummy return PC */
	SAVE_SWITCH_STACK
	mov.l	er3,er5				/* save syscall entry */
	jsr	SYMBOL_NAME(syscall_trace)
	mov.l	er5,er3
	RESTORE_SWITCH_STACK
	adds	#4, sp				/* dummy return PC */
	mov.l	@(LER1:16,sp),er0
	mov.l	@(LER2:16,sp),er1
	mov.l	@(LER3:16,sp),er2
	jsr	@er3
	mov.l	er0,@(LER0:16,sp)		/* save the return value */
	subs	#4, sp				/* dummy return PC */
	SAVE_SWITCH_STACK
	jsr	SYMBOL_NAME(syscall_trace)

SYMBOL_NAME_LABEL(ret_from_signal)
	RESTORE_SWITCH_STACK
	adds	#4, sp				/* dummy return PC */

SYMBOL_NAME_LABEL(ret_from_exception)
	mov.b	@(LCCR+1:16,sp),r0l
	btst	#4,r0l				/* check if returning to kernel */
	bne	3f:16				/* if so, skip resched, signals */
	mov.l	@SYMBOL_NAME(_current_task),er0
	mov.l	@(need_resched:16,er0),er1
	bne	SYMBOL_NAME(reschedule):16
	mov.b	@((flags+(PT_DTRACE >> 3)):16,er0),r1l
	mov.b	r1l,r1h
	bclr	#(PT_DTRACE & 7),r1h
	mov.b	r1h,@((flags+3-(PT_DTRACE >> 3)):16,er0)
	btst	#(PT_DTRACE & 7),r1l		/* check for delayed trace */
	beq	1f
	sub.l	er2,er2
	inc.l	#1,er2
	mov.l	er0,er1
	sub.l	er0,er0
	mov.b	#LSIGTRAP,r0l
	jsr	@SYMBOL_NAME(send_sig)		/* send_sig(LSIGTRAP,_current_task,1) */
	mov.l	@SYMBOL_NAME(_current_task),er0
1:
	mov.l	@(state:16,er0),er1		/* state */
	bne	SYMBOL_NAME(reschedule):16
	mov.l	@(counter:16,er0),er1		/* counter */
	beq	SYMBOL_NAME(reschedule):16
	mov.l	@(sigpending:16,er0),er1
	bne	Lsignal_return:16
3:
	RESTORE_ALL				/* Does RTE */

Lsignal_return:
	subs	#4, sp				/* dummy return PC */
	SAVE_SWITCH_STACK
	mov.l	sp,er1
	add	#SWITCH_STACK_SIZE,er1
	sub.l	er0,er0
	jsr	@SYMBOL_NAME(do_signal)
	RESTORE_SWITCH_STACK
	adds	#4, sp				/* dummy return PC */
	RESTORE_ALL

SYMBOL_NAME_LABEL(reschedule)
	/* save top of frame */
	mov.l	sp,er0
	jsr	@SYMBOL_NAME(set_esp0)

	mov.l	#SYMBOL_NAME(ret_from_exception),er0
	mov.l	er0,@-sp
	jmp	@SYMBOL_NAME(schedule)

SYMBOL_NAME_LABEL(resume)
	/*
	 *	er0 = prev
	 *	er1 = next
	 *	return last in er2
	 */

	/* save sr */
	sub.w	r3,r3
	stc	ccr,r3l
	stc	exr,r3h
	mov.w	r3,@((TASK_THREAD+THREAD_CCR):16,er0)
	SAVE_SWITCH_STACK

	/* disable interrupts */
	orc	#0x80,ccr
	mov.l	@SYMBOL_NAME(sw_usp),er3
	mov.l	er3,@((TASK_THREAD+THREAD_USP):16,er0)
	mov.l	sp,@((TASK_THREAD+THREAD_KSP):16,er0)
	
	/* get pointer to tss struct (er1 contains new task) */
	mov.l   @SYMBOL_NAME(_current_task), er2
	mov.l	er1,@SYMBOL_NAME(_current_task)

	/* Skip address space switching if they are the same. */
	/* FIXME: what did we hack out of here, this does nothing! */

	mov.l	@((TASK_THREAD+THREAD_USP):16,er1),er0
	mov.l	er0,@SYMBOL_NAME(sw_usp)
	mov.l	@((TASK_THREAD+THREAD_KSP):16,er1),sp
	RESTORE_SWITCH_STACK
			
	/* restore status register */
	mov.w	@((TASK_THREAD+THREAD_CCR):16,er1),r3

	ldc	r3l,ccr
	ldc	r3h,exr
	
	rts

SYMBOL_NAME_LABEL(trace_break)
	subs	#4,sp			/* dummy LVEC */
	SAVE_ALL
	sub.l	er1,er1
	dec.l	#1,er1
	mov.l	er1,@(LORIG,sp)	
	mov.l	sp,er0
	jsr	@SYMBOL_NAME(set_esp0)
	mov.l	@SYMBOL_NAME(sw_usp),er0
	mov.l	@er0,er1
	subs	#2,er1
	mov.l	er1,@er0	
	and.w	#0xff,e1
	mov.l	er1,er0
	jsr	@SYMBOL_NAME(trace_trap)
	jmp	@SYMBOL_NAME(ret_from_exception)	

SYMBOL_NAME_LABEL(syscall_trampoline)
	SAVE_SWITCH_STACK
	mov.l	er0,er6
	mov.l	sp,er0
	add.l	#SWITCH_STACK_SIZE,er0
	jsr	@er6
	RESTORE_SWITCH_STACK
	rts

#if defined(CONFIG_BLK_DEV_BLKMEM)
/*
	romfs move to __ebss
	(used crt0)
*/
.globl	__move_romfs
_romfs_sig_len = 8

__move_romfs:	
	mov.l	#__sbss,er0
	mov.l	#_romfs_sig,er1
	mov.b	#_romfs_sig_len,r3l
1:					/* check romfs image */
	mov.b	@er0+,r2l
	mov.b	@er1+,r2h
	cmp.b	r2l,r2h
	bne	2f
	dec.b	r3l
	bne	1b

	/* find romfs image */
	mov.l	@__sbss+8,er0		/* romfs length(be) */
	mov.l	#__sbss,er1
	add.l	er0,er1			/* romfs image end */
	mov.l	#__ebss,er2
	add.l	er0,er2			/* distination address */
#if defined(CONFIG_INTELFLASH)
	add.l	#512,er2
#endif
	adds	#2,er0
	adds	#1,er0
	shlr	#2,er0			/* transfer length */
1:
	mov.l	@er1,er3		/* copy image */
	mov.l	er3,@er2
	subs	#4,er1
	subs	#4,er2
	dec.l	#1,er0
	bpl	1b
2:
	rts

	.section	.rodata
_romfs_sig:	
	.ascii	"-rom1fs-"
#endif

	.section	.bss
SYMBOL_NAME_LABEL(sw_ksp)
	.space	4	
SYMBOL_NAME_LABEL(sw_usp)
	.space	4	
