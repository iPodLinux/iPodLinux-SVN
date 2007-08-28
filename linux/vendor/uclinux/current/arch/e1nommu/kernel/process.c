/*
 *  linux/arch/e1nommu/kernel/process.c
 *
 *  Copyright (C) 2002-2003,	George Thanos <george.thanos@gdt.gr>
 *  							Yannis Mitsos <yannis.mitsos@gdt.gr>
 *
 *  This file handles the architecture-dependent parts of 
 *  process handling..
 *
 *	The process handling scheme is the following. Each process has a kernel register stack
 *	and a kernel aggregate stack. In user mode each process has its own user
 *	register stack and user aggregate stack. In general each process has 4 stacks,
 *	2 in user mode and 2 in kernel mode.
 *	When we run in user mode and an interrupt takes place we need two registers to
 *	indicate the kernel register stack pointer and the kernel aggregate stack pointer,
 *	since we need to change stacks. These registers are G11 and G9 respectively.
 *	G11 indicates the lower bound of the kernel register stack(grows to higher addresses)
 *	and G9 the upper bound of the kernel aggregate stack(grows to lower addresses).
 *
 *	For more details on the implementation see the comments and inline documentation of each
 *	function.
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/reboot.h>
#include <linux/mman.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/machdep.h>
#include <asm/setup.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/unistd.h>
#include <asm/stack.h>

#define _STH asm volatile("ori   SR, 0x20");
#define _CLH asm volatile("andni SR, 0x20");

void Flush_Register_Context(void);
void ret_from_vfork(void);

/* This function performs the actual task switching */
long in_switch_to=0;
void __switch_to(struct task_struct * prev, struct task_struct * next)
{
#if 0 
	in_switch_to++;
	printk("switch_to : %d\n", in_switch_to);
	printk("prev id : %d, next id: %d\n"
	       "prev task: 0x%08x, next task: 0x%08x\n",
	       prev->pid, next->pid, prev, next ); 
#endif
	/* Lock! An interrupt here could be a disaster. */
	cli();
	Flush_Register_Context();

	/* Perform register saving. PC should not be saved */
	asm volatile(
    	"mov 	%0, SR\n\t"
	"mov    %1, G2\n\t"
	"mov    %2, G3\n\t"
	"mov    %3, G4\n\t"
	"mov    %4, G5\n\t"
	"mov    %5, G6\n\t"
	"mov    %6, G7\n\t"
	:"=l"(prev->thread.SR),
	 "=l"(prev->thread.G2),
	 "=l"(prev->thread.G3),
	 "=l"(prev->thread.G4),
	 "=l"(prev->thread.G5),
	 "=l"(prev->thread.G6),
	 "=l"(prev->thread.G7)
	 :/*no input*/
	 :"cc","memory","%L0","%L1","%L2","%L3","%L4");

	asm volatile(
	"mov    %0, G8\n\t"
	"mov    %1, G9\n\t"
	"mov    %2, G11\n\t"
	"mov    %3, G12\n\t"
	"mov    %4, G13\n\t"
	"mov    %5, G14\n\t"
	"mov    %6, G15\n\t"
	"setadr L4\n\t" // Don't save L0 - saved PC
    "mov 	%7, L4\n\t"
	"ori	SR, 0x20\n\t"
	"mov    %8, UB\n\t" /* Upper Stack Bound - G19*/
	:"=l"(prev->thread.G8),
	 "=l"(prev->thread.G9),
	 "=l"(prev->thread.G11),
	 "=l"(prev->thread.G12),
	 "=l"(prev->thread.G13),
	 "=l"(prev->thread.G14),
	 "=l"(prev->thread.G15),
	 "=l"(prev->thread.SP),
	 "=l"(prev->thread.UB)
	 :/*no input*/
	 :"cc","memory","%L2");

	prev->thread.SP += 16;
	current = next;

	/* Perform restorings */
	asm volatile(
	"movi   L0, _after_return_instruction\n\t"
	"ori    L0, 0x1\n\t" /* S flag */
	"mov 	L1, %0\n\t"
	"mov    G2, %1\n\t"
	"mov    G3, %2\n\t"
	"mov    G4, %3\n\t"
	"mov    G5, %4\n\t"
	"mov    G6, %5\n\t"
	"mov    G7, %6\n\t"
	"mov    G8, %7\n\t"
	"mov    G9, %8\n\t"
	:/* no output*/
	:"l"(next->thread.SR),
	 "l"(next->thread.G2),
	 "l"(next->thread.G3),
	 "l"(next->thread.G4),
	 "l"(next->thread.G5),
	 "l"(next->thread.G6),
	 "l"(next->thread.G7),
	 "l"(next->thread.G8),
	 "l"(next->thread.G9)
	:"cc","memory","%L0","%L1");

	asm volatile(
	"mov    G11,%0\n\t"
	"mov    G12,%1\n\t"
	"mov    G13,%2\n\t"
	"mov    G14,%3\n\t"
	"mov    G15,%4\n\t"
	"fetch  4\n\t"
	"ori	SR, 0x20\n\t" /* Set H flag */
	"mov    SP, %5\n\t"   /* SP - G18 */
	"ori	SR, 0x20\n\t" /* Set H flag */
	"mov    UB, %6\n\t"   /* UB - G19 */
	"ret    PC, L0\n\t"   /* Validate new Local Register Stack */
	"_after_return_instruction:\n\t"
	:/* no output*/
	:"l"(next->thread.G11),
	 "l"(next->thread.G12),
	 "l"(next->thread.G13),
	 "l"(prev->thread.G14),
	 "l"(next->thread.G15),
	 "l"(next->thread.SP),
	 "l"(next->thread.UB)
	:"cc","%L0","%L1","memory");

	sti();
	/* gcc saves L2 to Aggregate stack. Since we have saved
	 * L2 explicitely and we need to use our saved value
	 * we have to return before gcc does so!
	 */
	asm volatile(	"addi   G3, 8\n\t"
			"ret PC, L2\n\t");
}

/* This is the architecture dependent initialization function
 * when vforking/cloning a new process */
#define DEBUG_copy_thread 0 
int copy_thread(int nr, unsigned long clone_flags, unsigned long  child_ustack,	
		unsigned long unused, struct task_struct *p, struct pt_regs *regs)
{
#define	   FL         	regs->FL
#define	   L         	regs->L
#define    ret_PC     	L[FL+0]
#define    ret_SR     	L[FL+1]
#define    ret_SP     	L[FL+2]
#define    RETURN_MODE  (ret_PC & 0x1)? 1:0
#define    KERNEL_MODE  1
#define    USER_MODE    0
#define    ALLOC_KAGGREGATE_STACK              alloc_task_struct()
#define    ALLOC_KREGISTER_STACK               alloc_task_struct()

/* ReturnSR :
 * Given the <_SP_> value  we produce a SR that will enable
 * a function to return with a Frame Pointer of <_FP_> registers
 * higher in the register stack than <_SP_> and a Frame Length 
 * of <_FL_>. (FLAGS : S=1, I=0, L=0)
 * Be careful!! We don't check if FL=16 to set it 0.
 * When FL=16 set _FL_ to 0.
 */
#define ReturnSR(_FP_, _FL_, _SP_)				\
({												\
	 	volatile unsigned long RetSR = 0; 		\
		RetSR = ((_SP_) + ( (long)(_FP_)<<2 ) ) & 0x1FC; /*0x1FC - b0000 0001 1111 1100*/ \
		RetSR = RetSR << 23;/*25-2*/			\
		RetSR |= (_FL_)<<21;					\
		RetSR |= 0x00040000; /* S=1, I=0, L=0 */\
		RetSR;									\
})

	char *G3, *G4, *G5, *G9, *G11;
	unsigned long *Flowers;
	long AggregateStackSize, RegisterStackSize, mode = RETURN_MODE;
#if DEBUG_copy_thread
	printk("copy_thread...\n");
	printk("&p : 0x%08x\n", (int)p );
#endif
	
	/* G11 is the register stack pointer
	 * Register Stack grows to higher addresses - G11 points to the bottom of the stack*/
	p->thread.G11 = (unsigned long)ALLOC_KREGISTER_STACK;
	/* G9 is the aggregate stack pointer
	 * Aggregate Stack grows to lower addresses - G9 points to the top of the stack*/
	p->thread.G9  = (unsigned long)p + THREAD_SIZE;

	asm volatile(
	"mov %0, G11\n\t"
	"mov %1, G9\n\t"
	"mov %2, G3\n\t"
	"mov %3, G4\n\t"
	"mov %4, G5\n\t"
	:"=l"(G11),
	 "=l"(G9),
	 "=l"(G3),
	 "=l"(G4),
	 "=l"(G5)
	:/* no input */);
#if _DEBUG_copy_thread
	printk( "G11: 0x%08x, G9: 0x%08x,\n"
		"G3: 0x%08x,  G4: 0x%08x, G5: 0x%08x\n",
		G11, G9, G3, G4, G5 );
#endif
	

	/* We differentiate initialization of kernel register stack,
	 * depending whether we came from user or kernel mode */
	/* The child process returns in the following order:
	 * 1. Validates Kernel Register Stack within __switch_to().
	 * 2. Returns from switch_to() to ret_from_vfork() (skips schedule() )
	 */
	if( mode == KERNEL_MODE ) {
		/* Actions: 
		 * a.Copy Kernel Aggregate Stack of parent to child (G3 -> G9)
		 * b.Copy Kernel Register Stack.
		 * c.Add the Flowers (We call "Flowers" the extra information that we
		 * add to the child's kernel register stack in order to return)*/

		/* Copy Kernel Aggregate Stack */
		AggregateStackSize = (long)(G9 - G3);
		if(AggregateStackSize & 0x3) BUG();/* Check for word allignment */
		/* Aggregate Stack. We cannot copy the full stack size. 
	 	 * We would overwrite child's process descriptor */
		 memcpy( (char*)(p->thread.G9 - AggregateStackSize), G3, AggregateStackSize );

		/* Copy Kernel Register Stack. */
		RegisterStackSize = ret_SP - (int)G11;
		if(RegisterStackSize & 0x3 ) BUG();/* Check for word allignment */
		memcpy( (char*)p->thread.G11, G11, RegisterStackSize );

		/* Initialise Globals (Globals is the information needed in order to
		 * perform task switching within <__switch_to()>) */
		p->thread.SP =  (unsigned long)(p->thread.G11 + RegisterStackSize + 10*4);
		p->thread.UB =  (unsigned long)(p->thread.G11 + THREAD_SIZE);
		/* This is the SR that is used to change-validate the register stack
		 * within __switch_to()*/
		p->thread.SR = ReturnSR(-4, 15, p->thread.SP); /* __switch_to FL=15 */

		/*Put to G2 the value of the current FER*/
		_STH;
		p->thread.G2 = 0;
		p->thread.G3 =  p->thread.G9 - (G9 - G3);
		p->thread.G4 =  p->thread.G9 - (G9 - G4);
		p->thread.G5 =  p->thread.G9 - (G9 - G5);
		p->thread.G6 =  0;
		p->thread.G7 =  0;
		p->thread.G8 =  0;
	/*	p->thread.G9   Initialised to top  
		p->thread.G10  Used by GDB 
		p->thread.G11  Initialised to top */
		p->thread.G12 = 0;
		p->thread.G13 = 0;
		p->thread.G14 = 0;
		p->thread.G15 = 0;
		
		/*Flowers!!! - We arbitrarily call Flowers the additional information
		 * that we put at the top of the register stack in order to return*/
		Flowers    =  (unsigned long*)(p->thread.G11 + RegisterStackSize );
		Flowers[-1] = 0; /* Return value should be 0 when the child returns */
		Flowers[0] =  L[FL + 0]; 
		Flowers[1] =  ReturnSR(-16, 6, p->thread.SP); /* Return to <clone> */
		Flowers[2] =  L[FL + 2];
		Flowers[3] =  L[FL + 3];
		Flowers[4] =  L[FL + 4];
		Flowers[5] =  L[FL + 5];
	/* 	Flowers[6] - next parameter of switch_to
		Flowers[7] - prev parameter of switch_to	*/
		Flowers[8] = (unsigned long)&ret_from_vfork;  Flowers[8] |= 0x1;
		Flowers[9] = ReturnSR(-10, 6, p->thread.SP); /* Return to <ret_from_vfork>*/
	} 
	else /* USER_MODE */
	{
		unsigned long *TopOfchildAggStack, *TopOfparentAggStack;
		/* Actions: 
		 * a.Copy only the necessary Kernel Aggregate Stack to return.
		 * b.Don't Copy Kernel Register Stack. Just set SP to a reasonable value.
		 * c.Initialize Globals */

	    /* We need 4 registers inside <__switch_to> and another 3
		 * in <ret_from_vfork> totally 7. Thus 10 registers is ample space.
		 * Stack will altered since we come from user mode, so we just want 
		 * the appropriate info in order to return from user to kernel mode.
		 */

		/* Initialise Globals (Globals is the information needed in order to
		 * perform task switching within <__switch_to()>) */
		p->thread.SP =  (unsigned long)(p->thread.G11 + 10*4);
		p->thread.UB =  (unsigned long)(p->thread.G11 + THREAD_SIZE);
		p->thread.SR =  ReturnSR(-4, 15, p->thread.SP); /* __switch_to FL=15 */

		/*Put to G2 the value of the current FER*/
		_STH;
		p->thread.G2 =  0;
		p->thread.G3 =  p->thread.G9 - 13 * 4;
		p->thread.G4 =  p->thread.G9 -  4 * 4;
		p->thread.G5 =  p->thread.G9 - (G9 - G5);
		p->thread.G6 =  0;
		p->thread.G7 =  0;
		p->thread.G8 =  0;
	/*	p->thread.G9   Initialised to top  
		p->thread.G10  Used by GDB 
		p->thread.G11  Initialised to top */
		p->thread.G12 = 0;
		p->thread.G13 = 0;
		p->thread.G14 = 0;
		p->thread.G15 = 0;

		/* Return value should be 0 when the child returns.
		 * This is done in ret_from_vfork in entry.S */
		TopOfchildAggStack  = (unsigned long*)(p->thread.G9);
		TopOfparentAggStack  = (unsigned long*)G9;
		/* Since we need to return to user mode we need the
		 * info located in the kernel aggregate stack of parent
		 * We copy info from parent to child kernel aggregate stack.*/
		TopOfchildAggStack[-1]  = TopOfparentAggStack[-1]; 
		TopOfchildAggStack[-2]  = TopOfparentAggStack[-2]; 
		TopOfchildAggStack[-3]  = TopOfparentAggStack[-3]; 
		TopOfchildAggStack[-4]  = TopOfparentAggStack[-4]; 
		TopOfchildAggStack[-5]  = TopOfparentAggStack[-5]; 
		TopOfchildAggStack[-6]  = TopOfparentAggStack[-6]; 
		TopOfchildAggStack[-7]  = TopOfparentAggStack[-7]; 
		TopOfchildAggStack[-8]  = TopOfparentAggStack[-8]; 
		TopOfchildAggStack[-9]  = TopOfparentAggStack[-9]; 
		TopOfchildAggStack[-10] = TopOfparentAggStack[-10]; 
		TopOfchildAggStack[-11] = TopOfparentAggStack[-11]; 

		/* <Flowers> is again the info that we add to the kernel 
		 * register stack of the child in order to gracefully return. */
		Flowers = (unsigned long*)p->thread.G11;
		Flowers[0] = 0;
		Flowers[1] = 0;
		Flowers[2] = 0;
		Flowers[3] = 0;
		Flowers[4] = 0;
		Flowers[5] = 0;
	/* 	Flowers[6] - next parameter of switch_to
		Flowers[7] - prev parameter of switch_to	*/
		Flowers[8] = (unsigned long)&ret_from_vfork; Flowers[8] |= 0x1;
		Flowers[9] = ReturnSR(-10, 6, p->thread.SP); /* Return to <ret_from_vfork> with FL=6*/

		/* When calling vfork in user mode parent may not be able to return
		 * because the child has corrupted the stack. In order to return safely
		 * we need to copy the parent's return information, and restore it just
		 * before parent returns. */
		current->thread.vfork_ret_info.ret_from_vfork = 1;
		get_user(current->thread.vfork_ret_info.ReturnPC, &L[1]);
		get_user(current->thread.vfork_ret_info.ReturnSR, &L[2]);
		/*
		printk("L[1] : 0x%x, L[2] : 0x%x\n", (int)L[1], (int)L[2]);
		printk("current : 0x%x, current->ret_from_vfork : 0x%x\n", 
				current, 
				&current->thread.vfork_ret_info.ret_from_vfork ); */
	}

#if DEBUG_copy_thread 
	printk( "p->pid : %d\n", p->pid );
	printk( "p->thread.G11: 0x%08x, p->thread.G9: 0x%08x,\n"
		"p->thread.G3: 0x%08x,  p->thread.G4: 0x%08x,\np->thread.G5: 0x%08x\n",
		p->thread.G11, p->thread.G9, p->thread.G3, p->thread.G4, p->thread.G5 );
#endif
		return 0;

#undef    ReturnSR
#undef    FL
#undef	  L
#undef    ret_PC
#undef    ret_SR
#undef    ret_SP
#undef    RETURN_MODE
#undef    KERNEL_MODE
#undef    USER_MODE 
#undef    ALLOC_KAGGREGATE_STACK
#undef    ALLOC_KREGISTER_STACK
}

/*
 * Create a kernel thread
 */
#define DEBUG_kernel_thread 0
extern asmlinkage long sys_exit(int);
long arch_kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
	unsigned long _pid;
	flags |= CLONE_VM;

	_pid = clone(flags);
#if DEBUG_kernel_thread
	printk("Kernel_thread: _pid:%d\n",_pid);
#endif
	if(!_pid){
#if DEBUG_kernel_thread
		printk("Child thread %d is ready to run...\n"
		       "fn:0x%08x, arg:0x%08x\n",current->pid, fn, (long)arg);
#endif
		fn(arg);
		sys_exit(1);
	}
	return _pid;
}

asmlinkage int sys_fork(struct pt_regs regs)
{
	return(-EINVAL);
}

#define DBG_sys_vfork 0
asmlinkage int sys_vfork(struct pt_regs regs)
{
	int retval;
#if DBG_sys_vfork
	printk("Entered vfork, pid : %d\n", current->pid);	
#endif
	retval =  do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, 0, &regs, 0);
#if DBG_sys_vfork
	printk("Exiting vfork, pid : %d\n", current->pid);	
#endif
	return retval;
}

asmlinkage int sys_clone(struct pt_regs regs)
{
#define L	regs.L
#define FL	regs.FL
	unsigned long clone_flags = L[ FL-2];
	return do_fork( clone_flags, 0, &regs, 0);
#undef  L
#undef  FL
}

/*
 * The idle loop on hyperstone..
 */
static void default_idle(void)
{
#ifdef DEBUG
	printk("default_idle...\n");
#endif
		while(1) {
		if (!current->need_resched){
		asm volatile("
PowerDown:
			ANDNI    SR, 0x8000              
            STW.IOA  0,  0, 0x0B800000
            BR       (long)PDLoopX2         
PDLoopX2:   NOP
            NOP
            NOP
            NOP
            NOP"
		: /* no outputs */
		: 
		);

		}
		schedule();
	}
}

void (*idle)(void) = default_idle;

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	init_idle();
	current->nice = 20;
	current->counter = -100;
	idle();
}

void machine_restart(char * __unused)
{
	if (mach_reset)
		mach_reset();
	for (;;);
}

void machine_halt(void)
{
	if (mach_halt)
		mach_halt();
	for (;;);
}

void machine_power_off(void)
{
	if (mach_power_off)
		mach_power_off();
	for (;;);
}

/* In order to execute a new application we have to
 * modify the kernel aggregate stack pointers. Pointers
 * should show in the following memory area:
 * FROM : (TOP_OF_STACK-44) -- TO : (TOP_OF_STACK-16)
 * TOP_OF_STACK - 4 = Kernel Register stack pointer G3
 * TOP_OF_STACK - 8 = Kernel Register stack pointer G4
 * TOP_OF_STACK - 12= Kernel Register stack pointer G5
 * TOP_OF_STACK - 16= Index to the top of the next Aggregate Stack Frame
 * Function should be executed just before returning from
 * sys_execve system call, when no other job has to be done in terms of
 * process execution.
 */
void Switch_AggregateStackValues(void)
{
	unsigned long G3, G4, G5, G9;
	asm volatile(	"mov %0, G9\n\t"
			:"=l"(G9)
	     		:/*no input*/
	     		:"%G9");
	G3 = G9 - 44; 
	G4 = G9 - 16;
	G5 = G9 - 0x1000;
	asm volatile(	"mov G3, %0\n\t"
			"mov G4, %1\n\t"
			"mov G5, %2\n\t"
			:/*no output*/
			:"l"(G3), "l"(G4), "l"(G5)
			:"%G3", "%G4", "%G5" );
}

/*
 *  sys_execve() executes a new program.
 */
#if 0
#define DBG_sys_execve
#endif
asmlinkage int sys_execve(struct pt_regs regs)
{
#define L		regs.L
#define FL		regs.FL
#define ARG1		L [ FL-3 ]
#define ARG2		L [ FL-4 ]
#define ARG3		L [ FL-5 ]
#define ReturnPC	L [ FL ]
	/* ReturnPC is the PC value of the interrupted task
	 * (Next instruction to be executed)
	 */ 
#ifdef DBG_sys_execve
		char **argv = ((char **)ARG2);
		char **envp = ((char **)ARG3);
		char *name  = ((char *) ARG1);
#else
#define name		((char *)ARG1)
#define argv		((char **)ARG2)
#define envp		((char **)ARG3)
#endif
        int error;
		char *filename;

#ifdef DBG_sys_execve
		int i, argc = 0, envc = 0;
		printk("name:<%s>, argv:[0x%x],  envp:[0x%x]\n", name, argv, envp );
		if(argv)
			for(i=0; argv[i] != (char*)0; i++)
				argc++;
		printk("argc : %d\n", argc);
#endif

        lock_kernel();
        filename = getname(name);
#ifdef DBG_sys_execve
		printk("filename:<%s>\n", filename );
#endif
        error = PTR_ERR(filename);
        if (IS_ERR(filename))
				goto out;
        error = do_execve(name, argv, envp, &regs);

	   /* We are not aware if we came from user or kernel mode.
	 	* Since execve needs user mode, we have to explicitely
	 	* verify that.
	 	*/ 
		ReturnPC &= 0xffFFffFE;
        putname(filename);
out:
        unlock_kernel();
        return error;
#undef name
#undef argv
#undef envp
	
#undef ReturnPC
#undef L
#undef FL
#undef ARG1
#undef ARG2
#undef ARG3
}

#define DEBUG_start_thread 0 
void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long usp)
{
	unsigned long *G9;
	struct {
		unsigned long FP;
		unsigned long FL;
	} SR;
	unsigned long first_arg;
#define TOP_OF_KERNEL_AGGREGATE_STACK  G9
#define uG3				*(TOP_OF_KERNEL_AGGREGATE_STACK - 1)
#define uG4				*(TOP_OF_KERNEL_AGGREGATE_STACK - 2)
#define uG5				*(TOP_OF_KERNEL_AGGREGATE_STACK - 3)
#define SavedG4				*(TOP_OF_KERNEL_AGGREGATE_STACK - 4)
#define L0				*(TOP_OF_KERNEL_AGGREGATE_STACK - 5)
#define L1				*(TOP_OF_KERNEL_AGGREGATE_STACK - 6)
#define L2				*(TOP_OF_KERNEL_AGGREGATE_STACK - 7)
#define L3				*(TOP_OF_KERNEL_AGGREGATE_STACK - 8)
#define L4				*(TOP_OF_KERNEL_AGGREGATE_STACK - 9)
#define L5				*(TOP_OF_KERNEL_AGGREGATE_STACK - 10)
#define UB				*(TOP_OF_KERNEL_AGGREGATE_STACK - 11)
#define uSavedPC			L0
#define uSavedSR			L1
#define uSavedSP			L2
#define uSavedUB			UB
	asm volatile("mov %0, G9\n\t"
		     :"=l"(TOP_OF_KERNEL_AGGREGATE_STACK)
		     :/*no input*/
		     :"%G9");
/* We split the user stack into two different entities of approximately 0x1000
 * bytes each. The upper entity in memory is the User Aggregate Stack, while the
 * lower entity in memory is the User Register Stack.
 */
	/* User Register Stack */
	uSavedSP =  ((current->mm->end_brk + 3) & ~3);	
	uSavedUB =  ((current->mm->end_brk + 3) & ~3) + USER_REGISTER_STACK_SIZE;
	/* We keep STACK_OFFSET stack space, just in case we run out of stack
	 * This will enable us to handle the exception*/
	uSavedUB -= STACK_OFFSET;

	/* User Aggregate Stack */
	uG3 = usp;
	uG4 = usp;
	/* We assign a slight space between the two stacks */
	uG5 = uSavedUB + STACK_OFFSET + 0x10;
	/* We keep STACK_OFFSET stack space, just in case we run out of stack
	 * This will enable us to handle the exception*/
	uG5 += STACK_OFFSET;

	/* Saved SR value */
	/* Trap enable : Invalid Operation, Div by zero, Overflow
	 * Trap disable: Underflow, Inexact.
	 * Rounding Mode : Round to nearest.
	 */
	uSavedSR = 0x1C00; 
	/* FL = 2 arbitrarily */
/*1.FL*/SR.FL = 2; SR.FL <<= 21; uSavedSR |= SR.FL;
/*2.FP*/SR.FP = uSavedSP; SR.FP &= 0x000001fc; 
	SR.FP <<= 23; uSavedSR |= SR.FP;

	/* User PC */
	uSavedPC = pc;
    /* Pass Arguments */
	/* argc is located where <usp> points to. 
	 * argv is located 4 bytes higher from argc.
	 * envp is located (sizeof(char*) * (argc+1)) bytes
	 * higher than argv. (See _uClibc_start in uClibc for 
	 * more details)
	 */
	first_arg = usp;
	/* Set the value of <first_arg> in the base of the User Register Stack. */
	*(unsigned long *)uSavedSP = first_arg;
	/* We assign the uSavedSP a little bit higher so as to be able to fill
	 * the Register Stack with the value of <first_arg>.
	 */
	uSavedSP += 16; 
#if DEBUG_start_thread 
	/* This is just for debugging. It should not compiled in */
	printk("DEBUG start_thread...\n");
	printk("G3: [0x%x], G4: [0x%x], G5: [0x%x]\n", uG3, uG4, uG5 );
	printk("uPC: [0x%x], uSR: [0x%x], uSavedSP: [0x%x], uSavedUB: [0x%x]\n",
		uSavedPC, uSavedSR, uSavedSP, uSavedUB );
	printk("&uPC: [0x%x], &uSR: [0x%x], &uSavedSP: [0x%x], &uSavedUB: [0x%x]\n",
		&uSavedPC, &uSavedSR, &uSavedSP, &uSavedUB );
	printk("Remember G5 -= STACK_OFFSET, UB += STACK_OFFSET\n");
	{	unsigned long G11;
		asm volatile("mov %0, G11\n\t"
		     :"=l"(G11)
		     :/*no input*/
		     :"%G11");
		printk("G9: [0x%x], G11: [0x%x]\n", (long)G9, (long)G11 );
	}
#endif

#undef uSavedUB
#undef uSavedSP
#undef uSR
#undef uPC
#undef SavedG4
#undef L0
#undef L1
#undef L2
#undef L3
#undef L4
#undef L5
#undef UB
#undef uG3
#undef uG4
#undef uG5
#undef TOP_OF_KERNEL_AGGREGATE_STACK
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
	/* We de-allocate the kernel register stack 
	 * we allocated in copy_thread..*/
	/* free_task_struct(current->thread.G11); */
}

void flush_thread(void)
{
	/* Nothing to be done for the time being */
}

/*
 * Return saved PC of a blocked thread.
 */

inline unsigned long thread_saved_pc(struct thread_struct *t) { return 0; }

unsigned long get_wchan(struct task_struct *p)
{
		return 0; /* Empty for the time being */
}

/* If we are in kernel aggregate stack; function 
 * returns 1, else returns 0.
 */
//#define DBG_AreWeInKernelAggrStack
long AreWeInKernelAggrStack(void)
{
	unsigned long G3, G9;
	asm volatile("mov %0, G3\n\t"
				 "mov %1, G9\n\t"
				 :"=l"(G3),
				  "=l"(G9)
				 :/*no input*/);
	if( G9 > G3 && (G9 - 0x1000) < G3) {
#ifdef DBG_AreWeInKernelAggrStack
			printk("AreWeInKernelAggrStack returns 1\n");
#endif
			return 1;
	}
	else {
			current->thread.stack_use.excp_in_km_while_using_user_aggr_stack = 1;
#ifdef DBG_AreWeInKernelAggrStack
			printk("AreWeInKernelAggrStack returns 0\n");
#endif
			return 0;
	}
}

/* If we are in kernel register stack; function 
 * returns 1, else returns 0.
 */
//#define DBG_AreWeInKernelRegisterStack
long AreWeInKernelRegisterStack(void)
{
	unsigned long SP, G11; 
	asm volatile("ori SR, 0x20\n\t"
				 "mov %0, SP\n\t"
				 "mov %1, G11\n\t"
				 :"=l"(SP),
				  "=l"(G11)
				 :/*no input*/ );

	if( G11 <= SP  &&  G11 + 0x1000 > SP ) {
#ifdef DBG_AreWeInKernelRegisterStack
			printk("AreWeInKernelRegisterStack returns 1\n");
#endif
			return 1;
	}
	else {
			current->thread.stack_use.excp_in_km_while_using_user_reg_stack = 1;
#ifdef DBG_AreWeInKernelRegisterStack
			printk("AreWeInKernelRegisterStack returns 0\n");
#endif
			return 0;
	}
}

/* If we need to change register stack; function
 * returns 1, else returns 0
 */
//#define DBG_ShouldWeChangeRegisterStack
long ShouldWeChangeBackRegisterStack(void)
{
	if( current->thread.stack_use.excp_in_km_while_using_user_reg_stack == 1 ) {
		current->thread.stack_use.excp_in_km_while_using_user_reg_stack = 0;
#ifdef DBG_ShouldWeChangeRegisterStack
		printk("ShouldWeChangeRegisterStack returns 1\n");
#endif
		return 1;
	} else {
#ifdef DBG_ShouldWeChangeRegisterStack
		printk("ShouldWeChangeRegisterStack returns 0\n");
#endif
		return 0;
	}
}

/* If we need to change aggregate stack; function
 * returns 1, else returns 0.
 */
//#define DBG_ShouldWeChangeAggregateStack
long ShouldWeChangeBackAggregateStack(void)
{
	if( current->thread.stack_use.excp_in_km_while_using_user_aggr_stack == 1 ) {
		current->thread.stack_use.excp_in_km_while_using_user_aggr_stack = 0;
#ifdef DBG_ShouldWeChangeAggregateStack
		printk("ShouldWeChangeAggregateStack returns 1\n");
#endif
		return 1;
	} else {
#ifdef DBG_ShouldWeChangeAggregateStack
		printk("ShouldWeChangeAggregateStack returns 0\n");
#endif
		return 0;
	}
}

long ShortOfUserRegisterStack(void)
{
#define UserSP	G9[-7]
#define UserUB	G9[-11]
		unsigned long *G9;

		/* G9 is the Aggregate Stack Pointer */
		asm volatile("mov %0, G9"
					:"=l"(G9)
					:/* no input */);

		if( UserUB - UserSP < STACK_OFFSET )
				return 1;
		else
				return 0;
#undef  UserSP
#undef  UserUB
}

long ShortOfUserAggregateStack(void)
{
#define UserG3  G9[-1]
#define UserG5  G9[-3]
		unsigned long *G9;

		/* G9 is the Aggregate Stack Pointer */
		asm volatile("mov %0, G9"
					:"=l"(G9)
					:/* no input */);

		if( UserG3 - UserG5 < STACK_OFFSET )
				return 1;
		else
				return 0;
#undef UserG3
#undef UserG5
}	

/* Code Related to stack growning 
 * Non-valid for uClinux */
#if 0
#define DBG_expand_user_reg_stack
void expand_user_reg_stack(void)
{
#define UserSP	G9[-7]
#define UserUB	G9[-11]

		unsigned long NewStackSize = 2 * current->thread.ursi.size;
		unsigned long *NewLowerBound = (unsigned long*)0;
		unsigned long *G9;
		unsigned long prevSP;

		/* G9 is the Aggregate Stack Pointer */
		asm volatile("mov %0, G9"
					:"=l"(G9)
					:/* no input */);

		/* Allocate the new Register Stack */
		NewLowerBound =  (unsigned long*) do_mmap( 0, 0, NewStackSize,
								 PROT_READ | PROT_EXEC | PROT_WRITE, 0, 0 );

		/*Zero new stack */
		memset( (char*)NewLowerBound, 0, NewStackSize);
		/* Copy old stack to the new one */
		memcpy( NewLowerBound, current->thread.ursi.LowerBound, current->thread.ursi.size ); 

#ifdef DBG_expand_user_reg_stack
		printk("Entered expand_user_reg_stack...\n");
		printk("Previous stack size : 0x%x, New stack size : 0x%x\n", current->thread.ursi.size, NewStackSize );
		printk("Previous LowerBound : 0x%x, New LowerBound : 0x%x\n", current->thread.ursi.LowerBound, NewLowerBound ); 
#endif

		/* Set SP, UB values. The actual return values are located in
		 * the Kernel Aggregate Stack. */
		/* Put their values in the Kernel Aggregate Stack */
#ifdef DBG_expand_user_reg_stack
		printk("Previous UserSP : 0x%x, Previous UserUB : 0x%x\n", UserSP, UserUB );
		printk("stack size : 0x%x\n", UserSP - current->thread.ursi.LowerBound );
#endif
		prevSP = UserSP;
	 	UserSP = prevSP - current->thread.ursi.LowerBound + (unsigned long)NewLowerBound; 	
		UserUB = (unsigned long)NewLowerBound + NewStackSize - STACK_OFFSET;

#ifdef DBG_expand_user_reg_stack
		printk("New UserSP : 0x%x, New UserUB : 0x%x\n", UserSP, UserUB );
		printk("G9 : 0x%x, &UserSP : 0x%x, &UserUB : 0x%x\n", G9, &UserSP, &UserUB );
#endif

		/* Memory unmap the already mapped stack */
		if( current->thread.uasi.size > 0x1000 )
		do_munmap( current->mm, current->thread.ursi.LowerBound, current->thread.ursi.size );

		/* Update the values of the register_stack_info struct */
		current->thread.ursi.LowerBound = NewLowerBound;
		current->thread.ursi.size = NewStackSize;
#ifdef DBG_expand_user_reg_stack
		printk("Exiting expand_user_reg_stack\n");
#endif

#undef UserSP
#undef UserUB
}

#define DBG_expand_user_aggr_stack
void expand_user_aggr_stack(void)
{
		unsigned long NewStackSize = 2 * current->thread.uasi.size;
		unsigned long *NewLowerBound = (unsigned long*)0;
		unsigned long *G9;
		unsigned long G3, G4, G5;
		unsigned long prevG3, prevG4, prevG5;

#define UserG3  G9[-1]
#define UserG4  G9[-2]
#define UserG5  G9[-3]
	
		/* G9 is the Aggregate Stack Pointer */
		asm volatile("mov %0, G9"
					:"=l"(G9)
					:/* no input */);

		/* Allocate the new Register Stack */
		NewLowerBound =  (unsigned long*) do_mmap( 0, 0, NewStackSize,
								 PROT_READ | PROT_EXEC | PROT_WRITE, 0, 0 );

		/*Zero new stack */
		memset( (char*)NewLowerBound, 0, NewStackSize);
		/* Copy old stack to the new one */
		memcpy( (char*)((unsigned long)NewLowerBound + (NewStackSize/2)), 
				current->thread.uasi.LowerBound, 
				current->thread.uasi.size ); 

#ifdef DBG_expand_user_aggr_stack
		printk("Entered expand_user_aggr_stack\n");
		printk("Previous stack size : 0x%x, New stack size : 0x%x\n", current->thread.uasi.size, NewStackSize );
		printk("Previous LowerBound : 0x%x, New LowerBound : 0x%x\n", current->thread.uasi.LowerBound, NewLowerBound ); 
#endif

		/* Set new G3, G4, G5 values. The actual return values are located in
		 * the Kernel Aggregate Stack. */
		prevG3 = UserG3;
		UserG3 = prevG3 - current->thread.uasi.LowerBound + (unsigned long)NewLowerBound + (NewStackSize/2);
		prevG4 = UserG4;
		UserG4 = prevG4 - current->thread.uasi.LowerBound + (unsigned long)NewLowerBound + (NewStackSize/2);
		prevG5 = UserG5;
		UserG5 = (unsigned long)NewLowerBound + STACK_OFFSET;

#ifdef DBG_expand_user_aggr_stack
		printk("prevG3 : 0x%x, newG3 : 0x%x\n", prevG3, UserG3 );
		printk("prevG4 : 0x%x, newG4 : 0x%x\n", prevG4, UserG4 );
		printk("prevG5 : 0x%x, newG5 : 0x%x\n", prevG5, UserG5 );
#endif

		/* Memory unmap the already mapped stack */
		if( current->thread.uasi.size > 0x1000 )
		do_munmap( current->mm, current->thread.ursi.LowerBound, current->thread.ursi.size );

		/* Update the values of the register_stack_info struct */
		current->thread.ursi.LowerBound = NewLowerBound;
		current->thread.ursi.size = NewStackSize;
#ifdef DBG_expand_user_aggr_stack
		printk("Exiting expand_user_aggr_stack\n");
#endif

#undef UserG3
#undef UserG4
#undef UserG5
}

long changeSPvalue(unsigned long newUB, unsigned long oldUB, unsigned long oldSP )
{
		unsigned long newSP = newUB - (oldUB - oldSP) - (current->thread.ursi.size/2);
		printk("oldSP : 0x%x, newSP : 0x%x\n", oldSP, newSP );
		return newSP;
}

/* Printing SP, UB in supervisor mode */
void print_SP_UB(void)
{
	unsigned long SP, UB;
	printk("Entered print_SP_UB\n");
	asm volatile("ori SR, 0x20\n\t"
				 "mov %0, SP\n\t"
				 "ori SR, 0x20\n\t"
				 "mov %1, UB\n\t"
				 :"=l"(SP),
				  "=l"(UB)
				 :/*no input*/);
	printk("SP : 0x%x, UB, : 0x%x\n", (int)SP, (int)UB);
}
#endif
