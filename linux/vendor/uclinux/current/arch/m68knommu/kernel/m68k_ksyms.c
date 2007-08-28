#include <linux/module.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/user.h>
#include <linux/elfcore.h>
#include <linux/in6.h>
#include <linux/interrupt.h>
#include <linux/config.h>

#include <asm/setup.h>
#include <asm/machdep.h>
#include <asm/pgalloc.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/checksum.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>
#include <asm/current.h>

//asmlinkage long long __ashrdi3 (long long, int);
//asmlinkage long long __lshrdi3 (long long, int);
extern char m68k_debug_device[];

extern void dump_thread(struct pt_regs *, struct user *);
extern int dump_fpu(elf_fpregset_t *);

/* platform dependent support */

#if 0
EXPORT_SYMBOL(m68k_machtype);
EXPORT_SYMBOL(m68k_cputype);
EXPORT_SYMBOL(m68k_is040or060);
EXPORT_SYMBOL(cache_push);
EXPORT_SYMBOL(cache_clear);
#ifndef CONFIG_SINGLE_MEMORY_CHUNK
EXPORT_SYMBOL(mm_vtop);
EXPORT_SYMBOL(mm_ptov);
EXPORT_SYMBOL(mm_end_of_chunk);
#endif
EXPORT_SYMBOL(m68k_realnum_memory);
EXPORT_SYMBOL(m68k_memory);
#endif
#ifndef CONFIG_SUN3
#if 0
EXPORT_SYMBOL(mm_vtop_fallback);
#endif
EXPORT_SYMBOL(__ioremap);
EXPORT_SYMBOL(iounmap);
#if 0
EXPORT_SYMBOL(kernel_set_cachemode);
#endif
#endif
#if 0
EXPORT_SYMBOL(m68k_debug_device);
#endif
EXPORT_SYMBOL(dump_fpu);
EXPORT_SYMBOL(dump_thread);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(strstr);
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strcat);
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strncmp);

EXPORT_SYMBOL(ip_fast_csum);

#if 0
EXPORT_SYMBOL(local_irq_count);
EXPORT_SYMBOL(local_bh_count);
#endif
EXPORT_SYMBOL(mach_enable_irq);
EXPORT_SYMBOL(mach_disable_irq);
EXPORT_SYMBOL(kernel_thread);
#ifdef CONFIG_VME
EXPORT_SYMBOL(vme_brdtype);
#endif

/* Networking helper routines. */
EXPORT_SYMBOL(csum_partial_copy);

/* The following are special because they're not called
   explicitly (the C compiler generates them).  Fortunately,
   their interface isn't gonna change any time soon now, so
   it's OK to leave it out of version control.  */
//EXPORT_SYMBOL_NOVERS(__ashrdi3);
//EXPORT_SYMBOL_NOVERS(__lshrdi3);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL_NOVERS(memcmp);
EXPORT_SYMBOL_NOVERS(memscan);
EXPORT_SYMBOL_NOVERS(memmove);

EXPORT_SYMBOL_NOVERS(__down_failed);
EXPORT_SYMBOL_NOVERS(__down_failed_interruptible);
EXPORT_SYMBOL_NOVERS(__down_failed_trylock);
EXPORT_SYMBOL_NOVERS(__up_wakeup);

EXPORT_SYMBOL(get_wchan);

/*
 * libgcc functions - functions that are used internally by the
 * compiler...  (prototypes are not correct though, but that
 * doesn't really matter since they're not versioned).
 */
extern void __gcc_bcmp(void);
extern void __ashldi3(void);
extern void __ashrdi3(void);
extern void __cmpdi2(void);
extern void __divdi3(void);
extern void __divsi3(void);
extern void __lshrdi3(void);
extern void __moddi3(void);
extern void __modsi3(void);
extern void __muldi3(void);
extern void __mulsi3(void);
extern void __negdi2(void);
extern void __ucmpdi2(void);
extern void __udivdi3(void);
extern void __udivmoddi4(void);
extern void __udivsi3(void);
extern void __umoddi3(void);
extern void __umodsi3(void);

        /* gcc lib functions */
EXPORT_SYMBOL_NOVERS(__gcc_bcmp);
EXPORT_SYMBOL_NOVERS(__ashldi3);
EXPORT_SYMBOL_NOVERS(__ashrdi3);
EXPORT_SYMBOL_NOVERS(__cmpdi2);
EXPORT_SYMBOL_NOVERS(__divdi3);
EXPORT_SYMBOL_NOVERS(__divsi3);
EXPORT_SYMBOL_NOVERS(__lshrdi3);
EXPORT_SYMBOL_NOVERS(__moddi3);
EXPORT_SYMBOL_NOVERS(__modsi3);
EXPORT_SYMBOL_NOVERS(__muldi3);
EXPORT_SYMBOL_NOVERS(__mulsi3);
EXPORT_SYMBOL_NOVERS(__negdi2);
EXPORT_SYMBOL_NOVERS(__ucmpdi2);
EXPORT_SYMBOL_NOVERS(__udivdi3);
EXPORT_SYMBOL_NOVERS(__udivmoddi4);
EXPORT_SYMBOL_NOVERS(__udivsi3);
EXPORT_SYMBOL_NOVERS(__umoddi3);
EXPORT_SYMBOL_NOVERS(__umodsi3);

EXPORT_SYMBOL_NOVERS(_current_task);

EXPORT_SYMBOL_NOVERS(is_in_rom);

#ifdef CONFIG_COLDFIRE
extern unsigned int *dma_device_address;
extern unsigned long dma_base_addr, _ramend, _ramstart;
EXPORT_SYMBOL_NOVERS(dma_base_addr);
EXPORT_SYMBOL_NOVERS(dma_device_address);
EXPORT_SYMBOL_NOVERS(_ramend);
EXPORT_SYMBOL_NOVERS(_ramstart);

extern asmlinkage void trap(void);
extern void	*_ramvec;
EXPORT_SYMBOL_NOVERS(trap);
EXPORT_SYMBOL_NOVERS(_ramvec);

#endif
