/*
 * arch/e1nommu/platform/E132XS/traps.c
 * 
 * Copyright (C) 2002 GDT,  George Thanos<george.thanos@gdt.gr>
 *                          Yannis Mitsos<yannis.mitsos@gdt.gr>
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/current.h>
#include <asm/page.h>
#include <asm/system.h>
#include <asm/delay.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <asm/unistd.h>
#include <asm/stack.h>

long ShortOfUserRegisterStack(void);
long ShortOfUserAggregateStack(void);

void Common_Exception_Handler(struct pt_regs regs, int exception_num)
{
		unsigned long *G9, G3, G4;
		asm volatile("mov %0, G9\n\t"
					 "mov %1, G3\n\t"
					 "mov %2, G4\n\t"
					 :"=l"(G9),
					  "=l"(G3),
					  "=l"(G4)
					 :/*no input*/);

        /* printk("WARNING:***Common_Exception_Handler***\n");
	 */

#define L    regs.L
#define FL   regs.FL

	/* Some register values may not be pushed appropriately
	 * to memory, thus use of <exception_num> is more appropriate.
	 * switch( L[FL+3] ) {
	 */
	switch( exception_num ) {
	case 9: 
		printk("Exception 57...\n");
		while(1)
		{
			/* Parity Error */
			toggle_green_led(0);
		}
		break;	
	case 10: 
		printk("Exception 58...\n");
		while(1)
		{
			/* Parity Error */
			toggle_red_led(0);
		}
		break;	
	case 11:
		printk("Exception 59...\n");
		while(1)
		{
			/*Extended Overflow */
			toggle_green_led(500000);
		}
		break;	
	case 12: 
#define UserUB  G9[-11]
#define UserG5  G9[-3]

		if( ShortOfUserRegisterStack() ) {
				if  ( user_mode( (struct pt_regs*)&regs ) ) { 
					/* Exception happened in User Mode */
					printk("Short of User Mode Register Stack. U!\n");
					printk("FATAL ERROR - Killing process : %d ......\n", current->pid );
					//send_sig( SIGKILL, current, 1);
					kill( current->pid, SIGKILL );
				} else if ( current->thread.stack_use.excp_in_km_while_using_user_reg_stack ) {
					/* Exception happened in Kernel Mode but while still using the User Register stack */
					printk("Short of User Mode Register Stack. K!\n");
					printk("FATAL ERROR - Killing process : %d ......\n", current->pid );
					do_exit(SIGKILL);
				} else {
					/* Exception happened in Kernel Mode */
					printk( "Short of Kernel Mode Register Stack!\n"
							"KERNEL BUG!\n");
					goto _kernel_bug;
				}
				break;
		}
		else if( ShortOfUserAggregateStack() ) {
				if  ( user_mode( (struct pt_regs*)&regs ) ) {
					/* Exception happened in User Mode */
					printk("Short of User Mode Aggregate Stack. U!\n");
					printk("FATAL ERROR - Killing process : %d ......\n", current->pid );
					kill( current->pid, SIGKILL );
				} else if( current->thread.stack_use.excp_in_km_while_using_user_aggr_stack ) {
					/* Exception happened in Kernel Mode but while still using the User Aggregate stack */
					printk("Short of User Mode Aggregate Stack. K!\n");
					printk("FATAL ERROR - Killing process : %d ......\n", current->pid );
					do_exit(SIGKILL);
				} else {
					/* Exception happened in Kernel Mode */
					printk( "Short of Kernel Mode Aggregate Stack!\n"
							"KERNEL BUG!\n");
					goto _kernel_bug;
				}
				break;
		}

		if  ( user_mode( (struct pt_regs*)&regs ) ) {

			/* restore UB & G5 to their prev values */
			UserG5 += STACK_OFFSET; 
			UserUB -= STACK_OFFSET; 

			printk("Pointer or Priviledge Error Happened in User Mode\n");
			printk("Killing process : %d ......\n", current->pid );
			printk("PC : 0x%08x\n", (int)L[FL+0] ); 
			printk("SR : 0x%08x\n", (int)L[FL+1] );
			printk("SP : 0x%08x\n", (int)L[FL+4] );
			printk("FL : 0x%08x\n", (int)L[FL+5] );
			printk("current->pid : %d\n", current->pid );

			kill( current->pid, SIGKILL );
			break;
		}
		else
			printk("Pointer or Priviledge Error Happened in Kernel Mode\n"
				   "KERNEL BUG\n" );
_kernel_bug:
	{
		/* Kernel Mode... */
		unsigned long UB, G5;
		asm volatile("ori SR, 0x20\n\t"
					 "mov %0, UB\n\t"
					 "mov %1, G5\n\t"
					 :"=l"(UB),
					  "=l"(G5)
					 :/* no input */ );
		G5 += STACK_OFFSET; 
		UB -= STACK_OFFSET; 
		asm volatile("ori SR, 0x20\n\t"
					 "mov UB, %0\n\t"
					 "mov G5, %1\n\t"
					 :/* no output */
					 :"l"(UB),
					  "l"(G5) );
		printk("PC : 0x%08x\n", (int)L[FL+0] ); 
		printk("SR : 0x%08x\n", (int)L[FL+1] );
		printk("SP : 0x%08x\n", (int)L[FL+4] );
		printk("FL : 0x%08x\n", (int)L[FL+5] );
		printk("current->pid : %d\n", current->pid );

		cli();
		toggle_red_led(500000);
#undef UserUB
#undef UserG5
	}
		break;
	case 13 :
		if  ( user_mode( (struct pt_regs*)&regs ) ) {
			printk( "An instruction of all ones happened in user mode.\n"
			"Killing process...\n");
			kill( current->pid, SIGKILL );
		} else {
			printk("An instruction of all ones happened in kernel mode\n");
			BUG();
		}
		break;
	case 14 :
		/* printk("Floating Exception took place...\nSending SIGFPE signal\n");
		 */
		kill( current->pid, SIGFPE );
		/* the actual exception bits are cleared on the
		 * next FP instruction. We don't have to clear them.
		 */
		break;
	default:
		printk("Exception handler: Something else happened...\n");
		while(1);
		break;
	}
#undef L
#undef FL
}

int sys_kprintf(const char *msg, int len)
{
		char *str;
		int flags;

		save_flags(flags); cli();

		if( len == 0 ) {
				return 1;
		}

		str = (char *)kmalloc( len, GFP_ATOMIC );	
		copy_from_user( str, msg, len );
		printk("%s\n", str);
		kfree(str);

		restore_flags(flags);
		return 100;
}

void _toggle_red_led(int arg)
{
	toggle_red_led(arg);
}

void _toggle_green_led(int arg)
{
	toggle_green_led(arg);
}

void _toggle_red_and_green_led(int arg)
{
	toggle_red_and_green_led(arg);
}
