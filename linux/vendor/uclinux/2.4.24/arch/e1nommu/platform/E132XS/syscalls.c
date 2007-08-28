/*
 *  arch/e1nommu/platform/E132XS/syscalls.c
 *
 *  Copyrights: (C) 2002 GDT,  George Thanos<george.thanos@gdt.gr>
 *                             Yannis Mitsos<yannis.mitsos@gdt.gr>
 */
#include  <linux/sys.h>
#include  <asm/syscalls.h>

#define L              regs.L
#define FL             regs.FL
#define RETURN_VALUE   L [ FL-1 ]
#define SYSCALL_ENTRY  L [ FL-2 ]
#define ARG1	       L [ FL-3 ]
#define ARG2	       L [ FL-4 ]
#define ARG3	       L [ FL-5 ]
#define ARG4	       L [ FL-6 ]
#define ARG5	       L [ FL-7 ]
#define ARG6	       L [ FL-8 ]

#define SYSCALL_ARG_0( name )	 			\
void e132xs_##name(struct pt_regs regs ) 		\
{ 				  			\
	RETURN_VALUE = sys_##name( );			\
}

#define SYSCALL_ARG_1( name, type1 ) 			\
void e132xs_##name(struct pt_regs regs ) 		\
{ 				  			\
	RETURN_VALUE = sys_##name( (type1)ARG1 );	\
}

#define SYSCALL_ARG_2( name, type1, type2)  			\
void e132xs_##name(struct pt_regs regs )			\
{ 				  				\
	RETURN_VALUE = sys_##name( (type1)ARG1, (type2)ARG2 );	\
}

#define SYSCALL_ARG_3( name, type1, type2, type3 ) 				\
	void e132xs_##name(struct pt_regs regs ) 				\
{ 				  						\
	RETURN_VALUE = sys_##name( (type1)ARG1, (type2)ARG2, (type3)ARG3 );	\
}

#define SYSCALL_ARG_4( name, type1, type2, type3, type4) 				\
void e132xs_##name(struct pt_regs regs ) 							\
{ 				  								\
	RETURN_VALUE = sys_##name( (type1)ARG1, (type2)ARG2, (type3)ARG3, (type4)ARG4 );	\
}

#define SYSCALL_ARG_5( name, type1, type2, type3, type4, type5 ) 					\
void e132xs_##name(struct pt_regs regs ) 								\
{ 				  									\
	RETURN_VALUE = sys_##name( (type1)ARG1, (type2)ARG2, (type3)ARG3, (type4)ARG4, (type5)ARG5 );	\
}

#define SYSCALL_ARG_6( name, type1, type2, type3, type4, type5, type6 ) 					\
void e132xs_##name(struct pt_regs regs ) 									\
{ 				  										\
	RETURN_VALUE = sys_##name( (type1)ARG1, (type2)ARG2, (type3)ARG3, (type4)ARG4, (type5)ARG5, (type6)ARG6 );\
}

#define SYSCALL_REGS( name )	 			\
void e132xs_##name(struct pt_regs regs ) 		\
{ 				  			\
	RETURN_VALUE = sys_##name( regs );		\
}

SYSCALL_ARG_0(ni_syscall) /*System Call Not Implemented - ERROR */
SYSCALL_ARG_1(exit, int ) /* 1 */ 
SYSCALL_REGS(fork)
SYSCALL_ARG_3(read, unsigned int, char *, size_t)
SYSCALL_ARG_3(write, unsigned int, const char *, size_t)
SYSCALL_ARG_3(open, const char *, int, int) /*5*/
SYSCALL_ARG_1(close, unsigned int)
SYSCALL_ARG_3(waitpid, pid_t, unsigned int *, int)
SYSCALL_ARG_2(creat, const char *, int)
SYSCALL_ARG_2(link, const char *, const char *)
SYSCALL_ARG_1(unlink, const char *) /*10*/
#if 0
SYSCALL_REGS(execve)
#else
/* In order to modify the Kernel Aggregate SP
 * before we return to the execution of the new process
 * we adopted this ugly scheme.
 */
void Switch_AggregateStackValues(void);
void e132xs_execve(struct pt_regs regs )
{
	RETURN_VALUE = sys_execve( regs );
	Switch_AggregateStackValues();
}
#endif
SYSCALL_ARG_1(chdir, const char *)
SYSCALL_ARG_1(time, int *)
SYSCALL_ARG_3(mknod, const char *, int, dev_t)
SYSCALL_ARG_2(chmod, const char *, mode_t) /*15*/
SYSCALL_ARG_3(chown, const char *, uid_t, gid_t)
/* sys_break is not used */
/* sys_oldstat is not used */
SYSCALL_ARG_3(lseek,unsigned int,off_t,unsigned int)
SYSCALL_ARG_0(getpid) /*20*/
SYSCALL_ARG_5(mount,char *, char *, char *, unsigned long, void *)
SYSCALL_ARG_2(umount,char *, int)
SYSCALL_ARG_1(setuid,uid_t)
SYSCALL_ARG_0(getuid)
SYSCALL_ARG_1(stime, int *) /*25*/
/*SYSCALL_ARG_4(ptrace,long,long,long,long) not available FIXME */
SYSCALL_ARG_1(alarm, unsigned int)
/* sys_oldfstat is not used */
SYSCALL_ARG_0(pause)
SYSCALL_ARG_2(utime,char *, struct utimbuf *) /*30*/
/* sys_stty is not used */
/* sys_gtty is not used */
SYSCALL_ARG_2(access,const char *, int)
SYSCALL_ARG_1(nice,int)
/* sys_ftime is not used */ /*35*/
SYSCALL_ARG_0(sync)
SYSCALL_ARG_2(kill,int, int)
SYSCALL_ARG_2(rename,const char *, const char *)
SYSCALL_ARG_2(mkdir,const char *, int)
SYSCALL_ARG_1(rmdir,const char *) /*40*/
SYSCALL_ARG_1(dup,int)
SYSCALL_ARG_1(pipe, unsigned long *)
SYSCALL_ARG_1(times,struct tms *)
/* sys_prof is not used */
SYSCALL_ARG_1(brk,long)  /*45*/
SYSCALL_ARG_1(setgid,gid_t)
SYSCALL_ARG_0(getgid)
SYSCALL_ARG_2(signal,int, __sighandler_t)
SYSCALL_ARG_0(geteuid)
SYSCALL_ARG_0(getegid) /*50*/
SYSCALL_ARG_1(acct, const char *)
/* sys_umount2 is not used */
/* sys_lock is not used */
SYSCALL_ARG_3(ioctl,unsigned int, unsigned int, unsigned long)
SYSCALL_ARG_3(fcntl,unsigned int, unsigned int, unsigned long) /*55*/
/* sys_mpx is not used */
SYSCALL_ARG_2(setpgid,pid_t, pid_t)
/* sys_ulimit is not used */
/* sys_oldolduname is not used */
SYSCALL_ARG_1(umask,int) /*60*/
SYSCALL_ARG_1(chroot,const char *)
SYSCALL_ARG_2(ustat,dev_t, struct ustat *)
SYSCALL_ARG_2(dup2, int, int)
SYSCALL_ARG_0(getppid)
SYSCALL_ARG_0(getpgrp) /*65*/
SYSCALL_ARG_0(setsid)
SYSCALL_ARG_3(sigaction,int, const struct sigaction *, struct sigaction *)
SYSCALL_ARG_0(sgetmask)
SYSCALL_ARG_1(ssetmask,int)
SYSCALL_ARG_2(setreuid, uid_t, uid_t) /*70*/
SYSCALL_ARG_2(setregid, uid_t, uid_t)
SYSCALL_REGS(sigsuspend)
SYSCALL_ARG_1(sigpending,old_sigset_t *)
SYSCALL_ARG_2(sethostname, char *, int)
SYSCALL_ARG_2(setrlimit,unsigned int, struct rlimit *)  /*75*/
SYSCALL_ARG_2(old_getrlimit, unsigned int, struct rlimit *)
SYSCALL_ARG_2(getrusage, int, struct rusage *)
SYSCALL_ARG_2(gettimeofday, struct timeval *, struct timezone *)
SYSCALL_ARG_2(settimeofday, struct timeval *, struct timezone *)
SYSCALL_ARG_2(getgroups, int, gid_t *) /*80*/
SYSCALL_ARG_2(setgroups, int, gid_t *)
SYSCALL_ARG_1(old_select, struct sel_arg_struct *)
SYSCALL_ARG_2(symlink,const char *, const char *)
/* sys_oldstat is not used */
SYSCALL_ARG_3(readlink, const char *, char *, int)
SYSCALL_ARG_1(uselib, const char *)
SYSCALL_ARG_2(swapon, const char *, int)
SYSCALL_ARG_4(reboot, int, int, unsigned int, void *)
/* sys_readdir is not used */
SYSCALL_ARG_1(old_mmap, struct mmap_arg_struct *) /*90*/
SYSCALL_ARG_2(munmap, unsigned long, size_t)
SYSCALL_ARG_2(truncate, const char *, unsigned long)
SYSCALL_ARG_2(ftruncate, unsigned int, unsigned long)
SYSCALL_ARG_2(fchmod, unsigned int, mode_t)
SYSCALL_ARG_3(fchown, unsigned int, uid_t, gid_t)
SYSCALL_ARG_2(getpriority, int, int)
SYSCALL_ARG_3(setpriority, int, int, int)
/* sys_profil is not used*/
SYSCALL_ARG_2(statfs, char *, struct statfs *)
SYSCALL_ARG_2(fstatfs, unsigned int, struct statfs *) /*100*/
SYSCALL_ARG_3(ioperm, unsigned long, unsigned long, int)
SYSCALL_ARG_2(socketcall, int, unsigned long *)
SYSCALL_ARG_3(syslog,int, char *, int)
SYSCALL_ARG_3(setitimer, int, struct itimerval *, struct itimerval *)
SYSCALL_ARG_2(getitimer, int, struct itimerval *)
SYSCALL_ARG_2(newstat, char *, struct __old_kernel_stat *)
SYSCALL_ARG_2(newlstat, char *, struct __old_kernel_stat *)
SYSCALL_ARG_2(newfstat, unsigned int, struct __old_kernel_stat *)
SYSCALL_ARG_1(olduname, struct old_utsname *)
/* sys_iopl is not used*/ /*110*/
SYSCALL_ARG_0(vhangup)
/* sys_idle is not used*/
/* sys_vm86 is not used*/
SYSCALL_ARG_4(wait4, pid_t, unsigned int *, int, struct rusage *)
SYSCALL_ARG_1(swapoff, const char *)
SYSCALL_ARG_1(sysinfo, struct sysinfo *)
SYSCALL_ARG_6(ipc, uint, int, int, int, void *, long)
SYSCALL_ARG_1(fsync, unsigned int)
SYSCALL_ARG_0(sigreturn)
SYSCALL_REGS(clone) /*120*/
SYSCALL_ARG_2(setdomainname,char *,int)
SYSCALL_ARG_1(newuname, struct new_utsname *)
/*SYSCALL_ARG_4(cacheflush, unsigned long, int, int, unsigned long) not available FIXME */
SYSCALL_ARG_1(adjtimex, struct timex *)
SYSCALL_ARG_3(mprotect, unsigned long, size_t, unsigned long)
SYSCALL_ARG_3(sigprocmask, int, old_sigset_t *, old_sigset_t *)
SYSCALL_ARG_2(create_module, const char *, size_t)
SYSCALL_ARG_2(init_module, const char *, struct module *)
SYSCALL_ARG_1(delete_module, const char *)
SYSCALL_ARG_1(get_kernel_syms, struct kernel_sym *) /*130*/
SYSCALL_ARG_4(quotactl, int, const char *, int, caddr_t)
SYSCALL_ARG_1(getpgid, pid_t)
SYSCALL_ARG_1(fchdir, unsigned int)
SYSCALL_ARG_2(bdflush, int, long)
SYSCALL_ARG_3(sysfs, int, unsigned long, unsigned long) /*135*/
SYSCALL_ARG_1(personality, unsigned long)
/* sys_afs_syscall not implemented*/
SYSCALL_ARG_1(setfsuid, uid_t)
SYSCALL_ARG_1(setfsgid, gid_t)
SYSCALL_ARG_5(llseek, unsigned int , unsigned long , unsigned long, loff_t, unsigned int)
SYSCALL_ARG_3(getdents, unsigned int, void *, unsigned int)
SYSCALL_ARG_5(select,int, fd_set *, fd_set *, fd_set *, struct timeval *)
SYSCALL_ARG_2(flock, unsigned int, unsigned int)
SYSCALL_ARG_3(msync, unsigned long, size_t, int)
SYSCALL_ARG_3(readv, unsigned long, const struct iovec *, unsigned long)
SYSCALL_ARG_3(writev,unsigned long, const struct iovec *, unsigned long)
SYSCALL_ARG_1(getsid, pid_t)
SYSCALL_ARG_1(fdatasync, unsigned int)
SYSCALL_ARG_1(sysctl, struct __sysctl_args *)
SYSCALL_ARG_2(mlock, unsigned long, size_t) /*150*/
SYSCALL_ARG_2(munlock, unsigned long, size_t)
SYSCALL_ARG_1(mlockall,int )
SYSCALL_ARG_0(munlockall)
SYSCALL_ARG_2(sched_setparam, pid_t, struct sched_param *)
SYSCALL_ARG_2(sched_getparam, pid_t, struct sched_param *)
SYSCALL_ARG_3(sched_setscheduler, pid_t, int, struct sched_param *)
SYSCALL_ARG_1(sched_getscheduler, pid_t)
SYSCALL_ARG_0(sched_yield)
SYSCALL_ARG_1(sched_get_priority_max, int)
SYSCALL_ARG_1(sched_get_priority_min, int) /*160*/
SYSCALL_ARG_2(sched_rr_get_interval, pid_t, struct timespec *)
SYSCALL_ARG_2(nanosleep, struct timespec *, struct timespec *)
SYSCALL_ARG_5(mremap, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)
SYSCALL_ARG_3(setresuid, uid_t, uid_t, uid_t)
SYSCALL_ARG_3(getresuid, uid_t *, uid_t *, uid_t *) /*165*/
/*166 is missing*/
SYSCALL_ARG_5(query_module, const char *, int, char *, size_t, size_t *)
SYSCALL_ARG_3(poll, struct pollfd *, unsigned int, long)
SYSCALL_ARG_3(nfsservctl, int, void *, void *)
SYSCALL_ARG_3(setresgid, gid_t, gid_t, gid_t) /*170*/
SYSCALL_ARG_3(getresgid, gid_t *, gid_t *, gid_t *)
SYSCALL_ARG_5(prctl, int, unsigned long, unsigned long, unsigned long, unsigned long)
/*SYSCALL_ARG_1(rt_sigreturn, ) * FIXME */
SYSCALL_ARG_4(rt_sigaction, int, const struct sigaction *, struct sigaction *, size_t)
SYSCALL_ARG_4(rt_sigprocmask, int, sigset_t *, sigset_t *, size_t)
SYSCALL_ARG_2(rt_sigpending, sigset_t *, size_t)
SYSCALL_ARG_4(rt_sigtimedwait, const sigset_t *, siginfo_t *, const struct timespec *, size_t)
SYSCALL_ARG_3(rt_sigqueueinfo, int, int, siginfo_t *)
SYSCALL_REGS(rt_sigsuspend)
SYSCALL_ARG_4(pread, unsigned int, char *, size_t, loff_t) /*180*/
SYSCALL_ARG_4(pwrite, unsigned int, const char *, size_t, loff_t)
SYSCALL_ARG_3(lchown, const char *, uid_t,gid_t)
SYSCALL_ARG_2(getcwd, char *, unsigned long)
SYSCALL_ARG_2(capget, cap_user_header_t, cap_user_data_t)
SYSCALL_ARG_2(capset, cap_user_header_t, const cap_user_data_t)
/*SYSCALL_ARG_2(sigaltstack ) * FIXME */
SYSCALL_ARG_4(sendfile, int, int, off_t *, size_t)
/* getpmsg is not used */
/* putpmsg is not used */
SYSCALL_REGS(vfork) /*190*/
SYSCALL_ARG_2(getrlimit, unsigned int, struct rlimit *) /* FIXME */
SYSCALL_ARG_6(mmap2, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long )
SYSCALL_ARG_2(truncate64, const char *, loff_t)
SYSCALL_ARG_2(ftruncate64, unsigned int, loff_t)
SYSCALL_ARG_3(stat64, char *, struct stat64 *, long)
SYSCALL_ARG_3(lstat64, char *, struct stat64 *, long)
SYSCALL_ARG_3(fstat64, unsigned long, struct stat64 *, long)
/* chown32 is not used  */
/* getuid32 is not used  */
/* getgid32 is not used  */ /*200*/
/* geteuid32 is not used  */
/* getegid32 is not used  */
/* setreuid32 is not used  */
/* setregid32 is not used  */
/* getgroups32 is not used  */
/* setgroups32 is not used  */
/* fchown32 is not used  */
/* setresuid32 is not used  */
/* getresuid32 is not used  */
/* setresgid32 is not used  */ /*210*/
/* getresgid32 is not used  */
/* lchown32 is not used  */
/* setuid32 is not used  */
/* setgid32 is not used  */
/* setfsuid32 is not used  */ /*215*/
/* setfsgid32 is not used  */
/* Number 217 is not used */
/* Number 218 is not used */
SYSCALL_ARG_2(pivot_root, const char *, const char *)
SYSCALL_ARG_3(getdents64, unsigned int, void *, unsigned int) /* 220 */
SYSCALL_ARG_0(gettid)
SYSCALL_ARG_2(tkill, int, int)
SYSCALL_ARG_2(kprintf, char *, int)
SYSCALL_ARG_1(e1newSP, unsigned long)

typedef struct {
	void (*handler)(struct pt_regs);
} syscall_entry_t;

extern syscall_entry_t  SysCall_Table[];

#define SYSCALLS_DEBUG  0 
void Common_SysCall_Handler(struct pt_regs regs)
{
	unsigned long entry = SYSCALL_ENTRY;
	RETURN_VALUE = -ENOSYS;

#if SYSCALLS_DEBUG
	printk("SYSCALLS_DEBUG: entry : %d  <-> L : 0x%x, FL : %d\n", entry, L, FL );
#endif

	if( entry > NR_syscalls || !SysCall_Table[entry].handler )
			return;
	sti();
	SysCall_Table[entry].handler(regs);

	if(current->thread.vfork_ret_info.ret_from_vfork) {
		L[1] = current->thread.vfork_ret_info.ReturnPC;
		L[2] = current->thread.vfork_ret_info.ReturnSR;
		current->thread.vfork_ret_info.ret_from_vfork = 0;
	}
}
