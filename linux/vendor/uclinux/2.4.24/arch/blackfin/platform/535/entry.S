/* -*- mode: asm -*-
 *
 *  linux/arch/frio/kernel/entry.S
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file README.legal in the main directory of this archive
 * for more details.
 *
 * Linux/frio support: Tony Kou & Metrowerks
 */

/*
 * entry.S  contains the system-call and fault low-level handling routines.
 * This also contains the timer-interrupt handler, as well as all interrupts
 * and faults that can result in a task-switch.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call.
 *
 */

/*
 * Secondary stage of kernel interrupt and exception handling. Points of entry
 * into the kernel are in interrupt.S There are jumps to this file from
 * interrupt.S Context is already saved when control comes to this file. SP
 * points to bottom of the context, hence the stack appears same as pt_regs.
 * ... amit
 */


#include <linux/sys.h>
#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/entry.h>
#include <asm/errno.h>
#include <asm/setup.h>
#include <asm/traps.h>

#include "frio_defs.h"

.globl SYMBOL_NAME(system_call), SYMBOL_NAME(buserr), SYMBOL_NAME(trap)
.globl SYMBOL_NAME(resume), SYMBOL_NAME(ret_from_exception)
.globl SYMBOL_NAME(ret_from_signal)
.globl SYMBOL_NAME(sys_call_table)
.globl SYMBOL_NAME(sys_fork), SYMBOL_NAME(sys_clone), SYMBOL_NAME(sys_vfork)
.globl SYMBOL_NAME(ret_from_interrupt), SYMBOL_NAME(bad_interrupt)
.globl SYMBOL_NAME(inthandler1), SYMBOL_NAME(inthandler2),SYMBOL_NAME(inthandler3)
.globl SYMBOL_NAME(inthandler4),SYMBOL_NAME(inthandler5),SYMBOL_NAME(inthandler6)
.globl SYMBOL_NAME(inthandler7)
.globl SYMBOL_NAME(kernel_to_user_mode), SYMBOL_NAME(do_delay_trace), SYMBOL_NAME(Lsignal_return)

.text

ENTRY(trap) /* Exception: 4th entry into system event table(supervisor mode */
	SAVE_ALL_INT
	
        p2.l = 0x2108;
        p2.h = 0xffe0;
        r0 = [p2];              // Read current IPEND
        [sp + PT_IPEND] = r0;   // Store IPEND

	r1 = [sp + PT_SEQSTAT];	// reason code is it bit 5:0
	r0.l = 0x3f;
	r0.h = 0x0;
	r1 = r1 & r0;
	CC = r1 == 0;
	if ! cc jump 1f;
	raise 14;	/* invoked by TRAP #0, for sys call */
	jump.s	SYMBOL_NAME(ret_from_exception); /* pcrel12 enough */
1:
	r0 = sp; /* stack frame pt_regs pointer argument ==> r0 */
	call	SYMBOL_NAME(trap_c);
	jump.s	SYMBOL_NAME(ret_from_exception); /* pcrel12 enough */

ENTRY(reschedule)
	/* save top of frame*/
	p2.l = SYMBOL_NAME(_current_task);
	p2.h = SYMBOL_NAME(_current_task);
	[p2+(TASK_THREAD+THREAD_ESP0)] = sp;

	p3.l = SYMBOL_NAME(ret_from_exception);
	p3.h = SYMBOL_NAME(ret_from_exception);
	rets = p3;
	jump.l SYMBOL_NAME(schedule); /* unknow how far away */
	/* Note: asmlinkage void schedule(void) */

	/* After a fork we jump here directly from resume,*/
	/* so that %r1 contains the previous task*/
	/* Theoretically only needed on SMP, but let's watch*/
	/* what happens in schedule_tail() in future...*/
ENTRY(ret_from_fork)

	/* Don't bother with trace yet - akale */
	r0 = r1;
	call SYMBOL_NAME(schedule_tail);
	/* Note: void schedule_tail(struct task_struct *prev)	*/
	
	jump.s SYMBOL_NAME(ret_from_exception);

badsys:
	r7 = -ENOSYS; /* signextending enough */
	[sp + PT_R0] = r7;	/* return value from system call */
	rti;	

do_trace:
	r7 = -ENOSYS;
	[sp + PT_R0] = r7;	/* needed for strace */
	sp += -4;		/* why minus then plus again ??? Tony */
	call SYMBOL_NAME(syscall_trace); /* void syscall_trace(void) */
	sp += 4;
	r5 = [sp+PT_R5];
	r0 = -ENOSYS;
	r7 = NR_syscalls;
	cc = r5 < r7;
	if ! cc jump 1f;
	p4 = r1;
	p5.l = SYMBOL_NAME(sys_call_table);
	p5.h = SYMBOL_NAME(sys_call_table);
	p5 = p5 + (p4 << 2);
	r0 = [sp + PT_R0];	/* confirm the system call args */
	r1 = [sp + PT_R1];
	r2 = [sp + PT_R2];
	jump (p5);
1:
	[sp + PT_R0] = r0;	/* save the return value */
	sp += -4;		/* dummy return address  */
	jump.l SYMBOL_NAME(syscall_trace); /* void syscall_trace(void) */

ENTRY(system_call)	/* entry for excpt 0 into EVENT table */
	/* save top of frame*/
	SAVE_ALL_INT
	[--sp] = reti;
	sp += 4;
	
        p2.l = 0x2108;
        p2.h = 0xffe0;
        r0 = [p2];              // Read current IPEND
        [sp + PT_IPEND] = r0;   // Store IPEND

	p2.l = SYMBOL_NAME(_current_task);
	p2.h = SYMBOL_NAME(_current_task);
	[p2+(TASK_THREAD+THREAD_ESP0)] = sp;

	
#if 0 /* amit don't trace syscalls for now */
	r7 = [p2+TASK_FLAGS];
	cc = bittst (r7,PF_TRACESYS_BIT);
	if cc jump do_trace;
#endif

	r7 = NR_syscalls;
	cc = r5 < r7;	/* r5 = number of system call */
	if ! cc jump badsys;

	p4 = r5;
	p5.l = SYMBOL_NAME(sys_call_table);
	p5.h = SYMBOL_NAME(sys_call_table);
	p5 = p5 + (p4<<2);
	r0 = [sp + PT_R0];	/* confirm the system call args */
	r1 = [sp + PT_R1];
	r2 = [sp + PT_R2];
	p5 = [p5];
	call (p5);
	
	[sp+PT_R0] = r0;
	jump.s SYMBOL_NAME(ret_from_exception);

ENTRY(ret_from_exception)

	r0 = [sp + PT_IPEND];
	CC = bittst(r0, 15);	/* check if returning to kernel*/
	if cc jump 4f;   /* if so, skip resched, signals*/   

	cc = bittst(r0, 3);	/* check if return from exception */
	if cc jump 1f;
	CC = bittst(r0, 14);	/* check if return from interrupt */
	if cc jump 1f;
	/* reduce the nested interrupt IVG14 --> IVG 15, */
	/* see also asm/processor.h: start_thread */
	call kernel_to_user_mode; 
	r0.l = 0x2;
	r0.h = 0x0;	/* flag for return to user mode */
	[sp + PT_IPEND] = r0;
1:
	/* only allow interrupts when we are really the last one on the*/
	/* kernel stack, otherwise stack overflow can occur during*/
	/* heavy interupt load*/
	p5.l = SYMBOL_NAME(_current_task);
	p5.h = SYMBOL_NAME(_current_task);
	r7 = [p5];
	p5 = r7;
	r7 = [p5+TASK_NEEDRESCHED];
	cc = r7 == 0;
	if cc jump 2f;
	jump.l SYMBOL_NAME(reschedule);
2:
	/* check for delayed trace*/
	r7 = [p5+TASK_FLAGS];
	cc = bittst(r7, PF_DTRACE_BIT);
	bitclr (r7, PF_DTRACE_BIT);
	[p5+TASK_FLAGS] = r7;
	if !cc jump 5f;
	call SYMBOL_NAME(do_delayed_trace);
5:
	r7 = [p5+TASK_STATE];		/* state*/
	cc = r7 == 0;
	if cc jump 3f
	jump.l SYMBOL_NAME(reschedule);
3:
	r7 = [p5+TASK_SIGPENDING];
	cc = r7 == 0;
	if cc jump 4f;
	call SYMBOL_NAME(Lsignal_return);
4:
	p2.l = 0x2108;
	p2.h = 0xffe0;
	r0 = [p2];              // Read current IPEND
	CC = bittst(r0, 3);	/* check if return from exception*/
	if cc jump 7f;
	
	r7 = [sp + PT_SYSCFG];	/* handle single step - STchen */
	cc = bittst(r7, 0);
	if cc jump supper_single_step;
	
	CC = bittst(r0, 14);	/* check if return from interrupt */
	if cc jump 6f;
	r0 = [sp + PT_IPEND];
	CC = bittst(r0, 1);	/* return to user mode */
	if cc jump 6f;
7:	
	RESTORE_ALL
6:
	RESTORE_ALL2

supper_single_step:
	r0.l = prev_single_step;
	r0.l = prev_single_step;
	p2.l = 0x200c;
	p2.h = 0xffe0;
	[p2] = r0;
	r0.l = force_exception;
	r0.h = force_exception;
	reti = r0;
	rti;
	
	/* we must force the exception for supper single step while */ 
	/* system return to user mode from system call, otherwise single */
	/* step will happen in kernel but not in application --STchen*/
force_exception:
	excpt 0x1;
	
	/* return from exception to prevent supper single step */
prev_single_step:
	r0.l = SYMBOL_NAME(trap);
	r0.h = SYMBOL_NAME(trap);
	p2.l = 0x200c;
	p2.h = 0xffe0;
	[p2] = r0;
	RESTORE_ALL
	
SYMBOL_NAME_LABEL(Lsignal_return)
	r0 = 0;
	r1 = sp;
	[--sp] = rets;
	call SYMBOL_NAME(do_signal);
	rets = [sp++];
	rts;

SYMBOL_NAME_LABEL(do_delayed_trace)
	[--sp] = rets;
	r0 = LSIGTRAP;
	r1 = p5;
	r2 = 1;
	call SYMBOL_NAME(send_sig);
	rets = [sp++];
	rts;

ENTRY(sys_fork)
	call	SYMBOL_NAME(frio_fork);
	rts;

ENTRY(sys_clone)
	r0 = sp;
	[--sp] = rets;
	call	SYMBOL_NAME(frio_clone);
	rets = [sp++];
	rts;

ENTRY(sys_vfork)
	r0 = sp;
	[--sp] = rets;
	call	SYMBOL_NAME(frio_vfork);
	rets = [sp++];
	rts;

ENTRY(sys_sigsuspend)
	call	SYMBOL_NAME(do_sigsuspend);
	rts;

ENTRY(sys_rt_sigsuspend)
	call	SYMBOL_NAME(do_rt_sigsuspend);
	rts;

ENTRY(sys_sigreturn)
	r0 = sp;
	[--sp] = rets;
	call	SYMBOL_NAME(do_sigreturn);
	rets = [sp++];
	rts;

ENTRY(sys_rt_sigreturn)
	call	SYMBOL_NAME(do_rt_sigreturn);
	rts;

/* Force kernel return from interrupt 15 to user mode -- Steven Chen*/
SYMBOL_NAME_LABEL(kernel_to_user_mode)
	[--sp] = rets;
	reti = [sp++];
	rti;
	
SYMBOL_NAME_LABEL(resume)
	/*
	 * Beware - when entering resume, prev (the current task) is
	 * in r0, next (the new task) is in r1.
	 */

	p0 = r0;
	p1 = r1;

	[--sp] = rets;
	[--sp] = (r7:6, p5:3);
	[--sp] = fp;

	/* save seqstat */
	r7 = seqstat;
	[p0+(TASK_THREAD+THREAD_SR)] = r7;

	/* save usp */
	p2 = usp;
	[p0+(TASK_THREAD+THREAD_USP)] = p2;

	/* save current kernel stack pointer */
	[p0+(TASK_THREAD+THREAD_KSP)] = sp;

	/* save program counter */
	r1.l = SYMBOL_NAME(new_old_task);
	r1.h = SYMBOL_NAME(new_old_task);
	[p0+(TASK_THREAD+THREAD_PC)] = r1;

	/* Return previous task in %r1 */
	r1.l = SYMBOL_NAME(_current_task);
	r1.h = SYMBOL_NAME(_current_task);

	/* switch to new task (p1 contains new task) */
	p5 = r1;
	[p5] = p1;

	/* restore the kernel stack pointer */
	sp = [p1+(TASK_THREAD+THREAD_KSP)];

	/* restore user stack pointer */
	p0 = [p1+(TASK_THREAD+THREAD_USP)];
	usp = p0;

	/* restore status register */
	r7 = [p1+(TASK_THREAD+THREAD_SR)];
	seqstat = r7;

	/* restore pc */
	p0 = [p1+(TASK_THREAD+THREAD_PC)];
	jump (p0); 

	/*
	 * While we don't rely on compiler magic, nothing prevents us from
	 * simple jumps.
	 * Following code actually lands up in a new (old) task. .. akale
	 */

new_old_task:	
	fp = [sp++];

	/* You may get assembler errors if you modify following instruction.
	 * Had to do lots of experiments to get it right. The assembler simply
	 * doesn't accept a correct syntax - amit */
	( R7:6, p5 : 3) = [sp++];

	rets = [sp++];

	/*
	 * When we come out of resume, r0 carries "old" task, becuase we are
	 * in "new" task.
	 */

	rts;


.data
ALIGN
SYMBOL_NAME_LABEL(sys_call_table)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 0  -  old "setup()" system call*/
	.long SYMBOL_NAME(sys_exit)
	.long SYMBOL_NAME(sys_fork)
	.long SYMBOL_NAME(sys_read)
	.long SYMBOL_NAME(sys_write)
	.long SYMBOL_NAME(sys_open)		/* 5 */
	.long SYMBOL_NAME(sys_close)
	.long SYMBOL_NAME(sys_waitpid)
	.long SYMBOL_NAME(sys_creat)
	.long SYMBOL_NAME(sys_link)
	.long SYMBOL_NAME(sys_unlink)		/* 10 */
	.long SYMBOL_NAME(sys_execve)
	.long SYMBOL_NAME(sys_chdir)
	.long SYMBOL_NAME(sys_time)
	.long SYMBOL_NAME(sys_mknod)
	.long SYMBOL_NAME(sys_chmod)		/* 15 */
	.long SYMBOL_NAME(sys_chown16)
	.long SYMBOL_NAME(sys_ni_syscall)	/*old break syscall holder*/
	.long SYMBOL_NAME(sys_stat)
	.long SYMBOL_NAME(sys_lseek)
	.long SYMBOL_NAME(sys_getpid)		/* 20 */
	.long SYMBOL_NAME(sys_mount)
	.long SYMBOL_NAME(sys_oldumount)
	.long SYMBOL_NAME(sys_setuid16)
	.long SYMBOL_NAME(sys_getuid16)
	.long SYMBOL_NAME(sys_stime)		/* 25 */
	.long SYMBOL_NAME(sys_ptrace)
	.long SYMBOL_NAME(sys_alarm)
	.long SYMBOL_NAME(sys_fstat)
	.long SYMBOL_NAME(sys_pause)
	.long SYMBOL_NAME(sys_utime)		/* 30 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old stty syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old gtty syscall holder */
	.long SYMBOL_NAME(sys_access)
	.long SYMBOL_NAME(sys_nice)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 35 old ftime syscall */
	.long SYMBOL_NAME(sys_sync)
	.long SYMBOL_NAME(sys_kill)
	.long SYMBOL_NAME(sys_rename)
	.long SYMBOL_NAME(sys_mkdir)
	.long SYMBOL_NAME(sys_rmdir)		/* 40 */
	.long SYMBOL_NAME(sys_dup)
	.long SYMBOL_NAME(sys_pipe)
	.long SYMBOL_NAME(sys_times)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old prof syscall holder */
	.long SYMBOL_NAME(sys_brk)		/* 45 */
	.long SYMBOL_NAME(sys_setgid16)
	.long SYMBOL_NAME(sys_getgid16)
	.long SYMBOL_NAME(sys_signal)
	.long SYMBOL_NAME(sys_geteuid16)
	.long SYMBOL_NAME(sys_getegid16)	/* 50 */
	.long SYMBOL_NAME(sys_acct)
	.long SYMBOL_NAME(sys_umount)		/* recycled unused phys() */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old lock syscall holder */
	.long SYMBOL_NAME(sys_ioctl)
	.long SYMBOL_NAME(sys_fcntl)		/* 55 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* old mpx syscall holder */
	.long SYMBOL_NAME(sys_setpgid)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old ulimit syscall */
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_umask)		/* 60 */
	.long SYMBOL_NAME(sys_chroot)
	.long SYMBOL_NAME(sys_ustat)
	.long SYMBOL_NAME(sys_dup2)
	.long SYMBOL_NAME(sys_getppid)
	.long SYMBOL_NAME(sys_getpgrp)		/* 65 */
	.long SYMBOL_NAME(sys_setsid)
	.long SYMBOL_NAME(sys_sigaction)
	.long SYMBOL_NAME(sys_sgetmask)
	.long SYMBOL_NAME(sys_ssetmask)
	.long SYMBOL_NAME(sys_setreuid16)	/* 70 */
	.long SYMBOL_NAME(sys_setregid16)
	.long SYMBOL_NAME(sys_sigsuspend)
	.long SYMBOL_NAME(sys_sigpending)
	.long SYMBOL_NAME(sys_sethostname)
	.long SYMBOL_NAME(sys_setrlimit)	/* 75 */
	.long SYMBOL_NAME(sys_old_getrlimit)
	.long SYMBOL_NAME(sys_getrusage)
	.long SYMBOL_NAME(sys_gettimeofday)
	.long SYMBOL_NAME(sys_settimeofday)
	.long SYMBOL_NAME(sys_getgroups16)	/* 80 */
	.long SYMBOL_NAME(sys_setgroups16)
	.long SYMBOL_NAME(old_select)
	.long SYMBOL_NAME(sys_symlink)
	.long SYMBOL_NAME(sys_lstat)
	.long SYMBOL_NAME(sys_readlink)		/* 85 */
	.long SYMBOL_NAME(sys_uselib)
	.long SYMBOL_NAME(sys_swapon)
	.long SYMBOL_NAME(sys_reboot)
	.long SYMBOL_NAME(old_readdir)
	.long SYMBOL_NAME(old_mmap)		/* 90 */
	.long SYMBOL_NAME(sys_munmap)
	.long SYMBOL_NAME(sys_truncate)
	.long SYMBOL_NAME(sys_ftruncate)
	.long SYMBOL_NAME(sys_fchmod)
	.long SYMBOL_NAME(sys_fchown16)		/* 95 */
	.long SYMBOL_NAME(sys_getpriority)
	.long SYMBOL_NAME(sys_setpriority)
	.long SYMBOL_NAME(sys_ni_syscall)	/* old profil syscall */
	.long SYMBOL_NAME(sys_statfs)
	.long SYMBOL_NAME(sys_fstatfs)		/* 100 */
	.long SYMBOL_NAME(sys_ioperm)
	.long SYMBOL_NAME(sys_socketcall)
	.long SYMBOL_NAME(sys_syslog)
	.long SYMBOL_NAME(sys_setitimer)
	.long SYMBOL_NAME(sys_getitimer)	/* 105 */
	.long SYMBOL_NAME(sys_newstat)
	.long SYMBOL_NAME(sys_newlstat)
	.long SYMBOL_NAME(sys_newfstat)
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_ni_syscall)	/* iopl for i386 */ /* 110 */
	.long SYMBOL_NAME(sys_vhangup)
	.long SYMBOL_NAME(sys_ni_syscall)	/* obsolete idle() syscall */
	.long SYMBOL_NAME(sys_ni_syscall)	/* vm86old for i386 */
	.long SYMBOL_NAME(sys_wait4)
	.long SYMBOL_NAME(sys_swapoff)		/* 115 */
	.long SYMBOL_NAME(sys_sysinfo)
	.long SYMBOL_NAME(sys_ipc)
	.long SYMBOL_NAME(sys_fsync)
	.long SYMBOL_NAME(sys_sigreturn)
	.long SYMBOL_NAME(sys_clone)		/* 120 */
	.long SYMBOL_NAME(sys_setdomainname)
	.long SYMBOL_NAME(sys_newuname)
	.long SYMBOL_NAME(sys_cacheflush)	/* modify_ldt for i386 */
	.long SYMBOL_NAME(sys_adjtimex)
	.long SYMBOL_NAME(sys_mprotect)		/* 125 */
	.long SYMBOL_NAME(sys_sigprocmask)
	.long SYMBOL_NAME(sys_create_module)
	.long SYMBOL_NAME(sys_init_module)
	.long SYMBOL_NAME(sys_delete_module)
	.long SYMBOL_NAME(sys_get_kernel_syms)	/* 130 */
	.long SYMBOL_NAME(sys_quotactl)
	.long SYMBOL_NAME(sys_getpgid)
	.long SYMBOL_NAME(sys_fchdir)
	.long SYMBOL_NAME(sys_bdflush)
	.long SYMBOL_NAME(sys_sysfs)		/* 135 */
	.long SYMBOL_NAME(sys_personality)
	.long SYMBOL_NAME(sys_ni_syscall)	/* for afs_syscall */
	.long SYMBOL_NAME(sys_setfsuid16)
	.long SYMBOL_NAME(sys_setfsgid16)
	.long SYMBOL_NAME(sys_llseek)		/* 140 */
	.long SYMBOL_NAME(sys_getdents)
	.long SYMBOL_NAME(sys_select)
	.long SYMBOL_NAME(sys_flock)
	.long SYMBOL_NAME(sys_msync)
	.long SYMBOL_NAME(sys_readv)		/* 145 */
	.long SYMBOL_NAME(sys_writev)
	.long SYMBOL_NAME(sys_getsid)
	.long SYMBOL_NAME(sys_fdatasync)
	.long SYMBOL_NAME(sys_sysctl)
	.long SYMBOL_NAME(sys_mlock)		/* 150 */
	.long SYMBOL_NAME(sys_munlock)
	.long SYMBOL_NAME(sys_mlockall)
	.long SYMBOL_NAME(sys_munlockall)
	.long SYMBOL_NAME(sys_sched_setparam)
	.long SYMBOL_NAME(sys_sched_getparam)   /* 155 */
	.long SYMBOL_NAME(sys_sched_setscheduler)
	.long SYMBOL_NAME(sys_sched_getscheduler)
	.long SYMBOL_NAME(sys_sched_yield)
	.long SYMBOL_NAME(sys_sched_get_priority_max)
	.long SYMBOL_NAME(sys_sched_get_priority_min)  /* 160 */
	.long SYMBOL_NAME(sys_sched_rr_get_interval)
	.long SYMBOL_NAME(sys_nanosleep)
	.long SYMBOL_NAME(sys_mremap)
	.long SYMBOL_NAME(sys_setresuid16)
	.long SYMBOL_NAME(sys_getresuid16)	/* 165 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* for vm86 */
	.long SYMBOL_NAME(sys_query_module)
	.long SYMBOL_NAME(sys_poll)
	.long SYMBOL_NAME(sys_nfsservctl)
	.long SYMBOL_NAME(sys_setresgid16)	/* 170 */
	.long SYMBOL_NAME(sys_getresgid16)
	.long SYMBOL_NAME(sys_prctl)
	.long SYMBOL_NAME(sys_rt_sigreturn)
	.long SYMBOL_NAME(sys_rt_sigaction)
	.long SYMBOL_NAME(sys_rt_sigprocmask)	/* 175 */
	.long SYMBOL_NAME(sys_rt_sigpending)
	.long SYMBOL_NAME(sys_rt_sigtimedwait)
	.long SYMBOL_NAME(sys_rt_sigqueueinfo)
	.long SYMBOL_NAME(sys_rt_sigsuspend)
	.long SYMBOL_NAME(sys_pread)		/* 180 */
	.long SYMBOL_NAME(sys_pwrite)
	.long SYMBOL_NAME(sys_lchown16);
	.long SYMBOL_NAME(sys_getcwd)
	.long SYMBOL_NAME(sys_capget)
	.long SYMBOL_NAME(sys_capset)           /* 185 */
	.long SYMBOL_NAME(sys_sigaltstack)
	.long SYMBOL_NAME(sys_sendfile)
	.long SYMBOL_NAME(sys_ni_syscall)	/* streams1 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* streams2 */
	.long SYMBOL_NAME(sys_vfork)            /* 190 */
	.long SYMBOL_NAME(sys_getrlimit)
	.long SYMBOL_NAME(sys_mmap2)
	.long SYMBOL_NAME(sys_truncate64)
	.long SYMBOL_NAME(sys_ftruncate64)
	.long SYMBOL_NAME(sys_stat64)		/* 195 */
	.long SYMBOL_NAME(sys_lstat64)
	.long SYMBOL_NAME(sys_fstat64)
	.long SYMBOL_NAME(sys_chown)
	.long SYMBOL_NAME(sys_getuid)
	.long SYMBOL_NAME(sys_getgid)		/* 200 */
	.long SYMBOL_NAME(sys_geteuid)
	.long SYMBOL_NAME(sys_getegid)
	.long SYMBOL_NAME(sys_setreuid)
	.long SYMBOL_NAME(sys_setregid)
	.long SYMBOL_NAME(sys_getgroups)	/* 205 */
	.long SYMBOL_NAME(sys_setgroups)
	.long SYMBOL_NAME(sys_fchown)
	.long SYMBOL_NAME(sys_setresuid)
	.long SYMBOL_NAME(sys_getresuid)
	.long SYMBOL_NAME(sys_setresgid)	/* 210 */
	.long SYMBOL_NAME(sys_getresgid)
	.long SYMBOL_NAME(sys_lchown)
	.long SYMBOL_NAME(sys_setuid)
	.long SYMBOL_NAME(sys_setgid)
	.long SYMBOL_NAME(sys_setfsuid)		/* 215 */
	.long SYMBOL_NAME(sys_setfsgid)
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_ni_syscall)
	.long SYMBOL_NAME(sys_getdents64)	/* 220 */

	/* no recognization now by nisa-gcc */
	.rept NR_syscalls-(.-SYMBOL_NAME(sys_call_table))/4	
	.long SYMBOL_NAME(sys_ni_syscall)
	.endr
