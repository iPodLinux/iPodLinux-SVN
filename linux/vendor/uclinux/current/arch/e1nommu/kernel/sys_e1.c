/*  Copyright (C) 2002-2003,    George Thanos <george.thanos@gdt.gr>
 *				Yannis Mitsos <yannis.mitsos@gdt.gr>
 *
 * linux/arch/e1nommu/kernel/sys_hyperstone.c
 *
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/utsname.h>

#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/ipc.h>
#include <asm/syscalls.h>

/*
 * sys_pipe() is the normal C calling standard for creating
 * a pipe. It's not the way unix traditionally does this, though.
 */
asmlinkage int sys_pipe(unsigned long * fildes)
{
	int fd[2];
	int error;

	error = do_pipe(fd);
	if (!error) {
		if (copy_to_user(fildes, fd, 2*sizeof(int)))
			error = -EFAULT;
	}
	return error;
}

/* common code for old and new mmaps */
static inline long do_mmap2(
	unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags,
	unsigned long fd, unsigned long pgoff)
{
	int error = -EBADF;
	struct file * file = NULL;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
out:
	return error;
}

asmlinkage long sys_mmap2(unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags,
	unsigned long fd, unsigned long pgoff)
{
	return do_mmap2(addr, len, prot, flags, fd, pgoff);
}

/*
 * Perform the select(nd, in, out, ex, tv) and mmap() system
 * calls. Linux/m68k cloned Linux/i386, which didn't use to be able to
 * handle more than 4 system call parameters, so these system calls
 * used a memory block for parameter passing..
 */
//#define DBG_old_mmap
asmlinkage int sys_old_mmap(struct mmap_arg_struct *arg)
{
	struct mmap_arg_struct a;
	int error = -EFAULT;

	if (copy_from_user(&a, arg, sizeof(a)))
		goto out;
#ifdef DBG_old_mmap
	printk("old_mmap..\n"
		   "addr  : 0x%x, len : 0x%x, prot : 0x%x\n"
		   "flags : 0x%x, fd  : 0x%x, offset : 0x%x\n",
			a.addr, a.len, a.prot, a.flags, a.fd, a.offset );
#endif

	error = -EINVAL;
	if (a.offset & ~PAGE_MASK)
		goto out;

	a.flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);

	error = do_mmap2(a.addr, a.len, a.prot, a.flags, a.fd, a.offset >> PAGE_SHIFT);
#ifdef DBG_old_mmap
	printk("old_mmap do_mmap2 ret_val : %d\n", error);
#endif
out:
	return error;
}

#if 0 /* DAVIDM - do we want this */
struct mmap_arg_struct64 {
	__u32 addr;
	__u32 len;
	__u32 prot;
	__u32 flags;
	__u64 offset; /* 64 bits */
	__u32 fd;
};

asmlinkage long sys_mmap64(struct mmap_arg_struct64 *arg)
{
	int error = -EFAULT;
	struct file * file = NULL;
	struct mmap_arg_struct64 a;
	unsigned long pgoff;

	if (copy_from_user(&a, arg, sizeof(a)))
		return -EFAULT;

	if ((long)a.offset & ~PAGE_MASK)
		return -EINVAL;

	pgoff = a.offset >> PAGE_SHIFT;
	if ((a.offset >> PAGE_SHIFT) != pgoff)
		return -EINVAL;

	if (!(a.flags & MAP_ANONYMOUS)) {
		error = -EBADF;
		file = fget(a.fd);
		if (!file)
			goto out;
	}
	a.flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, a.addr, a.len, a.prot, a.flags, pgoff);
	up_write(&current->mm->mmap_sem);
	if (file)
		fput(file);
out:
	return error;
}
#endif

asmlinkage long sys_old_select(struct sel_arg_struct *arg)
{
	struct sel_arg_struct a;

	if (copy_from_user(&a, arg, sizeof(a)))
		return -EFAULT;
	/* sys_select() does the appropriate kernel locking */
	return sys_select(a.n, a.inp, a.outp, a.exp, a.tvp);
}

/* Originally derived from MIPS. Added exclusive access
 * to system_utsname through uts_sem. */
asmlinkage long sys_olduname(struct old_utsname * name)
{
	int error;

	if (!name)
		return -EFAULT;
	if (!access_ok(VERIFY_WRITE,name,sizeof(struct old_utsname)))
		return -EFAULT;

	down_read(&uts_sem);
	
	error = __copy_to_user(&name->sysname,&system_utsname.sysname,__NEW_UTS_LEN);
	error -= __put_user(0,name->sysname+__NEW_UTS_LEN);
	error -= __copy_to_user(&name->nodename,&system_utsname.nodename,__NEW_UTS_LEN);
	error -= __put_user(0,name->nodename+__NEW_UTS_LEN);
	error -= __copy_to_user(&name->release,&system_utsname.release,__NEW_UTS_LEN);
	error -= __put_user(0,name->release+__NEW_UTS_LEN);
	error -= __copy_to_user(&name->version,&system_utsname.version,__NEW_UTS_LEN);
	error -= __put_user(0,name->version+__NEW_UTS_LEN);
	error -= __copy_to_user(&name->machine,&system_utsname.machine,__NEW_UTS_LEN);
	error = __put_user(0,name->machine+__NEW_UTS_LEN);
	error =	error ? -EFAULT : 0;

	up_read(&uts_sem);

	return error;
}

/*
 * sys_ipc() is the de-multiplexer for the SysV IPC calls..
 *
 * This is really horribly ugly.
 */
asmlinkage long sys_ipc (uint call, int first, int second,
			int third, void *ptr, long fifth)
{
	int version, ret;

	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

	if (call <= SEMCTL)
		switch (call) {
		case SEMOP:
			return sys_semop (first, (struct sembuf *)ptr, second);
		case SEMGET:
			return sys_semget (first, second, third);
		case SEMCTL: {
			union semun fourth;
			if (!ptr)
				return -EINVAL;
			if (get_user(fourth.__pad, (void **) ptr))
				return -EFAULT;
			return sys_semctl (first, second, third, fourth);
			}
		default:
			return -EINVAL;
		}
	if (call <= MSGCTL) 
		switch (call) {
		case MSGSND:
			return sys_msgsnd (first, (struct msgbuf *) ptr, 
					  second, third);
		case MSGRCV:
			switch (version) {
			case 0: {
				struct ipc_kludge tmp;
				if (!ptr)
					return -EINVAL;
				if (copy_from_user (&tmp,
						    (struct ipc_kludge *)ptr,
						    sizeof (tmp)))
					return -EFAULT;
				return sys_msgrcv (first, tmp.msgp, second,
						   tmp.msgtyp, third);
				}
			default:
				return sys_msgrcv (first,
						   (struct msgbuf *) ptr,
						   second, fifth, third);
			}
		case MSGGET:
			return sys_msgget ((key_t) first, second);
		case MSGCTL:
			return sys_msgctl (first, second,
					   (struct msqid_ds *) ptr);
		default:
			return -EINVAL;
		}
	if (call <= SHMCTL) 
		switch (call) {
		case SHMAT:
			switch (version) {
			default: {
				ulong raddr;
				ret = sys_shmat (first, (char *) ptr,
						 second, &raddr);
				if (ret)
					return ret;
				return put_user (raddr, (ulong *) third);
			}
			}
		case SHMDT: 
			return sys_shmdt ((char *)ptr);
		case SHMGET:
			return sys_shmget (first, second, third);
		case SHMCTL:
			return sys_shmctl (first, second,
					   (struct shmid_ds *) ptr);
		default:
			return -EINVAL;
		}

	return -EINVAL;
}

asmlinkage int sys_ioperm(unsigned long from, unsigned long num, int on)
{
  return -ENOSYS;
}

/*
 * Old cruft
 */
asmlinkage int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return -ERESTARTNOHAND;
}

/* The following system call is added for the hyperstone
 * architecture in order Non-local exits to be feasible in the library.
 */
//#define DBG_sys_e1newSP
asmlinkage long sys_e1newSP( unsigned long SavedSP)
{	
	unsigned long *G9, tmpSR, tmpSP;
	struct {
		unsigned long FP;
		unsigned long FL;
	} SR;

#ifdef DBG_sys_e1newSP
	printk("Entered sys_e1newSP\n");
	printk("SavedSP : 0x%x\n", (int)SavedSP );
#endif
	asm volatile("mov %0, G9"
		     :"=l"(G9)
		     :/*no input*/ );

#define ReturnSP	G9[-7] 
#define ReturnSR	G9[-6] 
#define UserFL		G9[-10]
	/* We assign a frame length of 15 reg for longjmp
	 * It is big big enough...*/
#define longjmpFL	15

	/* Construct ReturnSP */
	tmpSR  = ReturnSR;
	/* We want to maintain the FP info in the SR */
	tmpSR &= 0x00007f00;
/*1.FL*/SR.FL = longjmpFL; SR.FL <<= 21; tmpSR |= SR.FL;
/*2.FP*/SR.FP = SavedSP; SR.FP &= 0x000001fc;
        SR.FP <<= 23; tmpSR |= SR.FP;
	ReturnSR = tmpSR;

	/* Construct ReturnSR */
	tmpSP = SavedSP;
	tmpSP += longjmpFL<<2;
	ReturnSP = tmpSP;
#ifdef DBG_sys_e1newSP
	printk("ReturnSR : 0x%x, ReturnSP : 0x%x\n", (int)ReturnSR, (int)ReturnSP );
#endif

	return 0;

#undef  longjmpFL
#undef  UserFL
#undef  ReturnSP
#undef  ReturnSP
}

