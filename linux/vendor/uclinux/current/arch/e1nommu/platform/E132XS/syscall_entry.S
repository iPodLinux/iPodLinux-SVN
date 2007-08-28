/*
 *  arch/e1nommu/platform/E132XS/syscall_entry.S
 *
 *  Copyright (C) 2002 GDT,  George Thanos<george.thanos@gdt.gr>
 *                           Yannis Mitsos<yiannis.mitsos@gdt.gr>
 */
#include <linux/linkage.h>

.data
.balign 4
.global _SysCall_Table
_SysCall_Table:
.long 0
.long SYMBOL_NAME(_e132xs_exit) /*1*/
.long SYMBOL_NAME(_e132xs_fork)
.long SYMBOL_NAME(_e132xs_read)
.long SYMBOL_NAME(_e132xs_write)
.long SYMBOL_NAME(_e132xs_open) /*5*/
.long SYMBOL_NAME(_e132xs_close)
.long SYMBOL_NAME(_e132xs_waitpid)
.long SYMBOL_NAME(_e132xs_creat)
.long SYMBOL_NAME(_e132xs_link)
.long SYMBOL_NAME(_e132xs_unlink) /*10*/
.long SYMBOL_NAME(_e132xs_execve)
.long SYMBOL_NAME(_e132xs_chdir)
.long SYMBOL_NAME(_e132xs_time)
.long SYMBOL_NAME(_e132xs_mknod)
.long SYMBOL_NAME(_e132xs_chmod) /*15*/
.long SYMBOL_NAME(_e132xs_chown)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_break is not used */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_oldstat is not used */
.long SYMBOL_NAME(_e132xs_lseek)
.long SYMBOL_NAME(_e132xs_getpid) /*20*/
.long SYMBOL_NAME(_e132xs_mount)
.long SYMBOL_NAME(_e132xs_umount)
.long SYMBOL_NAME(_e132xs_setuid)
.long SYMBOL_NAME(_e132xs_getuid)
.long SYMBOL_NAME(_e132xs_stime)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_ptrace FIXME */
.long SYMBOL_NAME(_e132xs_alarm)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_oldfstat is not used */
.long SYMBOL_NAME(_e132xs_pause)
.long SYMBOL_NAME(_e132xs_utime) /*30*/
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_stty is not used */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_gtty is not used */
.long SYMBOL_NAME(_e132xs_access)
.long SYMBOL_NAME(_e132xs_nice)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_ftime is not used */
.long SYMBOL_NAME(_e132xs_sync)
.long SYMBOL_NAME(_e132xs_kill)
.long SYMBOL_NAME(_e132xs_rename)
.long SYMBOL_NAME(_e132xs_mkdir)
.long SYMBOL_NAME(_e132xs_rmdir) /*40*/
.long SYMBOL_NAME(_e132xs_dup)
.long SYMBOL_NAME(_e132xs_pipe)
.long SYMBOL_NAME(_e132xs_times)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_prof is not used */
.long SYMBOL_NAME(_e132xs_brk)
.long SYMBOL_NAME(_e132xs_setgid)
.long SYMBOL_NAME(_e132xs_getgid)
.long SYMBOL_NAME(_e132xs_signal)
.long SYMBOL_NAME(_e132xs_geteuid)
.long SYMBOL_NAME(_e132xs_getegid) /*50*/
.long SYMBOL_NAME(_e132xs_acct)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_umount2 is not used */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_lock is not used */
.long SYMBOL_NAME(_e132xs_ioctl)
.long SYMBOL_NAME(_e132xs_fcntl)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_mpx is not used */
.long SYMBOL_NAME(_e132xs_setpgid)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_ulimit is not used */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_oldolduname is not used */
.long SYMBOL_NAME(_e132xs_umask) /*60*/
.long SYMBOL_NAME(_e132xs_chroot)
.long SYMBOL_NAME(_e132xs_ustat)
.long SYMBOL_NAME(_e132xs_dup2)
.long SYMBOL_NAME(_e132xs_getppid)
.long SYMBOL_NAME(_e132xs_getpgrp)
.long SYMBOL_NAME(_e132xs_setsid)
.long SYMBOL_NAME(_e132xs_sigaction) 
.long SYMBOL_NAME(_e132xs_sgetmask)
.long SYMBOL_NAME(_e132xs_ssetmask)
.long SYMBOL_NAME(_e132xs_setreuid) /*70*/
.long SYMBOL_NAME(_e132xs_setregid)
.long SYMBOL_NAME(_e132xs_sigsuspend)
.long SYMBOL_NAME(_e132xs_sigpending)
.long SYMBOL_NAME(_e132xs_sethostname)
.long SYMBOL_NAME(_e132xs_setrlimit)
.long SYMBOL_NAME(_e132xs_old_getrlimit)
.long SYMBOL_NAME(_e132xs_getrusage)
.long SYMBOL_NAME(_e132xs_gettimeofday)
.long SYMBOL_NAME(_e132xs_settimeofday)
.long SYMBOL_NAME(_e132xs_getgroups) /*80*/
.long SYMBOL_NAME(_e132xs_setgroups)
.long SYMBOL_NAME(_e132xs_old_select)
.long SYMBOL_NAME(_e132xs_symlink)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_oldstat is not used */
.long SYMBOL_NAME(_e132xs_readlink)
.long SYMBOL_NAME(_e132xs_uselib)
.long SYMBOL_NAME(_e132xs_swapon)
.long SYMBOL_NAME(_e132xs_reboot)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_readdir is not used */
.long SYMBOL_NAME(_e132xs_old_mmap) /*90*/
.long SYMBOL_NAME(_e132xs_munmap)
.long SYMBOL_NAME(_e132xs_truncate)
.long SYMBOL_NAME(_e132xs_ftruncate)
.long SYMBOL_NAME(_e132xs_fchmod)
.long SYMBOL_NAME(_e132xs_fchown)
.long SYMBOL_NAME(_e132xs_getpriority)
.long SYMBOL_NAME(_e132xs_setpriority)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_profil is not used*/
.long SYMBOL_NAME(_e132xs_statfs)
.long SYMBOL_NAME(_e132xs_fstatfs) /*100*/
.long SYMBOL_NAME(_e132xs_ioperm)
.long SYMBOL_NAME(_e132xs_socketcall)
.long SYMBOL_NAME(_e132xs_syslog)
.long SYMBOL_NAME(_e132xs_setitimer)
.long SYMBOL_NAME(_e132xs_getitimer)
.long SYMBOL_NAME(_e132xs_newstat)
.long SYMBOL_NAME(_e132xs_newlstat)
.long SYMBOL_NAME(_e132xs_newfstat)
.long SYMBOL_NAME(_e132xs_olduname)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_iopl is not used*/ /*110*/
.long SYMBOL_NAME(_e132xs_vhangup)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_idle is not used*/
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_vm86 is not used*/
.long SYMBOL_NAME(_e132xs_wait4)
.long SYMBOL_NAME(_e132xs_swapoff)
.long SYMBOL_NAME(_e132xs_sysinfo)
.long SYMBOL_NAME(_e132xs_ipc)
.long SYMBOL_NAME(_e132xs_fsync)
.long SYMBOL_NAME(_e132xs_sigreturn)
.long SYMBOL_NAME(_e132xs_clone)  /*120*/
.long SYMBOL_NAME(_e132xs_setdomainname)
.long SYMBOL_NAME(_e132xs_newuname)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_cacheflush FIXME */
.long SYMBOL_NAME(_e132xs_adjtimex)
.long SYMBOL_NAME(_e132xs_mprotect)
.long SYMBOL_NAME(_e132xs_sigprocmask)
.long SYMBOL_NAME(_e132xs_create_module)
.long SYMBOL_NAME(_e132xs_init_module)
.long SYMBOL_NAME(_e132xs_delete_module)
.long SYMBOL_NAME(_e132xs_get_kernel_syms) /* 130 */
.long SYMBOL_NAME(_e132xs_quotactl)
.long SYMBOL_NAME(_e132xs_getpgid)
.long SYMBOL_NAME(_e132xs_fchdir)
.long SYMBOL_NAME(_e132xs_bdflush)
.long SYMBOL_NAME(_e132xs_sysfs)
.long SYMBOL_NAME(_e132xs_personality)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* afs_syscall not implemented*/
.long SYMBOL_NAME(_e132xs_setfsuid)
.long SYMBOL_NAME(_e132xs_setfsgid)
.long SYMBOL_NAME(_e132xs_llseek) /* 140 */
.long SYMBOL_NAME(_e132xs_getdents)
.long SYMBOL_NAME(_e132xs_select)
.long SYMBOL_NAME(_e132xs_flock)
.long SYMBOL_NAME(_e132xs_msync)
.long SYMBOL_NAME(_e132xs_readv)
.long SYMBOL_NAME(_e132xs_writev)
.long SYMBOL_NAME(_e132xs_getsid)
.long SYMBOL_NAME(_e132xs_fdatasync)
.long SYMBOL_NAME(_e132xs_sysctl)
.long SYMBOL_NAME(_e132xs_mlock) /* 150 */
.long SYMBOL_NAME(_e132xs_munlock)
.long SYMBOL_NAME(_e132xs_mlockall)
.long SYMBOL_NAME(_e132xs_munlockall)
.long SYMBOL_NAME(_e132xs_sched_setparam)
.long SYMBOL_NAME(_e132xs_sched_getparam)
.long SYMBOL_NAME(_e132xs_sched_setscheduler)
.long SYMBOL_NAME(_e132xs_sched_getscheduler)
.long SYMBOL_NAME(_e132xs_sched_yield)
.long SYMBOL_NAME(_e132xs_sched_get_priority_max)
.long SYMBOL_NAME(_e132xs_sched_get_priority_min) /* 160 */
.long SYMBOL_NAME(_e132xs_sched_rr_get_interval)
.long SYMBOL_NAME(_e132xs_nanosleep)
.long SYMBOL_NAME(_e132xs_mremap)
.long SYMBOL_NAME(_e132xs_setresuid)
.long SYMBOL_NAME(_e132xs_getresuid)
.long SYMBOL_NAME(_e132xs_ni_syscall) /*Entry is empty - No system call corresponds*/ /*166*/
.long SYMBOL_NAME(_e132xs_query_module)
.long SYMBOL_NAME(_e132xs_poll)
.long SYMBOL_NAME(_e132xs_nfsservctl)
.long SYMBOL_NAME(_e132xs_setresgid) /* 170 */
.long SYMBOL_NAME(_e132xs_getresgid)
.long SYMBOL_NAME(_e132xs_prctl)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_rt_sigreturn FIXME */
.long SYMBOL_NAME(_e132xs_rt_sigaction)
.long SYMBOL_NAME(_e132xs_rt_sigprocmask)
.long SYMBOL_NAME(_e132xs_rt_sigpending)
.long SYMBOL_NAME(_e132xs_rt_sigtimedwait)
.long SYMBOL_NAME(_e132xs_rt_sigqueueinfo)
.long SYMBOL_NAME(_e132xs_rt_sigsuspend)
.long SYMBOL_NAME(_e132xs_pread) /* 180 */
.long SYMBOL_NAME(_e132xs_pwrite)
.long SYMBOL_NAME(_e132xs_lchown)
.long SYMBOL_NAME(_e132xs_getcwd)
.long SYMBOL_NAME(_e132xs_capget)
.long SYMBOL_NAME(_e132xs_capset)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* sys_sigaltstack FIXME */
.long SYMBOL_NAME(_e132xs_sendfile)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* getpmsg is not used */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* putpmsg is not used */
.long SYMBOL_NAME(_e132xs_vfork) /* 190 */
.long SYMBOL_NAME(_e132xs_getrlimit) /* FIXME */
.long SYMBOL_NAME(_e132xs_mmap2) /* mmap2 contains 6 arguments */
.long SYMBOL_NAME(_e132xs_truncate64)
.long SYMBOL_NAME(_e132xs_ftruncate64)
.long SYMBOL_NAME(_e132xs_stat64)
.long SYMBOL_NAME(_e132xs_lstat64)
.long SYMBOL_NAME(_e132xs_fstat64)
.long SYMBOL_NAME(_e132xs_ni_syscall) /* chown32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* getuid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* getgid32 is not used  */ /* 200 */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* geteuid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* getegid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setreuid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setregid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* getgroups32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setgroups32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* fchown32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setresuid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* getresuid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setresgid32 is not used  */ /* 210 */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* getresgid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* lchown32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setuid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setgid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setfsuid32 is not used  */ /* 215 */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* setfsgid32 is not used  */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* number 217 is not used */
.long SYMBOL_NAME(_e132xs_ni_syscall) /* number 218 is not used */
.long SYMBOL_NAME(_e132xs_pivot_root)
.long SYMBOL_NAME(_e132xs_getdents64) /* 220 */
.long SYMBOL_NAME(_e132xs_gettid)
.long SYMBOL_NAME(_e132xs_tkill)
.long SYMBOL_NAME(_e132xs_kprintf)
.long SYMBOL_NAME(_e132xs_e1newSP) /*Hyperstone E1 specific*/
.long 0
.long 0
.long 0
.long 0
.long 0
