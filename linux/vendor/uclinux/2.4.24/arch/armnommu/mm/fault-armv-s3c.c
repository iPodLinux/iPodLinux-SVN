/*
 *  linux/arch/arm/mm/fault-s3c.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Modifications for ARM processor (c) 1995-2001 Russell King
 *  Modifications for Samsung ARM processors (c) 2002 Oleksandr Zhadan
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/unaligned.h>

extern void die_if_kernel(const char *str, struct pt_regs *regs, int err);
extern void show_pte(struct mm_struct *mm, unsigned long addr);
extern int do_page_fault(unsigned long addr, int error_code,
			 struct pt_regs *regs);
extern int do_translation_fault(unsigned long addr, int error_code,
				struct pt_regs *regs);
extern void do_bad_area(struct task_struct *tsk, struct mm_struct *mm,
			unsigned long addr, int error_code,
			struct pt_regs *regs);


#if defined(CONFIG_CPU_S3C45x0_ALIGNMENT_PROCFS) && defined(CONFIG_SYSCTL)

struct align {
	struct align *next;
	unsigned int  addr;
	unsigned int  offset;
	unsigned int  count;
	unsigned char name[16];
	};

struct align root_align;

static int proc_alignment_read(char *page, char **start, off_t off,
			       int count, int *eof, void *data)
{
	char *p = page;
	int len;
	struct align *ptr;

	for ( ptr=&root_align; ptr != NULL; ptr = ptr->next ) {
	    if   ((unsigned int)ptr == (unsigned int)&root_align)
		 p += sprintf(p, "Address(Offset)\t\tCounter\t\tName\n");
	    else p += sprintf(p, "0x%08x(0x%08x)\t%6d\t\t%s\n", ptr->addr, ptr->offset,
								ptr->count, ptr->name);
	    }

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

/*
 * This needs to be done after sysctl_init, otherwise sys/
 * will be overwritten.
 */
static int __init alignment_init(void)
{
	create_proc_read_entry("sys/debug/alignment", 0, NULL,
				proc_alignment_read, NULL);
				
	root_align.next   = NULL;
	root_align.addr   = 0;
	root_align.offset = 0;
	root_align.count  = 0;
	memset(root_align.name, 0, sizeof(root_align.name));
	return 0;
}

__initcall(alignment_init);
#endif

static int
do_alignment(unsigned long addr, int error_code, struct pt_regs *regs)
{
#if defined(CONFIG_CPU_S3C45x0_ALIGNMENT_PROCFS) && defined(CONFIG_SYSCTL)
	unsigned int instrptr;
	struct align *ptr;
	
        instrptr = instruction_pointer(regs) - 4; 

	if   ( user_mode(regs) )
	     for ( ptr=&root_align; ptr != NULL; ptr = ptr->next ) {
		 if  ( !strcmp (ptr->name, current->comm) )
		     if  ( ptr->addr == instrptr ) {
			 ptr->count += 1;
			 return 0;
			 }
		 if  ( !ptr->next ) 
		     break;
		}
	else for ( ptr=&root_align; ptr != NULL; ptr = ptr->next ) {
		 if  ( ptr->addr == instrptr ) {
		     ptr->count += 1;
		     return 0;
		     }
		 if  ( !ptr->next ) 
		     break;
		 }	
	    
	ptr->next = kmalloc(sizeof(struct align), GFP_KERNEL);
	if  (!ptr->next)
	    panic("do_alignment: unable to allocate memory!");
	memset(ptr->next->name, 0, sizeof(ptr->next->name));

	ptr->next->next   = 0;
	ptr->next->addr   = instrptr;
	ptr->next->count  = 1;
	
	if   ( user_mode(regs) ) {
	     ptr->next->offset = ptr->next->addr - current->mm->start_code;
	     strncpy(ptr->next->name, current->comm, 15);
	     }
	else {
	     ptr->next->offset = ptr->next->addr;
	     strcpy(ptr->next->name, "_KERNEL_");
	     }
#endif

#if defined(CONFIG_CPU_S3C45x0_ALIGNMENT_TRAP)
	return 0;
#else
	return 1;
#endif		
}


/*
 * Some section permission faults need to be handled gracefully, for
 * instance, when they happen due to a __{get,put}_user during an oops).
 */
static int
do_sect_fault(unsigned long addr, int error_code, struct pt_regs *regs)
{
	struct task_struct *tsk = current;
	do_bad_area(tsk, tsk->active_mm, addr, error_code, regs);
	return 0;
}

/*
 * Hook for things that need to trap external faults.  Note that
 * we don't guarantee that this will be the final version of the
 * interface.
 */
int (*external_fault)(unsigned long addr, struct pt_regs *regs);

static int
do_external_fault(unsigned long addr, int error_code, struct pt_regs *regs)
{
	if (external_fault)
		return external_fault(addr, regs);
	return 1;
}

/*
 * This abort handler always returns "fault".
 */
static int
do_bad(unsigned long addr, int fsr, struct pt_regs *regs)
{
	return 1;
}

static const struct fsr_info {
	int	(*fn)(unsigned long addr, int error_code, struct pt_regs *regs);
	int	sig;
	char	*name;
} fsr_info[] = {
	{ do_bad,		SIGSEGV, "vector exception"		   },
	{ do_alignment,		SIGILL,	 "alignment exception"		   },
	{ do_bad,		SIGKILL, "terminal exception"		   },
	{ do_bad,		SIGILL,	 "alignment exception"		   },
	{ do_bad,		SIGBUS,	 "external abort on linefetch"	   },
	{ do_bad,		SIGSEGV, "section translation fault"	   },
	{ do_bad,		SIGBUS,	 "external abort on linefetch"	   },
	{ do_bad,		SIGSEGV, "page translation fault"	   },
	{ do_bad,		SIGBUS,	 "external abort on non-linefetch" },
	{ do_bad,		SIGSEGV, "section domain fault"		   },
	{ do_bad,		SIGBUS,	 "external abort on non-linefetch" },
	{ do_bad,		SIGSEGV, "page domain fault"		   },
	{ do_bad,		SIGBUS,	 "external abort on translation"   },
	{ do_bad,		SIGSEGV, "section permission fault"	   },
	{ do_bad,		SIGBUS,	 "external abort on translation"   },
	{ do_bad,		SIGSEGV, "page permission fault"	   }
};

/*
 * Currently dropped down to debug level
 */
asmlinkage void
do_DataAbort(unsigned long addr, int error_code, struct pt_regs *regs, int fsr)
{
	const struct fsr_info *inf = fsr_info + (fsr & 15);

	if (!inf->fn(addr, error_code, regs))
		return;

	force_sig(inf->sig, current);
	printk(KERN_ALERT "Unhandled fault: %s (%X) at 0x%08lx\n",
		inf->name, fsr, addr);
	show_pte(current->mm, addr);
	die_if_kernel("Oops", regs, 0);
	return;
}

asmlinkage void
do_PrefetchAbort(unsigned long addr, struct pt_regs *regs)
{
	do_translation_fault(addr, 0, regs);
}

/*
 * if PG_dcache_dirty is set for the page, we need to ensure that any
 * cache entries for the kernels virtual memory range are written
 * back to the page.
 */
void check_pgcache_dirty(struct page *page)
{
}
