/*
 * This program is used to generate definitions needed by
 * assembly language modules.
 *
 * We use the technique used in the OSF Mach kernel code:
 * generate asm statements containing #defines,
 * compile this file to assembler, and then extract the
 * #defines from the assembly-language output.
 */

/*  Changes by AKBAR HUSSAIN Lineo  */

#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/hardirq.h>

/*  Now nisa gas don't understand : #define blur, blur	*/
#define DEFINE(sym, val) \
	asm volatile("\n#define " #sym " %c0" : : "i" (val))

int main(void)
{
	/* offsets into the task struct (defined in /include/linux/sched.h) */
	DEFINE(TASK_STATE, offsetof(struct task_struct, state));
	DEFINE(TASK_FLAGS, offsetof(struct task_struct, flags));
	DEFINE(TASK_PTRACE, offsetof(struct task_struct, ptrace));
	DEFINE(TASK_BLOCKED, offsetof(struct task_struct, blocked));
	DEFINE(TASK_COUNTER, offsetof(struct task_struct, counter));
	DEFINE(TASK_SIGPENDING, offsetof(struct task_struct, sigpending));
	DEFINE(TASK_NEEDRESCHED, offsetof(struct task_struct, need_resched));
	DEFINE(TASK_THREAD, offsetof(struct task_struct, thread));
	DEFINE(TASK_MM, offsetof(struct task_struct, mm));
	DEFINE(TASK_ACTIVE_MM, offsetof(struct task_struct, active_mm));

	/* offsets into the thread struct (defined in 
	   include/asm-frionommu/processor.h) 
	*/
	/* Unused - amit
	DEFINE(CPUSTAT_SOFTIRQ_ACTIVE, offsetof(irq_cpustat_t, __softirq_active));
	DEFINE(CPUSTAT_SOFTIRQ_MASK, offsetof(irq_cpustat_t, __softirq_mask));
	*/
	/* Need to fix this when more thread definitions are added - akale */
	DEFINE(THREAD_KSP, offsetof(struct thread_struct, ksp));
	DEFINE(THREAD_USP, offsetof(struct thread_struct, usp));
	DEFINE(THREAD_SR, offsetof(struct thread_struct, seqstat));
	DEFINE(THREAD_ESP0, offsetof(struct thread_struct, esp0));
	DEFINE(THREAD_PC, offsetof(struct thread_struct, pc));

	/* offsets into the pt_regs */
	DEFINE(PT_ORIG_R0, offsetof(struct pt_regs, orig_r0));
	DEFINE(PT_R0, offsetof(struct pt_regs, r0));
	DEFINE(PT_R1, offsetof(struct pt_regs, r1));
	DEFINE(PT_R2, offsetof(struct pt_regs, r2));
	DEFINE(PT_R3, offsetof(struct pt_regs, r3));
	DEFINE(PT_R4, offsetof(struct pt_regs, r4));
	DEFINE(PT_R5, offsetof(struct pt_regs, r5));
	DEFINE(PT_R6, offsetof(struct pt_regs, r6));
	DEFINE(PT_R7, offsetof(struct pt_regs, r7));
	DEFINE(PT_P0, offsetof(struct pt_regs, p0));
	DEFINE(PT_P1, offsetof(struct pt_regs, p1));
	DEFINE(PT_P2, offsetof(struct pt_regs, p2));
	DEFINE(PT_P3, offsetof(struct pt_regs, p3));
	DEFINE(PT_P4, offsetof(struct pt_regs, p4));
	DEFINE(PT_P5, offsetof(struct pt_regs, p5));

	DEFINE(PT_A0w, offsetof(struct pt_regs, a0w));
	DEFINE(PT_A1w, offsetof(struct pt_regs, a1w));
	DEFINE(PT_A0x, offsetof(struct pt_regs, a0x));
	DEFINE(PT_A1x, offsetof(struct pt_regs, a1x));
	DEFINE(PT_RETS, offsetof(struct pt_regs, rets));
	DEFINE(PT_RESERVED, offsetof(struct pt_regs, reserved));
	DEFINE(PT_ASTAT, offsetof(struct pt_regs, astat));
	DEFINE(PT_SEQSTAT, offsetof(struct pt_regs, seqstat));
	DEFINE(PT_PC, offsetof(struct pt_regs, pc));
	DEFINE(PT_IPEND, offsetof(struct pt_regs, ipend));
	DEFINE(PT_USP, offsetof(struct pt_regs, usp));
	DEFINE(PT_FP, offsetof(struct pt_regs, fp));
	DEFINE(PT_SYSCFG, offsetof(struct pt_regs, syscfg));

	/* offsets into the irq_handler struct */
	DEFINE(IRQ_HANDLER, offsetof(struct irq_node, handler));
	DEFINE(IRQ_DEVID, offsetof(struct irq_node, dev_id));
	DEFINE(IRQ_NEXT, offsetof(struct irq_node, next));

	/* offsets into the kernel_stat struct */
	DEFINE(STAT_IRQ, offsetof(struct kernel_stat, irqs));

	/* signal defines */
	DEFINE(SIGSEGV, SIGSEGV);
	DEFINE(SEGV_MAPERR, SEGV_MAPERR);
	DEFINE(SIGTRAP, SIGTRAP);

	DEFINE(PT_PTRACED, PT_PTRACED);
	DEFINE(PT_TRACESYS, PT_TRACESYS);
	DEFINE(PT_DTRACE, PT_DTRACE);

	return 0;
}
