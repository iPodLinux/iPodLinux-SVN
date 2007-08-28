//vic Copied from m68knommu architecture
/*
 * This program is used to generate definitions needed by
 * assembly language modules.
 *
 * We use the technique used in the OSF Mach kernel code:
 * generate asm statements containing #defines,
 * compile this file to assembler, and then extract the
 * #defines from the assembly-language output.
 */

#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <asm/irq.h>
#include <asm/hardirq.h>
#include <asm/errno.h>
#include <asm/niosconf.h>

#define DEFINE(sym, val) \
	asm volatile ("\n#define " #sym " %0" : : "i" (val))

int main (void)
{
	/* offsets into the task struct */
//vic	DEFINE(TASK_STATE, offsetof(struct task_struct, state));
	DEFINE(TASK_FLAGS, offsetof(struct task_struct, flags));
//vic	DEFINE (TASK_PTRACE, offsetof(struct task_struct, ptrace));
	DEFINE(TASK_BLOCKED, offsetof(struct task_struct, blocked));
//vic	DEFINE(TASK_COUNTER, offsetof(struct task_struct, counter));
	DEFINE(TASK_SIGPENDING, offsetof(struct task_struct, sigpending));
	DEFINE(TASK_NEED_RESCHED, offsetof(struct task_struct, need_resched));
	DEFINE(TASK_THREAD, offsetof(struct task_struct, thread));
//vic	DEFINE(TASK_MM, offsetof(struct task_struct, mm));
//vic	DEFINE(TASK_ACTIVE_MM, offsetof(struct task_struct, active_mm));

	/* offsets into the cpu status struct */
//vic	DEFINE(CPUSTAT_SOFTIRQ_PENDING, offsetof(irq_cpustat_t, __softirq_pending));
//vic	DEFINE(CPUSTAT_LOCAL_IRQ_COUNT, offsetof(irq_cpustat_t, __local_irq_count));

	/* offsets into the thread struct */
	DEFINE(THREAD_KSP, offsetof(struct thread_struct, ksp));
	DEFINE(THREAD_KPC, offsetof(struct thread_struct, kpc));
	DEFINE(THREAD_KPSR, offsetof(struct thread_struct, kpsr));
	DEFINE(THREAD_FORK_KPSR, offsetof(struct thread_struct, fork_kpsr));
	DEFINE(THREAD_SPARE_REG_WINDOW, offsetof (struct thread_struct, spare_reg_window));
	DEFINE(THREAD_SPARE_W_SAVED, offsetof (struct thread_struct, spare_w_saved));
	DEFINE(THREAD_FLAGS, offsetof(struct thread_struct, flags));

	/* thread flag bits */
	DEFINE(NIOS_FLAG_KTHREAD, NIOS_FLAG_KTHREAD);
	DEFINE(NIOS_FLAG_COPROC, NIOS_FLAG_COPROC);
	DEFINE(NIOS_FLAG_DEBUG, NIOS_FLAG_DEBUG);

	/* register window struct */
	DEFINE(REGWIN_SZ, REGWIN_SZ);

	/* ptrace register struct */
	DEFINE(TRACEREG_SZ, TRACEREG_SZ);

	/* task / stack size */
	DEFINE(THREAD_SIZE, THREAD_SIZE);

//vic	/* offsets into the kernel_stat struct */
//vic	DEFINE(STAT_IRQ, offsetof(struct kernel_stat, irqs));

	/* signal defines */
	DEFINE(SIGCHLD, SIGCHLD);

	/* ptrace flag bits */
//vic	DEFINE(PT_PTRACED, PT_PTRACED);
//vic	DEFINE(PT_TRACESYS, PT_TRACESYS);
//vic	DEFINE(PT_DTRACE, PT_DTRACE);

	/* error values */
//vic	DEFINE(ENOSYS, ENOSYS);

	/* clone flag bits */
	DEFINE(CLONE_VFORK, CLONE_VFORK);
	DEFINE(CLONE_VM, CLONE_VM);

	/* the flash chip */
	DEFINE(NIOS_FLASH_START, NIOS_FLASH_START);
	
	/* the kernel placement in the flash*/
	DEFINE(KERNEL_FLASH_START, KERNEL_FLASH_START);
	
	/* the romdisk placement in the flash */
	DEFINE(LINUX_ROMFS_START, LINUX_ROMFS_START);
	DEFINE(LINUX_ROMFS_END, LINUX_ROMFS_END);
	
	/* the sdram */
	DEFINE(LINUX_SDRAM_START, LINUX_SDRAM_START);
	
	return 0;
}
